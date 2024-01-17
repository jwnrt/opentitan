// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include "screen.h"

#include "display_drivers/st7735/lcd_st7735.h"
#include "string.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/runtime/print.h"

void screen_show_menu(St7735Context *lcd, Menu_t *menu, size_t selected) {
  size_t line = 0;
  // Clean the screen.
  lcd_st7735_fill_rectangle(lcd,
                            (LCD_rectangle){.origin = {.x = 0, .y = 0},
                                            .width = lcd->parent.width,
                                            .height = lcd->parent.font->height},
                            menu->color);

  // Invert background and foreground colors for the title.
  lcd_st7735_set_font_colors(lcd, menu->color, menu->background);
  screen_println(lcd, menu->title, alined_center, line++, false);
  // Set the colors for the menu items.
  lcd_st7735_set_font_colors(lcd, menu->background, menu->color);
  // Draw the menu items.
  for (int i = 0; i < menu->items_count; ++i) {
    screen_println(lcd, menu->items[i], alined_left, line++, false);
  }

  // Drow a boarder around the selected item.
  selected++;
  lcd_st7735_draw_horizontal_line(
      lcd,
      (LCD_Line){{.x = 0, .y = lcd->parent.font->height * selected},
                 lcd->parent.width},
      menu->selected_color);
  lcd_st7735_draw_horizontal_line(
      lcd,
      (LCD_Line){{.x = 0, .y = lcd->parent.font->height * (selected + 1) - 1},
                 lcd->parent.width},
      menu->selected_color);
  lcd_st7735_draw_vertical_line(
      lcd,
      (LCD_Line){{.x = 0, .y = lcd->parent.font->height * selected},
                 lcd->parent.font->height - 1},
      menu->selected_color);
  lcd_st7735_draw_vertical_line(
      lcd,
      (LCD_Line){{.x = lcd->parent.width - 1,
                  .y = lcd->parent.font->height * selected},
                 lcd->parent.font->height - 1},
      menu->selected_color);
}

void screen_println(St7735Context *lcd, const char *str,
                    TextAlignment_t alignment, size_t line, bool clean) {
  // Align the test in the left.
  LCD_Point pos = {.y = line * lcd->parent.font->height, .x = 0};

  if (alignment != alined_left) {
    // Align the text in the right.
    size_t str_width = strlen(str) * lcd->parent.font->descriptor_table->width;
    if (str_width < lcd->parent.width) {
      pos.x = lcd->parent.width - str_width;
    }
    if (alignment == alined_center) {
      // Align the test in the center.
      pos.x /= 2;
    }
  }

  if (clean) {
    // Clean the screen.
    lcd_st7735_fill_rectangle(
        lcd,
        (LCD_rectangle){.origin = {.x = 0, .y = pos.y},
                        .width = lcd->parent.width,
                        .height = lcd->parent.font->height},
        0xffffff);
  }
  // Draw the text.
  lcd_st7735_puts(lcd, pos, str);
}

void screen_profile_print(St7735Context *lcd, uint32_t cycles) {
  uint32_t clock_mhz = (uint32_t)kClockFreqCpuHz / 1000000;
  uint32_t time_micros = cycles / clock_mhz;
  char string[64] = {0};
  base_snprintf(string, sizeof(string), "Took %uK cycles", cycles / 1000);
  screen_println(lcd, string, alined_center, 8, true);
  base_snprintf(string, sizeof(string), "%ums @ %u MHz", time_micros / 1000,
                clock_mhz);
  screen_println(lcd, string, alined_center, 9, true);
}

size_t strlen(const char *str) {
  char *end = (char *)str;
  while (*end) {
    end++;
  }
  return (size_t)(end - str);
}