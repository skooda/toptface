#include <pebble.h>

extern uint32_t MESSAGE_KEY_KEY_TITLE;
extern uint32_t MESSAGE_KEY_KEY_ARTIST;
extern uint32_t MESSAGE_KEY_KEY_COMMAND;
extern uint32_t MESSAGE_KEY_KEY_STATE;

// Commands sent to the phone (KEY_COMMAND values).
typedef enum {
  CMD_PLAY_PAUSE  = 0,
  CMD_NEXT        = 1,
  CMD_PREVIOUS    = 2,
  CMD_VOLUME_UP   = 3,
  CMD_VOLUME_DOWN = 4,
} MusicCommand;

// Vertical centers of the three physical buttons, used to line the on-screen
// button hints up with them. Hints sit on the LEFT edge (the built-in Music
// app draws them on the right, against the buttons).
#define HINT_COL_W   30
#define Y_PREV       34
#define Y_PLAY       84
#define Y_NEXT      134

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

static const GPathInfo PREV_TRI_INFO = { 3, (GPoint[]){ {12, -7}, {12, 7}, {0, 0} } };
static const GPathInfo PLAY_TRI_INFO = { 3, (GPoint[]){ {0, -8}, {0, 8}, {14, 0} } };
static const GPathInfo NEXT_TRI_INFO = { 3, (GPoint[]){ {0, -7}, {0, 7}, {12, 0} } };

// ==================== Hint column drawing ====================

static void hint_layer_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorWhite);

  // PREV (skip back): bar + left-pointing triangle.
  graphics_fill_rect(ctx, GRect(5, Y_PREV - 7, 3, 14), 0, GCornerNone);
  gpath_move_to(s_prev_tri, GPoint(9, Y_PREV));
  gpath_draw_filled(ctx, s_prev_tri);

  // PLAY / PAUSE: two bars when playing, a triangle when paused.
  if (s_playing) {
    graphics_fill_rect(ctx, GRect(7, Y_PLAY - 8, 4, 16), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(15, Y_PLAY - 8, 4, 16), 0, GCornerNone);
  } else {
    gpath_move_to(s_play_tri, GPoint(8, Y_PLAY));
    gpath_draw_filled(ctx, s_play_tri);
  }

  // NEXT (skip forward): right-pointing triangle + bar.
  gpath_move_to(s_next_tri, GPoint(7, Y_NEXT));
  gpath_draw_filled(ctx, s_next_tri);
  graphics_fill_rect(ctx, GRect(20, Y_NEXT - 7, 3, 14), 0, GCornerNone);

  // Separator between the hint column and the now-playing text.
  GRect bounds = layer_get_bounds(layer);
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, 8),
                          GPoint(bounds.size.w - 1, bounds.size.h - 8));
}

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

  Tuple *state = dict_find(iter, MESSAGE_KEY_KEY_STATE);
  if (state && state->type == TUPLE_INT) {
    s_playing = state->value->int32 != 0;
    layer_mark_dirty(s_hint_layer);
  }
}

// ==================== Buttons ====================

static void select_click_handler(ClickRecognizerRef recognizer, void *ctx) {
  s_playing = !s_playing;            // optimistic; phone confirms via KEY_STATE
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

  int content_x = HINT_COL_W + 2;
  int content_w = bounds.size.w - content_x - 2;

  s_title_layer = text_layer_create(GRect(content_x, 50, content_w, 66));
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_text_color(s_title_layer, GColorWhite);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_title_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_title_layer, "Musick");
  layer_add_child(root, text_layer_get_layer(s_title_layer));

  s_artist_layer = text_layer_create(GRect(content_x, 116, content_w, 30));
  text_layer_set_background_color(s_artist_layer, GColorClear);
  text_layer_set_text_color(s_artist_layer, GColorWhite);
  text_layer_set_font(s_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
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
  app_message_open(128, 128);
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
