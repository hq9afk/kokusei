#include <algorithm>
#include <clocale>
#include <cstring>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon-names.h>

#include "core/log.h"

#include "service/input_service.h"

std::optional<KeyEvent> translate_key(xkb_state *state, uint32_t keycode,
                                      xkb_compose_state *compose) {
    xkb_keycode_t xkb_code = keycode + 8;
    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, xkb_code);
    bool shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,
                                              XKB_STATE_MODS_EFFECTIVE) > 0;
    bool alt = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT,
                                            XKB_STATE_MODS_EFFECTIVE) > 0;
    bool ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL,
                                             XKB_STATE_MODS_EFFECTIVE) > 0;

    if (compose) {
        xkb_compose_state_feed(compose, sym);
        xkb_compose_status status = xkb_compose_state_get_status(compose);
        if (status == XKB_COMPOSE_COMPOSED) {
            char buf[32];
            int n = xkb_compose_state_get_utf8(compose, buf, sizeof(buf));
            xkb_compose_state_reset(compose);
            if (n <= 0)
                return std::nullopt;
            return KeyEvent{KeyKind::Text,
                            std::string(buf, static_cast<size_t>(n))};
        }
        if (status == XKB_COMPOSE_CANCELLED) {
            xkb_compose_state_reset(compose);
            return std::nullopt;
        }
        if (status == XKB_COMPOSE_COMPOSING) {
            char buf[8];
            int n = xkb_keysym_to_utf8(sym, buf, sizeof(buf));
            if (n <= 0) {
                char name[64];
                if (xkb_keysym_get_name(sym, name, sizeof(name)) > 0 &&
                    std::strncmp(name, "dead_", 5) == 0) {
                    xkb_keysym_t base =
                        xkb_keysym_from_name(name + 5, XKB_KEYSYM_NO_FLAGS);
                    if (base != XKB_KEY_NoSymbol)
                        n = xkb_keysym_to_utf8(base, buf, sizeof(buf));
                }
            }
            if (n <= 0)
                return std::nullopt;
            return KeyEvent{KeyKind::Preedit,
                            std::string(buf, static_cast<size_t>(n - 1))};
        }
    }

    switch (sym) {
    case XKB_KEY_Up:
        return KeyEvent{KeyKind::Up, "", shift, alt, ctrl};
    case XKB_KEY_Down:
        return KeyEvent{KeyKind::Down, "", shift, alt, ctrl};
    case XKB_KEY_Left:
        return KeyEvent{KeyKind::Left, "", shift, alt, ctrl};
    case XKB_KEY_Right:
        return KeyEvent{KeyKind::Right, "", shift, alt, ctrl};
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        return KeyEvent{KeyKind::Enter, ""};
    case XKB_KEY_Escape:
        return KeyEvent{KeyKind::Escape, ""};
    case XKB_KEY_BackSpace:
        return KeyEvent{KeyKind::Backspace, ""};
    case XKB_KEY_Tab:
        return KeyEvent{KeyKind::Tab, ""};
    default:
        break;
    }

    char buf[32];
    int n = xkb_state_key_get_utf8(state, xkb_code, buf, sizeof(buf));
    if (n <= 0)
        return std::nullopt;
    return KeyEvent{KeyKind::Text, std::string(buf, static_cast<size_t>(n)),
                    shift, alt, ctrl};
}

namespace {

namespace kbd {

void keymap_cb(void *data, wl_keyboard *, uint32_t format, int32_t fd,
               uint32_t size) {
    auto *state = static_cast<KeyboardState *>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }
    void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED)
        return;

    if (state->keymap)
        xkb_keymap_unref(state->keymap);
    if (state->xkb)
        xkb_state_unref(state->xkb);
    state->xkb = nullptr;

    state->keymap = xkb_keymap_new_from_string(
        state->ctx, static_cast<const char *>(map), XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, size);
    if (!state->keymap) {
        klog("keyboard: failed to compile keymap");
        return;
    }
    state->xkb = xkb_state_new(state->keymap);
}

void enter_cb(void *data, wl_keyboard *, uint32_t, wl_surface *surface,
              wl_array *) {
    auto *state = static_cast<KeyboardState *>(data);
    state->focused_surface = surface;
    if (state->on_focus_surface)
        state->on_focus_surface(surface, true);
}

void set_repeat_timer(KeyboardState &state, bool armed) {
    if (state.repeat_timer_fd < 0)
        return;
    itimerspec spec{};
    if (armed && state.repeat_rate_hz > 0) {
        spec.it_value.tv_sec = state.repeat_delay_ms / 1000;
        spec.it_value.tv_nsec = (state.repeat_delay_ms % 1000) * 1000000L;
        int64_t interval_ns = 1000000000LL / state.repeat_rate_hz;
        spec.it_interval.tv_sec = interval_ns / 1000000000LL;
        spec.it_interval.tv_nsec = interval_ns % 1000000000LL;
    }

    timerfd_settime(state.repeat_timer_fd, 0, &spec, nullptr);
}

