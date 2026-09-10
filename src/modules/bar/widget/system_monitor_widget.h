#pragma once

#include "modules/bar/widget/widget_capsule.h"

struct MonitorOutput;

namespace bar_detail {
Pill cpu_pill(MonitorOutput &mon);
} // namespace bar_detail
