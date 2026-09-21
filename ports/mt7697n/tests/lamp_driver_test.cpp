// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the actual Tasmota lamp driver with its production PWM backend.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#define USE_LIGHT
#define USE_YLXD01YL_LIGHT
#define PROGMEM
#define PSTR(x) x
enum {FUNC_MODULE_INIT,FUNC_PRE_INIT,FUNC_SET_CHANNELS,FUNC_COMMAND,
      FUNC_WEB_ADD_MAIN_BUTTON,FUNC_WEB_GET_ARG,FUNC_ABOUT_TO_RESTART};
enum {LOG_LEVEL_INFO,LOG_LEVEL_ERROR,LT_CW=10,P_RGB_REMAP=0};
struct Config {
  uint8_t lamp_config_version=0,lamp_night=0,light_dimmer=100,poweronstate=3;
  unsigned power=1;
  uint8_t light_color[5]={255,255,255,255,255};
  uint8_t param[1]={};
  struct { bool pwm_multi_channels=false; } flag3;
  struct { bool pwm_ct_mode=false; } flag4;
} config;
Config* Settings=&config;
struct {unsigned light_type=0,light_driver=0;} TasmotaGlobal;
struct {int data_len=0,payload=-1;char* command=nullptr;} XdrvMailbox;
template<class... T> void AddLog(T...) {}
template<class... T> void Response_P(T...) {}
template<class... T> void WSContentSend_P(T...) {}
bool DecodeCommand(const char*,void(*const* commands)()) {commands[0]();return true;}
unsigned ct_min,ct_max;
void setCTRange(unsigned a,unsigned b) {ct_min=a;ct_max=b;}
struct {unsigned getCT(){return 250;} unsigned getBriCT(){return 25;}} light_state;
struct {void changeCTB(unsigned,unsigned){}} light_controller;
std::string webarg,webcommand;
void WebGetArg(const char*,char* value,unsigned size) {snprintf(value,size,"%s",webarg.c_str());}
void ExecuteWebCommand(char* command) {webcommand=command;}
extern "C" {
#include "hal_gpio.h"
#include "hal_pinmux.h"
#include "hal_pwm.h"
}
uint32_t duties[34]={};
bool fail_write=false;
extern "C" {
hal_pwm_status_t hal_pwm_init(hal_pwm_source_clock_t) {return HAL_PWM_STATUS_OK;}
hal_pwm_status_t hal_pwm_set_frequency(hal_pwm_channel_t,uint32_t hz,uint32_t* n) {
  assert(hz==10000);*n=4000;return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_set_duty_cycle(hal_pwm_channel_t c,uint32_t value) {
  if(fail_write){fail_write=false;return HAL_PWM_STATUS_ERROR;}
  duties[c]=value;
  assert(duties[32]+duties[33]<=4000);
  assert(!duties[31] || (!duties[32] && !duties[33]));
  return HAL_PWM_STATUS_OK;
}
hal_pinmux_status_t hal_pinmux_set_function(hal_gpio_pin_t,uint8_t) {return HAL_PINMUX_STATUS_OK;}
hal_pwm_status_t hal_pwm_start(hal_pwm_channel_t) {return HAL_PWM_STATUS_OK;}
hal_pwm_status_t hal_pwm_stop(hal_pwm_channel_t) {return HAL_PWM_STATUS_OK;}
}
#include "../../../tasmota/tasmota_xlgt_light/xlgt_12_ylxd01yl.ino"
int main() {
  assert(Xlgt12(FUNC_MODULE_INIT));
  assert(TasmotaGlobal.light_type==LT_CW && YlxdReady);
  assert(!Settings->power && !Settings->poweronstate && Settings->light_dimmer==10);
  assert(Xlgt12(FUNC_PRE_INIT) && ct_min==153 && ct_max==370);
  uint16_t channels[2]={1023,0};
  XdrvMailbox.command=reinterpret_cast<char*>(channels);
  assert(Xlgt12(FUNC_SET_CHANNELS) && duties[33]==4000 && !duties[32]);
  channels[0]=0;channels[1]=1023;
  Xlgt12(FUNC_SET_CHANNELS);
  assert(duties[32]==4000 && !duties[33]);
  XdrvMailbox.data_len=1;XdrvMailbox.payload=1;
  Xlgt12(FUNC_COMMAND);
  assert(Settings->lamp_night==1 && duties[31]==4000 && !duties[32]);
  channels[0]=channels[1]=0;
  Xlgt12(FUNC_SET_CHANNELS);
  assert(!duties[31]);
  webarg="0";Xlgt12(FUNC_WEB_GET_ARG);assert(webcommand=="LampNight 0");
  webarg="1";Xlgt12(FUNC_WEB_GET_ARG);assert(webcommand=="LampNight 1");
  channels[0]=512;Xlgt12(FUNC_SET_CHANNELS);
  Xlgt12(FUNC_ABOUT_TO_RESTART);assert(!duties[31]);
  fail_write=true;channels[0]=1023;
  Xlgt12(FUNC_SET_CHANNELS);
  assert(!YlxdReady && !duties[31] && !duties[32] && !duties[33]);
  puts("Production lamp driver dispatch, migration, channel order, mode handover, web commands, restart and failure passed");
}
