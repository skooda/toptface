# toptface Watchface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a retro pixel-style digital watchface for Pebble 2 (diorite) that displays time, date, a TOTP placeholder, and battery level using a custom 8-bit pixel font.

**Architecture:** Single `main.c` file with four layers: three `TextLayer`s (time/date/TOTP) using a custom pixel font, and one procedurally drawn `Layer` for the battery. Services `tick_timer_service` and `battery_state_service` drive all updates.

**Tech Stack:** C (Pebble SDK 4.9.127), diorite platform, Press Start 2P TTF font, `pebble build` + `pebble install --emulator diorite` for verification.

---

## File Map

| File | Responsibility |
|------|---------------|
| `package.json` | Pebble manifest: UUID, platform target (diorite), watchface flag, font resource declarations |
| `src/c/main.c` | All watchface logic: window, layers, service subscriptions, callbacks |
| `resources/fonts/pixel.ttf` | Press Start 2P TTF — the only font file; referenced at 3 sizes via characterRegex entries |

---

### Task 1: Scaffold project — package.json and directory structure

**Files:**
- Create: `package.json`
- Create: `src/c/main.c` (skeleton only)
- Create directory: `resources/fonts/`

- [ ] **Step 1: Create directory structure**

```bash
mkdir -p src/c resources/fonts
```

- [ ] **Step 2: Create package.json**

```json
{
  "name": "toptface",
  "author": "Pavel Skoda",
  "version": "1.0.0",
  "keywords": ["pebble-app"],
  "projectType": "native",
  "sdk": {
    "version": "^4.9"
  },
  "pebble": {
    "displayName": "toptface",
    "uuid": "cd0fb816-9172-488a-8dd8-4969ed7f1e7f",
    "sdkVersion": "3",
    "targetPlatforms": ["diorite"],
    "watchapp": {
      "watchface": true
    },
    "resources": {
      "media": []
    }
  },
  "dependencies": {},
  "devDependencies": {}
}
```

- [ ] **Step 3: Create minimal skeleton src/c/main.c**

```c
#include <pebble.h>

static Window *s_window;

static void window_load(Window *window) {}
static void window_unload(Window *window) {}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
```

- [ ] **Step 4: Verify skeleton builds**

```bash
pebble build
```

Expected: `BUILD SUCCESSFUL` — the `.pbw` file appears in `build/`.

- [ ] **Step 5: Commit**

```bash
git add package.json src/ resources/
git commit -m "chore: scaffold project structure"
```

---

### Task 2: Acquire pixel font

**Files:**
- Create: `resources/fonts/pixel.ttf`

- [ ] **Step 1: Download Press Start 2P font from Google Fonts**

Visit https://fonts.google.com/specimen/Press+Start+2P, click "Download family", and extract the zip. Copy the TTF:

```bash
cp ~/Downloads/Press_Start_2P/PressStart2P-Regular.ttf resources/fonts/pixel.ttf
```

- [ ] **Step 2: Verify file exists and is a valid TTF**

```bash
ls -lh resources/fonts/pixel.ttf
file resources/fonts/pixel.ttf
```

Expected: `TrueType font data` in file output.

- [ ] **Step 3: Commit**

```bash
git add resources/fonts/pixel.ttf
git commit -m "chore: add Press Start 2P pixel font"
```

---

### Task 3: Declare font resources in package.json

**Files:**
- Modify: `package.json` (resources.media array)

- [ ] **Step 1: Replace the empty `media` array in package.json**

Update the `"resources"` section in `package.json`:

```json
"resources": {
  "media": [
    {
      "type": "font",
      "name": "FONT_PIXEL_TIME_42",
      "file": "fonts/pixel.ttf",
      "characterRegex": "[0-9: ]"
    },
    {
      "type": "font",
      "name": "FONT_PIXEL_DATE_18",
      "file": "fonts/pixel.ttf",
      "characterRegex": "[0-9.%]"
    },
    {
      "type": "font",
      "name": "FONT_PIXEL_TOTP_24",
      "file": "fonts/pixel.ttf",
      "characterRegex": "[0-9\\- ]"
    }
  ]
}
```

Each entry generates a `RESOURCE_ID_FONT_PIXEL_*` constant usable with `resource_get_handle()` in C.

- [ ] **Step 2: Verify build with font resources**

