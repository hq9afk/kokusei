#include <algorithm>

#include "core/log.h"
#include "core/path_home.h"

#include "service/trulla_service.h"

void trulla_service_apply_field_text(Config &cfg, TrullaFieldId id,
                                     const std::string &text,
                                     const std::string &monitor) {
    try {
        switch (id) {
        case TrullaFieldId::ExpansePath:
            cfg.expanse_path = path_expand_home(text);
            break;
        case TrullaFieldId::ExpanseDir:
            cfg.expanse_dir = path_expand_home(text);
            break;
        case TrullaFieldId::ExpanseAnimatedDir:
            cfg.expanse_animated_dir = path_expand_home(text);
            break;
        case TrullaFieldId::AmbientTimeout: {
            auto v = static_cast<uint32_t>(std::clamp(
                std::stoi(text), kTrullaIdleTimeoutMin, kTrullaIdleTimeoutMax));
            if (monitor.empty())
                cfg.ambient_timeout_seconds = v;
            else
                cfg.monitor_overrides[monitor].ambient_timeout_seconds = v;
            break;
        }
        case TrullaFieldId::ScreensaverTimeout: {
            auto v = static_cast<uint32_t>(std::clamp(
                std::stoi(text), kTrullaIdleTimeoutMin, kTrullaIdleTimeoutMax));
            if (monitor.empty())
                cfg.screensaver_timeout_seconds = v;
            else
                cfg.monitor_overrides[monitor].screensaver_timeout_seconds = v;
            break;
        }
        case TrullaFieldId::ResonanceFps:
            cfg.resonance.fps =
                std::clamp(std::stoi(text), kResonanceFpsMin, kResonanceFpsMax);
            break;
        case TrullaFieldId::ResonanceParticleThin:
            cfg.resonance.particle_thin =
                std::clamp(std::stof(text), kResonanceParticleThinMin,
                           kResonanceParticleThinMax);
            break;
        case TrullaFieldId::ResonanceParticleSize:
            cfg.resonance.particle_size =
                std::clamp(std::stoi(text), kResonanceParticleSizeMin,
                           kResonanceParticleSizeMax);
            break;
        case TrullaFieldId::ResonanceComplexity:
            cfg.resonance.fractal_complexity =
                std::clamp(std::stoi(text), kResonanceComplexityMin,
                           kResonanceComplexityMax);
            break;
        case TrullaFieldId::ResonanceGlowDirections:
            cfg.resonance.glow_directions =
                std::clamp(std::stof(text), kResonanceGlowDirectionsMin,
                           kResonanceGlowDirectionsMax);
            break;
        case TrullaFieldId::ResonanceGlowQuality:
            cfg.resonance.glow_quality =
                std::clamp(std::stof(text), kResonanceGlowQualityMin,
                           kResonanceGlowQualityMax);
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

void trulla_service_save(const Config &cfg) { save_config(cfg); }
