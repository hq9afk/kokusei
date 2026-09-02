#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "core/log.h"

#include "service/upower_service.h"

namespace {

enum class UpowerDeviceState : uint32_t {
    Charging = 1,
    FullyCharged = 4,
};

constexpr const char *kService = "org.freedesktop.UPower";
constexpr const char *kManagerPath = "/org/freedesktop/UPower";
constexpr const char *kManagerIface = "org.freedesktop.UPower";
constexpr const char *kDeviceIface = "org.freedesktop.UPower.Device";
constexpr const char *kPropertiesIface = "org.freedesktop.DBus.Properties";

template <typename T> std::optional<T> variant_get(const sdbus::Variant &v) {
    try {
        return v.get<T>();
    } catch (const sdbus::Error &) {
        return std::nullopt;
    }
}

void refresh(UpowerState &state) {
    if (!state.device)
        return;
    try {
        state.device->callMethodAsync("GetAll")
            .onInterface(kPropertiesIface)
            .withArguments(std::string(kDeviceIface))
            .uponReplyInvoke([&state](
                                 std::optional<sdbus::Error> err,
                                 std::map<std::string, sdbus::Variant> props) {
                if (err) {
                    klog("upower: property read failed (%s): %s",
                         err->getName().c_str(), err->getMessage().c_str());
                    return;
                }
                if (!state.device)
                    return;

                bool present = state.present;
                bool charging = state.charging;
                bool full = state.full;
                int percent = state.percent;

                if (auto it = props.find("IsPresent"); it != props.end())
                    if (auto v = variant_get<bool>(it->second))
                        present = *v;
                if (auto it = props.find("State"); it != props.end())
                    if (auto v = variant_get<uint32_t>(it->second)) {
                        charging = *v == static_cast<uint32_t>(
                                             UpowerDeviceState::Charging);
                        full = *v == static_cast<uint32_t>(
                                         UpowerDeviceState::FullyCharged);
                    }
                if (auto it = props.find("Percentage"); it != props.end())
                    if (auto v = variant_get<double>(it->second))
                        percent = static_cast<int>(std::lround(*v));

                if (present != state.present || charging != state.charging ||
                    full != state.full || percent != state.percent) {
                    state.dirty = true;
                }
                state.present = present;
                state.charging = charging;
                state.full = full;
                state.percent = percent;
            });
    } catch (const sdbus::Error &e) {
        klog("upower: GetAll dispatch failed (%s): %s", e.getName().c_str(),
             e.getMessage().c_str());
    }
}

void refresh_device(UpowerState &state, UpowerDeviceEntry &entry) {
    if (!entry.proxy)
        return;
    try {
        std::string path = entry.path;
        entry.proxy->callMethodAsync("GetAll")
            .onInterface(kPropertiesIface)
            .withArguments(std::string(kDeviceIface))
            .uponReplyInvoke(
                [&state, path](std::optional<sdbus::Error> err,
                               std::map<std::string, sdbus::Variant> props) {
                    if (err) {
                        klog("upower: device property read failed (%s): %s",
                             err->getName().c_str(), err->getMessage().c_str());
                        return;
                    }
                    auto it = std::find_if(
                        state.devices.begin(), state.devices.end(),
                        [&](const auto &e) { return e->path == path; });
                    if (it == state.devices.end())
                        return;
                    UpowerDeviceEntry &e = **it;

                    if (auto p = props.find("Type"); p != props.end())
                        if (auto v = variant_get<uint32_t>(p->second))
                            e.type = *v;
                    if (auto p = props.find("IsPresent"); p != props.end())
                        if (auto v = variant_get<bool>(p->second))
                            e.present = *v;
                    if (auto p = props.find("State"); p != props.end())
                        if (auto v = variant_get<uint32_t>(p->second))
                            e.state = *v;
                    if (auto p = props.find("Percentage"); p != props.end())
                        if (auto v = variant_get<double>(p->second))
                            e.percent = static_cast<int>(std::lround(*v));
                    if (auto p = props.find("TimeToEmpty"); p != props.end())
                        if (auto v = variant_get<int64_t>(p->second))
                            e.time_to_empty_s = static_cast<int>(*v);
                    if (auto p = props.find("TimeToFull"); p != props.end())
                        if (auto v = variant_get<int64_t>(p->second))
                            e.time_to_full_s = static_cast<int>(*v);
                    if (auto p = props.find("NativePath"); p != props.end())
                        if (auto v = variant_get<std::string>(p->second))
                            e.native_path = *v;
                    state.dirty = true;
                });
    } catch (const sdbus::Error &e) {
        klog("upower: device GetAll dispatch failed (%s): %s",
             e.getName().c_str(), e.getMessage().c_str());
    }
}

void refresh_on_battery(UpowerState &state) {
    if (!state.manager)
        return;
    try {
        state.manager->callMethodAsync("GetAll")
            .onInterface(kPropertiesIface)
            .withArguments(std::string(kManagerIface))
            .uponReplyInvoke(
                [&state](std::optional<sdbus::Error> err,
                         std::map<std::string, sdbus::Variant> props) {
                    if (err) {
                        klog("upower: OnBattery read failed (%s): %s",
                             err->getName().c_str(), err->getMessage().c_str());
                        return;
                    }
                    if (!state.manager)
                        return;
                    if (auto it = props.find("OnBattery"); it != props.end())
                        if (auto v = variant_get<bool>(it->second))
                            state.on_battery = *v;
                    state.dirty = true;
                });
    } catch (const sdbus::Error &e) {
        klog("upower: OnBattery GetAll dispatch failed (%s): %s",
             e.getName().c_str(), e.getMessage().c_str());
    }
}

void register_device(UpowerState &state, const std::string &path) {
    for (const auto &existing : state.devices)
        if (existing->path == path)
            return;

    auto entry = std::make_unique<UpowerDeviceEntry>();
    entry->path = path;
    try {
        entry->proxy = sdbus::createProxy(
            *state.bus, sdbus::ServiceName{kService}, sdbus::ObjectPath{path});
    } catch (const sdbus::Error &e) {
        klog("upower: failed to create proxy for %s: %s", path.c_str(),
             e.getMessage().c_str());
        return;
    }

    UpowerDeviceEntry *slot = entry.get();
    state.devices.push_back(std::move(entry));
    refresh_device(state, *slot);
    slot->proxy->uponSignal("PropertiesChanged")
        .onInterface(kPropertiesIface)
        .call([&state, slot](const std::string &,
                             const std::map<std::string, sdbus::Variant> &,
                             const std::vector<std::string> &) {
            refresh_device(state, *slot);
        });
}

} // namespace

