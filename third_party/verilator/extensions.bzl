# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

verilator = module_extension(
    implementation = lambda _: _verilator_repos(),
)

def _verilator_repos():
    http_archive(
        name = "verilator",
        url = "https://github.com/jwnrt/verilator/releases/download/4.210/verilator-b8a9c875d33bba90a1a5902a69d5e85a86937d8a.tar.gz",
        sha256 = "31f58abc3a68a3fd04e758945cb166e99339e3260adedca1522b93b9b265cb2e",
        build_file = ":BUILD.verilator.bazel",
        strip_prefix = "repo",
    )
