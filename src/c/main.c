#include <pebble.h>

static Window *s_window;
static TextLayer *s_time_layer;
static GFont s_font_time;
static char s_time_buf[6]; // "HH:MM\0"

static TextLayer *s_date_layer;
static GFont s_font_date;
static char s_date_buf[11]; // "DD.MM.YYYY\0"

static TextLayer *s_totp_layer;
static GFont s_font_totp;

static Layer *s_battery_layer;
static int s_battery_level = 0;
static char s_batt_buf[5]; // "100%\0"

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

static void battery_update_proc(Layer *layer, GContext *ctx) {
  // Number of filled segments (0-5): round(charge / 20)
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

static void battery_handler(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
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

  s_font_totp = fonts_load_custom_font(
    resource_get_handle(RESOURCE_ID_FONT_PIXEL_TOTP_24));

  s_totp_layer = text_layer_create(GRect(0, 118, 144, 30));
  text_layer_set_background_color(s_totp_layer, GColorClear);
  text_layer_set_text_color(s_totp_layer, GColorWhite);
  text_layer_set_font(s_totp_layer, s_font_totp);
  text_layer_set_text_alignment(s_totp_layer, GTextAlignmentCenter);
  text_layer_set_text(s_totp_layer, "123-456"); // TOTP placeholder — swap this string for real generator
  layer_add_child(root, text_layer_get_layer(s_totp_layer));

  s_battery_layer = layer_create(GRect(56, 148, 88, 18));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(root, s_battery_layer);
}

static void window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_font_time);
  text_layer_destroy(s_date_layer);
  fonts_unload_custom_font(s_font_date);
  text_layer_destroy(s_totp_layer);
  fonts_unload_custom_font(s_font_totp);
  layer_destroy(s_battery_layer);
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

  battery_state_service_subscribe(battery_handler);
  battery_handler(battery_state_service_peek());
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
