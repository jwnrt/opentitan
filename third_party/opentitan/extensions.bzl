# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _opentitan_repos():
    http_archive(
        name = "earlgrey_1.0.0",
        url = "https://github.com/lowRISC/opentitan/archive/refs/tags/Earlgrey-PROD-M6.tar.gz",
        sha256 = "6bcc33f89538868f2da39a49277c08ce2ea69ef8d8a328c19158aee9a367c20e",
        strip_prefix = "opentitan-Earlgrey-PROD-M6",
    )

opentitan = module_extension(
    implementation = lambda _: _opentitan_repos(),
)
