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

typedef struct Platform {
  top_earlgrey_pinmux_mio_out_t csb;
  top_earlgrey_pinmux_mio_out_t sd0;
  top_earlgrey_pinmux_mio_out_t clk;
  top_earlgrey_pinmux_mio_out_t reset;
  top_earlgrey_pinmux_mio_out_t dc;
  top_earlgrey_pinmux_mio_out_t led;
  size_t spi_speed;
} Platform_t;

static const Platform_t kCw340Platform = {
    .csb = kTopEarlgreyPinmuxMioOutIob6,
    .sd0 = kTopEarlgreyPinmuxMioOutIob0,
    .clk = kTopEarlgreyPinmuxMioOutIob1,
    .reset = kTopEarlgreyPinmuxMioOutIob4,
    .dc = kTopEarlgreyPinmuxMioOutIob2,
    .led = kTopEarlgreyPinmuxMioOutIob3,
    .spi_speed = 3000000,  // 3Mhz
};

static const Platform_t kSiliconPlatform = {
    .csb = kTopEarlgreyPinmuxMioOutIob1,
    .sd0 = kTopEarlgreyPinmuxMioOutIob7,
    .clk = kTopEarlgreyPinmuxMioOutIob9,
    .reset = kTopEarlgreyPinmuxMioOutIob3,
    .dc = kTopEarlgreyPinmuxMioOutIob5,
    .led = kTopEarlgreyPinmuxMioOutIob11,
    .spi_speed = 12000000,  // 12Mhz
};

static status_t pinmux_select(const dif_pinmux_t *pinmux, Platform_t pinmap) {
  // CSB.
  TRY(dif_pinmux_output_select(pinmux, pinmap.csb,
                               kTopEarlgreyPinmuxOutselSpiHost1Csb));

  TRY(dif_pinmux_output_select(pinmux, pinmap.sd0,
                               kTopEarlgreyPinmuxOutselSpiHost1Sd0));
  // SCLK.
  TRY(dif_pinmux_output_select(pinmux, pinmap.clk,
                               kTopEarlgreyPinmuxOutselSpiHost1Sck));

  // RESET.
  TRY(dif_pinmux_output_select(pinmux, pinmap.reset,
                               kTopEarlgreyPinmuxOutselGpioGpio0));
  // A0/DC.
  TRY(dif_pinmux_output_select(pinmux, pinmap.dc,
                               kTopEarlgreyPinmuxOutselGpioGpio1));
  // LED.
  TRY(dif_pinmux_output_select(pinmux, pinmap.led,
                               kTopEarlgreyPinmuxOutselGpioGpio2));
  return OK_STATUS();
}

bool test_main(void) {
  dif_spi_host_t spi_host;
  const Platform_t *config = NULL;
  switch (kDeviceType) {
    case kDeviceFpgaCw340:
      config = &kCw340Platform;
      break;
    case kDeviceSilicon:
      config = &kSiliconPlatform;
      break;
    default:
      CHECK(false, "Platform not supported");
  }

  CHECK_DIF_OK(dif_spi_host_init(
      mmio_region_from_addr(TOP_EARLGREY_SPI_HOST1_BASE_ADDR), &spi_host));

  CHECK(kClockFreqPeripheralHz <= UINT32_MAX,
        "kClockFreqPeripheralHz must fit in uint32_t");

  CHECK_DIF_OK(dif_spi_host_configure(&spi_host,
                                      (dif_spi_host_config_t){
                                          .spi_clock = config->spi_speed,
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

  pinmux_select(&pinmux, *config);

  dif_aes_t aes;
  // Initialise AES.
  CHECK_DIF_OK(
      dif_aes_init(mmio_region_from_addr(TOP_EARLGREY_AES_BASE_ADDR), &aes));
  CHECK_DIF_OK(dif_aes_reset(&aes));

  run_demo(&spi_host, &gpio, &aes, (display_pin_map_t){0, 1, 2, 3, 4});

  return true;
}
