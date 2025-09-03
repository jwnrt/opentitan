// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

pub mod monitor;
pub mod reset;

use std::cell::RefCell;
use std::rc::Rc;
use std::str::FromStr;

use anyhow::{bail, ensure, Context};

use crate::backend::qemu::QemuOpts;
use crate::io::gpio::{GpioError, GpioPin};
use crate::io::uart::Uart;
use crate::transport::common::uart::SerialPortUart;
use crate::transport::qemu::monitor::Monitor;
use crate::transport::qemu::reset::QemuReset;
use crate::transport::{
    Capabilities, Capability, Transport, TransportError, TransportInterfaceType,
};

const UART_BAUD: u32 = 115200;

/// Represents a connection to a running QEMU emulation.
pub struct Qemu {
    /// Connection to the QEMU monitor which can control the emulator.
    _monitor: Rc<RefCell<Monitor>>,

    /// Reset pin (actually goes via the `monitor`).
    reset: Rc<dyn GpioPin>,

    /// Console UART.
    uart: Option<Rc<dyn Uart>>,
}

impl Qemu {
    /// Creates a QEMU transport connection from `options`.
    ///
    /// The transport will configure capabilities that it can find from the running QEMU
    /// instance. It looks for the following chardevs which should be connected to QEMU devices.
    ///
    /// You can create a chardev with `-chardev null,id=<id>` and connect it using:
    ///
    /// * `console` - connect to UART using `-serial chardev:console`.
    pub fn from_options(options: QemuOpts) -> anyhow::Result<Self> {
        let mut monitor = Monitor::new(options.qemu_monitor_tty.unwrap())?;

        // Get list of configured chardevs from QEMU.
        let chardevs = monitor.query_chardev()?;

        // If there's a chardev called `console`, configure it as a PTY and use as UART.
        let uart = match chardevs.iter().any(|c| c == "console") {
            true => {
                // Ask the monitor to create a PTY for the console UART:
                let console_pty = monitor.create_pty("console")?;
                let uart: Rc<dyn Uart> = Rc::new(
                    SerialPortUart::open_pseudo(console_pty.to_str().unwrap(), UART_BAUD)
                        .context("failed to open QEMU console PTY")?,
                );
                Some(uart)
            }
            false => None,
        };

        let monitor = Rc::new(RefCell::new(monitor));

        // Resetting is done over the monitor, but we model it like a pin to enable strapping it.
        let reset = QemuReset::new(Rc::clone(&monitor));
        let reset = Rc::new(reset);

        Ok(Qemu {
            _monitor: monitor,
            reset,
            uart,
        })
    }
}

impl Transport for Qemu {
    fn capabilities(&self) -> anyhow::Result<Capabilities> {
        // We have to unconditionally claim to support GPIO because of the reset
        // pin which actually goes via the monitor. Attempting to use a non-reset
        // GPIO pin in `.gpio_pin` will cause an error if GPIO isn't connected.
        let mut cap = Capability::GPIO;

        if self.uart.is_some() {
            cap |= Capability::UART;
        }

        Ok(Capabilities::new(cap))
    }

    fn uart(&self, instance: &str) -> anyhow::Result<Rc<dyn Uart>> {
        ensure!(
            instance == "0",
            TransportError::InvalidInstance(TransportInterfaceType::Uart, instance.to_string())
        );
        Ok(Rc::clone(self.uart.as_ref().unwrap()))
    }

    fn gpio_pin(&self, instance: &str) -> anyhow::Result<Rc<dyn GpioPin>> {
        let pin = u8::from_str(instance).with_context(|| format!("can't convert {instance:?}"))?;

        if pin < 32 {
            bail!("GPIO interface not currently supported");
        } else if pin == 255 {
            Ok(Rc::clone(&self.reset))
        } else {
            Err(GpioError::InvalidPinNumber(pin).into())
        }
    }
}
