#pragma once

#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

inline std::string default_expanse_dir() {
    const char *home = getenv("HOME");
    return std::string(home ? home : "") + "/Pictures";
}

inline std::string default_animated_expanse_dir() {
    const char *home = getenv("HOME");
    return std::string(home ? home : "") + "/Videos";
}

struct MonitorOverride {
    bool enabled = false;
    bool spark = true;
    bool heralds = true;
    bool autohide = false;
    bool ambient_enabled = true;
    uint32_t ambient_timeout_seconds = 150;
    bool screensaver_enabled = true;
    uint32_t screensaver_timeout_seconds = 300;
    bool penance = true;

    bool operator==(const MonitorOverride &) const = default;
};

struct Config {
    std::string expanse_path = KOKUSEI_DEFAULT_WALLPAPER;
    std::string expanse_dir = default_expanse_dir();

    std::map<std::string, std::vector<std::string>> expanse_columns;
    std::map<std::string, int> expanse_column_counts;
    std::map<std::string, std::vector<std::string>> expanse_fill_modes;

    bool expanse_animated_enabled = false;
    std::string expanse_animated_dir = default_animated_expanse_dir();
    std::map<std::string, std::vector<std::string>> expanse_animated_columns;
    std::map<std::string, int> expanse_animated_column_counts;
    std::map<std::string, std::vector<std::string>> expanse_animated_fill_modes;

    bool autohide = false;
    bool default_spark_enabled = true;
    bool default_heralds_enabled = true;
    bool default_expanse_enabled = true;
    bool default_penance_panel_enabled = true;
    std::map<std::string, MonitorOverride> monitor_overrides;

    bool starward_animated_logo = true;

    bool blink_management_enabled = true;
    bool ambient_enabled = true;
    uint32_t ambient_timeout_seconds = 150;
    bool screensaver_enabled = true;
    uint32_t screensaver_timeout_seconds = 300;
};

bool spark_effective_enabled(const Config &cfg,
                             const std::string &monitor_name);

bool heralds_effective_enabled(const Config &cfg,
                               const std::string &monitor_name);

bool autohide_effective_enabled(const Config &cfg,
                                const std::string &monitor_name);

bool ambient_effective_enabled(const Config &cfg,
                               const std::string &monitor_name);

uint32_t ambient_effective_timeout_seconds(const Config &cfg,
                                           const std::string &monitor_name);

bool screensaver_effective_enabled(const Config &cfg,
                                   const std::string &monitor_name);

uint32_t screensaver_effective_timeout_seconds(const Config &cfg,
                                               const std::string &monitor_name);

bool penance_effective_enabled(const Config &cfg,
                               const std::string &monitor_name);

std::string config_path();

Config load_config();

void save_config(const Config &cfg);

int config_watch_init(const std::string &path);

struct ConfigWatchEvent {
    bool changed = false;
    bool removed = false;
};

ConfigWatchEvent config_watch_poll(int fd);
