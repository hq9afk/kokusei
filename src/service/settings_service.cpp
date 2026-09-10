#include <algorithm>

#include "core/log.h"
#include "core/path_home.h"

#include "service/settings_service.h"

void settings_service_apply_field_text(Config &cfg, SettingsFieldId id,
                                     const std::string &text,
                                     const std::string &monitor) {
    try {
        switch (id) {
        case SettingsFieldId::WallpaperPath:
            cfg.wallpaper_path = path_expand_home(text);
            break;
        case SettingsFieldId::WallpaperDir:
            cfg.wallpaper_dir = path_expand_home(text);
            break;
        case SettingsFieldId::WallpaperAnimatedDir:
            cfg.wallpaper_animated_dir = path_expand_home(text);
            break;
        case SettingsFieldId::AmbientTimeout: {
            auto v = static_cast<uint32_t>(std::clamp(
                std::stoi(text), kSettingsIdleTimeoutMin, kSettingsIdleTimeoutMax));
            if (monitor.empty())
                cfg.ambient_timeout_seconds = v;
            else
                cfg.monitor_overrides[monitor].ambient_timeout_seconds = v;
            break;
        }
        case SettingsFieldId::ScreensaverTimeout: {
            auto v = static_cast<uint32_t>(std::clamp(
                std::stoi(text), kSettingsIdleTimeoutMin, kSettingsIdleTimeoutMax));
            if (monitor.empty())
                cfg.screensaver_timeout_seconds = v;
            else
                cfg.monitor_overrides[monitor].screensaver_timeout_seconds = v;
            break;
        }
        case SettingsFieldId::VisualizerFps:
            cfg.visualizer.fps =
                std::clamp(std::stoi(text), kVisualizerFpsMin, kVisualizerFpsMax);
            break;
        case SettingsFieldId::VisualizerParticleThin:
            cfg.visualizer.particle_thin =
                std::clamp(std::stof(text), kVisualizerParticleThinMin,
                           kVisualizerParticleThinMax);
            break;
        case SettingsFieldId::VisualizerParticleSize:
            cfg.visualizer.particle_size =
                std::clamp(std::stoi(text), kVisualizerParticleSizeMin,
                           kVisualizerParticleSizeMax);
            break;
        case SettingsFieldId::VisualizerComplexity:
            cfg.visualizer.fractal_complexity =
                std::clamp(std::stoi(text), kVisualizerComplexityMin,
                           kVisualizerComplexityMax);
            break;
        case SettingsFieldId::VisualizerGlowDirections:
            cfg.visualizer.glow_directions =
                std::clamp(std::stof(text), kVisualizerGlowDirectionsMin,
                           kVisualizerGlowDirectionsMax);
            break;
        case SettingsFieldId::VisualizerGlowQuality:
            cfg.visualizer.glow_quality =
                std::clamp(std::stof(text), kVisualizerGlowQualityMin,
                           kVisualizerGlowQualityMax);
            break;
        default:
            break;
        }
    } catch (const std::exception &) {
        klog("settings: could not parse '%s' for field %d, keeping previous "
             "value",
             text.c_str(), static_cast<int>(id));
    }
}

void settings_service_save(const Config &cfg) { save_config(cfg); }
