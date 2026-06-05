# Plan: ESP32-C6 AI Virtual Pet (ST7789 + DeepSeek API)

**TL;DR**: Build a Tamagotchi-style virtual pet on ESP32-C6 with an ST7789 240x240 SPI display and two buttons. The pet has basic stats (hunger, happiness, energy) that decay over time, and the twist: pressing the "talk" button sends the pet's current context to the DeepSeek API, which responds in-character as the pet. Responses display as speech bubbles on screen.

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

### Phase 2: Pet State Management
*Depends on Phase 1 (needs display for debug), can partially parallel with Phase 3.*

1. **Define pet data structures** in `pet_state.h`:
   - `pet_stats_t`: hunger (0-100), happiness (0-100), energy (0-100), age, mood enum
   - `pet_context_t`: wraps stats + name + personality description for AI
2. **Implement stat decay timer** — use `esp_timer` to tick stats every N seconds (hunger -1, happiness -1, energy -1 when awake). Stats bottom out at 0.
3. **Implement care actions** — `pet_feed()`, `pet_play()`, `pet_sleep()` that boost respective stats. Each has cooldown to prevent spam.
4. **Implement mood calculation** — derive mood (happy, neutral, sad, hungry, sleepy) from stat thresholds.
5. **Save/load state to NVS** — persist pet stats on change (debounced), restore on boot. Store pet name and personality too.

### Phase 3: Button Input Handling
*Parallel with Phase 2.*

1. **Configure GPIO interrupts** for both buttons with internal pull-ups, debounce with a 200ms software debounce timer.
2. **Implement short-press / long-press detection** — short press = primary action, long press (2s) = secondary action or menu.
3. **Button A (Action) behavior** — short press cycles through: Feed → Play → Sleep → (back to idle). Each triggers corresponding `pet_*()` function.
4. **Button B (Talk) behavior** — short press triggers AI conversation with current pet context. Long press enters settings menu (WiFi config, pet name).

### Phase 4: WiFi & DeepSeek AI Client
*Depends on Phase 2 (needs pet context). Parallel with Phase 3.*

1. **Implement WiFi manager** — connect to stored SSID/password from NVS. Show connection status on display. Auto-reconnect on disconnect.
2. **Implement DeepSeek API client** in `ai_client.c`:
   - Build system prompt that defines pet personality (e.g., "You are a cute virtual pet named Pippy. You are a small fox-like creature...")
   - Attach current pet stats to user message context (hunger level, mood)
   - POST to `https://api.deepseek.com/chat/completions` with `deepseek-chat` model
   - Parse JSON response with cJSON to extract `choices[0].message.content`
   - Handle errors gracefully (timeout, no WiFi, API error) — show fallback canned responses
3. **Implement response caching** — store last few AI responses so repeated "talk" presses don't always call API (saves tokens).
4. **Store API key in NVS** — read DeepSeek API key from NVS, with a default placeholder for development.

### Phase 5: Pixel Art & UI Rendering
*Depends on Phase 1 (display), Phase 2 (pet state), Phase 4 (AI responses).*

1. **Create pixel art sprites** — define pet sprite as a `const uint16_t[]` bitmap array (e.g., 48x48 pixels, RGB565). Create multiple frames for idle animation, plus mood variants (happy, sad, hungry).
2. **Implement sprite rendering** — `display_draw_sprite(x, y, width, height, data)` that copies pixel data to framebuffer.
3. **Implement simple idle animation** — cycle through 2-3 sprite frames every ~500ms using timer.
4. **Build main UI screen layout** (240x240):
   - Top 16px: Status bar (pet name, WiFi icon)
   - Center 120px: Pet sprite + animation area
   - Below pet: Mood icon/emoji
   - Bottom 40px: Stat bars (hunger, happiness, energy as horizontal bars)
5. **Build speech bubble overlay** — when AI responds, overlay a rounded-rect speech bubble with text. Auto-dismiss after 8 seconds or on button press.
6. **Implement text wrapping** — for AI responses, wrap text to fit within speech bubble width. Use a simple 8x8 or 6x8 bitmap font.

### Phase 6: Game Loop & Integration
*Depends on all prior phases.*

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

## Relevant Files (to be created)

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Top-level project CMake |
| `sdkconfig.defaults` | Default Kconfig overrides |
| `main/CMakeLists.txt` | Main component CMake |
| `main/main.c` | Entry point, FreeRTOS tasks, game loop |
| `main/display.h` / `display.c` | ST7789 driver + framebuffer + drawing primitives |
| `main/pet_state.h` / `pet_state.c` | Pet stats, decay logic, mood, NVS persistence |
| `main/buttons.h` / `buttons.c` | GPIO interrupt handling, debounce, long/short press |
| `main/wifi_handler.h` / `wifi_handler.c` | WiFi connect/reconnect, NVS credential storage |
| `main/ai_client.h` / `ai_client.c` | DeepSeek API HTTP client, JSON build/parse |
| `main/sprites.h` / `sprites.c` | Pixel art sprite data (bitmap arrays) |
| `main/ui.h` / `ui.c` | UI layout, stat bars, speech bubble, animation |
| `main/font.h` / `font.c` | 6x8 or 8x8 bitmap font for text rendering |

---

## Verification

1. **Display test**: Flash Phase 1, verify ST7789 shows test pattern (colored rectangles, text "Hello")
2. **Pet state test**: Via serial monitor, verify stats decay every 10s, feeding/playing adjusts stats, mood changes at thresholds
3. **Button test**: Verify short press triggers correct action, long press triggers menu, debounce works (no double-fires)
4. **WiFi test**: Flash with WiFi credentials in NVS, verify connection, verify auto-reconnect after AP reboot
5. **AI test**: Press Talk button, verify DeepSeek API call succeeds, response displays in speech bubble, fallback message shows when offline
6. **Integration test**: Run full loop — pet animates, stats decay, feed pet, talk to pet, AI responds in character, speech bubble appears and dismisses
7. **Persistence test**: Reboot device, verify pet stats and name restore from NVS
8. **Long-run test**: Let pet run for 1+ hour, verify no memory leaks, stats don't overflow, WiFi stays connected

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
