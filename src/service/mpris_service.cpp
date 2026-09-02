#include <optional>

#include "core/log.h"

#include "service/mpris_service.h"

MprisPlaybackStatus mpris_detail_parse_playback_status(const std::string &s) {
    if (s == "Playing")
        return MprisPlaybackStatus::Playing;
    if (s == "Paused")
        return MprisPlaybackStatus::Paused;
    return MprisPlaybackStatus::Stopped;
}

std::string mpris_detail_format_position(int64_t position_us) {
    if (position_us < 0)
        position_us = 0;
    int64_t total_seconds = position_us / 1000000;
    int64_t minutes = total_seconds / 60;
    int64_t seconds = total_seconds % 60;
    std::string result = std::to_string(minutes) + ":";
    if (seconds < 10)
        result += "0";
    result += std::to_string(seconds);
    return result;
}

bool mpris_detail_is_local_art_url(const std::string &url) {
    return url.starts_with("file://");
}

int mpris_detail_select_player(
    const std::vector<MprisPlayerCandidate> &players) {
    if (players.empty())
        return -1;
    for (size_t i = 0; i < players.size(); ++i)
        if (players[i].status == MprisPlaybackStatus::Playing)
            return static_cast<int>(i);
    return 0;
}

bool mpris_detail_scan_collect(MprisScan &scan, const std::string &bus_name,
                               MprisPlaybackStatus status) {
    scan.candidates.push_back({bus_name, status});
    return scan.expected > 0 && scan.candidates.size() >= scan.expected;
}

namespace mpris_detail {

constexpr const char *kBusDaemonService = "org.freedesktop.DBus";
constexpr const char *kBusDaemonPath = "/org/freedesktop/DBus";
constexpr const char *kPlayerObjectPath = "/org/mpris/MediaPlayer2";
constexpr const char *kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr const char *kPropertiesIface = "org.freedesktop.DBus.Properties";
constexpr const char *kNamePrefix = "org.mpris.MediaPlayer2.";

template <typename T> std::optional<T> variant_get(const sdbus::Variant &v) {
    try {
        return v.get<T>();
    } catch (const sdbus::Error &) {
        return std::nullopt;
    }
}

MprisTrackInfo
parse_metadata(const std::map<std::string, sdbus::Variant> &metadata) {
    MprisTrackInfo info;
    if (auto it = metadata.find("xesam:title"); it != metadata.end())
        if (auto v = variant_get<std::string>(it->second))
            info.title = *v;
    if (auto it = metadata.find("xesam:artist"); it != metadata.end())
        if (auto v = variant_get<std::vector<std::string>>(it->second))
            if (!v->empty())
                info.artist = (*v)[0];
    if (auto it = metadata.find("mpris:artUrl"); it != metadata.end())
        if (auto v = variant_get<std::string>(it->second))
            info.art_url = *v;
    if (auto it = metadata.find("mpris:length"); it != metadata.end())
        if (auto v = variant_get<int64_t>(it->second))
            info.length_us = *v;
    return info;
}

sdbus::IProxy *player_proxy(MprisState &state, sdbus::IConnection &bus,
                            const std::string &name) {
    auto it = state.player_proxies.find(name);
    if (it != state.player_proxies.end())
        return it->second.get();
    auto proxy = sdbus::createProxy(bus, sdbus::ServiceName{name},
                                    sdbus::ObjectPath{kPlayerObjectPath});
    sdbus::IProxy *raw = proxy.get();
    state.player_proxies.emplace(name, std::move(proxy));
    return raw;
}

void subscribe_player(MprisState &state, sdbus::IConnection &bus,
                      const std::string &name) {
    state.player = sdbus::createProxy(bus, sdbus::ServiceName{name},
                                      sdbus::ObjectPath{kPlayerObjectPath});
    state.selected_bus_name = name;
    state.has_player = true;

    state.player->uponSignal("PropertiesChanged")
        .onInterface(kPropertiesIface)
        .call([&state](const std::string &iface,
                       const std::map<std::string, sdbus::Variant> &changed,
                       const std::vector<std::string> &) {
            if (iface != kPlayerIface)
                return;
            if (auto it = changed.find("PlaybackStatus"); it != changed.end())
                if (auto s = variant_get<std::string>(it->second))
                    state.status = mpris_detail_parse_playback_status(*s);
            if (auto it = changed.find("Metadata"); it != changed.end())
                if (auto m = variant_get<std::map<std::string, sdbus::Variant>>(
                        it->second))
                    state.track = parse_metadata(*m);
        });

    state.player->callMethodAsync("Get")
        .onInterface(kPropertiesIface)
        .withArguments(std::string(kPlayerIface), std::string("PlaybackStatus"))
        .uponReplyInvoke(
            [&state, name](std::optional<sdbus::Error> err, sdbus::Variant v) {
                if (err || state.selected_bus_name != name)
                    return;
                if (auto s = variant_get<std::string>(v))
                    state.status = mpris_detail_parse_playback_status(*s);
            });

    state.player->callMethodAsync("Get")
        .onInterface(kPropertiesIface)
        .withArguments(std::string(kPlayerIface), std::string("Metadata"))
        .uponReplyInvoke([&state, name](std::optional<sdbus::Error> err,
                                        sdbus::Variant v) {
            if (err || state.selected_bus_name != name)
                return;
            if (auto m = variant_get<std::map<std::string, sdbus::Variant>>(v))
                state.track = parse_metadata(*m);
        });
}

void finish_scan(MprisState &state, sdbus::IConnection &bus) {
    int selected = mpris_detail_select_player(state.scan.candidates);
    if (selected < 0) {
        state.player.reset();
        state.selected_bus_name.clear();
        state.has_player = false;
        state.status = MprisPlaybackStatus::Stopped;
        state.track = MprisTrackInfo{};
        return;
    }
    std::string pick =
        state.scan.candidates[static_cast<size_t>(selected)].bus_name;
    if (pick != state.selected_bus_name)
        subscribe_player(state, bus, pick);
}

void begin_scan(MprisState &state, sdbus::IConnection &bus) {
    uint64_t gen = ++state.scan_generation;
    state.scan = MprisScan{};
    state.scan.generation = gen;

    state.dbus_daemon->callMethodAsync("ListNames")
        .onInterface(kBusDaemonService)
        .uponReplyInvoke([&state, &bus, gen](std::optional<sdbus::Error> err,
                                             std::vector<std::string> names) {
            if (err || gen != state.scan_generation)
                return;
            std::vector<std::string> players;
            for (const std::string &n : names)
                if (n.starts_with(kNamePrefix))
                    players.push_back(n);
            state.scan.expected = players.size();
            if (players.empty()) {
                finish_scan(state, bus);
                return;
            }
            for (const std::string &n : players) {
                player_proxy(state, bus, n)
                    ->callMethodAsync("Get")
                    .onInterface(kPropertiesIface)
                    .withArguments(std::string(kPlayerIface),
                                   std::string("PlaybackStatus"))
                    .uponReplyInvoke([&state, &bus, gen,
                                      n](std::optional<sdbus::Error> perr,
                                         sdbus::Variant v) {
                        if (gen != state.scan_generation)
                            return;
                        MprisPlaybackStatus st = MprisPlaybackStatus::Stopped;
                        if (!perr)
                            if (auto s = variant_get<std::string>(v))
                                st = mpris_detail_parse_playback_status(*s);
                        if (mpris_detail_scan_collect(state.scan, n, st))
                            finish_scan(state, bus);
                    });
            }
        });
}

} // namespace mpris_detail

