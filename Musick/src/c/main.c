#include <pebble.h>

extern uint32_t MESSAGE_KEY_KEY_TITLE;
extern uint32_t MESSAGE_KEY_KEY_ARTIST;
extern uint32_t MESSAGE_KEY_KEY_COMMAND;

// Commands sent to the phone (KEY_COMMAND values).
typedef enum {
  CMD_PLAY_PAUSE = 0,
  CMD_NEXT       = 1,
  CMD_PREVIOUS   = 2,
} MusicCommand;

// ==================== State ====================

static Window    *s_window;

static TextLayer *s_title_layer;
static char       s_title_buf[64];

static TextLayer *s_artist_layer;
static char       s_artist_buf[64];

static TextLayer *s_hint_layer;

// ==================== AppMessage ====================

static void send_command(MusicCommand cmd) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_KEY_COMMAND, (uint8_t)cmd);
  app_message_outbox_send();
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *title = dict_find(iter, MESSAGE_KEY_KEY_TITLE);
  if (title && title->type == TUPLE_CSTRING) {
    strncpy(s_title_buf, title->value->cstring, sizeof(s_title_buf) - 1);
    s_title_buf[sizeof(s_title_buf) - 1] = '\0';
    text_layer_set_text(s_title_layer, s_title_buf);
  }

  Tuple *artist = dict_find(iter, MESSAGE_KEY_KEY_ARTIST);
  if (artist && artist->type == TUPLE_CSTRING) {
    strncpy(s_artist_buf, artist->value->cstring, sizeof(s_artist_buf) - 1);
    s_artist_buf[sizeof(s_artist_buf) - 1] = '\0';
    text_layer_set_text(s_artist_layer, s_artist_buf);
  }
}

// ==================== Buttons ====================

static void select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_PLAY_PAUSE);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_PREVIOUS);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_NEXT);
}

static void click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

// ==================== Window ====================

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_title_layer = text_layer_create(GRect(4, 44, bounds.size.w - 8, 60));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_title_layer, "Musick");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_artist_layer = text_layer_create(GRect(4, 104, bounds.size.w - 8, 28));
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, GColorWhite);
  text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_artist_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_artist_layer, "Not playing");
  layer_add_child(root, text_layer_get_layer(s_artist_layer));

  s_hint_layer = text_layer_create(GRect(4, bounds.size.h - 22, bounds.size.w - 8, 20));
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, GColorWhite);
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_text(s_hint_layer, "\xE2\x96\xB2 prev   play/pause   next \xE2\x96\xBC");
  layer_add_child(root, text_layer_get_layer(s_hint_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_artist_layer);
  text_layer_destroy(s_hint_layer);
}

// ==================== Init / deinit ====================

static void init(void) {
  s_title_buf[0] = '\0';
  s_artist_buf[0] = '\0';

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(128, 128);
}

static void deinit(void) {
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