```bash
pebble build
```

Expected: `BUILD SUCCESSFUL`. The build output will include compiled font resources.

- [ ] **Step 3: Commit**

```bash
git add package.json
git commit -m "chore: declare pixel font resources"
```

---

### Task 4: Implement time layer

**Files:**
- Modify: `src/c/main.c`

Layout: `GRect(0, 30, 144, 50)` — centered, upper third of the 144×168 display.

- [ ] **Step 1: Replace the skeleton globals and add tick handler in main.c**

**Replace** everything between `#include <pebble.h>` and `static void window_load` with:

```c
static Window *s_window;
static TextLayer *s_time_layer;
static GFont s_font_time;
static char s_time_buf[6]; // "HH:MM\0"

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buf);
}
```

- [ ] **Step 2: Update window_load to create the time layer**

```c
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  s_font_time = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_TIME_42));

  s_time_layer = text_layer_create(GRect(0, 30, 144, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, s_font_time);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));
}
```

- [ ] **Step 3: Update window_unload**

```c
static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_font_time);
}
```

- [ ] **Step 4: Subscribe to tick service and force initial update in init()**

```c
static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Force initial update so time shows immediately on load
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, MINUTE_UNIT);
}
```

- [ ] **Step 5: Unsubscribe in deinit()**

```c
static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}
```

- [ ] **Step 6: Build and run on emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: Emulator opens showing current time in pixel font on black background.

- [ ] **Step 7: Commit**

```bash
git add src/c/main.c
git commit -m "feat: add time layer with pixel font"
```

---

### Task 5: Add date layer

**Files:**
- Modify: `src/c/main.c`

Layout: `GRect(0, 88, 144, 24)` — below time, using `FONT_PIXEL_DATE_18`.

- [ ] **Step 1: Add date globals and update tick_handler**

Add globals:

```c
static TextLayer *s_date_layer;
static GFont s_font_date;
static char s_date_buf[11]; // "DD.MM.YYYY\0"
```

Update `tick_handler` to also update the date (only when the day changes):

```c
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buf);

  static int last_mday = -1;
  if (tick_time->tm_mday != last_mday) {
    last_mday = tick_time->tm_mday;
    snprintf(s_date_buf, sizeof(s_date_buf), "%02d.%02d.%04d",
             tick_time->tm_mday,
             tick_time->tm_mon + 1,
             tick_time->tm_year + 1900);
    text_layer_set_text(s_date_layer, s_date_buf);
  }
}
```

- [ ] **Step 2: Create date layer in window_load (after time layer creation)**

```c
  s_font_date = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_DATE_18));

  s_date_layer = text_layer_create(GRect(0, 88, 144, 24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, s_font_date);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));
```

- [ ] **Step 3: Destroy date layer in window_unload**

```c
  text_layer_destroy(s_date_layer);
  fonts_unload_custom_font(s_font_date);
```

- [ ] **Step 4: Build and verify on emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: Date `DD.MM.YYYY` appears below the time in smaller pixel font.

- [ ] **Step 5: Commit**

```bash
git add src/c/main.c
git commit -m "feat: add date layer"
```

---

### Task 6: Add TOTP placeholder layer

**Files:**
- Modify: `src/c/main.c`

Layout: `GRect(0, 118, 144, 30)` — below date, using `FONT_PIXEL_TOTP_24`. Hardcoded `"123-456"`.

- [ ] **Step 1: Add TOTP globals**

```c
static TextLayer *s_totp_layer;
static GFont s_font_totp;
```

- [ ] **Step 2: Create TOTP layer in window_load (after date layer creation)**

```c
  s_font_totp = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_TOTP_24));

  s_totp_layer = text_layer_create(GRect(0, 118, 144, 30));
  text_layer_set_background_color(s_totp_layer, GColorClear);
  text_layer_set_text_color(s_totp_layer, GColorWhite);
  text_layer_set_font(s_totp_layer, s_font_totp);
  text_layer_set_text_alignment(s_totp_layer, GTextAlignmentCenter);
  text_layer_set_text(s_totp_layer, "123-456"); // TOTP placeholder — swap this string for real generator
  layer_add_child(root, text_layer_get_layer(s_totp_layer));
```

- [ ] **Step 3: Destroy TOTP layer in window_unload**

