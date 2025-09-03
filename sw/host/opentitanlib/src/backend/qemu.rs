// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

use std::path::PathBuf;

use anyhow::Result;
use clap::Args;

use crate::transport::qemu::Qemu;
use crate::transport::Transport;

#[derive(Clone, Debug, Args)]
pub struct QemuOpts {
    #[arg(long, required_if_eq("interface", "qemu"))]
    pub qemu_monitor_tty: Option<PathBuf>,

    #[arg(long, required = false)]
    pub qemu_args: Vec<String>,
}

pub fn create(args: &QemuOpts) -> Result<Box<dyn Transport>> {
    Ok(Box::new(Qemu::from_options(args.clone())?))
}
