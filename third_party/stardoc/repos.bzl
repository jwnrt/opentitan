# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def stardoc_repos():
    http_archive(
        name = "io_bazel_stardoc",
        sha256 = "3082a199f39e1fd9ed608421516fdbe9e9af8eb34f52e46e9a8c4798c8e6bfad",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/stardoc/releases/download/0.6.1/stardoc-0.6.1.tar.gz",
            "https://github.com/bazelbuild/stardoc/releases/download/0.6.1/stardoc-0.6.1.tar.gz",
        ],
    )
