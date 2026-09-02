#pragma once

#include <string>

#include "app/config.h"

int expanse_service_column_count(const Config &cfg,
                                 const std::string &monitor_name,
                                 bool animated);

std::string expanse_service_column_override(const Config &cfg,
                                            const std::string &monitor_name,
                                            int column_index, bool animated);

std::string expanse_service_column_path(const Config &cfg,
                                        const std::string &monitor_name,
                                        int column_index, bool animated);

std::string expanse_service_fill_mode(const Config &cfg,
                                      const std::string &monitor_name,
                                      int column_index, bool animated);
