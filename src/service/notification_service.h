#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <sdbus-c++/sdbus-c++.h>

struct NotificationRecord {
    uint32_t id = 0;
    std::string app_name;
    std::string summary;
    std::string body;
    uint8_t urgency = 1;
    int32_t timeout_ms = 5000;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point expires_at;
};

struct NotificationService {
    std::unique_ptr<sdbus::IConnection> bus;
    std::unique_ptr<sdbus::IObject> object;
    uint32_t next_id = 1;
    std::vector<NotificationRecord> records;
};

bool notification_service_init(NotificationService &service,
                               const std::function<void()> &on_change);

uint32_t notification_service_push(NotificationService &service,
                                   const std::string &app_name,
                                   const std::string &summary,
                                   const std::string &body,
                                   int32_t expire_timeout_ms = -1,
                                   uint32_t id = 0, uint8_t urgency = 1);

bool notification_service_close(NotificationService &service, uint32_t id);

bool notification_service_sweep_expired(NotificationService &service);
