// SPDX-License-Identifier: GPL-3.0-or-later
// Run the verbatim common-core startup policy, not a duplicate implementation.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
using power_t = uint32_t;
constexpr uint32_t POWER_MASK = 0xffffffff, POWER_SIZE = 32, MAX_PULSETIMERS = 32;
enum { POWER_ALL_OFF, POWER_ALL_ON, POWER_ALL_SAVED_TOGGLE, POWER_ALL_SAVED,
       POWER_ALL_ALWAYS_ON, POWER_ALL_OFF_PULSETIME_ON, SRC_RESTART };
struct {
  unsigned poweronstate=3, power=0, pulse_timer[32]{};
  struct { bool save_state=true; } flag;
} settings;
auto* Settings = &settings;
struct {
  unsigned devices_present=1, power_latching=0, power=0, blink_powersave=0;
} TasmotaGlobal;
bool cold;
bool ResetReasonPowerOn() { return cold; }
std::vector<uint32_t> output;
void SetDevicePower(uint32_t power, int source) {
  assert(source == SRC_RESTART);
  output.push_back(power & 1);
}
bool bitRead(uint32_t value, unsigned bit) { return (value >> bit) & 1; }
void SetPulseTimer(unsigned, unsigned) {}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable" // ESP-only local in upstream.
#include "power_on_function.inc"
#pragma GCC diagnostic pop
int main() {
  for (bool is_cold : {false,true}) {
    cold = is_cold;
    for (unsigned option=0; option<=5; ++option) {
      for (unsigned saved : {0u,1u}) {
        for (bool remember : {false,true}) {
          Settings->poweronstate=option;
          Settings->power=saved;
          Settings->flag.save_state=remember;
          TasmotaGlobal.power=0;
          output.clear();
          SetPowerOnState();
          bool forced = option==4 || (cold && (option==0 || option==1 || option==5));
          if (!forced && !remember) assert(output.empty());
          else {
            unsigned expected = option==4 ? 1 :
                !cold ? saved : option==1 ? 1 :
                (option==0 || option==5) ? 0 : option==2 ? saved^1 : saved;
            assert(output.size()==1 && output[0]==expected);
          }
        }
      }
    }
  }
  puts("Common Tasmota power-on policy: 48 cold/warm, option and save-state cases passed");
}
