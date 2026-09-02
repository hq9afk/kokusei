#pragma once

#include <cstddef>
#include <string>

std::string elide(const std::string &s, size_t max_chars);

std::string elide_middle(const std::string &s, size_t max_chars);
