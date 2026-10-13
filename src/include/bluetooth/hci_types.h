/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum {
  HciStatusCode_Success = 0x00,
  HciStatusCode_UnknownConnectionIdentifier = 0x02,
  HciStatusCode_UnspecifiedError = 0x1F,
  //! LMP Error Transaction Collision / LL Procedure Collision
  HciStatusCode_LLProcedureCollision = 0x23,
  HciStatusCode_DifferentTransactionCollision = 0x2A,
  HciStatusCode_VS_Base = 0x50,
  HciStatusCode_Max = UINT16_MAX
} HciStatusCode;

#ifndef __clang__
_Static_assert(sizeof(HciStatusCode) == 2, "packed structs expect the status code to be 2 bytes!");
#endif

// disconnect reasons are just status codes
typedef HciStatusCode HciDisconnectReason;
