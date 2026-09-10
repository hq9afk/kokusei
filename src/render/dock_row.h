#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "config/dock_config.h"

#include "render/animation.h"
#include "render/node.h"
#include "render/texture.h"

#include "service/dock_service.h"

class DockIconCache {
  public:
    const Texture *lookup(const std::string &window_class);

  private:
    std::unordered_map<std::string, Texture> cache_;
};

struct DockRowState {
    std::unordered_map<std::string, float> slot_x;
};

float dock_row_width(const std::vector<DockEntry> &entries);

void draw_dock_row(Node *root, DockIconCache &icons, DockRowState &row,
                   AnimationManager &animations, float x, float y_center,
                   const std::vector<DockEntry> &entries,
                   uint64_t anim_owner_base);
