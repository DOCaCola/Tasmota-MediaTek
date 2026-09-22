// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(TASMOTA_PLATFORM_MT7697N) && defined(USE_YLXD01YL_LIGHT)
#define XDRV_95 95
#include <platform/native_remote.h>

bool NativeRemoteStarted=false;
unsigned NativeRemoteLastState=255;
int NativeRemoteCtDirection=1;
uint32_t NativeRemoteUnsupported=0;

void CmndRemoteStatus(void) {
  const auto s=mt7697::remote::status(millis());
  Response_P(PSTR("{\"RemoteStatus\":{\"State\":%u,\"Devices\":%u,\"Pairing\":%u,"
    "\"Reports\":%u,\"Dropped\":%u,\"ControlDropped\":%u,\"Invalid\":%u,"
    "\"Decrypted\":%u,\"Duplicates\":%u,\"Actions\":%u,\"Unsupported\":%u,"
    "\"Paired\":%u,\"Failures\":%u,\"Error\":%u,\"PID\":%u,\"RSSI\":%d,\"Code\":%u}}"),
    s.state,s.devices,s.pairing_seconds,s.reports,s.dropped,s.control_dropped,
    s.invalid,s.decrypted,s.duplicates,s.actions,NativeRemoteUnsupported,
    s.paired,s.failures,s.last_error,s.last_pid,s.last_rssi,s.last_code);
}
void CmndRemotePair(void) {
  bool ok=false;
  if (XdrvMailbox.data_len && XdrvMailbox.payload>=0 && XdrvMailbox.payload<=120)
    ok=mt7697::remote::pair(millis(),XdrvMailbox.payload);
  ResponseCmndChar(ok?"Done":"Use 0..120 seconds when scanning");
}
void CmndRemoteForget(void) {
  bool ok=false;
  if (XdrvMailbox.data_len && XdrvMailbox.payload>=0 && XdrvMailbox.payload<=10)
    ok=mt7697::remote::forget(XdrvMailbox.payload);
  ResponseCmndChar(ok?"Done":"Use slot 1..10, or 0 for all, when scanning");
}
void CmndRemoteList(void) {
  Response_P(PSTR("{\"RemoteList\":["));
  bool first=true;
  for (unsigned i=0;i<10;++i) {
    uint16_t pid; uint8_t mac[6];
    if (!mt7697::remote::device(i,pid,mac)) continue;
    ResponseAppend_P(PSTR("%s{\"Slot\":%u,\"PID\":%u,\"MAC\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}"),
        first?"":",",i+1,pid,mac[5],mac[4],mac[3],mac[2],mac[1],mac[0]);
    first=false;
  }
  ResponseAppend_P(PSTR("]}"));
}
const char kNativeRemoteCommands[] PROGMEM="Remote|Status|Pair|Forget|List";
void (*const NativeRemoteCommands[])(void) PROGMEM={
  &CmndRemoteStatus,&CmndRemotePair,&CmndRemoteForget,&CmndRemoteList
};

void NativeRemoteCommand(const char* command) {
  char text[40]; strlcpy(text,command,sizeof(text));
  ExecuteCommand(text,SRC_BUTTON);
}
void NativeRemoteAction(uint32_t code) {
  char command[40]={};
  switch (code) {
    case 0: strcpy(command,"Power ON"); break;
    case 1: strcpy(command,"Power OFF"); break;
    case 7: strcpy(command,"Power TOGGLE"); break;
    case 4: snprintf(command,sizeof(command),"LampNight %u",!Settings->lamp_night); break;
    case 0x020004:
      NativeRemoteCommand("LampNight 1");
      NativeRemoteCommand("Dimmer 1");
      strcpy(command,"Power ON"); break;
    case 3: strcpy(command,"Dimmer +"); break;
    case 5: strcpy(command,"Dimmer -"); break;
    case 0x020003: strcpy(command,"Dimmer 100"); break;
    case 0x020005: strcpy(command,"Dimmer 1"); break;
    case 2: case 8: case 9: {
      if (Settings->lamp_night) return;
      // Use Tasmota's CT range and 10% steps; bounce at the endpoints.
      uint16_t lo,hi; getCTRange(&lo,&hi);
      int ct=light_state.getCT();
      if (code==8) NativeRemoteCtDirection=1;
      if (code==9) NativeRemoteCtDirection=-1;
      if (ct>=hi) NativeRemoteCtDirection=-1;
      if (ct<=lo) NativeRemoteCtDirection=1;
      ct+=NativeRemoteCtDirection*max(1,(int(hi)-lo)/10);
      ct=constrain(ct,int(lo),int(hi));
      snprintf(command,sizeof(command),"CT %d",ct); break;
    }
    default: ++NativeRemoteUnsupported; return;
  }
  // Stock handheld actions normally use 500 ms. Do not change saved fade settings.
  Light.fade_once_enabled=true; Light.fade_once_value=true;
  Light.speed_once_enabled=true; Light.speed_once_value=1;
  NativeRemoteCommand(command);
}
bool Xdrv95(uint32_t function) {
  switch (function) {
    case FUNC_LOOP:
      if (!NativeRemoteStarted && millis()>=10000 && wifi_ready()) {
        NativeRemoteStarted=true;
        const bool ok=mt7697::remote::start(millis());
        AddLog(ok?LOG_LEVEL_INFO:LOG_LEVEL_ERROR,PSTR("BLE: Remote receiver %s"),
               ok?"starting":"failed");
      }
      if (NativeRemoteStarted) {
        mt7697::remote::poll(millis());
        uint32_t code;
        // One light action per pass, preserving fairness for HTTP/Wi-Fi/OTA.
        if (mt7697::remote::action(code)) NativeRemoteAction(code);
        const auto s=mt7697::remote::status(millis());
        if (s.state!=NativeRemoteLastState) {
          NativeRemoteLastState=s.state;
          AddLog(LOG_LEVEL_INFO,PSTR("BLE: Remote state %u, devices %u, error %u"),
                 s.state,s.devices,s.last_error);
        }
      }
      break;
    case FUNC_COMMAND:
      return DecodeCommand(kNativeRemoteCommands,NativeRemoteCommands);
  }
  return false;
}
#endif
