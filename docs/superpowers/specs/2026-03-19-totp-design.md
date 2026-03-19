# TOTP Feature Design

**Date:** 2026-03-19
**Project:** toptface (Pebble watchface)
**Status:** Approved

## Overview

Add real TOTP (RFC 6238) code generation to the toptface watchface. The watch computes the 6-digit code on-device every 30 seconds. The user enters the base32 secret via a configuration page in the Pebble app.

## Requirements

- One fixed TOTP account
- Secret entered via Pebble config page (web form), stored on watch
- TOTP computed on-watch (offline-capable)
- Display refreshes at every 30-second boundary
- Format: `XXX-XXX` (6 digits, hyphen in center)

## Architecture

### New files

- `src/pkjs/index.js` — PebbleKit JS: embeds config HTML as a data URI, opens it via `Pebble.openURL()`, receives secret via `webviewclosed` event, sends to watch via AppMessage key `KEY_SECRET`

### Modified files

- `src/c/main.c` — extended with:
  - `base32_decode()` — decodes base32 secret to raw bytes (max 64 chars input → 40 bytes output)
  - `hmac_sha1()` — HMAC-SHA1 (~120 lines pure C, SHA-1 block function + outer/inner padding)
  - `totp_generate()` — RFC 6238 computation
  - AppMessage inbox handler — receives secret, validates, stores with `persist_write_string(KEY_STORAGE, secret)`, recomputes and displays immediately
  - `tick_handler` — recomputes TOTP every second, updates display only when code changes (avoids flicker)
  - Startup: `persist_read_string` → compute initial code; display `"------"` if no secret stored
  - `app_message_open(128, 8)` called in `init()` — 128 bytes inbox (covers 64-char secret + overhead), 8 bytes outbox

- `package.json` — add `messageKeys` entry and `pkjs` enablement:
  ```json
  "messageKeys": { "KEY_SECRET": 0 },
  "pkjs": { "file": "src/pkjs/index.js" }
  ```

### Tick timer

Changed from `MINUTE_UNIT` to `SECOND_UNIT` to detect 30-second boundaries promptly. Watchface is always-on so the battery tradeoff is acceptable.

## Config Page Delivery

The HTML form is embedded as a JavaScript string in `index.js` and opened as a `data:text/html,...` URI via `Pebble.openURL()`. No external hosting required.

```js
var html = '<html>...form...</html>';
Pebble.openURL('data:text/html,' + encodeURIComponent(html));
```

Keep HTML under ~2 KB (Android WebView URI length limit).

The form:
- One text input for base32 secret
- Client-side validation: strip whitespace, uppercase, reject non-`[A-Z2-7=]` characters with an inline error message
- On Save: `window.location.href = 'pebblejs://close#' + encodeURIComponent(secret)`

In `index.js`, `webviewclosed` handler:
```js
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === '') return;
  var secret = decodeURIComponent(e.response);
  if (!secret) return;
  Pebble.sendAppMessage({ KEY_SECRET: secret });
});
```
Note: `e.response` contains the raw encoded fragment (not decoded by Pebble runtime), so `decodeURIComponent` is required.

## Data Flow

```
User: Pebble app → watch settings → data: URI config page
form: Save → pebblejs://close#<base32secret>
pkjs: webviewclosed → sendAppMessage({ KEY_SECRET: secret })
watch C: inbox_received → persist_write_string(0, secret) → recompute immediately
watch C: every second tick → totp_generate(secret, time(NULL)/30) → update display if changed
watch C: on startup → persist_read_string(0) → compute initial code
```

## TOTP Algorithm (RFC 6238 / RFC 4226)

`time(NULL)` on Pebble returns UTC Unix epoch seconds — correct for TOTP (RFC 6238 §4 requires UTC).

```
counter = (uint64_t)time(NULL) / 30   // 8 bytes, big-endian
hmac    = HMAC-SHA1(base32_decode(secret), counter)
offset  = hmac[19] & 0x0f
code    = (hmac[offset..offset+3] & 0x7fffffff) % 1_000_000
display = sprintf("%03d-%03d", code/1000, code%1000)
```

## Buffer Sizes

| Purpose | Type | Size |
|---------|------|------|
| base32 secret input (max) | `char[]` | 65 bytes (64 chars + `\0`) |
| decoded key buffer | `uint8_t[]` | 40 bytes |
| `persist_write_string` key (`KEY_STORAGE`) | `uint32_t` | `1` (distinct from `KEY_SECRET=0`) |
| AppMessage inbox | bytes | 128 |
| AppMessage outbox | bytes | 8 |

## Storage

- `persist_write_string(KEY_STORAGE, secret)` where `#define KEY_STORAGE 1` — survives watch restart
- `persist_read_string(KEY_STORAGE, buf, 65)` — read on startup
- If no secret: display `"------"`

## Error Handling

- Invalid/empty secret from AppMessage: ignore, keep existing stored secret
- `base32_decode` failure (invalid chars): return 0 length → display `"------"`
- AppMessage delivery failure: user can re-open settings and re-save

## Out of Scope

- Multiple TOTP accounts
- TOTP period other than 30s
- SHA256/SHA512 TOTP variants
- Remaining-time progress indicator
