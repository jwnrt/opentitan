# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load(
    "@bazel_skylib//lib:dicts.bzl",
    "dicts",
)
load(
    "//rules/opentitan:defs.bzl",
    "EARLGREY_SILICON_OWNER_ROM_EXT_ENVS",
    "cw310_params",
    "opentitan_test",
    "silicon_params",
)

package(default_visibility = ["//visibility:public"])

opentitan_test(
    name = "spi_display_demo",
    srcs = [
        "main.c",
    ],
    copts = [
        "-nostdlib",
        "-ffreestanding",
        # Disable the date-time warning only for the hello world program.
        "-Wno-date-time",
    ],
    exec_env = dicts.add(
        EARLGREY_SILICON_OWNER_ROM_EXT_ENVS,
        {
            "//hw/top_earlgrey:fpga_cw340_test_rom": None,
        },
    ),
    silicon = silicon_params(
        interface = "ftdi",
    ),
    deps = [
        ":spi_display_app",
        "//sw/device/lib/base:mmio",
    ],
)

cc_library(
    name = "spi_display_app",
    srcs = [
        "aes_demo.c",
        "ctap.c",
        "app.c",
        "screen.c",
    ],
    hdrs = [
        "app.h",
        "context.h",
        "ctap.h",
        "demos.h",
        "screen.h",
    ],
    deps = [
        ":spi_display_lib",
        ":spi_display_driver_lib",
        "//sw/device/examples/spi_display/images:logo_ibex",
        "//sw/device/examples/spi_display/images:logo_lowrisc_05_80px",
        "//sw/device/examples/spi_display/images:logo_opentitan_160_39px",
        "//sw/device/examples/spi_display/images:logo_tux_80_95px",
        "//sw/device/examples/spi_display/images:ot_stronks_160_100px",
        "//sw/device/examples/spi_display/images:shape_opentitan_80_80px",
    ],
)

cc_library(
    name = "spi_display_lib",
    deps = [
        "//hw/top_earlgrey/sw/autogen:top_earlgrey",
        "//sw/device/examples:demos",
        "//sw/device/lib/arch:device",
        "//sw/device/lib/crt",
        "//sw/device/lib/dif:aes",
        "//sw/device/lib/dif:gpio",
        "//sw/device/lib/dif:pinmux",
        "//sw/device/lib/dif:spi_device",
        "//sw/device/lib/dif:spi_host",
        "//sw/device/lib/dif:uart",
        "//sw/device/lib/dif:rv_plic",
        "//sw/device/lib/runtime:hart",
        "//sw/device/lib/runtime:log",
        "//sw/device/lib/runtime:print",
        "//sw/device/lib/testing:aes_testutils",
        "//sw/device/lib/testing:pinmux_testutils",
        "//sw/device/lib/testing:profile",
        "//sw/device/lib/testing:spi_device_testutils",
        "//sw/device/lib/testing:spi_flash_testutils",
        "//sw/device/lib/testing:rv_plic_testutils",
        "//sw/device/lib/testing/test_framework:check",
        "//sw/device/lib/testing/test_framework:ottf_main",
        "//sw/device/lib/testing/test_framework:ottf_start",
        "//sw/device/lib/testing/test_framework:ottf_test_config",
    ],
)
cc_library(
    name = "spi_display_driver_lib",
    srcs = [
        "display_drivers/core/lcd_base.c",
        "display_drivers/core/lucida_console_10pt.c",
        "display_drivers/st7735/lcd_st7735.c",
    ],
    hdrs = [
        "display_drivers/core/font.h",
        "display_drivers/core/lcd_base.h",
        "display_drivers/core/lucida_console_10pt.h",
        "display_drivers/core/lucida_console_12pt.h",
        "display_drivers/st7735/lcd_st7735.h",
        "display_drivers/st7735/lcd_st7735_cmds.h",
        "display_drivers/st7735/lcd_st7735_init.h",
    ],
)