void leave_cb(void *data, wl_keyboard *, uint32_t, wl_surface *surface) {
    auto *state = static_cast<KeyboardState *>(data);
    if (state->repeat_active) {
        state->repeat_active = false;
        set_repeat_timer(*state, false);
    }
    if (state->focused_surface == surface)
        state->focused_surface = nullptr;
    if (state->on_focus_surface)
        state->on_focus_surface(surface, false);
}

void key_cb(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key,
            uint32_t key_state) {
    auto *state = static_cast<KeyboardState *>(data);
    if (!state->xkb)
        return;
    xkb_keycode_t xkb_code = key + 8;

    if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (state->repeat_active && key == state->repeat_keycode) {
            state->repeat_active = false;
            set_repeat_timer(*state, false);
        }
        return;
    }
    if (key_state != WL_KEYBOARD_KEY_STATE_PRESSED)
        return;

    if (auto ev = translate_key(state->xkb, key, state->compose_state))
        state->pending.push_back(*ev);

    if (state->keymap && xkb_keymap_key_repeats(state->keymap, xkb_code)) {
        state->repeat_keycode = key;
        state->repeat_active = true;
        set_repeat_timer(*state, true);
    }
}

void modifiers_cb(void *data, wl_keyboard *, uint32_t, uint32_t mods_depressed,
                  uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
    auto *state = static_cast<KeyboardState *>(data);
    if (!state->xkb)
        return;
    xkb_state_update_mask(state->xkb, mods_depressed, mods_latched, mods_locked,
                          0, 0, group);
}

void repeat_info_cb(void *data, wl_keyboard *, int32_t rate, int32_t delay) {
    auto *state = static_cast<KeyboardState *>(data);
    klog("keyboard: compositor repeat_info rate=%dHz delay=%dms", rate, delay);
    state->repeat_rate_hz = rate;
    state->repeat_delay_ms = delay;

    if (state->repeat_active)
        set_repeat_timer(*state, true);
}

constexpr wl_keyboard_listener kKeyboardListener = {
    .keymap = keymap_cb,
    .enter = enter_cb,
    .leave = leave_cb,
    .key = key_cb,
    .modifiers = modifiers_cb,
    .repeat_info = repeat_info_cb,
};

} // namespace kbd

namespace ptr {

void enter_cb(void *data, wl_pointer *, uint32_t serial, wl_surface *surface,
              wl_fixed_t sx, wl_fixed_t sy) {
    auto *state = static_cast<PointerState *>(data);
    state->focused_surface = surface;
    state->x = wl_fixed_to_double(sx);
    state->y = wl_fixed_to_double(sy);
    state->dirty = true;
    state->last_enter_serial = serial;
    pointer_set_cursor_shape(*state, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
}

void leave_cb(void *data, wl_pointer *, uint32_t, wl_surface *surface) {
    auto *state = static_cast<PointerState *>(data);
    if (state->focused_surface == surface) {
        state->focused_surface = nullptr;
        state->dirty = true;
    }
}

void motion_cb(void *data, wl_pointer *, uint32_t, wl_fixed_t sx,
               wl_fixed_t sy) {
    auto *state = static_cast<PointerState *>(data);
    state->x = wl_fixed_to_double(sx);
    state->y = wl_fixed_to_double(sy);
    state->dirty = true;
}

void button_cb(void *data, wl_pointer *, uint32_t serial, uint32_t,
               uint32_t button, uint32_t button_state) {
    auto *state = static_cast<PointerState *>(data);
    if (button != BTN_LEFT && button != BTN_RIGHT)
        return;
    if (button_state == WL_POINTER_BUTTON_STATE_PRESSED)
        state->last_button_serial = serial;
    state->pending_clicks.push_back(
        {state->focused_surface,
         button_state == WL_POINTER_BUTTON_STATE_PRESSED, button, state->x,
         state->y, serial});
}
void axis_cb(void *data, wl_pointer *, uint32_t, uint32_t axis,
             wl_fixed_t value) {
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL)
        return;
    auto *state = static_cast<PointerState *>(data);
    state->pending_scrolls.push_back(
        {state->focused_surface, wl_fixed_to_double(value)});
}
void frame_cb(void *, wl_pointer *) {}
void axis_source_cb(void *, wl_pointer *, uint32_t) {}
void axis_stop_cb(void *, wl_pointer *, uint32_t, uint32_t) {}
void axis_discrete_cb(void *, wl_pointer *, uint32_t, int32_t) {}
void axis_value120_cb(void *, wl_pointer *, uint32_t, int32_t) {}
void axis_relative_direction_cb(void *, wl_pointer *, uint32_t, uint32_t) {}
void warp_cb(void *, wl_pointer *, wl_fixed_t, wl_fixed_t) {}

constexpr wl_pointer_listener kPointerListener = {
    .enter = enter_cb,
    .leave = leave_cb,
    .motion = motion_cb,
    .button = button_cb,
    .axis = axis_cb,
    .frame = frame_cb,
    .axis_source = axis_source_cb,
    .axis_stop = axis_stop_cb,
    .axis_discrete = axis_discrete_cb,
    .axis_value120 = axis_value120_cb,
    .axis_relative_direction = axis_relative_direction_cb,
    .warp = warp_cb,
};

} // namespace ptr

