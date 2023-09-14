// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
#include <assert.h>

#include "app.h"
#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/base/macros.h"
#include "sw/device/lib/base/memory.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_pinmux.h"
#include "sw/device/lib/dif/dif_spi_host.h"
#include "sw/device/lib/runtime/hart.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG();

status_t spi_host1_pinmux_select_cw340(const dif_pinmux_t *pinmux) {
  LOG_INFO("%s", __func__);
  // CSB.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob6,  // J18B - HD_IOB6
                               kTopEarlgreyPinmuxOutselSpiHost1Csb));

  // SD0/SDA/MOSI.
  TRY(dif_pinmux_input_select(pinmux, kTopEarlgreyPinmuxPeripheralInSpiHost1Sd0,
                              kTopEarlgreyPinmuxInselIob0));  // J18B - HD_IOB0
  TRY(dif_pinmux_output_select(
      pinmux, kTopEarlgreyPinmuxMioOutIob0,
      kTopEarlgreyPinmuxOutselSpiHost1Sd0));  // J18B - HD_IOB0

  // SCLK.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob1,  // J18B  - HD_IOB1
                               kTopEarlgreyPinmuxOutselSpiHost1Sck));
  return OK_STATUS();
}

status_t gpio_pinmux_select_cw340(const dif_pinmux_t *pinmux) {
  LOG_INFO("%s", __func__);

  // RESET.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob4,  // J18B - HD_IOB4
                               kTopEarlgreyPinmuxOutselGpioGpio0));
  // A0/DC.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob2,  // J18B - HD_IOB2
                               kTopEarlgreyPinmuxOutselGpioGpio1));
  // LED.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob3,  // J18B - HD_IOB3
                               kTopEarlgreyPinmuxOutselGpioGpio2));
  // // CSB.
  // TRY(dif_pinmux_output_select(
  //     pinmux, kTopEarlgreyPinmuxMioOutIob6,  // J18B - HD_IOB6
  //     kTopEarlgreyPinmuxOutselGpioGpio3));
  return OK_STATUS();
}



status_t spi_host1_pinmux_select_chip(const dif_pinmux_t *pinmux) {
  LOG_INFO("%s", __func__);
  // CSB.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob1,  
                               kTopEarlgreyPinmuxOutselSpiHost1Csb));

  // SD0/SDA/MOSI.
  TRY(dif_pinmux_input_select(pinmux, kTopEarlgreyPinmuxPeripheralInSpiHost1Sd0,
                              kTopEarlgreyPinmuxInselIob7)); 
  TRY(dif_pinmux_output_select(
      pinmux, kTopEarlgreyPinmuxMioOutIob7,
      kTopEarlgreyPinmuxOutselSpiHost1Sd0));
  // SCLK.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob9, 
                               kTopEarlgreyPinmuxOutselSpiHost1Sck));
  return OK_STATUS();
}

status_t gpio_pinmux_select_chip(const dif_pinmux_t *pinmux) {
  LOG_INFO("%s", __func__);

  // RESET.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob3, 
                               kTopEarlgreyPinmuxOutselGpioGpio0));
  // A0/DC.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob5, 
                               kTopEarlgreyPinmuxOutselGpioGpio1));
  // LED.
  TRY(dif_pinmux_output_select(pinmux,
                               kTopEarlgreyPinmuxMioOutIob11, 
                               kTopEarlgreyPinmuxOutselGpioGpio2));
  return OK_STATUS();
}

bool test_main(void) {
  dif_spi_host_t spi_host;
  CHECK_DIF_OK(dif_spi_host_init(
      mmio_region_from_addr(TOP_EARLGREY_SPI_HOST1_BASE_ADDR), &spi_host));

  CHECK(kClockFreqPeripheralHz <= UINT32_MAX,
        "kClockFreqPeripheralHz must fit in uint32_t");

  CHECK_DIF_OK(dif_spi_host_configure(&spi_host,
                                      (dif_spi_host_config_t){
                                          .spi_clock = 10000000,
                                          .peripheral_clock_freq_hz =
                                              (uint32_t)kClockFreqPeripheralHz,
                                      }),
               "SPI_HOST config failed!");
  CHECK_DIF_OK(dif_spi_host_output_set_enabled(&spi_host, true));

  dif_gpio_t gpio;
  CHECK_DIF_OK(
      dif_gpio_init(mmio_region_from_addr(TOP_EARLGREY_GPIO_BASE_ADDR), &gpio));
  CHECK_DIF_OK(dif_gpio_output_set_enabled_all(&gpio, 0x0f));

  dif_pinmux_t pinmux;
  CHECK_DIF_OK(dif_pinmux_init(
      mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR), &pinmux));

  spi_host1_pinmux_select_chip(&pinmux);

  gpio_pinmux_select_chip(&pinmux);

  dif_aes_t aes;
  // Initialise AES.
  CHECK_DIF_OK(
      dif_aes_init(mmio_region_from_addr(TOP_EARLGREY_AES_BASE_ADDR), &aes));
  CHECK_DIF_OK(dif_aes_reset(&aes));

  run_demo(&spi_host, &gpio, &aes, (display_pin_map_t){0, 1, 2, 3, 4});

  return true;
}
