#include <pebble.h>

extern uint32_t MESSAGE_KEY_KEY_SECRET;

// ==================== SHA-1 ====================

static uint32_t rot32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1_block(uint32_t s[5], const uint8_t b[64]) {
  uint32_t w[80];
  for (int i = 0; i < 16; i++)
    w[i] = ((uint32_t)b[i*4] << 24) | ((uint32_t)b[i*4+1] << 16) |
            ((uint32_t)b[i*4+2] << 8) | b[i*4+3];
  for (int i = 16; i < 80; i++)
    w[i] = rot32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
  uint32_t a = s[0], b2 = s[1], c = s[2], d = s[3], e = s[4];
  for (int i = 0; i < 80; i++) {
    uint32_t f, k;
    if      (i < 20) { f = (b2 & c) | (~b2 & d); k = 0x5A827999u; }
    else if (i < 40) { f = b2 ^ c ^ d;            k = 0x6ED9EBA1u; }
    else if (i < 60) { f = (b2 & c) | (b2 & d) | (c & d); k = 0x8F1BBCDCu; }
    else             { f = b2 ^ c ^ d;            k = 0xCA62C1D6u; }
    uint32_t t = rot32(a, 5) + f + e + k + w[i];
    e = d; d = c; c = rot32(b2, 30); b2 = a; a = t;
  }
  s[0] += a; s[1] += b2; s[2] += c; s[3] += d; s[4] += e;
}

static void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
  uint32_t s[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  uint8_t blk[64];
  size_t i;
  for (i = 0; i + 64 <= len; i += 64)
    sha1_block(s, data + i);
  size_t r = len - i;
  memcpy(blk, data + i, r);
  blk[r] = 0x80;
  memset(blk + r + 1, 0, 63 - r);
  if (r >= 56) { sha1_block(s, blk); memset(blk, 0, 56); }
  uint64_t bits = (uint64_t)len * 8;
  for (int j = 0; j < 8; j++) blk[63 - j] = (uint8_t)(bits >> (j * 8));
  sha1_block(s, blk);
  for (int j = 0; j < 5; j++) {
    out[j*4]   = (uint8_t)(s[j] >> 24);
    out[j*4+1] = (uint8_t)(s[j] >> 16);
    out[j*4+2] = (uint8_t)(s[j] >> 8);
    out[j*4+3] = (uint8_t)(s[j]);
  }
}

// ==================== HMAC-SHA1 (8-byte message for TOTP counter) ====================

static void hmac_sha1(const uint8_t *key, size_t klen, const uint8_t msg[8], uint8_t out[20]) {
  uint8_t k[64];
  memset(k, 0, 64);
  if (klen > 64) sha1(key, klen, k);
  else           memcpy(k, key, klen);

  // inner = SHA1(ipad || msg)
  uint8_t ipad[72];
  for (int i = 0; i < 64; i++) ipad[i] = k[i] ^ 0x36;
  memcpy(ipad + 64, msg, 8);
  uint8_t inner[20];
  sha1(ipad, 72, inner);

  // outer = SHA1(opad || inner)
  uint8_t opad[84];
  for (int i = 0; i < 64; i++) opad[i] = k[i] ^ 0x5C;
  memcpy(opad + 64, inner, 20);
  sha1(opad, 84, out);
}

// ==================== Base32 decode ====================

static int base32_decode(const char *enc, uint8_t *out, int maxlen) {
  int bits = 0, acc = 0, n = 0;
  for (const char *p = enc; *p && *p != '='; p++) {
    int v;
    char c = *p;
    if      (c >= 'A' && c <= 'Z') v = c - 'A';
    else if (c >= 'a' && c <= 'z') v = c - 'a';
    else if (c >= '2' && c <= '7') v = c - '2' + 26;
    else continue;
    acc = (acc << 5) | v;
    bits += 5;
    if (bits >= 8) { bits -= 8; if (n < maxlen) out[n++] = (acc >> bits) & 0xFF; }
  }
  return n;
}

// ==================== TOTP (RFC 6238) ====================

