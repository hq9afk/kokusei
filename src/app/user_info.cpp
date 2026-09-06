#include <pwd.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>

#include "app/user_info.h"

#ifndef KOKUSEI_PROFILE_MEDIA
#define KOKUSEI_PROFILE_MEDIA ""
#endif

namespace user_info {

std::string username() {
    struct passwd *pw = getpwuid(getuid());
    if (!pw)
        return "unknown";
    if (pw->pw_gecos && pw->pw_gecos[0] != '\0') {
        std::string gecos = pw->pw_gecos;
        std::string name = gecos.substr(0, gecos.find(','));
        if (!name.empty())
            return name;
    }
    return pw->pw_name ? pw->pw_name : "unknown";
}

std::string os_pretty_name() {
    std::ifstream f("/etc/os-release");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("PRETTY_NAME=", 0) != 0)
            continue;
        std::string v = line.substr(12);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        return v;
    }
    return "Linux";
}

std::string uptime_string() {
    struct sysinfo info;
    if (sysinfo(&info) != 0)
        return "";
    long hours = info.uptime / 3600;
    long minutes = (info.uptime % 3600) / 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "Up: %02ld:%02ld", hours, minutes);
    return buf;
}

std::string profile_media_path() {
    const char *candidates[] = {KOKUSEI_PROFILE_MEDIA,
                                "assets/gifs/profile.gif"};
    for (const char *c : candidates) {
        if (c && *c) {
            struct stat st{};
            if (stat(c, &st) == 0)
                return c;
        }
    }
    return "";
}

} // namespace user_info
