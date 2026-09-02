#pragma once

#include <string>

#include "render/texture.h"

unsigned char *load_image_decode(const std::string &path, int &width,
                                 int &height, int svg_target_px = 0);

Texture load_image_texture(const std::string &path, int svg_target_px = 0);
