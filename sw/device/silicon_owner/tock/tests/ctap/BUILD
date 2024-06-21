# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("@rules_rust//cargo:defs.bzl", "cargo_build_script")
load("//rules:signing.bzl", "sign_bin")
load("//rules:tock.bzl", "TOCK_ENVS", "tock_elf2tab", "tock_image")
load("//rules/opentitan:defs.bzl", "EARLGREY_SILICON_OWNER_ROM_EXT_ENVS", "OPENTITAN_CPU", "cw310_params", "opentitan_test", "silicon_params")

package(default_visibility = ["//visibility:public"])

cargo_build_script(
    name = "opensk_build_script",
    srcs = ["build.rs"],
    data = [
        "crypto_data/aaguid.txt",
        "crypto_data/opensk_upgrade_pub.pem",
    ],
    deps = [
        "//sw/device/silicon_owner/tock/tests/ctap/cbor",
        "@crate_index//:uuid",
        "@crate_index//:openssl",
    ],
)

rust_library(
    name = "ctap2",
    srcs = [
        "src/lib.rs",
        "src/env/mod.rs",
        "src/env/tock/buffer_upgrade_storage.rs",
        "src/env/tock/clock.rs",
        "src/env/tock/commands.rs",
        "src/env/tock/mod.rs",
        "src/env/tock/phantom_buffer_storage.rs",
        "src/env/tock/storage.rs",
        "src/env/tock/storage_helper.rs",
        "src/env/tock/upgrade_helper.rs",
    ],
    # We specifically restrict our build target to the OpenTitan
    # CPU because libtock does not support an x86_64 target.
    target_compatible_with = [OPENTITAN_CPU],
    deps = [
        "//sw/device/silicon_owner/tock/apps:single_app_layout",
        "//sw/device/silicon_owner/tock/tests/ctap/opensk",
        "//sw/device/silicon_owner/tock/tests/ctap/cbor",
        "//sw/device/silicon_owner/tock/tests/ctap/crypto",
        "//sw/device/silicon_owner/tock/tests/ctap/persistent_store",
        "//sw/device/silicon_owner/tock/tests/ctap/byteorder",
        "//sw/device/silicon_owner/tock/tests/ctap/rand_core",
        "//sw/device/silicon_owner/tock/tests/ctap/lang-items",
        "//sw/device/silicon_owner/tock/tests/ctap/libtock-drivers",
        "@libtock//apis/alarm",
        "@libtock//apis/buttons",
        "@libtock//apis/console",
        "@libtock//apis/leds",
        "@libtock//runtime",
        "@libtock//platform",
        "@crate_index//:arrayref",
        ":opensk_build_script",
    ],
    aliases = {
        "//sw/device/silicon_owner/tock/tests/ctap/cbor": "sk_cbor",
    },
)

rust_binary(
    name = "ctap",
    srcs = [
        "src/main.rs",
    ],
    deps = [
        ":ctap2",
        "@crate_index//:arrayref",
        "//sw/device/silicon_owner/tock/tests/ctap/lang-items",
        "//sw/device/silicon_owner/tock/tests/ctap/libtock-drivers",
        "//sw/device/silicon_owner/tock/tests/ctap/byteorder",
        "//sw/device/silicon_owner/tock/tests/ctap/opensk",
        "@libtock//runtime",
    ],
)

tock_elf2tab(
    name = "tab",
    src = ":ctap",
    arch = "rv32imc",
    flash_start = 0x20040000,
)

tock_image(
    name = "image_fpga",
    testonly = True,
    apps = [":tab"],
    exec_env = "//hw/top_earlgrey:fpga_cw310_sival_rom_ext",
    kernel = "//sw/device/silicon_owner/tock/tests:test_kernel_fpga",
)

tock_image(
    name = "image_silicon",
    apps = [":tab"],
    exec_env = "//hw/top_earlgrey:silicon_owner_proda_rom_ext",
    kernel = "//sw/device/silicon_owner/tock/tests:test_kernel_silicon",
)

opentitan_test(
    name = "ctap_demo",
    exec_env = TOCK_ENVS,
    cw310 = cw310_params(
        binaries = {":image_fpga": "firmware"},
    ),
    silicon = silicon_params(
        binaries = {":image_silicon": "firmware"},
        interface = "ftdi",
    ),
)
