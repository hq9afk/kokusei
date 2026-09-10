#include <algorithm>
#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app/config.h"

#include "core/log.h"
#include "core/path_home.h"

bool osd_effective_enabled(const Config &cfg,
                             const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.osd;
    return cfg.default_osd_enabled;
}

bool notifications_effective_enabled(const Config &cfg,
                               const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.notifications;
    return cfg.default_notifications_enabled;
}

bool autohide_effective_enabled(const Config &cfg,
                                const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.autohide;
    return cfg.autohide;
}

bool ambient_effective_enabled(const Config &cfg,
                               const std::string &monitor_name) {
    if (!cfg.idle_management_enabled)
        return false;
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.ambient_enabled;
    return cfg.ambient_enabled;
}

uint32_t ambient_effective_timeout_seconds(const Config &cfg,
                                           const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.ambient_timeout_seconds;
    return cfg.ambient_timeout_seconds;
}

bool screensaver_effective_enabled(const Config &cfg,
                                   const std::string &monitor_name) {
    if (!cfg.idle_management_enabled)
        return false;
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.screensaver_enabled;
    return cfg.screensaver_enabled;
}

uint32_t
screensaver_effective_timeout_seconds(const Config &cfg,
                                      const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.screensaver_timeout_seconds;
    return cfg.screensaver_timeout_seconds;
}

bool lock_effective_enabled(const Config &cfg,
                               const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.lock;
    return cfg.default_lock_panel_enabled;
}

std::string config_path() {
    const char *home = getenv("HOME");
    if (!home)
        return "";
    return std::string(home) + "/.config/kokusei/config.json";
}

namespace {

bool is_reserved_displays_key(const std::string &key) {
    return key == "defaultOsd" || key == "defaultNotifications" ||
           key == "defaultWallpaper" || key == "defaultLock" ||
           key == "defaultOsd" || key == "defaultNotifications" ||
           key == "defaultWallpaper" || key == "defaultLock";
}

nlohmann::json section(const nlohmann::json &j, const char *key,
                       const char *legacy_key) {
    if (j.contains(key))
        return j.value(key, nlohmann::json::object());
    return j.value(legacy_key, nlohmann::json::object());
}

void expand_column_paths(
    std::map<std::string, std::vector<std::string>> &columns) {
    for (auto &[name, paths] : columns)
        for (std::string &p : paths)
            p = path_expand_home(p);
}

nlohmann::json collapsed_column_paths(
    const std::map<std::string, std::vector<std::string>> &columns) {
    nlohmann::json out = nlohmann::json::object();
    for (const auto &[name, paths] : columns) {
        nlohmann::json arr = nlohmann::json::array();
        for (const std::string &p : paths)
            arr.push_back(path_collapse_home(p));
        out[name] = arr;
    }
    return out;
}

template <typename T>
T pick(const nlohmann::json &o, const char *key, const char *legacy_key,
       T fallback) {
    if (o.contains(key))
        return o.value(key, fallback);
    return o.value(legacy_key, fallback);
}

} // namespace

