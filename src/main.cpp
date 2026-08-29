#include <Arduino.h>
#include "display.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"

uint32_t last_display_update = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting Q-Watch...");

  // 1. Init Display first so user sees something immediately
  displayManager.begin();

  // 2. Init WiFi/Portal
  wifiPortal.begin();

  // 3. Init NTP clock (will sync asynchronously once Wi-Fi connects)
  qclock.begin(configManager.get().timezone);
}

void loop() {
  // Always loop portal and clock
  wifiPortal.loop();
  qclock.loop();

  // Only attempt weather updates if we are connected
  if (wifiPortal.getState() == WifiState::CONNECTED) {
      weather.loop();
  }

  // Update Display at ~10 FPS to save cycles but keep seconds looking smooth
  if (millis() - last_display_update > 100) {
      displayManager.update();
      last_display_update = millis();
  }
}
