#!/usr/bin/env bash

# Copyright lowRISC contributors (OpenTitan project).
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

set -e

fail() {
  echo "$1" >&2
  exit 1
}

# Mandatory environment variables:
if [ ! -f "$QEMU_BIN"      ]; then fail "expected QEMU_BIN to point to QEMU binary"; done
if [ ! -f "$QEMU_CONFIG"   ]; then fail "expected QEMU_CONFIG to point to QEMU configuration file"; done
if [ ! -f "$QEMU_ROM"      ]; then fail "expected QEMU_ROM to point to QEMU ROM image"; done
if [ ! -f "$QEMU_OTP"      ]; then fail "expected QEMU_OTP to point to QEMU OTP image"; done
if [ ! -f "$QEMU_FLASH"    ]; then fail "expected QEMU_FLASH to point to QEMU flash image"; done
if [ ! -f "$QEMU_SPIFLASH" ]; then fail "expected QEMU_SPIFLASH to point to SPI flash image"; done

# Optional environment variables:
QEMU_ICOUNT="${QEMU_ICOUNT:-6}"
QEMU_LOG="${QEMU_LOG:-/dev/fd/1}"
QEMU_MONITOR="${QEMU_MONITOR:-qemu-monitor}"

qemu_args=(
  # Disable GUI features.
  "-display" "none"

  # Select Earlgrey 1.0.0 machine.
  "-M" "ot-earlgrey"

  # Load RTL constants from config file.
  "-readconfig" "$QEMU_CONFIG"

  # Fork to background once initialization has finished.
  "-daemonize"
  
  # Hold CPU stopped until GDB or the QEMU monitor starts it.
  "-S"

  # Send logs to a file.
  "-D" "$QEMU_LOG"
  "-d" "guest_errors,unimp"

  # Configure the ROM, OTP, and Flash data.
  "-object" "ot-rom_img,id=rom,file=${QEMU_ROM}"
  "-drive" "if=pflash,file=${QEMU_OTP},format=raw"
  "-drive" "if=mtd,id=eflash,bus=2,file=${QEMU_FLASH},format=raw"

  # Scale the CPU clock (1MHz >> `icount`).
  "-icount" "shift=${QEMU_ICOUNT}"

  # Do not quit QEMU on fatal resets - some tests need these.
  "-global" "ot-rstmgr.fatal_reset=0"

  # Because flash info pages are not spliced and QEMU does not currently
  # support flash scrambling/ECCs, any uninitialised seeds read from the flash
  # creator/owner secret pages will be all `0xFF...`. This will cause the
  # keymgr to error when advancing to the OwnerIntermediate state, preventing
  # further use. Temporarily disable the relevant keymgr data validity check
  # via an opt-in QEMU property.
  # TODO: remove this property when either QEMU flash info page splicing
  # is available, or the QEMU `flash_ctrl` implements scrambling & ECC support.
  "-global" "ot-keymgr.disable-flash-seed-check=true"

  # By default QEMU will exit when the test status register is written.
  # OpenTitanTool expects to be able to do multiple resets, for example after
  # bootstrapping, and then execute the test. Resetting could cause the test
  # to run, finish, and exit, which we don't want to happen.
  "-global" "ot-ibex_wrapper.dv-sim-status-exit=off"

  # To enable limited support for UART rescue in the ROM_EXT, we need to
  # be able to toggle break signals on/off in QEMU's UART and mock this
  # in the oversampled `VAL` register.
  "-global" "ot-uart.oversample-break=true"
  "-global" "ot-uart.toggle-break=true"

  # QEMU will by default interpret quit commands over JTAG to the TAP Ctrls
  # as signals to exit VM execution. We want to be able to disconnect from
  # JTAG without stopping execution completely for tests.
  "-global" "tap-ctrl-rbb.quit=false"

  # Configure the monitor in QMP mode with a PTY.
  "-chardev" "pty,id=monitor,path=qemu-monitor"
  "-mon" "chardev=monitor,mode=control"

  # Connect log device to PTY (only used for optional pass/fail message).
  "-chardev" "pty,id=log"
  "-global" "ot-ibex_wrapper.logdev=log"

  # Connect UARTs to PTYs.
  "-chardev" "pty,id=uart0"
  "-chardev" "pty,id=uart1"
  "-chardev" "pty,id=uart2"
  "-chardev" "pty,id=uart3"
  "-serial" "chardev:uart0"
  "-serial" "chardev:uart1"
  "-serial" "chardev:uart2"
  "-serial" "chardev:uart3"

  # Connect SPI device to PTY.
  "-chardev" "pty,id=spidev"

  # Attach SPI flash to SPI Host 0/SPI Device bus. Chosen model is W25Q256 (32MiB)
  "-global" "ot-earlgrey-board.spiflash0=w25q256"
  "-drive" "if=mtd,file=${QEMU_SPIFLASH},format=raw,bus=0"

  # Connect I2Cs to PTYs.
  "-chardev" "pty,id=i2c0"
  "-chardev" "pty,id=i2c1"
  "-chardev" "pty,id=i2c2"
  "-device" "ot-i2c_host_proxy,bus=ot-i2c0,chardev=i2c0"
  "-device" "ot-i2c_host_proxy,bus=ot-i2c1,chardev=i2c1"
  "-device" "ot-i2c_host_proxy,bus=ot-i2c2,chardev=i2c2"

  # Connect GPIO interface to PTY.
  "-chardev" "pty,id=gpio"
  "-global" "ot-gpio-eg.chardev=gpio"

  # Connect USB command and protocol interfaces to PTYs.
  "-chardev" "pty,id=usbdev-cmd"
  "-chardev" "pty,id=usbdev-host"

  # Connect JTAG remote bit-bang interfaces to sockets.
  "-chardev" "socket,id=taprbb,path=qemu-jtag.sock,server=on,wait=off"
)

echo "Starting QEMU:"
set -x
"$QEMU_BIN" "${qemu_args[@]}" "$@"
