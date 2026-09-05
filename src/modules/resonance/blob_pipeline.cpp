#include <chrono>
#include <vector>

#include "config/resonance_config.h"

#include "core/log.h"

#include "modules/resonance/blob_pipeline.h"
#include "modules/resonance/resonance_shaders.h"

#include "render/gl.h"
#include "render/palette.h"

namespace {

constexpr GLfloat kQuadVerts[18] = {-1, -1, 0, 1, -1, 0, -1, 1, 0,
                                    1,  1,  0, 1, -1, 0, -1, 1, 0};

int resonance_canvas_size(int width, int height) {
    int smaller = width < height ? width : height;
    int size = static_cast<int>(static_cast<float>(smaller) *
                                kResonanceCanvasFraction);
    return size < kResonanceCanvasMin ? kResonanceCanvasMin : size;
}

} // namespace

bool ResonanceBlobPipeline::init() {
    if (ready_)
        return true;

    std::string ncs1 = resonance_shaders::ncs1_fs();
    std::string ncs2 = resonance_shaders::ncs2_fs();
    std::string glow = resonance_shaders::glow_fs();

    ncs1_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                    ncs1.c_str(), "resonance_ncs1");
    ncs2_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                    ncs2.c_str(), "resonance_ncs2");
    glow_prog_ = gl_compile_program(resonance_shaders::kFullscreenVs,
                                    glow.c_str(), "resonance_glow");
    if (!ncs1_prog_ || !ncs2_prog_ || !glow_prog_) {
        klog("resonance_blob: shader compile failed");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts,
                 GL_STATIC_DRAW);
    glBindVertexArray(0);
    glGenFramebuffers(1, &clear_fbo_);

    ready_ = true;
    return true;
}

