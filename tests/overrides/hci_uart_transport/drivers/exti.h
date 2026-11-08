/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

typedef enum { ExtiTrigger_Rising, ExtiTrigger_Falling, ExtiTrigger_RisingFalling } ExtiTrigger;
typedef void (*ExtiHandlerCallback)(bool *should_context_switch);

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb);
void exti_enable(ExtiConfig config);
void exti_disable(ExtiConfig config);
