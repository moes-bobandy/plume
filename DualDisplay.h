// ============================================================================
// DualDisplay — optional external ILI9341 on M5Cardputer ADV
// ============================================================================
// Pinout matches guicmg/cardputer_adv_external_screen:
//   CS=GPIO5, RST=GPIO13, DC=GPIO15, MOSI=GPIO14, SCK=GPIO40
// Init AFTER M5Cardputer.begin(). Rotation 7. Graceful no-op if absent.
//
// Heap policy: no full-frame sprite (320x240x2 ≈ 150KB). Direct panel draw
// only; callers must skip updates when free heap is low (~17KB typical).
//
// Shared SPI with the SD slot (FSPI / SPI2_HOST, MOSI/SCK). CS idle HIGH also
// keeps the Cardputer SD-slot enable asserted. Do not overlap DualDisplay
// transfers with SD I/O (take sdMutex in the sketch if needed).
//
// Pin conflict: RST/DC claim GPS UART pins (13/15). When present() is true,
// skip SerialGPS on those pins (see docs/LAUNCHER.md).
// ============================================================================
#pragma once

#include <M5GFX.h>
#include <SPI.h>
#include "driver/gpio.h"

// Set to 1 when an external ILI9341 is wired (guicmg pinout). Default 0 keeps
// upstream single-screen behavior and leaves GPS UART pins 13/15 alone.
// There is no MISO on that wiring, so hardware absence cannot be probed —
// enabling this claims RST/DC and skips GPS in the sketch when begin() succeeds.
#ifndef PLUME_DUAL_SCREEN
#define PLUME_DUAL_SCREEN 0
#endif

namespace DualDisplay {

static constexpr int EXT_W     = 320;
static constexpr int EXT_H     = 240;
static constexpr int PIN_CS    = 5;
static constexpr int PIN_RST   = 13;
static constexpr int PIN_DC    = 15;
static constexpr int PIN_MOSI  = 14;
static constexpr int PIN_SCK   = 40;
static constexpr uint32_t MIN_HEAP_DRAW = 6000;
static constexpr uint32_t DRAW_INTERVAL_MS = 750;

class ExtPanel : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI       _bus;
public:
    ExtPanel() {
        {
            auto cfg = _bus.config();
            cfg.spi_host   = SPI2_HOST;   // FSPI — same host as Plume SD
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_SCK;
            cfg.pin_mosi   = PIN_MOSI;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = PIN_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs   = PIN_CS;
            cfg.pin_rst  = PIN_RST;
            cfg.pin_busy = -1;
            cfg.panel_width   = 240;
            cfg.panel_height  = 320;
            cfg.memory_width  = 240;
            cfg.memory_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable   = false;
            cfg.invert     = false;
            cfg.rgb_order  = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;   // SD shares MOSI/SCK
            _panel.config(cfg);
        }
        setPanel(&_panel);
    }
};

struct FeedRow {
    char    name[20];
    int8_t  rssi;
    uint8_t proto;     // 0=WiFi 1=BLE
    bool    is_flock;
};

// File-scope state (header included once from the .ino).
static ExtPanel* s_ext     = nullptr;
static bool      s_present = false;
static bool      s_tried   = false;
static uint32_t  s_last_ms = 0;
static uint8_t   s_ext_store[sizeof(ExtPanel)];  // no heap for the device object

inline bool present() { return s_present && s_ext != nullptr; }

