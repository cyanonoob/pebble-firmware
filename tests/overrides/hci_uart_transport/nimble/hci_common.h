/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#define BLE_HCI_EVCODE_LE_META (0x3E)
#define BLE_HCI_LE_SUBEV_ADV_RPT (0x02)
#define BLE_HCI_ISO_LENGTH_MASK (0x3fff)
#define BLE_HCI_ISO_LENGTH(l) ((l) & BLE_HCI_ISO_LENGTH_MASK)
