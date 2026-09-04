#include <cstdint>
#include <cstdio>

#include "config/resonance_config.h"

#include "core/log.h"

#include "modules/resonance/audio_stages.h"
#include "modules/resonance/resonance_shaders.h"

#include "render/gl.h"

#ifndef GL_R16
#define GL_R16 0x822A
#endif

namespace {

constexpr GLfloat kQuadVerts[18] = {-1, -1, 0, 1, -1, 0, -1, 1, 0,
                                    1,  1,  0, 1, -1, 0, -1, 1, 0};

void quantize(std::vector<uint16_t> &out, const float *data, int size) {
    out.resize(static_cast<size_t>(size));
    for (int i = 0; i < size; ++i) {
        float v = data ? data[i] : 0.0f;
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        out[static_cast<size_t>(i)] =
            static_cast<uint16_t>(v * 65535.0f + 0.5f);
    }
}

} // namespace

bool ResonanceAudioStages::init() {
    if (ready_)
        return true;

    pass_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                    resonance_shaders::kAudioPassFs,
                                    "resonance_audio_pass");
    gravity_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                       resonance_shaders::kAudioGravityFs,
                                       "resonance_audio_gravity");
    average_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                       resonance_shaders::kAudioAverageFs,
                                       "resonance_audio_average");
    smooth_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                      resonance_shaders::kAudioSmoothFs,
                                      "resonance_audio_smooth");
    if (!pass_prog_ || !gravity_prog_ || !average_prog_ || !smooth_prog_) {
        klog("resonance_audio_stages: shader compile failed");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts,
                 GL_STATIC_DRAW);
    glBindVertexArray(0);

    GLuint probe_tex = 0, probe_fbo = 0;
    glGenTextures(1, &probe_tex);
    glBindTexture(GL_TEXTURE_2D, probe_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, 4, 1, 0, GL_RED, GL_UNSIGNED_SHORT,
                 nullptr);
    glGenFramebuffers(1, &probe_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, probe_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           probe_tex, 0);
    GLenum probe = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &probe_fbo);
    glDeleteTextures(1, &probe_tex);
    if (probe != GL_FRAMEBUFFER_COMPLETE) {
        klog("resonance_audio_stages: R16 render target unsupported (0x%x)",
             probe);
        return false;
    }

    glUseProgram(smooth_prog_);
    glUniform1i(glGetUniformLocation(smooth_prog_, "sample_mode"),
                kResonanceSampleMode);
    glUniform1f(glGetUniformLocation(smooth_prog_, "sample_hybrid_weight"),
                kResonanceSampleHybridWeight);
    glUniform1f(glGetUniformLocation(smooth_prog_, "sample_scale"),
                kResonanceSampleScale);
    glUniform1f(glGetUniformLocation(smooth_prog_, "sample_range"),
                kResonanceSampleRange);
    glUniform1f(glGetUniformLocation(smooth_prog_, "smooth_factor"),
                kResonanceSmoothFactor);
    glUseProgram(0);

    ready_ = true;
    return true;
}

void ResonanceAudioStages::destroy() {
    GLuint progs[] = {pass_prog_, gravity_prog_, average_prog_, smooth_prog_};
    for (GLuint p : progs)
        if (p)
            glDeleteProgram(p);
    pass_prog_ = gravity_prog_ = average_prog_ = smooth_prog_ = 0;

    GLuint texs[] = {raw_l_,        raw_r_,         pass_.tex_l,
                     pass_.tex_r,   average_.tex_l, average_.tex_r,
                     smooth_.tex_l, smooth_.tex_r};
    for (GLuint t : texs)
        if (t)
            glDeleteTextures(1, &t);
    GLuint fbos[] = {pass_.fbo, average_.fbo, smooth_.fbo};
    for (GLuint f : fbos)
        if (f)
            glDeleteFramebuffers(1, &f);
    for (Target &r : ring_) {
        if (r.tex_l)
            glDeleteTextures(1, &r.tex_l);
        if (r.tex_r)
            glDeleteTextures(1, &r.tex_r);
        if (r.fbo)
            glDeleteFramebuffers(1, &r.fbo);
    }

    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);

    *this = ResonanceAudioStages{};
}

