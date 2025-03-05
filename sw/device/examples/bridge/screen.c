#include "sw/device/examples/bridge/screen.h"

#include <core/lucida_console_10pt.h>
#include <st7735/lcd_st7735.h>

#include "sw/device/examples/spi_display/images/logo_opentitan_160_39.h"
#include "sw/device/lib/arch/device.h"
#include "sw/device/lib/base/mmio.h"
#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_pinmux.h"
#include "sw/device/lib/dif/dif_spi_host.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/silicon_creator/lib/dbg_print.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

#define GPIO_PIN_RESET 0
#define GPIO_PIN_DC 1
#define GPIO_PIN_LED 2
#define GPIO_PIN_CS 11

static uint32_t spi_write(void *handle, uint8_t *data, size_t len);
static uint32_t gpio_write(void *handle, bool cs, bool dc);
static void timer_delay(uint32_t ms);

static LCD_Interface lcd_iface = {
    .handle = NULL,
    .spi_write = spi_write,
    .gpio_write = gpio_write,
    .timer_delay = timer_delay,
};

void screen_init(screen_context_t *screen_ctx) {
  mmio_region_t base_addr;

  dif_pinmux_t pinmux;
  base_addr = mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR);
  CHECK_DIF_OK(dif_pinmux_init(base_addr, &pinmux));

  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIoc6,
                                        kTopEarlgreyPinmuxOutselSpiHost1Csb));
  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIob1,
                                        kTopEarlgreyPinmuxOutselSpiHost1Sd0));
  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIob2,
                                        kTopEarlgreyPinmuxOutselSpiHost1Sck));
  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIoc7,
                                        kTopEarlgreyPinmuxOutselGpioGpio0));
  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIoc9,
                                        kTopEarlgreyPinmuxOutselGpioGpio1));
  CHECK_DIF_OK(dif_pinmux_output_select(&pinmux, kTopEarlgreyPinmuxMioOutIob0,
                                        kTopEarlgreyPinmuxOutselGpioGpio2));

  dif_pinmux_pad_attr_t out_attr;
  dif_pinmux_pad_attr_t in_attr = {
      .slew_rate = 1,
      .drive_strength = 3,
      .flags = kDifPinmuxPadAttrPullResistorEnable |
               kDifPinmuxPadAttrPullResistorUp};
  dif_pinmux_index_t pads[] = {
      kTopEarlgreyMuxedPadsIoc6,
      kTopEarlgreyMuxedPadsIob1,
      kTopEarlgreyMuxedPadsIob2,
  };
  for (int i = 0; i < sizeof(pads) / sizeof(dif_pinmux_index_t); i++) {
    CHECK_DIF_OK(dif_pinmux_pad_write_attrs(
        &pinmux, pads[i], kDifPinmuxPadKindMio, in_attr, &out_attr));
  }

  base_addr = mmio_region_from_addr(TOP_EARLGREY_GPIO_BASE_ADDR);
  CHECK_DIF_OK(dif_gpio_init(base_addr, &screen_ctx->gpio));
  CHECK_DIF_OK(dif_gpio_output_set_enabled_all(&screen_ctx->gpio, 0xf));

  // Set LCD control pins:
  CHECK_DIF_OK(dif_gpio_write(&screen_ctx->gpio, GPIO_PIN_DC, 0));
  CHECK_DIF_OK(dif_gpio_write(&screen_ctx->gpio, GPIO_PIN_LED, 1));

  // Reset display:
  CHECK_DIF_OK(dif_gpio_write(&screen_ctx->gpio, GPIO_PIN_RESET, 0));
  timer_delay(1);
  CHECK_DIF_OK(dif_gpio_write(&screen_ctx->gpio, GPIO_PIN_RESET, 1));

  base_addr = mmio_region_from_addr(TOP_EARLGREY_SPI_HOST1_BASE_ADDR);
  CHECK_DIF_OK(dif_spi_host_init(base_addr, &screen_ctx->spi_host));
  CHECK_DIF_OK(dif_spi_host_configure(
      &screen_ctx->spi_host,
      (dif_spi_host_config_t){
          .spi_clock = 16000000,  // 16Mhz
          .peripheral_clock_freq_hz = (uint32_t)kClockFreqUsbHz,
      }));
  CHECK_DIF_OK(dif_spi_host_output_set_enabled(&screen_ctx->spi_host, true));

  lcd_iface.handle = screen_ctx;
  lcd_st7735_init(&screen_ctx->lcd, &lcd_iface);
  lcd_st7735_set_orientation(&screen_ctx->lcd, LCD_Rotate180);
  lcd_st7735_set_font(&screen_ctx->lcd, &lucidaConsole_10ptFont);
  lcd_st7735_set_font_colors(&screen_ctx->lcd, 0xFFFFFF, 0x000000);

  lcd_st7735_clean(&screen_ctx->lcd);
}

void screen_keep_alive(screen_context_t *screen_ctx) {
  static const char *status_icon[4] = {"|", "/", "-", "\\"};
  static uint32_t status_icon_index = 0;
  lcd_st7735_puts(&screen_ctx->lcd, (LCD_Point){.x = 160 - 8, .y = 128 - 12},
                  status_icon[status_icon_index++ % ARRAYSIZE(status_icon)]);
}

void screen_draw_logo(screen_context_t *screen_ctx) {
  lcd_st7735_draw_rgb565(
      &screen_ctx->lcd,
      (LCD_rectangle){.origin = {.x = 0, .y = 20}, .width = 160, .height = 39},
      (uint8_t *)logo_opentitan_160_39);

  lcd_st7735_puts(&screen_ctx->lcd, (LCD_Point){.x = 25, .y = 70},
                  "Symphony Mode");

  uint32_t colours[] = {
      0x0000ff, 0x00a5ff, 0x00ffff, 0x00ff00, 0xff0000,
  };

  for (uint32_t i = 0; i < sizeof(colours) / sizeof(uint32_t); i++) {
    lcd_st7735_fill_rectangle(&screen_ctx->lcd,
                              (LCD_rectangle){
                                  .origin = {.x = 0, .y = 90 + i * 5},
                                  .width = screen_ctx->lcd.parent.width,
                                  .height = 5,
                              },
                              colours[i]);
  }
}

static uint32_t spi_write(void *handle, uint8_t *data, size_t len) {
  screen_context_t *ctx = (screen_context_t *)handle;

  dif_spi_host_segment_t transaction = {
      .type = kDifSpiHostSegmentTypeTx,
      .tx =
          {
              .width = kDifSpiHostWidthStandard,
              .buf = data,
              .length = len,
          },
  };

  CHECK_DIF_OK(
      dif_spi_host_transaction(&ctx->spi_host, /*csid=*/0, &transaction, 1));

  ibex_timeout_t deadline = ibex_timeout_init(5000);
  dif_spi_host_status_t status;
  do {
    CHECK_DIF_OK(dif_spi_host_get_status(&ctx->spi_host, &status));
    if (ibex_timeout_check(&deadline)) {
      // Timeout.
      return 0;
    }
  } while (!status.tx_empty);

  return (uint32_t)len;
}

static uint32_t gpio_write(void *handle, bool cs, bool dc) {
  screen_context_t *ctx = (screen_context_t *)handle;

  CHECK_DIF_OK(dif_gpio_write(&ctx->gpio, GPIO_PIN_CS, cs));
  CHECK_DIF_OK(dif_gpio_write(&ctx->gpio, GPIO_PIN_DC, dc));

  return 0;
}

static void timer_delay(uint32_t ms) { busy_spin_micros(ms * 1000); }
