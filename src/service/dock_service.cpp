#include <algorithm>

#include "service/dock_service.h"

std::vector<DockEntry>
dock_entries_for_monitor(const HyprlandState &hypr,
                         const std::string &monitor_name) {
    auto it = hypr.by_monitor.find(monitor_name);
    if (it == hypr.by_monitor.end() || it->second.active_id < 0)
        return {};
    int active_id = it->second.active_id;

    std::vector<const HyprClient *> matched;
    for (const HyprClient &c : hypr.clients)
        if (c.workspace_id == active_id)
            matched.push_back(&c);

    std::stable_sort(matched.begin(), matched.end(),
                     [](const HyprClient *a, const HyprClient *b) {
                         return a->at[0] < b->at[0];
                     });

    std::vector<DockEntry> entries;
    entries.reserve(matched.size());
    for (const HyprClient *c : matched)
        entries.push_back(
            {c->address, c->window_class, c->focus_history_id == 0});
    return entries;
}
