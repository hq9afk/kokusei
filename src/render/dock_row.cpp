#include <cmath>
#include <unordered_set>

#include "render/color_ops.h"
#include "render/dock_row.h"
#include "render/image.h"
#include "render/palette.h"

#include "service/icon_service.h"

namespace {

constexpr Color kFocusedTint =
    with_alpha(palette::text, kDockIconFocusedOpacity);
constexpr Color kUnfocusedTint =
    with_alpha(palette::text, kDockIconUnfocusedOpacity);

} // namespace

const Texture *DockIconCache::lookup(const std::string &window_class) {
    auto it = cache_.find(window_class);
    if (it == cache_.end()) {
        std::string path = resolve_window_icon_path(window_class);
        it =
            cache_
                .emplace(window_class,
                         path.empty() ? Texture{}
                                      : load_image_texture(path, kDockIconSize))
                .first;
    }
    return it->second.id ? &it->second : nullptr;
}

float dock_row_width(const std::vector<DockEntry> &entries) {
    if (entries.empty())
        return 0.0f;
    return static_cast<float>(entries.size()) * kDockIconSize +
           static_cast<float>(entries.size() - 1) * kDockIconSpacing;
}

void draw_dock_row(Node *root, DockIconCache &icons, DockRowState &row,
                   AnimationManager &animations, float x, float y_center,
                   const std::vector<DockEntry> &entries,
                   uint64_t anim_owner_base) {
    std::unordered_set<std::string> live;
    live.reserve(entries.size());
    for (const DockEntry &e : entries)
        live.insert(e.address);
    for (auto it = row.slot_x.begin(); it != row.slot_x.end();) {
        if (live.count(it->first))
            ++it;
        else
            it = row.slot_x.erase(it);
    }

    float icon_y = std::round(y_center - kDockIconSize / 2.0f);
    for (size_t i = 0; i < entries.size(); ++i) {
        const DockEntry &e = entries[i];
        float target_x =
            x + static_cast<float>(i) * (kDockIconSize + kDockIconSpacing);

        auto slot = row.slot_x.find(e.address);
        if (slot == row.slot_x.end()) {
            row.slot_x[e.address] = target_x;
        } else if (std::fabs(slot->second - target_x) > 0.5f) {
            std::string address = e.address;
            animations.animate(
                slot->second, target_x, kDockReorderMs, Easing::EaseOutQuad,
                [&row, address](float v) { row.slot_x[address] = v; }, {},
                anim_owner_base + i);
        }

        const Texture *tex = icons.lookup(e.window_class);
        if (!tex)
            continue;
        node_add_texture_rect(root, std::round(row.slot_x[e.address]), icon_y,
                              kDockIconSize, kDockIconSize, *tex,
                              rgba(e.focused ? kFocusedTint : kUnfocusedTint));
    }
}
