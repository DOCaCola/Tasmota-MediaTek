#include <platform/ntp.h>
#include <assert.h>
#include <stdio.h>
#include <initializer_list>

static void word(uint8_t* p, uint32_t value) {
  for (int i = 3; i >= 0; --i) { p[i] = value; value >>= 8; }
}
int main() {
  uint8_t packet[48] = {}, token[8] = {1,2,3,4,5,6,7,8};
  packet[0] = 0x24;
  packet[1] = 1;
  memcpy(packet + 24, token, 8);
  word(packet + 40, 1700000000UL + 2208988800UL);
  word(packet + 44, 0x80000000);
  uint32_t seconds = 0, nanos = 0;
  assert(mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  assert(seconds == 1700000000 && nanos == 500000000);
  for (unsigned size = 0; size < 48; ++size) {
    assert(!mt7697::ntp_decode(packet, size, token, seconds, nanos));
  }
  const uint8_t bad_headers[] = {0xe4, 0x23, 0x14, 0x2c};
  for (uint8_t header : bad_headers) {
    packet[0] = header;
    assert(!mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  }
  packet[0] = 0x1c; // NTPv3 accepted.
  assert(mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  for (uint8_t stratum : {0,16,255}) {
    packet[1] = stratum;
    assert(!mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  }
  packet[1] = 15;
  packet[24] ^= 1;
  assert(!mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  packet[24] ^= 1;
  // Era rollover: 2036-02-07 plus one second.
  word(packet + 40, 1);
  assert(mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  assert(seconds == 2085978497UL);
  word(packet + 40, 0); word(packet + 44, 0);
  assert(!mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  word(packet + 40, 1451606400UL + 2208988800UL);
  assert(!mt7697::ntp_decode(packet, 48, token, seconds, nanos));
  seconds = 100; nanos = 750000000;
  mt7697::rtc_advance(10250, seconds, nanos);
  assert(seconds == 111 && nanos == 0);
  mt7697::rtc_advance(uint32_t(500 - 0xffffff00UL), seconds, nanos);
  assert(seconds == 111 && nanos == 756000000);
  puts("NTP validation and elapsed RTC tests passed");
}
