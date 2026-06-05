# esp-pet

A Tamagotchi-style AI virtual pet for the ESP32-C6, featuring an ST7789 240×240 SPI display and two buttons. The pet has stats (hunger, happiness, energy) that decay over time, and can talk back via the DeepSeek API — responding in-character with speech bubbles on screen.

## Hardware

| Component    | Model / Notes          |
|-------------|------------------------|
| MCU         | ESP32-C6               |
| Display     | ST7789 240×240 IPS SPI |
| Buttons     | 2× tactile (Action + Talk) |

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

## Status

- [x] Architecture & plan written — see [`plan.md`](plan.md)
- [x] Phase 1: Project scaffolding & display driver
- [ ] Phase 2: Pet state management
- [ ] Phase 3: Button input handling
- [ ] Phase 4: WiFi & DeepSeek API client
- [ ] Phase 5: Pixel art sprites & UI rendering
- [ ] Phase 6: Game loop & integration
- [ ] Phase 7: Polish & configuration

## Tech Stack

- **Framework**: ESP-IDF (C)
- **Display**: `esp_lcd_panel_st7789` native driver, custom framebuffer (no LVGL)
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