GLuint ResonanceAudioStages::ensure_tex(GLuint &tex, int size) {
    if (tex)
        return tex;
    std::vector<uint16_t> zeros(static_cast<size_t>(size), 0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, size, 1, 0, GL_RED,
                 GL_UNSIGNED_SHORT, zeros.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void ResonanceAudioStages::upload_raw(GLuint tex, const float *data, int size) {
    static thread_local std::vector<uint16_t> buf;
    quantize(buf, data, size);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, size, 1, 0, GL_RED,
                 GL_UNSIGNED_SHORT, buf.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ResonanceAudioStages::bind_target(Target &t, int offset, int size) {
    if (!t.fbo)
        glGenFramebuffers(1, &t.fbo);
    GLuint &tex = (offset == 1) ? t.tex_r : t.tex_l;
    ensure_tex(tex, size);
    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           tex, 0);
}

void ResonanceAudioStages::draw_quad() {
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}

void ResonanceAudioStages::run_channel(int offset, int size) {
    int &oidx = (offset == 1) ? out_idx_ : out_idx_l_;
    GLuint raw = (offset == 1) ? raw_r_ : raw_l_;
    GLuint pass_tex = (offset == 1) ? pass_.tex_r : pass_.tex_l;

    bind_target(pass_, offset, size);
    pass_tex = (offset == 1) ? pass_.tex_r : pass_.tex_l;
    glViewport(0, 0, size, 1);
    glUseProgram(pass_prog_);
    glActiveTexture(GL_TEXTURE0 + offset);
    glBindTexture(GL_TEXTURE_2D, raw);
    glUniform1i(glGetUniformLocation(pass_prog_, "audioR"), offset);
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    draw_quad();
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);
    glFinish();

    glUseProgram(gravity_prog_);
    glActiveTexture(GL_TEXTURE0 + offset);
    glBindTexture(GL_TEXTURE_2D, pass_tex);
    glUniform1i(glGetUniformLocation(gravity_prog_, "audioR"), offset);
    glUniform1f(glGetUniformLocation(gravity_prog_, "diff"),
                kResonanceGravityStep / static_cast<float>(kResonanceFps));
    glViewport(0, 0, size, 1);
    draw_quad();
    glFinish();

    bind_target(ring_[oidx], offset, size);
    glViewport(0, 0, size, 1);
    glUseProgram(pass_prog_);
    glActiveTexture(GL_TEXTURE0 + offset);
    glBindTexture(GL_TEXTURE_2D, pass_tex);
    glUniform1i(glGetUniformLocation(pass_prog_, "audioR"), offset);
    draw_quad();
    glFinish();

    bind_target(average_, offset, size);
    glViewport(0, 0, size, 1);
    glUseProgram(average_prog_);
    glUniform1i(glGetUniformLocation(average_prog_, "avgFrames"),
                kResonanceGravityAverageFrames);
    for (int t = 0; t < kRing; ++t) {
        int unit = offset + 1 + t;
        int fr = oidx - t;
        if (fr < 0)
            fr += kRing;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D,
                      (offset == 1) ? ring_[fr].tex_r : ring_[fr].tex_l);
        char name[12];
        std::snprintf(name, sizeof(name), "audioR%d", t);
        glUniform1i(glGetUniformLocation(average_prog_, name), unit);
    }
    glFinish();
    draw_quad();
    glFinish();

    oidx = (oidx + 1) % kRing;

    bind_target(smooth_, offset, size);
    glViewport(0, 0, size, 1);
    glUseProgram(smooth_prog_);
    glUniform1i(glGetUniformLocation(smooth_prog_, "audioRSize"), size);
    glUniform1i(glGetUniformLocation(smooth_prog_, "adjacentSampleNums"),
                kResonanceAdjacentSampleNums);
    glActiveTexture(GL_TEXTURE0 + offset);
    glBindTexture(GL_TEXTURE_2D,
                  (offset == 1) ? average_.tex_r : average_.tex_l);
    glUniform1i(glGetUniformLocation(smooth_prog_, "audioR"), offset);
    glFinish();
    draw_quad();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

bool ResonanceAudioStages::run(int size, const std::vector<float> &l,
                               const std::vector<float> &r) {
    if (!ready_ || size <= 0)
        return false;
    if (static_cast<int>(l.size()) < size || static_cast<int>(r.size()) < size)
        return false;

    size_ = size;

    if (!raw_r_) {
        std::vector<uint16_t> zeros(static_cast<size_t>(size), 0);
        for (GLuint *t : {&raw_r_, &raw_l_}) {
            glGenTextures(1, t);
            glBindTexture(GL_TEXTURE_2D, *t);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, size, 1, 0, GL_RED,
                         GL_UNSIGNED_SHORT, zeros.data());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    upload_raw(raw_r_, r.data(), size);
    upload_raw(raw_l_, l.data(), size);

    glDisable(GL_SCISSOR_TEST);
    run_channel(1, size);
    run_channel(2, size);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    return true;
}
