/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "board/board.h"

typedef enum { GPIO_OType_PP, GPIO_OType_OD } GPIOOType_TypeDef;
typedef enum { GPIO_Speed_2MHz, GPIO_Speed_25MHz, GPIO_Speed_50MHz } GPIOSpeed_TypeDef;

void gpio_input_init(const InputConfig *input_cfg);
void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed);
void gpio_output_set(const OutputConfig *pin_config, bool asserted);
