## Dual Screen (optional)

Plume can drive an external **ILI9341** (320×240) on Cardputer ADV in addition to the built-in ST7789. The primary UI stays on the internal LCD; the external panel shows a live device-feed / detections summary.

- Module: `DualDisplay.h` — enable with `#define PLUME_DUAL_SCREEN 1` (default **0** = upstream single-screen + GPS)
- Wiring, pin conflicts (GPS / SD), and heap notes: **[docs/LAUNCHER.md](docs/LAUNCHER.md)**
- Init order: `M5Cardputer.begin` → internal display setup → `DualDisplay::begin()` → `setRotation(7)` on success

## Launcher install (bmorcelli)

Install Plume from SD using [bmorcelli Launcher](https://github.com/bmorcelli/Launcher) with an **app-only** `.bin` at offset **0x10000**. Do **not** flash merged bootloader+partitions images through Launcher.

Full steps (Arduino export, SD copy, Reset to return): **[docs/LAUNCHER.md](docs/LAUNCHER.md)**

