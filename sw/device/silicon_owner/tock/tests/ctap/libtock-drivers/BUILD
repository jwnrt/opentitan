# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library")

package(default_visibility = ["//visibility:public"])

rust_library(
    name = "libtock-drivers",
    srcs = [
        "src/crp.rs",
        "src/lib.rs",
        "src/nfc.rs",
        "src/result.rs",
        "src/rng.rs",
        "src/storage.rs",
        "src/timer.rs",
        "src/usb_ctap_hid.rs",
        "src/util.rs",
    ],
    deps = [
        "@libtock//apis/alarm",
        "@libtock//apis/console",
        "@libtock//platform",
    ],
)

