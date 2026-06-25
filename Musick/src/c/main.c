#include <pebble.h>

// AppMessage keys. These are explicit integers (not auto-assigned messageKeys)
// so the Android companion can use the exact same key IDs — keep both sides in
// sync. See companion-android/.../PebbleConstants.java.
#define MSG_KEY_TITLE    0
#define MSG_KEY_ARTIST   1
#define MSG_KEY_COMMAND  2
#define MSG_KEY_STATE    3

// Commands sent to the Android companion (KEY_COMMAND values).
typedef enum {
  CMD_PLAY_PAUSE  = 0,
  CMD_NEXT        = 1,
  CMD_PREVIOUS    = 2,
  CMD_VOLUME_UP   = 3,
  CMD_VOLUME_DOWN = 4,
} MusicCommand;

// The button hints sit on the LEFT edge (the built-in Music app draws them on
// the right, against the physical buttons). Their vertical positions track the
// real button centers as a fraction of screen height, so the layout works on
// any platform (built for emery / Pebble Time 2).
#define HINT_COL_W   34
#define ICON_HALF_H   9   // half-height of the icon glyphs

// ==================== State ====================

static Window    *s_window;

static Layer     *s_hint_layer;
static GPath     *s_prev_tri;   // left-pointing triangle (skip back)
static GPath     *s_play_tri;   // right-pointing triangle (play)
static GPath     *s_next_tri;   // right-pointing triangle (skip forward)
static bool       s_playing = false;

static TextLayer *s_title_layer;
static char       s_title_buf[64];

static TextLayer *s_artist_layer;
static char       s_artist_buf[64];

static const GPathInfo PREV_TRI_INFO = { 3, (GPoint[]){ {14, -9}, {14, 9}, {0, 0} } };
static const GPathInfo PLAY_TRI_INFO = { 3, (GPoint[]){ {0, -10}, {0, 10}, {17, 0} } };
static const GPathInfo NEXT_TRI_INFO = { 3, (GPoint[]){ {0, -9}, {0, 9}, {14, 0} } };

// ==================== Hint column drawing ====================

static void hint_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int y_prev = bounds.size.h * 22 / 100;
  int y_play = bounds.size.h / 2;
  int y_next = bounds.size.h * 78 / 100;

  graphics_context_set_fill_color(ctx, GColorWhite);

  // PREV (skip back): bar + left-pointing triangle.
  graphics_fill_rect(ctx, GRect(5, y_prev - ICON_HALF_H, 3, ICON_HALF_H * 2), 0, GCornerNone);
  gpath_move_to(s_prev_tri, GPoint(10, y_prev));
  gpath_draw_filled(ctx, s_prev_tri);

  // PLAY / PAUSE: two bars when playing, a triangle when paused.
  if (s_playing) {
    graphics_fill_rect(ctx, GRect(8, y_play - 10, 5, 20), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(18, y_play - 10, 5, 20), 0, GCornerNone);
  } else {
    gpath_move_to(s_play_tri, GPoint(9, y_play));
    gpath_draw_filled(ctx, s_play_tri);
  }

  // NEXT (skip forward): right-pointing triangle + bar.
  gpath_move_to(s_next_tri, GPoint(8, y_next));
  gpath_draw_filled(ctx, s_next_tri);
  graphics_fill_rect(ctx, GRect(24, y_next - ICON_HALF_H, 3, ICON_HALF_H * 2), 0, GCornerNone);

  // Separator between the hint column and the now-playing text.
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, 10),
                          GPoint(bounds.size.w - 1, bounds.size.h - 10));
}

// ==================== AppMessage ====================

static void send_command(MusicCommand cmd) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iter, MSG_KEY_COMMAND, (uint8_t)cmd);
  app_message_outbox_send();
}

static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *title = dict_find(iter, MSG_KEY_TITLE);
  if (title && title->type == TUPLE_CSTRING) {
    strncpy(s_title_buf, title->value->cstring, sizeof(s_title_buf) - 1);
    s_title_buf[sizeof(s_title_buf) - 1] = '\0';
    text_layer_set_text(s_title_layer, s_title_buf);
  }

  Tuple *artist = dict_find(iter, MSG_KEY_ARTIST);
  if (artist && artist->type == TUPLE_CSTRING) {
    strncpy(s_artist_buf, artist->value->cstring, sizeof(s_artist_buf) - 1);
    s_artist_buf[sizeof(s_artist_buf) - 1] = '\0';
    text_layer_set_text(s_artist_layer, s_artist_buf);
  }

  Tuple *state = dict_find(iter, MSG_KEY_STATE);
  if (state && state->type == TUPLE_INT) {
    s_playing = state->value->int32 != 0;
    layer_mark_dirty(s_hint_layer);
  }
}

// ==================== Buttons ====================

static void select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  s_playing = !s_playing;            // optimistic; companion confirms via KEY_STATE
  layer_mark_dirty(s_hint_layer);
  send_command(CMD_PLAY_PAUSE);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_PREVIOUS);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_NEXT);
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_VOLUME_UP);
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  send_command(CMD_VOLUME_DOWN);
}

static void click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 0, up_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 0, down_long_click_handler, NULL);
}

// ==================== Window ====================

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_hint_layer = layer_create(GRect(0, 0, HINT_COL_W, bounds.size.h));
  layer_set_update_proc(s_hint_layer, hint_layer_update);
  layer_add_child(root, s_hint_layer);

  int content_x = HINT_COL_W + 4;
  int content_w = bounds.size.w - content_x - 4;

  int title_h = 88;
  int title_y = (bounds.size.h - title_h) / 2 - 12;
  s_title_layer = text_layer_create(GRect(content_x, title_y, content_w, title_h));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_title_layer, "Musick");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_artist_layer = text_layer_create(GRect(content_x, title_y + title_h, content_w, 32));
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, GColorLightGray);
  text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_artist_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_artist_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_artist_layer, "Not playing");
  layer_add_child(root, text_layer_get_layer(s_artist_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_artist_layer);
  layer_destroy(s_hint_layer);
}

// ==================== Init / deinit ====================

static void init(void) {
  s_title_buf[0] = '\0';
  s_artist_buf[0] = '\0';

  s_prev_tri = gpath_create(&PREV_TRI_INFO);
  s_play_tri = gpath_create(&PLAY_TRI_INFO);
  s_next_tri = gpath_create(&NEXT_TRI_INFO);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  app_message_register_inbox_received(inbox_received);
  app_message_open(256, 64);
}

static void deinit(void) {
  app_message_deregister_callbacks();
  window_destroy(s_window);
  gpath_destroy(s_prev_tri);
  gpath_destroy(s_play_tri);
  gpath_destroy(s_next_tri);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
