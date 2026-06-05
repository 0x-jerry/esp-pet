# esp-pet

A Tamagotchi-style AI virtual pet for the ESP32-C6, featuring an ST7789 240×240 SPI display, two physical buttons, and **Xbox Wireless Controller** support via Bluetooth LE. The pet has stats (hunger, happiness, energy) that decay over time, and can talk back via the DeepSeek API — responding in-character with speech bubbles on screen.

## Hardware

| Component    | Model / Notes          |
|-------------|------------------------|
| MCU         | ESP32-C6               |
| Display     | ST7789 240×240 IPS SPI |
| Buttons     | 2x tactile (Action + Talk) |
| Controller  | Xbox Wireless Controller (BLE) |

### Pinout (ESP32-C6 SPI2 IOMUX)

| Signal | GPIO |
|--------|------|
| SCLK   | 6    |
| MOSI   | 7    |
| CS     | 2    |
| DC     | 1    |
| RST    | 0    |
| BL     | 3    |
| BTN_L  | 14   |
| BTN_R  | 15   |

## Controls

| Input              | Action                    |
|-------------------|---------------------------|
| BTN_L / Xbox A    | Cycle expressions (Happy → Neutral → Sad → Surprised) |
| BTN_R / Xbox B    | Pick random expression   |
| Xbox D-pad        | (reserved for navigation) |

### Pairing the Xbox Controller

1. Put the controller in pairing mode (hold the **pair button** on top until the Xbox logo flashes rapidly)
2. The ESP32-C6 will auto-detect and connect — the status line shows `Ctrl: connected`
3. If connection fails, press Reset on the ESP32-C6 and re-pair

## Status

- [x] Phase 1: Project scaffolding & display driver
- [x] Physical buttons + BLE Xbox controller input
- [ ] Phase 2: Pet state management
- [ ] Phase 3: WiFi & DeepSeek API client
- [ ] Phase 4: Pixel art sprites & UI rendering
- [ ] Phase 5: Game loop & integration
- [ ] Phase 6: Polish & configuration

## Tech Stack

- **Framework**: ESP-IDF v6.0.1 (C)
- **Display**: `esp_lcd_panel_st7789` native driver, custom framebuffer (no LVGL)
- **BLE**: NimBLE stack (BLE central/HID host for Xbox controller)
- **AI**: DeepSeek Chat Completions API (`deepseek-chat`)
- **JSON**: cJSON (bundled with ESP-IDF)
- **Storage**: NVS (Non-Volatile Storage) for pet state, WiFi creds, API key
- **WiFi**: `esp_wifi` + `esp_http_client` with HTTPS

## Build

```bash
# Requires ESP-IDF v6.0.1+ installed
eim run "idf.py set-target esp32c6"
eim run "idf.py build"
eim run "idf.py flash"
```

## License

MIT
