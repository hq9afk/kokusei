#include <algorithm>
#include <cmath>
#include <dirent.h>
#include <fstream>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>

#include "core/async_process.h"
#include "core/log.h"

#include "service/brightness_service.h"

namespace {

constexpr float kBrightnessMinLevel = 0.01f;

std::string find_backlight_device() {
    DIR *dir = opendir("/sys/class/backlight");
    if (!dir)
        return {};
    std::string name;
    while (dirent *entry = readdir(dir)) {
        if (entry->d_name[0] == '.')
            continue;
        name = entry->d_name;
        break;
    }
    closedir(dir);
    return name;
}

int read_int_file(const std::string &path) {
    std::ifstream f(path);
    int value = 0;
    f >> value;
    return value;
}

} // namespace

void brightness_init(BrightnessBackend &backend) {
    backend.device = find_backlight_device();
    if (backend.device.empty()) {
        klog("brightness: no backlight device found, brightness control "
             "disabled");
        return;
    }
    backend.max = read_int_file("/sys/class/backlight/" + backend.device +
                                "/max_brightness");
    klog("brightness: device %s (max %d)", backend.device.c_str(), backend.max);
}

float brightness_get(const BrightnessBackend &backend) {
    if (backend.device.empty() || backend.max <= 0)
        return 0.0f;
    int current =
        read_int_file("/sys/class/backlight/" + backend.device + "/brightness");
    return static_cast<float>(current) / backend.max;
}

void brightness_set(const BrightnessBackend &backend, float level01) {
    if (backend.device.empty())
        return;
    float clamped = std::clamp(level01, kBrightnessMinLevel, 1.0f);
    int percent = static_cast<int>(std::lround(clamped * 100.0f));
    spawn_detached("brightnessctl -q -d " + backend.device + " set " +
                   std::to_string(percent) + "%");
}

int brightness_watch_init(const BrightnessBackend &backend) {
    if (backend.device.empty())
        return -1;
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
        return -1;
    std::string path = "/sys/class/backlight/" + backend.device + "/brightness";
    if (inotify_add_watch(fd, path.c_str(), IN_MODIFY) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

bool brightness_watch_poll(int fd) {
    char buf[256];
    bool changed = false;
    while (read(fd, buf, sizeof(buf)) > 0)
        changed = true;
    return changed;
}
