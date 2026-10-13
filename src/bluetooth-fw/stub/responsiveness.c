/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "bluetooth/responsiveness.h"
#include "bluetooth/gap_le_connect.h"

#include <inttypes.h>

HciStatusCode bt_driver_le_connection_parameter_update(
    const BTDeviceInternal *addr, const BleConnectionParamsUpdateReq *req) {
  return HciStatusCode_Success;
}
