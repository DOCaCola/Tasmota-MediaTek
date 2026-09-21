// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace mt7697 {
inline uint32_t ntp_word(const uint8_t* p) {
  return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
         uint32_t(p[2]) << 8 | p[3];
}

// Validate a server reply against the request's transmit timestamp.
// The caller additionally checks the source address and UDP port.
inline bool ntp_decode(const uint8_t* packet, size_t size,
                       const uint8_t* token, uint32_t& seconds, uint32_t& nanos) {
  if (size < 48 || (packet[0] & 7) != 4 || (packet[0] >> 6) == 3 ||
      ((packet[0] >> 3) & 7) < 3 || ((packet[0] >> 3) & 7) > 4 ||
      packet[1] == 0 || packet[1] > 15 || memcmp(packet + 24, token, 8)) {
    return false;
  }
  const uint32_t whole = ntp_word(packet + 40);
  const uint32_t fraction = ntp_word(packet + 44);
  if (!(whole | fraction)) { return false; }
  // Unsigned subtraction unfolds the 2036 NTP rollover into the Unix era.
  // Tasmota's uint32_t clock ends in 2106; accept only its valid date range.
  const uint32_t unix_time = whole - 2208988800UL;
  if (unix_time <= 1451606400UL) { return false; }
  seconds = unix_time;
  nanos = (uint64_t(fraction) * 1000000000ULL) >> 32;
  return true;
}

inline void rtc_advance(uint32_t elapsed_ms, uint32_t& seconds, uint32_t& nanos) {
  const uint64_t elapsed = uint64_t(elapsed_ms) * 1000000ULL + nanos;
  seconds += elapsed / 1000000000ULL;
  nanos = elapsed % 1000000000ULL;
}
}  // namespace mt7697
