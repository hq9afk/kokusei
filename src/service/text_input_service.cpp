#include <algorithm>
#include <utility>

#include "core/log.h"

#include "service/text_input_service.h"

#include "text-input-unstable-v3-client-protocol.h"

namespace {

void text_input_enter(void *data, zwp_text_input_v3 *, wl_surface *surface) {
    static_cast<TextInputService *>(data)->handle_enter(surface);
}

void text_input_leave(void *data, zwp_text_input_v3 *, wl_surface *surface) {
    static_cast<TextInputService *>(data)->handle_leave(surface);
}

void text_input_preedit_string(void *data, zwp_text_input_v3 *,
                               const char *text, int32_t, int32_t) {
    static_cast<TextInputService *>(data)->handle_preedit_string(text);
}

void text_input_commit_string(void *data, zwp_text_input_v3 *,
                              const char *text) {
    static_cast<TextInputService *>(data)->handle_commit_string(text);
}

void text_input_delete_surrounding_text(void *data, zwp_text_input_v3 *,
                                        uint32_t before_length,
                                        uint32_t after_length) {
    static_cast<TextInputService *>(data)->handle_delete_surrounding_text(
        before_length, after_length);
}

void text_input_done(void *data, zwp_text_input_v3 *, uint32_t serial) {
    static_cast<TextInputService *>(data)->handle_done(serial);
}

void text_input_action(void *, zwp_text_input_v3 *, uint32_t, uint32_t) {}

void text_input_language(void *, zwp_text_input_v3 *, const char *) {}

void text_input_preedit_hint(void *, zwp_text_input_v3 *, uint32_t, uint32_t,
                             uint32_t) {}

constexpr zwp_text_input_v3_listener kTextInputListener = {
    .enter = text_input_enter,
    .leave = text_input_leave,
    .preedit_string = text_input_preedit_string,
    .commit_string = text_input_commit_string,
    .delete_surrounding_text = text_input_delete_surrounding_text,
    .done = text_input_done,
    .action = text_input_action,
    .language = text_input_language,
    .preedit_hint = text_input_preedit_hint,
};

} // namespace

TextInputService::~TextInputService() { cleanup(); }

bool TextInputService::bind(zwp_text_input_manager_v3 *manager, wl_seat *seat) {
    if (!manager || !seat)
        return false;
    if (manager_ == manager && seat_ == seat && text_input_)
        return true;

    cleanup();
    manager_ = manager;
    seat_ = seat;
    text_input_ = zwp_text_input_manager_v3_get_text_input(manager_, seat_);
    if (!text_input_) {
        klog("text-input: failed to create text-input-v3 object");
        cleanup();
        return false;
    }
    zwp_text_input_v3_add_listener(text_input_, &kTextInputListener, this);
    klog("text-input: text-input-v3 bound");
    return true;
}

void TextInputService::cleanup() {
    if (active_client_)
        deactivate_client(active_client_);
    if (text_input_) {
        zwp_text_input_v3_destroy(text_input_);
        text_input_ = nullptr;
    }
    manager_ = nullptr;
    seat_ = nullptr;
    entered_surface_ = nullptr;
    keyboard_focus_surface_ = nullptr;
    active_surface_ = nullptr;
    active_client_ = nullptr;
    pending_edit_ = {};
    commit_serial_ = 0;
    enabled_ = false;
}

void TextInputService::set_focused_client(wl_surface *surface,
                                          TextInputClient *client) {
    if (!surface || !client) {
        clear_focused_client(active_client_);
        return;
    }
    if (!text_input_)
        return;

    if (active_client_ == client && active_surface_ == surface) {
        if (!enabled_)
            enable_active();
        else
            commit_active_state(false);
        return;
    }

    disable_active();
    if (active_client_)
        deactivate_client(active_client_);

    active_surface_ = surface;
    active_client_ = client;
    active_client_->text_input_activated(*this);
    enable_active();
}

