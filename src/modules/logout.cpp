#include <GLES3/gl32.h>
#include <algorithm>
#include <cairo/cairo-ft.h>
#include <cairo/cairo.h>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <ft2build.h>

#include "app/monitor_output.h"
#include "app/wayland_state.h"

#include "core/async_process.h"
#include "core/log.h"

#include "modules/logout.h"

#include "render/color_ops.h"
#include "render/gl.h"
#include "render/layer_surface.h"
#include "render/node.h"

#include FT_FREETYPE_H

void thunder_burst_draw(ThunderBurst &tb, Renderer &renderer,
                        const ThunderParams &p) {
    if (!tb.bolt_tried) {
        tb.bolt_tried = true;
        tb.bolt_program = gl_compile_program_files(
            "renderer/quad.vert", "logout/thunder_burst.frag",
            "thunder_bolt");
        klog("logout: thunder bolt_program=%u", tb.bolt_program);
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

    if (!tb.bolt_logged) {
        tb.bolt_logged = true;
        klog("logout: bolt thick=%.3f amp=%.2f intensity=%.3f progress=%.3f "
             "seed=%.2f quad=%.0fx%.0f a=(%.0f,%.0f) b=(%.0f,%.0f) time=%.2f",
             p.thick, p.amp, p.intensity, p.progress, p.seed, w, h, ax, ay, bx,
             by, p.time_s);
    }

    renderer.draw_custom(tb.bolt_program, min_x, min_y, w, h, [&](GLuint prog) {
        static bool loc_logged = false;
        if (!loc_logged) {
            loc_logged = true;
            klog("logout: bolt loc a_pos=%d size=%d a=%d b=%d seed=%d "
                 "time=%d",
                 glGetAttribLocation(prog, "a_pos"),
                 glGetUniformLocation(prog, "u_size"),
                 glGetUniformLocation(prog, "u_a"),
                 glGetUniformLocation(prog, "u_b"),
                 glGetUniformLocation(prog, "u_seed"),
                 glGetUniformLocation(prog, "u_time"));
        }
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
        tb.shock_program = gl_compile_program_files(
            "renderer/quad.vert", "logout/thunder_shock.frag",
            "thunder_shock");
        klog("logout: thunder shock_program=%u", tb.shock_program);
    }
    if (!tb.shock_program || !p.core || !p.glow || p.radius <= 0.0f)
        return;

    if (!tb.shock_logged) {
        tb.shock_logged = true;
        klog("logout: thunder shock draw radius=%.2f intensity=%.3f "
             "progress=%.3f",
             p.radius, p.intensity, p.progress);
    }

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

Rect logout_detail_button_rect(int index, float center_x, float center_y) {
    float angle =
        kLogoutStartAngle + kLogoutStepAngle * static_cast<float>(index);
    float radius = kLogoutButtonsRadius;
    float x = center_x + radius * std::cos(angle) - kLogoutButtonSize / 2.0f;
    float y = center_y + radius * std::sin(angle) - kLogoutButtonSize / 2.0f;
    return {x, y, kLogoutButtonSize, kLogoutButtonSize};
}

namespace {

struct YujiMaiFont {
    FT_Face face = nullptr;
    cairo_font_face_t *cairo_face = nullptr;
};

YujiMaiFont &yujimai_font() {
    static YujiMaiFont font = [] {
        auto t0 = std::chrono::steady_clock::now();
        YujiMaiFont f;
        static FT_Library library;
        if (FT_Init_FreeType(&library)) {
            klog("logout: FT_Init_FreeType failed");
            return f;
        }
        const char *candidates[] = {
            KOKUSEI_YUJIMAI_FONT,
            "assets/fonts/YujiMai.ttf",
        };
        for (const char *path : candidates) {
            if (FT_New_Face(library, path, 0, &f.face) == 0) {
                klog("logout: loaded YujiMai from %s", path);
                break;
            }
            f.face = nullptr;
        }
        if (!f.face) {
            klog("logout: failed to load YujiMai.ttf");
            return f;
        }
        f.cairo_face = cairo_ft_font_face_create_for_ft_face(f.face, 0);
        klog("logout: YujiMai init %.0fms",
             std::chrono::duration<float, std::milli>(
                 std::chrono::steady_clock::now() - t0)
                 .count());
        return f;
    }();
    return font;
}

uint32_t decode_utf8_codepoint(const std::string &s) {
    if (s.empty())
        return 0;
    unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (c0 < 0x80)
        return c0;
    if ((c0 & 0xE0) == 0xC0 && s.size() >= 2) {
        return static_cast<uint32_t>((c0 & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((c0 & 0xF0) == 0xE0 && s.size() >= 3) {
        return (static_cast<uint32_t>(c0 & 0x0F) << 12) |
               (static_cast<uint32_t>(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((c0 & 0xF8) == 0xF0 && s.size() >= 4) {
        return (static_cast<uint32_t>(c0 & 0x07) << 18) |
               (static_cast<uint32_t>(s[1] & 0x3F) << 12) |
               (static_cast<uint32_t>(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    return 0;
}

uint64_t button_scale_owner(int i) { return 30 + static_cast<uint64_t>(i); }
uint64_t button_border_owner(int i) { return 40 + static_cast<uint64_t>(i); }
uint64_t edge_slash_owner(int e) { return 50 + static_cast<uint64_t>(e); }
uint64_t button_push_owner(int i) { return 60 + static_cast<uint64_t>(i); }
uint64_t button_gate_owner(int i) { return 80 + static_cast<uint64_t>(i); }

int star_vertex(int step_index) {
    return step_index * kLogoutStarStep % kLogoutButtonCount;
}

void schedule_after(AnimationManager &anim, float delay_ms, uint64_t owner,
                    std::function<void()> fn) {
    anim.animate(
        0.0f, 1.0f, delay_ms, Easing::Linear, [](float) {}, std::move(fn),
        owner);
}

void cancel_open_close_tweens(LogoutState &state) {
    AnimationManager &a = state.base.animations;
    a.cancelForOwner(kLogoutLogoOwner);
    a.cancelForOwner(kLogoutInputReadyOwner);
    a.cancelForOwner(kLogoutCloseChainOwner);
    a.cancelForOwner(kLogoutExitOwner);
    a.cancelForOwner(kLogoutBurstOwner);
    a.cancelForOwner(kLogoutHoldOwner);
    for (int i = 0; i < kLogoutButtonCount; ++i) {
        a.cancelForOwner(edge_slash_owner(i));
        a.cancelForOwner(button_push_owner(i));
        a.cancelForOwner(button_gate_owner(i));
    }
}

void set_button_highlight(LogoutState &state, int i, bool on) {
    if (i < 0 || i >= kLogoutButtonCount)
        return;
    size_t idx = static_cast<size_t>(i);
    float target = on ? 1.0f : 0.0f;
    state.base.animations.animate(
        state.button_highlight_scale[idx], target, kLogoutButtonScaleMs,
        Easing::EaseOutCubic,
        [&state, idx](float v) { state.button_highlight_scale[idx] = v; }, {},
        button_scale_owner(i));
    state.base.animations.animate(
        state.button_highlight_border[idx], target, kLogoutButtonBorderMs,
        Easing::EaseOutCubic,
        [&state, idx](float v) { state.button_highlight_border[idx] = v; }, {},
        button_border_owner(i));
}

bool is_highlighted(const LogoutState &state, int i) {
    return i == state.selected_index || i == state.hovered_index;
}

void update_highlight(LogoutState &state, int i) {
    set_button_highlight(state, i, is_highlighted(state, i));
}

void finish_close(LogoutState &state) {
    animated_image_hide(state.logo);
    state.base.open = false;
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        state.base.layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    overlay_panel_update_input_region(state.base);
    wl_surface_commit(state.base.surface);
}

void start_burst(LogoutState &state) {
    state.burst = 0.0f;
    state.base.animations.animate(
        0.0f, 1.0f, kLogoutBurstMs, Easing::EaseOutCubic,
        [&state](float v) { state.burst = v; },
        [&state] {
            state.input_ready = true;
            update_highlight(state, state.selected_index);
        },
        kLogoutBurstOwner);

    for (int i = 0; i < kLogoutButtonCount; ++i) {
        size_t idx = static_cast<size_t>(i);
        state.base.animations.animate(
            0.0f, 1.0f, kLogoutPushMs, Easing::EaseOutBack,
            [&state, idx](float v) { state.button_travel[idx] = v; }, {},
            button_push_owner(i));
    }
}

void start_exit_burst(LogoutState &state) {
    state.burst = 0.0f;
    state.exit_fade = 1.0f;
    state.base.animations.animate(
        0.0f, 1.0f, kLogoutBurstMs, Easing::EaseOutCubic,
        [&state](float v) { state.burst = v; },
        [&state] { finish_close(state); }, kLogoutBurstOwner);

    state.base.animations.animate(
        1.0f, 0.0f, kLogoutExitFadeMs, Easing::EaseOutCubic,
        [&state](float v) { state.exit_fade = v; }, {}, kLogoutExitOwner);

    for (int i = 0; i < kLogoutButtonCount; ++i) {
        size_t idx = static_cast<size_t>(i);
        state.base.animations.animate(
            state.button_travel[idx], 1.0f + kLogoutExitSpread,
            kLogoutExitFadeMs, Easing::EaseOutCubic,
            [&state, idx](float v) { state.button_travel[idx] = v; }, {},
            button_push_owner(i));
    }
}

void start_slashes(LogoutState &state) {
    float step = kLogoutSlashMs * kLogoutSlashAdvanceFrac;
    for (int e = 0; e < kLogoutButtonCount; ++e) {
        size_t idx = static_cast<size_t>(e);
        float delay = static_cast<float>(e) * step;
        schedule_after(
            state.base.animations, delay, button_gate_owner(e), [&state, idx] {
                state.base.animations.animate(
                    0.0f, 1.0f, kLogoutSlashMs, Easing::EaseOutCubic,
                    [&state, idx](float v) { state.slash[idx] = v; }, {},
                    edge_slash_owner(static_cast<int>(idx)));
            });
    }

    float slash_time =
        static_cast<float>(kLogoutButtonCount - 1) * step + kLogoutSlashMs;
    float wait = slash_time + kLogoutHoldMs;
    schedule_after(state.base.animations, wait, kLogoutHoldOwner, [&state] {
        if (state.exiting)
            start_exit_burst(state);
        else
            start_burst(state);
    });
}

void start_open_sequence(LogoutState &state) {
    cancel_open_close_tweens(state);
    animated_image_show(state.logo,
                        [&state] { logout_request_frame(state); });
    state.exiting = false;
    state.input_ready = false;
    state.logo_scale = 0.0f;
    state.exit_fade = 0.0f;
    state.burst = 0.0f;
    state.burst_started = std::chrono::steady_clock::now();
    for (int i = 0; i < kLogoutButtonCount; ++i) {
        state.slash[static_cast<size_t>(i)] = 0.0f;
        state.button_travel[static_cast<size_t>(i)] = 0.0f;
    }

    state.base.animations.animate(
        0.0f, 1.0f, kLogoutLogoAnimMs, Easing::EaseOutBack,
        [&state](float v) { state.logo_scale = v; },
        [&state] {
            schedule_after(state.base.animations, kLogoutHoldMs,
                           kLogoutHoldOwner,
                           [&state] { start_slashes(state); });
        },
        kLogoutLogoOwner);
}

void start_close_sequence(LogoutState &state) {
    cancel_open_close_tweens(state);
    state.input_ready = false;
    state.exiting = true;
    state.burst = 0.0f;
    state.exit_fade = 1.0f;
    state.burst_started = std::chrono::steady_clock::now();
    int prev_hovered = state.hovered_index;
    state.hovered_index = -1;
    set_button_highlight(state, state.selected_index, false);
    set_button_highlight(state, prev_hovered, false);
    for (int e = 0; e < kLogoutButtonCount; ++e)
        state.slash[static_cast<size_t>(e)] = 0.0f;

    start_slashes(state);
}

void fast_hide(LogoutState &state) {
    cancel_open_close_tweens(state);
    for (int i = 0; i < kLogoutButtonCount; ++i) {
        size_t idx = static_cast<size_t>(i);
        state.slash[idx] = 0.0f;
        state.button_travel[idx] = 0.0f;
        state.button_highlight_scale[idx] = 0.0f;
        state.button_highlight_border[idx] = 0.0f;
        state.base.animations.cancelForOwner(button_scale_owner(i));
        state.base.animations.cancelForOwner(button_border_owner(i));
    }
    state.hovered_index = -1;
    state.logo_scale = 0.0f;
    state.exit_fade = 0.0f;
    state.burst = 0.0f;
    state.exiting = false;
    state.input_ready = false;
    finish_close(state);
}

} // namespace

RasterizedText rasterize_yujimai_glyph(const std::string &codepoint_utf8) {
    RasterizedText result;
    YujiMaiFont &font = yujimai_font();
    if (!font.cairo_face)
        return result;

    uint32_t codepoint = decode_utf8_codepoint(codepoint_utf8);
    FT_UInt glyph_index = FT_Get_Char_Index(font.face, codepoint);
    if (glyph_index == 0) {
        klog("logout: no glyph for codepoint U+%04X", codepoint);
        return result;
    }

    cairo_matrix_t font_matrix;
    cairo_matrix_init_scale(&font_matrix, kLogoutGlyphPx, kLogoutGlyphPx);
    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_scaled_font_t *scaled_font = cairo_scaled_font_create(
        font.cairo_face, &font_matrix, &ctm, kokusei_font_options());

    cairo_glyph_t measure_glyph = {glyph_index, 0, 0};
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(scaled_font, &measure_glyph, 1, &extents);

    int width = static_cast<int>(std::ceil(extents.width));
    int height = static_cast<int>(std::ceil(extents.height));
    if (width <= 0 || height <= 0) {
        cairo_scaled_font_destroy(scaled_font);
        return result;
    }

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    cairo_set_scaled_font(cr, scaled_font);
    cairo_set_source_rgba(cr, 1, 1, 1, 1);
    cairo_glyph_t draw_glyph = {glyph_index, -extents.x_bearing,
                                -extents.y_bearing};
    cairo_show_glyphs(cr, &draw_glyph, 1);
    cairo_surface_flush(surface);

    result = surface_to_rgba(surface, width, height);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    cairo_scaled_font_destroy(scaled_font);
    return result;
}

bool logout_create_surface(LogoutState &state, wl_compositor *compositor,
                             zwlr_layer_shell_v1 *layer_shell,
                             wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-logout", output);
}

bool logout_init_egl(LogoutState &state, Renderer &renderer,
                       EGLDisplay display, EGLConfig config,
                       EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state] { logout_paint(state); };
    return true;
}

void logout_retarget(LogoutState &state, wl_compositor *compositor,
                       zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                       Renderer &renderer, EGLDisplay egl_display,
                       EGLConfig egl_config, EGLContext egl_context,
                       wl_output *target_output, const char *target_name) {
    wl_output *bound = overlay_panel_retarget(
        state.base, display, state.bound_output, target_output, target_name,
        [&](wl_output *out) {
            return logout_create_surface(state, compositor, layer_shell, out);
        },
        [&] {
            return logout_init_egl(state, renderer, egl_display, egl_config,
                                     egl_context);
        });
    if (bound)
        state.bound_output = bound;
}

void logout_request_frame(LogoutState &state) {
    overlay_panel_request_frame(state.base);
}

void logout_apply_logo_config(LogoutState &state, bool animated) {
    if (state.logo_source_set && animated == state.logo_animated)
        return;

    const char *candidates[2];
    if (animated) {
        candidates[0] = KOKUSEI_LOGOUT_LOGO;
        candidates[1] = "assets/logout/logo.gif";
    } else {
        candidates[0] = KOKUSEI_LOGOUT_LOGO_STATIC;
        candidates[1] = "assets/logout/logo.png";
    }
    std::string path = candidates[1];
    for (const char *candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            path = candidate;
            break;
        }
    }

    AnimatedImageStyle style;
    style.size = kLogoutLogoSize;
    style.decode = {animated ? 30 : 1, static_cast<int>(kLogoutLogoSize)};
    if (animated) {
        style.circular = true;
        style.border_width = kLogoutBorderWidth;
        style.border_color = rgba(palette::accent);
    }
    animated_image_set_source(state.logo, path, style);
    state.logo_animated = animated;
    state.logo_source_set = true;
}

void logout_toggle(LogoutState &state, bool by_widget) {
    if (!state.base.layer_surface || state.base.egl_surface == EGL_NO_SURFACE)
        return;

    bool opening = !state.base.open;
    klog("logout: toggle open=%d by_widget=%d", opening, by_widget);
    if (opening) {
        state.selected_index = 0;
        state.base.open = true;
        state.base.opacity = 1.0f;
        zwlr_layer_surface_v1_set_keyboard_interactivity(
            state.base.layer_surface,
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
        overlay_panel_update_input_region(state.base);
        wl_surface_commit(state.base.surface);
        state.opened_by_widget = by_widget;
        start_open_sequence(state);
    } else {
        start_close_sequence(state);
    }
    overlay_panel_request_frame(state.base);
}

std::vector<IpcHandler> logout_ipc_handlers(LogoutState &logout,
                                              WaylandState &state) {
    return {
        {"logout",
         [&logout, &state] {
             if (!logout.base.open) {
                 MonitorOutput *target =
                     app_detail::active_target_monitor(state);
                 if (target && (target->output.wl != logout.bound_output ||
                                !logout.base.layer_surface))
                     logout_retarget(
                         logout, state.compositor, state.layer_shell,
                         state.display, state.renderer, state.egl_display,
                         state.egl_config, state.egl_context, target->output.wl,
                         target->output.name.c_str());
             }
             logout_apply_logo_config(logout,
                                        state.cfg.logout_animated_logo);
             logout_toggle(logout);
         },
         "toggle the logout overlay"},
    };
}

void logout_execute(LogoutState &state, int index) {
    if (index < 0 || index >= kLogoutButtonCount)
        return;
    const char *cmd = kLogoutActions[static_cast<size_t>(index)].command;
    if (cmd && cmd[0] != '\0')
        spawn_detached(cmd);
    state.selected_index = 0;
    fast_hide(state);
    overlay_panel_request_frame(state.base);
}

void logout_handle_key_event(LogoutState &state, const KeyEvent &event) {
    if (!state.input_ready)
        return;
    switch (event.kind) {
    case KeyKind::Left: {
        int old = state.selected_index;
        state.selected_index =
            (state.selected_index + kLogoutButtonCount - 1) %
            kLogoutButtonCount;
        update_highlight(state, old);
        update_highlight(state, state.selected_index);
        break;
    }
    case KeyKind::Right: {
        int old = state.selected_index;
        state.selected_index =
            (state.selected_index + 1) % kLogoutButtonCount;
        update_highlight(state, old);
        update_highlight(state, state.selected_index);
        break;
    }
    case KeyKind::Text:
        if (event.text.size() == 1 && event.text[0] >= '1' &&
            event.text[0] <= '8') {
            int old = state.selected_index;
            state.selected_index = event.text[0] - '1';
            if (old != state.selected_index) {
                update_highlight(state, old);
                update_highlight(state, state.selected_index);
            }
        }
        break;
    case KeyKind::Enter:
        logout_execute(state, state.selected_index);
        break;
    case KeyKind::Escape:
        logout_toggle(state);
        break;
    default:
        break;
    }
}

void logout_handle_click(LogoutState &state, double px, double py) {
    float cx = static_cast<float>(state.base.width) / 2.0f;
    float cy = static_cast<float>(state.base.height) / 2.0f;
    for (int i = 0; i < kLogoutButtonCount; ++i) {
        Rect r = logout_detail_button_rect(i, cx, cy);
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
            if (state.input_ready)
                logout_execute(state, i);
            return;
        }
    }
    logout_toggle(state);
}

void logout_handle_hover(LogoutState &state, double px, double py) {
    if (!state.input_ready)
        return;
    float cx = static_cast<float>(state.base.width) / 2.0f;
    float cy = static_cast<float>(state.base.height) / 2.0f;
    int found = -1;
    for (int i = 0; i < kLogoutButtonCount; ++i) {
        Rect r = logout_detail_button_rect(i, cx, cy);
        if (px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h) {
            found = i;
            break;
        }
    }
    if (found == state.hovered_index)
        return;
    int old = state.hovered_index;
    state.hovered_index = found;
    update_highlight(state, old);
    update_highlight(state, found);
}

void logout_clear_hover(LogoutState &state) {
    if (state.hovered_index == -1)
        return;
    int old = state.hovered_index;
    state.hovered_index = -1;
    update_highlight(state, old);
}

void logout_paint(LogoutState &state) {
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    auto now = std::chrono::steady_clock::now();
    state.base.animations.tick(now);
    animated_image_tick(state.logo, now);
    if (!gl_make_current(state.base.egl_display, state.base.egl_surface,
                         state.base.egl_context))
        return;
    state.renderer->begin_frame(state.base.width, state.base.height,
                                state.base.output_scale.scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.scene.rebuild();

    std::deque<Color> blend;
    if (state.base.open) {
        float cx = static_cast<float>(state.base.width) / 2.0f;
        float cy = static_cast<float>(state.base.height) / 2.0f;

        for (int i = 0; i < kLogoutButtonCount; ++i) {
            size_t idx = static_cast<size_t>(i);
            float t = state.button_travel[idx];
            float visible = state.exiting
                                ? state.exit_fade
                                : (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
            if (visible <= 0.002f)
                continue;

            Rect fin = logout_detail_button_rect(i, cx, cy);
            float fcx = fin.x + fin.w / 2.0f;
            float fcy = fin.y + fin.h / 2.0f;
            float bx = cx + (fcx - cx) * t;
            float by = cy + (fcy - cy) * t;

            float highlight_scale =
                1.0f + (kLogoutHighlightScale - 1.0f) *
                           state.button_highlight_scale[idx];
            float scale = (state.exiting ? 1.0f : t) * highlight_scale;
            float w = fin.w * scale;
            float h = fin.h * scale;
            float x = bx - w / 2.0f;
            float y = by - h / 2.0f;

            blend.push_back(with_alpha(palette::field_bg, visible));
            const Color &fill = blend.back();
            blend.push_back(
                with_alpha(lerp_color(palette::accent, palette::accent_alt,
                                      state.button_highlight_border[idx]),
                           visible));
            const Color &border = blend.back();

            Node *btn = state.scene.root.claim_child();
            btn->kind = NodeKind::RoundedRect;
            btn->x = x;
            btn->y = y;
            btn->w = w;
            btn->h = h;
            btn->radius = kLogoutButtonCornerRadius * scale;
            btn->border_width = kLogoutBorderWidth;
            btn->fill = rgba(fill);
            btn->border = rgba(border);

            Texture &tex = state.glyph_tex[idx];
            if (!tex.id) {
                RasterizedText rt =
                    rasterize_yujimai_glyph(kLogoutActions[idx].glyph_utf8);
                if (rt.width > 0)
                    tex =
                        make_texture_rgba(rt.width, rt.height, rt.rgba.data());
            }
            if (tex.id) {
                blend.push_back(with_alpha(palette::text, visible));
                const Color &glyph_tint = blend.back();
                Node *glyph = state.scene.root.claim_child();
                glyph->kind = NodeKind::Texture;
                glyph->x = bx - tex.width / 2.0f;
                glyph->y = by - tex.height / 2.0f;
                glyph->w = static_cast<float>(tex.width);
                glyph->h = static_cast<float>(tex.height);
                glyph->tex = &tex;
                glyph->tint = rgba(glyph_tint);
            }
        }

        float ls = kLogoutLogoSize;
        Node *logo_group = node_add_group(&state.scene.root, cx - ls / 2.0f,
                                          cy - ls / 2.0f, ls, ls);
        logo_group->scale = state.logo_scale;
        animated_image_draw(state.logo, logo_group, 0.0f, 0.0f, ls, ls,
                            state.exiting ? state.exit_fade : 1.0f);
    }

    state.renderer->set_opacity(state.base.opacity);
    state.scene.draw(*state.renderer);
    state.renderer->set_opacity(1.0f);

    if (state.base.open) {
        float cx = static_cast<float>(state.base.width) / 2.0f;
        float cy = static_cast<float>(state.base.height) / 2.0f;
        float time_s =
            std::chrono::duration<float>(now - state.burst_started).count();
        Color core = with_alpha(palette::text, 1.0f);
        Color glow = with_alpha(palette::electro, 1.0f);

        ThunderParams p;
        p.time_s = time_s;
        p.core = rgba(core);
        p.glow = rgba(glow);
        p.amp = kLogoutBoltAmp;

        for (int e = 0; e < kLogoutButtonCount; ++e) {
            float s = state.slash[static_cast<size_t>(e)];
            if (s <= 0.002f)
                continue;

            Rect va = logout_detail_button_rect(star_vertex(e), cx, cy);
            Rect vb = logout_detail_button_rect(star_vertex(e + 1), cx, cy);
            float ax = va.x + va.w / 2.0f;
            float ay = va.y + va.h / 2.0f;
            float bx = vb.x + vb.w / 2.0f;
            float by = vb.y + vb.h / 2.0f;
            float ox = (bx - ax) * kLogoutSlashOvershoot;
            float oy = (by - ay) * kLogoutSlashOvershoot;

            p.ax = ax - ox;
            p.ay = ay - oy;
            p.bx = bx + ox;
            p.by = by + oy;
            p.seed = static_cast<float>(e) * 1.7f + 1.0f;
            p.progress = s * 2.0f < 1.0f ? s * 2.0f : 1.0f;
            p.intensity = 4.0f * s * (1.0f - s) * 1.2f;
            thunder_burst_draw(state.thunder, *state.renderer, p);
        }

        if (state.burst > 0.002f && state.burst < 0.999f) {
            ThunderShockParams s;
            s.cx = cx;
            s.cy = cy;
            s.radius = kLogoutBurstRingMax;
            s.time_s = time_s;
            s.progress = state.burst;
            s.intensity = 1.3f;
            s.core = rgba(core);
            s.glow = rgba(glow);
            thunder_shock_draw(state.thunder, *state.renderer, s);

            p.ax = cx - kLogoutFinishSpan / 2.0f;
            p.ay = cy + kLogoutFinishRise / 2.0f;
            p.bx = cx + kLogoutFinishSpan / 2.0f;
            p.by = cy - kLogoutFinishRise / 2.0f;
            float sweep = state.burst * kLogoutFinishSweep;
            p.progress = sweep < 1.0f ? sweep : 1.0f;
            p.intensity = (1.0f - state.burst) * kLogoutFinishIntensity;
            p.seed = 21.0f;
            p.amp = kLogoutBoltAmp * 2.4f;
            p.thick = kLogoutFinishThick;
            thunder_burst_draw(state.thunder, *state.renderer, p);

            if (sweep >= 1.0f) {
                float head_end = 1.0f / kLogoutFinishSweep;
                float linger =
                    1.0f - (state.burst - head_end) / (1.0f - head_end);
                linger = linger < 0.0f ? 0.0f : linger;
                float crackle = 0.55f + 0.45f * std::sin(time_s * 71.0f) *
                                            std::sin(time_s * 127.0f);
                p.progress = 1.0f;
                p.intensity =
                    linger * linger * kLogoutFinishLingerIntensity * crackle;
                p.seed = 53.0f;
                p.amp = kLogoutBoltAmp * 1.5f;
                p.thick = kLogoutFinishThick * 0.55f;
                thunder_burst_draw(state.thunder, *state.renderer, p);
            }
        }
    }

    gl_check("logout_paint");
    auto sw0 = std::chrono::steady_clock::now();
    if (!eglSwapBuffers(state.base.egl_display, state.base.egl_surface))
        klog("logout: eglSwapBuffers failed, egl error 0x%04x",
             eglGetError());
    float sw = std::chrono::duration<float, std::milli>(
                   std::chrono::steady_clock::now() - sw0)
                   .count();
    if (sw > 5.0f)
        klog("logout: eglSwapBuffers %.1fms", sw);

    static int frame = 0;
    static std::chrono::steady_clock::time_point prev = now;
    if (++frame % 30 == 0) {
        int slashes = 0;
        for (int i = 0; i < kLogoutButtonCount; ++i)
            if (state.slash[static_cast<size_t>(i)] > 0.002f)
                ++slashes;
        klog("logout: paint #%d open=%d slashes=%d burst=%.2f dt=%.1fms",
             frame, state.base.open, slashes, state.burst,
             std::chrono::duration<float, std::milli>(now - prev).count());
    }
    prev = now;

    if (state.base.animations.hasActive() ||
        animated_image_animating(state.logo))
        overlay_panel_request_frame(state.base);
}
