#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static GFont s_font_time;
static char s_time_buf[6]; // "HH:MM\0"

static TextLayer *s_date_layer;
static GFont s_font_date;
static char s_date_buf[11]; // "DD.MM.YYYY\0"

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

  s_font_date = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_DATE_18));

  s_date_layer = text_layer_create(GRect(0, 88, 144, 24));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, s_font_date);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_font_time);
  text_layer_destroy(s_date_layer);
  fonts_unload_custom_font(s_font_date);
}

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

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
