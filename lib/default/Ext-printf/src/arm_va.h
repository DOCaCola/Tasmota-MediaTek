// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

// AAPCS32 va_list has one pointer to the next argument in the register-save /
// stack argument area. Called immediately after consuming a four-byte value,
// as required by Ext-printf's existing argument-rewrite implementation.
inline void* ext_arm_previous_word(va_list& args) {
  static_assert(sizeof(va_list) == sizeof(uint32_t), "Requires AAPCS32 va_list");
  static_assert(sizeof(void*) == 4, "Requires a 32-bit pointer");
  uint8_t* next;
  memcpy(&next, &args, sizeof(next));
  return next - 4;
}

inline void ext_arm_skip_standard(va_list& args, char*& format) {
  bool wide = false;
  if (*format == 'l' || *format == 'h') {
    const char modifier = *format++;
    if (*format == modifier) { wide = modifier == 'l'; ++format; }
  } else if (*format == 'j' || *format == 'L') {
    wide = true;
    ++format;
  } else if (*format == 'z' || *format == 't') {
    ++format;
  }
  if (strchr("aAeEfFgG", *format)) {
    (void)va_arg(args, double);  // AAPCS32 aligns promoted doubles to eight bytes.
  } else if (wide && strchr("diouxX", *format)) {
    (void)va_arg(args, uint64_t);
  } else {
    (void)va_arg(args, uint32_t);
  }
}
