// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(USE_LIGHT) && defined(USE_YLXD01YL_LIGHT)
#define XLGT_12 12
#include "ylxd01yl_light.h"

ylxd01yl::Pwm YlxdPwm;
bool YlxdReady = false;
uint16_t YlxdFrame[2] = {};

bool YlxdApply(void) {
  if (!YlxdReady) return false;
  const auto duty = ylxd01yl::frame(YlxdFrame[0], YlxdFrame[1], Settings->lamp_night);
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
    Settings->lamp_night = XdrvMailbox.payload;
    if (Settings->save_data) TasmotaGlobal.save_data_counter = 2;
    YlxdApply(); // Backend decreases the old group before enabling the new.
  }
  Response_P(PSTR("{\"LampNight\":%d,\"LampReady\":%s}"),
             Settings->lamp_night, YlxdReady ? "true" : "false");
}
void CmndLampStatus(void) {
  ylxd01yl::PwmStatus state;
  const bool ok = YlxdPwm.status(state);
  const auto target = ylxd01yl::frame(YlxdFrame[0],YlxdFrame[1],Settings->lamp_night);
  Response_P(PSTR("{\"LampStatus\":{\"Night\":%d,\"Ready\":%s,\"Readback\":%s,"
                   "\"Frame\":[%u,%u],\"Target\":[%u,%u,%u],"
                   "\"Duty\":[%u,%u,%u],\"Hz\":[%u,%u,%u],\"Running\":[%u,%u,%u]}}"),
      Settings->lamp_night,YlxdReady?"true":"false",ok?"true":"false",
      YlxdFrame[0],YlxdFrame[1],target.warm,target.cold,target.night,
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
      const uint16_t* channels = reinterpret_cast<const uint16_t*>(XdrvMailbox.command);
      YlxdFrame[0] = channels[0]; YlxdFrame[1] = channels[1];
      YlxdApply();
      return true;
    }
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
      if (YlxdReady) YlxdPwm.off();
      return true;
  }
  return false;
}
#endif
