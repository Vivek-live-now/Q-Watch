#include <Arduino.h>
#include "display.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "button_manager.h"
#include "ui_core.h"
#include "battery.h"

String last_drawn_time = "";
uint32_t last_portal_draw = 0;
uint32_t last_batt_print = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  displayManager.begin();
  btnManager.begin();
  ui.begin();
  battery.begin();

  wifiPortal.begin();
  qclock.begin(configManager.get().timezone);
}

void loop() {
  wifiPortal.loop();
  qclock.loop();
  weather.loop();

  btnManager.loop();
  ui.loop(); // Process button inputs into UI state

  // Periodically print battery info for testing purposes (since UI integration is not requested yet)
  if (millis() - last_batt_print >= 5000) {
      float vbat = battery.readVoltage();
      int pct = battery.readPercentage();

      // We know the divider is exactly 1/2, so the ADC node voltage is vbat / 2.0
      Serial.printf("[BATTERY] Raw Node: %.2fV | Calc VBAT: %.2fV | Est Pct: %d%%\n", (vbat / 2.0f), vbat, pct);
      last_batt_print = millis();
  }

  // Energy Efficiency & UI Updates
  String current_time = qclock.getSecondsStr();
  bool time_changed = (current_time != last_drawn_time);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

  if (time_changed || portal_update_due || ui.needsRedraw()) {
      displayManager.update();
      ui.clearRedrawFlag();
      last_drawn_time = current_time;
      if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
  }
}
