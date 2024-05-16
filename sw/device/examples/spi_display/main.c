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
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

OTTF_DEFINE_TEST_CONFIG();

typedef struct Platform {
  pinmux_testutils_mio_dict_t csb;
  pinmux_testutils_mio_dict_t sd0;
  pinmux_testutils_mio_dict_t clk;
  pinmux_testutils_mio_dict_t reset;
  pinmux_testutils_mio_dict_t dc;
  pinmux_testutils_mio_dict_t led;
  size_t spi_speed;
  LCD_Orientation orientation;
} Platform_t;

static const Platform_t kCw340Board = {
    .csb = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob6),
    .sd0 = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob0),
    .clk = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob1),
    .reset = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob4),
    .dc = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob2),
    .led = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob3),
    .spi_speed = 3000000,  // 3Mhz
    .orientation = LCD_Rotate0,
};

static const Platform_t kBrewBoard = {
    .csb = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob1),
    .sd0 = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob7),
    .clk = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob9),
    .reset = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob3),
    .dc = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob5),
    .led = PINMUX_TESTUTILS_NEW_MIO_DICT(Iob11),
    .spi_speed = 12000000,  // 12Mhz
    .orientation = LCD_Rotate0,
};

static const Platform_t kVoyager1Board = {
    .csb = PINMUX_TESTUTILS_NEW_MIO_DICT(Ioc6),
    .sd0 = PINMUX_TESTUTILS_NEW_MIO_DICT(Ior2),
    .clk = PINMUX_TESTUTILS_NEW_MIO_DICT(Ior3),
    .reset = PINMUX_TESTUTILS_NEW_MIO_DICT(Ior4),
    .dc = PINMUX_TESTUTILS_NEW_MIO_DICT(Ioc9),
    .led = PINMUX_TESTUTILS_NEW_MIO_DICT(Ior1),
    .spi_speed = 22000000,  // 22Mhz
    .orientation = LCD_Rotate180,
};

static status_t pinmux_select(const dif_pinmux_t *pinmux, Platform_t pinmap) {
  // CSB.
  TRY(dif_pinmux_output_select(pinmux, pinmap.csb.out,
                               kTopEarlgreyPinmuxOutselSpiHost1Csb));

  TRY(dif_pinmux_output_select(pinmux, pinmap.sd0.out,
                               kTopEarlgreyPinmuxOutselSpiHost1Sd0));
  // SCLK.
  TRY(dif_pinmux_output_select(pinmux, pinmap.clk.out,
                               kTopEarlgreyPinmuxOutselSpiHost1Sck));

  // RESET.
  TRY(dif_pinmux_output_select(pinmux, pinmap.reset.out,
                               kTopEarlgreyPinmuxOutselGpioGpio0));
  // A0/DC.
  TRY(dif_pinmux_output_select(pinmux, pinmap.dc.out,
                               kTopEarlgreyPinmuxOutselGpioGpio1));
  // LED.
  TRY(dif_pinmux_output_select(pinmux, pinmap.led.out,
                               kTopEarlgreyPinmuxOutselGpioGpio2));

  if (kDeviceType == kDeviceSilicon) {
    dif_pinmux_pad_attr_t out_attr;
    dif_pinmux_pad_attr_t in_attr = {
        .slew_rate = 1,
        .drive_strength = 3,
        .flags = kDifPinmuxPadAttrPullResistorEnable |
                 kDifPinmuxPadAttrPullResistorUp};

    TRY(dif_pinmux_pad_write_attrs(pinmux, pinmap.clk.pad, kDifPinmuxPadKindMio,
                                   in_attr, &out_attr));
    TRY(dif_pinmux_pad_write_attrs(pinmux, pinmap.sd0.pad, kDifPinmuxPadKindMio,
                                   in_attr, &out_attr));
    TRY(dif_pinmux_pad_write_attrs(pinmux, pinmap.csb.pad, kDifPinmuxPadKindMio,
                                   in_attr, &out_attr));
  }
  return OK_STATUS();
}

bool test_main(void) {
  dif_spi_host_t spi_host;
  const volatile Platform_t *config = NULL;
  switch (kDeviceType) {
    case kDeviceFpgaCw340:
      LOG_INFO("FPGA CW340 detected!");
      config = &kCw340Board;
      break;
    case kDeviceSilicon:
      LOG_INFO("Silicon detected!");
      config = &kBrewBoard;
      config = &kVoyager1Board;
      break;
    default:
      CHECK(false, "Platform not supported");
  }

  dif_pinmux_t pinmux;
  CHECK_DIF_OK(dif_pinmux_init(
      mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR), &pinmux));

  pinmux_select(&pinmux, *config);

  CHECK_DIF_OK(dif_spi_host_init(
      mmio_region_from_addr(TOP_EARLGREY_SPI_HOST1_BASE_ADDR), &spi_host));

  CHECK(kClockFreqUsbHz <= UINT32_MAX,
        "kClockFreqPeripheralHz must fit in uint32_t");

  CHECK_DIF_OK(dif_spi_host_configure(
                   &spi_host,
                   (dif_spi_host_config_t){
                       .spi_clock = config->spi_speed,
                       .peripheral_clock_freq_hz = (uint32_t)kClockFreqUsbHz,
                   }),
               "SPI_HOST config failed!");
  CHECK_DIF_OK(dif_spi_host_output_set_enabled(&spi_host, true));

  dif_gpio_t gpio;
  CHECK_DIF_OK(
      dif_gpio_init(mmio_region_from_addr(TOP_EARLGREY_GPIO_BASE_ADDR), &gpio));
  CHECK_DIF_OK(dif_gpio_output_set_enabled_all(&gpio, 0x0f));

  dif_aes_t aes;
  // Initialise AES.
  CHECK_DIF_OK(
      dif_aes_init(mmio_region_from_addr(TOP_EARLGREY_AES_BASE_ADDR), &aes));
  CHECK_DIF_OK(dif_aes_reset(&aes));

  run_demo(&spi_host, &gpio, &aes, (display_pin_map_t){0, 1, 2, 3, 4}, config->orientation);

  return true;
}
