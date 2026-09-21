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
    YlxdApply(); // Backend decreases the old group before enabling the new.
  }
  Response_P(PSTR("{\"LampNight\":%d,\"LampReady\":%s}"),
             Settings->lamp_night, YlxdReady ? "true" : "false");
}
const char kYlxdCommands[] PROGMEM = "Lamp|Night";
void (* const YlxdCommands[])(void) PROGMEM = { &CmndLampNight };

bool Xlgt12(uint32_t function) {
  switch (function) {
    case FUNC_MODULE_INIT:
      if (Settings->lamp_config_version != 1) {
        Settings->lamp_config_version = 1;
        Settings->lamp_night = 0;
        // First lighting-capable boot must not restore the network-only
        // build's synthetic relay power bit as full lamp brightness.
        Settings->power = 0;
        Settings->poweronstate = 0;
        Settings->light_dimmer = 10;
        memset(Settings->light_color,0,sizeof(Settings->light_color));
        Settings->light_color[3] = 255; // Cold white, normal CCT mode.
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
