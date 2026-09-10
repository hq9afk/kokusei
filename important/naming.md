# kokusei module names

Modules are named after their function. Every module directory, source pair, config header, symbol prefix, IPC verb, and `Module::name()` string uses the functional name. `rain` is the one non-functional name kept, because its two sims (`matrix_rain`, `stiletto_rain`) have no single functional label.

`config.cpp` keeps a legacy-key fallback so `config.json` files written under the earlier Keqing-lore names still load; the next save rewrites the keys to the functional names.

Description column is the verb's help text as printed by `kokusei help`.

| Module | Retired code name | IPC verb | Description |
|--------|-------------------|----------|-------------|
| `launcher` | `overseer` | `launcher` / `launcher global` | toggle the launcher, searching from $HOME / toggle the launcher, searching from / |
| `logout` | `starward` | `logout` | toggle the logout overlay |
| `dashboard` | `yuheng` | `dashboard` | toggle the control center |
| `overview` | `liyue` | `overview` | toggle the overview (Hyprland only) |
| `settings` | `trulla` | `settings` | toggle the settings panel |
| `rain` | `rain` (`matrix`) | `rain` | toggle the rain overlay (`matrix_rain` and `stiletto_rain` sims selectable in settings' Rain tab) |
| `visualizer` | `resonance` | `visualizer` | toggle the audio visualizer overlay |
| `lock` | `penance` | `lock` | lock the session |
| `idle` | `blink` | — | no IPC verb (per-monitor module) |
| `osd` | `spark` | — | no IPC verb (per-monitor module) |
| `bar` | `qixing` | — | no IPC verb (per-monitor module) |
| `wallpaper` | `expanse` | — | no IPC verb (per-monitor module) |
| `notification` | `herald` | — | no IPC verb (per-monitor module) |

Non-module verb: `kill` — gracefully quit kokusei.

## Config legacy keys

`config.cpp`'s `section()` / `pick()` read the functional key first, then fall back to the retired key: `bar` then `qixing`, `wallpaper` then `expanse`, `idle` then `blink`, `logout` then `starward`, `visualizer` then `resonance`, `osd` then `spark`, `notifications` then `heralds`, `lock` then `penance`, and the `displays` per-monitor `defaultOsd` / `defaultNotifications` / `defaultWallpaper` / `defaultLock` then their `defaultSpark` / `defaultHeralds` / `defaultExpanse` / `defaultPenance` predecessors.

## Renderer effect names

`logout`'s lightning effects keep their own names (`thunder_burst`, `thunder_shock`, `thunder_bolt`); they are effect names, not the module token.
