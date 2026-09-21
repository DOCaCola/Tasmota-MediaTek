// SPDX-License-Identifier: GPL-3.0-or-later
#ifdef TASMOTA_PLATFORM_MT7697N
#include <platform/ntp.h>

// All socket and RTC operations run on the Arduino task. DNS uses the SDK's
// bounded blocking lookup; waiting for a UDP reply never blocks the scheduler.
static struct {
  WiFiUDP udp;
  IPAddress address;
  uint8_t token[8];
  uint8_t server = 0;
  uint32_t started = 0;
  uint32_t next = 0;
  bool pending = false;
} NativeNtp;

void NativeNtpClose(uint32_t delay_ms) {
  NativeNtp.udp.stop();
  NativeNtp.pending = false;
  NativeNtp.next = millis() + delay_ms;
}

void WifiPollNtp(void) {
  if (TasmotaGlobal.global_state.network_down || Rtc.user_time_entry) {
    if (NativeNtp.pending) { NativeNtpClose(0); }
    return;
  }
  if (NativeNtp.pending) {
    if (uint32_t(millis() - NativeNtp.started) >= 5000) {
      NativeNtpClose(60000);
      return;
    }
    // One datagram per loop bounds work even when unsolicited traffic arrives.
    int size = NativeNtp.udp.parsePacket();
    if (size >= 48 && NativeNtp.udp.remoteIP() == NativeNtp.address &&
        NativeNtp.udp.remotePort() == 123) {
      uint8_t packet[48];
      uint32_t seconds, nanos;
      if (NativeNtp.udp.read(packet, sizeof(packet)) == sizeof(packet) &&
          mt7697::ntp_decode(packet, sizeof(packet), NativeNtp.token, seconds, nanos)) {
        NativeNtpClose(3600000);
        Rtc.utc_time = seconds;
        Rtc.nanos = nanos;
        RtcSync("NTP");
      }
    }
    return;
  }
  if (!TasmotaGlobal.ntp_force_sync && int32_t(millis() - NativeNtp.next) < 0) {
    return;
  }
  TasmotaGlobal.ntp_force_sync = false;
  NativeNtp.next = millis() + 60000;
  const char* server = nullptr;
  for (unsigned i = 0; i < MAX_NTP_SERVERS; ++i) {
    server = SettingsText(SET_NTPSERVER1 + NativeNtp.server);
    NativeNtp.server = (NativeNtp.server + 1) % MAX_NTP_SERVERS;
    if (server[0]) { break; }
  }
  if (!server || !server[0] || !WifiHostByName(server, NativeNtp.address)) { return; }
  uint8_t packet[48] = {};
  packet[0] = 0x23;  // Version 4, client mode, clock not yet synchronized.
  packet[2] = 6;
  // Correlate replies, including before a wall clock exists. This is not
  // authentication; the native target currently uses ordinary unicast SNTP.
  for (unsigned i = 0; i < sizeof(NativeNtp.token); ++i) {
    NativeNtp.token[i] = random(1, 256);
  }
  memcpy(packet + 40, NativeNtp.token, sizeof(NativeNtp.token));
  if (!NativeNtp.udp.begin(0) ||
      !NativeNtp.udp.beginPacket(NativeNtp.address, 123) ||
      NativeNtp.udp.write(packet, sizeof(packet)) != sizeof(packet) ||
      !NativeNtp.udp.endPacket()) {
    NativeNtpClose(60000);
    return;
  }
  NativeNtp.started = millis();
  NativeNtp.pending = true;
}
#endif
