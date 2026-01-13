# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("//rules/opentitan:hw.bzl", "opentitan_top")
load(
    "//hw/top_earlgrey_1.0.0/data/autogen:defs.bzl",
    "EARLGREY_1_0_0_IPS",
    _EARLGREY_1_0_0_ALERTS = "EARLGREY_1_0_0_ALERTS",
)
load(
    "//hw/top_earlgrey_1.0.0/data/otp:defs.bzl",
    "EARLGREY_1_0_0_OTP_SIGVERIFY_FAKE_KEYS",
    "EARLGREY_1_0_0_STD_OTP_OVERLAYS",
)

EARLGREY_1_0_0 = opentitan_top(
    name = "earlgrey_1.0.0",
    hjson = "@earlgrey_1.0.0//hw/top_earlgrey/data/autogen:top_earlgrey.gen.hjson",
    top_lib = "@earlgrey_1.0.0//hw/top_earlgrey/sw/autogen:top_earlgrey",
    top_ld = "@earlgrey_1.0.0//hw/top_earlgrey_1.0.0/sw/autogen:top_earlgrey_memory",
    otp_map = "@earlgrey_1.0.0//hw/ip/otp_ctrl/data/otp:otp_ctrl_mmap.hjson",
    std_otp_overlay = EARLGREY_1_0_0_STD_OTP_OVERLAYS,
    otp_sigverify_fake_keys = EARLGREY_1_0_0_OTP_SIGVERIFY_FAKE_KEYS,
    ips = EARLGREY_1_0_0_IPS,
    # secret_cfgs = {
    #     "testing": "@earlgrey_1.0.0//hw/top_earlgrey/data/autogen:top_earlgrey.secrets.testing.gen.hjson",
    # },
)

EARLGREY_1_0_0_SLOTS = {
    "rom_ext_slot_a": "0x0",
    "rom_ext_slot_b": "0x80000",
    "owner_slot_a": "0x10000",
    "owner_slot_b": "0x90000",
    "rom_ext_size": "0x10000",
}

# Re-export.
EARLGREY_1_0_0_ALERTS = _EARLGREY_1_0_0_ALERTS

# Must match with hw/top_earlgrey/ip_autogen/alert_handler/data/alert_handler.hjson
# The order of this list must match the order of the alerts in the OTP. Do not
# use a set here.
EARLGREY_1_0_0_LOC_ALERTS = [
    "alert_pingfail",
    "esc_pingfail",
    "alert_integfail",
    "esc_integfail",
    "bus_integfail",
    "shadow_reg_update_error",
    "shadow_reg_storage_error",
]
