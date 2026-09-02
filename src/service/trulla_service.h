#pragma once

#include <string>

#include "app/config.h"

#include "config/trulla_config.h"

void trulla_service_apply_field_text(Config &cfg, TrullaFieldId id,
                                     const std::string &text,
                                     const std::string &monitor = "");

void trulla_service_save(const Config &cfg);