bool mpris_init(MprisState &state) {
    using namespace mpris_detail;
    try {
        state.bus = sdbus::createSessionBusConnection();
        sdbus::IConnection &bus = *state.bus;

        state.dbus_daemon =
            sdbus::createProxy(bus, sdbus::ServiceName{kBusDaemonService},
                               sdbus::ObjectPath{kBusDaemonPath});

        state.dbus_daemon->uponSignal("NameOwnerChanged")
            .onInterface(kBusDaemonService)
            .call([&state, &bus](const std::string &name, const std::string &,
                                 const std::string &) {
                if (name.starts_with(kNamePrefix))
                    begin_scan(state, bus);
            });

        begin_scan(state, bus);
        klog("mpris: connected, scanning for players");
        return true;
    } catch (const sdbus::Error &e) {
        klog("mpris: connection failed (%s): %s", e.getName().c_str(),
             e.getMessage().c_str());
        state.dbus_daemon.reset();
        state.bus.reset();
        return false;
    }
}

namespace {

void call_player_method(MprisState &state, const char *method) {
    if (!state.player)
        return;
    try {
        state.player->callMethodAsync(method)
            .onInterface(mpris_detail::kPlayerIface)
            .uponReplyInvoke([method](std::optional<sdbus::Error> err) {
                if (err)
                    klog("mpris: %s failed: %s", method,
                         err->getMessage().c_str());
            });
    } catch (const sdbus::Error &e) {
        klog("mpris: %s dispatch failed: %s", method, e.getMessage().c_str());
    }
}

} // namespace

void mpris_play_pause(MprisState &state) {
    call_player_method(state, "PlayPause");
}

void mpris_next(MprisState &state) { call_player_method(state, "Next"); }

void mpris_previous(MprisState &state) {
    call_player_method(state, "Previous");
}

void mpris_poll_position(MprisState &state) {
    if (!state.has_player || !state.player || state.position_inflight)
        return;
    std::string name = state.selected_bus_name;
    try {
        state.position_inflight = true;
        state.player->callMethodAsync("Get")
            .onInterface(mpris_detail::kPropertiesIface)
            .withArguments(std::string(mpris_detail::kPlayerIface),
                           std::string("Position"))
            .uponReplyInvoke([&state, name](std::optional<sdbus::Error> err,
                                            sdbus::Variant v) {
                state.position_inflight = false;
                if (err || state.selected_bus_name != name)
                    return;
                if (auto pos = mpris_detail::variant_get<int64_t>(v))
                    state.track.position_us = *pos;
            });
    } catch (const sdbus::Error &) {
        state.position_inflight = false;
    }
}
