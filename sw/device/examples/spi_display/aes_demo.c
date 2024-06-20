// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>

#include "context.h"
#include "demos.h"
// #include "lowrisc_logo.h"
#include "images/logo_tux_80_95.h"
// #include "ot_shape_80x80.h"
#include "screen.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/aes_testutils.h"
#include "sw/device/lib/testing/profile.h"

 static dif_aes_transaction_t transaction = {
      .operation = kDifAesOperationEncrypt,
      .mode = kDifAesModeEcb,
      .key_len = kDifAesKey128,
      .key_provider = kDifAesKeySoftwareProvided,
      .mask_reseeding = kDifAesReseedPer64Block,
      .manual_operation = kDifAesManualOperationAuto,
      .reseed_on_key_change = false,
      .ctrl_aux_lock = false,
  };

static const uint8_t kKeyShare1[16] = {
    0x0f, 0x1f, 0x2f, 0x3F, 0x4f, 0x5f, 0x6f, 0x7f,
    0x8f, 0x9f, 0xaf, 0xbf, 0xcf, 0xdf, 0xef, 0xff,
};

static const unsigned char kAesModesKey128[16] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81};

static status_t encrypt_and_display(context_t *app, uint8_t *plain_image, int image_len);

status_t run_aes(context_t *app) {
  char fmt_str[64] = {0};
  LCD_rectangle rectangle = {
      .origin = {.x = 0, .y = 12}, .width = 80, .height = 95};
  uint8_t *plain_image = (uint8_t *)tmux_80_95;
  int image_len = sizeof(tmux_80_95);

  lcd_st7735_clean(&app->lcd);
  lcd_st7735_set_font_colors(&app->lcd, BGRColorBlue, BGRColorWhite);
  screen_println(&app->lcd, "ECB      CBC", alined_center, 0, true);
  lcd_st7735_set_font_colors(&app->lcd, BGRColorWhite, BGRColorBlue);

  lcd_st7735_draw_rgb565(&app->lcd, rectangle, plain_image);
  rectangle.origin.x = rectangle.width;
  lcd_st7735_draw_rgb565(&app->lcd, rectangle, plain_image);

  screen_println(&app->lcd, "Will encrypt the", alined_center, 8, true);
  screen_println(&app->lcd, "images with OT AES ", alined_center, 9, true);

  for (int i = 3; i >= 0; i--) {
    size_t len = base_snprintf(fmt_str, sizeof(fmt_str), "%u", i);
    fmt_str[len] = 0;
    screen_println(&app->lcd, fmt_str, alined_center, 3, false);
    if (i > 0){
      busy_spin_micros(1000 * 1000);
    }
  }

  uint8_t key_share0[sizeof(kAesModesKey128)];
  for (int i = 0; i < sizeof(kAesModesKey128); ++i) {
    key_share0[i] = kAesModesKey128[i] ^ kKeyShare1[i];
  }

  dif_aes_key_share_t key;
  memcpy(key.share0, key_share0, sizeof(key.share0));
  memcpy(key.share1, kKeyShare1, sizeof(key.share1));
  dif_aes_iv_t iv = {.iv = {0xeb, 0x10, 0x15, 0xca}};

  uint64_t profile = profile_start();

  rectangle.origin.x = 0;
  lcd_st7735_rgb565_start(&app->lcd, rectangle);
  transaction.mode = kDifAesModeEcb;
  TRY(dif_aes_start(&app->aes, &transaction, &key, &iv));
  TRY(encrypt_and_display(app, plain_image, image_len));
  lcd_st7735_rgb565_finish(&app->lcd);
  TRY(dif_aes_end(&app->aes));


  rectangle.origin.x = rectangle.width;
  lcd_st7735_rgb565_start(&app->lcd, rectangle);
  transaction.mode = kDifAesModeCbc;
  TRY(dif_aes_start(&app->aes, &transaction, &key, &iv));
  TRY(encrypt_and_display(app, plain_image, image_len));
  lcd_st7735_rgb565_finish(&app->lcd);
  TRY(dif_aes_end(&app->aes));

  uint32_t cycles = profile_end(profile);
  uint32_t clock_mhz = (uint32_t)kClockFreqCpuHz / 1000000;
  uint32_t time_micros = cycles / clock_mhz;

  size_t len = base_snprintf(fmt_str, sizeof(fmt_str), "Took ~%uM CPU cycles",
                             cycles / 1000000);
  fmt_str[len] = 0;
  screen_println(&app->lcd, fmt_str, alined_center, 8, true);
  len = base_snprintf(fmt_str, sizeof(fmt_str), "or %u ms @ %u MHz",
                      time_micros / 1000, clock_mhz);
  fmt_str[len] = 0;
  screen_println(&app->lcd, fmt_str, alined_center, 9, true);

  return OK_STATUS();
}

static status_t encrypt_and_display(context_t *app, uint8_t *plain_image, int image_len){
  dif_aes_data_t in_data_plain;
  memcpy(in_data_plain.data, plain_image, sizeof(in_data_plain.data));
  TRY(dif_aes_load_data(&app->aes, in_data_plain));
  do {
    AES_TESTUTILS_WAIT_FOR_STATUS(&app->aes, kDifAesStatusOutputValid, true,
                                  5000);
    dif_aes_data_t out_data;
    TRY(dif_aes_read_output(&app->aes, &out_data));

    plain_image += sizeof(kKeyShare1);
    image_len -= sizeof(kKeyShare1);
    memcpy(in_data_plain.data, plain_image, sizeof(in_data_plain.data));
    // Load the plain text to trigger the encryption operation.
    AES_TESTUTILS_WAIT_FOR_STATUS(&app->aes, kDifAesStatusInputReady, true,
                                  5000);
    TRY(dif_aes_load_data(&app->aes, in_data_plain));

    lcd_st7735_rgb565_put(&app->lcd, (uint8_t *)&out_data.data,
                          sizeof(out_data.data));
  } while (image_len > 0);
  return OK_STATUS();
}
