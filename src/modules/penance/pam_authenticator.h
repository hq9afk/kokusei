#pragma once

#include <string>
#include <string_view>

namespace pam_auth {

struct Result {
    bool success = false;
    std::string message;
};

Result authenticate_current_user(std::string_view password);

void secure_clear(std::string &value);

} // namespace pam_auth
