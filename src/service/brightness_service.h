#pragma once

#include <string>

struct BrightnessBackend {
    std::string device;
    int max = 0;
};

void brightness_init(BrightnessBackend &backend);

float brightness_get(const BrightnessBackend &backend);

void brightness_set(const BrightnessBackend &backend, float level01);

int brightness_watch_init(const BrightnessBackend &backend);

bool brightness_watch_poll(int fd);
