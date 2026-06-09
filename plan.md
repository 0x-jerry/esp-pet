# Plan: ESP32-C6 AI Virtual Pet (ST7789 + DeepSeek API)

**TL;DR**: Build a Tamagotchi-style virtual pet on ESP32-C6 with an ST7789 240x240 SPI display and two buttons. The pet has basic stats (hunger, happiness, energy) that decay over time, and the twist: pressing the "talk" button sends the pet's current context to the DeepSeek API, which responds in-character as the pet. Responses display as speech bubbles on screen.

---

## Progress

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ | Project scaffolding & ST7789 display driver |
| 2 | ✅ | Pet state management (stats, NVS, mood, tick timer) |
| 3 | ✅ | Button input + Xbox BLE controller |
| 4 | ✅ | Pixel art sprites & UI rendering (speech bubble, stat bars, gamepad debug) |
| 5 | ⬜ | WiFi & DeepSeek AI client |
| 6 | ⬜ | Game loop — integration & polish |
| 7 | ⬜ | Settings menu, sound, deep sleep |

**Current binary**: `esp-pet.bin` ~640KB (79% free in 3MB factory partition).

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                    ESP32-C6                      │
│  ┌──────────┐  ┌──────────┐  ┌───────────────┐  │
│  │ Button A │  │ Button B │  │ ST7789 240x240│  │
│  │ (Action) │  │  (Talk)  │  │   SPI Display │  │
│  └────┬─────┘  └────┬─────┘  └───────┬───────┘  │
│       │              │               │           │
│  ┌────▼──────────────▼───────────────▼───────┐  │
│  │              Main Loop                     │  │
│  │  ┌─────────┐  ┌──────────┐  ┌──────────┐  │  │
│  │  │Pet State│  │  Button   │  │ Display  │  │  │
│  │  │ Manager │  │  Handler  │  │ Renderer │  │  │
│  │  └────┬────┘  └──────────┘  └────┬─────┘  │  │
│  │       │                          │         │  │
│  │  ┌────▼──────────────────────────▼───────┐  │  │
│  │  │           AI Client                    │  │  │
│  │  │   (WiFi → HTTPS → DeepSeek API)       │  │  │
│  │  └───────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

---

## Hardware Pin Configuration (ESP32-C6 Native SPI2)

| Signal   | GPIO | Notes                       |
|----------|------|-----------------------------|
| SCLK     | 6    | SPI2 CLK (IO_MUX)           |
| MOSI     | 7    | SPI2 MOSI (IO_MUX)          |
| CS       | 2    | SPI2 CS, any GPIO           |
| DC       | 1    | Data/Command, any GPIO      |
| RST      | 0    | Display reset, any GPIO     |
| BL       | 3    | Backlight, any GPIO (PWM)   |
| BTN_L    | 14   | "Action" button (feed/play) |
| BTN_R    | 15   | "Talk" button (AI chat)     |

---

## Phases

### Phase 1: Project Scaffolding & Display Driver
*Blocks nothing — foundation for all other phases.*

1. **Create ESP-IDF project structure** with `idf.py create-project` or manual CMakeLists setup. Configure for ESP32-C6 target.
2. **Configure sdkconfig** — enable WiFi, HTTPS, SPI, and set CPU frequency to 160MHz. Set partition table for NVS storage.
3. **Implement ST7789 display driver** using ESP-IDF's `esp_lcd_panel_st7789` API (not raw SPI). Initialize SPI2 bus, create panel IO handle, configure panel (240x240, RGB ordering, 16bpp), turn on backlight via PWM.
4. **Create framebuffer abstraction** — a `display.c/h` module with `display_init()`, `display_fill_rect()`, `display_draw_sprite()`, `display_draw_text()`, `display_flush()` using `esp_lcd_panel_draw_bitmap()`.
5. **Test with a simple test pattern** — fill screen with color, draw rectangles, verify display works.

### Phase 2: Pet State Management ✅

*Implemented in `main/pet/pet_state.c/h`.*

