# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@rules_rust//rust:defs.bzl", "rust_library")

package(default_visibility = ["//visibility:public"])

rust_library(
    name = "crypto",
    srcs = [
        "src/aes256.rs",
        "src/cbc.rs",
        "src/ec/exponent256.rs",
        "src/ec/gfp256.rs",
        "src/ec/int256.rs",
        "src/ec/mod.rs",
        "src/ec/montgomery.rs",
        "src/ec/point.rs",
        "src/ec/precomputed.rs",
        "src/ecdh.rs",
        "src/ecdsa.rs",
        "src/hkdf.rs",
        "src/hmac.rs",
        "src/lib.rs",
        "src/sha256.rs",
        "src/util.rs",
    ],
    deps = [
        "@crate_index//:arrayref",
        "@crate_index//:subtle",
        "@crate_index//:hex",
        "@crate_index//:ring",
        "@crate_index//:untrusted",
        "@crate_index//:serde",
        "@crate_index//:serde_json",
        "@crate_index//:regex",
        "@crate_index//:rand_core",
        "@crate_index//:zeroize",
        "//sw/device/silicon_owner/tock/tests/ctap/byteorder",
    ],
)
