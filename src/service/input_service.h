#pragma once

#include <cstdint>
#include <functional>
#include <linux/input-event-codes.h>
#include <optional>
#include <string>
#include <vector>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>

#include "cursor-shape-v1-client-protocol.h"

enum class KeyKind {
    Text,
    Preedit,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Escape,
    Backspace,
    Tab
};

struct KeyEvent {
    KeyKind kind;
    std::string text;
    bool shift = false;
    bool alt = false;
    bool ctrl = false;
};

struct KeyboardState {
    xkb_context *ctx = nullptr;
    xkb_keymap *keymap = nullptr;
    xkb_state *xkb = nullptr;
    xkb_compose_table *compose_table = nullptr;
    xkb_compose_state *compose_state = nullptr;
    wl_keyboard *keyboard = nullptr;
    wl_surface *focused_surface = nullptr;
    std::vector<KeyEvent> pending;
    std::function<void(wl_surface *, bool)> on_focus_surface;

    int repeat_timer_fd = -1;
    int32_t repeat_rate_hz = 25;
    int32_t repeat_delay_ms = 400;
    uint32_t repeat_keycode = 0;
    bool repeat_active = false;
};

struct PointerClick {
    wl_surface *surface;
    bool pressed;
    uint32_t button = BTN_LEFT;
    double x = 0, y = 0;
    uint32_t serial = 0;
};

struct PointerScroll {
    wl_surface *surface;
    double dy;
};

struct PointerState {
    wl_pointer *pointer = nullptr;
    wl_surface *focused_surface = nullptr;
    double x = -1, y = -1;
    bool dirty = false;
    std::vector<PointerClick> pending_clicks;
    std::vector<PointerScroll> pending_scrolls;

    wp_cursor_shape_manager_v1 *cursor_shape_manager = nullptr;
    wp_cursor_shape_device_v1 *cursor_shape_device = nullptr;
    uint32_t last_enter_serial = 0;
    uint32_t last_button_serial = 0;
};

struct SeatCapabilityState {
    KeyboardState *keyboard = nullptr;
    PointerState *pointer = nullptr;
};

std::optional<KeyEvent> translate_key(xkb_state *state, uint32_t keycode,
                                      xkb_compose_state *compose = nullptr);

void keyboard_attach_seat(SeatCapabilityState &seat_state, wl_seat *seat);

std::vector<KeyEvent> keyboard_drain_events(KeyboardState &state);

void keyboard_repeat_tick(KeyboardState &state);

void pointer_bind(PointerState &state, wl_seat *seat);

void pointer_release(PointerState &state);

std::vector<PointerClick> pointer_drain_clicks(PointerState &state);

std::vector<PointerScroll> pointer_drain_scrolls(PointerState &state);

void pointer_set_cursor_shape(PointerState &state,
                              wp_cursor_shape_device_v1_shape shape);
