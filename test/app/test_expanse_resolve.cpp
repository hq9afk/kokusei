
#include <cassert>

#include "service/expanse_service.h"

void test_expanse_resolve() {
    Config cfg;
    cfg.expanse_path = "/global.png";
    cfg.expanse_columns["DP-1"] = {"/dp1.png"};
    cfg.expanse_fill_modes["DP-1"] = {"fit", "tile"};

    assert(expanse_service_column_path(cfg, "DP-1", 0, false) == "/dp1.png");
    assert(expanse_service_column_path(cfg, "HDMI-1", 0, false) ==
           "/global.png");
    assert(expanse_service_fill_mode(cfg, "DP-1", 0, false) == "fit");
    assert(expanse_service_fill_mode(cfg, "DP-1", 1, false) == "tile");
    assert(expanse_service_fill_mode(cfg, "DP-1", 2, false) == "crop");
    assert(expanse_service_fill_mode(cfg, "HDMI-1", 0, false) == "crop");

    assert(expanse_service_column_path(cfg, "DP-1", 1, false) == "/global.png");
    assert(expanse_service_column_path(cfg, "HDMI-1", 3, false) ==
           "/global.png");
    assert(expanse_service_column_count(cfg, "DP-1", false) == 1);
    cfg.expanse_column_counts["DP-1"] = 2;
    assert(expanse_service_column_count(cfg, "DP-1", false) == 2);

    cfg.default_expanse_enabled = false;
    assert(expanse_service_column_path(cfg, "HDMI-1", 0, false).empty());
    assert(expanse_service_column_path(cfg, "DP-1", 1, false).empty());
    assert(expanse_service_column_path(cfg, "DP-1", 0, false) == "/dp1.png");

    assert(expanse_service_column_override(cfg, "HDMI-1", 0, false).empty());
    assert(expanse_service_column_override(cfg, "DP-1", 0, false) ==
           "/dp1.png");
    cfg.default_expanse_enabled = true;
    assert(expanse_service_column_override(cfg, "HDMI-1", 0, false).empty());

    cfg.expanse_animated_columns["DP-1"] = {"/dp1.mp4"};
    cfg.expanse_animated_fill_modes["DP-1"] = {"fit"};
    assert(expanse_service_column_path(cfg, "DP-1", 0, true) == "/dp1.mp4");
    assert(expanse_service_column_path(cfg, "HDMI-1", 0, true) ==
           "/global.png");
    assert(expanse_service_column_override(cfg, "HDMI-1", 0, true).empty());
    assert(expanse_service_column_override(cfg, "DP-1", 0, true) == "/dp1.mp4");
    assert(expanse_service_fill_mode(cfg, "DP-1", 0, true) == "fit");
    assert(expanse_service_fill_mode(cfg, "HDMI-1", 0, true) == "crop");
    assert(expanse_service_column_count(cfg, "DP-1", true) == 1);
    cfg.expanse_animated_column_counts["DP-1"] = 3;
    assert(expanse_service_column_count(cfg, "DP-1", true) == 3);

    cfg.default_expanse_enabled = false;
    assert(expanse_service_column_path(cfg, "HDMI-1", 0, true).empty());
    assert(expanse_service_column_path(cfg, "DP-1", 0, true) == "/dp1.mp4");
}
