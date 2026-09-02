#include <cstdlib>

#include "core/path_home.h"

namespace {

std::string home_dir() {
    const char *home = getenv("HOME");
    return home && *home ? std::string(home) : std::string();
}

} // namespace

std::string path_collapse_home(const std::string &path) {
    std::string home = home_dir();
    if (home.empty() || path.compare(0, home.size(), home) != 0)
        return path;
    if (path.size() == home.size())
        return "~";
    if (path[home.size()] == '/')
        return "~" + path.substr(home.size());
    return path;
}

std::string path_expand_home(const std::string &path) {
    if (path.empty() || path[0] != '~')
        return path;
    if (path.size() == 1)
        return home_dir().empty() ? path : home_dir();
    if (path[1] != '/')
        return path;
    std::string home = home_dir();
    return home.empty() ? path : home + path.substr(1);
}
