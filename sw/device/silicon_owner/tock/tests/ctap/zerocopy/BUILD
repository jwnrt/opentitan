# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library", "rust_proc_macro")

package(default_visibility = ["//visibility:public"])

rust_proc_macro(
    name = "zerocopy-derive",
    srcs = [
        "zerocopy-derive/src/lib.rs",
        "zerocopy-derive/src/ext.rs",
        "zerocopy-derive/src/repr.rs",
    ],
    deps = [
        "@crate_index//:proc-macro2",
        "@crate_index//:quote",
        "@crate_index//:syn",
    ],
)

rust_library(
    name = "zerocopy",
    srcs = [
        "src/byte_slice.rs",
        "src/byteorder.rs",
        "src/deprecated.rs",
        "src/error.rs",
        "src/impls.rs",
        "src/layout.rs",
        "src/lib.rs",
        "src/macro_util.rs",
        "src/macros.rs",
        "src/pointer/aliasing_safety.rs",
        "src/pointer/mod.rs",
        "src/pointer/ptr.rs",
        "src/ref.rs",
        "src/util.rs",
        "src/wrappers.rs",
    ],
    proc_macro_deps = [
        ":zerocopy-derive",
    ],
)

