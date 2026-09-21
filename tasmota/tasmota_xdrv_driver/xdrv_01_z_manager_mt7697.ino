// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(TASMOTA_PLATFORM_MT7697N) && defined(USE_WEBSERVER)
extern "C" {
#include <dhcpd.h>
#include <ethernetif.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/sys.h>
}
#include <utility/wifi_drv.h>
#include <platform/wifi_startup.h>

bool native_manager_active = false;
uint32_t native_manager_tick = 0;

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
  const int mode=wifi_config_set_opmode(WIFI_MODE_STA_ONLY);
  const int radio=wifi_config_set_radio(0);
  if (mode<0 || radio<0)
    AddLog(LOG_LEVEL_ERROR,PSTR("WIF: Failed to stop setup AP; restart required"));
}
bool NativeWifiStartAP(const char* name, const char* passphrase, int channel) {
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: Radio init, reset %s, heap %u, stack %u"),
    mt7697::reset_reason_text(),mt7697::free_heap(),mt7697::stack_low_water_bytes());
  const bool first_init=!wifi_ready();
  if (first_init && !mt7697::prepare_setup_ap(name,passphrase?passphrase:"",channel)) return false;
  init_global_connsys();
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: Radio ready; configuring AP+STA"));
  const auto configure=[&]() {
    // Mode changes start the AP immediately, so the radio must be running.
    if (wifi_config_set_radio(1)<0 || wifi_config_set_opmode(WIFI_MODE_REPEATER)<0 ||
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
  AddLog(LOG_LEVEL_INFO,PSTR("WIF: AP+STA configured; starting IP services"));
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
  const size_t sl=strlen(ssid),pl=strlen(password);
  if (!sl || sl>32 || (pl && (pl<8 || pl>63))) {
    Wifi.wifi_test_counter=1;return;
  }
  for (size_t i=0;i<pl;++i) {
    if (static_cast<unsigned char>(password[i])<32 || static_cast<unsigned char>(password[i])>126) {
      Wifi.wifi_test_counter=1;return;
    }
  }
  int result=password[0] ? WiFiDrv::wifiSetPassphrase(ssid,sl,password,pl) : WiFiDrv::wifiSetNetwork(ssid,sl);
  if (result!=WL_SUCCESS) Wifi.wifi_test_counter=1;
}
void NativeWifiManagerPoll() {
  if (!native_manager_active) return;
  if (uint32_t(millis()-native_manager_tick)>=1000) {
    native_manager_tick=millis();
    if (Wifi.config_counter && --Wifi.config_counter==0) TasmotaGlobal.restart_flag=2;
  }
}
#endif
