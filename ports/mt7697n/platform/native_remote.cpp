// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_remote.h"
#include "remote_protocol.h"
#include <atomic>
#include <string.h>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <task_def.h>
#include <hal_efuse.h>
#include <hal_trng.h>
#include <nvdm.h>
#include <bt_system.h>
#include <bt_gap_le.h>
#include <bt_gattc.h>
#include <bt_gatts.h>
void bt_task(void*);
}

namespace mt7697 { namespace remote {
namespace {
enum State : uint8_t { Off, Starting, Address, ScanStarting, Scanning, ScanStopping,
                      Connecting, Service, Characteristics, Descriptors,
                      Authenticating, Disconnecting, Cancelling, Fault };
enum : uint32_t { BadEvent=0xe001, Timeout=0xe002, Storage=0xe003,
                  Overflow=0xe004, Protocol=0xe005, Random=0xe006, NoHandles=0xe007 };
struct Advertisement {
  uint8_t mac[6], type, event, length, data[31];
  int8_t rssi;
};
struct Event {
  uint32_t message, status;
  uint16_t handle, length;
  uint8_t data[64];
};
struct Device {
  uint16_t pid;
  uint8_t mac[6], key[12];
};
static_assert(sizeof(Device)==20,"Persistent BLE record layout");
struct Store { uint32_t version; Device device[10]; };
Store records{};
uint8_t last_sequence[10]={};
bool sequence_valid[10]={};
State state=Off;
QueueHandle_t advertisements=nullptr, controls=nullptr;
TaskHandle_t task=nullptr;
std::atomic<uint32_t> reports{0}, dropped{0}, control_dropped{0};
Status counters{};
uint32_t deadline=0, pair_until=0, time_now=0, transaction_deadline=0;
uint32_t observed_control_drops=0;
bool window=false, pair_pending=false, write_pending=false;
uint16_t connection=BT_HANDLE_INVALID, service_start=0, service_end=0;
uint16_t auth_handle=0, hello_handle=0, key_handle=0, cccd=0, descriptor_end=0;
uint8_t auth_properties=0, hello_properties=0, key_properties=0;
Advertisement peer{};
uint16_t peer_pid=0;
Authentication authentication;
Op pending_op=Op::None;
uint8_t public_address[6], random_address[6];
bt_gap_le_local_key_t local_key{};
alignas(4) char tx_buffer[256], rx_buffer[1024];
alignas(4) char timer_buffer[10*BT_CONTROL_BLOCK_SIZE_OF_TIMER];
alignas(4) char connection_buffer[2*BT_CONTROL_BLOCK_SIZE_OF_LE_CONNECTION];
uint32_t action_queue[8]={}; unsigned action_in=0, action_out=0;

bool expired(uint32_t at) { return int32_t(time_now-at)>=0; }
bool random_bytes(void* target,size_t size) {
  if (hal_trng_init()!=HAL_TRNG_STATUS_OK) return false;
  auto* out=static_cast<uint8_t*>(target);
  bool ok=true;
  while (size) {
    uint32_t word;
    if (hal_trng_get_generated_random_number(&word)!=HAL_TRNG_STATUS_OK) { ok=false; break; }
    const size_t n=size>4?4:size; memcpy(out,&word,n); out+=n; size-=n;
  }
  hal_trng_deinit(); return ok;
}
void transition(State next,unsigned duration=5000) { state=next; deadline=time_now+duration; }
bool save(const Store& next) {
  return nvdm_write_data_item("Tasmota","ble_remotes",NVDM_DATA_ITEM_TYPE_RAW_DATA,
      reinterpret_cast<const uint8_t*>(&next),sizeof(next))==NVDM_STATUS_OK;
}
int slot(const uint8_t mac[6],uint16_t pid) {
  for (unsigned i=0;i<10;++i)
    if (records.device[i].pid==pid && !memcmp(records.device[i].mac,mac,6)) return i;
  return -1;
}
bool scan() {
  bt_hci_cmd_le_set_scan_enable_t enable={BT_HCI_ENABLE,BT_HCI_DISABLE};
  bt_hci_cmd_le_set_scan_parameters_t p={};
  p.le_scan_type=BT_HCI_SCAN_TYPE_PASSIVE;
  p.own_address_type=BT_HCI_SCAN_ADDR_RANDOM;
  p.scanning_filter_policy=BT_HCI_SCAN_FILTER_ACCEPT_ALL_ADVERTISING_PACKETS;
  p.le_scan_interval=0x24; p.le_scan_window=0x11;
  const auto result=bt_gap_le_set_scan(&enable,&p);
  if (result!=BT_STATUS_SUCCESS) { counters.last_error=result; state=Fault; return false; }
  transition(ScanStarting); return true;
}
void disconnect() {
  pair_pending=false;
  pending_op=Op::None; write_pending=false;
  authentication.abort();
  if (connection!=BT_HANDLE_INVALID) {
    bt_hci_cmd_disconnect_t p={}; p.connection_handle=connection; p.reason=0x13;
    const auto result=bt_gap_le_disconnect(&p);
    if (result!=BT_STATUS_SUCCESS) { counters.last_error=result; state=Fault; return; }
    transition(Disconnecting);
  } else if (state==Connecting) {
    const auto result=bt_gap_le_cancel_connection();
    if (result!=BT_STATUS_SUCCESS) { counters.last_error=result; state=Fault; return; }
    transition(Cancelling);
  } else {
    state=Fault;
  }
}
void fail(uint32_t error) {
  counters.last_error=error; ++counters.failures;
  window=false;
  pair_pending=false;
  disconnect();
}
bool accepted(bt_status_t result) {
  if (result==BT_STATUS_SUCCESS) return true;
  fail(result); return false;
}
bool gatt_write(uint16_t handle,const uint8_t* data,size_t size) {
  // One outstanding write; retain the PDU until its response arrives.
  static uint8_t pdu[15];
  pdu[0]=BT_ATT_OPCODE_WRITE_REQUEST; pdu[1]=handle; pdu[2]=handle>>8;
  memcpy(pdu+3,data,size);
  bt_gattc_write_charc_req_t p={};
  p.attribute_value_length=size; p.att_req=reinterpret_cast<bt_att_write_req_t*>(pdu);
  write_pending=true;
  return accepted(bt_gattc_write_charc(connection,&p));
}
void request(const Request& r) {
  if (r.op==Op::None) return;
  if (r.op==Op::Failed) { fail(Protocol); return; }
  if (r.op==Op::Done) {
    Store next=records;
    int index=slot(peer.mac,peer_pid);
    if (index<0) for (unsigned i=0;i<10;++i) if (!next.device[i].pid) { index=i; break; }
    if (index<0) { fail(Storage); return; }
    auto& d=next.device[index];
    d.pid=peer_pid; memcpy(d.mac,peer.mac,6); memcpy(d.key,authentication.key(),12);
    if (!save(next)) { fail(Storage); return; }
    records=next; sequence_valid[index]=false; ++counters.paired;
    window=false; disconnect(); return;
  }
  deadline=time_now+5000; pending_op=r.op;
  if (r.op==Op::Subscribe) {
    const uint8_t notify[]={1,0}; gatt_write(cccd,notify,2);
  } else if (r.op==Op::Write) {
    gatt_write(r.uuid==0x10?hello_handle:auth_handle,r.data,r.size);
  } else if (r.op==Op::Read) {
    bt_gattc_read_charc_req_t p={}; p.opcode=BT_ATT_OPCODE_READ_REQUEST;
    p.attribute_handle=key_handle; accepted(bt_gattc_read_charc(connection,&p));
  }
}
void connect() {
  service_start=service_end=auth_handle=hello_handle=key_handle=cccd=descriptor_end=0;
  auth_properties=hello_properties=key_properties=0;
  bt_hci_cmd_le_create_connection_t p={};
  p.le_scan_interval=0x10; p.le_scan_window=0x10;
  p.initiator_filter_policy=BT_HCI_CONN_FILTER_ASSIGNED_ADDRESS;
  p.own_address_type=BT_ADDR_RANDOM;
  p.peer_address.type=peer.type; memcpy(p.peer_address.addr,peer.mac,6);
  p.conn_interval_min=24; p.conn_interval_max=40;
  p.supervision_timeout=400; p.maximum_ce_length=0x50;
  transition(Connecting,10000); accepted(bt_gap_le_connect(&p));
}
void discover_characteristics() {
  transition(Characteristics);
  BT_GATTC_NEW_DISCOVER_CHARC_REQ(p,service_start,service_end);
  accepted(bt_gattc_discover_charc(connection,&p));
}
bool discovery_status(uint32_t s) {
  return s==BT_STATUS_SUCCESS || s==BT_ATT_ERRCODE_CONTINUE ||
         s==BT_ATT_ERRCODE_ATTRIBUTE_NOT_FOUND;
}
bool data_status(uint32_t s) {
  return s==BT_STATUS_SUCCESS || s==BT_ATT_ERRCODE_CONTINUE;
}
void control(const Event& e) {
  if (state==Starting && e.message==BT_POWER_ON_CNF) {
    if (e.status) { fail(e.status); return; }
    transition(Address); accepted(bt_gap_le_set_random_address(random_address)); return;
  }
  if (state==Address && e.message==BT_GAP_LE_SET_RANDOM_ADDRESS_CNF) {
    if (e.status) fail(e.status); else scan();
    return;
  }
  if (e.message==BT_GAP_LE_SET_SCAN_CNF &&
      (state==ScanStarting || state==ScanStopping)) {
    if (e.status) { fail(e.status); return; }
    if (state==ScanStopping) connect(); else state=Scanning;
    return;
  }
  if (e.message==BT_GAP_LE_CONNECT_CNF && state==Connecting) {
    if (e.status) {
      counters.last_error=e.status; ++counters.failures;
      pair_pending=false; window=false; scan();
    }
    return;
  }
  if (e.message==BT_GAP_LE_CONNECT_IND) {
    if (e.status) {
      if (state==Connecting || state==Cancelling) {
        ++counters.failures; counters.last_error=e.status;
        pair_pending=false; window=false; scan();
      }
      return;
    }
    // Late success during cancellation must be disconnected, never orphaned.
    connection=e.handle;
    if (state!=Connecting || e.length!=7 || e.data[0]!=peer.type ||
        memcmp(e.data+1,peer.mac,6)) { fail(BadEvent); return; }
    transition(Service);
    uint16_t uuid=0xfe95;
    BT_GATTC_NEW_DISCOVER_PRIMARY_SERVICE_BY_UUID16_REQ(p,uuid);
    accepted(bt_gattc_discover_primary_service_by_uuid(connection,&p)); return;
  }
  if (e.message==BT_GAP_LE_DISCONNECT_IND && e.handle==connection) {
    const bool expected=state==Disconnecting;
    connection=BT_HANDLE_INVALID; authentication.abort(); pending_op=Op::None; write_pending=false;
    pair_pending=false;
    if (!expected) {
      ++counters.failures; counters.last_error=e.status?e.status:BadEvent; window=false;
    }
    scan(); return;
  }
  if (e.message==BT_GAP_LE_CONNECT_CANCEL_CNF && state==Cancelling) {
    pair_pending=false;
    if (e.status) { state=Fault; counters.last_error=e.status; } else scan();
    return;
  }
  if (connection==BT_HANDLE_INVALID || e.handle!=connection) return;
  if (state==Service && e.message==BT_GATTC_DISCOVER_PRIMARY_SERVICE_BY_UUID) {
    if (!discovery_status(e.status)) { fail(e.status); return; }
    if (data_status(e.status) && e.length) {
      if (e.length<5 || (e.length-1)%4) { fail(BadEvent); return; }
      if (service_start) { fail(BadEvent); return; }
      service_start=le16(e.data+1); service_end=le16(e.data+3);
      if (!service_start || service_end<=service_start) { fail(BadEvent); return; }
    }
    if (e.status!=BT_ATT_ERRCODE_CONTINUE) {
      if (!service_start) fail(NoHandles); else discover_characteristics();
    }
  } else if (state==Characteristics && e.message==BT_GATTC_DISCOVER_CHARC) {
    if (!discovery_status(e.status)) { fail(e.status); return; }
    if (data_status(e.status) && e.length) {
      if (e.length<2) { fail(BadEvent); return; }
      const unsigned stride=e.data[1];
      if ((stride!=7 && stride!=21) || (e.length-2)%stride) { fail(BadEvent); return; }
      for (unsigned p=2;p<e.length;p+=stride) {
        const uint16_t declaration=le16(e.data+p), value=le16(e.data+p+3);
        if (declaration<service_start || value<=declaration || value>service_end) { fail(BadEvent); return; }
        if (auth_handle && declaration>auth_handle && declaration-1<descriptor_end)
          descriptor_end=declaration-1;
        const uint16_t uuid=stride==7?le16(e.data+p+5):0xffff;
        if (uuid==1) { auth_handle=value; auth_properties=e.data[p+2]; descriptor_end=service_end; }
        if (uuid==0x10) { hello_handle=value; hello_properties=e.data[p+2]; }
        if (uuid==0x14) { key_handle=value; key_properties=e.data[p+2]; }
      }
    }
    if (e.status!=BT_ATT_ERRCODE_CONTINUE) {
      if (!auth_handle || !hello_handle || !key_handle || descriptor_end<=auth_handle ||
          (auth_properties&0x18)!=0x18 || !(hello_properties&8) || !(key_properties&2)) {
        fail(NoHandles); return;
      }
      transition(Descriptors);
      bt_gattc_discover_charc_descriptor_req_t p={};
      p.opcode=BT_ATT_OPCODE_FIND_INFORMATION_REQUEST;
      p.starting_handle=auth_handle+1; p.ending_handle=descriptor_end;
      accepted(bt_gattc_discover_charc_descriptor(connection,&p));
    }
  } else if (state==Descriptors && e.message==BT_GATTC_DISCOVER_CHARC_DESCRIPTOR) {
    if (!discovery_status(e.status)) { fail(e.status); return; }
    if (data_status(e.status) && e.length) {
      if (e.length<2 || (e.data[1]!=1 && e.data[1]!=2)) { fail(BadEvent); return; }
      const unsigned stride=e.data[1]==1?4:18;
      if ((e.length-2)%stride) { fail(BadEvent); return; }
      for (unsigned p=2;p<e.length;p+=stride)
        if (stride==4 && le16(e.data+p+2)==0x2902) {
          const uint16_t h=le16(e.data+p);
          if (h<=auth_handle || h>descriptor_end || cccd) { fail(BadEvent); return; }
          cccd=h;
        }
    }
    if (e.status!=BT_ATT_ERRCODE_CONTINUE) {
      if (!cccd) { fail(NoHandles); return; }
      uint8_t token[12];
      if (!random_bytes(token,sizeof(token))) { fail(Random); return; }
      transition(Authenticating);
      request(authentication.begin(peer.mac,peer_pid,token,false));
    }
  } else if (state==Authenticating) {
    if (e.message==BT_GATTC_WRITE_CHARC && write_pending) {
      write_pending=false; pending_op=Op::None;
      request(authentication.complete(e.status==BT_STATUS_SUCCESS));
    } else if (e.message==BT_GATTC_READ_CHARC && pending_op==Op::Read) {
      pending_op=Op::None;
      request(authentication.complete(e.status==BT_STATUS_SUCCESS && e.length==13,
                                      e.data+1,e.length?e.length-1:0));
    } else if (e.message==BT_GATTC_CHARC_VALUE_NOTIFICATION) {
      if (e.status || e.length<3 || le16(e.data+1)!=auth_handle) { fail(BadEvent); return; }
      request(authentication.notification(e.data+3,e.length-3));
    }
  }
}
void advertisement(const Advertisement& a) {
  Beacon b;
  if (!parse(a.data,a.length,a.mac,b)) { ++counters.invalid; return; }
  counters.last_pid=b.pid; counters.last_rssi=a.rssi;
  const int index=slot(b.mac,b.pid);
  if (index<0) {
    if (state==Scanning && window && !expired(pair_until) &&
        a.event==BT_GAP_LE_ADV_REPORT_EVT_TYPE_ADV_IND && pairing_request(b)) {
      bool room=false;
      for (const auto& d:records.device) if (!d.pid) room=true;
      if (!room) { counters.last_error=Storage; return; }
      peer=a; peer_pid=b.pid; pair_pending=true;
      transaction_deadline=time_now+30000;
      bt_hci_cmd_le_set_scan_enable_t p={BT_HCI_DISABLE,BT_HCI_DISABLE};
      transition(ScanStopping); accepted(bt_gap_le_set_scan(&p,nullptr));
    }
    return;
  }
  uint8_t plain[24];
  const size_t n=decrypt(b,records.device[index].key,plain);
  if (!n) { ++counters.invalid; return; }
  ++counters.decrypted;
  if (sequence_valid[index] && last_sequence[index]==b.sequence) { ++counters.duplicates; return; }
  // Reserve output capacity before consuming the sequence; a retransmission
  // can then recover from a full queue.
  if (action_in-action_out>=8) { ++dropped; return; }
  for (size_t p=0;p<n;p+=3+plain[p+2]) {
    if (le16(plain+p)==0x1001 && plain[p+2]==3) {
      const uint32_t code=plain[p+3] | (uint32_t(plain[p+4])<<8) | (uint32_t(plain[p+5])<<16);
      action_queue[action_in++%8]=code; counters.last_code=code; ++counters.actions;
      last_sequence[index]=b.sequence; sequence_valid[index]=true;
      return;
    }
  }
}
} // namespace

bool start(uint32_t now) {
  if (state!=Off) return state!=Fault;
  time_now=now;
  uint32_t size=sizeof(records);
  const auto loaded=nvdm_read_data_item("Tasmota","ble_remotes",
      reinterpret_cast<uint8_t*>(&records),&size);
  if (loaded==NVDM_STATUS_ITEM_NOT_FOUND) { records={}; records.version=1; }
  else if (loaded!=NVDM_STATUS_OK || size!=sizeof(records) || records.version!=1) {
    counters.last_error=Storage; state=Fault; return false;
  }
  for (const auto& d:records.device)
    if (d.pid && d.pid!=0x153 && d.pid!=0x3b6) { counters.last_error=Storage; state=Fault; return false; }
  if (hal_efuse_read(0x1a,public_address,6)!=HAL_EFUSE_OK ||
      !random_bytes(random_address,6) || !random_bytes(&local_key,sizeof(local_key))) {
    state=Fault; counters.last_error=Random; return false;
  }
  random_address[5]|=0xc0; // Bluetooth static random address, not a public MAC.
  uint8_t any=0, all=0xff;
  for (auto v:public_address) { any|=v; all&=v; }
  if (!any || all==0xff) { state=Fault; counters.last_error=BadEvent; return false; }
  advertisements=xQueueCreate(16,sizeof(Advertisement)); controls=xQueueCreate(8,sizeof(Event));
  if (!advertisements || !controls) {
    if (advertisements) vQueueDelete(advertisements);
    if (controls) vQueueDelete(controls);
    advertisements=controls=nullptr; state=Fault; counters.last_error=Overflow; return false;
  }
  bt_memory_init_packet(BT_MEMORY_TX_BUFFER,tx_buffer,sizeof(tx_buffer));
  bt_memory_init_packet(BT_MEMORY_RX_BUFFER,rx_buffer,sizeof(rx_buffer));
  bt_memory_init_control_block(BT_MEMORY_CONTROL_BLOCK_TIMER,timer_buffer,sizeof(timer_buffer));
  bt_memory_init_control_block(BT_MEMORY_CONTROL_BLOCK_LE_CONNECTION,connection_buffer,sizeof(connection_buffer));
  transition(Starting);
  if (xTaskCreate(bt_task,BLUETOOTH_TASK_NAME,BLUETOOTH_TASK_STACKSIZE/sizeof(StackType_t),
                  public_address,BLUETOOTH_TASK_PRIO,&task)!=pdPASS) {
    vQueueDelete(advertisements); vQueueDelete(controls);
    advertisements=controls=nullptr;
    state=Fault; counters.last_error=Overflow; return false;
  }
  return true;
}
void poll(uint32_t now) {
  time_now=now;
  if (state==Off) return;
  if (window && expired(pair_until)) window=false;
  if (!controls) return;
  // Process control traffic first, with a fixed work budget per main-loop pass.
  if (observed_control_drops!=control_dropped.load()) {
    observed_control_drops=control_dropped.load(); fail(Overflow);
  }
  Event e;
  for (unsigned i=0;i<8 && xQueueReceive(controls,&e,0)==pdTRUE;++i) control(e);
  Advertisement a;
  for (unsigned i=0;i<4 && xQueueReceive(advertisements,&a,0)==pdTRUE;++i) advertisement(a);
  if (state!=Scanning && state!=Fault && expired(deadline)) {
    if (state==Disconnecting || state==Cancelling) { state=Fault; counters.last_error=Timeout; }
    else fail(Timeout);
  }
  if (pair_pending && expired(transaction_deadline) && state!=Disconnecting && state!=Cancelling)
    fail(Timeout);
}
bool pair(uint32_t now,unsigned seconds) {
  if (seconds>120 || state!=Scanning) return false;
  time_now=now; window=seconds!=0; pair_until=now+seconds*1000; return true;
}
bool forget(unsigned index) {
  if (index>10 || state!=Scanning) return false;
  Store next=records;
  if (!index) memset(next.device,0,sizeof(next.device));
  else next.device[index-1]={};
  if (!save(next)) { counters.last_error=Storage; return false; }
  records=next; memset(sequence_valid,0,sizeof(sequence_valid)); return true;
}
Status status(uint32_t now) {
  auto s=counters; s.state=state; s.reports=reports.load(); s.dropped=dropped.load();
  s.control_dropped=control_dropped.load();
  for (const auto& d:records.device) if (d.pid) ++s.devices;
  s.pairing_seconds=window && int32_t(pair_until-now)>0?(pair_until-now+999)/1000:0;
  return s;
}
bool device(unsigned index,uint16_t& pid,uint8_t mac[6]) {
  if (index>=10 || !records.device[index].pid) return false;
  pid=records.device[index].pid; memcpy(mac,records.device[index].mac,6); return true;
}
bool action(uint32_t& code) {
  if (action_out==action_in) return false;
  code=action_queue[action_out++%8]; return true;
}
} }

