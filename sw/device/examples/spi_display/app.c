// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "app.h"

#include "demos.h"
#include "display_drivers/core/lucida_console_10pt.h"
#include "display_drivers/st7735/lcd_st7735.h"
#include "images/logo_opentitan_160_39.h"
#include "images/ot_stronks_160_100.h"
#include "screen.h"
#include "sw/device/lib/dif/dif_spi_host.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/testing/spi_device_testutils.h"
#include "sw/device/lib/testing/spi_flash_emulator.h"
#include "sw/device/lib/testing/spi_flash_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"

enum {
  kBtnDebounceMillis = 40,
};
// Local functions declaration.
static uint32_t spi_write(void *handle, uint8_t *data, size_t len);
static uint32_t gpio_write(void *handle, bool cs, bool dc);
static void timer_delay(uint32_t ms);
static status_t scan_buttons(context_t *ctx, uint32_t timeout);
static status_t notavail_demo(context_t *ctx);
static status_t aes_demo(context_t *ctx);
static status_t spi_passthru_demo(context_t *ctx);

status_t run_demo(dif_spi_host_t *spi_lcd, dif_spi_host_t *spi_flash,
                  dif_spi_device_handle_t *spid, dif_gpio_t *gpio,
                  dif_aes_t *aes, display_pin_map_t pins,
                  LCD_Orientation orientation) {
  LOG_INFO("%s: Initializing pins", __func__);
  // Set the initial state of the LCD control pins.
  TRY(dif_gpio_write(gpio, pins.dc, 0x0));
  TRY(dif_gpio_write(gpio, pins.led, 0x1));

  // Reset LCD.
  LOG_INFO("%s: Reseting display", __func__);
  TRY(dif_gpio_write(gpio, pins.reset, 0x0));
  timer_delay(150);
  TRY(dif_gpio_write(gpio, pins.reset, 0x1));

  // Init LCD driver and set the SPI driver.
  St7735Context lcd;
  context_t ctx = {spi_lcd, spi_flash, spid, gpio, aes, pins, &lcd};
  LCD_Interface interface = {
      .handle = &ctx,              // SPI handle.
      .spi_write = spi_write,      // SPI write callback.
      .gpio_write = gpio_write,    // GPIO write callback.
      .timer_delay = timer_delay,  // Timer delay callback.
  };
  LOG_INFO("%s: Initializing display", __func__);
  lcd_st7735_init(&lcd, &interface);

  // Set the LCD orientation.
  lcd_st7735_set_orientation(&lcd, orientation);

  // Setup text font bitmaps to be used and the colors.
  lcd_st7735_set_font(&lcd, &lucidaConsole_10ptFont);
  lcd_st7735_set_font_colors(&lcd, BGRColorWhite, BGRColorBlack);

  LOG_INFO("%s: Clearing...", __func__);
  // Clean display with a white rectangle.
  lcd_st7735_clean(&lcd);

  LOG_INFO("%s: Ot logo...", __func__);
  screen_println(&lcd, "Opentitan", alined_center, 7, true);
  screen_println(&lcd, "Boot successful!", alined_center, 8, true);
  timer_delay(1500);
  // Draw the splash screen with a RGB 565 bitmap and text in the bottom.
  lcd_st7735_draw_rgb565(
      &lcd,
      (LCD_rectangle){.origin = {.x = 0, .y = 20}, .width = 160, .height = 39},
      (uint8_t *)logo_opentitan_160_39);
  timer_delay(1500);

  size_t selected = 0;
  LOG_INFO("%s: Starting menu.", __func__);
  // Show the main menu.
  const char *items[] = {
      "1. AES ECB/CDC",
      "2. SPI passthru",
      "3. Another demo",
      "4. Yet another demo",
  };
  Menu_t main_menu = {
      .title = "Demo mode",
      .color = BGRColorBlue,
      .selected_color = BGRColorRed,
      .background = BGRColorWhite,
      .items_count = sizeof(items) / sizeof(items[0]),
      .items = items,
  };
  lcd_st7735_clean(&lcd);
  do {
    screen_show_menu(&lcd, &main_menu, selected);
    status_t ret = scan_buttons(&ctx, 1000);

    if (!status_ok(ret)) {
      continue;
    }

    switch (UNWRAP(ret)) {
      case 0:
      case 1:
      case 2:
      case 3:
        selected = (size_t)UNWRAP(ret);
        break;
      case 4:
        switch (selected) {
          case 0:
            TRY(aes_demo(&ctx));
            break;
          case 1:
            TRY(spi_passthru_demo(&ctx));
            break;
          default:
            screen_println(&lcd, "Option not avail!", alined_center, 8, true);
            break;
        }
        break;
      default:
        break;
    }
  } while (1);
}

