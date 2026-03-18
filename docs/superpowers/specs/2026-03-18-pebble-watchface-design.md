# Pebble Watchface — Design Spec
_Date: 2026-03-18_

## Overview

A retro/pixel-style digital watchface for Pebble (diorite platform — Pebble 2) built with Pebble SDK 4.9.127. Displays time, date, TOTP code placeholder, and battery level using a custom 8-bit pixel font.

## Target Platform

- **Device:** Pebble Core 2 Duo (diorite)
- **SDK:** 4.9.127
- **Display:** 144×168 px, black & white

## Visual Layout

```
┌─────────────────┐
│                 │
│    12:34        │  ← time (large pixel font)
│                 │
│    18.03.2026   │  ← date (small pixel font)
│                 │
│  [ 123-456 ]    │  ← TOTP placeholder (monospace pixel font)
│                 │
│  [██░░░] 60%    │  ← battery (pixel art, bottom right)
└─────────────────┘
```

## Architecture

### File Structure

```
toptface/
├── src/
│   └── c/
│       └── main.c
├── resources/
│   └── fonts/
│       └── pixel.ttf        # Custom 8-bit pixel font
└── package.json             # Pebble manifest
```

### Layers

| Layer | Type | Content | Font |
|-------|------|---------|------|
| Time | `TextLayer` | `HH:MM` | Custom pixel, large (~42px) |
| Date | `TextLayer` | `DD.MM.YYYY` | Custom pixel, small (~18px) |
| TOTP | `TextLayer` | `"123-456"` (hardcoded) | Custom pixel monospace (~24px) |
| Battery | `Layer` (custom `update_proc`) | Icon + percentage | Drawn programmatically |

### Data Flow

1. **`tick_timer_service`** — fires every minute → updates time `TextLayer` and date `TextLayer`
2. **`battery_state_service`** — fires on change → redraws battery `Layer`
3. **TOTP** — static string `"123-456"` set at init; ready for future dynamic replacement via `text_layer_set_text()`

## Key Design Decisions

- **Custom pixel font** — loaded via `fonts_load_custom_font()` from resources; gives authentic retro look
- **Battery drawn procedurally** — `update_proc` draws a pixel-art battery bar + percentage text; no bitmap assets needed
- **TOTP as TextLayer** — simple swap point for future TOTP generator integration; no architectural change required
- **Minimal layers** — 4 layers total keeps memory footprint small on constrained Pebble hardware

## Future Extension Points

- **TOTP generation** — replace hardcoded `"123-456"` with computed TOTP in `text_layer_set_text()`; timer callback every 30s
- **Health data** — add step count / HR layer using `health_service`
- **Settings** — add `app_message` handler for Pebble mobile config page
