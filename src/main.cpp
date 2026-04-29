#include <esp_http_client.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <htcw_json.h>

#include "custom_panel.h"
#include "gfx.hpp"
#include "panel.h"
#include "uix.hpp"

#define TELEGRAMA_IMPLEMENTATION
#include "telegrama.hpp"
#undef TELEGRAMA_IMPLEMENTATION

using namespace gfx;
using namespace uix;
using namespace json;

static uix::display lcd;

// snprintf format strings
static const char* time_format_string = " %02d:%02d";
static const char* ready_format_string = "Ready";
static const char* done_format_string = "Done";

// to hold strings formatted for display
static char time_text[15] = {0};

void panel_lcd_flush_complete(void) {
  // let UIX know the DMA transfer completed
  lcd.flush_complete();
}
static void uix_flush(const rect16& bounds, const void* bitmap, void* state) {
  // similar to LVGL
  panel_lcd_flush(bounds.x1, bounds.y1, bounds.x2, bounds.y2, (void*)bitmap);
}

// type to represent our display
using screen_t = uix::screen<gsc_pixel<LCD_BIT_DEPTH>>;
// native screen color
using scr_color_t = color<screen_t::pixel_type>;
// UIX color (rgba32)
using uix_color_t = color<uix_pixel>;

// fonts and screens
static tt_font clock_text_font(telegrama, 20, font_size_units::px);

static screen_t main_screen;

// control types
using label_t = uix::label<screen_t::control_surface_type>;
using icon_t = uix::image_box<screen_t::control_surface_type>;
using vlabel_t = uix::vlabel<screen_t::control_surface_type>;
using rect_t = uix::painter<screen_t::control_surface_type>;

label_t time_label;

void draw_screen() {
  snprintf(time_text, sizeof(time_text), time_format_string, 6, 42);

  time_label.text(time_text);

  while (lcd.dirty()) {
    lcd.update();
  }
}

void app_loop(void* params) {
  while (true) {
    uint32_t started = pdTICKS_TO_MS(xTaskGetTickCount());

    draw_screen();

    uint16_t elapsed = pdTICKS_TO_MS(xTaskGetTickCount()) - started;

    vTaskDelay(pdMS_TO_TICKS(1000 - elapsed));
  }
}

extern "C" void app_main() {
  panel_lcd_init();

  lcd.buffer_size(LCD_TRANSFER_SIZE);
  lcd.buffer1((uint8_t*)panel_lcd_transfer_buffer());
  lcd.buffer2((uint8_t*)panel_lcd_transfer_buffer2());
  lcd.on_flush_callback(uix_flush);

  clock_text_font.initialize();

  main_screen.dimensions({LCD_WIDTH, LCD_HEIGHT});
  main_screen.background_color(scr_color_t::black);

  time_label.bounds(srect16(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1));
  time_label.font(clock_text_font);
  time_label.color(uix_color_t::purple);
  time_label.text_justify(uix_justify::center);
  main_screen.register_control(time_label);

  lcd.active_screen(main_screen);

  xTaskCreatePinnedToCore(app_loop, "app", 8192, nullptr, 10, nullptr, 0);
}
