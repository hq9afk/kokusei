#pragma once

#include <cstdint>
#include <vector>

#include "config/rain_config.h"

#include "render/texture.h"

class StilettoRain {
  public:
    void rebuild(int width, int height, bool async_speed);
    void tick();

    const Texture &texture() const { return texture_; }
    bool ready() const { return texture_.id != 0; }

  private:
    struct Comet {
        float drop = 0.0f;
        bool ever_reset = false;
        float last_drop = 0.0f;
        bool last_valid = false;
        float speed = 1.0f;
    };

    void decay();
    float start_drop() const { return -2.0f * kStilettoRainStepPx; }

    int width_ = 0;
    int height_ = 0;
    int column_count_ = 0;
    float origin_x_ = 0.0f;
    bool sweeping_ = false;
    float sweep_drop_ = 0.0f;

    std::vector<uint8_t> buffer_;
    std::vector<uint8_t> frame_;
    int stride_ = 0;
    std::vector<Comet> comets_;
    Texture texture_;
};
