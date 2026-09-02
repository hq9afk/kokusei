#include <cassert>
#include <chrono>

#include "render/marquee_scroll.h"

static void test_marquee_fits() {
    AnimationManager anim;
    MarqueeTextState state;
    marquee_scroll_update(anim, state, "short", 40.0f, 100.0f);
    assert(!state.marqueeing);
    assert(state.scroll_offset == 0.0f);
    assert(!anim.hasActive());
}

static void test_marquee_overflow_cycles() {
    AnimationManager anim;
    MarqueeTextState state;
    auto now = std::chrono::steady_clock::now();

    marquee_scroll_update(anim, state, "a very long scrolling label", 300.0f,
                          100.0f);
    assert(state.marqueeing);
    assert(anim.hasActive());
    assert(state.scroll_offset == 0.0f);

    marquee_scroll_update(anim, state, "a very long scrolling label", 300.0f,
                          100.0f);
    assert(state.marqueeing);

    now += std::chrono::milliseconds(1300);
    anim.tick(now);
    anim.tick(now);
    assert(state.scroll_offset > 0.0f);
    assert(state.scroll_offset < 200.0f);

    now += std::chrono::milliseconds(20000);
    anim.tick(now);
    anim.tick(now);
    assert(state.scroll_offset >= 0.0f);
    assert(state.scroll_offset <= 200.0f + 1e-3f);
}

static void test_marquee_text_change_resets() {
    AnimationManager anim;
    MarqueeTextState state;
    auto now = std::chrono::steady_clock::now();

    marquee_scroll_update(anim, state, "first long label text here", 300.0f,
                          100.0f);
    now += std::chrono::milliseconds(1300);
    anim.tick(now);
    anim.tick(now);
    assert(state.scroll_offset > 0.0f);

    marquee_scroll_update(anim, state, "second, different label text", 320.0f,
                          100.0f);
    assert(state.scroll_offset == 0.0f);
    assert(state.last_text == "second, different label text");
}

static void test_marquee_shrink_to_fit_stops() {
    AnimationManager anim;
    MarqueeTextState state;
    auto now = std::chrono::steady_clock::now();

    marquee_scroll_update(anim, state, "long enough to overflow the box",
                          300.0f, 100.0f);
    now += std::chrono::milliseconds(1300);
    anim.tick(now);
    assert(state.marqueeing);

    marquee_scroll_update(anim, state, "long enough to overflow the box", 90.0f,
                          100.0f);
    assert(!state.marqueeing);
    assert(state.scroll_offset == 0.0f);
    assert(!anim.hasActive());
}

void test_marquee_scroll() {
    test_marquee_fits();
    test_marquee_overflow_cycles();
    test_marquee_text_change_resets();
    test_marquee_shrink_to_fit_stops();
}