void TextInputService::clear_focused_client(TextInputClient *client) {
    if (!client || client != active_client_)
        return;
    disable_active();
    deactivate_client(client);
    active_client_ = nullptr;
    active_surface_ = nullptr;
    pending_edit_ = {};
}

void TextInputService::on_keyboard_focus_surface(wl_surface *surface,
                                                 bool entered) {
    if (entered)
        keyboard_focus_surface_ = surface;
    else if (keyboard_focus_surface_ == surface)
        keyboard_focus_surface_ = nullptr;

    if (active_client_ && entered && active_surface_accepts_text_input())
        enable_active();
}

bool TextInputService::active_surface_accepts_text_input() const {
    return active_surface_ != nullptr &&
           (entered_surface_ == active_surface_ ||
            keyboard_focus_surface_ == active_surface_);
}

void TextInputService::handle_enter(wl_surface *surface) {
    entered_surface_ = surface;
    pending_edit_ = {};
    if (active_client_ && active_surface_ == surface)
        enable_active();
}

void TextInputService::handle_leave(wl_surface *surface) {
    if (entered_surface_ != surface)
        return;
    entered_surface_ = nullptr;
    enabled_ = false;
    pending_edit_ = {};
    if (active_client_ && active_surface_ == surface)
        active_client_->text_input_reset_preedit();
}

void TextInputService::handle_preedit_string(const char *text) {
    pending_edit_.has_preedit = true;
    pending_edit_.preedit_text = text ? text : "";
}

void TextInputService::handle_commit_string(const char *text) {
    pending_edit_.has_commit_text = true;
    pending_edit_.commit_text = text ? text : "";
}

void TextInputService::handle_delete_surrounding_text(uint32_t before_length,
                                                      uint32_t after_length) {
    pending_edit_.has_delete = true;
    pending_edit_.delete_before_length = before_length;
    pending_edit_.delete_after_length = after_length;
}

void TextInputService::handle_done(uint32_t serial) {
    TextInputEdit edit = std::move(pending_edit_);
    pending_edit_ = {};

    if (active_client_) {
        active_client_->text_input_apply_edit(edit);
        if (serial == commit_serial_)
            commit_active_state(true);
    }
}

void TextInputService::enable_active() {
    if (!text_input_ || !active_client_ || !active_surface_)
        return;
    if (!active_surface_accepts_text_input())
        return;

    if (!enabled_) {
        zwp_text_input_v3_enable(text_input_);
        enabled_ = true;
    }
    commit_active_state(false);
}

void TextInputService::disable_active() {
    if (!text_input_ || !enabled_) {
        enabled_ = false;
        return;
    }
    if (active_surface_accepts_text_input()) {
        zwp_text_input_v3_disable(text_input_);
        commit_protocol_state();
        wl_surface_commit(active_surface_);
    }
    enabled_ = false;
}

void TextInputService::commit_active_state(bool from_input_method) {
    if (!text_input_ || !active_client_ || !active_surface_ || !enabled_)
        return;
    if (!active_surface_accepts_text_input())
        return;

    TextInputState state = active_client_->text_input_state();
    uint32_t purpose = state.purpose == TextInputPurpose::Password
                           ? ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_PASSWORD
                           : ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL;
    zwp_text_input_v3_set_content_type(
        text_input_, ZWP_TEXT_INPUT_V3_CONTENT_HINT_NONE, purpose);
    zwp_text_input_v3_set_text_change_cause(
        text_input_, from_input_method
                         ? ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD
                         : ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
    zwp_text_input_v3_set_cursor_rectangle(
        text_input_, state.cursor_rect_x, state.cursor_rect_y,
        std::max(1, state.cursor_rect_w), std::max(1, state.cursor_rect_h));
    commit_protocol_state();
    wl_surface_commit(active_surface_);
}

void TextInputService::commit_protocol_state() {
    zwp_text_input_v3_commit(text_input_);
    ++commit_serial_;
}

void TextInputService::deactivate_client(TextInputClient *client) {
    client->text_input_reset_preedit();
    client->text_input_deactivated(*this);
}
