#include <Arduino.h>
#include "display.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "button_manager.h"
#include "ui_core.h"

String last_drawn_time = "";
uint32_t last_portal_draw = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  displayManager.begin();
  btnManager.begin();
  ui.begin();

  wifiPortal.begin();
  qclock.begin(configManager.get().timezone);
}

void loop() {
  wifiPortal.loop();
  qclock.loop();
  weather.loop();

  btnManager.loop();
  ui.loop();

  // Energy Efficiency & UI Updates
  String current_time = qclock.getSecondsStr();
  // We force a redraw if the clock changes (for APP_HOME and APP_CLOCK mostly),
  // OR if the portal needs a 1Hz refresh, OR if a button press triggered a state change (ui.needsRedraw())
  bool time_changed = (current_time != last_drawn_time);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

  if (time_changed || portal_update_due || ui.needsRedraw()) {
      displayManager.update();
      ui.clearRedrawFlag();
      last_drawn_time = current_time;
      if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
  }
}
