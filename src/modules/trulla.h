#pragma once

#include <EGL/egl.h>
#include <functional>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "app/config.h"
#include "app/ipc.h"
#include "app/text_input_client.h"

#include "config/trulla_config.h"

#include "modules/trulla/expanse_tab.h"

#include "render/animated_image.h"
#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/scene.h"
#include "render/text_field.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/media_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

enum class TrullaTab { Expanse, Displays, Blink, Starward };

class Renderer;
struct WaylandState;

using TrullaCommitFn = std::function<void(Config)>;

struct TrullaState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    Rect panel_rect;
    std::vector<PanelClickRegion> click_regions;

    TrullaTab active_tab = TrullaTab::Expanse;
    TrullaFieldId focused_field = TrullaFieldId::None;
    TextFieldState field_buffer;
    TextFieldTypeAnim field_anim;
    AnimatedImage profile_pic;
    std::function<void(bool)> sync_text_input_focus;

    ExpanseSubtabState expanse_static;
    ExpanseSubtabState expanse_animated;
    bool expanse_animated_active = false;
    std::function<MediaDecodeStatus(const std::string &monitor, int column)>
        expanse_decode_status;

    std::vector<std::string> monitor_names;

    std::string displays_selected_monitor;
    std::string blink_selected_monitor;
};

std::string trulla_detail_format_field(const Config &cfg, TrullaFieldId id,
                                       const std::string &monitor = "");

bool trulla_create_surface(TrullaState &state, wl_compositor *compositor,
                           zwlr_layer_shell_v1 *layer_shell,
                           wl_output *output = nullptr);

bool trulla_init_egl(TrullaState &state, const Config &cfg, Renderer &renderer,
                     EGLDisplay display, EGLConfig config, EGLContext context,
                     std::function<std::vector<std::string>()> monitor_names_fn,
                     std::function<std::string()> focused_monitor_fn,
                     std::function<MediaDecodeStatus(const std::string &, int)>
                         expanse_decode_status_fn);

void trulla_request_frame(TrullaState &state);

void trulla_commit_focused_field(TrullaState &state, const Config &cfg,
                                 const TrullaCommitFn &on_commit);

void trulla_toggle(TrullaState &state, const Config &cfg,
                   const TrullaCommitFn &on_commit);

std::vector<IpcHandler> trulla_ipc_handlers(TrullaState &trulla,
                                            WaylandState &state);

void trulla_focus_field(TrullaState &state, const Config &cfg,
                        const TrullaCommitFn &on_commit, TrullaFieldId id);

void trulla_handle_click(TrullaState &state, const Config &cfg,
                         const TrullaCommitFn &on_commit, double px, double py);

bool trulla_point_is_clickable(const TrullaState &state, double px, double py);

void trulla_handle_key_event(TrullaState &state, const Config &cfg,
                             const TrullaCommitFn &on_commit,
                             const KeyEvent &event);

void trulla_paint(TrullaState &state, const Config &cfg,
                  const std::vector<std::string> &monitor_names,
                  const std::string &focused_monitor);

void trulla_handle_scroll(TrullaState &state, double dy);

TextInputState trulla_text_input_state(const TrullaState &state);

void trulla_text_input_apply_edit(TrullaState &state,
                                  const TextInputEdit &edit);

void draw_toggle_switch(TrullaState &state, Node *parent, float x, float y,
                        bool active, const char *tag);

void draw_toggle_row(TrullaState &state, Node *parent, int32_t scale, float x,
                     float y, float w, const std::string &label, bool value,
                     const char *tag, bool tiled);
