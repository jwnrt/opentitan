
#include "display_drivers/st7735/lcd_st7735.h"
#include "sw/device/lib/dif/dif_aes.h"
#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_rv_plic.h"
#include "sw/device/lib/dif/dif_spi_host.h"
#include "sw/device/lib/dif/dif_spi_device.h"

#ifndef OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_CONTEXT_H_
#define OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_CONTEXT_H_

typedef struct pin {
  dif_gpio_pin_t idx;
  bool state;
} pin_t;

typedef struct display_pin_map {
  pin_t reset;
  pin_t dc;
  pin_t led;
  pin_t cs;
  pin_t btn_up;
  pin_t btn_down;
  pin_t btn_left;
  pin_t btn_right;
  pin_t btn_ok;
} display_pin_map_t;

typedef struct context {
  dif_spi_host_t spi_lcd;
  dif_spi_host_t spi_flash;
  dif_spi_device_handle_t spid;
  dif_gpio_t gpio;
  dif_aes_t aes;
  dif_rv_plic_t rv_plic;
  display_pin_map_t pins;
  St7735Context lcd;
} context_t;

#endif
