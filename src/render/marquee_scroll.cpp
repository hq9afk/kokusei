#include "render/marquee_scroll.h"

namespace {

constexpr float kMarqueePauseMs = 1200.0f;
constexpr float kMarqueeSpeedPxPerSec = 35.0f;

void start_pause_at_start(AnimationManager &anim, MarqueeTextState &state,
                          std::uint64_t owner, float overflow);
void start_scroll(AnimationManager &anim, MarqueeTextState &state,
                  std::uint64_t owner, float overflow);
void start_pause_at_end(AnimationManager &anim, MarqueeTextState &state,
                        std::uint64_t owner, float overflow);
void start_snap(AnimationManager &anim, MarqueeTextState &state,
                std::uint64_t owner, float overflow);

void start_pause_at_start(AnimationManager &anim, MarqueeTextState &state,
                          std::uint64_t owner, float overflow) {
    anim.animate(
        0.0f, 0.0f, kMarqueePauseMs, Easing::Linear, [](float) {},
        [&anim, &state, owner, overflow] {
            start_scroll(anim, state, owner, overflow);
        },
        owner);
}

void start_scroll(AnimationManager &anim, MarqueeTextState &state,
                  std::uint64_t owner, float overflow) {
    float duration_ms = overflow / kMarqueeSpeedPxPerSec * 1000.0f;
    anim.animate(
        0.0f, overflow, duration_ms, Easing::Linear,
        [&state](float v) { state.scroll_offset = v; },
        [&anim, &state, owner, overflow] {
            start_pause_at_end(anim, state, owner, overflow);
        },
        owner);
}

void start_pause_at_end(AnimationManager &anim, MarqueeTextState &state,
                        std::uint64_t owner, float overflow) {
    anim.animate(
        overflow, overflow, kMarqueePauseMs, Easing::Linear, [](float) {},
        [&anim, &state, owner, overflow] {
            start_snap(anim, state, owner, overflow);
        },
        owner);
}

void start_snap(AnimationManager &anim, MarqueeTextState &state,
                std::uint64_t owner, float overflow) {
    anim.animate(
        overflow, 0.0f, 0.0f, Easing::Linear,
        [&state](float v) { state.scroll_offset = v; },
        [&anim, &state, owner, overflow] {
            start_pause_at_start(anim, state, owner, overflow);
        },
        owner);
}

} // namespace

void marquee_scroll_update(AnimationManager &anim, MarqueeTextState &state,
                           const std::string &text, float text_width,
                           float box_w) {
    std::uint64_t owner = marquee_owner(state);
    float overflow = text_width - box_w;

    if (overflow <= 0.5f) {
        if (state.marqueeing) {
            anim.cancelForOwner(owner);
            state.marqueeing = false;
            state.scroll_offset = 0.0f;
        }
        state.last_text = text;
        state.last_box_w = box_w;
        return;
    }

    bool inputs_changed = !state.marqueeing || state.last_text != text ||
                          state.last_box_w != box_w;
    if (!inputs_changed)
        return;

    anim.cancelForOwner(owner);
    state.scroll_offset = 0.0f;
    state.last_text = text;
    state.last_box_w = box_w;
    state.marqueeing = true;
    start_pause_at_start(anim, state, owner, overflow);
}
