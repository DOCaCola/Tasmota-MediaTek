#pragma once
#include <stdint.h>
#define WIFI_PORT_STA 0
#define WIFI_MODE_STA_ONLY 1
#define WIFI_MODE_AP_ONLY 2
#define WIFI_MODE_REPEATER 3
#define WIFI_STATUS_LINK_CONNECTED 1
#define WIFI_AUTH_MODE_OPEN 0
#define WIFI_AUTH_MODE_WPA2_PSK 6
#define WIFI_ENCRYPT_TYPE_WEP_DISABLED 0
#define WIFI_ENCRYPT_TYPE_AES_ENABLED 3
#define ERR_OK 0
#define NETIF_TYPE_STA 0
struct ip_addr_t { uint32_t addr; };
extern const ip_addr_t zero_ip;
#define IP4_ADDR_ANY (&zero_ip)
#define ip4_addr_isany_val(ip) ((ip).addr == 0)
struct netif { ip_addr_t ip_addr; bool link, up, lease; };
typedef int sys_sem_t;
void init_global_connsys();
bool wifi_ready();
int wifi_config_set_radio(uint8_t);
int wifi_config_get_opmode(uint8_t*);
int wifi_config_set_opmode(uint8_t);
int wifi_connection_disconnect_ap();
int wifi_config_set_ssid(uint8_t,uint8_t*,uint8_t);
int wifi_config_set_security_mode(uint8_t,int,int);
int wifi_config_set_wpa_psk_key(uint8_t,uint8_t*,uint8_t);
int wifi_config_reload_setting();
int wifi_connection_get_link_status(uint8_t*);
netif* netif_find_by_type(int);
void netif_set_link_down(netif*);
void netif_set_link_up(netif*);
void netif_set_up(netif*);
void netif_set_default(netif*);
void netif_set_addr(netif*,const ip_addr_t*,const ip_addr_t*,const ip_addr_t*);
int netif_is_link_up(netif*);
int dhcp_start(netif*);
void dhcp_stop(netif*);
int dhcp_supplied_address(netif*);
int sys_sem_new(sys_sem_t*,int);
void sys_sem_free(sys_sem_t*);
void sys_sem_signal(sys_sem_t*);
void sys_arch_sem_wait(sys_sem_t*,int);
int tcpip_callback(void(*)(void*),void*);