Config load_config() {
    Config cfg;
    std::string path = config_path();
    if (path.empty())
        return cfg;
    try {
        std::ifstream f(path);
        if (!f)
            return cfg;
        nlohmann::json j = nlohmann::json::parse(f);

        nlohmann::json bar = section(j, "bar", "qixing");
        cfg.autohide = bar.value("autohideEnabled", cfg.autohide);

        nlohmann::json wallpaper = section(j, "wallpaper", "expanse");
        cfg.wallpaper_dir = wallpaper.value("dir", cfg.wallpaper_dir);
        if (auto it = wallpaper.find("columns");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.wallpaper_columns[name] =
                        val.get<std::vector<std::string>>();
        if (auto it = wallpaper.find("columnCounts");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_number_integer())
                    cfg.wallpaper_column_counts[name] = val.get<int>();
        if (auto it = wallpaper.find("fillModes");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.wallpaper_fill_modes[name] =
                        val.get<std::vector<std::string>>();
        cfg.wallpaper_animated_enabled =
            wallpaper.value("animatedEnabled", cfg.wallpaper_animated_enabled);
        cfg.wallpaper_animated_dir =
            wallpaper.value("animatedDir", cfg.wallpaper_animated_dir);
        if (auto it = wallpaper.find("animatedColumns");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.wallpaper_animated_columns[name] =
                        val.get<std::vector<std::string>>();
        if (auto it = wallpaper.find("animatedColumnCounts");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_number_integer())
                    cfg.wallpaper_animated_column_counts[name] = val.get<int>();
        if (auto it = wallpaper.find("animatedFillModes");
            it != wallpaper.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.wallpaper_animated_fill_modes[name] =
                        val.get<std::vector<std::string>>();

        cfg.wallpaper_dir = path_expand_home(cfg.wallpaper_dir);
        cfg.wallpaper_animated_dir = path_expand_home(cfg.wallpaper_animated_dir);
        expand_column_paths(cfg.wallpaper_columns);
        expand_column_paths(cfg.wallpaper_animated_columns);

        nlohmann::json displays = j.value("displays", nlohmann::json::object());
        cfg.default_osd_enabled = pick(displays, "defaultOsd", "defaultSpark",
                                         cfg.default_osd_enabled);
        cfg.default_notifications_enabled =
            pick(displays, "defaultNotifications", "defaultHeralds",
                 cfg.default_notifications_enabled);
        cfg.default_wallpaper_enabled =
            pick(displays, "defaultWallpaper", "defaultExpanse",
                 cfg.default_wallpaper_enabled);
        cfg.default_lock_panel_enabled =
            pick(displays, "defaultLock", "defaultPenance",
                 cfg.default_lock_panel_enabled);
        for (const auto &[name, val] : displays.items()) {
            if (is_reserved_displays_key(name) || !val.is_object())
                continue;
            MonitorOverride mo;
            mo.enabled = val.value("_enabled", mo.enabled);
            mo.osd = pick(val, "osd", "spark", mo.osd);
            mo.notifications =
                pick(val, "notifications", "heralds", mo.notifications);
            mo.autohide = val.value("autohide", mo.autohide);
            mo.ambient_enabled =
                val.value("ambientEnabled", mo.ambient_enabled);
            mo.ambient_timeout_seconds =
                val.value("ambientTimeoutSeconds", mo.ambient_timeout_seconds);
            mo.screensaver_enabled =
                val.value("screensaverEnabled", mo.screensaver_enabled);
            mo.screensaver_timeout_seconds = val.value(
                "screensaverTimeoutSeconds", mo.screensaver_timeout_seconds);
            mo.lock = pick(val, "lock", "penance", mo.lock);
            cfg.monitor_overrides[name] = mo;
        }

        nlohmann::json logout = section(j, "logout", "starward");
        cfg.logout_animated_logo =
            logout.value("animatedLogo", cfg.logout_animated_logo);

        nlohmann::json idle = section(j, "idle", "blink");
        cfg.idle_management_enabled =
            idle.value("enabled", cfg.idle_management_enabled);
        cfg.ambient_enabled =
            idle.value("ambientEnabled", cfg.ambient_enabled);
        cfg.ambient_timeout_seconds =
            idle.value("ambientTimeoutSeconds", cfg.ambient_timeout_seconds);
        cfg.screensaver_enabled =
            idle.value("screensaverEnabled", cfg.screensaver_enabled);
        cfg.screensaver_timeout_seconds = idle.value(
            "screensaverTimeoutSeconds", cfg.screensaver_timeout_seconds);

        nlohmann::json visualizer = section(j, "visualizer", "resonance");
        cfg.visualizer.fps =
            std::clamp(visualizer.value("fps", cfg.visualizer.fps),
                       kVisualizerFpsMin, kVisualizerFpsMax);
        cfg.visualizer.particle_thin = std::clamp(
            visualizer.value("particleThin", cfg.visualizer.particle_thin),
            kVisualizerParticleThinMin, kVisualizerParticleThinMax);
        cfg.visualizer.particle_size = std::clamp(
            visualizer.value("particleSize", cfg.visualizer.particle_size),
            kVisualizerParticleSizeMin, kVisualizerParticleSizeMax);
        cfg.visualizer.fractal_complexity =
            std::clamp(visualizer.value("fractalComplexity",
                                       cfg.visualizer.fractal_complexity),
                       kVisualizerComplexityMin, kVisualizerComplexityMax);
        cfg.visualizer.glow_directions = std::clamp(
            visualizer.value("glowDirections", cfg.visualizer.glow_directions),
            kVisualizerGlowDirectionsMin, kVisualizerGlowDirectionsMax);
        cfg.visualizer.glow_quality = std::clamp(
            visualizer.value("glowQuality", cfg.visualizer.glow_quality),
            kVisualizerGlowQualityMin, kVisualizerGlowQualityMax);
        cfg.visualizer.visualizer_shape =
            visualizer.value("visualizerShape", std::string("bar")) == "sphere"
                ? VisualizerShape::Sphere
                : VisualizerShape::Bar;

        nlohmann::json rain = j.value("rain", nlohmann::json::object());
        cfg.rain.mode = rain.value("mode", std::string("matrix")) == "stiletto"
                            ? RainMode::Stiletto
                            : RainMode::Matrix;
        cfg.rain.async_speed = rain.value("asyncSpeed", false);
    } catch (const nlohmann::json::exception &) {
    }
    return cfg;
}

namespace {

bool write_file_atomic(const std::string &path, const std::string &content) {
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos)
        mkdir(path.substr(0, slash).c_str(), 0755);
    std::string tmp_path = path + ".tmp";
    {
        std::ofstream f(tmp_path, std::ios::trunc);
        if (!f || !(f << content))
            return false;
    }
    if (rename(tmp_path.c_str(), path.c_str()) != 0) {
        unlink(tmp_path.c_str());
        return false;
    }
    return true;
}

} // namespace

void save_config(const Config &cfg) {
    std::string path = config_path();
    if (path.empty())
        return;

    nlohmann::json wallpaper;
    wallpaper["dir"] = path_collapse_home(cfg.wallpaper_dir);
    wallpaper["columns"] = collapsed_column_paths(cfg.wallpaper_columns);
    wallpaper["columnCounts"] = cfg.wallpaper_column_counts;
    wallpaper["fillModes"] = cfg.wallpaper_fill_modes;
    wallpaper["animatedEnabled"] = cfg.wallpaper_animated_enabled;
    wallpaper["animatedDir"] = path_collapse_home(cfg.wallpaper_animated_dir);
    wallpaper["animatedColumns"] =
        collapsed_column_paths(cfg.wallpaper_animated_columns);
    wallpaper["animatedColumnCounts"] = cfg.wallpaper_animated_column_counts;
    wallpaper["animatedFillModes"] = cfg.wallpaper_animated_fill_modes;

    nlohmann::json displays;
    displays["defaultOsd"] = cfg.default_osd_enabled;
    displays["defaultNotifications"] = cfg.default_notifications_enabled;
    displays["defaultWallpaper"] = cfg.default_wallpaper_enabled;
    displays["defaultLock"] = cfg.default_lock_panel_enabled;
    for (const auto &[name, ov] : cfg.monitor_overrides) {
        nlohmann::json mo;
        mo["_enabled"] = ov.enabled;
        mo["osd"] = ov.osd;
        mo["notifications"] = ov.notifications;
        mo["autohide"] = ov.autohide;
        mo["ambientEnabled"] = ov.ambient_enabled;
        mo["ambientTimeoutSeconds"] = ov.ambient_timeout_seconds;
        mo["screensaverEnabled"] = ov.screensaver_enabled;
        mo["screensaverTimeoutSeconds"] = ov.screensaver_timeout_seconds;
        mo["lock"] = ov.lock;
        displays[name] = mo;
    }

    nlohmann::json idle;
    idle["enabled"] = cfg.idle_management_enabled;
    idle["ambientEnabled"] = cfg.ambient_enabled;
    idle["ambientTimeoutSeconds"] = cfg.ambient_timeout_seconds;
    idle["screensaverEnabled"] = cfg.screensaver_enabled;
    idle["screensaverTimeoutSeconds"] = cfg.screensaver_timeout_seconds;

    nlohmann::json visualizer;
    visualizer["fps"] = cfg.visualizer.fps;
    visualizer["particleThin"] = cfg.visualizer.particle_thin;
    visualizer["particleSize"] = cfg.visualizer.particle_size;
    visualizer["fractalComplexity"] = cfg.visualizer.fractal_complexity;
    visualizer["glowDirections"] = cfg.visualizer.glow_directions;
    visualizer["glowQuality"] = cfg.visualizer.glow_quality;
    visualizer["visualizerShape"] =
        cfg.visualizer.visualizer_shape == VisualizerShape::Sphere
            ? "sphere"
            : "bar";

    nlohmann::json rain;
    rain["mode"] = cfg.rain.mode == RainMode::Stiletto ? "stiletto" : "matrix";
    rain["asyncSpeed"] = cfg.rain.async_speed;

    nlohmann::json j;
    j["bar"] = {{"autohideEnabled", cfg.autohide}};
    j["wallpaper"] = wallpaper;
    j["displays"] = displays;
    j["logout"] = {{"animatedLogo", cfg.logout_animated_logo}};
    j["idle"] = idle;
    j["visualizer"] = visualizer;
    j["rain"] = rain;

    if (!write_file_atomic(path, j.dump(2)))
        klog("config: failed to save %s", path.c_str());
}

int config_watch_init(const std::string &path) {
    if (path.empty())
        return -1;
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0)
        return -1;
    if (inotify_add_watch(fd, path.c_str(), IN_MODIFY | IN_CLOSE_WRITE) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

ConfigWatchEvent config_watch_poll(int fd) {
    char buf[4096] __attribute__((aligned(alignof(struct inotify_event))));
    ConfigWatchEvent result;
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (char *p = buf; p < buf + n;) {
            auto *ev = reinterpret_cast<struct inotify_event *>(p);
            if (ev->mask & IN_IGNORED)
                result.removed = true;
            else
                result.changed = true;
            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    return result;
}
