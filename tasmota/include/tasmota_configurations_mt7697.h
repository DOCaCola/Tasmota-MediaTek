// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Initial native target: serial commands, MQTT, persistent station settings.
// Native OTA integration is still required; ESP updaters are not selected.
#undef USE_WEBSERVER
#undef USE_EMULATION
#undef USE_EMULATION_HUE
#undef USE_EMULATION_WEMO
#undef USE_EMULATION_SHELLY
#undef USE_DISCOVERY
#undef USE_TASMOTA_DISCOVERY
#undef USE_LIGHT
#undef USE_I2C
#undef USE_SPI
#undef USE_TLS
#undef USE_MQTT_TLS
#undef USE_ARDUINO_OTA
#undef USE_UFILESYS
#undef USE_SDCARD
#undef USE_TIMERS
#undef USE_SUNRISE
#undef USE_RULES
#undef USE_SCRIPT
#undef USE_BERRY
#undef USE_PWM_DIMMER
#undef USE_SERIAL_BRIDGE
#undef USE_TELNET
#undef USE_ETHERNET
#undef USE_DEVICE_GROUPS
#undef USE_ESP32_WDT

#undef USE_PING

#undef USE_IMPROV

#define TASMOTA_ARCH "mt7697n"
#define ARDUINO_CORE_RELEASE "LinkIt-0.10.21-native"
