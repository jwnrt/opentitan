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

static const uint8_t kKeyShare1[16] = {
    0x0f, 0x1f, 0x2f, 0x3F, 0x4f, 0x5f, 0x6f, 0x7f,
    0x8f, 0x9f, 0xaf, 0xbf, 0xcf, 0xdf, 0xef, 0xff,
};

static const unsigned char kAesModesKey128[16] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81};

const char *aes_mode_to_string(dif_aes_mode_t mode) {
  // clang-format off
  switch (mode) {
    case kDifAesModeEcb: return "ECB";
    case kDifAesModeCbc: return "CBC";
    case kDifAesModeCfb: return "CFB";
    case kDifAesModeOfb: return "OFB";
    case kDifAesModeCtr: return "CTR";
    default:  return "Unknown";
  }
  // clang-format on
  return "Unknown";
}

status_t run_aes(context_t *app, dif_aes_mode_t mode) {
  LCD_rectangle rectangle = {
      .origin = {.x = 0, .y = 0}, .width = 80, .height = 95};
  uint8_t *plain_image = (uint8_t *)tmux_80_95;
  int image_len = sizeof(tmux_80_95);
  // uint8_t *plain_image = (uint8_t *)ot_shape_80_80;
  // int image_len = sizeof(ot_shape_80_80);

  lcd_st7735_clean(app->lcd);
  lcd_st7735_draw_rgb565(app->lcd, rectangle, plain_image);

  char string[64] = {0};
  base_snprintf(string, sizeof(string), "AES %s", aes_mode_to_string(mode));

  screen_println(app->lcd, " Will encrypt the", alined_center, 7, true);
  screen_println(app->lcd, " image above with ", alined_center, 8, true);
  screen_println(app->lcd, string, alined_center, 9, true);

  for (size_t i = 5; i > 0; i--) {
    size_t len = base_snprintf(string, sizeof(string), " %u     ", i);
    string[len] = 0;
    screen_println(app->lcd, string, alined_right, 3, false);
    busy_spin_micros(1000 * 1000);
  }
  screen_println(app->lcd, " 0     ", alined_right, 3, false);

  uint8_t key_share0[sizeof(kAesModesKey128)];
  for (int i = 0; i < sizeof(kAesModesKey128); ++i) {
    key_share0[i] = kAesModesKey128[i] ^ kKeyShare1[i];
  }

  dif_aes_key_share_t key;
  memcpy(key.share0, key_share0, sizeof(key.share0));
  memcpy(key.share1, kKeyShare1, sizeof(key.share1));
  dif_aes_iv_t iv = {.iv = {0xeb, 0x10, 0x15, 0xca}};

  dif_aes_transaction_t transaction = {
      .operation = kDifAesOperationEncrypt,
      .mode = mode,
      .key_len = kDifAesKey128,
      .key_provider = kDifAesKeySoftwareProvided,
      .mask_reseeding = kDifAesReseedPer64Block,
      .manual_operation = kDifAesManualOperationAuto,
      .reseed_on_key_change = false,
      .ctrl_aux_lock = false,
  };
  rectangle.origin.x = rectangle.width;
  lcd_st7735_rgb565_start(app->lcd, rectangle);

  uint64_t profile = profile_start();
  TRY(dif_aes_start(app->aes, &transaction, &key, &iv));

  dif_aes_data_t in_data_plain;
  memcpy(in_data_plain.data, plain_image, sizeof(in_data_plain.data));
  TRY(dif_aes_load_data(app->aes, in_data_plain));
  do {
    AES_TESTUTILS_WAIT_FOR_STATUS(app->aes, kDifAesStatusOutputValid, true,
                                  5000);
    dif_aes_data_t out_data;
    TRY(dif_aes_read_output(app->aes, &out_data));

    plain_image += sizeof(kKeyShare1);
    image_len -= sizeof(kKeyShare1);
    memcpy(in_data_plain.data, plain_image, sizeof(in_data_plain.data));
    // Load the plain text to trigger the encryption operation.
    AES_TESTUTILS_WAIT_FOR_STATUS(app->aes, kDifAesStatusInputReady, true,
                                  5000);
    TRY(dif_aes_load_data(app->aes, in_data_plain));

    lcd_st7735_rgb565_put(app->lcd, (uint8_t *)&out_data.data,
                          sizeof(out_data.data));
  } while (image_len > 0);

  lcd_st7735_rgb565_finish(app->lcd);
  TRY(dif_aes_end(app->aes));

  uint32_t cycles = profile_end(profile);
  uint32_t clock_mhz = (uint32_t)kClockFreqCpuHz / 1000000;
  uint32_t time_micros = cycles / clock_mhz;

  screen_println(app->lcd, "Encrypted in", alined_center, 7, true);
  size_t len = base_snprintf(string, sizeof(string), "~%u Mi CPU cycles",
                             cycles / 1000000);
  string[len] = 0;
  screen_println(app->lcd, string, alined_center, 8, true);
  LOG_INFO("%s", string);
  len = base_snprintf(string, sizeof(string), "or %u ms @ %u MHz",
                      time_micros / 1000, clock_mhz);
  string[len] = 0;
  screen_println(app->lcd, string, alined_center, 9, true);

  return OK_STATUS();
}
