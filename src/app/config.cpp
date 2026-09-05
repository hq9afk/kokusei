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

bool spark_effective_enabled(const Config &cfg,
                             const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.spark;
    return cfg.default_spark_enabled;
}

bool heralds_effective_enabled(const Config &cfg,
                               const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.heralds;
    return cfg.default_heralds_enabled;
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
    if (!cfg.blink_management_enabled)
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
    if (!cfg.blink_management_enabled)
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

bool penance_effective_enabled(const Config &cfg,
                               const std::string &monitor_name) {
    auto it = cfg.monitor_overrides.find(monitor_name);
    if (it != cfg.monitor_overrides.end() && it->second.enabled)
        return it->second.penance;
    return cfg.default_penance_panel_enabled;
}

std::string config_path() {
    const char *home = getenv("HOME");
    if (!home)
        return "";
    return std::string(home) + "/.config/kokusei/config.json";
}

namespace {

bool is_reserved_displays_key(const std::string &key) {
    return key == "defaultSpark" || key == "defaultHeralds" ||
           key == "defaultExpanse" || key == "defaultPenance" ||
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

        nlohmann::json qixing = section(j, "qixing", "bar");
        cfg.autohide = qixing.value("autohideEnabled", cfg.autohide);

        nlohmann::json expanse = section(j, "expanse", "wallpaper");
        cfg.expanse_dir = expanse.value("dir", cfg.expanse_dir);
        if (auto it = expanse.find("columns");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.expanse_columns[name] =
                        val.get<std::vector<std::string>>();
        if (auto it = expanse.find("columnCounts");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_number_integer())
                    cfg.expanse_column_counts[name] = val.get<int>();
        if (auto it = expanse.find("fillModes");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.expanse_fill_modes[name] =
                        val.get<std::vector<std::string>>();
        cfg.expanse_animated_enabled =
            expanse.value("animatedEnabled", cfg.expanse_animated_enabled);
        cfg.expanse_animated_dir =
            expanse.value("animatedDir", cfg.expanse_animated_dir);
        if (auto it = expanse.find("animatedColumns");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.expanse_animated_columns[name] =
                        val.get<std::vector<std::string>>();
        if (auto it = expanse.find("animatedColumnCounts");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_number_integer())
                    cfg.expanse_animated_column_counts[name] = val.get<int>();
        if (auto it = expanse.find("animatedFillModes");
            it != expanse.end() && it->is_object())
            for (const auto &[name, val] : it->items())
                if (val.is_array())
                    cfg.expanse_animated_fill_modes[name] =
                        val.get<std::vector<std::string>>();

        cfg.expanse_dir = path_expand_home(cfg.expanse_dir);
        cfg.expanse_animated_dir = path_expand_home(cfg.expanse_animated_dir);
        expand_column_paths(cfg.expanse_columns);
        expand_column_paths(cfg.expanse_animated_columns);

        nlohmann::json displays = j.value("displays", nlohmann::json::object());
        cfg.default_spark_enabled = pick(displays, "defaultSpark", "defaultOsd",
                                         cfg.default_spark_enabled);
        cfg.default_heralds_enabled =
            pick(displays, "defaultHeralds", "defaultNotifications",
                 cfg.default_heralds_enabled);
        cfg.default_expanse_enabled =
            pick(displays, "defaultExpanse", "defaultWallpaper",
                 cfg.default_expanse_enabled);
        cfg.default_penance_panel_enabled =
            pick(displays, "defaultPenance", "defaultLock",
                 cfg.default_penance_panel_enabled);
        for (const auto &[name, val] : displays.items()) {
            if (is_reserved_displays_key(name) || !val.is_object())
                continue;
            MonitorOverride mo;
            mo.enabled = val.value("_enabled", mo.enabled);
            mo.spark = pick(val, "spark", "osd", mo.spark);
            mo.heralds = pick(val, "heralds", "notifications", mo.heralds);
            mo.autohide = val.value("autohide", mo.autohide);
            mo.ambient_enabled =
                val.value("ambientEnabled", mo.ambient_enabled);
            mo.ambient_timeout_seconds =
                val.value("ambientTimeoutSeconds", mo.ambient_timeout_seconds);
            mo.screensaver_enabled =
                val.value("screensaverEnabled", mo.screensaver_enabled);
            mo.screensaver_timeout_seconds = val.value(
                "screensaverTimeoutSeconds", mo.screensaver_timeout_seconds);
            mo.penance = pick(val, "penance", "lock", mo.penance);
            cfg.monitor_overrides[name] = mo;
        }

        nlohmann::json starward = j.value("starward", nlohmann::json::object());
        cfg.starward_animated_logo =
            starward.value("animatedLogo", cfg.starward_animated_logo);

        nlohmann::json blink = section(j, "blink", "idle");
        cfg.blink_management_enabled =
            blink.value("enabled", cfg.blink_management_enabled);
        cfg.ambient_enabled =
            blink.value("ambientEnabled", cfg.ambient_enabled);
        cfg.ambient_timeout_seconds =
            blink.value("ambientTimeoutSeconds", cfg.ambient_timeout_seconds);
        cfg.screensaver_enabled =
            blink.value("screensaverEnabled", cfg.screensaver_enabled);
        cfg.screensaver_timeout_seconds = blink.value(
            "screensaverTimeoutSeconds", cfg.screensaver_timeout_seconds);

        nlohmann::json resonance =
            j.value("resonance", nlohmann::json::object());
        cfg.resonance.fps =
            std::clamp(resonance.value("fps", cfg.resonance.fps),
                       kResonanceFpsMin, kResonanceFpsMax);
        cfg.resonance.particle_thin = std::clamp(
            resonance.value("particleThin", cfg.resonance.particle_thin),
            kResonanceParticleThinMin, kResonanceParticleThinMax);
        cfg.resonance.particle_size = std::clamp(
            resonance.value("particleSize", cfg.resonance.particle_size),
            kResonanceParticleSizeMin, kResonanceParticleSizeMax);
        cfg.resonance.fractal_complexity =
            std::clamp(resonance.value("fractalComplexity",
                                       cfg.resonance.fractal_complexity),
                       kResonanceComplexityMin, kResonanceComplexityMax);
        cfg.resonance.glow_directions = std::clamp(
            resonance.value("glowDirections", cfg.resonance.glow_directions),
            kResonanceGlowDirectionsMin, kResonanceGlowDirectionsMax);
        cfg.resonance.glow_quality = std::clamp(
            resonance.value("glowQuality", cfg.resonance.glow_quality),
            kResonanceGlowQualityMin, kResonanceGlowQualityMax);
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

    nlohmann::json expanse;
    expanse["dir"] = path_collapse_home(cfg.expanse_dir);
    expanse["columns"] = collapsed_column_paths(cfg.expanse_columns);
    expanse["columnCounts"] = cfg.expanse_column_counts;
    expanse["fillModes"] = cfg.expanse_fill_modes;
    expanse["animatedEnabled"] = cfg.expanse_animated_enabled;
    expanse["animatedDir"] = path_collapse_home(cfg.expanse_animated_dir);
    expanse["animatedColumns"] =
        collapsed_column_paths(cfg.expanse_animated_columns);
    expanse["animatedColumnCounts"] = cfg.expanse_animated_column_counts;
    expanse["animatedFillModes"] = cfg.expanse_animated_fill_modes;

    nlohmann::json displays;
    displays["defaultSpark"] = cfg.default_spark_enabled;
    displays["defaultHeralds"] = cfg.default_heralds_enabled;
    displays["defaultExpanse"] = cfg.default_expanse_enabled;
    displays["defaultPenance"] = cfg.default_penance_panel_enabled;
    for (const auto &[name, ov] : cfg.monitor_overrides) {
        nlohmann::json mo;
        mo["_enabled"] = ov.enabled;
        mo["spark"] = ov.spark;
        mo["heralds"] = ov.heralds;
        mo["autohide"] = ov.autohide;
        mo["ambientEnabled"] = ov.ambient_enabled;
        mo["ambientTimeoutSeconds"] = ov.ambient_timeout_seconds;
        mo["screensaverEnabled"] = ov.screensaver_enabled;
        mo["screensaverTimeoutSeconds"] = ov.screensaver_timeout_seconds;
        mo["penance"] = ov.penance;
        displays[name] = mo;
    }

    nlohmann::json blink;
    blink["enabled"] = cfg.blink_management_enabled;
    blink["ambientEnabled"] = cfg.ambient_enabled;
    blink["ambientTimeoutSeconds"] = cfg.ambient_timeout_seconds;
    blink["screensaverEnabled"] = cfg.screensaver_enabled;
    blink["screensaverTimeoutSeconds"] = cfg.screensaver_timeout_seconds;

    nlohmann::json resonance;
    resonance["fps"] = cfg.resonance.fps;
    resonance["particleThin"] = cfg.resonance.particle_thin;
    resonance["particleSize"] = cfg.resonance.particle_size;
    resonance["fractalComplexity"] = cfg.resonance.fractal_complexity;
    resonance["glowDirections"] = cfg.resonance.glow_directions;
    resonance["glowQuality"] = cfg.resonance.glow_quality;

    nlohmann::json j;
    j["qixing"] = {{"autohideEnabled", cfg.autohide}};
    j["expanse"] = expanse;
    j["displays"] = displays;
    j["starward"] = {{"animatedLogo", cfg.starward_animated_logo}};
    j["blink"] = blink;
    j["resonance"] = resonance;

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
