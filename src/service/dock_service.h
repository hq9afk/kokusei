#pragma once

#include <string>
#include <vector>

#include "service/hyprland_service.h"

struct DockEntry {
    std::string address;
    std::string window_class;
    bool focused = false;
};

std::vector<DockEntry>
dock_entries_for_monitor(const HyprlandState &hypr,
                         const std::string &monitor_name);
