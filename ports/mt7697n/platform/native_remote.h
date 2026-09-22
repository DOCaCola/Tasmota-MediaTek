// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
namespace mt7697 { namespace remote {
struct Status {
  uint32_t reports, dropped, control_dropped, invalid, decrypted, duplicates;
  uint32_t actions, paired, failures, last_error, last_code;
  uint16_t last_pid;
  int8_t last_rssi;
  uint8_t state, devices;
  uint32_t pairing_seconds;
};
// All API calls belong to the Tasmota main task. start() requires CONNSYS ready.
bool start(uint32_t now);
void poll(uint32_t now);
bool pair(uint32_t now, unsigned seconds);
bool forget(unsigned slot); // 1..10; 0 removes all; persists before changing RAM
Status status(uint32_t now);
bool device(unsigned index,uint16_t& pid,uint8_t mac[6]); // SDK address byte order
bool action(uint32_t& code);
} }
