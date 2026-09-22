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
enum {FUNC_LOOP,FUNC_MODULE_INIT,FUNC_PRE_INIT,FUNC_SET_CHANNELS,FUNC_COMMAND,
      FUNC_WEB_ADD_MAIN_BUTTON,FUNC_WEB_GET_ARG,FUNC_ABOUT_TO_RESTART};
enum {LOG_LEVEL_INFO,LOG_LEVEL_ERROR,LT_CW=10,P_RGB_REMAP=0,LS_POWER=0};
struct Config {
  uint8_t lamp_config_version=0,lamp_night=0,light_dimmer=100,poweronstate=3;
  uint8_t light_correction=1,light_fade=0,lamp_day_dimmer=0,lamp_night_dimmer=0,light_scheme=0;
  unsigned save_data=300;
  unsigned power=1;
  uint8_t light_color[5]={255,255,255,255,255};
  uint8_t param[1]={};
  struct { bool pwm_multi_channels=false, hass_tele_on_power=false; } flag3;
  struct { bool pwm_ct_mode=false; } flag4;
} config;
Config* Settings=&config;
struct {unsigned light_type=0,light_driver=0,save_data_counter=300; bool skip_light_fade=false;} TasmotaGlobal;
struct {int data_len=0,payload=-1;char* command=nullptr;} XdrvMailbox;
template<class... T> void AddLog(T...) {}
template<class... T> void Response_P(T...) {}
unsigned state_publishes=0;
void MqttPublishTeleState() { ++state_publishes; }
template<class... T> void WSContentSend_P(T...) {}
bool DecodeCommand(const char*,void(*const* commands)()) {commands[0]();return true;}
unsigned ct_min,ct_max;
void setCTRange(unsigned a,unsigned b) {ct_min=a;ct_max=b;}
static uint32_t now;
uint32_t millis() {return now;}
struct {bool update=false,power=true,fade_initialized=false,fade_running=false;
 bool fade_once_enabled=false,speed_once_enabled=false;} Light;
bool LightGetFadeSetting(){return Settings->light_fade;}
unsigned LightGetSpeedSetting(){return 1;}
struct {unsigned ct=250,dimmer=100; unsigned getCT(){return ct;}
 unsigned getBriCT(){return 25;} unsigned getDimmer(){return dimmer;}} light_state;
struct {void changeCTB(unsigned,unsigned){} void changeDimmer(unsigned d){light_state.dimmer=d;}} light_controller;
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
  assert(duties[32]+duties[33]<=4320);
  assert(!duties[31] || (!duties[32] && !duties[33]));
  return HAL_PWM_STATUS_OK;
}
hal_pinmux_status_t hal_pinmux_set_function(hal_gpio_pin_t,uint8_t) {return HAL_PINMUX_STATUS_OK;}
hal_pwm_status_t hal_pwm_start(hal_pwm_channel_t) {return HAL_PWM_STATUS_OK;}
hal_pwm_status_t hal_pwm_stop(hal_pwm_channel_t) {return HAL_PWM_STATUS_OK;}
hal_pwm_status_t hal_pwm_get_running_status(hal_pwm_channel_t, hal_pwm_running_status_t* s) {
  *s=HAL_PWM_BUSY;return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_get_frequency(hal_pwm_channel_t,uint32_t* hz) {
  *hz=10000;return HAL_PWM_STATUS_OK;
}
hal_pwm_status_t hal_pwm_get_duty_cycle(hal_pwm_channel_t c,uint32_t* duty) {
  *duty=duties[c];return HAL_PWM_STATUS_OK;
}
}
#include "../../../tasmota/tasmota_xlgt_light/xlgt_12_ylxd01yl.ino"
int main() {
  assert(Xlgt12(FUNC_MODULE_INIT));
  assert(TasmotaGlobal.light_type==LT_CW && YlxdReady);
  assert(!Settings->power && !Settings->poweronstate && Settings->light_dimmer==10);
  assert(Settings->lamp_config_version==3 && !Settings->light_correction);
  assert(Settings->lamp_day_dimmer==10 && Settings->lamp_night_dimmer==5);
  assert(Xlgt12(FUNC_PRE_INIT) && ct_min==153 && ct_max==370);
  light_state.ct=153;
  assert(Xlgt12(FUNC_SET_CHANNELS) && duties[33]==4000 && !duties[32]);
  light_state.ct=370;
  Xlgt12(FUNC_SET_CHANNELS);
  assert(Light.fade_running && duties[33] && duties[32]);
  now=500;Xlgt12(FUNC_LOOP);
  assert(!Light.fade_running && duties[32]==4000 && !duties[33]);
  XdrvMailbox.data_len=1;XdrvMailbox.payload=1;
  Settings->flag3.hass_tele_on_power=true;
  Xlgt12(FUNC_COMMAND);
  assert(state_publishes==1);
  CmndLampNight(); // Idempotent group selection does not publish a change.
  assert(state_publishes==1);
  assert(Settings->lamp_night==1 && light_state.dimmer==5);
  Xlgt12(FUNC_SET_CHANNELS);
  assert(!duties[32] && !duties[33]);
  now+=500;Xlgt12(FUNC_LOOP);
  assert(duties[31]==200);
  assert(TasmotaGlobal.save_data_counter==2);
  light_state.dimmer=17;Xlgt12(FUNC_SET_CHANNELS);
  now+=500;Xlgt12(FUNC_LOOP);assert(duties[31]==680);
  XdrvMailbox.payload=0;CmndLampNight();
  assert(light_state.dimmer==100 && Settings->lamp_night_dimmer==17);
  Xlgt12(FUNC_SET_CHANNELS);now+=500;Xlgt12(FUNC_LOOP);
  assert(duties[32]==4000 && !duties[31]);
  CmndLampStatus();
  Light.power=false;Xlgt12(FUNC_SET_CHANNELS);
  now+=500;Xlgt12(FUNC_LOOP);assert(!duties[32]);
  assert(Settings->lamp_day_dimmer==100);
  webarg="0";Xlgt12(FUNC_WEB_GET_ARG);assert(webcommand=="LampNight 0");
  webarg="1";Xlgt12(FUNC_WEB_GET_ARG);assert(webcommand=="LampNight 1");
  Light.power=true;Xlgt12(FUNC_SET_CHANNELS);
  Xlgt12(FUNC_ABOUT_TO_RESTART);assert(!duties[32]);
  fail_write=true;Xlgt12(FUNC_SET_CHANNELS);
  assert(!YlxdReady && !duties[31] && !duties[32] && !duties[33]);
  puts("Production lamp driver: migration, fades, group brightness, power, restart and failure passed");
}
