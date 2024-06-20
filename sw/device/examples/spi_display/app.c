// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "app.h"

#include "demos.h"
#include "context.h"
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
#include "sw/device/lib/runtime/irq.h"
#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"
#include "sw/device/lib/testing/rv_plic_testutils.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/base/memory.h"

enum {
  kBtnDebounceMillis = 50,
};

// Local functions declaration.
static uint32_t spi_write(void *handle, uint8_t *data, size_t len);
static uint32_t gpio_write(void *handle, bool cs, bool dc);
static void timer_delay(uint32_t ms);
static status_t config_interrupts(void);
static status_t aes_demo(void);
static status_t spi_passthru_demo(void);

extern context_t ctx;

status_t run_demo(LCD_Orientation orientation) {
  LOG_INFO("%s: Initializing pins", __func__);
  // Set the initial state of the LCD control pins.
  TRY(dif_gpio_write(&ctx.gpio, ctx.pins.dc.idx, 0x0));
  TRY(dif_gpio_write(&ctx.gpio, ctx.pins.led.idx, 0x1));

  // Reset LCD.
  LOG_INFO("%s: Reseting display", __func__);
  TRY(dif_gpio_write(&ctx.gpio, ctx.pins.reset.idx, 0x0));
  timer_delay(150);
  TRY(dif_gpio_write(&ctx.gpio, ctx.pins.reset.idx, 0x1));

  TRY(config_interrupts());

  irq_global_ctrl(true);
  irq_external_ctrl(true);

  LCD_Interface interface = {
      .handle = NULL,
      .spi_write = spi_write,      // SPI write callback.
      .gpio_write = gpio_write,    // GPIO write callback.
      .timer_delay = timer_delay,  // Timer delay callback.
  };

  LOG_INFO("%s: Initializing display", __func__);
  lcd_st7735_init(&ctx.lcd, &interface);

  // Set the LCD orientation.
  lcd_st7735_set_orientation(&ctx.lcd, orientation);

  // Setup text font bitmaps to be used and the colors.
  lcd_st7735_set_font(&ctx.lcd, &lucidaConsole_10ptFont);
  lcd_st7735_set_font_colors(&ctx.lcd, BGRColorWhite, BGRColorBlack);

  LOG_INFO("%s: Clearing...", __func__);
  // Clean display with a white rectangle.
  lcd_st7735_clean(&ctx.lcd);

  LOG_INFO("%s: Ot logo...", __func__);
  screen_println(&ctx.lcd, "Opentitan", alined_center, 7, true);
  screen_println(&ctx.lcd, "Boot successful!", alined_center, 8, true);
  timer_delay(1500);
  // Draw the splash screen with a RGB 565 bitmap and text in the bottom.
  lcd_st7735_draw_rgb565(
      &ctx.lcd,
      (LCD_rectangle){.origin = {.x = 0, .y = 20}, .width = 160, .height = 39},
      (uint8_t *)logo_opentitan_160_39);
  timer_delay(1500);

  LOG_INFO("%s: Starting menu.", __func__);
  // Show the main menu.
  const char *items[] = {
      "1. AES ECB/CDC",
      "2. SPI passthru",
      "3. CTAP demo",
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

  lcd_st7735_clean(&ctx.lcd);

  screen_show_menu(&ctx.lcd, &main_menu, 0);

  display_pin_map_t old_state = ctx.pins;

  while (true) {
    irq_global_ctrl(true);
    wait_for_interrupt();

    while (memcmp(&ctx.pins, &old_state, sizeof(display_pin_map_t))) {
      old_state = ctx.pins;
      timer_delay(kBtnDebounceMillis);
    }

    irq_global_ctrl(false);

    pin_t pins[] = {ctx.pins.btn_up, ctx.pins.btn_down, ctx.pins.btn_left, ctx.pins.btn_right};
    size_t selected = 0;
    for (size_t i = 0; i < ARRAYSIZE(pins); i++) {
      if (pins[i].state) {
        selected = i;
        break;
      }
    }

    screen_show_menu(&ctx.lcd, &main_menu, selected);

    if (ctx.pins.btn_ok.state) {
      switch (selected) {
        case 0:
          TRY(aes_demo());
          break;
        case 1:
          TRY(spi_passthru_demo());
          break;
        default:
          screen_println(&ctx.lcd, "Option not avail!", alined_center, 8, true);
          break;
      }
    }
  }
}

static status_t aes_demo(void) {
  TRY(run_aes(&ctx));

  timer_delay(5000);
  lcd_st7735_clean(&ctx.lcd);

  return OK_STATUS();
}

static status_t spi_passthru_demo(void) {
  static bool enabled = false;
  lcd_st7735_clean(&ctx.lcd);
  if (!enabled) {
    enabled = true;

    screen_println(&ctx.lcd, "Enabling passthru!", alined_center, 5, true);
    TRY(dif_spi_device_set_passthrough_mode(&ctx.spid, kDifToggleEnabled));
    TRY(spi_device_testutils_configure_passthrough(
        &ctx.spid,
        /*filters=*/0x00,
        /*upload_write_commands=*/false));

    TRY(dif_spi_host_output_set_enabled(&ctx.spi_flash, true));
  } else {
    enabled = false;
    screen_println(&ctx.lcd, "Disabling passthru!", alined_center, 5, true);
    TRY(dif_spi_device_set_passthrough_mode(&ctx.spid, kDifToggleDisabled));
    TRY(dif_spi_host_output_set_enabled(&ctx.spi_flash, false));
  }
  timer_delay(3000);
  lcd_st7735_clean(&ctx.lcd);
  return OK_STATUS();
}

status_t config_interrupts(void) {
  uint32_t button_mask =
    (1 << ctx.pins.btn_up.idx) |
    (1 << ctx.pins.btn_down.idx) |
    (1 << ctx.pins.btn_left.idx) |
    (1 << ctx.pins.btn_right.idx) |
    (1 << ctx.pins.btn_ok.idx);
  
  TRY(dif_gpio_irq_set_trigger(&ctx.gpio, button_mask, kDifGpioIrqTriggerEdgeRisingFalling));
  TRY(dif_gpio_irq_restore_all(&ctx.gpio, &button_mask));

  rv_plic_testutils_irq_range_enable(
    &ctx.rv_plic, kTopEarlgreyPlicTargetIbex0, kTopEarlgreyPlicIrqIdGpioGpio0,
    kTopEarlgreyPlicIrqIdGpioGpio0 + kDifGpioNumPins);

  return OK_STATUS();
}

void ottf_external_isr(uint32_t *exc_info) {
  dif_rv_plic_irq_id_t plic_irq_id;
  CHECK_DIF_OK(
      dif_rv_plic_irq_claim(&ctx.rv_plic, kTopEarlgreyPlicTargetIbex0, &plic_irq_id));

  // Check if it is the right peripheral.
  top_earlgrey_plic_peripheral_t peripheral = (top_earlgrey_plic_peripheral_t)
      top_earlgrey_plic_interrupt_for_peripheral[plic_irq_id];
  CHECK(peripheral == kTopEarlgreyPlicPeripheralGpio,
        "Interrupt from incorrect peripheral: (exp: %d, obs: %s)",
        kTopEarlgreyPlicPeripheralGpio, peripheral);

  // Correlate the interrupt fired from GPIO.
  uint32_t gpio_pin_irq_fired = plic_irq_id - kTopEarlgreyPlicIrqIdGpioGpio0;

  dif_gpio_state_t gpio_state;
  CHECK_DIF_OK(dif_gpio_read_all(&ctx.gpio, &gpio_state));

  pin_t* pins[] = {&ctx.pins.btn_up, &ctx.pins.btn_down,
                  &ctx.pins.btn_left, &ctx.pins.btn_right,
                  &ctx.pins.btn_ok};
  for (int i = 0; i < ARRAYSIZE(pins); i++) {
    pins[i]->state = !((gpio_state >> pins[i]->idx) & 1);
  }

  // Clear the interrupt at GPIO.
  CHECK_DIF_OK(dif_gpio_irq_acknowledge(&ctx.gpio, gpio_pin_irq_fired));

  // Complete the IRQ at PLIC.
  CHECK_DIF_OK(dif_rv_plic_irq_complete(&ctx.rv_plic, kTopEarlgreyPlicTargetIbex0,
                                        plic_irq_id));
}

static uint32_t spi_write(void *handle, uint8_t *data, size_t len) {
  // LOG_INFO("%s, %x, %d", __func__, *data, len);
  const uint32_t data_sent = len;

  dif_spi_host_segment_t transaction = {.type = kDifSpiHostSegmentTypeTx,
                                        .tx = {
                                            .width = kDifSpiHostWidthStandard,
                                            .buf = data,
                                            .length = len,
                                        }};
  CHECK_DIF_OK(
      dif_spi_host_transaction(&ctx.spi_lcd, /*csid=*/0, &transaction, 1));
  ibex_timeout_t deadline = ibex_timeout_init(5000);
  dif_spi_host_status_t status;
  do {
    CHECK_DIF_OK(dif_spi_host_get_status(&ctx.spi_lcd, &status));
    if (ibex_timeout_check(&deadline)) {
      LOG_INFO("%s, Timeout", __func__);
      return 0;
    }
  } while (!status.tx_empty);
  return data_sent;
}

static uint32_t gpio_write(void *handle, bool cs, bool dc) {
  CHECK_DIF_OK(dif_gpio_write(&ctx.gpio, ctx.pins.cs.idx, cs));
  CHECK_DIF_OK(dif_gpio_write(&ctx.gpio, ctx.pins.dc.idx, dc));
  return 0;
}

static void timer_delay(uint32_t ms) { busy_spin_micros(ms * 1000); }
