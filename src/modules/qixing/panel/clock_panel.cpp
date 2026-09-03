#include <GLES3/gl32.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <string>

#include "modules/qixing/panel/clock_panel.h"

#include "render/gl.h"
#include "render/icon.h"
#include "render/icons.h"
#include "render/layer_surface.h"
#include "render/palette.h"
#include "render/text.h"

namespace {

const char *month_name(int month) {
    static const std::array<const char *, 12> names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 0 || month > 11)
        return "";
    return names[static_cast<size_t>(month)];
}

const std::array<const char *, 7> kWeekdays = {"Mo", "Tu", "We", "Th",
                                               "Fr", "Sa", "Su"};

const float kClockTodayText[4] = {1.0f, 1.0f, 1.0f, 1.0f};

float content_width() { return kClockPanelWidth - 2.0f * kPanelPadding; }

float cell_size() { return content_width() / 7.0f; }

float panel_height() {
    float body_h =
        kClockWeekdayRowHeight + kClockGridTopGap + 6.0f * cell_size();
    return kPanelPadding + kPanelHeaderHeight + kPanelHeaderDividerGap + 1.0f +
           kPanelContentGap + body_h + kPanelTrailingSpacerHeight +
           kPanelPadding;
}

void draw_nav_button(Node *root, TextureCache &cache, int32_t scale,
                     std::vector<PanelClickRegion> &click_regions, float x,
                     float y, const char *glyph, const std::string &tag) {
    using namespace panel_chrome_detail;
    Rect r = {x, y, kClockNavButtonSize, kClockNavButtonSize};
    node_add_rrect(root, r.x, r.y, r.w, r.h, kClockNavButtonSize / 2.0f, 0.0f,
                   rgba(palette::overlay), rgba(palette::overlay));
    if (glyph) {
        const Texture *tex = cached_icon(cache, glyph, scale);
        if (tex)
            node_add_texture(root, r.x + (r.w - tex->width) / 2.0f,
                             r.y + (r.h - tex->height) / 2.0f, *tex,
                             rgba(palette::text));
    } else {
        float dot = 6.0f;
        node_add_rrect(root, r.x + (r.w - dot) / 2.0f, r.y + (r.h - dot) / 2.0f,
                       dot, dot, dot / 2.0f, 0.0f, rgba(palette::accent),
                       rgba(palette::accent));
    }
    click_regions.push_back({PanelClickKind::HeaderAction, r, tag});
}

} // namespace

bool clock_panel_create_surface(ClockPanelState &state,
                                wl_compositor *compositor,
                                zwlr_layer_shell_v1 *layer_shell,
                                wl_output *output) {
    return overlay_panel_create_surface(state.base, compositor, layer_shell,
                                        "kokusei-clock-panel", output);
}

bool clock_panel_init_egl(ClockPanelState &state, Renderer &renderer,
                          EGLDisplay display, EGLConfig config,
                          EGLContext context) {
    state.renderer = &renderer;
    if (!overlay_panel_init_egl(state.base, display, config, context))
        return false;
    state.base.frame_clock.draw = [&state] {
        clock_panel_paint(state, state.pending_pill_center_x,
                          state.pending_qixing_height,
                          state.pending_qixing_top_margin);
    };
    return true;
}

void clock_panel_request_frame(ClockPanelState &state, float pill_center_x,
                               float qixing_height, float qixing_top_margin) {
    state.pending_pill_center_x = pill_center_x;
    state.pending_qixing_height = qixing_height;
    state.pending_qixing_top_margin = qixing_top_margin;
    overlay_panel_request_frame(state.base);
}

void clock_panel_toggle(ClockPanelState &state, float pill_center_x) {
    panel_penance_toggle(
        state.base, state.locked_center_x, pill_center_x,
        [&state] { panel_reveal_open(state.reveal); },
        [&state] {
            state.month_offset = 0;
            panel_reveal_close(state.reveal, state.base,
                               [&state] { state.locked_center_x = -1.0f; });
        });
}

