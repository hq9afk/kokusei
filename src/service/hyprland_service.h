#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct Workspace {
    int id = -1;
    std::string name;
    bool occupied = false;
};

struct MonitorWorkspaces {
    std::vector<Workspace> workspaces;
    int active_id = -1;
};

struct HyprMonitor {
    int id = -1;
    std::string name;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double scale = 1.0;
    int transform = 0;
    std::array<double, 4> reserved{0.0, 0.0, 0.0, 0.0};
};

struct HyprClient {
    std::string address;
    std::string window_class;
    std::string title;
    int workspace_id = -1;
    int monitor_id = -1;
    std::array<double, 2> at{0.0, 0.0};
    std::array<double, 2> size{0.0, 0.0};
    bool floating = false;
    int fullscreen = 0;
    bool pinned = false;
    long focus_history_id = 0;
    bool xwayland = false;
};

struct HyprlandState {
    std::string request_socket_path;
    std::string event_socket_path;
    int event_fd = -1;
    std::unordered_map<std::string, MonitorWorkspaces> by_monitor;
    std::string focused_monitor;
    std::vector<HyprMonitor> monitors;
    std::vector<HyprClient> clients;
};

void hypr_refresh(HyprlandState &state);

bool hypr_init(HyprlandState &state);

enum class HyprEventResult {
    None,
    ActiveChanged,
    StructuralChanged,
    Disconnected
};

HyprEventResult hypr_poll_events(HyprlandState &state);

void hypr_dispatch(HyprlandState &state, const std::string &command);

void hypr_tile_focus_workspace(HyprlandState &state, int id,
                               bool global = false);

void hypr_tile_move_window(HyprlandState &state, int id, bool follow = true,
                           const std::string &address = {},
                           bool global = false);

enum class HyprCloseScope { Workspace, Monitor, All };

void hypr_tile_close_workspace(HyprlandState &state, HyprCloseScope scope,
                               int id = -1);

void hypr_tile_move_workspace_in(HyprlandState &state, int id,
                                 bool global = false);

void hypr_tile_swap_workspace(HyprlandState &state, int id,
                              bool global = false);
