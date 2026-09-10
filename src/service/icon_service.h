#pragma once

#include <string>

std::string icon_direct_path(const std::string &icon_field);

std::string resolve_app_icon_path(const std::string &icon_field);

std::string resolve_window_icon_path(const std::string &window_class);
