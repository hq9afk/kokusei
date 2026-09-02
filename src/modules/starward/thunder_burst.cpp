#include <GLES2/gl2.h>
#include <algorithm>
#include <cmath>

#include "modules/starward/thunder_burst.h"

#include "render/gl.h"
#include "render/renderer.h"

#include "shaders/renderer_shaders.h"

void thunder_burst_draw(ThunderBurst &tb, Renderer &renderer,
                        const ThunderParams &p) {
    if (!tb.bolt_tried) {
        tb.bolt_tried = true;
        tb.bolt_program = gl_compile_program(kRendererQuadVs, kThunderBurstFs);
    }
    if (!tb.bolt_program || !p.core || !p.glow)
        return;

    float pad = p.pad + p.amp * 3.0f + p.thick * 30.0f;
    float min_x = std::min(p.ax, p.bx) - pad;
    float min_y = std::min(p.ay, p.by) - pad;
    float w = std::fabs(p.bx - p.ax) + pad * 2.0f;
    float h = std::fabs(p.by - p.ay) + pad * 2.0f;

    float ax = p.ax - min_x;
    float ay = p.ay - min_y;
    float bx = p.bx - min_x;
    float by = p.by - min_y;

    renderer.draw_custom(tb.bolt_program, min_x, min_y, w, h, [&](GLuint prog) {
        glUniform2f(glGetUniformLocation(prog, "u_size"), w, h);
        glUniform2f(glGetUniformLocation(prog, "u_a"), ax, ay);
        glUniform2f(glGetUniformLocation(prog, "u_b"), bx, by);
        glUniform1f(glGetUniformLocation(prog, "u_time"), p.time_s);
        glUniform1f(glGetUniformLocation(prog, "u_progress"), p.progress);
        glUniform1f(glGetUniformLocation(prog, "u_intensity"), p.intensity);
        glUniform1f(glGetUniformLocation(prog, "u_seed"), p.seed);
        glUniform1f(glGetUniformLocation(prog, "u_amp"), p.amp);
        glUniform1f(glGetUniformLocation(prog, "u_thick"), p.thick);
        glUniform4fv(glGetUniformLocation(prog, "u_core"), 1, p.core);
        glUniform4fv(glGetUniformLocation(prog, "u_glow"), 1, p.glow);
    });
}

void thunder_shock_draw(ThunderBurst &tb, Renderer &renderer,
                        const ThunderShockParams &p) {
    if (!tb.shock_tried) {
        tb.shock_tried = true;
        tb.shock_program = gl_compile_program(kRendererQuadVs, kThunderShockFs);
    }
    if (!tb.shock_program || !p.core || !p.glow || p.radius <= 0.0f)
        return;

    float min_x = p.cx - p.radius;
    float min_y = p.cy - p.radius;
    float side = p.radius * 2.0f;

    float lcx = p.cx - min_x;
    float lcy = p.cy - min_y;

    renderer.draw_custom(
        tb.shock_program, min_x, min_y, side, side, [&](GLuint prog) {
            glUniform2f(glGetUniformLocation(prog, "u_size"), side, side);
            glUniform2f(glGetUniformLocation(prog, "u_center"), lcx, lcy);
            glUniform1f(glGetUniformLocation(prog, "u_time"), p.time_s);
            glUniform1f(glGetUniformLocation(prog, "u_progress"), p.progress);
            glUniform1f(glGetUniformLocation(prog, "u_radius"), p.radius);
            glUniform1f(glGetUniformLocation(prog, "u_intensity"), p.intensity);
            glUniform4fv(glGetUniformLocation(prog, "u_core"), 1, p.core);
            glUniform4fv(glGetUniformLocation(prog, "u_glow"), 1, p.glow);
        });
}
