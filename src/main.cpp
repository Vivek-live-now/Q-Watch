#include <Arduino.h>
#include "display.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"

String last_drawn_time = "";
uint32_t last_portal_draw = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  displayManager.begin();
  wifiPortal.begin();
  qclock.begin(configManager.get().timezone);
}

void loop() {
  wifiPortal.loop();
  qclock.loop();
  weather.loop();

  // Energy Efficiency: Only update display when seconds change.
  // If in portal mode, limit redraws to 1Hz (1000ms) to avoid CPU/SPI thrashing.
  String current_time = qclock.getSecondsStr();
  bool time_changed = (current_time != last_drawn_time);
  bool portal_update_due = (wifiPortal.getState() == WifiState::PORTAL && millis() - last_portal_draw >= 1000);

  if (time_changed || portal_update_due) {
      displayManager.update();
      last_drawn_time = current_time;
      if (wifiPortal.getState() == WifiState::PORTAL) last_portal_draw = millis();
  }
}
