// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(USE_LIGHT) && defined(USE_YLXD01YL_LIGHT)
#define XLGT_12 12
#include "ylxd01yl_fade.h"

ylxd01yl::Pwm YlxdPwm;
bool YlxdReady = false;
ylxd01yl::Fade YlxdFade;

bool YlxdApply(void) {
  if (!YlxdReady) return false;
  const auto duty = YlxdFade.current();
  const bool ok = Settings->lamp_night ? YlxdPwm.night(duty.night) :
                                       YlxdPwm.daylight(duty.warm,duty.cold);
  if (!ok) {
    YlxdReady = false;
    AddLog(LOG_LEVEL_ERROR, PSTR("LGT: YLXD01YL PWM failed; outputs disabled"));
  }
  return ok;
}

void CmndLampNight(void) {
  if (XdrvMailbox.data_len && XdrvMailbox.payload >= 0 && XdrvMailbox.payload <= 1) {
    if (Settings->lamp_night != XdrvMailbox.payload) {
      if (Settings->lamp_night) Settings->lamp_night_dimmer=light_state.getDimmer();
      else Settings->lamp_day_dimmer=light_state.getDimmer();
      Settings->lamp_night = XdrvMailbox.payload;
      // Group selection keeps power unchanged and restores that group's dimmer.
      light_controller.changeDimmer(Settings->lamp_night ?
          Settings->lamp_night_dimmer : Settings->lamp_day_dimmer);
      Light.update=true;
      if (Settings->save_data) TasmotaGlobal.save_data_counter = 2;
      if (Settings->flag3.hass_tele_on_power) MqttPublishTeleState();
    }
  }
  Response_P(PSTR("{\"LampNight\":%d,\"LampReady\":%s}"),
             Settings->lamp_night, YlxdReady ? "true" : "false");
}
void CmndLampStatus(void) {
  ylxd01yl::PwmStatus state;
  const bool ok = YlxdPwm.status(state);
  const auto target = YlxdFade.target();
  Response_P(PSTR("{\"LampStatus\":{\"Night\":%d,\"Ready\":%s,\"Readback\":%s,"
                   "\"Fading\":%s,\"DayDimmer\":%u,\"NightDimmer\":%u,\"Target\":[%u,%u,%u],"
                   "\"Duty\":[%u,%u,%u],\"Hz\":[%u,%u,%u],\"Running\":[%u,%u,%u]}}"),
      Settings->lamp_night,YlxdReady?"true":"false",ok?"true":"false",
      YlxdFade.active()?"true":"false",Settings->lamp_day_dimmer,Settings->lamp_night_dimmer,
      target.warm,target.cold,target.night,
      state.duty[0],state.duty[1],state.duty[2],
      state.frequency[0],state.frequency[1],state.frequency[2],
      state.running[0],state.running[1],state.running[2]);
}
const char kYlxdCommands[] PROGMEM = "Lamp|Night|Status";
void (* const YlxdCommands[])(void) PROGMEM = { &CmndLampNight, &CmndLampStatus };

bool Xlgt12(uint32_t function) {
  switch (function) {
    case FUNC_MODULE_INIT:
      if (Settings->lamp_config_version == 0) {
        Settings->lamp_night = 0;
        // First lighting-capable boot must not restore the network-only
        // build's synthetic relay power bit as full lamp brightness.
        Settings->power = 0;
        Settings->poweronstate = 0;
        Settings->light_dimmer = 10;
        memset(Settings->light_color,0,sizeof(Settings->light_color));
        Settings->light_color[3] = 255; // Cold white, normal CCT mode.
      }
      if (Settings->lamp_config_version < 2) {
        // Stock uses linear requested brightness for both groups. In particular,
        // gamma correction can quantize low night brightness to a zero frame.
        Settings->light_correction = 0;
        Settings->lamp_config_version = 2;
      }
      if (Settings->lamp_config_version < 3) {
        Settings->lamp_day_dimmer=Settings->lamp_night ? 10 : Settings->light_dimmer;
        Settings->lamp_night_dimmer=Settings->lamp_night ? Settings->light_dimmer : 5;
        Settings->light_fade=1;
        Settings->lamp_config_version=3;
      }
      Settings->light_correction=0;
      Settings->lamp_night = Settings->lamp_night == 1;
      Settings->flag3.pwm_multi_channels = false;
      Settings->flag4.pwm_ct_mode = false;
      Settings->param[P_RGB_REMAP] = 0;
      TasmotaGlobal.light_type = LT_CW;
      TasmotaGlobal.light_driver = XLGT_12;
      YlxdReady = YlxdPwm.begin();
      AddLog(YlxdReady ? LOG_LEVEL_INFO : LOG_LEVEL_ERROR,
             PSTR("LGT: YLXD01YL PWM %s"), YlxdReady ? "ready, outputs off" : "initialization failed");
      return true;
    case FUNC_PRE_INIT:
      setCTRange(ylxd01yl::kColdMired,ylxd01yl::kWarmMired);
      light_controller.changeCTB(light_state.getCT(),light_state.getBriCT());
      return true;
    case FUNC_SET_CHANNELS: {
      const unsigned dimmer=light_state.getDimmer();
      if (Settings->light_scheme == LS_POWER) {
        if (Settings->lamp_night) Settings->lamp_night_dimmer=dimmer;
        else Settings->lamp_day_dimmer=dimmer;
      }
      unsigned kelvin=1000000u/light_state.getCT();
      if (light_state.getCT()==ylxd01yl::kWarmMired) kelvin=2700;
      if (light_state.getCT()==ylxd01yl::kColdMired) kelvin=6500;
      const auto target=ylxd01yl::target(kelvin,Light.power ? dimmer : 0,Settings->lamp_night);
      const uint32_t duration=(LightGetFadeSetting() && !TasmotaGlobal.skip_light_fade &&
          Light.fade_initialized && Settings->light_scheme == LS_POWER) ?
          LightGetSpeedSetting()*500u : 0;
      YlxdFade.set(target,Settings->lamp_night,Light.power?dimmer:0,duration,millis());
      YlxdFade.poll(millis());
      Light.fade_initialized=true;
      Light.fade_running=YlxdFade.active();
      Light.fade_once_enabled=Light.speed_once_enabled=false;
      YlxdApply();
      return true;
    }
    case FUNC_LOOP:
      if (YlxdFade.poll(millis())) YlxdApply();
      Light.fade_running=YlxdFade.active();
      return true;
    case FUNC_COMMAND:
      return DecodeCommand(kYlxdCommands,YlxdCommands);
    case FUNC_WEB_ADD_MAIN_BUTTON:
      WSContentSend_P(PSTR("<p><button onclick=\"la('&ln=0');\">Daylight</button>"
                          "<button onclick=\"la('&ln=1');\">Night light</button></p>"));
      return true;
    case FUNC_WEB_GET_ARG: {
      char value[8];
      WebGetArg(PSTR("ln"),value,sizeof(value));
      if (!strcmp(value,"0") || !strcmp(value,"1")) {
        char command[] = "LampNight 0";
        command[10] = value[0];
        ExecuteWebCommand(command);
      }
      return true;
    }
    case FUNC_ABOUT_TO_RESTART:
      YlxdFade.off();
      Light.fade_running=false;
      if (YlxdReady) YlxdPwm.off();
      return true;
  }
  return false;
}
#endif
