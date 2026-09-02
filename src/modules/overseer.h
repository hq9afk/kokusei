#pragma once

#include <EGL/egl.h>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "app/text_input_client.h"

#include "config/overseer_config.h"

#include "core/async_process.h"

#include "render/animation.h"
#include "render/overlay_panel.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/text.h"
#include "render/text_field.h"
#include "render/texture.h"
#include "render/texture_cache.h"

#include "service/frame_service.h"
#include "service/input_service.h"
#include "service/output_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct OverseerRowHit {
    Rect rect;
    int index;
};

struct OverseerState {
    wl_surface *surface = nullptr;
    zwlr_layer_surface_v1 *layer_surface = nullptr;
    wl_egl_window *egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLDisplay egl_display = nullptr;
    EGLContext egl_context = nullptr;
    Renderer *renderer = nullptr;
    OutputScale output_scale;
    FrameClock frame_clock;
    Scene scene;
    TextureCache tcache;
    bool configured = false;
    bool open = false;
    AnimationManager animations;
    float opacity = 0.0f;
    float anim_height = 0.0f;
    float anim_height_target = -1.0f;

    float highlight_offset = 0.0f;
    float highlight_offset_target = -1.0f;

    float scroll_offset = 0.0f;
    float scroll_offset_target = -1.0f;
    TextFieldState search;
    TextFieldTypeAnim query_anim;
    std::function<void(bool)> sync_text_input_focus;
    OverseerMode mode = OverseerMode::Drun;
    std::string effective_query;
    std::string search_root;
    std::vector<DesktopEntry> apps;
    std::vector<DrunResult> results;
    int selected_index = -1;
    int hovered_index = -1;
    std::vector<OverseerRowHit> row_hitboxes;
    bool search_dirty = false;
    std::chrono::steady_clock::time_point search_dirty_at{};
    AsyncProcess search_dirs_proc, search_files_proc;
    bool search_running = false;
    std::chrono::steady_clock::time_point search_started_at{};
    std::string search_query;
    bool awaiting_restart = false;
    pid_t pending_kill_dirs = -1, pending_kill_files = -1;
    std::chrono::steady_clock::time_point pending_kill_since{};
    std::string pending_restart_query;
    SubmenuState submenu;
    VisitStore visits;
    Texture bullet_tex[kOverseerMaxVisible];
    std::unordered_map<std::string, Texture> app_icon_cache;

    wl_compositor *compositor = nullptr;
    int32_t width = 0, height = 0;
    Rect box_rect{};
    Rect cursor_rect{};
    wl_output *bound_output = nullptr;
};

bool overseer_create_surface(OverseerState &state, wl_compositor *compositor,
                             zwlr_layer_shell_v1 *layer_shell,
                             wl_output *output = nullptr);

bool overseer_init_egl(OverseerState &state, Renderer &renderer,
                       EGLDisplay display, EGLConfig config,
                       EGLContext context);

void overseer_destroy_surface(OverseerState &state);

void overseer_retarget(OverseerState &state, wl_compositor *compositor,
                       zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                       Renderer &renderer, EGLDisplay egl_display,
                       EGLConfig egl_config, EGLContext egl_context,
                       wl_output *target_output, const char *target_name);

void overseer_request_frame(OverseerState &state);

void overseer_search_start_pending(OverseerState &state);

bool overseer_search_poll(OverseerState &state);

const Texture *overseer_icon_lookup(OverseerState &state, const std::string &id,
                                    const std::string &icon_field);

int overseer_poll_timeout_ms(const OverseerState &state);

bool overseer_tick(OverseerState &state);

void overseer_toggle(OverseerState &state, bool global);

void overseer_handle_key_event(OverseerState &state, const KeyEvent &event);

void overseer_handle_click(OverseerState &state, double px, double py);

void overseer_handle_pointer_move(OverseerState &state,
                                  wl_surface *focused_surface, double px,
                                  double py);

void overseer_paint(OverseerState &state);

TextInputState overseer_text_input_state(const OverseerState &state);

void overseer_text_input_apply_edit(OverseerState &state,
                                    const TextInputEdit &edit);
