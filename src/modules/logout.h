#pragma once

#include <array>
#include <chrono>
#include <vector>

#include "app/ipc.h"

#include "config/logout_config.h"

#include "modules/logout/thunder_burst.h"

#include "render/animated_image.h"
#include "render/overlay_panel.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/text.h"
#include "render/texture.h"

#include "service/input_service.h"

struct WaylandState;

struct LogoutState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    int selected_index = 0;
    int hovered_index = -1;
    bool opened_by_widget = false;
    wl_output *bound_output = nullptr;

    bool input_ready = false;
    bool exiting = false;

    float logo_scale = 0.0f;
    float exit_fade = 0.0f;
    float burst = 0.0f;
    std::chrono::steady_clock::time_point burst_started{};

    std::array<float, kLogoutButtonCount> slash{};
    std::array<float, kLogoutButtonCount> button_travel{};

    std::array<float, kLogoutButtonCount> button_highlight_scale{};
    std::array<float, kLogoutButtonCount> button_highlight_border{};

    std::array<Texture, kLogoutButtonCount> glyph_tex{};

    ThunderBurst thunder;
    AnimatedImage logo;
    bool logo_animated = true;
    bool logo_source_set = false;
};

RasterizedText rasterize_yujimai_glyph(const std::string &codepoint_utf8);

Rect logout_detail_button_rect(int index, float center_x, float center_y);

bool logout_create_surface(LogoutState &state, wl_compositor *compositor,
                             zwlr_layer_shell_v1 *layer_shell,
                             wl_output *output = nullptr);

bool logout_init_egl(LogoutState &state, Renderer &renderer,
                       EGLDisplay display, EGLConfig config,
                       EGLContext context);

void logout_retarget(LogoutState &state, wl_compositor *compositor,
                       zwlr_layer_shell_v1 *layer_shell, wl_display *display,
                       Renderer &renderer, EGLDisplay egl_display,
                       EGLConfig egl_config, EGLContext egl_context,
                       wl_output *target_output, const char *target_name);

void logout_request_frame(LogoutState &state);

void logout_apply_logo_config(LogoutState &state, bool animated);

void logout_toggle(LogoutState &state, bool by_widget = false);

std::vector<IpcHandler> logout_ipc_handlers(LogoutState &logout,
                                              WaylandState &state);

void logout_execute(LogoutState &state, int index);

void logout_handle_key_event(LogoutState &state, const KeyEvent &event);

void logout_handle_click(LogoutState &state, double px, double py);

void logout_handle_hover(LogoutState &state, double px, double py);

void logout_clear_hover(LogoutState &state);

void logout_paint(LogoutState &state);
