#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "M5GFX.h"
#include "M5Unified.h"

#define POMODORO_INTERVAL_MINS 25
#define SLEEP_INTERVAL_US 1000000ULL
#define SLEEP_INTERVAL_MS 1000U
#define IDLE_TIMEOUT_MS 10000U
#define NVS_NAMESPACE "timer"
#define NVS_KEY "state"

typedef struct {
  uint8_t minutes;
  uint8_t seconds;
  bool timer_running;
  bool timer_elapsed;
} timer_state_t;

bool backlight_enabled;
bool blink_enabled;
timer_state_t state = {POMODORO_INTERVAL_MINS, 0, false, false};
uint32_t last_active;
uint32_t last_blink;

static const char* time_format_string = " %02d:%02d";

void save_state() {
  puts("Save state to NVS");
  nvs_handle_t handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
    nvs_set_blob(handle, NVS_KEY, &state, sizeof(state));
    nvs_commit(handle);
    nvs_close(handle);
  }
}

void load_state() {
  puts("Load state from NVS");
  nvs_handle_t handle;
  size_t required_size = sizeof(state);
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
    if (nvs_get_blob(handle, NVS_KEY, &state, &required_size) != ESP_OK) {
      puts("Setting default state because nvs_get_blob failed");
      state = {POMODORO_INTERVAL_MINS, 0, false, false};
    }
    nvs_close(handle);
  }
}

void enter_deep_sleep() {
  save_state();
  M5.Lcd.setBrightness(0);
  rtc_gpio_init(GPIO_NUM_11);
  rtc_gpio_pullup_en(GPIO_NUM_11);
  rtc_gpio_pulldown_dis(GPIO_NUM_11);
  rtc_gpio_init(GPIO_NUM_12);
  rtc_gpio_pullup_en(GPIO_NUM_12);
  rtc_gpio_pulldown_dis(GPIO_NUM_12);
  esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_US);
  esp_sleep_enable_ext1_wakeup((1ULL << GPIO_NUM_11) | (1ULL << GPIO_NUM_12),
                               ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

void toggle_timer() { state.timer_running = !state.timer_running; }

void reset_timer() {
  state = {POMODORO_INTERVAL_MINS, 0, false, false};
  M5.Lcd.clearDisplay(TFT_BLACK);
}

void tick_timer() {
  if (state.seconds == 0) {
    state.seconds = 59;
    if (state.minutes == 0) {
      state = {POMODORO_INTERVAL_MINS, 0, false, true};
    } else {
      state.minutes--;
    }
  } else {
    state.seconds--;
  }
}

void poll_input() {
  M5.update();
  bool btnA = M5.BtnA.wasPressed();
  bool btnB = M5.BtnB.wasPressed();

  if (btnA) toggle_timer();
  if (btnB) reset_timer();
  if (btnA || btnB) last_active = M5.millis();
}

void draw_screen() {
  uint8_t battery_pct = M5.Power.getBatteryLevel();

  if (backlight_enabled || state.timer_elapsed) {
    M5.Lcd.setBrightness(40);
  } else {
    M5.Lcd.setBrightness(0);
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.fillRect(2, 2, 62, 20, state.timer_elapsed ? TFT_RED : TFT_BLACK);
  M5.Lcd.setCursor(2, 2);
  M5.Lcd.printf("%d%%", battery_pct);

  if (state.timer_elapsed) {
    uint32_t current_millis = M5.millis();

    if (current_millis - last_blink > 1000) {
      blink_enabled = !blink_enabled;
      last_blink = current_millis;
    }

    M5.Lcd.fillRect(58, 67 - 14, 240 - 58, 135 - 67 - 14, TFT_RED);
    if (blink_enabled) {
      M5.Lcd.setTextSize(4);
      M5.Lcd.setTextColor(TFT_WHITE);
      M5.Lcd.setCursor(58, 67 - 14);
      M5.Lcd.print("00:00");
    }
  } else {
    M5.Lcd.setTextSize(4);
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.fillRect(34, 67 - 14, 240 - 32, 135 - 67 - 14, TFT_BLACK);
    M5.Lcd.setCursor(34, 67 - 14);
    M5.Lcd.printf(time_format_string, state.minutes, state.seconds);
  }
}

void drawing_task(void* args) {
  while (true) {
    uint32_t started = M5.millis();

    draw_screen();

    uint32_t elapsed = M5.millis() - started;

    vTaskDelay(pdMS_TO_TICKS(SLEEP_INTERVAL_MS - elapsed));
  }

  vTaskDelete(NULL);
}

void timer_task(void* args) {
  while (true) {
    uint32_t started = M5.millis();

    bool was_elapsed = state.timer_elapsed;

    if (state.timer_running) {
      tick_timer();
    }

    if (!was_elapsed && state.timer_elapsed) {
      M5.Lcd.clearDisplay(TFT_RED);
    }

    backlight_enabled =
        (M5.millis() - last_active <= IDLE_TIMEOUT_MS) || state.timer_elapsed;

    if (!backlight_enabled) {
      enter_deep_sleep();
    }

    uint32_t elapsed = M5.millis() - started;

    vTaskDelay(pdMS_TO_TICKS(SLEEP_INTERVAL_MS - elapsed));
  }

  vTaskDelete(NULL);
}

void input_task(void* args) {
  while (true) {
    poll_input();

    vTaskDelay(pdMS_TO_TICKS(50));
  }

  vTaskDelete(NULL);
}

extern "C" void app_main() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.setBrightness(0);

  // handle filled/corrupt NVS data by reformatting
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }

  // if woken up from sleep, restore state
  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
  if (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
    rtc_gpio_deinit(GPIO_NUM_11);
    rtc_gpio_deinit(GPIO_NUM_12);
    load_state();
  }

  if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
    // tick the timer if it is running
    if (state.timer_running) {
      tick_timer();
    }
    // timer not elapsed: save and go back to sleep
    if (!state.timer_elapsed) {
      enter_deep_sleep();
    } else {
      // timer elapsed: set last_active so the idle timeout works correctly
      M5.Lcd.clearDisplay(TFT_RED);
      last_active = M5.millis();
    }
  } else if (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1) {
    // button press woke the device: turn screen on
    last_active = M5.millis();
  }

  // start
  xTaskCreatePinnedToCore(timer_task, "timer", 4096, nullptr, 10, nullptr, 1);
  xTaskCreatePinnedToCore(drawing_task, "drawing", 4096, nullptr, 10, nullptr,
                          0);
  xTaskCreate(input_task, "input", 4096, nullptr, 11, nullptr);
}