// SPDX-License-Identifier: GPL-3.0-or-later
#include "ota.h"
// Kept by an explicit application linker root. This identifies the image/layout,
// not its publisher: the SDK package checksums are not digital signatures.
extern "C" const char mt7697_image_identity[32] = MT7697_IMAGE_ID;
