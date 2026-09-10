#include <cassert>

#include "service/dock_service.h"

void test_dock() {
    HyprlandState state;
    state.by_monitor["DP-1"].active_id = 2;
    state.by_monitor["DP-2"].active_id = 5;

    HyprClient a;
    a.address = "0xaaa";
    a.window_class = "firefox";
    a.workspace_id = 2;
    a.at = {300.0, 0.0};
    a.focus_history_id = 1;

    HyprClient b;
    b.address = "0xbbb";
    b.window_class = "kitty";
    b.workspace_id = 2;
    b.at = {100.0, 0.0};
    b.focus_history_id = 0;

    HyprClient c;
    c.address = "0xccc";
    c.window_class = "mpv";
    c.workspace_id = 1;
    c.at = {0.0, 0.0};
    c.focus_history_id = 2;

    state.clients = {a, b, c};

    auto entries = dock_entries_for_monitor(state, "DP-1");
    assert(entries.size() == 2);
    assert(entries[0].address == "0xbbb");
    assert(entries[0].window_class == "kitty");
    assert(entries[0].focused);
    assert(entries[1].address == "0xaaa");
    assert(!entries[1].focused);

    assert(dock_entries_for_monitor(state, "DP-2").empty());
    assert(dock_entries_for_monitor(state, "HDMI-1").empty());

    state.clients[0].at = {50.0, 0.0};
    auto reordered = dock_entries_for_monitor(state, "DP-1");
    assert(reordered.size() == 2);
    assert(reordered[0].address == "0xaaa");
    assert(reordered[1].address == "0xbbb");
}
