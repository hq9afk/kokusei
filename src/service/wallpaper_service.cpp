#include <string>
#include <vector>

#include "service/wallpaper_service.h"

namespace {

template <typename Map>
const typename Map::mapped_type *lookup(const Map &map,
                                        const std::string &monitor_name) {
    auto it = map.find(monitor_name);
    return it != map.end() ? &it->second : nullptr;
}

const std::vector<std::string> *
columns_for(const Config &cfg, const std::string &monitor_name, bool animated) {
    return lookup(animated ? cfg.wallpaper_animated_columns : cfg.wallpaper_columns,
                  monitor_name);
}

const std::vector<std::string> *fill_modes_for(const Config &cfg,
                                               const std::string &monitor_name,
                                               bool animated) {
    return lookup(animated ? cfg.wallpaper_animated_fill_modes
                           : cfg.wallpaper_fill_modes,
                  monitor_name);
}

} // namespace

int wallpaper_service_column_count(const Config &cfg,
                                 const std::string &monitor_name,
                                 bool animated) {
    const auto *counts = lookup(animated ? cfg.wallpaper_animated_column_counts
                                         : cfg.wallpaper_column_counts,
                                monitor_name);
    return counts && *counts > 0 ? *counts : 1;
}

std::string wallpaper_service_column_override(const Config &cfg,
                                            const std::string &monitor_name,
                                            int column_index, bool animated) {
    const auto *cols = columns_for(cfg, monitor_name, animated);
    if (cols && column_index >= 0 &&
        static_cast<size_t>(column_index) < cols->size())
        return (*cols)[static_cast<size_t>(column_index)];
    return "";
}

std::string wallpaper_service_column_path(const Config &cfg,
                                        const std::string &monitor_name,
                                        int column_index, bool animated) {
    std::string override = wallpaper_service_column_override(
        cfg, monitor_name, column_index, animated);
    if (!override.empty())
        return override;
    return cfg.default_wallpaper_enabled ? cfg.wallpaper_path : "";
}

std::string wallpaper_service_fill_mode(const Config &cfg,
                                      const std::string &monitor_name,
                                      int column_index, bool animated) {
    const auto *modes = fill_modes_for(cfg, monitor_name, animated);
    if (modes && column_index >= 0 &&
        static_cast<size_t>(column_index) < modes->size() &&
        !(*modes)[static_cast<size_t>(column_index)].empty())
        return (*modes)[static_cast<size_t>(column_index)];
    return "crop";
}
