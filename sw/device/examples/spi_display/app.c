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
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/testing/test_framework/check.h"

// Local functions declaration.
static uint32_t spi_write(void *handle, uint8_t *data, size_t len);
static uint32_t gpio_write(void *handle, bool cs, bool dc);
static void timer_delay(uint32_t ms);
static status_t scan_buttons(context_t *ctx, uint32_t timeout);

status_t run_demo(dif_spi_host_t *spi, dif_gpio_t *gpio, dif_aes_t *aes,
                  display_pin_map_t pins) {
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
  context_t ctx = {spi, gpio, aes, pins, &lcd};
  LCD_Interface interface = {
      .handle = &ctx,              // SPI handle.
      .spi_write = spi_write,      // SPI write callback.
      .gpio_write = gpio_write,    // GPIO write callback.
      .timer_delay = timer_delay,  // Timer delay callback.
  };
  LOG_INFO("%s: Initializing display", __func__);
  lcd_st7735_init(&lcd, &interface);

  // Set the LCD orientation.
  lcd_st7735_set_orientation(&lcd, LCD_Rotate0);

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

  // size_t selected = 0;
  do {
    lcd_st7735_clean(&lcd);

    // // Show the main menu.
    // const char *items[] = {
    //     "0. AES ECB/CDC",
    //     "1. OTBN",
    // };
    // Menu_t main_menu = {
    //     .title = "Main menu",
    //     .color = BGRColorBlue,
    //     .selected_color = BGRColorRed,
    //     .background = BGRColorWhite,
    //     .items_count = sizeof(items) / sizeof(items[0]),
    //     .items = items,
    // };
    // screen_show_menu(&lcd, &main_menu, selected);

    // if (TRY(scan_buttons(&ctx, 1000)) == ctx.pins.usr_btn) {
    //   lcd_st7735_puts(&lcd, (LCD_Point){.x = 5, .y = 80}, "Button 1 pressed");
    //   timer_delay(1000);
    // }

    // switch (selected) {
    //   case 0: {
        TRY(run_aes(&ctx));
    //     break;
    //   }
    //   case 1: {
        // lcd_st7735_puts(&lcd, (LCD_Point){.x = 5, .y = 80}, "Not available yet");
    //     break;
    //   }
    //   default:
    //     break;
    // }
    timer_delay(5000);


    lcd_st7735_clean(&lcd);
    lcd_st7735_draw_rgb565(
        &lcd,
        (LCD_rectangle){.origin = {.x = 0, .y = 12}, .width = 160, .height = 100},
        (uint8_t *)ot_stronks_160_100);

    timer_delay(3000);


    // lcd_st7735_clean(&lcd);
    // screen_show_menu(&lcd, &main_menu, selected);
    // timer_delay(800);

    // selected = (selected + 1) % main_menu.items_count;
  } while (1);
}

status_t scan_buttons(context_t *ctx, uint32_t timeout) {
  ibex_timeout_t deadline = ibex_timeout_init(timeout * 1000);
  do {
    bool state = false;
    TRY(dif_gpio_read(ctx->gpio, ctx->pins.usr_btn, &state));
    if (state) {
      return OK_STATUS((int32_t)ctx->pins.usr_btn);
    }
  } while (!ibex_timeout_check(&deadline));
  return OK_STATUS();
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
  CHECK_DIF_OK(dif_spi_host_transaction(ctx->spi, /*csid=*/0, &transaction, 1));
  ibex_timeout_t deadline = ibex_timeout_init(5000);
  dif_spi_host_status_t status;
  do {
    CHECK_DIF_OK(dif_spi_host_get_status(ctx->spi, &status));
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
