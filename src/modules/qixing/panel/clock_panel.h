#pragma once

#include <EGL/egl.h>
#include <array>
#include <vector>
#include <wayland-client.h>

#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/input_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

constexpr float kClockPanelWidth = 280.0f;
constexpr float kClockWeekdayRowHeight = 22.0f;
constexpr float kClockGridTopGap = 2.0f;
constexpr float kClockCellCirclePadding = 4.0f;
constexpr float kClockNavButtonSize = 20.0f;
constexpr float kClockNavButtonGap = 6.0f;

struct CalendarDay {
    int year;
    int month;
    int day;
    bool in_month;
};

struct CalendarMonth {
    int year;
    int month;
};

std::array<CalendarDay, 42> clock_panel_cells(int year, int month);

CalendarMonth clock_panel_month_shifted(int year, int month, int delta);

bool clock_panel_same_day(const CalendarDay &cell, int year, int month,
                          int day);

struct ClockPanelState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;
    float locked_center_x = -1.0f;
    PanelHeightReveal reveal;
    int month_offset = 0;

    float pending_pill_center_x = 0.0f;
    float pending_qixing_height = 0.0f;
    float pending_qixing_top_margin = 0.0f;
};

bool clock_panel_create_surface(ClockPanelState &state,
                                wl_compositor *compositor,
                                zwlr_layer_shell_v1 *layer_shell,
                                wl_output *output = nullptr);

bool clock_panel_init_egl(ClockPanelState &state, Renderer &renderer,
                          EGLDisplay display, EGLConfig config,
                          EGLContext context);

void clock_panel_request_frame(ClockPanelState &state, float pill_center_x,
                               float qixing_height, float qixing_top_margin);

void clock_panel_toggle(ClockPanelState &state, float pill_center_x = -1.0f);

void clock_panel_handle_click(ClockPanelState &state, double px, double py);

void clock_panel_handle_key_event(ClockPanelState &state,
                                  const KeyEvent &event);

void clock_panel_paint(ClockPanelState &state, float pill_center_x,
                       float qixing_height, float qixing_top_margin);