void clock_panel_handle_click(ClockPanelState &state, double px, double py) {
    auto hit = [](const Rect &r, double x, double y) {
        return r.w > 0 && x >= r.x && x < r.x + r.w && y >= r.y &&
               y < r.y + r.h;
    };

    for (const PanelClickRegion &region : state.click_regions) {
        if (!hit(region.rect, px, py))
            continue;
        if (region.kind == PanelClickKind::Close) {
            clock_panel_toggle(state);
        } else if (region.kind == PanelClickKind::HeaderAction) {
            if (region.tag == "prev")
                --state.month_offset;
            else if (region.tag == "next")
                ++state.month_offset;
            else
                state.month_offset = 0;
        }
        return;
    }

    if (!hit(state.panel_rect, px, py))
        clock_panel_toggle(state);
}

void clock_panel_handle_key_event(ClockPanelState &state,
                                  const KeyEvent &event) {
    if (event.kind == KeyKind::Escape)
        clock_panel_toggle(state);
}

void clock_panel_paint(ClockPanelState &state, float pill_center_x,
                       float qixing_height, float qixing_top_margin) {
    using namespace panel_chrome_detail;
    if (state.base.egl_surface == EGL_NO_SURFACE)
        return;
    state.base.animations.tick(std::chrono::steady_clock::now());
    gl_make_current(state.base.egl_display, state.base.egl_surface,
                    state.base.egl_context);
    int32_t scale = state.base.output_scale.scale;
    state.renderer->begin_frame(state.base.width, state.base.height, scale);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    state.click_regions.clear();
    state.panel_rect = {};
    state.scene.rebuild();

    if (!state.base.open) {
        state.scene.draw(*state.renderer);
        eglSwapBuffers(state.base.egl_display, state.base.egl_surface);
        return;
    }

    Node *root = &state.scene.root;

    float panel_w = kClockPanelWidth;
    if (state.locked_center_x < 0.0f)
        state.locked_center_x = pill_center_x;
    float total_h = panel_height();
    float clip_h = panel_reveal_tick(state.reveal, state.base, total_h);
    float panel_h = std::max(0.0f, state.reveal.target);
    float panel_x = std::clamp(
        state.locked_center_x - panel_w / 2.0f, kPanelSideMargin,
        static_cast<float>(state.base.width) - panel_w - kPanelSideMargin);
    float panel_y = qixing_height + qixing_top_margin + kPanelGap;
    state.panel_rect = {panel_x, panel_y, panel_w, panel_h};

    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    int today_y = local.tm_year + 1900;
    int today_m = local.tm_mon;
    int today_d = local.tm_mday;
    CalendarMonth disp =
        clock_panel_month_shifted(today_y, today_m, state.month_offset);

    panel_draw_box(root, panel_x, panel_y, panel_w, panel_h);
    float header_y = panel_y + kPanelPadding;
    std::string title =
        std::string(month_name(disp.month)) + " " + std::to_string(disp.year);
    float header_right =
        panel_draw_header(root, state.tcache, scale, title, panel_x, panel_y,
                          panel_w, state.click_regions);

    float nav_y = header_y + (kPanelHeaderHeight - kClockNavButtonSize) / 2.0f;
    float nav_x = header_right - kClockNavButtonSize;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x,
                    nav_y, icon::chevron_right, "next");
    nav_x -= kClockNavButtonSize + kClockNavButtonGap;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x,
                    nav_y, nullptr, "today");
    nav_x -= kClockNavButtonSize + kClockNavButtonGap;
    draw_nav_button(root, state.tcache, scale, state.click_regions, nav_x,
                    nav_y, icon::chevron_left, "prev");

    float divider_y = header_y + kPanelHeaderHeight + kPanelHeaderDividerGap;
    node_add_rect(root, panel_x + kPanelPadding, divider_y,
                  panel_w - 2.0f * kPanelPadding, 1.0f,
                  rgba(palette::text_alpha06));

    float content_x = panel_x + kPanelPadding;
    float content_top = divider_y + 1.0f + kPanelContentGap;
    float cell = cell_size();

    for (int col = 0; col < 7; ++col) {
        const Texture *tex = cached_text(
            state.tcache, kWeekdays[static_cast<size_t>(col)], scale);
        if (!tex)
            continue;
        float cx = content_x + col * cell + (cell - tex->width) / 2.0f;
        float cy = content_top + (kClockWeekdayRowHeight - tex->height) / 2.0f;
        node_add_texture(root, cx, cy, *tex, rgba(palette::text));
    }

    float grid_y = content_top + kClockWeekdayRowHeight + kClockGridTopGap;
    std::array<CalendarDay, 42> cells =
        clock_panel_cells(disp.year, disp.month);
    for (int i = 0; i < 42; ++i) {
        const CalendarDay &day = cells[static_cast<size_t>(i)];
        float cx = content_x + (i % 7) * cell;
        float cy = grid_y + (i / 7) * cell;
        bool is_today = clock_panel_same_day(day, today_y, today_m, today_d);

        if (is_today) {
            float d = cell - kClockCellCirclePadding * 2.0f;
            node_add_rrect(root, cx + (cell - d) / 2.0f, cy + (cell - d) / 2.0f,
                           d, d, d / 2.0f, 0.0f, rgba(palette::accent),
                           rgba(palette::accent));
        }

        const Texture *tex =
            cached_text(state.tcache, std::to_string(day.day), scale);
        if (!tex)
            continue;
        const float *text_col = is_today       ? kClockTodayText
                                : day.in_month ? rgba(palette::text)
                                               : rgba(palette::text_dim);
        node_add_texture(root, cx + (cell - tex->width) / 2.0f,
                         cy + (cell - tex->height) / 2.0f, *tex, text_col);
    }

    if (clip_h + 0.5f < panel_h) {
        ScopedClip clip(*state.renderer, panel_x, panel_y, panel_w,
                        std::max(0.0f, clip_h));
        state.scene.draw(*state.renderer);
    } else {
        state.scene.draw(*state.renderer);
    }
    eglSwapBuffers(state.base.egl_display, state.base.egl_surface);

    if (state.base.animations.hasActive())
        overlay_panel_request_frame(state.base);
}

