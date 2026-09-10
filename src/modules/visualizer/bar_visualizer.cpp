#include "config/visualizer_config.h"

#include "core/log.h"

#include "modules/visualizer/bar_visualizer.h"

#include "render/gl.h"
#include "render/palette.h"

namespace {

constexpr GLfloat kQuadVerts[18] = {-1, -1, 0, 1, -1, 0, -1, 1, 0,
                                    1,  1,  0, 1, -1, 0, -1, 1, 0};

} // namespace

bool BarVisualizer::init() {
    if (ready_)
        return true;

    prog_ = gl_compile_program_files("visualizer/fullscreen.vert",
                                     "visualizer/bar/bar.frag", "visualizer_bar");
    if (!prog_) {
        klog("visualizer_bar: shader compile failed");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts,
                 GL_STATIC_DRAW);
    glBindVertexArray(0);

    ready_ = true;
    return true;
}

void BarVisualizer::destroy() {
    if (prog_)
        glDeleteProgram(prog_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    *this = BarVisualizer{};
}

void BarVisualizer::draw_quad() {
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
}

void BarVisualizer::render(int width, int height, int tick, float fade,
                          GLuint audio_l_tex, GLuint audio_r_tex,
                          int audio_size, const VisualizerParams &params) {
    (void)tick;
    (void)audio_size;
    (void)params;
    if (!ready_ || width <= 0 || height <= 0)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(palette::window_backdrop.r, palette::window_backdrop.g,
                 palette::window_backdrop.b, palette::window_backdrop.a * fade);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog_);

    float slot = kVisualizerBarWidth + kVisualizerBarSpacing;
    int bar_count = static_cast<int>(
        (static_cast<float>(width) - kVisualizerBarSpacing) / slot);
    bar_count = bar_count < 1 ? 1 : bar_count;

    glUniform2f(glGetUniformLocation(prog_, "u_resolution"),
                static_cast<float>(width), static_cast<float>(height));
    glUniform1f(glGetUniformLocation(prog_, "u_fade"), fade);
    glUniform3f(glGetUniformLocation(prog_, "u_accent"), palette::accent.r,
                palette::accent.g, palette::accent.b);
    glUniform1i(glGetUniformLocation(prog_, "u_barCount"), bar_count);
    glUniform1f(glGetUniformLocation(prog_, "u_barWidth"), kVisualizerBarWidth);
    glUniform1f(glGetUniformLocation(prog_, "u_barSpacing"),
                kVisualizerBarSpacing);
    glUniform1f(glGetUniformLocation(prog_, "u_barRadius"),
                kVisualizerBarRadius);
    glUniform1f(glGetUniformLocation(prog_, "u_barHeightRatio"),
                kVisualizerBarHeightRatio);
    glUniform1f(glGetUniformLocation(prog_, "u_barOpacity"),
                kVisualizerBarOpacity);
    glUniform1f(glGetUniformLocation(prog_, "u_minBarHeight"),
                kVisualizerBarMinHeight);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, audio_r_tex);
    glUniform1i(glGetUniformLocation(prog_, "audioR"), 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, audio_l_tex);
    glUniform1i(glGetUniformLocation(prog_, "audioL"), 2);

    glActiveTexture(GL_TEXTURE0);

    int band_h = static_cast<int>(kVisualizerBarHeightRatio *
                                  static_cast<float>(height)) +
                 1;
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, width, band_h);
    draw_quad();
    glDisable(GL_SCISSOR_TEST);

    glDisable(GL_BLEND);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
