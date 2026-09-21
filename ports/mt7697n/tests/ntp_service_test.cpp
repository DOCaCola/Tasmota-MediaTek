// Exercise the production main-loop service without sockets or a wall clock.
#include <platform/ntp.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

#define TASMOTA_PLATFORM_MT7697N
#define MAX_NTP_SERVERS 3
#define SET_NTPSERVER1 0
using IPAddress = uint32_t;
static uint32_t now = 100;
uint32_t millis() { return now; }
long random(long low, long) { return low; }
static bool dns_ok = true;
static unsigned dns_calls;
const char* SettingsText(unsigned i) {
  static const char* servers[] = {"first", "", "third"};
  return servers[i];
}
bool WifiHostByName(const char*, IPAddress& address) {
  ++dns_calls; address = 42; return dns_ok;
}
struct {
  struct { bool network_down = false; } global_state;
  bool ntp_force_sync = false;
} TasmotaGlobal;
struct { bool user_time_entry = false; uint32_t utc_time = 0, nanos = 0; } Rtc;
static unsigned syncs;
void RtcSync(const char*) { ++syncs; }
class WiFiUDP {
 public:
  bool begin_ok = true, packet_ok = true, send_ok = true, write_ok = true;
  bool open = false;
  unsigned closes = 0, sends = 0;
  IPAddress source = 42;
  unsigned port = 123;
  std::vector<uint8_t> rx, tx;
  int begin(unsigned) { open = begin_ok; return begin_ok; }
  int beginPacket(IPAddress, unsigned) { tx.clear(); return packet_ok; }
  size_t write(const uint8_t* data, size_t size) {
    tx.assign(data, data + size); return write_ok ? size : 0;
  }
  int endPacket() { ++sends; return send_ok; }
  void stop() { ++closes; open = false; rx.clear(); }
  int parsePacket() { return rx.size(); }
  IPAddress remoteIP() { return source; }
  unsigned remotePort() { return port; }
  size_t read(uint8_t* data, size_t size) {
    assert(size <= rx.size()); memcpy(data, rx.data(), size); rx.clear(); return size;
  }
};
#include "../../../tasmota/tasmota_support/support_time_mt7697.ino"

static void reset() {
  NativeNtp = {};
  TasmotaGlobal = {};
  Rtc = {};
  syncs = dns_calls = 0;
  dns_ok = true;
  now = 100;
}
static void reply() {
  auto& packet = NativeNtp.udp.rx;
  packet.assign(48, 0);
  packet[0] = 0x24; packet[1] = 1;
  memcpy(packet.data() + 24, NativeNtp.token, 8);
  uint32_t value = 1700000000UL + 2208988800UL;
  for (int i = 43; i >= 40; --i) { packet[i] = value; value >>= 8; }
}
int main() {
  reset();
  TasmotaGlobal.global_state.network_down = true;
  WifiPollNtp(); assert(!dns_calls);
  TasmotaGlobal.global_state.network_down = false;
  Rtc.user_time_entry = true;
  WifiPollNtp(); assert(!dns_calls);
  Rtc.user_time_entry = false;
  WifiPollNtp();
  assert(NativeNtp.pending && NativeNtp.udp.open && NativeNtp.udp.sends == 1);
  assert(NativeNtp.udp.tx.size() == 48 && NativeNtp.udp.tx[0] == 0x23);
  reply(); NativeNtp.udp.source = 99;
  WifiPollNtp(); assert(!syncs && NativeNtp.pending);
  NativeNtp.udp.source = 42; NativeNtp.udp.port = 80;
  WifiPollNtp(); assert(!syncs);
  NativeNtp.udp.port = 123; NativeNtp.udp.rx[24] ^= 1;
  WifiPollNtp(); assert(!syncs);
  reply(); WifiPollNtp();
  assert(syncs == 1 && Rtc.utc_time == 1700000000UL);
  assert(!NativeNtp.pending && !NativeNtp.udp.open);
  now += 3599999;
  WifiPollNtp(); assert(NativeNtp.udp.sends == 1);
  now++;
  WifiPollNtp(); assert(NativeNtp.udp.sends == 2 && NativeNtp.server == 0);
  TasmotaGlobal.global_state.network_down = true;
  WifiPollNtp(); assert(!NativeNtp.pending && !NativeNtp.udp.open);

  // Every socket failure closes resources and applies retry throttling.
  for (unsigned failure = 0; failure < 4; ++failure) {
    reset();
    if (failure == 0) NativeNtp.udp.begin_ok = false;
    if (failure == 1) NativeNtp.udp.packet_ok = false;
    if (failure == 2) NativeNtp.udp.write_ok = false;
    if (failure == 3) NativeNtp.udp.send_ok = false;
    WifiPollNtp();
    assert(!NativeNtp.pending && !NativeNtp.udp.open && NativeNtp.udp.closes == 1);
    WifiPollNtp(); assert(dns_calls == 1);
  }
  reset(); dns_ok = false;
  WifiPollNtp(); WifiPollNtp(); assert(dns_calls == 1);
  now += 60000; dns_ok = true;
  WifiPollNtp(); assert(NativeNtp.pending);
  Rtc.user_time_entry = true;
  WifiPollNtp(); assert(!NativeNtp.pending && !NativeNtp.udp.open);

  reset(); now = 0xfffffff0;
  TasmotaGlobal.ntp_force_sync = true;
  WifiPollNtp(); assert(NativeNtp.pending);
  now += 5000;
  reply(); WifiPollNtp(); // Expired replies cannot set time.
  assert(!syncs && !NativeNtp.pending && !NativeNtp.udp.open);
  now += 59999; WifiPollNtp(); assert(NativeNtp.udp.sends == 1);
  now++; WifiPollNtp(); assert(NativeNtp.udp.sends == 2);
  puts("Native NTP service tests passed");
}
