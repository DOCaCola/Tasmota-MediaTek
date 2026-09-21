// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#define PSTR(s) s
#define LOG_LEVEL_INFO 2
void AddLog(int,const char*,...) {}
struct IPAddress { int value=0; };
char native_test_ssid[33]{},native_test_password[64]{};
bool native_test_accepted=false,native_test_pending=false;
struct {int wifi_test_counter=30;} Wifi;
struct Config {int sta_active=1;} settings;
Config* Settings=&settings;
enum {SET_STASSID1,SET_STAPWD1};
std::string saved[2]{"old-network","old-password"};
void SettingsUpdateText(int slot,const char* s) {saved[slot]=s;}
bool registration=true,configured=true,has_ip=false;
int starts=0;
bool NativeWifiRegisterFailureHandler() {return registration;}
bool WifiGetIP(IPAddress* address,bool exclude_ap) {assert(exclude_ap);address->value=has_ip?123:0;return has_ip;}
namespace mt7697 {
bool station_start(const char* ssid,const char* pass) {
  ++starts;assert(strcmp(ssid,native_test_ssid)==0);assert(strcmp(pass,native_test_password)==0);
  has_ip=false;return configured;
}
}
#include "wifi_trial_functions.inc"
int main() {
  IPAddress ip;
  has_ip=true; // a previous connection must not accept the proposed password
  NativeWifiTestBegin("new-network","wrong-password");
  assert(starts==0 && native_test_pending);
  assert(!NativeWifiTestHasIP(&ip) && starts==1);
  assert(!NativeWifiTestHasIP(&ip));
  NativeWifiTestDiscard();assert(!NativeWifiTestHasIP(&ip));
  assert(saved[0]=="old-network" && saved[1]=="old-password" && settings.sta_active==1);
  assert(native_test_password[0]==0);
  for (int i=0;i<2;++i) {
    NativeWifiTestBegin("new-network","bad");assert(!native_test_pending && Wifi.wifi_test_counter==1);
    assert(!NativeWifiTestHasIP(&ip));NativeWifiTestDiscard();
  }
  configured=false;NativeWifiTestBegin("new-network","password");
  assert(!NativeWifiTestHasIP(&ip));has_ip=true;assert(!NativeWifiTestHasIP(&ip));
  assert(saved[0]=="old-network");NativeWifiTestDiscard();configured=true;
  registration=false;NativeWifiTestBegin("new-network","password");
  assert(!native_test_pending);NativeWifiTestDiscard();registration=true;
  NativeWifiTestBegin("new-network","password");assert(!NativeWifiTestHasIP(&ip));
  has_ip=true;assert(NativeWifiTestHasIP(&ip));NativeWifiTestCommit();
  assert(saved[0]=="new-network" && saved[1]=="password" && settings.sta_active==0);
  assert(native_test_password[0]==0);
  NativeWifiTestBegin("open-network","");assert(!NativeWifiTestHasIP(&ip));
  has_ip=true;assert(NativeWifiTestHasIP(&ip));NativeWifiTestCommit();
  assert(saved[0]=="open-network" && saved[1].empty());
  puts("Wi-Fi trials: deferred start, failed/repeated trials preserve settings, fresh IP and slot-1 commit passed");
}
