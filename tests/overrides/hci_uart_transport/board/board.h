/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

// Just enough of the board layer to compile third_party/nimble/transport/hci_uart_transport.c on
// the host. The test defines BLUETOOTH_UART and BOARD_CONFIG_BT_COMMON.

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  void *gpio;
  uint16_t gpio_pin;
} InputConfig;

typedef struct {
  void *gpio;
  uint16_t gpio_pin;
  bool active_high;
} OutputConfig;

typedef struct {
  void *gpio;
  uint16_t gpio_pin;
  uint16_t gpio_pin_source;
  uint8_t gpio_af;
} AfConfig;

typedef struct {
  uint8_t exti_port_source;
  uint8_t exti_line;
} ExtiConfig;

typedef struct {
  struct {
    InputConfig int_gpio;
    ExtiConfig int_exti;
  } wakeup;
} BoardConfigBTCommon;

typedef const struct UARTDevice UARTDevice;

extern UARTDevice *const BLUETOOTH_UART;
extern const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON;
