#include <st7735/lcd_st7735.h>

#include "sw/device/lib/dif/dif_gpio.h"
#include "sw/device/lib/dif/dif_spi_host.h"

typedef struct screen_context {
  St7735Context lcd;
  dif_gpio_t gpio;
  dif_spi_host_t spi_host;
} screen_context_t;

void screen_init(screen_context_t *screen_ctx);
void screen_draw_logo(screen_context_t *screen_ctx);
void screen_keep_alive(screen_context_t *screen_ctx);
