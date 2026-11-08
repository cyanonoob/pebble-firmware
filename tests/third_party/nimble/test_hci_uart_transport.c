/* SPDX-FileCopyrightText: 2026 Geert van Dijk */
/* SPDX-License-Identifier: Apache-2.0 */

// Protocol tests for the CC2564 eHCILL sleep handshake in the NimBLE HCI UART transport, checked
// against TI SWRA288B ("Enhanced HCILL: Four-Wire Power Management Protocol"). The UART, GPIO,
// EXTI and FreeRTOS primitives are faked; the transport and NimBLE's H4 parser are real.

#include "clar.h"

#include "fake_kernel_malloc.h"
#include "stubs_freertos.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

// Included rather than linked so the test can run the transport's RX task one pass at a time
// (prv_rx_task_process) instead of in its endless loop.
#include "transport/hci_uart_transport.c"

#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Board

#define CTS_PIN 11
#define RTS_PIN 12

static UARTDevice s_bt_uart = {
    .cts_gpio = {.gpio = (void *)1, .gpio_pin = CTS_PIN},
    .rts_gpio = {.gpio = (void *)1, .gpio_pin = RTS_PIN},
};
UARTDevice *const BLUETOOTH_UART = &s_bt_uart;

const BoardConfigBTCommon BOARD_CONFIG_BT_COMMON = {
    .wakeup =
        {
            .int_gpio = {.gpio = (void *)1, .gpio_pin = CTS_PIN},
            .int_exti = {.exti_line = CTS_PIN},
        },
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Fake UART, GPIO and EXTI: the wires between the watch and the CC2564

static struct {
  bool on;
  bool rx_irq;
  bool tx_irq;
  UARTRXInterruptHandler rx_handler;
  UARTTXInterruptHandler tx_handler;
  int power_ups;
  uint8_t sent[256];
  size_t sent_len;
} s_uart;

// Host RTS driven high as a GPIO: the controller must not send.
static bool s_rts_held_high;
// CTS reconfigured from flow control to a wakeup input.
static bool s_cts_is_wakeup_input;
static bool s_cts_exti_enabled;
static ExtiHandlerCallback s_cts_exti_cb;

void uart_init(UARTDevice *dev) {
  cl_assert(!s_uart.on);
  s_uart.on = true;
  s_uart.power_ups++;
}

void uart_deinit(UARTDevice *dev) {
  s_uart.on = false;
  s_uart.rx_irq = false;
  s_uart.tx_irq = false;
}

void uart_set_baud_rate(UARTDevice *dev, uint32_t baud_rate) {}

void uart_enable_flow_control(UARTDevice *dev) {
  s_rts_held_high = false;
  s_cts_is_wakeup_input = false;
}

void uart_set_rx_interrupt_handler(UARTDevice *dev, UARTRXInterruptHandler irq_handler) {
  s_uart.rx_handler = irq_handler;
}

void uart_set_tx_interrupt_handler(UARTDevice *dev, UARTTXInterruptHandler irq_handler) {
  s_uart.tx_handler = irq_handler;
}

void uart_set_rx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  s_uart.rx_irq = enabled;
}

void uart_set_tx_interrupt_enabled(UARTDevice *dev, bool enabled) {
  s_uart.tx_irq = enabled;
}

void uart_write_byte(UARTDevice *dev, uint8_t data) {
  cl_assert_(s_uart.on, "wrote to a powered-down UART");
  cl_assert(s_uart.sent_len < sizeof(s_uart.sent));
  s_uart.sent[s_uart.sent_len++] = data;
}

void gpio_input_init(const InputConfig *input_cfg) {
  if (input_cfg->gpio_pin == CTS_PIN) {
    s_cts_is_wakeup_input = true;
  }
}

void gpio_output_init(const OutputConfig *pin_config, GPIOOType_TypeDef otype,
                      GPIOSpeed_TypeDef speed) {}

void gpio_output_set(const OutputConfig *pin_config, bool asserted) {
  if (pin_config->gpio_pin == RTS_PIN) {
    s_rts_held_high = asserted;
  }
}

void exti_configure_pin(ExtiConfig cfg, ExtiTrigger trigger, ExtiHandlerCallback cb) {
  s_cts_exti_cb = cb;
}

void exti_enable(ExtiConfig config) {
  s_cts_exti_enabled = true;
}

