# kokusei module names

Modules are named after Keqing lore, not their function. Table maps the old
functional name to the current code name. IPC verbs and code identifiers all
moved with the rename; `config.cpp` keeps a legacy-key fallback so pre-rename
`config.json` files still load.

Description column is the verb's help text as printed by `kokusei help`.

| Old name | New name | IPC verb | Description |
|----------|----------|----------|-------------|
| launcher | overseer | `overseer` / `overseer global` | toggle the overseer, searching from $HOME / toggle the overseer, searching from / |
| logout | starward | `starward` | toggle the starward overlay (name kept unchanged through the rename) |
| dashboard | yuheng | `yuheng` | toggle the control center |
| overview | liyue | `liyue` | toggle the overview (Hyprland only) |
| settings | trulla | `trulla` | toggle the settings panel |
| matrix | stiletto | `stiletto` | toggle the matrix rain overlay |
| visualizer | resonance | `resonance` | toggle the audio visualizer overlay |
| lock | penance | `penance` | lock the session |
| idle | blink | — | no IPC verb (per-monitor module) |
| osd | spark | — | no IPC verb (per-monitor module) |
| bar | qixing | — | no IPC verb (per-monitor module) |
| wallpaper | expanse | — | no IPC verb (per-monitor module) |
| notification | herald | — | no IPC verb (per-monitor module) |

Non-module verb: `kill` — gracefully quit kokusei.
