// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(TASMOTA_PLATFORM_MT7697N) && defined(USE_WEBSERVER)
extern "C" {
#include <dhcpd.h>
#include <ethernetif.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/sys.h>
}
#include <platform/sdk_network.h>
#include <platform/wifi_startup.h>

bool native_manager_active = false;
uint32_t native_manager_tick = 0;
char native_test_ssid[33] = {};
char native_test_password[64] = {};
bool native_test_accepted = false;
bool native_test_pending = false;
char native_ap_name[33] = {};
char native_ap_password[64] = {};
int native_ap_channel = 1;
std::atomic<unsigned> native_connection_failure{0};
bool native_failure_handler_registered = false;

int32_t NativeWifiFailureEvent(wifi_event_t, uint8_t* payload, uint32_t length) {
  // Preserve the SDK's port/reason bytes; logging happens on the main task.
  if (length >= 3) {
    native_connection_failure.store(0x80000000u | unsigned(payload[0]) |
        (unsigned(payload[1]) << 8) | (unsigned(payload[2]) << 16));
  }
  return 0;
}
bool NativeWifiRegisterFailureHandler() {
  if (native_failure_handler_registered) return true;
  const int result=wifi_connection_register_event_handler(
      WIFI_EVENT_IOT_CONNECTION_FAILED,NativeWifiFailureEvent);
  if (result<0) {
    AddLog(LOG_LEVEL_ERROR,PSTR("WIF: Failure-event registration failed (%d)"),result);
    return false;
  }
  native_failure_handler_registered=true;
  return true;
}

