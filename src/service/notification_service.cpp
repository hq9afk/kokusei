#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

#include "core/log.h"

#include "service/notification_service.h"

namespace {

int32_t resolve_timeout_ms(int32_t expire_timeout_ms) {
    return expire_timeout_ms > 0 ? expire_timeout_ms : 5000;
}

NotificationRecord *find_record(NotificationService &service, uint32_t id) {
    auto it =
        std::find_if(service.records.begin(), service.records.end(),
                     [id](const NotificationRecord &r) { return r.id == id; });
    return it == service.records.end() ? nullptr : &*it;
}

void apply_record(NotificationRecord &record, const std::string &app_name,
                  const std::string &summary, const std::string &body,
                  uint8_t urgency, int32_t expire_timeout_ms) {
    record.app_name = app_name;
    record.summary = summary;
    record.body = body;
    record.urgency = urgency;
    record.timeout_ms = resolve_timeout_ms(expire_timeout_ms);
    record.created_at = std::chrono::steady_clock::now();
    record.expires_at =
        record.created_at + std::chrono::milliseconds(record.timeout_ms);
}

} // namespace

uint32_t notification_service_push(NotificationService &service,
                                   const std::string &app_name,
                                   const std::string &summary,
                                   const std::string &body,
                                   int32_t expire_timeout_ms, uint32_t id,
                                   uint8_t urgency) {
    if (id == 0)
        id = service.next_id++;
    else if (id >= service.next_id)
        service.next_id = id + 1;

    if (NotificationRecord *existing = find_record(service, id)) {
        apply_record(*existing, app_name, summary, body, urgency,
                     expire_timeout_ms);
        klog("notification: replaced id=%u", id);
        return id;
    }

    NotificationRecord record;
    record.id = id;
    apply_record(record, app_name, summary, body, urgency, expire_timeout_ms);
    klog("notification: id=%u app='%s' summary='%s'", id, app_name.c_str(),
         summary.c_str());
    service.records.insert(service.records.begin(), std::move(record));
    return id;
}

bool notification_service_close(NotificationService &service, uint32_t id) {
    auto before = service.records.size();
    std::erase_if(service.records,
                  [id](const NotificationRecord &r) { return r.id == id; });
    return service.records.size() != before;
}

bool notification_service_sweep_expired(NotificationService &service) {
    auto now = std::chrono::steady_clock::now();
    auto before = service.records.size();
    std::erase_if(service.records, [now](const NotificationRecord &r) {
        return now >= r.expires_at;
    });
    return service.records.size() != before;
}

bool notification_service_init(NotificationService &service,
                               const std::function<void()> &on_change) {
    try {
        service.bus = sdbus::createSessionBusConnection();
        service.object = sdbus::createObject(
            *service.bus, sdbus::ObjectPath{"/org/freedesktop/Notifications"});

        service.object
            ->addVTable(
                sdbus::registerMethod("Notify").implementedAs(
                    [&service, on_change](
                        const std::string &app_name, uint32_t replaces_id,
                        const std::string &, const std::string &summary,
                        const std::string &body,
                        const std::vector<std::string> &,
                        const std::map<std::string, sdbus::Variant> &hints,
                        int32_t expire_timeout) -> uint32_t {
                        uint8_t urgency = 1;
                        auto hint_it = hints.find("urgency");
                        if (hint_it != hints.end()) {
                            try {
                                urgency = hint_it->second.get<uint8_t>();
                            } catch (const sdbus::Error &) {
                            }
                        }
                        uint32_t id = notification_service_push(
                            service, app_name, summary, body, expire_timeout,
                            replaces_id, urgency);
                        if (on_change)
                            on_change();
                        return id;
                    }),
                sdbus::registerMethod("CloseNotification")
                    .implementedAs([&service, on_change](uint32_t id) {
                        klog("notification: closed id=%u", id);
                        notification_service_close(service, id);
                        if (on_change)
                            on_change();
                    }),
                sdbus::registerMethod("GetCapabilities")
                    .implementedAs(
                        []() -> std::vector<std::string> { return {"body"}; }),
                sdbus::registerMethod("GetServerInformation")
                    .implementedAs(
                        []() -> std::tuple<std::string, std::string,
                                           std::string, std::string> {
                            return {"kokusei", "kokusei", "0.1.0", "1.2"};
                        }))
            .forInterface("org.freedesktop.Notifications");

        service.bus->requestName(
            sdbus::ServiceName{"org.freedesktop.Notifications"});
        klog("notification: registered org.freedesktop.Notifications");
        return true;
    } catch (const sdbus::Error &e) {
        klog("notification: D-Bus registration failed (%s): %s - is another "
             "notification daemon running?",
             e.getName().c_str(), e.getMessage().c_str());
        service.object.reset();
        service.bus.reset();
        return false;
    }
}
