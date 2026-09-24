// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>
#include <vector>
#include <platform/native_remote.h>
#define TASMOTA_PLATFORM_MT7697N
#define USE_YLXD01YL_LIGHT
#define PROGMEM
#define PSTR(x) x
using std::max;
template<class T> T constrain(T x,T lo,T hi) { return std::max(lo,std::min(x,hi)); }
static size_t TestStrlcpy(char* p,const char* s,size_t n) { snprintf(p,n,"%s",s); return strlen(s); }
enum {SRC_BUTTON,LOG_LEVEL_INFO,LOG_LEVEL_ERROR,LOG_LEVEL_DEBUG,FUNC_LOOP,FUNC_COMMAND};
struct { bool lamp_night=false; } settings;
auto* Settings=&settings;
struct { int data_len=0,payload=0; } XdrvMailbox;
struct { bool fade_once_enabled=false,fade_once_value=false,speed_once_enabled=false; int speed_once_value=0; } Light;
struct { int ct=250; int getCT(){return ct;} } light_state;
uint32_t millis() { return 0; }
bool wifi_ready() { return false; }
void getCTRange(uint16_t* lo,uint16_t* hi) { *lo=154; *hi=370; }
template<class... T> void AddLog(T...) {}
template<class... T> void Response_P(T...) {}
template<class... T> void ResponseAppend_P(T...) {}
void ResponseCmndChar(const char*) {}
bool DecodeCommand(const char*,void(*const*)()) { return false; }
std::vector<std::string> commands;
void ExecuteCommand(char* s,int source) {
  assert(source==SRC_BUTTON); commands.push_back(s);
  if (!strcmp(s,"LampNight 1")) Settings->lamp_night=true;
  if (!strcmp(s,"LampNight 0")) Settings->lamp_night=false;
}
namespace mt7697 { namespace remote {
bool start(uint32_t) { assert(false); return false; }
void poll(uint32_t) {}
bool pair(uint32_t,unsigned) { return false; }
bool forget(unsigned) { return false; }
Status status(uint32_t) { return {}; }
bool device(unsigned,uint16_t&,uint8_t[6]) { return false; }
bool action(uint32_t&) { return false; }
} }
// Bind the driver to the test helper without redeclaring a host libc symbol.
#define strlcpy TestStrlcpy
#include "../../../tasmota/tasmota_xdrv_driver/xdrv_95_mt7697_remote.ino"
#undef strlcpy

int main() {
  const std::pair<uint32_t,const char*> cases[]={{0,"Power ON"},{1,"Power OFF"},
    {7,"Power TOGGLE"},{3,"Dimmer +"},{5,"Dimmer -"},
    {0x020003,"Dimmer 100"},{0x020005,"Dimmer 1"}};
  for (auto c:cases) {
    commands.clear(); NativeRemoteAction(c.first);
    assert(commands.size()==1 && commands[0]==c.second);
    assert(Light.fade_once_enabled && Light.fade_once_value &&
           Light.speed_once_enabled && Light.speed_once_value==1);
  }
  commands.clear(); NativeRemoteAction(4);
  assert(Settings->lamp_night && commands.back()=="LampNight 1");
  commands.clear(); NativeRemoteAction(2); assert(commands.empty());
  NativeRemoteAction(4); assert(!Settings->lamp_night);
  commands.clear(); NativeRemoteAction(0x020004);
  assert((commands==std::vector<std::string>{"LampNight 1","Dimmer 1","Power ON"}));
  Settings->lamp_night=false;
  light_state.ct=370; commands.clear(); NativeRemoteAction(2);
  assert(commands.back()=="CT 349");
  light_state.ct=154; NativeRemoteAction(2); assert(commands.back()=="CT 175");
  const auto n=commands.size(); NativeRemoteAction(6); NativeRemoteAction(0x020001);
  assert(commands.size()==n && NativeRemoteUnsupported==2); // No factory reset.
  Xdrv95(FUNC_LOOP); assert(!NativeRemoteStarted); // No radio init before Wi-Fi.
  puts("Remote lamp mappings: day/night, power, brightness, CT endpoints, 500 ms and unsupported actions pass");
}
