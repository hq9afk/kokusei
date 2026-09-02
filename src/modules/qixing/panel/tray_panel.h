#pragma once

#include <EGL/egl.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>

#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/popup_window.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/tray_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

constexpr float kTrayMenuWidth = 220.0f;
constexpr float kTrayMenuPadding = 4.0f;
constexpr float kTrayMenuItemHeight = 28.0f;
constexpr float kTrayMenuRowPaddingH = 8.0f;
constexpr float kTrayMenuLabelWidthOffset = 24.0f;
constexpr float kTrayMenuSeparatorHeight = 8.0f;
constexpr float kTrayMenuSeparatorLineHeight = 1.0f;
constexpr float kTrayMenuSeparatorWidthOffset = 12.0f;
constexpr float kTrayMenuBorderWidth = 1.0f;
constexpr float kTrayMenuRadius = 8.0f;

struct TrayMenuState {
    PopupWindowBase base;
    Renderer *renderer = nullptr;
    xdg_wm_base *wm_base = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;

    std::string item_key;
    std::vector<int32_t> menu_path;
    Rect anchor_cell;
    int32_t applied_h = 0;
};

struct TrayMenuOpenArgs {
    wl_compositor *compositor = nullptr;
    xdg_wm_base *wm_base = nullptr;
    zwlr_layer_surface_v1 *parent_layer = nullptr;
    wl_display *display = nullptr;
    EGLDisplay egl_display = nullptr;
    EGLConfig egl_config = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    wl_seat *seat = nullptr;
    uint32_t grab_serial = 0;
};

void tray_menu_paint(TrayMenuState &state, TrayState &tray);

void tray_menu_close(TrayMenuState &state);

void tray_menu_open(TrayMenuState &state, TrayState &tray, const TrayItem &item,
                    const Rect &anchor_cell, const TrayMenuOpenArgs &args);

void tray_menu_handle_click(TrayMenuState &state, TrayState &tray, double px,
                            double py);

void tray_menu_handle_key_event(TrayMenuState &state, const KeyEvent &event);

constexpr float kTrayCellSize = 40.0f;
constexpr float kTrayIconTargetSize = 20.0f;
constexpr int kTrayColumns = 4;
constexpr float kTrayGridGap = 4.0f;
constexpr float kTrayPanelWidth = kTrayColumns * kTrayCellSize +
                                  (kTrayColumns - 1) * kTrayGridGap +
                                  2.0f * kPanelPadding;

struct TrayPanelState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;
    std::unordered_map<std::string, Texture> icon_cache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;
    float locked_center_x = -1.0f;
    PanelHeightReveal reveal;

    float pending_pill_center_x = 0.0f;
    float pending_qixing_height = 0.0f;
    float pending_qixing_top_margin = 0.0f;
};

const Texture *tray_panel_detail_item_icon_texture(TrayPanelState &state,
                                                   const TrayItem &item);

bool tray_panel_create_surface(TrayPanelState &state, wl_compositor *compositor,
                               zwlr_layer_shell_v1 *layer_shell,
                               wl_output *output = nullptr);

bool tray_panel_init_egl(TrayPanelState &state, Renderer &renderer,
                         TrayState &tray, EGLDisplay display, EGLConfig config,
                         EGLContext context);

void tray_panel_request_frame(TrayPanelState &state, float pill_center_x,
                              float qixing_height, float qixing_top_margin);

void tray_panel_toggle(TrayPanelState &state, float pill_center_x = -1.0f);

void tray_panel_paint(TrayPanelState &state, TrayState &tray,
                      float pill_center_x, float qixing_height,
                      float qixing_top_margin);

struct TrayPanelClickResult {
    const TrayItem *open_menu_for = nullptr;
    Rect anchor_cell;
};

TrayPanelClickResult tray_panel_handle_click(TrayPanelState &state,
                                             TrayState &tray,
                                             TrayMenuState &menu, double px,
                                             double py, uint32_t button);
