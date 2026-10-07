/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <host/ble_gap.h>

#include <stdint.h>

//! Prepares the user-facing half of a pairing that needs input (NimBLE
//! BLE_GAP_EVENT_PASSKEY_ACTION) and returns the 6-digit code to show on the watch.
//!
//! For numeric comparison that is the controller's number, which the user confirms on both sides.
//! For passkey entry with the watch as the display (legacy pairing, or LE Secure Connections
//! without numeric comparison), the watch picks a random passkey, hands it to the stack right
//! away, and the user types it on the phone.
uint32_t nimble_pairing_handle_passkey_action(uint16_t conn_handle,
                                              const struct ble_gap_passkey_params *params);

//! Forgets the pairing that was waiting for the user, once it has ended (completed, failed, or
//! its link dropped). A late answer from the user is then ignored instead of being applied to
//! whatever runs on that connection handle next.
void nimble_pairing_handle_completed(void);
