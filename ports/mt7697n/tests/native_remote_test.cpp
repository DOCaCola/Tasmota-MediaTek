// SPDX-License-Identifier: GPL-3.0-or-later
// Real SDK type declarations and production backend; only OS/radio/flash boundaries replaced.
#include "../platform/native_remote.cpp"
#include "remote_beacon_vectors.h"
#include <assert.h>
#include <deque>
#include <vector>
#include <stdio.h>
using namespace mt7697::remote;
struct FakeQueue { unsigned capacity,size; std::deque<std::vector<uint8_t>> data; };
static unsigned api_calls=0, writes=0, saves=0;
static bt_status_t api_result=BT_STATUS_SUCCESS;
static bool save_ok=true;
static std::vector<uint8_t> written;
static bt_status_t api() { ++api_calls; auto r=api_result; api_result=BT_STATUS_SUCCESS; return r; }
extern "C" {
QueueHandle_t xQueueCreate(unsigned n,unsigned size) { return new FakeQueue{n,size,{}}; }
int xQueueSend(QueueHandle_t q,const void* p,unsigned) {
  auto& f=*static_cast<FakeQueue*>(q);
  if (f.data.size()==f.capacity) return 0;
  f.data.emplace_back(static_cast<const uint8_t*>(p),static_cast<const uint8_t*>(p)+f.size); return 1;
}
int xQueueReceive(QueueHandle_t q,void* p,unsigned) {
  auto& f=*static_cast<FakeQueue*>(q);
  if (f.data.empty()) return 0;
  memcpy(p,f.data.front().data(),f.size); f.data.pop_front(); return 1;
}
void vQueueDelete(QueueHandle_t q) { delete static_cast<FakeQueue*>(q); }
int xTaskCreate(void (*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t* t) { *t=reinterpret_cast<void*>(1); return 1; }
void bt_task(void*) {}
int32_t wifi_config_get_mac_address(uint8_t port,uint8_t* p) {
  assert(port==WIFI_PORT_STA);
  const uint8_t mac[]={0x78,0x11,0xdc,0xab,0xae,0x7d}; memcpy(p,mac,6); return 0;
}
int hal_trng_init(void) { return 0; }
int hal_trng_get_generated_random_number(uint32_t* p) { *p=0x12345678; return 0; }
int hal_trng_deinit(void) { return 0; }
int nvdm_read_data_item(const char*,const char*,uint8_t*,uint32_t*) { return NVDM_STATUS_ITEM_NOT_FOUND; }
int nvdm_write_data_item(const char*,const char*,int,const uint8_t*,uint32_t) { ++saves; return save_ok?0:2; }
void bt_memory_init_packet(bt_memory_packet_t,char*,uint32_t) {}
void bt_memory_init_control_block(bt_memory_control_block_t,char*,uint32_t) {}
bt_status_t bt_gap_le_set_random_address(bt_bd_addr_ptr_t) { return api(); }
bt_status_t bt_gap_le_set_scan(const bt_hci_cmd_le_set_scan_enable_t* e,const bt_hci_cmd_le_set_scan_parameters_t*) {
  assert(e->filter_duplicates==BT_HCI_DISABLE); return api();
}
bt_status_t bt_gap_le_connect(const bt_hci_cmd_le_create_connection_t*) { return api(); }
bt_status_t bt_gap_le_cancel_connection(void) { return api(); }
bt_status_t bt_gap_le_disconnect(const bt_hci_cmd_disconnect_t*) { return api(); }
bt_status_t bt_gattc_discover_primary_service_by_uuid(bt_handle_t,const bt_gattc_discover_primary_service_by_uuid_req_t*) { return api(); }
bt_status_t bt_gattc_discover_charc(bt_handle_t,const bt_gattc_discover_charc_req_t*) { return api(); }
bt_status_t bt_gattc_discover_charc_descriptor(bt_handle_t,const bt_gattc_discover_charc_descriptor_req_t*) { return api(); }
bt_status_t bt_gattc_read_charc(bt_handle_t,const bt_gattc_read_charc_req_t* p) {
  assert(p->attribute_handle==key_handle); return api();
}
bt_status_t bt_gattc_write_charc(bt_handle_t,const bt_gattc_write_charc_req_t* p) {
  ++writes;
  const auto* data=reinterpret_cast<uint8_t*>(p->att_req);
  written.assign(data,data+3+p->attribute_value_length); return api();
}
void bt_uuid_load(bt_uuid_t* uuid,const void* p,uint8_t n) {
  memset(uuid,0,sizeof(*uuid)); memcpy(uuid,p,n);
}
}
static uint32_t now=10000;
static void event(uint32_t msg,uint32_t result=0,uint16_t handle=BT_HANDLE_INVALID,
                  std::initializer_list<uint8_t> data={}) {
  Event e={}; e.message=msg; e.status=result; e.handle=handle; e.length=data.size();
  std::copy(data.begin(),data.end(),e.data); assert(xQueueSend(controls,&e,0));
  poll(++now);
}
static void disconnected() {
  event(BT_GAP_LE_DISCONNECT_IND,0,connection);
  event(BT_GAP_LE_SET_SCAN_CNF); assert(state==Scanning);
}
static void pair_connect() {
  assert(pair(now,60));
  bt_gap_le_advertising_report_ind_t a={};
  memcpy(a.address.addr,beacon_vectors[0].mac,6);
  a.address.type=BT_ADDR_PUBLIC; a.event_type=BT_GAP_LE_ADV_REPORT_EVT_TYPE_ADV_IND;
  const uint8_t bytes[]={13,0x16,0x95,0xfe,0x40,0x30,0x53,1,1,2,0,2,1,0x10};
  memcpy(a.data,bytes,sizeof(bytes)); a.data_length=sizeof(bytes);
  bt_app_event_callback(BT_GAP_LE_ADVERTISING_REPORT_IND,0,&a);
  memset(&a,0,sizeof(a)); // Callback must own its copy.
  poll(++now); assert(state==ScanStopping);
  event(BT_GAP_LE_SET_SCAN_CNF); assert(state==Connecting);
}
static void begin_pair() {
  pair_connect();
  event(BT_GAP_LE_CONNECT_IND,0,0x123,{0,0x10,0x21,0x32,0x43,0x54,0x65});
  assert(state==Service);
  event(BT_GATTC_DISCOVER_PRIMARY_SERVICE_BY_UUID,0,0x123,{7,1,0,20,0});
  assert(state==Characteristics);
  event(BT_GATTC_DISCOVER_CHARC,BT_ATT_ERRCODE_CONTINUE,0x123,
        {9,7,2,0,0x18,3,0,1,0,5,0,8,6,0,0x10,0});
  assert(state==Characteristics);
  event(BT_GATTC_DISCOVER_CHARC,0,0x123,{9,7,7,0,2,8,0,0x14,0});
  assert(state==Descriptors && descriptor_end==4);
  event(BT_GATTC_DISCOVER_CHARC_DESCRIPTOR,0,0x123,{5,1,4,0,2,0x29});
  assert(state==Authenticating && cccd==4);
  assert(written==std::vector<uint8_t>({0x12,6,0,0x90,0xca,0x85,0xde}));
}
int main() {
  assert(start(now));
  const uint8_t address[]={0x7d,0xae,0xab,0xdc,0x11,0x78};
  assert(!memcmp(public_address,address,6));
  event(BT_POWER_ON_CNF); event(BT_GAP_LE_SET_SCAN_CNF);
  assert(state==Scanning && !pair(now,121));
  begin_pair();
  event(BT_GATTC_WRITE_CHARC,0,connection); // hello -> subscription
  assert(written==std::vector<uint8_t>({0x12,4,0,1,0}));
  event(BT_GATTC_WRITE_CHARC,0,connection); // subscription -> token
  uint8_t proof[12]; wrap_token(true,peer.mac,peer_pid,written.data()+3,proof);
  // Drive the actual pointer-bearing SDK callback, then discard its storage.
  uint8_t pdu[15]={0x1b,3,0}; memcpy(pdu+3,proof,12);
  bt_gatt_handle_value_notification_t notification={15,connection,reinterpret_cast<bt_att_handle_value_notification_t*>(pdu)};
  bt_app_event_callback(BT_GATTC_CHARC_VALUE_NOTIFICATION,0,&notification);
  memset(pdu,0,sizeof(pdu)); poll(++now);
  event(BT_GATTC_WRITE_CHARC,0,connection); // token completion releases early proof
  assert(written.size()==7);
  event(BT_GATTC_WRITE_CHARC,0,connection); // authenticated -> key read
  uint8_t read_pdu[13]={0x0b};
  rc4(authentication.token(),12,beacon_vectors[0].key,read_pdu+1,12);
  bt_gattc_read_rsp_t read={13,connection,reinterpret_cast<bt_att_read_rsp_t*>(read_pdu)};
  bt_app_event_callback(BT_GATTC_READ_CHARC,0,&read);
  memset(read_pdu,0,sizeof(read_pdu)); poll(++now);
  assert(state==Disconnecting && saves==1 && records.device[0].pid==0x153);
  assert(!memcmp(records.device[0].key,beacon_vectors[0].key,12));
  disconnected();
  now+=40000; poll(now); assert(state==Scanning); // No stale transaction timeout.
  // Nonconnectable remote packets, adjacent duplicate suppression and new sequence.
  for (unsigned i:{0u,0u,1u}) {
    const auto& v=beacon_vectors[i];
    bt_gap_le_advertising_report_ind_t a={};
    a.event_type=BT_GAP_LE_ADV_REPORT_EVT_TYPE_ADV_NONCONN_IND;
    memcpy(a.address.addr,v.mac,6); memcpy(a.data,v.ad,v.size); a.data_length=v.size;
    bt_app_event_callback(BT_GAP_LE_ADVERTISING_REPORT_IND,0,&a);
    poll(++now);
  }
  uint32_t code;
  assert(action(code) && code==0); assert(action(code) && code==1); assert(!action(code));
  assert(counters.duplicates==1);
  save_ok=false; assert(!forget(0)); assert(records.device[0].pid==0x153);
  save_ok=true; assert(forget(0)); assert(!records.device[0].pid);
  begin_pair();
  event(BT_GATTC_WRITE_CHARC,5,connection); assert(state==Disconnecting);
  disconnected();
  // Failed connect command must not retain a transaction timer.
  pair_connect(); event(BT_GAP_LE_CONNECT_CNF,7);
  event(BT_GAP_LE_SET_SCAN_CNF); now+=40000; poll(now); assert(state==Scanning);
  // Connect timeout -> cancellation; late success is explicitly disconnected.
  pair_connect(); now+=10001; poll(now); assert(state==Cancelling);
  event(BT_GAP_LE_CONNECT_IND,0,0x124,{0,0x10,0x21,0x32,0x43,0x54,0x65});
  assert(state==Disconnecting && connection==0x124);
  disconnected();
  // A malformed discovery record cannot reach authentication.
  pair_connect();
  event(BT_GAP_LE_CONNECT_IND,0,0x125,{0,0x10,0x21,0x32,0x43,0x54,0x65});
  event(BT_GATTC_DISCOVER_PRIMARY_SERVICE_BY_UUID,0,0x125,{7,1,0,20,0});
  event(BT_GATTC_DISCOVER_CHARC,0,0x125,{9,0,2,0});
  assert(state==Disconnecting && counters.last_error==BadEvent);
  disconnected();
  // An immediate API failure must unwind instead of waiting for a callback
  // the SDK will never send.
  begin_pair(); api_result=BT_STATUS_FAIL;
  event(BT_GATTC_WRITE_CHARC,0,connection);
  assert(state==Disconnecting); disconnected();
  // Authentication timeout is bounded even if the peer goes silent.
  begin_pair(); now+=5001; poll(now); assert(state==Disconnecting); disconnected();
  // Failure to save must not replace the trusted device table.
  begin_pair();
  event(BT_GATTC_WRITE_CHARC,0,connection);
  event(BT_GATTC_WRITE_CHARC,0,connection);
  wrap_token(true,peer.mac,peer_pid,written.data()+3,proof);
  event(BT_GATTC_WRITE_CHARC,0,connection);
  uint8_t proof_pdu[15]={0x1b,3,0}; memcpy(proof_pdu+3,proof,12);
  notification={15,connection,reinterpret_cast<bt_att_handle_value_notification_t*>(proof_pdu)};
  bt_app_event_callback(BT_GATTC_CHARC_VALUE_NOTIFICATION,0,&notification); poll(++now);
  event(BT_GATTC_WRITE_CHARC,0,connection);
  read_pdu[0]=0x0b; rc4(authentication.token(),12,beacon_vectors[0].key,read_pdu+1,12);
  read={13,connection,reinterpret_cast<bt_att_read_rsp_t*>(read_pdu)};
  save_ok=false;
  bt_app_event_callback(BT_GATTC_READ_CHARC,0,&read); poll(++now);
  assert(state==Disconnecting && counters.last_error==Storage && !records.device[0].pid);
  save_ok=true; disconnected();
  assert(pair(now,60));
  // Bounded queue overload is counted, never a blocking callback.
  Event e={}; e.message=BT_GAP_LE_CONNECT_CNF;
  for (unsigned i=0;i<8;++i) assert(xQueueSend(controls,&e,0));
  bt_app_event_callback(BT_POWER_ON_CNF,0,nullptr);
  assert(control_dropped==1);
  poll(++now); assert(state==Fault && counters.last_error==Overflow);
  puts("Native remote: async discovery, early notification ownership, key persistence, nonconnectable RX, duplicates and failures pass");
}
