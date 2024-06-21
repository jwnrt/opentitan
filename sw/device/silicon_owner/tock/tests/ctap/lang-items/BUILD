# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library")

package(default_visibility = ["//visibility:public"])

rust_library(
    name = "lang-items",
    srcs = [
        "src/allocator.rs",
        "src/lib.rs",
        "src/panic_handler.rs",
        "src/util.rs",
    ],
    deps = [
        "@crate_index//:linked_list_allocator",
        "@libtock//apis/alarm",
        "@libtock//apis/leds",
        "@libtock//apis/low_level_debug",
        "@libtock//platform",
        "@libtock//runtime",
    ],
)

