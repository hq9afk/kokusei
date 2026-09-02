#include <cassert>
#include <clocale>

#include "service/input_service.h"

void test_keyboard() {
    xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    assert(ctx);

    xkb_rule_names names{};
    xkb_keymap *keymap =
        xkb_keymap_new_from_names(ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    assert(keymap);
    xkb_state *state = xkb_state_new(keymap);
    assert(state);

    auto ev = translate_key(state, 30);
    assert(ev.has_value());
    assert(ev->kind == KeyKind::Text);
    assert(ev->text == "a");

    auto enter_ev = translate_key(state, 28);
    assert(enter_ev.has_value());
    assert(enter_ev->kind == KeyKind::Enter);

    auto esc_ev = translate_key(state, 1);
    assert(esc_ev.has_value());
    assert(esc_ev->kind == KeyKind::Escape);

    auto bs_ev = translate_key(state, 14);
    assert(bs_ev.has_value());
    assert(bs_ev->kind == KeyKind::Backspace);

    auto shift_ev = translate_key(state, 42);
    assert(!shift_ev.has_value());

    xkb_state_update_key(state, 29 + 8, XKB_KEY_DOWN);
    auto ctrl_ev = translate_key(state, 30);
    assert(ctrl_ev.has_value());
    assert(ctrl_ev->ctrl);
    xkb_state_update_key(state, 29 + 8, XKB_KEY_UP);

    setlocale(LC_CTYPE, "en_US.utf8");
    xkb_compose_table *compose_table = xkb_compose_table_new_from_locale(
        ctx, "en_US.utf8", XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (compose_table) {
        xkb_compose_state *compose =
            xkb_compose_state_new(compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
        assert(compose);

        xkb_compose_state_feed(compose, XKB_KEY_Multi_key);

        auto apostrophe_ev = translate_key(state, 40, compose);
        assert(apostrophe_ev.has_value());
        assert(apostrophe_ev->kind == KeyKind::Preedit);

        auto e_ev = translate_key(state, 18, compose);
        assert(e_ev.has_value());
        assert(e_ev->kind == KeyKind::Text);
        assert(e_ev->text == "\xC3\xA9");

        xkb_compose_state_unref(compose);
        xkb_compose_table_unref(compose_table);
    }

    xkb_state_unref(state);
    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
}