String IPForUrl(const IPAddress& ip) { return ip.toString(); }
uint8_t WifiConfigCounter() {
  if (Wifi.config_counter) Wifi.config_counter = 180;
  return Wifi.config_counter;
}
struct NativeAPContext { netif* ap; sys_sem_t completed; };
void NativeAPNetif(void* context) {
  auto& request=*static_cast<NativeAPContext*>(context);
  auto* ap=request.ap;
  ip_addr_t ip, mask;
  IP4_ADDR(&ip,192,168,4,1); IP4_ADDR(&mask,255,255,255,0);
  netif_set_addr(ap,&ip,&mask,&ip);
  netif_set_up(ap);netif_set_link_up(ap);netif_set_default(ap);
  sys_sem_signal(&request.completed);
}
void NativeWifiAPCleanup() {
  // Removing the AP must leave the shared radio available to the station.
  const int mode=wifi_config_set_opmode(WIFI_MODE_STA_ONLY);
  if (mode<0)
    AddLog(LOG_LEVEL_ERROR,PSTR("WIF: Failed to stop setup AP; restart required"));
}
bool NativeWifiStartAP(const char* name, const char* passphrase, int channel) {
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: Radio init, reset %s, heap %u, stack %u"),
    mt7697::reset_reason_text(),mt7697::free_heap(),mt7697::stack_low_water_bytes());
  const bool first_init=!wifi_ready();
  if (first_init && !mt7697::prepare_setup_ap(name,passphrase?passphrase:"",channel)) return false;
  init_global_connsys();
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: Radio ready; configuring setup AP"));
  const auto configure=[&]() {
    // Mode changes start the AP immediately, so the radio must be running.
    // Idle setup must not restart the saved station connection on reload.
    // AP+STA is enabled only for an explicit credential trial.
    if (wifi_config_set_opmode(WIFI_MODE_AP_ONLY)<0 ||
        wifi_config_set_ssid(WIFI_PORT_AP,reinterpret_cast<uint8_t*>(const_cast<char*>(name)),strlen(name))<0 ||
        wifi_config_set_channel(WIFI_PORT_AP,channel)<0) return false;
    const bool secured=passphrase && passphrase[0];
    if (wifi_config_set_security_mode(WIFI_PORT_AP,
        secured?WIFI_AUTH_MODE_WPA2_PSK:WIFI_AUTH_MODE_OPEN,
        secured?WIFI_ENCRYPT_TYPE_AES_ENABLED:WIFI_ENCRYPT_TYPE_WEP_DISABLED)<0) return false;
    if (secured && wifi_config_set_wpa_psk_key(WIFI_PORT_AP,
        reinterpret_cast<uint8_t*>(const_cast<char*>(passphrase)),strlen(passphrase))<0) return false;
    return wifi_config_reload_setting()>=0;
  };
  if (!first_init && !configure()) { NativeWifiAPCleanup(); return false; }
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: AP-only configured; starting IP services"));
  NativeAPContext request={netif_find_by_type(NETIF_TYPE_AP),{}};
  if (!request.ap || sys_sem_new(&request.completed,0)!=ERR_OK) {
    NativeWifiAPCleanup(); return false;
  }
  if (tcpip_callback(NativeAPNetif,&request)!=ERR_OK) {
    sys_sem_free(&request.completed); NativeWifiAPCleanup(); return false;
  }
  // The callback owns this stack context until it signals completion.
  sys_arch_sem_wait(&request.completed,0);
  sys_sem_free(&request.completed);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  dhcpd_settings_t dhcp={};
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_server_address),"192.168.4.1");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_netmask),"255.255.255.0");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_gateway),"192.168.4.1");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_ip_pool_start),"192.168.4.2");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_ip_pool_end),"192.168.4.5");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_primary_dns),"192.168.4.1");
  strcpy(reinterpret_cast<char*>(dhcp.dhcpd_secondary_dns),"192.168.4.1");
  if (dhcpd_start(&dhcp)<0) { NativeWifiAPCleanup(); return false; }
  memmove(native_ap_name,name,strlen(name)+1);
  const char* password=passphrase?passphrase:"";
  memmove(native_ap_password,password,strlen(password)+1);
  native_ap_channel=channel;
  native_manager_active=true;
  native_manager_tick=millis();
  Wifi.config_counter=180;
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: Setup AP %s at http://192.168.4.1"),name);
  return true;
}
bool NativeWifiManagerActive() { return native_manager_active; }
void NativeWifiManagerStop() {
  if (!native_manager_active) return;
  StopWebserver();
  if (DnsServer) {DnsServer->stop();delete DnsServer;DnsServer=nullptr;}
  dhcpd_stop();
  NativeWifiAPCleanup();
  native_manager_active=false;
  Wifi.config_counter=0;
}
void NativeWifiTestBegin(const char* ssid,const char* password) {
  native_test_accepted=false;
  native_test_pending=false;
  memset(native_test_password,0,sizeof(native_test_password));
  const size_t sl=strlen(ssid),pl=strlen(password);
  if (!sl || sl>32 || (pl && (pl<8 || pl>63))) {
    Wifi.wifi_test_counter=1;return;
  }
  for (size_t i=0;i<pl;++i) {
    if (static_cast<unsigned char>(password[i])<32 || static_cast<unsigned char>(password[i])>126) {
      Wifi.wifi_test_counter=1;return;
    }
  }
  memcpy(native_test_ssid,ssid,sl+1);
  memcpy(native_test_password,password,pl+1);
  if (!NativeWifiRegisterFailureHandler()) {
    Wifi.wifi_test_counter=1;return;
  }
  // Finish the HTTP response before starting the station credential trial.
  native_test_pending=true;
}
bool NativeWifiTestHasIP(IPAddress* address) {
  if (native_test_pending) {
    native_test_pending=false;
    native_test_accepted=mt7697::station_start(native_test_ssid,native_test_password);
    AddLog(LOG_LEVEL_INFO,PSTR("WIF: Station configuration result %d"),native_test_accepted);
    if (!native_test_accepted) Wifi.wifi_test_counter=1;
    return false;
  }
  return native_test_accepted &&
      WifiGetIP(address,true);
}
void NativeWifiTestCommit() {
  Settings->sta_active=0;
  SettingsUpdateText(SET_STASSID1,native_test_ssid);
  SettingsUpdateText(SET_STAPWD1,native_test_password);
  memset(native_test_password,0,sizeof(native_test_password));
}
String NativeWifiTestSSID() { return String(native_test_ssid); }
void NativeWifiTestDiscard() {
  native_test_accepted=false;
  native_test_pending=false;
  memset(native_test_password,0,sizeof(native_test_password));
}
void NativeWifiTestRecoverAP() {
  // Remove the trial station and restore AP-only operation and IP services.
  const bool stopped=mt7697::station_stop();
  dhcpd_stop();
  if (!stopped || !NativeWifiStartAP(native_ap_name,native_ap_password,native_ap_channel)) {
    AddLog(LOG_LEVEL_ERROR,PSTR("WIF: Setup AP recovery failed; restart required"));
  }
}
void NativeWifiManagerPoll() {
  const unsigned failure=native_connection_failure.exchange(0);
  if (failure) {
    AddLog(LOG_LEVEL_ERROR,PSTR("WIF: Station failure port %u, reason bytes %02X %02X"),
      failure & 255,(failure >> 8)&255,(failure >> 16)&255);
  }
  if (!native_manager_active) return;
  if (uint32_t(millis()-native_manager_tick)>=1000) {
    native_manager_tick=millis();
    if (Wifi.config_counter && --Wifi.config_counter==0) TasmotaGlobal.restart_flag=2;
  }
}
#endif
