#pragma once

#include <cstdint>
#include <vector>

#include "config/rain_config.h"

#include "render/texture.h"

class MatrixRain {
  public:
    void rebuild(int width, int height, bool async_speed);
    void tick();

    const Texture &texture() const { return texture_; }
    bool ready() const { return texture_.id != 0; }

  private:
    struct Column {
        float drop = 0.0f;
        bool ever_reset = false;
        float last_head_drop = 0.0f;
        char32_t last_glyph = U' ';
        bool last_head_valid = false;
        float speed = 1.0f;
        float accum = 0.0f;
    };

    void decay();
    char32_t random_glyph() const;
    float start_drop() const { return -2.0f; }

    int width_ = 0;
    int height_ = 0;
    int column_count_ = 0;
    int row_count_ = 0;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;
    bool sweeping_ = false;
    float sweep_drop_ = 0.0f;

    std::vector<uint8_t> buffer_;
    int stride_ = 0;
    std::vector<Column> columns_;
    Texture texture_;
};
