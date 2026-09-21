// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared GPIO function encoding, independent of the selected MCU.
#define AGPIO(x) ((x)<<5)
#define AGMAX(x) ((x)?(x-1):0)
