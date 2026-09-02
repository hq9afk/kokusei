#pragma once

#include "render/node.h"

void draw_flat_bar(Node *parent, float x, float y, float w, float h,
                   float radius, float fraction01, float min_fill_w,
                   const float *track_color, const float *fill_color);
