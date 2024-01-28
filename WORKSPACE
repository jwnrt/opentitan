# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# If you're planning to add to this file, please read
# //third_party/README.md first.

workspace(name = "lowrisc_opentitan")

# CRT is the Compiler Repository Toolkit.  It contains the configuration for
# the windows compiler.
load("//third_party/crt:repos.bzl", "crt_repos")
crt_repos()
load("@crt//:repos.bzl", "crt_repos")
crt_repos()
load("@crt//:deps.bzl", "crt_deps")
crt_deps()
load("@crt//config:registration.bzl", "crt_register_toolchains")
crt_register_toolchains(riscv32 = True)

# Tools for release automation
load("//third_party/github:repos.bzl", "github_tools_repos")
github_tools_repos()

# Protobuf Toolchain
load("//third_party/protobuf:repos.bzl", "protobuf_repos")
protobuf_repos()
load("//third_party/protobuf:deps.bzl", "protobuf_deps")
protobuf_deps()

# FreeRTOS; used by the OTTF
load("//third_party/freertos:repos.bzl", "freertos_repos")
freertos_repos()

# LLVM Compiler Runtime for Profiling
load("//third_party/llvm_compiler_rt:repos.bzl", "llvm_compiler_rt_repos")
llvm_compiler_rt_repos()

# RISC-V Compliance Tests
load("//third_party/riscv-compliance:repos.bzl", "riscv_compliance_repos")
riscv_compliance_repos()

# CoreMark benchmark
load("//third_party/coremark:repos.bzl", "coremark_repos")
coremark_repos()

# The standard Keccak algorithms
load("//third_party/xkcp:repos.bzl", "xkcp_repos")
xkcp_repos()

# HSM related repositories (SoftHSM2, etc)
load("//third_party/hsm:repos.bzl", "hsm_repos")
hsm_repos()

# NIST CAVP Test Vectors
load("//third_party/nist_cavp_testvectors:repos.bzl", "nist_cavp_repos")
nist_cavp_repos()

# Bitstreams from https://storage.googleapis.com/opentitan-bitstreams/
load("//rules:bitstreams.bzl", "bitstreams_repo")
bitstreams_repo(name = "bitstreams")

# Setup for linking in external test hooks for both secure/non-secure
# manufacturer domains.
load("//rules:hooks_setup.bzl", "hooks_setup", "secure_hooks_setup")
hooks_setup(
    name = "hooks_setup",
    dummy = "sw/device/tests/closed_source",
)
secure_hooks_setup(
    name = "secure_hooks_setup",
    dummy = "sw/device/tests/closed_source",
)

# Declare the external test_hooks repositories. One for both manufacturer secure
# and non-secure domains.
load("@hooks_setup//:repos.bzl", "hooks_repo")
load("@secure_hooks_setup//:repos.bzl", "secure_hooks_repo")
hooks_repo(name = "manufacturer_test_hooks")
secure_hooks_repo(name = "secure_manufacturer_test_hooks")

# The nonhermetic_repo imports environment variables needed to run vivado.
load("//rules:nonhermetic.bzl", "nonhermetic_repo")
nonhermetic_repo(name = "nonhermetic")

# Binary firmware image for HyperDebug
load("//third_party/hyperdebug:repos.bzl", "hyperdebug_repos")
hyperdebug_repos()

register_toolchains(
    "//rules/opentitan:localtools",
)
