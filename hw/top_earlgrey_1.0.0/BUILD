# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load(
    "//rules/opentitan:defs.bzl",
    "DEFAULT_TEST_FAILURE_MSG",
    "DEFAULT_TEST_SUCCESS_MSG",
    "sim_qemu",
)
load("//hw/top_earlgrey_1.0.0:defs.bzl", "EARLGREY_1_0_0_SLOTS")

sim_qemu(
    name = "sim_qemu_base",
    design = "earlgrey",
    exec_env = "sim_qemu_base",
    libs = [
        "//sw/device/lib/arch:boot_stage_rom_ext",
        "//sw/device/lib/arch:sim_qemu",
        "//hw/top_earlgrey_1.0.0/sw/dt:sim_qemu",
    ],
    linker_script = "//sw/device/lib/testing/test_framework:ottf_ld_silicon_creator_slot_a",
    otp = "//hw/top_earlgrey_1.0.0/data/otp:img_rma",
    slot_spec = EARLGREY_1_0_0_SLOTS,
)
