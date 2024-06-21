# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library")
load("//rules/opentitan:defs.bzl", "OPENTITAN_CPU")

rust_library(
    name = "persistent_store",
    srcs = [
        "src/buffer.rs",
        "src/concat.rs",
        "src/driver.rs",
        "src/file.rs",
        "src/format/bitfield.rs",
        "src/format.rs",
        "src/fragment.rs",
        "src/lib.rs",
        "src/linear.rs",
        "src/model.rs",
        "src/storage.rs",
        "src/store.rs",
        "src/test.rs",
    ],
    target_compatible_with = [OPENTITAN_CPU],
)

