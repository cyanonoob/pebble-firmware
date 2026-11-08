/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "os/os_mbuf.h"

void *ble_transport_alloc_cmd(void);
void *ble_transport_alloc_evt(int discardable);
struct os_mbuf *ble_transport_alloc_acl_from_hs(void);
struct os_mbuf *ble_transport_alloc_iso_from_hs(void);
struct os_mbuf *ble_transport_alloc_acl_from_ll(void);
struct os_mbuf *ble_transport_alloc_iso_from_ll(void);
void ble_transport_free(void *buf);
int ble_transport_to_hs_evt(void *buf);
int ble_transport_to_hs_acl(struct os_mbuf *om);
int ble_transport_to_hs_iso(struct os_mbuf *om);
