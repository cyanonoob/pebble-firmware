/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "os/os_mbuf.h"

void ble_transport_ll_init(void);
int ble_transport_to_ll_cmd_impl(void *buf);
int ble_transport_to_ll_acl_impl(struct os_mbuf *om);
int ble_transport_to_ll_iso_impl(struct os_mbuf *om);
