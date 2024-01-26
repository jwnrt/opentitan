# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load(
    "//rules/opentitan:defs.bzl",
    "opentitan_test",
)
load(
    "//rules:opentitan_test.bzl",
    "cw340_params",
    "opentitan_functest",
)

package(default_visibility = ["//visibility:public"])

opentitan_test(
    name = "spi_display",
    srcs = [
        "main.c",
    ],
    copts = [
        "-nostdlib",
        "-ffreestanding",
        # Disable the date-time warning only for the hello world program.
        "-Wno-date-time",
    ],  
    exec_env = {
        "//hw/top_earlgrey:silicon_owner_sival_rom_ext": None,
        # "//hw/top_earlgrey:fpga_cw340_test_rom": None,
        # "//hw/top_earlgrey:silicon_owner_prodc_rom_ext": None,
    },
    deps = [
        ":spi_display_app",
        "//sw/device/lib/base:mmio",
    ],
)

opentitan_functest(
    name = "spi_display_fpga",
    srcs = [
        "main.c",
        
    ],
    copts = [
        "-nostdlib",
        "-ffreestanding",
        # Disable the date-time warning only for the hello world program.
        "-Wno-date-time",
    ],  
    manifest = "//sw/device/silicon_creator/rom_ext:manifest_standard",
    targets = [
        "cw340_test_rom",
    ],
    deps = [
        ":spi_display_app",
        "//sw/device/lib/base:mmio",
    ],
)

cc_library(
    name = "spi_display_app",
    srcs = [
        "app.c",
        "screen.c",
        "aes_demo.c",
    ],
    hdrs = [
        "app.h",
        "demos.h",
        "context.h",
        "screen.h",
    ],
    deps = [
        ":spi_display_lib",
        "//sw/device/examples/spi_display/images:logo_ibex",
        "//sw/device/examples/spi_display/images:logo_opentitan_160_39px",
        "//sw/device/examples/spi_display/images:shape_opentitan_80_80px",
        "//sw/device/examples/spi_display/images:logo_lowrisc_05_80px",
        "//sw/device/examples/spi_display/images:logo_tux_80_95px",
        "//sw/device/examples/spi_display/images:ot_stronks_160_100px",
        "//sw/device/examples/spi_display/display_drivers:spi_display_driver_lib",
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
        "//sw/device/lib/dif:spi_host",
        "//sw/device/lib/dif:uart",
        "//sw/device/lib/runtime:hart",
        "//sw/device/lib/runtime:log",
        "//sw/device/lib/runtime:print",
        "//sw/device/lib/testing:aes_testutils",
        "//sw/device/lib/testing:pinmux_testutils",
        "//sw/device/lib/testing:profile",
        "//sw/device/lib/testing/test_framework:check",
        "//sw/device/lib/testing/test_framework:ottf_main",
        "//sw/device/lib/testing/test_framework:ottf_start",
        "//sw/device/lib/testing/test_framework:ottf_test_config",
    ],
)

