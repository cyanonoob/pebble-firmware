/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "os/os.h"

#include <stdint.h>

// Flat stand-in for a NimBLE mbuf chain; the test implements the functions.
struct os_mbuf {
  uint16_t len;
  uint8_t data[512];
};

#define OS_MBUF_PKTLEN(om) ((om)->len)

int os_mbuf_append(struct os_mbuf *om, const void *data, uint16_t len);
int os_mbuf_free_chain(struct os_mbuf *om);
int os_mbuf_copydata(const struct os_mbuf *om, int off, int len, void *dst);
void os_mbuf_adj(struct os_mbuf *om, int req_len);
