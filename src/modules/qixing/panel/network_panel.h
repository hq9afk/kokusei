#pragma once

#include <EGL/egl.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>

#include "app/text_input_client.h"

#include "render/marquee_scroll.h"
#include "render/overlay_panel.h"
#include "render/panel_chrome.h"
#include "render/rect.h"
#include "render/renderer.h"
#include "render/scene.h"
#include "render/text_field.h"
#include "render/texture.h"
#include "render/texture_cache.h"

#include "service/input_service.h"
#include "service/network_service.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

constexpr float kNetErrorBannerHeight = 48.0f;
constexpr float kNetworkEthernetBannerHeight = 40.0f;
constexpr float kNetworkUnavailableStateHeight = 72.0f;
constexpr float kNetworkScanningStateHeight = 48.0f;
constexpr float kNetworkSectionGapSmall = 20.0f;
constexpr float kNetworkSectionGapLarge = 24.0f;
constexpr float kNetworkConnectDisabledAlpha = 0.4f;
constexpr uint64_t kNetworkPasswordTypeAnimOwnerBase = 10000;
constexpr uint64_t kNetworkPasswordRowSlideOwner = 10512;
constexpr float kNetworkCaptiveBg[4] = {1.0f, 0.76f, 0.03f, 0.15f};
constexpr float kNetworkCaptiveFg[4] = {1.0f, 0.7569f, 0.0275f, 1.0f};

struct NetworkPanelState {
    OverlayPanelBase base;
    Renderer *renderer = nullptr;
    Scene scene;
    TextureCache tcache;

    std::string sub_mode, sub_ssid;
    TextFieldState password_field;
    TextFieldTypeAnim password_anim;
    TextFieldRowSlide password_row_slide;
    Texture password_dot_tex;
    std::unordered_map<std::string, MarqueeTextState> ssid_marquees;
    std::function<void(bool)> sync_text_input_focus;

    Rect panel_rect;
    Rect sub_rect;
    std::vector<PanelClickRegion> click_regions;
    float locked_center_x = -1.0f;
    PanelHeightReveal reveal;
    float scroll_offset = 0.0f;
    float visible_content_height = 0.0f;

    float pending_pill_center_x = 0.0f;
    float pending_qixing_height = 0.0f;
    float pending_qixing_top_margin = 0.0f;
};

namespace network_panel_detail {

const char *signal_icon(int pct);

enum class RowKind {
    Error,
    Ethernet,
    NoAdapter,
    Disabled,
    Scanning,
    SectionConnected,
    SectionKnown,
    SectionAvailable,
    Device,
    Spacer,
};

struct PanelRow {
    RowKind kind;
    float height;
    const NetworkInfo *info = nullptr;
};

std::vector<PanelRow> build_rows(const NetworkState &net);

float content_height(const std::vector<PanelRow> &rows);

float panel_height(float content_h);

float sub_panel_height(const std::string &mode);

} // namespace network_panel_detail

bool network_panel_create_surface(NetworkPanelState &state,
                                  wl_compositor *compositor,
                                  zwlr_layer_shell_v1 *layer_shell,
                                  wl_output *output = nullptr);

bool network_panel_init_egl(NetworkPanelState &state, Renderer &renderer,
                            NetworkState &net, EGLDisplay display,
                            EGLConfig config, EGLContext context);

void network_panel_request_frame(NetworkPanelState &state, float pill_center_x,
                                 float qixing_height, float qixing_top_margin);

void network_panel_toggle(NetworkPanelState &state,
                          float pill_center_x = -1.0f);

void network_panel_handle_scroll(NetworkPanelState &state, NetworkState &net,
                                 double dy);

void network_panel_open_sub(NetworkPanelState &state, const std::string &mode,
                            const std::string &ssid);

void network_panel_close_sub(NetworkPanelState &state);

void network_panel_handle_key_event(NetworkPanelState &state, NetworkState &net,
                                    const KeyEvent &event);

void network_panel_handle_click(NetworkPanelState &state, NetworkState &net,
                                double px, double py);

void network_panel_paint(NetworkPanelState &state, NetworkState &net,
                         float pill_center_x, float qixing_height,
                         float qixing_top_margin);

TextInputState network_panel_text_input_state(const NetworkPanelState &state);

void network_panel_text_input_apply_edit(NetworkPanelState &state,
                                         const TextInputEdit &edit);
