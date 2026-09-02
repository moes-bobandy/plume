# Launcher + Dual Screen

Install Plume as an **app-only** binary via [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) on Cardputer ADV, and optionally drive an external ILI9341 beside the built-in LCD.

## Build an app-only `.bin` (do NOT flash a merged image via Launcher)

Launcher expects a firmware **application image** that loads at flash offset **`0x10000`**. It already owns the bootloader + partition table on device.

### Arduino IDE

1. Board: **M5Cardputer** (ESP32-S3), USB CDC on Boot enabled as usual for Plume.
2. Partition scheme: match what Launcher installed on the device (typically a scheme with app + SPIFFS/LittleFS). Do not invent a new layout just for export.
3. Sketch → **Export Compiled Binary**.
4. In the build output folder, pick the **app** image — commonly named like:
   - `FlockDetection_Cardputer_ADV.ino.bin`, or
   - `FlockDetection_Cardputer_ADV.ino.app.bin`
5. **Do not** copy / install via Launcher:
   - `*bootloader*.bin`
   - `*partitions*.bin`
   - merged / full-flash images (`*merged*.bin`, factory bundles that include bootloader+partitions+app)

Those merged images are for `esptool` / USB full-flash only. Flashing them through Launcher can brick the Launcher partition layout.

### Confirm offset

Arduino-ESP32 app images are linked for **`0x10000`**. If you build with another toolchain, verify the start address before copying to SD.

## Copy to SD and install from Launcher

1. Format the card **FAT32** (≤ 32 GB is the least painful path on Cardputer).
2. Copy the **app-only** `.bin` to the SD card (root or a folder Launcher browses — e.g. `/apps` or `/firmware`, depending on your Launcher build).
3. Insert the SD into the Cardputer ADV.
4. Boot **Launcher**, open the SD file browser, select the Plume `.bin`, and install / run it as an application.
5. On first run you should see the normal Plume boot sequence on the **internal** 240×135 LCD.

### Return to Launcher

- Press the Cardputer **physical Reset** button to leave Plume and return to Launcher (Launcher remains in its slot; Plume is the app image).
- There is no in-app “exit to Launcher” key — Reset is the documented path.
- After Reset, Launcher should come back; if you full-flashed a merged image over USB earlier, restore Launcher with its own installer before using SD app install again.

## Dual-screen wiring (optional ILI9341)

Reference: [guicmg/cardputer_adv_external_screen](https://github.com/guicmg/cardputer_adv_external_screen).

| Cardputer ADV | GPIO | ILI9341 | Function |
| :--- | :---: | :--- | :--- |
| Pin 2 | — | VCC / 5VIN | Power |
| Pin 4 | — | GND | Ground |
| Pin 13 | **5** | CS | Chip select (idle HIGH also enables SD slot) |
| Pin 12 | **13** | RESET | Panel reset |
| Pin 14 | **15** | DC | Data / command |
| Pin 9 | **14** | MOSI | SPI data (shared with SD) |
| Pin 7 | **40** | SCK | SPI clock (shared with SD) |
| Pin 6 | — | LED / backlight | Prefer 5VOUT / backlight pin on the module |

Software: set `PLUME_DUAL_SCREEN` to **1**, then `DualDisplay::begin()` runs **after** `M5Cardputer.begin()` and internal `setRotation(1)`. External panel uses **`setRotation(7)`**. Default `PLUME_DUAL_SCREEN=0` never touches EXT pins (upstream single-screen + GPS). With `=1`, init runs after internal display setup; `setRotation(7)` on success. No MISO on this wiring — absence cannot be probed; only use `=1` when the panel is actually wired.

### Conflicts (read before wiring)

- **GPS UART** on Plume uses **GPIO 15 (RX) / 13 (TX)**. Dual-screen RST/DC claim those pins. When the external panel is detected, Plume **skips GPS init** so the display can work.
- **SD** uses FSPI on **SCK=40 / MOSI=14 / MISO=39 / CS=12**, with **GPIO5 HIGH** enabling the slot. Dual-screen CS is also GPIO5: idle HIGH keeps the slot enabled; avoid SD I/O while the external panel is mid-transfer (Plume refreshes the external view on a timer and skips draws under ~6KB free heap).

### Heap

No PSRAM. Plume already keeps ~**17KB** free after init. The external path **does not** allocate a 320×240 framebuffer; it draws text directly and **skips** updates if heap is below 6KB. If alloc/init fails, internal UI is unchanged.

## Flash-test checklist

1. **No external panel:** boot matches upstream; GPS works on ADV; Launcher install of app-only `.bin` runs.
2. **With ILI9341 wired:** external shows `PLUME // EXT` feed summary; internal UI unchanged; GPS skipped (serial log notes DualDisplay).
3. **Heap:** under load, external refresh may pause; internal scanner must not crash / sprite-fail.
4. **SD logging:** detections still flush with dual-screen idle between draws.
5. **Reset** returns to Launcher.