void exti_disable(ExtiConfig config) {
  s_cts_exti_enabled = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// RX task: never actually started; the tests run prv_rx_task_process() themselves

void pebble_task_create(PebbleTask pebble_task, TaskParameters_t *task_params,
                        TaskHandle_t *handle) {
  *handle = (TaskHandle_t)1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Stop mode: every powered-up UART must hold exactly one InhibitorBluetooth reference

static int s_bt_stop_inhibits;

void stop_mode_disable(StopModeInhibitor inhibitor) {
  if (inhibitor == InhibitorBluetooth) {
    s_bt_stop_inhibits++;
  }
}

void stop_mode_enable(StopModeInhibitor inhibitor) {
  if (inhibitor == InhibitorBluetooth) {
    cl_assert(s_bt_stop_inhibits > 0);
    s_bt_stop_inhibits--;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FreeRTOS: non-blocking queues and semaphores, and a tick counter the tests advance

typedef struct {
  UBaseType_t length;
  UBaseType_t item_size;
  UBaseType_t count;
  UBaseType_t head;
  uint8_t *storage;
} FakeQueue;

static TickType_t s_ticks;

QueueHandle_t xQueueGenericCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize,
                                  const uint8_t ucQueueType) {
  FakeQueue *queue = calloc(1, sizeof(*queue));
  queue->length = uxQueueLength;
  queue->item_size = uxItemSize;
  queue->storage = calloc(uxQueueLength, uxItemSize ? uxItemSize : 1);
  return (QueueHandle_t)queue;
}

void vQueueDelete(QueueHandle_t xQueue) {
  FakeQueue *queue = (FakeQueue *)xQueue;
  free(queue->storage);
  free(queue);
}

BaseType_t xQueueGenericSend(QueueHandle_t xQueue, const void *const pvItemToQueue,
                             TickType_t xTicksToWait, const BaseType_t xCopyPosition) {
  FakeQueue *queue = (FakeQueue *)xQueue;
  cl_assert(queue->count < queue->length);
  const UBaseType_t tail = (queue->head + queue->count) % queue->length;
  memcpy(queue->storage + tail * queue->item_size, pvItemToQueue, queue->item_size);
  queue->count++;
  return pdTRUE;
}

BaseType_t xQueuePeekFromISR(QueueHandle_t xQueue, void *const pvBuffer) {
  FakeQueue *queue = (FakeQueue *)xQueue;
  if (queue->count == 0) {
    return pdFALSE;
  }
  memcpy(pvBuffer, queue->storage + queue->head * queue->item_size, queue->item_size);
  return pdTRUE;
}

static BaseType_t prv_queue_pop(FakeQueue *queue, void *buffer) {
  if (queue->count == 0) {
    return pdFALSE;
  }
  if (buffer) {
    memcpy(buffer, queue->storage + queue->head * queue->item_size, queue->item_size);
  }
  queue->head = (queue->head + 1) % queue->length;
  queue->count--;
  return pdTRUE;
}

BaseType_t xQueueReceiveFromISR(QueueHandle_t xQueue, void *const pvBuffer,
                                BaseType_t *const pxHigherPriorityTaskWoken) {
  return prv_queue_pop((FakeQueue *)xQueue, pvBuffer);
}

BaseType_t xQueueGenericReceive(QueueHandle_t xQueue, void *const pvBuffer, TickType_t xTicksToWait,
                                const BaseType_t xJustPeek) {
  FakeQueue *queue = (FakeQueue *)xQueue;
  if (xJustPeek) {
    return xQueuePeekFromISR(xQueue, pvBuffer);
  }
  return prv_queue_pop(queue, pvBuffer);
}

BaseType_t xQueueGiveFromISR(QueueHandle_t xQueue, BaseType_t *const pxHigherPriorityTaskWoken) {
  FakeQueue *queue = (FakeQueue *)xQueue;
  if (queue->count < queue->length) {
    queue->count++;
  }
  return pdTRUE;
}

TickType_t xTaskGetTickCount(void) {
  return s_ticks;
}

TickType_t xTaskGetTickCountFromISR(void) {
  return s_ticks;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// NimBLE: chipset hooks, host side of the transport, and a flat mbuf

void ble_chipset_init(void) {}

bool ble_chipset_start(void) {
  return true;
}

bool ble_chipset_is_hcill(void) {
  return true;
}

#define MAX_HS_EVENTS 8
static uint8_t s_hs_events[MAX_HS_EVENTS][MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE)];
static int s_hs_event_count;

void *ble_transport_alloc_cmd(void) {
  return kernel_malloc(260);
}

void *ble_transport_alloc_evt(int discardable) {
  return kernel_malloc(MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE));
}

struct os_mbuf *ble_transport_alloc_acl_from_ll(void) {
  return kernel_zalloc(sizeof(struct os_mbuf));
}

struct os_mbuf *ble_transport_alloc_acl_from_hs(void) {
  return NULL;
}

struct os_mbuf *ble_transport_alloc_iso_from_hs(void) {
  return NULL;
}

struct os_mbuf *ble_transport_alloc_iso_from_ll(void) {
  return NULL;
}

void ble_transport_free(void *buf) {
  kernel_free(buf);
}

int ble_transport_to_hs_evt(void *buf) {
  const uint8_t *event = buf;
  cl_assert(s_hs_event_count < MAX_HS_EVENTS);
  memcpy(s_hs_events[s_hs_event_count++], event, event[1] + 2);
  kernel_free(buf);
  return 0;
}

int ble_transport_to_hs_acl(struct os_mbuf *om) {
  kernel_free(om);
  return 0;
}

int ble_transport_to_hs_iso(struct os_mbuf *om) {
  kernel_free(om);
  return 0;
}

int os_mbuf_append(struct os_mbuf *om, const void *data, uint16_t len) {
  cl_assert(om->len + len <= sizeof(om->data));
  memcpy(&om->data[om->len], data, len);
  om->len += len;
  return 0;
}

int os_mbuf_free_chain(struct os_mbuf *om) {
  kernel_free(om);
  return 0;
}

int os_mbuf_copydata(const struct os_mbuf *om, int off, int len, void *dst) {
  memcpy(dst, &om->data[off], len);
  return 0;
}

void os_mbuf_adj(struct os_mbuf *om, int req_len) {
  memmove(om->data, &om->data[req_len], om->len - req_len);
  om->len -= req_len;
}

uint16_t get_le16(const void *buf) {
  const uint8_t *bytes = buf;
  return bytes[0] | (bytes[1] << 8);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers

#define SLEEP_IND CMD_HCILL_GO_TO_SLEEP_IND
#define SLEEP_ACK CMD_HCILL_GO_TO_SLEEP_ACK
#define WAKE_IND CMD_HCILL_WAKE_UP_IND
#define WAKE_ACK CMD_HCILL_WAKE_UP_ACK

// HCI_Reset as NimBLE hands it to the transport, and as it must appear on the wire.
static const uint8_t s_reset_cmd[] = {0x03, 0x0c, 0x00};
#define RESET_ON_WIRE HCI_H4_CMD, 0x03, 0x0c, 0x00
// Command Complete for HCI_Reset as the controller sends it.
#define RESET_COMPLETE_ON_WIRE HCI_H4_EVT, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00
static const uint8_t s_reset_complete_event[] = {0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};

static const TickType_t WAKE_TIMEOUT_TICKS = pdMS_TO_TICKS(EHCILL_WAKE_TIMEOUT_MS);

// Run the RX task and the UART TX interrupt until the transport has nothing left to do.
static void prv_settle(void) {
  for (int i = 0; i < 2000; i++) {
    prv_rx_task_process();
    if (!(s_uart.on && s_uart.tx_irq)) {
      return;
    }
    s_uart.tx_handler(BLUETOOTH_UART);
  }
  cl_fail("transport never went idle");
}

static void prv_controller_sends(const uint8_t *bytes, size_t len) {
  cl_assert_(s_uart.on, "controller sent to a powered-down UART");
  cl_assert_(!s_rts_held_high, "controller sent while host RTS was held high");
  for (size_t i = 0; i < len; i++) {
    cl_assert_(s_uart.rx_irq, "transport stopped receiving");
    const UARTRXErrorFlags no_errors = {};
    s_uart.rx_handler(BLUETOOTH_UART, bytes[i], &no_errors);
  }
  prv_settle();
}

#define CONTROLLER_SENDS(...)                   \
  do {                                          \
    const uint8_t bytes[] = {__VA_ARGS__};      \
    prv_controller_sends(bytes, sizeof(bytes)); \
  } while (0)

static void prv_expect_sent(const uint8_t *expected, size_t len) {
  cl_assert_equal_i(s_uart.sent_len, len);
  cl_assert_equal_m(s_uart.sent, expected, len);
  s_uart.sent_len = 0;
}

#define EXPECT_SENT(...)                         \
  do {                                           \
    const uint8_t expected[] = {__VA_ARGS__};    \
    prv_expect_sent(expected, sizeof(expected)); \
  } while (0)

static void prv_expect_nothing_sent(void) {
  cl_assert_equal_i(s_uart.sent_len, 0);
}

static void prv_host_sends_reset(void) {
  uint8_t *cmd = kernel_malloc(sizeof(s_reset_cmd));
  memcpy(cmd, s_reset_cmd, sizeof(s_reset_cmd));
  ble_transport_to_ll_cmd_impl(cmd);
}

// The controller's wake-up pulse on CTS, without running the RX task (the bottom half) yet.
static void prv_cts_edge(void) {
  cl_assert_(s_cts_exti_enabled, "CTS wakeup interrupt is not armed");
  bool should_context_switch = false;
  s_cts_exti_cb(&should_context_switch);
}

static void prv_expect_link_asleep(void) {
  cl_assert(!s_uart.on);
  cl_assert(s_rts_held_high);
  cl_assert(s_cts_is_wakeup_input);
  cl_assert(s_cts_exti_enabled);
  cl_assert_equal_i(s_bt_stop_inhibits, 0);
}

static void prv_expect_uart_up(void) {
  cl_assert(s_uart.on);
  cl_assert(!s_rts_held_high);
  cl_assert(!s_cts_is_wakeup_input);
  cl_assert(!s_cts_exti_enabled);
  cl_assert_equal_i(s_bt_stop_inhibits, 1);
}

static void prv_controller_puts_link_to_sleep(void) {
  CONTROLLER_SENDS(SLEEP_IND);
  EXPECT_SENT(SLEEP_ACK);
  prv_expect_link_asleep();
}

static void prv_expect_reset_complete_delivered(void) {
  cl_assert_equal_i(s_hs_event_count, 1);
  cl_assert_equal_m(s_hs_events[0], s_reset_complete_event, sizeof(s_reset_complete_event));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Tests

void test_hci_uart_transport__initialize(void) {
  memset(&s_uart, 0, sizeof(s_uart));
  s_rts_held_high = false;
  s_cts_is_wakeup_input = false;
  s_cts_exti_enabled = false;
  s_cts_exti_cb = NULL;
  s_bt_stop_inhibits = 0;
  s_ticks = 1000;
  s_hs_event_count = 0;

  // Transport state, since it is #included rather than reloaded per test.
  s_ehcill_sm = EHCILL_AWAKE;
  s_uart_on = false;
  s_ehcill_wake_retries = 0;
  uart_tx_dispose_head = NULL;
  chipset_start_done = false;

  ble_transport_ll_init();
  prv_expect_uart_up();
  s_uart.power_ups = 0;
}

void test_hci_uart_transport__cleanup(void) {
  vQueueDelete(s_tx_queue);
  vQueueDelete(s_rx_data_ready);
  vQueueDelete(s_cmd_done);
}

void test_hci_uart_transport__controller_sleeps_and_wakes_the_link(void) {
  prv_controller_puts_link_to_sleep();

  prv_cts_edge();
  prv_settle();
  prv_expect_uart_up();
  prv_expect_nothing_sent();

  CONTROLLER_SENDS(WAKE_IND);
  EXPECT_SENT(WAKE_ACK);

  CONTROLLER_SENDS(RESET_COMPLETE_ON_WIRE);
  prv_expect_reset_complete_delivered();
}

void test_hci_uart_transport__host_wakes_the_link_before_sending(void) {
  prv_controller_puts_link_to_sleep();

  prv_host_sends_reset();
  prv_settle();
  prv_expect_uart_up();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(WAKE_ACK);
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__crossed_wake_ups_count_as_acks(void) {
  // SWRA288B 2.6.3: both sides send WAKE_UP_IND; the controller's counts as our ack.
  prv_controller_puts_link_to_sleep();
  prv_host_sends_reset();
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(WAKE_IND);
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__sleep_request_crossing_our_wake_up_is_ignored(void) {
  // SWRA288B 2.6.4: a GO_TO_SLEEP_IND still queued in the controller arrives after our
  // WAKE_UP_IND. This used to crash ("illegal HCI command received while ...").
  prv_controller_puts_link_to_sleep();
  prv_host_sends_reset();
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(SLEEP_IND);
  prv_expect_nothing_sent();
  prv_expect_uart_up();

  CONTROLLER_SENDS(WAKE_ACK);
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__garbled_byte_while_waking_is_dropped(void) {
  prv_controller_puts_link_to_sleep();
  prv_cts_edge();
  prv_settle();

  CONTROLLER_SENDS(0xff);
  prv_expect_nothing_sent();

  CONTROLLER_SENDS(WAKE_IND);
  EXPECT_SENT(WAKE_ACK);
  CONTROLLER_SENDS(RESET_COMPLETE_ON_WIRE);
  prv_expect_reset_complete_delivered();
}

void test_hci_uart_transport__byte_that_cannot_start_a_packet_is_dropped(void) {
  // An unknown H4 packet type used to reach the H4 parser's assert(0) on sync loss.
  CONTROLLER_SENDS(0x00, RESET_COMPLETE_ON_WIRE);
  prv_expect_reset_complete_delivered();
  prv_expect_nothing_sent();
}

void test_hci_uart_transport__repeated_wake_up_is_acked_after_the_packet_in_flight(void) {
  prv_host_sends_reset();
  s_uart.tx_handler(BLUETOOTH_UART);
  s_uart.tx_handler(BLUETOOTH_UART);
  cl_assert_equal_i(s_uart.sent_len, 2);

  // The controller missed our earlier ack and asks again (SWRA288B 2.3). The ack must not land
  // inside the half-sent command.
  CONTROLLER_SENDS(WAKE_IND);
  EXPECT_SENT(RESET_ON_WIRE, WAKE_ACK);
}

void test_hci_uart_transport__sleep_request_with_a_queued_command_acks_then_wakes(void) {
  // SWRA288B 2.3 note: ack the sleep request, then go through a wake-up to send the command.
  prv_host_sends_reset();  // queued, not yet started on the wire
  CONTROLLER_SENDS(SLEEP_IND);
  EXPECT_SENT(SLEEP_ACK, WAKE_IND);
  cl_assert(s_uart.on);
  cl_assert_equal_i(s_bt_stop_inhibits, 1);

  CONTROLLER_SENDS(WAKE_ACK);
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__unanswered_wake_up_is_resent(void) {
  prv_controller_puts_link_to_sleep();
  prv_host_sends_reset();
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  s_ticks += WAKE_TIMEOUT_TICKS - 1;
  prv_settle();
  prv_expect_nothing_sent();

  s_ticks += 1;
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(WAKE_ACK);
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__lost_controller_wake_up_is_recovered(void) {
  prv_controller_puts_link_to_sleep();
  prv_cts_edge();
  prv_settle();
  prv_expect_nothing_sent();

  // The controller's WAKE_UP_IND never arrives; we start the wake-up from our side.
  s_ticks += WAKE_TIMEOUT_TICKS;
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(WAKE_ACK);
  prv_host_sends_reset();
  prv_settle();
  EXPECT_SENT(RESET_ON_WIRE);
}

void test_hci_uart_transport__resets_when_the_controller_stops_answering(void) {
  prv_controller_puts_link_to_sleep();
  prv_host_sends_reset();
  prv_settle();
  EXPECT_SENT(WAKE_IND);

  for (int i = 0; i < EHCILL_WAKE_MAX_RETRIES; i++) {
    s_ticks += WAKE_TIMEOUT_TICKS;
    prv_settle();
    EXPECT_SENT(WAKE_IND);
  }

  s_ticks += WAKE_TIMEOUT_TICKS;
  cl_assert_passert(prv_settle());
}

void test_hci_uart_transport__simultaneous_wake_ups_power_the_uart_once(void) {
  prv_controller_puts_link_to_sleep();

  // The controller pulses CTS and NimBLE queues a command before the RX task runs.
  prv_cts_edge();
  prv_host_sends_reset();
  prv_settle();
  cl_assert_equal_i(s_uart.power_ups, 1);
  prv_expect_uart_up();
  EXPECT_SENT(WAKE_IND);

  CONTROLLER_SENDS(WAKE_IND);
  EXPECT_SENT(RESET_ON_WIRE);

  // Going back to sleep releases the one stop-mode reference.
  prv_controller_puts_link_to_sleep();
}