bool upower_init(UpowerState &state) {
    try {
        state.bus = sdbus::createSystemBusConnection();
        state.manager =
            sdbus::createProxy(*state.bus, sdbus::ServiceName{kService},
                               sdbus::ObjectPath{kManagerPath});

        sdbus::ObjectPath device_path;
        state.manager->callMethod("GetDisplayDevice")
            .onInterface(kManagerIface)
            .storeResultsTo(device_path);

        state.device = sdbus::createProxy(
            *state.bus, sdbus::ServiceName{kService}, device_path);

        state.device->uponSignal("PropertiesChanged")
            .onInterface(kPropertiesIface)
            .call(
                [&state](const std::string &,
                         const std::map<std::string, sdbus::Variant> &,
                         const std::vector<std::string> &) { refresh(state); });

        std::vector<sdbus::ObjectPath> device_paths;
        state.manager->callMethod("EnumerateDevices")
            .onInterface(kManagerIface)
            .storeResultsTo(device_paths);
        for (const sdbus::ObjectPath &p : device_paths)
            register_device(state, p);

        state.manager->uponSignal("DeviceAdded")
            .onInterface(kManagerIface)
            .call([&state](const sdbus::ObjectPath &path) {
                register_device(state, path);
            });
        state.manager->uponSignal("DeviceRemoved")
            .onInterface(kManagerIface)
            .call([&state](const sdbus::ObjectPath &path) {
                auto it = std::find_if(
                    state.devices.begin(), state.devices.end(),
                    [&](const auto &e) { return e->path == path; });
                if (it != state.devices.end()) {
                    state.devices.erase(it);
                    state.dirty = true;
                }
            });
        state.manager->uponSignal("PropertiesChanged")
            .onInterface(kPropertiesIface)
            .call([&state](const std::string &,
                           const std::map<std::string, sdbus::Variant> &,
                           const std::vector<std::string> &) {
                refresh_on_battery(state);
            });

        refresh(state);
        refresh_on_battery(state);
        state.dirty = false;
        klog(
            "upower: connected, display device at %s, %zu device(s) enumerated",
            device_path.c_str(), state.devices.size());
        return true;
    } catch (const sdbus::Error &e) {
        klog("upower: connection failed (%s): %s - no battery info available",
             e.getName().c_str(), e.getMessage().c_str());
        state.device.reset();
        state.manager.reset();
        state.bus.reset();
        return false;
    }
}
