// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

pub mod alert {
    include!(concat!(env!(OUT_DIR), "alert.rs"));
}

pub mod dif {
    include!(concat!(env!("OUT_DIR"), "dif.rs"));
}

pub mod earlgrey {
    include!(concat!(env!("OUT_DIR"), "earlgrey.rs"));
}

pub mod hardened {
    include!(concat!(env!("OUT_DIR"), "hardened.rs"));
}

pub mod multibits {
    include!(concat!(env!("OUT_DIR"), "multibits.rs"));
}
