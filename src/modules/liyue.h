#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>

#include "app/ipc.h"

#include "config/liyue_config.h"

#include "render/overlay_panel.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"

#include "service/capture_service.h"
#include "service/input_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct WaylandState;

struct LiyueWindowTile {
    std::string address;
    Rect rect;
    int workspace_id = -1;
};

struct LiyueTileAnim {
    Rect current;
    Rect target;
    bool seen = false;
};

struct LiyueState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    ToplevelExportState capture;
    std::unordered_map<int, Texture> workspace_number_tex;
    bool opened_by_widget = false;
    wl_output *bound_output = nullptr;
    WaylandState *app_ptr = nullptr;

    int workspace_group = 0;
    int selected_workspace = -1;
    std::vector<LiyueWindowTile> tiles;
    std::unordered_map<std::string, LiyueTileAnim> tile_anim;

    Rect indicator_anim;
    Rect indicator_target;
    bool indicator_tracking = false;
    int indicator_page = -1;

    float slide_y = 0.0f;

    bool dragging = false;
    std::string drag_address;
    int drag_from_workspace = -1;
    int drag_target_workspace = -1;
    double drag_pointer_x = 0.0;
    double drag_pointer_y = 0.0;
    double drag_offset_x = 0.0;
    double drag_offset_y = 0.0;

    int poll_tick = 0;
};

bool liyue_create_surface(LiyueState &state, wl_compositor *compositor,
                          zwlr_layer_shell_v1 *layer_shell,
                          wl_output *output = nullptr);

bool liyue_init_egl(LiyueState &state, Renderer &renderer, EGLDisplay display,
                    EGLConfig config, EGLContext context);

void liyue_retarget(LiyueState &state, wl_compositor *compositor,
                    zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                    Renderer &renderer, EGLDisplay egl_display,
                    EGLConfig egl_config, EGLContext egl_context,
                    wl_output *target_output, const char *target_name);

void liyue_request_frame(LiyueState &state);

void liyue_toggle(LiyueState &state, WaylandState &app, bool by_widget = false);

std::vector<IpcHandler> liyue_ipc_handlers(LiyueState &liyue,
                                           WaylandState &state);

void liyue_handle_click(LiyueState &state, WaylandState &app, double px,
                        double py);

void liyue_handle_pointer_move(LiyueState &state, WaylandState &app, double px,
                               double py);

bool liyue_point_is_clickable(LiyueState &state, WaylandState &app, double px,
                              double py);

void liyue_handle_pointer_release(LiyueState &state, WaylandState &app);

void liyue_handle_key_event(LiyueState &state, WaylandState &app,
                            const KeyEvent &event);

void liyue_paint(LiyueState &state, WaylandState &app);