```c
  text_layer_destroy(s_totp_layer);
  fonts_unload_custom_font(s_font_totp);
```

- [ ] **Step 4: Build and verify on emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: `123-456` appears between date and battery area in medium pixel font.

- [ ] **Step 5: Commit**

```bash
git add src/c/main.c
git commit -m "feat: add TOTP placeholder layer"
```

---

### Task 7: Add battery layer

**Files:**
- Modify: `src/c/main.c`

Layout: `GRect(56, 148, 88, 18)`. Procedurally drawn: 5 segment bar (filled = white, empty = GColorDarkGray) + percentage text using `s_font_date`.

**Note:** `battery_update_proc` uses the global `s_font_date` which is loaded in `window_load` before the battery layer is created — correct order is guaranteed by the existing code structure.

- [ ] **Step 1: Add battery globals**

```c
static Layer *s_battery_layer;
static int s_battery_level = 0;
static char s_batt_buf[5]; // "100%\0"
```

- [ ] **Step 2: Add battery_update_proc before window_load**

```c
static void battery_update_proc(Layer *layer, GContext *ctx) {
  // Number of filled segments (0–5): round(charge / 20)
  int filled = (s_battery_level + 10) / 20;

  for (int i = 0; i < 5; i++) {
    GRect seg = GRect(i * 10, 4, 8, 10);
    if (i < filled) {
      graphics_context_set_fill_color(ctx, GColorWhite);
    } else {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
    }
    graphics_fill_rect(ctx, seg, 0, GCornerNone);
  }

  // Percentage text: starts 4px after bar (bar ends at x=48)
  snprintf(s_batt_buf, sizeof(s_batt_buf), "%d%%", s_battery_level);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_batt_buf, s_font_date,
                     GRect(52, 0, 36, 18),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
}
```

- [ ] **Step 3: Add battery_handler before init()**

```c
static void battery_handler(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}
```

- [ ] **Step 4: Create battery layer in window_load (last, after all TextLayers)**

```c
  s_battery_layer = layer_create(GRect(56, 148, 88, 18));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(root, s_battery_layer);
```

- [ ] **Step 5: Destroy battery layer in window_unload**

```c
  layer_destroy(s_battery_layer);
```

- [ ] **Step 6: Subscribe to battery service and force initial draw in init()**

In `init()`, after `window_stack_push`:

```c
  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());
```

- [ ] **Step 7: Unsubscribe in deinit()**

```c
  battery_state_service_unsubscribe();
```

- [ ] **Step 8: Build and verify on emulator**

```bash
pebble build && pebble install --emulator diorite
```

Expected: Full watchface visible — time, date, `123-456`, battery bar with percentage in bottom-right area.

- [ ] **Step 9: Take emulator screenshot to confirm layout**

```bash
pebble screenshot --emulator diorite watchface.png
open watchface.png
```

- [ ] **Step 10: Commit**

```bash
git add src/c/main.c
git commit -m "feat: add battery layer with pixel art bar"
```

---

### Task 8: Final assembly verification

**Files:**
- Read: `src/c/main.c` (review complete file for correctness)

- [ ] **Step 1: Verify complete main.c structure**

The final `src/c/main.c` should have this structure (top to bottom):

```
#include <pebble.h>

// Globals: s_window, layers, fonts, buffers, s_battery_level, s_batt_buf

static void battery_update_proc(Layer *, GContext *)
static void tick_handler(struct tm *, TimeUnits)
static void battery_handler(BatteryChargeState)
static void window_load(Window *)    // load fonts, create all layers
static void window_unload(Window *)  // destroy all layers, unload fonts
static void init(void)               // create window, push, subscribe services, force initial update
static void deinit(void)             // unsubscribe, destroy window
int main(void)                       // init / app_event_loop / deinit
```

- [ ] **Step 2: Clean build**

```bash
pebble build 2>&1 | tail -5
```

Expected last line: `'build/toptface.pbw' <size> bytes`

- [ ] **Step 3: Install and visually verify all four elements on emulator**

```bash
pebble install --emulator diorite
```

Checklist:
- [ ] Black background
- [ ] Time in large pixel font, centered
- [ ] Date in `DD.MM.YYYY` format, centered
- [ ] `123-456` TOTP placeholder, centered
- [ ] Battery bar (5 segments) + percentage, bottom-right

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete toptface v1 watchface"
```
