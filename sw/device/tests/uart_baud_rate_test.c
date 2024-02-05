// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_uart.h"
#include "sw/device/lib/runtime/hart.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_console.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

static const uint8_t kSendData[] = "UART baud test!";
static const uint32_t kBaseAddrs[4] = {
    TOP_EARLGREY_UART0_BASE_ADDR,
    TOP_EARLGREY_UART1_BASE_ADDR,
    TOP_EARLGREY_UART2_BASE_ADDR,
    TOP_EARLGREY_UART3_BASE_ADDR,
};
static uint32_t kBauds[7] = {
    9600, 115200, 230400, 128000, 256000, 1000000, 1500000,
};

enum {
  kBaudCountSilicon = 7,
  // The two highest Bauds won't work at the clock speed we run the FPGAs at.
  kBaudCountFpga = 5,
};

static dif_uart_t uart;

OTTF_DEFINE_TEST_CONFIG(.console.test_may_clobber = true,
                        .enable_concurrency = false);

// Send all bytes in `kSendData`, and check that they are received via the
// loopback mechanism.
void test_uart_baud(uint32_t baud_rate) {
  CHECK_DIF_OK(dif_uart_configure(
      &uart, (dif_uart_config_t){
                 .baudrate = (uint32_t)baud_rate,
                 .clk_freq_hz = (uint32_t)kClockFreqPeripheralHz,
                 .parity_enable = kDifToggleDisabled,
                 .parity = kDifUartParityEven,
                 .tx_enable = kDifToggleEnabled,
                 .rx_enable = kDifToggleDisabled,
             }));

  CHECK_DIF_OK(
      dif_uart_loopback_set(&uart, kDifUartLoopbackSystem, kDifToggleEnabled));
  CHECK_DIF_OK(dif_uart_fifo_reset(&uart, kDifUartDatapathAll));
  CHECK_DIF_OK(
      dif_uart_set_enable(&uart, kDifUartDatapathRx, kDifToggleEnabled));

  for (int i = 0; i < sizeof(kSendData); ++i) {
    CHECK_DIF_OK(dif_uart_byte_send_polled(&uart, kSendData[i]));

    uint8_t receive_byte;
    CHECK_DIF_OK(dif_uart_byte_receive_polled(&uart, &receive_byte));
    CHECK(kSendData[i] == receive_byte, "expected %c, got %c", kSendData[i],
          receive_byte);
  }
}

bool test_main(void) {
  // We test all four UARTs but in reverse order so that logging through UART0
  // is preserved for as long as possible.
  for (int8_t uart_idx = 3; uart_idx >= 0; uart_idx--) {
    if (uart_idx == 0) {
      LOG_INFO("Testing UART0 - console output will be disabled");
    } else {
      LOG_INFO("Testing UART%d", uart_idx);
    }

    mmio_region_t base_addr = mmio_region_from_addr(kBaseAddrs[uart_idx]);
    CHECK_DIF_OK(dif_uart_init(base_addr, &uart));

    size_t baud_count =
        kDeviceType == kDeviceSilicon ? kBaudCountSilicon : kBaudCountFpga;

    // Check every baud rate is sent and received okay.
    for (size_t baud_idx = 0; baud_idx < baud_count; ++baud_idx) {
      EXECUTE_TEST(test_uart_baud, kBauds[baud_idx]);
    }
  }

  return true;
}