// Probe / init. Safe to call once; subsequent calls return cached result.
inline bool begin() {
    if (s_tried) return s_present;
    s_tried = true;

#if !PLUME_DUAL_SCREEN
    Serial.println("[DUAL] disabled (PLUME_DUAL_SCREEN=0) — single-screen / GPS kept");
    s_present = false;
    return false;
#endif

    // Power rail settle after M5Cardputer.begin() (ILI9341 needs >100ms).
    delay(100);

    s_ext = new (s_ext_store) ExtPanel();
    auto release_pins_for_fallback = []() {
        // Restore shared pins so SD enable + GPS UART can claim them later.
        pinMode(PIN_CS, OUTPUT);
        digitalWrite(PIN_CS, HIGH);          // SD slot enable
        gpio_reset_pin((gpio_num_t)PIN_RST); // GPS_TX
        gpio_reset_pin((gpio_num_t)PIN_DC);  // GPS_RX
    };

    // No MISO on the guicmg wiring — cannot readPanelID reliably. Trust init().
    // Absent panels usually still "succeed"; operators who did not wire EXT keep
    // GPS by leaving DualDisplay unused (init still runs but GPS skip only if
    // we mark present). We mark present only after a successful init + rotation.
    if (!s_ext->init()) {
        Serial.println("[DUAL] ILI9341 init() failed — single-screen fallback");
        s_ext->~ExtPanel();
        s_ext = nullptr;
        s_present = false;
        release_pins_for_fallback();
        return false;
    }

    s_ext->setRotation(7);   // landscape — guicmg recommendation
    s_ext->fillScreen(0x0000);
    s_ext->setTextDatum(tl_datum);
    s_ext->setTextColor(0xFFFF, 0x0000);
    s_ext->setTextSize(2);
    s_ext->setCursor(8, 8);
    s_ext->print("PLUME");
    s_ext->setTextSize(1);
    s_ext->setCursor(8, 32);
    s_ext->print("external display ready");
    s_ext->setCursor(8, 48);
    s_ext->print("CS5 RST13 DC15 MOSI14 SCK40");

    s_present = true;
    Serial.println("[DUAL] ILI9341 OK rot=7 (shared SPI with SD; GPS UART 13/15 claimed)");
    return true;
}

// Secondary view: detections summary + live feed rows. Direct draw, no sprite.
// Skips when absent, heap-low, or inside the refresh interval.
inline void pushFeedSummary(long session_wifi, long session_ble, long lifetime,
                            const FeedRow* rows, int nrows, uint32_t free_heap) {
    if (!present()) return;
    if (free_heap < MIN_HEAP_DRAW) return;
    uint32_t now = millis();
    if (s_last_ms != 0 && (now - s_last_ms) < DRAW_INTERVAL_MS) return;
    s_last_ms = now;

    ExtPanel& d = *s_ext;
    d.startWrite();
    d.fillScreen(0x0000);

    d.setTextSize(2);
    d.setTextColor(0x07FF, 0x0000);  // cyan
    d.setCursor(8, 6);
    d.print("PLUME // EXT");

    d.setTextSize(1);
    d.setTextColor(0xC618, 0x0000);
    d.setCursor(8, 28);
    d.printf("sess W:%ld B:%ld   life:%ld", session_wifi, session_ble, lifetime);
    d.setCursor(8, 40);
    d.printf("heap:%u   feed:%d", (unsigned)free_heap, nrows);
    d.drawFastHLine(8, 52, EXT_W - 16, 0x7BEF);

    int y = 60;
    if (nrows <= 0) {
        d.setTextColor(0x8410, 0x0000);
        d.setCursor(8, y);
        d.print("(waiting for devices)");
    } else {
        for (int i = 0; i < nrows && y < EXT_H - 14; i++) {
            const FeedRow& r = rows[i];
            uint16_t col = r.is_flock ? 0xFD20   // orange
                         : (r.proto ? 0xF81F    // magenta BLE
                                    : 0x07E0);  // green WiFi
            d.setTextColor(col, 0x0000);
            d.setCursor(8, y);
            char line[48];
            snprintf(line, sizeof(line), "%s %-16s %4d",
                     r.proto ? "BLE" : "WiFi", r.name, (int)r.rssi);
            d.print(line);
            y += 14;
        }
    }

    d.setTextColor(0x8410, 0x0000);
    d.setCursor(8, EXT_H - 12);
    d.print("primary UI stays on internal LCD");
    d.endWrite();
}

}  // namespace DualDisplay