- **Stats**: `pet_stats_t` — hunger, happiness, energy (0–100 each) + age_ticks
- **Decay**: `esp_timer` every 10s — awake (all -1), sleeping (energy +3, hunger -2, happiness -1)
- **Care actions**: `pet_feed()`, `pet_play()`, `pet_sleep()` — boost stats, 3s cooldown each
- **Mood**: `pet_mood_t` enum (HAPPY/NEUTRAL/SAD/HUNGRY/SLEEPY), auto-derived from stat thresholds
- **Persistence**: NVS namespace `"pet"`, auto-save every 60s if dirty, restore on `pet_init()`
- **Personality**: name (`"Pippy"` default), personality string, `pet_build_context_str()` for AI context

### Phase 3: Button Input Handling ✅

*Implemented in `main/buttons.c/h` + `main/gamepad/`.*

- **Physical buttons**: GPIO14 (Action), GPIO15 (Talk), internal pull-ups, software debounce
- **Xbox BLE controller**: NimBLE central, GATT HID client, parses HID reports for buttons + axes + triggers
- **Virtual controller**: `controller.c/h` abstraction layer — unified API for physical buttons + gamepad
- Button A → cycles Feed/Play/Sleep via D-Pad L/R, triggers Action on A press. Button B → Talk.

### Phase 4: Pixel Art & UI Rendering ✅

*Implemented in `main/pet/pet_sprite.c/h` + `main/ui/ui.c/h`.*

- **Pixel sprite** (`pet_sprite.c`): 48×48 procedural pixel art, 2 idle frames (bob + ear change) + 1 sleep frame (closed eyes, zZz), mood-based colors, 500ms animation via `esp_timer`
- **UI layout** (`ui.c`): Title bar, mood label (color-coded), hint bar, compact stat bars (Hunger/Happy/Energy with fill), gamepad debug overlay, rainbow gradient
- **Speech bubble**: Rounded-rect overlay, 6×8 font text wrapping, 8s auto-dismiss, toggle via Talk button
- **main.c**: Slimmed to ~130 lines — wiring only: input polling → actions → delegate to ui/pet_sprite modules

### Phase 5: WiFi & DeepSeek AI Client ⬜
*Next up. Depends on Phase 2 (pet context), Phase 4 (speech bubble ready).*

1. **Implement WiFi manager** — connect to stored SSID/password from NVS. Show connection status on display. Auto-reconnect on disconnect.
2. **Implement DeepSeek API client** in `ai_client.c`:
   - Build system prompt that defines pet personality (e.g., "You are a cute virtual pet named Pippy. You are a small fox-like creature...")
   - Attach current pet stats to user message context (hunger level, mood)
   - POST to `https://api.deepseek.com/chat/completions` with `deepseek-chat` model
   - Parse JSON response with cJSON to extract `choices[0].message.content`
   - Handle errors gracefully (timeout, no WiFi, API error) — show fallback canned responses
3. **Implement response caching** — store last few AI responses so repeated "talk" presses don't always call API (saves tokens).
4. **Store API key in NVS** — read DeepSeek API key from NVS, with a default placeholder for development.

### Phase 6: Game Loop & Integration ⬜
*Partially done — main.c wiring complete. Depends on Phase 5 (AI client).*

