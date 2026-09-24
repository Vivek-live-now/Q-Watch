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

int last_drawn_sec = -1;
uint32_t last_portal_draw = 0;
uint32_t last_ui_draw = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  fileManager.begin();
  settingsManager.begin();

  displayManager.begin();
  btnManager.begin();
  ui.begin();
  battery.begin();
  sensors.begin();
  max30102Manager.begin();
  ledManager.begin();
  soundManager.begin();
  irEngine.begin();
  timekeeping.begin();
  animEngine.begin();
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    soundManager.playWake();
  } else {
    soundManager.playBoot();
    animEngine.playBootAnimation();
  }

  wifiPortal.begin();
  qclock.begin(configManager.get().timezone);
}

void loop() {
  wifiPortal.loop();
  qclock.loop();
  weather.loop();

  btnManager.loop();
  ui.loop();
  sensors.loop();
  max30102Manager.loop();
  ledManager.loop();
  soundManager.loop();
  irEngine.loop();
  timekeeping.loop();

  // Energy Efficiency & UI Updates
  int current_sec = qclock.getSecond();
  bool time_changed = (current_sec != last_drawn_sec);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

  bool clock_active = (ui.getState() == UIState::APP_CLOCK && (timekeeping.stopwatch.isRunning() || timekeeping.timer.isRunning() || timekeeping.alarmManager.isRinging()));
  bool anim_active = (ui.getState() == UIState::APP_ANIM_PLAYER && animEngine.isPlaying());
  bool wireless_active = (ui.getState() == UIState::APP_WIRELESS);
  bool active_app_update = ((ui.getState() == UIState::APP_COMPASS || ui.getState() == UIState::APP_MOTION || ui.getState() == UIState::APP_HEALTH || ui.getState() == UIState::APP_IR || clock_active || anim_active || wireless_active)
                            && millis() - last_ui_draw >= 20);

  if (time_changed || portal_update_due || ui.needsRedraw() || active_app_update) {
      displayManager.update();
      ui.clearRedrawFlag();
      last_drawn_sec = current_sec;
      last_ui_draw = millis();
      if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
  }
}
