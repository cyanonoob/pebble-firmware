/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

typedef const struct UARTDevice {
  AfConfig cts_gpio;
  AfConfig rts_gpio;
} UARTDevice;