static status_t aes_demo(context_t *ctx) {
  TRY(run_aes(ctx));
  timer_delay(5000);

  lcd_st7735_clean(ctx->lcd);
  lcd_st7735_draw_rgb565(
      ctx->lcd,
      (LCD_rectangle){.origin = {.x = 0, .y = 12}, .width = 160, .height = 100},
      (uint8_t *)ot_stronks_160_100);

  timer_delay(3000);
  lcd_st7735_clean(ctx->lcd);
  return OK_STATUS();
}

static status_t spi_passthru_demo(context_t *ctx) {
  static bool enabled = false;
  lcd_st7735_clean(ctx->lcd);
  if (!enabled) {
    enabled = true;

    screen_println(ctx->lcd, "Enabling passthru!", alined_center, 5, true);
    TRY(dif_spi_device_set_passthrough_mode(ctx->spid, kDifToggleEnabled));
    TRY(spi_device_testutils_configure_passthrough(
        ctx->spid,
        /*filters=*/0x00,
        /*upload_write_commands=*/false));

    TRY(dif_spi_host_output_set_enabled(ctx->spi_flash, true));
  } else {
    enabled = false;
    screen_println(ctx->lcd, "Disabling passthru!", alined_center, 5, true);
    TRY(dif_spi_device_set_passthrough_mode(ctx->spid, kDifToggleDisabled));
    TRY(dif_spi_host_output_set_enabled(ctx->spi_flash, false));
  }
  timer_delay(3000);
  lcd_st7735_clean(ctx->lcd);
  return OK_STATUS();
}

static status_t notavail_demo(context_t *ctx) {
  lcd_st7735_clean(ctx->lcd);
  while (true) {
    screen_println(ctx->lcd, "Option not avail!", alined_center, 5, true);
  }
}

status_t scan_buttons(context_t *ctx, uint32_t timeout) {
  ibex_timeout_t deadline = ibex_timeout_init(timeout * 1000);
  dif_gpio_pin_t pins[] = {ctx->pins.btn_up, ctx->pins.btn_down,
                           ctx->pins.btn_left, ctx->pins.btn_right,
                           ctx->pins.btn_ok};
  static size_t i = 0;
  do {
    i = (i + 1) % ARRAYSIZE(pins);
    bool state = true;
    TRY(dif_gpio_read(ctx->gpio, pins[i], &state));
    if (!state) {
      timer_delay(kBtnDebounceMillis);
      TRY(dif_gpio_read(ctx->gpio, pins[i], &state));
      if (!state) {
        LOG_INFO("Pin[%u]:%u pressed", i, pins[i]);
        return OK_STATUS((int32_t)i);
      }
    }

  } while (!ibex_timeout_check(&deadline));

  LOG_INFO("Btn scan timeout");
  return DEADLINE_EXCEEDED();
}

static uint32_t spi_write(void *handle, uint8_t *data, size_t len) {
  context_t *ctx = (context_t *)handle;
  // LOG_INFO("%s, %x, %d", __func__, *data, len);
  const uint32_t data_sent = len;

  dif_spi_host_segment_t transaction = {.type = kDifSpiHostSegmentTypeTx,
                                        .tx = {
                                            .width = kDifSpiHostWidthStandard,
                                            .buf = data,
                                            .length = len,
                                        }};
  CHECK_DIF_OK(
      dif_spi_host_transaction(ctx->spi_lcd, /*csid=*/0, &transaction, 1));
  ibex_timeout_t deadline = ibex_timeout_init(5000);
  dif_spi_host_status_t status;
  do {
    CHECK_DIF_OK(dif_spi_host_get_status(ctx->spi_lcd, &status));
    if (ibex_timeout_check(&deadline)) {
      LOG_INFO("%s, Timeout", __func__);
      return 0;
    }
  } while (!status.tx_empty);
  return data_sent;
}

static uint32_t gpio_write(void *handle, bool cs, bool dc) {
  context_t *ctx = (context_t *)handle;
  CHECK_DIF_OK(dif_gpio_write(ctx->gpio, ctx->pins.cs, cs));
  CHECK_DIF_OK(dif_gpio_write(ctx->gpio, ctx->pins.dc, dc));
  return 0;
}

static void timer_delay(uint32_t ms) { busy_spin_micros(ms * 1000); }