static uint32_t totp_compute(const uint8_t *key, size_t klen, time_t t) {
  uint64_t ctr = (uint64_t)t / 30;
  uint8_t msg[8];
  for (int i = 7; i >= 0; i--) { msg[i] = ctr & 0xFF; ctr >>= 8; }
  uint8_t digest[20];
  hmac_sha1(key, klen, msg, digest);
  int off = digest[19] & 0x0F;
  uint32_t code = ((uint32_t)(digest[off]   & 0x7F) << 24)
                | ((uint32_t) digest[off+1]          << 16)
                | ((uint32_t) digest[off+2]          <<  8)
                |  (uint32_t) digest[off+3];
  return code % 1000000u;
}

// ==================== Watch face state ====================

#define KEY_STORAGE 1  // persist key for secret (distinct from AppMessage KEY_SECRET=0)

static Window      *s_window;

static TextLayer   *s_time_layer;
static GFont        s_font_time;
static char         s_time_buf[6];

static TextLayer   *s_date_layer;
static GFont        s_font_date;
static char         s_date_buf[9];

static TextLayer   *s_totp_layer;
static GFont        s_font_totp;
static char         s_totp_buf[8];   // "XXX-XXX\0"
static uint32_t     s_last_code = UINT32_MAX;

static char         s_secret[65];  // base32 secret, persisted

// ==================== TOTP display ====================

static void update_totp(void) {
  if (s_secret[0] == '\0') {
    text_layer_set_text(s_totp_layer, "------");
    return;
  }
  uint8_t key[40];
  int klen = base32_decode(s_secret, key, 40);
  if (klen == 0) {
    text_layer_set_text(s_totp_layer, "------");
    return;
  }
  uint32_t code = totp_compute(key, (size_t)klen, time(NULL));
  if (code != s_last_code) {
    s_last_code = code;
    snprintf(s_totp_buf, sizeof(s_totp_buf), "%03u-%03u",
             (unsigned)(code / 1000), (unsigned)(code % 1000));
    text_layer_set_text(s_totp_layer, s_totp_buf);
  }
}

// ==================== Tick handler ====================

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", tick_time);
  text_layer_set_text(s_time_layer, s_time_buf);

  static int last_mday = -1;
  if (tick_time->tm_mday != last_mday) {
    last_mday = tick_time->tm_mday;
    snprintf(s_date_buf, sizeof(s_date_buf), "%02d.%02d.%02d",
             tick_time->tm_mday,
             tick_time->tm_mon + 1,
             (tick_time->tm_year + 1900) % 100);
    text_layer_set_text(s_date_layer, s_date_buf);
  }

  update_totp();
}

// ==================== AppMessage ====================

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t = dict_find(iter, MESSAGE_KEY_KEY_SECRET);
  if (t && t->type == TUPLE_CSTRING && t->length > 1) {
    strncpy(s_secret, t->value->cstring, 64);
    s_secret[64] = '\0';
    persist_write_string(KEY_STORAGE, s_secret);
    s_last_code = UINT32_MAX;
    update_totp();
  }
}

// ==================== Window ====================

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  s_font_time = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_TIME_24));

  s_time_layer = text_layer_create(GRect(0, 30, 144, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, s_font_time);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_font_date = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_DATE_8));

  s_date_layer = text_layer_create(GRect(0, 88, 144, 24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, s_font_date);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  s_font_totp = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_TOTP_8));

  s_totp_layer = text_layer_create(GRect(0, 118, 144, 30));
  text_layer_set_background_color(s_totp_layer, GColorClear);
  text_layer_set_text_color(s_totp_layer, GColorWhite);
  text_layer_set_font(s_totp_layer, s_font_totp);
  text_layer_set_text_alignment(s_totp_layer, GTextAlignmentCenter);
  text_layer_set_text(s_totp_layer, "------");
  layer_add_child(root, text_layer_get_layer(s_totp_layer));

}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_font_time);
  text_layer_destroy(s_date_layer);
  fonts_unload_custom_font(s_font_date);
  text_layer_destroy(s_totp_layer);
  fonts_unload_custom_font(s_font_totp);
}

// ==================== Init / deinit ====================

static void init(void) {
  // Load persisted secret
  memset(s_secret, 0, sizeof(s_secret));
  if (persist_exists(KEY_STORAGE))
    persist_read_string(KEY_STORAGE, s_secret, sizeof(s_secret));

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, SECOND_UNIT);

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 8);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
