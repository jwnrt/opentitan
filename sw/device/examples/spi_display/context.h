
#include "display_drivers/st7735/lcd_st7735.h"
#include "sw/device/lib/dif/dif_aes.h"
#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_spi_host.h"
#include "sw/device/lib/dif/dif_spi_device.h"

#ifndef OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_CONTEXT_H_
#define OPENTITAN_SW_DEVICE_EXAMPLE_SPI_DISPLAY_CONTEXT_H_
typedef struct display_pin_map {
  dif_gpio_pin_t reset;
  dif_gpio_pin_t dc;
  dif_gpio_pin_t led;
  dif_gpio_pin_t cs;
  dif_gpio_pin_t btn_up;
  dif_gpio_pin_t btn_down;
  dif_gpio_pin_t btn_left;
  dif_gpio_pin_t btn_right;
  dif_gpio_pin_t btn_ok;
} display_pin_map_t;

typedef struct context {
  dif_spi_host_t *spi_lcd;
  dif_spi_host_t *spi_flash;
  dif_spi_device_handle_t *spid;
  dif_gpio_t *gpio;
  dif_aes_t *aes;
  display_pin_map_t pins;
  St7735Context *lcd;
} context_t;

#endif