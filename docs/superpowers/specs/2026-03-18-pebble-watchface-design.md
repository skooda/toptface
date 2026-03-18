# Pebble Watchface (toptface) — Design Spec
_Date: 2026-03-18_

## Overview

A retro/pixel-style digital watchface for Pebble (diorite platform — Pebble 2) built with Pebble SDK 4.9.127. Displays time, date, a TOTP code placeholder (static `"123-456"` in v1), and battery level using a custom 8-bit pixel font.

**TOTP scope note:** TOTP generation (HMAC-SHA1, shared secret storage, 30s timer) is explicitly **out of scope for v1**. The TOTP layer ships as a static placeholder string. The architecture is designed so that swapping in a real generator requires only replacing the string passed to `text_layer_set_text()`.

## Target Platform

- **Device:** Pebble Core 2 Duo (diorite)
- **SDK:** 4.9.127
- **Display:** 144×168 px, 3-color e-paper (black, white, red)
- **Color usage:** v1 uses black/white only; red channel is reserved for future use (e.g. low battery warning)

## Visual Layout

```
┌─────────────────┐
│                 │
│    12:34        │  ← time (large pixel font, white text on black bg)
│                 │
│    18.03.2026   │  ← date (small pixel font)
│                 │
│  [ 123-456 ]    │  ← TOTP placeholder (monospace pixel font)
│                 │
│  [██░░░] 60%    │  ← battery (pixel art, bottom-right corner)
└─────────────────┘
```

**Window:** black background, white text throughout.

## Architecture

### File Structure

```
toptface/
├── src/
│   └── c/
│       └── main.c
├── resources/
│   └── fonts/
│       └── pixel.ttf        # Custom 8-bit pixel font (digits, colon, dash, %, space only)
└── package.json             # Pebble manifest
```

### Font Resources

Three sizes declared in `package.json` under `resources.media`:

```json
{ "type": "font", "name": "FONT_PIXEL_TIME_42",  "file": "fonts/pixel.ttf", "characterRegex": "[0-9: ]" },
{ "type": "font", "name": "FONT_PIXEL_DATE_18",  "file": "fonts/pixel.ttf", "characterRegex": "[0-9.%]" },
{ "type": "font", "name": "FONT_PIXEL_TOTP_24",  "file": "fonts/pixel.ttf", "characterRegex": "[0-9\\- ]" }
```

Character sets are restricted to digits and necessary symbols to minimize resource size and heap usage.

### Layers

| Layer | Type | Content | Font resource |
|-------|------|---------|---------------|
| Time | `TextLayer` | `HH:MM` | `FONT_PIXEL_TIME_42` |
| Date | `TextLayer` | `DD.MM.YYYY` | `FONT_PIXEL_DATE_18` |
| TOTP | `TextLayer` | `"123-456"` (hardcoded) | `FONT_PIXEL_TOTP_24` |
| Battery | `Layer` with `update_proc` | Pixel bar + `"XX%"` text | Drawn programmatically |

### Battery Layer Specification

- **Position:** bottom-right, `GRect(56, 148, 88, 18)`
- **Bar:** 5 segments, each 8×10 px, 2 px gap = 48 px total bar width; filled segments = `round(charge_pct / 20)`
- **Colors:** filled = `GColorWhite`, empty = `GColorDarkGray`
- **Percentage text:** rendered 4 px right of bar using `FONT_PIXEL_DATE_18` (18 px); format `"%d%%"`; ~36 px for 3 chars; total layout: 48 + 4 + 36 = 88 px fits layer width exactly

### Data Flow

1. **`tick_timer_service`** — fires every `MINUTE_UNIT` → updates time `TextLayer`; checks if day changed before updating date `TextLayer` (avoids redundant `text_layer_set_text()`)
2. **`battery_state_service`** — fires on change → calls `layer_mark_dirty()` on battery `Layer`
3. **TOTP** — static string `"123-456"` set once in `window_load`; swap point for future generator

### Date Formatting

Date is formatted manually using `struct tm` fields to guarantee `DD.MM.YYYY` output regardless of locale:

```c
snprintf(date_buf, sizeof(date_buf), "%02d.%02d.%04d",
         t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
```

### Memory Budget

Approximate heap usage:
- 3 custom font sizes: ~15–20 KB (restricted character sets keep this low)
- 4 layers + window: ~2 KB
- String buffers: < 1 KB
- **Total estimated: ~22 KB** — well within diorite's 64 KB app heap

### Error Handling

For v1, resource loading failures (NULL return from `fonts_load_custom_font()`, `text_layer_create()`) are not individually guarded — a crash-on-failure policy is acceptable during development. Production hardening (null-checks with fallback to system font) is a post-v1 task.