void ResonanceBlobPipeline::destroy() {
    GLuint progs[] = {ncs1_prog_, ncs2_prog_, glow_prog_};
    for (GLuint p : progs)
        if (p)
            glDeleteProgram(p);
    if (atomic_tex_)
        glDeleteTextures(1, &atomic_tex_);
    for (GLuint t : fbo_tex_)
        if (t)
            glDeleteTextures(1, &t);
    for (GLuint f : fbo_)
        if (f)
            glDeleteFramebuffers(1, &f);
    if (glow_tex_)
        glDeleteTextures(1, &glow_tex_);
    if (glow_fbo_)
        glDeleteFramebuffers(1, &glow_fbo_);
    if (clear_fbo_)
        glDeleteFramebuffers(1, &clear_fbo_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    *this = ResonanceBlobPipeline{};
}

void ResonanceBlobPipeline::ensure_targets(int canvas) {
    if (atomic_tex_ && canvas == canvas_)
        return;

    if (atomic_tex_) {
        glDeleteTextures(1, &atomic_tex_);
        atomic_tex_ = 0;
    }
    for (GLuint &t : fbo_tex_)
        if (t) {
            glDeleteTextures(1, &t);
            t = 0;
        }
    for (GLuint &f : fbo_)
        if (f) {
            glDeleteFramebuffers(1, &f);
            f = 0;
        }
    if (glow_tex_) {
        glDeleteTextures(1, &glow_tex_);
        glow_tex_ = 0;
    }
    if (glow_fbo_) {
        glDeleteFramebuffers(1, &glow_fbo_);
        glow_fbo_ = 0;
    }

    canvas_ = canvas;

    glGenTextures(1, &atomic_tex_);
    glBindTexture(GL_TEXTURE_2D, atomic_tex_);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, canvas, canvas);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, clear_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           atomic_tex_, 0);
    const GLuint zero[4] = {0, 0, 0, 0};
    glClearBufferuiv(GL_COLOR, 0, zero);

    GLuint *tex[] = {&fbo_tex_[0], &fbo_tex_[1], &glow_tex_};
    GLuint *fbo[] = {&fbo_[0], &fbo_[1], &glow_fbo_};
    for (int i = 0; i < 3; ++i) {
        glGenTextures(1, tex[i]);
        glBindTexture(GL_TEXTURE_2D, *tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, canvas, canvas, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, fbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, *tex[i], 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ResonanceBlobPipeline::draw_quad() {
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}

void ResonanceBlobPipeline::set_audio_uniforms(GLuint prog, GLuint audio_l_tex,
                                               GLuint audio_r_tex,
                                               int audio_size, int tick,
                                               int canvas,
                                               const ResonanceParams &params) {
    glUniform2f(glGetUniformLocation(prog, "resolution"),
                static_cast<float>(canvas), static_cast<float>(canvas));
    glUniform1f(glGetUniformLocation(prog, "time"), static_cast<float>(tick));
    glUniform1i(glGetUniformLocation(prog, "audioRSize"), audio_size);
    glUniform1i(glGetUniformLocation(prog, "audioLSize"), audio_size);
    glUniform3f(glGetUniformLocation(prog, "u_accent"), palette::accent.r,
                palette::accent.g, palette::accent.b);
    glUniform1f(glGetUniformLocation(prog, "particleThin"),
                params.particle_thin);
    glUniform1i(glGetUniformLocation(prog, "u_particleSize"),
                params.particle_size);
    glUniform1i(glGetUniformLocation(prog, "u_complexity"),
                params.fractal_complexity);
    glUniform1f(glGetUniformLocation(prog, "u_glowDirections"),
                params.glow_directions);
    glUniform1f(glGetUniformLocation(prog, "u_glowQuality"),
                params.glow_quality);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, audio_r_tex);
    glUniform1i(glGetUniformLocation(prog, "audioR"), 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, audio_l_tex);
    glUniform1i(glGetUniformLocation(prog, "audioL"), 2);

    glActiveTexture(GL_TEXTURE0);
}

void ResonanceBlobPipeline::render(int width, int height, int tick, float fade,
                                   GLuint audio_l_tex, GLuint audio_r_tex,
                                   int audio_size,
                                   const ResonanceParams &params) {
    if (!ready_ || width <= 0 || height <= 0)
        return;

    int canvas = resonance_canvas_size(width, height);

    static int trace_frames = 8;
    bool trace = trace_frames > 0;
    if (trace)
        --trace_frames;
    auto mark = [trace, tick](const char *tag) {
        if (!trace)
            return;
        auto t0 = std::chrono::steady_clock::now();
        glFinish();
        klog("resonance_blob: f%d %s %.1fms", tick, tag,
             std::chrono::duration<float, std::milli>(
                 std::chrono::steady_clock::now() - t0)
                 .count());
    };

    bool first_targets = atomic_tex_ == 0 || canvas != canvas_;
    ensure_targets(canvas);
    if (first_targets)
        mark("ensure_targets");

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    for (GLuint fbo : {fbo_[0], fbo_[1], glow_fbo_}) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, canvas, canvas);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glBindImageTexture(0, atomic_tex_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[0]);
    glUseProgram(ncs1_prog_);
    set_audio_uniforms(ncs1_prog_, audio_l_tex, audio_r_tex, audio_size, tick,
                       canvas, params);
    draw_quad();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    mark("ncs1");

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[1]);
    glUseProgram(ncs2_prog_);
    set_audio_uniforms(ncs2_prog_, audio_l_tex, audio_r_tex, audio_size, tick,
                       canvas, params);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex_[0]);
    glUniform1i(glGetUniformLocation(ncs2_prog_, "tex"), 0);
    draw_quad();
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                    GL_TEXTURE_FETCH_BARRIER_BIT);
    mark("ncs2");

    glBindFramebuffer(GL_FRAMEBUFFER, glow_fbo_);
    glUseProgram(glow_prog_);
    set_audio_uniforms(glow_prog_, audio_l_tex, audio_r_tex, audio_size, tick,
                       canvas, params);
    glUniform1f(glGetUniformLocation(glow_prog_, "u_fade"), fade);
    glUniform4f(glGetUniformLocation(glow_prog_, "u_backdrop"),
                palette::window_backdrop.r, palette::window_backdrop.g,
                palette::window_backdrop.b, palette::window_backdrop.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex_[1]);
    glUniform1i(glGetUniformLocation(glow_prog_, "tex"), 0);
    draw_quad();
    mark("glow");

    int off_x = (width - canvas) / 2;
    int off_y = (height - canvas) / 2;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(palette::window_backdrop.r, palette::window_backdrop.g,
                 palette::window_backdrop.b, palette::window_backdrop.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, glow_fbo_);
    glBlitFramebuffer(0, 0, canvas, canvas, off_x, off_y, off_x + canvas,
                      off_y + canvas, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}
