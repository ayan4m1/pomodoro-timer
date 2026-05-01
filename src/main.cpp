#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "M5GFX.h"
#include "M5PM1.h"
#include "M5Unified.h"

#define POMODORO_INTERVAL_MINS 1

static M5PM1 power;

// snprintf format strings
static const char* time_format_string = " %02d:%02d";

uint32_t last_active = 0;
uint32_t previous_millis = 0;
uint32_t last_blink = 0;
uint8_t seconds = 0;
uint8_t minutes = POMODORO_INTERVAL_MINS;
bool backlight_enabled = true;
bool timer_running = true;
bool timer_elapsed = false;
bool blink_active = false;
bool clear_display = true;

void draw_screen() {
  uint8_t battery_pct = M5.Power.getBatteryLevel();

  if (backlight_enabled || timer_elapsed) {
    M5.Lcd.setBrightness(40);
  } else {
    M5.Lcd.setBrightness(0);
  }

  if (timer_elapsed) {
    M5.Lcd.clearDisplay(TFT_RED);

    uint32_t current_millis = pdTICKS_TO_MS(xTaskGetTickCount());

    if (current_millis - last_blink > 1500) {
      blink_active = !blink_active;
    }

    if (blink_active) {
      M5.Lcd.setTextColor(TFT_WHITE);
      M5.Lcd.setCursor(58, 67 - 14);
      M5.Lcd.print("00:00");
    }
  } else {
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.fillRect(2, 2, 62, 20, TFT_BLACK);
    M5.Lcd.setCursor(2, 2);
    M5.Lcd.printf("%d%%", battery_pct);

    M5.Lcd.setTextSize(4);
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.fillRect(34, 67 - 14, 240 - 32, 135 - 67 - 14, TFT_BLACK);
    M5.Lcd.setCursor(34, 67 - 14);
    M5.Lcd.printf(time_format_string, minutes, seconds);
  }
}

void toggle_timer() { timer_running = !timer_running; }

void reset_timer() {
  minutes = POMODORO_INTERVAL_MINS;
  seconds = 0;
  timer_running = false;
  timer_elapsed = false;
  M5.Lcd.clearDisplay(TFT_BLACK);
}

void tick_timer() {
  if (seconds-- == 0) {
    seconds = 59;
    if (minutes-- == 0) {
      minutes = POMODORO_INTERVAL_MINS;
      seconds = 0;
      timer_running = false;
      timer_elapsed = true;
      last_blink = pdTICKS_TO_MS(xTaskGetTickCount());
    }
  }
}

void app_loop(void* params) {
  last_active = pdTICKS_TO_MS(xTaskGetTickCount());

  while (true) {
    M5.update();
    bool btnA = M5.BtnA.wasPressed();
    bool btnB = M5.BtnB.wasPressed();

    if (btnA) {
      toggle_timer();
    }

    if (btnB) {
      reset_timer();
    }

    if (btnA || btnB) {
      last_active = pdTICKS_TO_MS(xTaskGetTickCount());
    }

    if (timer_running) {
      uint32_t current_millis = pdTICKS_TO_MS(xTaskGetTickCount());

      if (current_millis - previous_millis >= 1000) {
        previous_millis = current_millis;
        tick_timer();
      }
    }

    backlight_enabled =
        (pdTICKS_TO_MS(xTaskGetTickCount()) - last_active <= 10000) ||
        timer_elapsed;

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void drawing_loop(void* arg) {
  while (true) {
    draw_screen();
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

extern "C" void app_main() {
  power.begin();
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.setBrightness(40);

  xTaskCreatePinnedToCore(app_loop, "app", 8192, nullptr, 10, nullptr, 0);
  xTaskCreatePinnedToCore(drawing_loop, "draw", 8192, nullptr, 10, nullptr, 1);
}
