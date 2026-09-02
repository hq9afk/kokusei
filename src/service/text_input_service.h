#pragma once

#include <cstdint>

#include "app/text_input_client.h"

struct wl_seat;
struct wl_surface;
struct zwp_text_input_manager_v3;
struct zwp_text_input_v3;

class TextInputService {
  public:
    ~TextInputService();

    bool bind(zwp_text_input_manager_v3 *manager, wl_seat *seat);
    void cleanup();

    void set_focused_client(wl_surface *surface, TextInputClient *client);
    void clear_focused_client(TextInputClient *client);
    void on_keyboard_focus_surface(wl_surface *surface, bool entered);

    void handle_enter(wl_surface *surface);
    void handle_leave(wl_surface *surface);
    void handle_preedit_string(const char *text);
    void handle_commit_string(const char *text);
    void handle_delete_surrounding_text(uint32_t before_length,
                                        uint32_t after_length);
    void handle_done(uint32_t serial);

  private:
    bool active_surface_accepts_text_input() const;
    void enable_active();
    void disable_active();
    void commit_active_state(bool from_input_method);
    void commit_protocol_state();
    void deactivate_client(TextInputClient *client);

    zwp_text_input_manager_v3 *manager_ = nullptr;
    wl_seat *seat_ = nullptr;
    zwp_text_input_v3 *text_input_ = nullptr;
    wl_surface *entered_surface_ = nullptr;
    wl_surface *keyboard_focus_surface_ = nullptr;
    wl_surface *active_surface_ = nullptr;
    TextInputClient *active_client_ = nullptr;
    TextInputEdit pending_edit_;
    uint32_t commit_serial_ = 0;
    bool enabled_ = false;
};