1. **Implement main loop** in `main.c` — using FreeRTOS tasks:
   - `display_task`: renders UI at ~30fps, handles animation
   - `pet_timer_task`: ticks pet stats every 10s
   - `button_task`: handles GPIO interrupts and debouncing
   - `ai_task`: handles async API calls (so display doesn't freeze)
2. **Wire everything together** — button presses update pet state, pet state drives display, button B triggers AI, AI response shows in speech bubble.
3. **Add startup sequence** — splash screen with pet name, WiFi connecting animation, then transition to main UI.

### Phase 7: Polish & Config
*Depends on Phase 6.*

1. **Add settings menu** (long-press Button B) — cycle through: set pet name, set WiFi SSID/password, show API key status, factory reset.
2. **Add sound feedback** (optional) — use ESP32-C6 LEDC for simple beeps on button press, feeding, etc. via a small buzzer on a spare GPIO.
3. **Add deep sleep support** — if no button pressed for 5 minutes, dim screen and slow stat decay. Wake on button press.

---

## Relevant Files

| File | Status | Purpose |
|------|--------|---------|
| `CMakeLists.txt` | ✅ | Top-level project CMake |
| `sdkconfig.defaults` | ✅ | Default Kconfig overrides |
| `main/CMakeLists.txt` | ✅ | Main component CMake (all modules registered) |
| `main/main.c` | ✅ | Entry point, display task, input wiring (~130 lines) |
| `main/display/display.h/c` | ✅ | ST7789 driver + SPI framebuffer |
| `main/display/graphics.h/c` | ✅ | RGB565 draw primitives (pixel, rect, text, sprites, rainbow) |
| `main/display/font.h/c` | ✅ | 6×8 bitmap font |
| `main/pet/pet_state.h/c` | ✅ | Stats, NVS, mood, tick timer, personality/context |
| `main/pet/pet_sprite.h/c` | ✅ | Pixel art sprites (48×48, 2 idle + 1 sleep), mood coloring, animation |
| `main/ui/ui.h/c` | ✅ | Layout zones, stat bars, gamepad debug, speech bubble |
| `main/buttons.h/c` | ✅ | GPIO handling, debounce |
| `main/gamepad/controller.h/c` | ✅ | Xbox virtual controller abstraction |
| `main/gamepad/xbox_ble.h/c` | ✅ | NimBLE Xbox GATT client |
| `main/gamepad/xbox_hid.h/c` | ✅ | HID report parser |
| `main/wifi_handler.h/c` | ⬜ | WiFi connect/reconnect, credential storage |
| `main/ai_client.h/c` | ⬜ | DeepSeek API HTTP client, JSON parsing |

---

## Verification

1. ✅ **Display test**: ST7789 shows test pattern, backlight works
2. ✅ **Pet state test**: Stats decay every 10s, care actions adjust stats, mood changes at thresholds, NVS persists across reboot
3. ✅ **Button test**: Short press triggers correct action, debounce works, Xbox controller connects via BLE and maps buttons correctly
4. ⬜ **WiFi test**: Flash with WiFi credentials in NVS, verify connection, verify auto-reconnect after AP reboot
5. ⬜ **AI test**: Press Talk button, verify DeepSeek API call succeeds, response displays in speech bubble, fallback message shows when offline
6. ⬜ **Integration test**: Run full loop — pet animates, stats decay, feed pet, talk to pet, AI responds in character, speech bubble appears and dismisses
7. ⬜ **Persistence test**: Reboot device, verify pet stats and name restore from NVS (basic restore already verified)
8. ⬜ **Long-run test**: Let pet run for 1+ hour, verify no memory leaks, stats don't overflow, WiFi stays connected

---

## Decisions

- **No LVGL**: Using direct framebuffer + simple drawing API instead. LVGL is heavy (~300KB+ flash) and overkill for a 240x240 pixel art display. Custom rendering gives full control over animations and uses less RAM.
- **ESP-IDF native SPI LCD driver**: Using `esp_lcd_panel_st7789` API rather than raw SPI. Cleaner code, built-in ST7789 init sequence, DMA support.
- **cJSON for API JSON**: Already included in ESP-IDF, lightweight, well-tested.
- **NVS for persistence**: ESP-IDF's Non-Volatile Storage for pet state, WiFi creds, API key. Survives reboots.
- **Model**: `deepseek-chat` (good balance of speed/cost for embedded device). `deepseek-reasoner` would be too slow/large.
- **Speech bubble UI**: AI responses shown as overlay text bubble that auto-dismisses — keeps main pet view clean.

---

## Further Considerations

1. **API costs**: Each "talk" press costs ~500-1000 tokens. With `deepseek-chat` pricing (~$0.14/M input, ~$0.28/M output), that's fractions of a cent per interaction. Could add a daily limit or cooldown.
2. **SSL certificates**: DeepSeek API uses HTTPS. ESP-IDF's `esp_http_client` needs root CA certificate bundled. Either use `mbedtls` with bundled cert or `.cert_pem` with `esp_tls` global CA store.
3. **Screen refresh**: ST7789 over SPI at 40MHz can push ~15-20fps for 240x240 at 16bpp. Fast enough for Tamagotchi-style animation. Use partial refresh (dirty rectangles) to reduce SPI traffic.
