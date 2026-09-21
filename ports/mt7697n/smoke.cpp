#include <Arduino.h>
#include <FreeRTOS.h>
#include "ylxd01yl_pwm.h"
#ifdef MT7697_NETWORK_PROBE
#include <LWiFi.h>
#endif

ylxd01yl::Pwm lamp;
#ifdef MT7697_DEPENDENCY_PROBE
bool check_tasmota_dependencies();
#endif

// SDK build verification only. This is not the Tasmota application.
void setup() {
  Serial.begin(115200);
  Serial.println("MT7697N SDK build probe");
  Serial.println(lamp.begin() ? "PWM initialized: all outputs zero" : "PWM initialization failed");
#ifdef MT7697_DEPENDENCY_PROBE
  Serial.println(check_tasmota_dependencies() ? "Runtime/MQTT checks passed" : "Runtime/MQTT checks failed");
#endif
#ifdef MT7697_NETWORK_PROBE
  Serial.print("visible networks=");
  Serial.println(WiFi.scanNetworks());
#endif
}

void loop() {
  Serial.print("heap=");
  Serial.println(static_cast<unsigned long>(xPortGetFreeHeapSize()));
  delay(1000);
}
