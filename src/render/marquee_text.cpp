#include "render/marquee_text.h"
#include "render/panel_chrome.h"

void draw_marquee_text(Node *parent, TextureCache &cache,
                       AnimationManager &anim, MarqueeTextState &state,
                       std::int32_t scale, const std::string &text, float x,
                       float y, float w, const float *color) {
    using namespace panel_chrome_detail;
    const Texture *tex = cached_text(cache, text, scale);
    marquee_scroll_update(anim, state, text,
                          tex ? static_cast<float>(tex->width) : 0.0f, w);
    if (!tex)
        return;

    if (!state.marqueeing) {
        node_add_texture(parent, x, y, *tex, color);
        return;
    }

    Node *clip =
        node_add_group(parent, x, y, w, static_cast<float>(tex->height), true);
    node_add_texture(clip, -state.scroll_offset, 0.0f, *tex, color);
}
