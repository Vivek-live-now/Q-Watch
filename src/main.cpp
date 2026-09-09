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

String last_drawn_time = "";
uint32_t last_portal_draw = 0;
uint32_t last_ui_draw = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  displayManager.begin();
  btnManager.begin();
  ui.begin();
  battery.begin();
  sensors.begin();
  ledManager.begin();
  soundManager.begin();
  soundManager.playBoot(); // Start 9-DOF fusion

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
  ledManager.loop();
  soundManager.loop(); // Runs at 100Hz non-blocking internally

  // Energy Efficiency & UI Updates
  String current_time = qclock.getSecondsStr();
  bool time_changed = (current_time != last_drawn_time);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

// If we are on a sensor-heavy screen, force a 10Hz redraw
  bool active_app_update = ((ui.getState() == UIState::APP_COMPASS || ui.getState() == UIState::APP_MOTION || ui.getState() == UIState::APP_DEBUG)
                            && millis() - last_ui_draw >= 100);

  if (time_changed || portal_update_due || ui.needsRedraw() || active_app_update) {
      displayManager.update();
      ui.clearRedrawFlag();
      last_drawn_time = current_time;
      last_ui_draw = millis();
      if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
  }
}
