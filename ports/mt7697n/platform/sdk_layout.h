// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
namespace mt7697 {
// Official MediaTek commit 53acd43fbee57b034141068969fa643465dfd743.
// Unlike the BSP 0.10.21 loader, this recognizes GD25Q32CSIG as 4 MiB.
constexpr uint32_t kBootloaderBytes = 27232;
constexpr uint8_t kBootloaderSha1[20] = {
  0x9a,0xc1,0xb6,0x65,0x15,0x0b,0xa5,0x8d,0xe3,0x46,
  0xbb,0xfd,0x8c,0x9e,0xb7,0x44,0x45,0x26,0x17,0x7f};
}
