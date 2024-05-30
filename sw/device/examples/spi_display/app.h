// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_MAIN_DEMO_H_
#define OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_MAIN_DEMO_H_

#include "context.h"
#include "sw/device/lib/base/status.h"
#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_spi_host.h"

status_t run_demo(dif_spi_host_t *spi_lcd, dif_spi_host_t *spi_flash, dif_spi_device_handle_t *spid, dif_gpio_t *gpio, dif_aes_t *aes,
                  display_pin_map_t pins, LCD_Orientation orientation);

#endif  // OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_MAIN_DEMO_H_