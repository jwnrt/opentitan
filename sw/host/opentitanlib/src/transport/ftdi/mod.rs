// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use anyhow::{ensure, Result, Context};
use serde_annotate::Annotate;
use std::any::Any;
use std::cell::RefCell;
use std::collections::hash_map::Entry;
use std::collections::HashMap;
use std::rc::Rc;

use crate::io::gpio::GpioPin;
use crate::io::spi::Target;
use crate::io::uart::Uart;
use crate::transport::common::uart::SerialPortUart;
use crate::transport::{
    Capabilities, Capability, Transport, TransportError, TransportInterfaceType,
};
use crate::util::parse_int::ParseInt;

use chip::Chip;
use ftdi_embedded_hal as ftdi_hal;

pub mod chip;
pub mod gpio;
pub mod spi;

#[derive(Default)]
struct Inner {
    spi: Option<Rc<dyn Target>>,
    gpio: HashMap<String, Rc<dyn GpioPin>>,
    uart: HashMap<u32, Rc<dyn Uart>>,
}

pub struct Ftdi<C: Chip> {
    pub(crate) ftdi_interfaces: HashMap<ftdi::Interface, ftdi_hal::FtHal<ftdi::Device>>,
    inner: RefCell<Inner>,
    phantom: std::marker::PhantomData<C>,
}

impl<C: Chip> Ftdi<C> {
    pub fn new() -> anyhow::Result<Self> {
        let mut ftdi_interfaces = HashMap::new();
        for interface in C::INTERFACES {
            let device = ftdi::find_by_vid_pid(C::VENDOR_ID, C::PRODUCT_ID)
                .interface(*interface)
                .open()?;
            ftdi_interfaces.insert(*interface, ftdi_hal::FtHal::init_freq(device, 8_000_000)?);
        }

        let ftdi_dev = Ftdi {
            ftdi_interfaces,
            inner: RefCell::default(),
            phantom: std::marker::PhantomData,
        };
        Ok(ftdi_dev)
    }

    fn open_uart(&self, instance: u32) -> Result<SerialPortUart> {
        let uart_name = format!("/dev/ttyUSB{instance}");
        SerialPortUart::open(&uart_name, C::UART_BAUD).context(uart_name)
    }
}

impl<C: Chip> Transport for Ftdi<C> {
    fn capabilities(&self) -> Result<Capabilities> {
        Ok(Capabilities::new(
            Capability::SPI | Capability::GPIO | Capability::UART | Capability::UART_NONBLOCKING,
        ))
    }

    fn uart(&self, instance: &str) -> Result<Rc<dyn Uart>> {
        let mut inner = self.inner.borrow_mut();
        let instance = u32::from_str(instance).ok().ok_or_else(|| {
            TransportError::InvalidInstance(TransportInterfaceType::Uart, instance.to_string())
        })?;
        let uart = match inner.uart.entry(instance) {
            Entry::Vacant(v) => {
                let u = v.insert(Rc::new(self.open_uart(instance)?));
                Rc::clone(u)
            }
            Entry::Occupied(o) => Rc::clone(o.get()),
        };
        Ok(uart)
    }

    fn gpio_pin(&self, pinname: &str) -> Result<Rc<dyn GpioPin>> {
        let mut inner = self.inner.borrow_mut();
        Ok(match inner.gpio.entry(pinname.to_string()) {
            Entry::Vacant(v) => {
                let u = v.insert(Rc::new(gpio::Pin::open::<C>(
                    &self.ftdi_interfaces,
                    pinname.to_string(),
                )?));
                Rc::clone(u)
            }
            Entry::Occupied(o) => Rc::clone(o.get()),
        })
    }

    fn spi(&self, instance: &str) -> Result<Rc<dyn Target>> {
        let mut inner = self.inner.borrow_mut();
        if inner.spi.is_none() {
            inner.spi = Some(Rc::new(spi::Spi::open(
                &self.ftdi_interfaces,
                gpio::Pin::open::<C>(&self.ftdi_interfaces, "bdbus3".to_string())?,
            )?));
        }
        Ok(Rc::clone(inner.spi.as_ref().unwrap()))
    }

    fn dispatch(&self, action: &dyn Any) -> Result<Option<Box<dyn Annotate>>> {
        Err(TransportError::UnsupportedOperation.into())
    }
}

/// Command for Transport::dispatch().
pub struct SetPll {}

/// Command for Transport::dispatch(). Resets the Chip whisperer board's SAM3X chip.
pub struct ResetSam3x {}
