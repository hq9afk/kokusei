#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "service/icon_service.h"

std::string icon_direct_path(const std::string &icon_field) {
    if (!icon_field.starts_with('/'))
        return "";
    return icon_field.ends_with(".png") ? icon_field : "";
}

namespace {

namespace fs = std::filesystem;

using IconIndex = std::unordered_map<std::string, std::string>;

std::string home_dir() {
    const char *home = getenv("HOME");
    return home ? home : "";
}

std::string active_gtk_icon_theme() {
    std::string home = home_dir();
    if (home.empty())
        return "";
    std::ifstream f(home + "/.config/gtk-3.0/settings.ini");
    std::string line;
    constexpr std::string_view kKey = "gtk-icon-theme-name=";
    while (std::getline(f, line)) {
        if (line.starts_with(kKey)) {
            std::string value = line.substr(kKey.size());
            while (!value.empty() &&
                   (value.back() == '\r' || value.back() == ' '))
                value.pop_back();
            return value;
        }
    }
    return "";
}

const std::vector<std::string> &theme_search_order() {
    static const std::vector<std::string> order = [] {
        std::vector<std::string> themes;
        std::string active = active_gtk_icon_theme();
        if (!active.empty() && active != "hicolor")
            themes.push_back(active);
        for (const char *theme :
             {"Adwaita", "AdwaitaLegacy", "breeze", "breeze-dark"}) {
            if (std::find(themes.begin(), themes.end(), theme) == themes.end())
                themes.push_back(theme);
        }
        themes.push_back("hicolor");
        return themes;
    }();
    return order;
}

struct ThemeIndex {
    IconIndex png;
    IconIndex svg;
};

void index_theme_root(const std::string &root, ThemeIndex &out) {
    std::error_code ec;
    if (!fs::is_directory(root, ec))
        return;
    for (const auto &entry : fs::recursive_directory_iterator(
             root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec || !entry.is_regular_file(ec))
            continue;
        const fs::path &p = entry.path();
        if (p.extension() == ".png")
            out.png.emplace(p.stem().string(), p.string());
        else if (p.extension() == ".svg")
            out.svg.emplace(p.stem().string(), p.string());
    }
}

const ThemeIndex &theme_index(const std::string &theme) {
    static std::unordered_map<std::string, ThemeIndex> indices;
    auto it = indices.find(theme);
    if (it != indices.end())
        return it->second;
    ThemeIndex index;
    std::string home = home_dir();
    if (!home.empty())
        index_theme_root(home + "/.local/share/icons/" + theme, index);
    index_theme_root("/usr/share/icons/" + theme, index);
    return indices.emplace(theme, std::move(index)).first->second;
}

} // namespace

std::string resolve_app_icon_path(const std::string &icon_field) {
    if (icon_field.empty())
        return "";
    std::string direct = icon_direct_path(icon_field);
    if (!direct.empty())
        return fs::exists(direct) ? direct : "";
    if (icon_field.starts_with('/'))
        return "";

    for (const std::string &theme : theme_search_order()) {
        const ThemeIndex &index = theme_index(theme);
        auto it = index.png.find(icon_field);
        if (it != index.png.end())
            return it->second;
    }
    for (const std::string &theme : theme_search_order()) {
        const ThemeIndex &index = theme_index(theme);
        auto it = index.svg.find(icon_field);
        if (it != index.svg.end())
            return it->second;
    }

    std::string pixmap = "/usr/share/pixmaps/" + icon_field + ".png";
    return fs::exists(pixmap) ? pixmap : "";
}