std::array<CalendarDay, 42> clock_panel_cells(int year, int month) {
    std::chrono::year_month_day first{
        std::chrono::year{year} /
        std::chrono::month{static_cast<unsigned>(month + 1)} /
        std::chrono::day{1}};
    std::chrono::sys_days first_days{first};
    unsigned start_offset =
        (std::chrono::weekday{first_days}.c_encoding() + 6u) % 7u;
    std::chrono::sys_days grid_start =
        first_days - std::chrono::days{static_cast<int>(start_offset)};

    std::array<CalendarDay, 42> cells{};
    for (int i = 0; i < 42; ++i) {
        std::chrono::year_month_day ymd{grid_start + std::chrono::days{i}};
        int cell_month =
            static_cast<int>(static_cast<unsigned>(ymd.month())) - 1;
        cells[static_cast<size_t>(i)] = {
            static_cast<int>(ymd.year()), cell_month,
            static_cast<int>(static_cast<unsigned>(ymd.day())),
            cell_month == month};
    }
    return cells;
}

CalendarMonth clock_panel_month_shifted(int year, int month, int delta) {
    std::chrono::year_month base =
        std::chrono::year{year} /
        std::chrono::month{static_cast<unsigned>(month + 1)};
    std::chrono::year_month shifted = base + std::chrono::months{delta};
    return {static_cast<int>(shifted.year()),
            static_cast<int>(static_cast<unsigned>(shifted.month())) - 1};
}

bool clock_panel_same_day(const CalendarDay &cell, int year, int month,
                          int day) {
    return cell.year == year && cell.month == month && cell.day == day;
}
