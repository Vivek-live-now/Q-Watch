#include "max30102_manager.h"
#include <Arduino.h>
#include "display.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "button_manager.h"
#include "ui_core.h"
#include "battery.h"
#include "sensors.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "file_manager.h"
#include "settings_data.h"
#include "ir_engine.h"
#include "timekeeping.h"
#include "anim_engine.h"
#include "vibration_manager.h"
#include "air_mouse.h"
#include "qlink.h"
#ifdef ARDUINO
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Hardware RTC memory boot loop detector & recovery guard
RTC_DATA_ATTR static uint32_t s_rapid_boot_count = 0;
RTC_DATA_ATTR static bool s_safe_mode_active = false;
#endif

int last_drawn_sec = -1;
uint32_t last_portal_draw = 0;
uint32_t last_ui_draw = 0;
static uint32_t s_boot_healthy_time = 0;

void setup() {
#ifdef ARDUINO
  // Transient brownout guard for ESP32-S3 SuperMini board startup
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    s_rapid_boot_count++;
    if (s_rapid_boot_count >= 3) {
      s_safe_mode_active = true;
    }
  } else {
    s_rapid_boot_count = 0;
  }
#else
  esp_sleep_wakeup_cause_t wakeup_reason = (esp_sleep_wakeup_cause_t)0;
#endif

  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  fileManager.begin();
  settingsManager.begin();

#ifdef ARDUINO
  if (s_safe_mode_active) {
    Serial.println("[CRITICAL] BOOT LOOP DETECTED! Booting into SAFE MODE (radios bypassed)...");
    settingsManager.get().wifi_enabled = false;
    settingsManager.get().ble_enabled = false;
    qlink.clearBondedDevices();
  }
#endif

  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // Silent background data recording: OLED, audio & LEDs must NOT turn on!
    ui.performSilentDeepSleepWake();
    // If performSilentDeepSleepWake returns, user pressed a button during sampling!
  }

  displayManager.begin();
  btnManager.begin();
  ui.begin();
  battery.begin();
  sensors.begin();
  max30102Manager.begin();
  ledManager.begin();
  soundManager.begin();
  vibrationManager.begin();
  irEngine.begin();
  timekeeping.begin();
  animEngine.begin();
  airMouse.begin();
  qlink.begin();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    soundManager.playWake();
  } else {
    soundManager.playBoot();
    animEngine.playBootAnimation();
  }

  wifiPortal.begin();
  qclock.begin(configManager.get().timezone);

#ifdef ARDUINO
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
#endif
  s_boot_healthy_time = millis();
}

void loop() {
  if (s_boot_healthy_time > 0 && (millis() - s_boot_healthy_time >= 4000)) {
#ifdef ARDUINO
    s_rapid_boot_count = 0;
    s_safe_mode_active = false;
#endif
    s_boot_healthy_time = 0;
  }

  wifiPortal.loop();
  qlink.loop();
  qclock.loop();
  weather.loop();

  btnManager.loop();
  ui.loop();
  sensors.loop();
  max30102Manager.loop();
  ledManager.loop();
  soundManager.loop();
  vibrationManager.loop();
  irEngine.loop();
  timekeeping.loop();
  airMouse.loop();

  // Energy Efficiency & UI Updates
  int current_sec = qclock.getSecond();
  bool time_changed = (current_sec != last_drawn_sec);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

  bool clock_active = (ui.getState() == UIState::APP_CLOCK && (timekeeping.stopwatch.isRunning() || timekeeping.timer.isRunning() || timekeeping.alarmManager.isRinging()));
  bool anim_active = (ui.getState() == UIState::APP_ANIM_PLAYER && animEngine.isPlaying());
  bool wireless_active = (ui.getState() == UIState::APP_WIRELESS);
  bool vibe_active = (ui.getState() == UIState::APP_VIBRATION && vibrationManager.isVibrating());
  bool active_app_update = ((ui.getState() == UIState::APP_COMPASS || ui.getState() == UIState::APP_MOTION || ui.getState() == UIState::APP_HEALTH || ui.getState() == UIState::APP_IR || ui.getState() == UIState::APP_RUNNING || clock_active || anim_active || wireless_active || vibe_active)
                            && millis() - last_ui_draw >= 20);

  if (!ui.isDisplayOff() && ui.getState() != UIState::SLEEPING) {
      if (time_changed || portal_update_due || ui.needsRedraw() || active_app_update) {
          displayManager.update();
          ui.clearRedrawFlag();
          last_drawn_sec = current_sec;
          last_ui_draw = millis();
          if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
      }
  }

  // Yield to FreeRTOS scheduler to allow idle core power saving and avoid 100% spinlock
  if (ui.isDisplayOff()) {
      delay(10);
  } else {
      delay(1);
  }
}
