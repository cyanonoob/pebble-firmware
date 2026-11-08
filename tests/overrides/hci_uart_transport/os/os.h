/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

// Stands in for NimBLE's os/os.h plus the Pebble NimBLE port (nimble_npl_os.h), which on the
// watch is how these headers reach the transport.

#pragma once

#include "FreeRTOS.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/stop.h"
#include "os/mutex.h"
#include "queue.h"
#include "semphr.h"
#include "syscfg/syscfg.h"
#include "task.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

uint16_t get_le16(const void *buf);