void seat_capabilities_cb(void *data, wl_seat *seat, uint32_t caps) {
    auto *seat_state = static_cast<SeatCapabilityState *>(data);

    KeyboardState *kb = seat_state->keyboard;
    bool has_keyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;
    if (has_keyboard && !kb->keyboard) {
        kb->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(kb->keyboard, &kbd::kKeyboardListener, kb);
    } else if (!has_keyboard && kb->keyboard) {
        wl_keyboard_release(kb->keyboard);
        kb->keyboard = nullptr;
    }

    PointerState *pointer = seat_state->pointer;
    bool has_pointer = caps & WL_SEAT_CAPABILITY_POINTER;
    if (has_pointer && !pointer->pointer) {
        pointer_bind(*pointer, seat);
    } else if (!has_pointer && pointer->pointer) {
        pointer_release(*pointer);
    }
}

void seat_name_cb(void *, wl_seat *, const char *) {}

constexpr wl_seat_listener kSeatListener = {
    .capabilities = seat_capabilities_cb,
    .name = seat_name_cb,
};

} // namespace

void keyboard_attach_seat(SeatCapabilityState &seat_state, wl_seat *seat) {
    if (!seat_state.keyboard->ctx)
        seat_state.keyboard->ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!seat_state.keyboard->compose_table) {
        const char *locale = setlocale(LC_CTYPE, "");
        seat_state.keyboard->compose_table = xkb_compose_table_new_from_locale(
            seat_state.keyboard->ctx, locale ? locale : "C",
            XKB_COMPOSE_COMPILE_NO_FLAGS);
        if (seat_state.keyboard->compose_table)
            seat_state.keyboard->compose_state = xkb_compose_state_new(
                seat_state.keyboard->compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
    }
    if (seat_state.keyboard->repeat_timer_fd < 0) {
        seat_state.keyboard->repeat_timer_fd =
            timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    }
    wl_seat_add_listener(seat, &kSeatListener, &seat_state);
}

std::vector<KeyEvent> keyboard_drain_events(KeyboardState &state) {
    std::vector<KeyEvent> events = std::move(state.pending);
    state.pending.clear();
    return events;
}

void keyboard_repeat_tick(KeyboardState &state) {
    if (state.repeat_timer_fd < 0)
        return;
    uint64_t expirations = 0;
    ssize_t n = read(state.repeat_timer_fd, &expirations, sizeof(expirations));
    if (n != sizeof(expirations) || !state.repeat_active || !state.xkb)
        return;
    auto ev =
        translate_key(state.xkb, state.repeat_keycode, state.compose_state);
    if (!ev)
        return;
    constexpr uint64_t kMaxCatchUp = 8;
    for (uint64_t i = 0; i < std::min(expirations, kMaxCatchUp); ++i)
        state.pending.push_back(*ev);
}

void pointer_bind(PointerState &state, wl_seat *seat) {
    state.pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(state.pointer, &ptr::kPointerListener, &state);
}

void pointer_release(PointerState &state) {
    if (state.cursor_shape_device) {
        wp_cursor_shape_device_v1_destroy(state.cursor_shape_device);
        state.cursor_shape_device = nullptr;
    }
    if (state.pointer) {
        wl_pointer_release(state.pointer);
        state.pointer = nullptr;
    }
    state.focused_surface = nullptr;
}

void pointer_set_cursor_shape(PointerState &state,
                              wp_cursor_shape_device_v1_shape shape) {
    if (!state.cursor_shape_manager || !state.pointer)
        return;
    if (!state.cursor_shape_device)
        state.cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(
            state.cursor_shape_manager, state.pointer);
    wp_cursor_shape_device_v1_set_shape(state.cursor_shape_device,
                                        state.last_enter_serial, shape);
}

std::vector<PointerClick> pointer_drain_clicks(PointerState &state) {
    std::vector<PointerClick> clicks = std::move(state.pending_clicks);
    state.pending_clicks.clear();
    return clicks;
}

std::vector<PointerScroll> pointer_drain_scrolls(PointerState &state) {
    std::vector<PointerScroll> scrolls = std::move(state.pending_scrolls);
    state.pending_scrolls.clear();
    return scrolls;
}
