// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use std::fs;
use std::io::{BufReader, Read};
use std::path::PathBuf;
use std::time::Duration;

use anyhow::{Context, Result};
use clap::Parser;
use object::{Object, ObjectSymbol};

use opentitanlib::app::TransportWrapper;
use opentitanlib::execute_test;
use opentitanlib::io::uart::Uart;
use opentitanlib::test_utils;
use opentitanlib::test_utils::init::InitializeTest;
use opentitanlib::test_utils::mem::{MemRead32Req, MemWrite32Req, MemWriteReq};
use opentitanlib::uart::console::UartConsole;

#[derive(Debug, Parser)]
struct Opts {
    #[command(flatten)]
    init: InitializeTest,

    /// Console receive timeout.
    #[arg(long, value_parser = humantime::parse_duration, default_value = "600s")]
    timeout: Duration,

    /// Path to the ELF file being tested on the device.
    #[arg(long)]
    firmware_elf: PathBuf,
}

struct TestData {
    expected_data: Vec<u8>,
    uart_idx: u8,
    uart_idx_addr: u32,
    baud_rate_addr: u32,
}

fn main() -> Result<()> {
    let opts = Opts::parse();
    opts.init.init_logging();

    let elf_file = fs::read(&opts.firmware_elf).context("failed to read ELF")?;
    let object = object::File::parse(elf_file.as_ref()).context("failed to parse ELF")?;

    let expected_data = test_utils::object::symbol_data(&object, "kSendData")?;
    eprintln!("{expected_data:?}");

    let uart_idx_addr = object
        .symbols()
        .find(|symbol| symbol.name() == Ok("uart_idx"))
        .context("failed to find uart_idx symbol")?
        .address() as u32;

    let baud_rate_addr = object
        .symbols()
        .find(|symbol| symbol.name() == Ok("baud_rate"))
        .context("failed to find baud_rate symbol")?
        .address() as u32;

    let transport = opts.init.init_target()?;
    let uart_console = transport.uart("console")?;

    for uart_idx in 0..4 {
        transport.reset_target(Duration::from_millis(500), true)?;

        let test_data = TestData {
            expected_data: expected_data.clone(),
            uart_idx,
            uart_idx_addr,
            baud_rate_addr,
        };

        execute_test!(uart_tx_rx, &opts, &transport, &*uart_console, &test_data);
    }

    Ok(())
}

/// Send and receive data with a device's UART.
fn uart_tx_rx(
    opts: &Opts,
    transport: &TransportWrapper,
    console: &dyn Uart,
    test_data: &TestData,
) -> Result<()> {
    let TestData {
        expected_data,
        uart_idx,
        uart_idx_addr,
        baud_rate_addr,
    } = test_data;

    UartConsole::wait_for(console, r"waiting for commands", opts.timeout)?;
    MemWriteReq::execute(console, *uart_idx_addr, &[*uart_idx])?;

    UartConsole::wait_for(console, r"Starting test[^\n]*\n", opts.timeout)?;

    let baud_rate = MemRead32Req::execute(console, *baud_rate_addr)?;
    MemWrite32Req::execute(console, *baud_rate_addr, 0)?;

    let uart = transport.uart("dut")?;
    uart.set_baudrate(baud_rate)?;

    UartConsole::wait_for(console, r"Receiving data[^\n]*\n", opts.timeout)?;

    log::info!("Sending data...");
    uart.write(&expected_data).context("failed to send data")?;
    log::info!("data sent {}", expected_data.len());

    UartConsole::wait_for(console, r"Data sent[^\n]*\n", opts.timeout)?;
    log::info!("Reading data...");
    let mut data = vec![0u8; expected_data.len()];

    let mut buf_reader = BufReader::new(&*uart);
    buf_reader
        .read_exact(&mut data)
        .context("failed to read data")?;

    assert_eq!(data, *expected_data);

    UartConsole::wait_for(console, r"PASS![^\r\n]*", opts.timeout)?;

    Ok(())
}