extern "C" bt_status_t bt_app_event_callback(bt_msg_type_t message,bt_status_t status,void* buffer) {
  using namespace mt7697::remote;
  if (message==BT_GAP_LE_ADVERTISING_REPORT_IND) {
    ++reports;
    if (status || !buffer || !advertisements) return BT_STATUS_SUCCESS;
    const auto& a=*static_cast<bt_gap_le_advertising_report_ind_t*>(buffer);
    if (a.data_length>31) { ++dropped; return BT_STATUS_SUCCESS; }
    // Filter unrelated service data before consuming queue capacity.
    Beacon b;
    if (!parse(a.data,a.data_length,a.address.addr,b)) return BT_STATUS_SUCCESS;
    Advertisement copy={};
    memcpy(copy.mac,a.address.addr,6); copy.type=a.address.type; copy.event=a.event_type;
    copy.length=a.data_length; copy.rssi=a.rssi; memcpy(copy.data,a.data,a.data_length);
    if (xQueueSend(advertisements,&copy,0)!=pdTRUE) ++dropped;
    return BT_STATUS_SUCCESS;
  }
  Event e={}; e.message=message; e.status=status; e.handle=BT_HANDLE_INVALID;
  const uint8_t* payload=nullptr;
  if (message==BT_GAP_LE_CONNECT_IND && !status && buffer) {
    const auto& c=*static_cast<bt_gap_le_connection_ind_t*>(buffer);
    e.handle=c.connection_handle; e.data[0]=c.peer_addr.type;
    memcpy(e.data+1,c.peer_addr.addr,6); e.length=7;
  } else if (message==BT_GAP_LE_DISCONNECT_IND && buffer) {
    e.handle=static_cast<bt_gap_le_disconnect_ind_t*>(buffer)->connection_handle;
  } else if (message==BT_GATTC_WRITE_CHARC && buffer) {
    e.handle=static_cast<bt_gattc_write_rsp_t*>(buffer)->connection_handle;
  } else if (message==BT_GATTC_DISCOVER_PRIMARY_SERVICE_BY_UUID ||
             message==BT_GATTC_DISCOVER_CHARC || message==BT_GATTC_DISCOVER_CHARC_DESCRIPTOR ||
             message==BT_GATTC_READ_CHARC || message==BT_GATTC_CHARC_VALUE_NOTIFICATION) {
    // These SDK response types share the packed (length,handle,pointer) prefix.
    // Copy each field rather than aliasing an unrelated response structure.
    if (buffer) {
      memcpy(&e.length,buffer,2);
      memcpy(&e.handle,static_cast<uint8_t*>(buffer)+2,2);
      memcpy(&payload,static_cast<uint8_t*>(buffer)+4,sizeof(payload));
      if (status!=BT_STATUS_SUCCESS && status!=BT_ATT_ERRCODE_CONTINUE) e.length=0;
      if (e.length>sizeof(e.data) || (e.length && !payload)) {
        e.status=BadEvent; e.length=0;
      }
      if (e.length) memcpy(e.data,payload,e.length);
    } else { e.status=BadEvent; }
  } else if (message!=BT_POWER_ON_CNF && message!=BT_GAP_LE_SET_RANDOM_ADDRESS_CNF &&
             message!=BT_GAP_LE_SET_SCAN_CNF && message!=BT_GAP_LE_CONNECT_CNF &&
             message!=BT_GAP_LE_CONNECT_CANCEL_CNF) {
    return BT_STATUS_SUCCESS;
  }
  if (controls && xQueueSend(controls,&e,0)!=pdTRUE) ++control_dropped;
  return BT_STATUS_SUCCESS;
}
extern "C" bt_gap_le_local_config_req_ind_t* bt_gap_le_get_local_config(void) {
  static bt_gap_le_local_config_req_ind_t config={&mt7697::remote::local_key,false};
  return &config;
}
extern "C" bt_gap_le_local_key_t* bt_gap_le_get_local_key(void) {
  return &mt7697::remote::local_key;
}
extern "C" bt_gap_le_bonding_info_t* bt_gap_le_get_bonding_info(const bt_addr_t) {
  static bt_gap_le_bonding_info_t info={}; return &info;
}
extern "C" bt_status_t bt_gap_le_get_pairing_config(bt_gap_le_bonding_start_ind_t*) {
  // FE95 uses application authentication. Do not enable unrelated SMP bonding.
  return BT_STATUS_FAIL;
}
extern "C" bool bt_gap_le_is_connection_update_request_accepted(bt_handle_t,bt_gap_le_connection_update_param_t*) {
  return true;
}
extern "C" const bt_gatts_service_t** bt_get_gatt_server(void) {
  // Central-only application, with no local attributes or provisioning service.
  static const bt_gatts_service_t* services[]={nullptr}; return services;
}
