# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library")

package(default_visibility = ["//visibility:public"])

rust_library(
    name = "rand_core",
    srcs = [
        "src/blanket_impls.rs",
        "src/block.rs",
        "src/impls.rs",
        "src/le.rs",
        "src/lib.rs",
        "src/os.rs",
    ],
    deps = [
        "//sw/device/silicon_owner/tock/tests/ctap/zerocopy",
    ],
)
