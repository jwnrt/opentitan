use std::env;
use std::fs;
use std::path::Path;
use std::process::{Command, Stdio};

/// Relative path to the root of the OpenTitan repo.
const PATH_TO_ROOT: &str = "../../..";

/// ujson headers to process from C to Rust.
const UJSON_HEADERS: [UjsonHeader; 5] = [
    UjsonHeader {
        input: "sw/device/lib/testing/json/command.h",
        output: "command.rs",
    },
    UjsonHeader {
        input: "sw/device/lib/testing/json/gpio.h",
        output: "gpio.rs",
    },
    UjsonHeader {
        input: "sw/device/lib/testing/json/i2c_target.h",
        output: "i2c_target.rs",
    },
    UjsonHeader {
        input: "sw/device/lib/testing/json/pinmux_config.h",
        output: "pinmux_config.rs",
    },
    UjsonHeader {
        input: "sw/device/lib/testing/json/spi_passthru.h",
        output: "spi_passthru.rs",
    },
];

/// Mapping of a ujson header file to a Rust module output.
struct UjsonHeader {
    /// Input path to a ujson C header file.
    ///
    /// Relative to the repo's root.
    input: &'static str,
    /// Output path to write the Rust module to.
    ///
    /// Relative to the `OUT_DIR` directory.
    output: &'static str,
}

fn main() {
    UJSON_HEADERS.iter().for_each(generate_ujson);
}

fn generate_ujson(UjsonHeader { input, output }: &UjsonHeader) {
    let input = Path::new(PATH_TO_ROOT).join(input);
    if !input.exists() {
        panic!("file {} doesn't exist", input.display());
    }

    println!("cargo:rerun-if-changed={}", input.display());

    let clang_output = Command::new("clang")
        .arg("-E") // Run the preprocessor only.
        .arg("-nostdinc") // Do not include the standard libraries.
        .arg("-DNOSTDINC=1")
        .arg("-DRUST_PREPROCESSOR_EMIT=1") // Enable outputting of Rust code.
        .arg("-Dopentitanlib=crate") // Replace instances of `opentitanlib` with `crate`.
        .arg(format!("-I{PATH_TO_ROOT}")) // Add the project root for includes.
        .arg(input.as_os_str())
        .stderr(Stdio::inherit()) // Propagate errors.
        .stdout(Stdio::piped()) // Capture output.
        .output()
        .expect("failed to execute clang");

    if !clang_output.status.success() {
        panic!("clang failed to preprocess headers");
    }

    let rust_file = String::from_utf8_lossy(&clang_output.stdout);

    // Strip empty and C-preprocessor lines.
    let rust_file = rust_file
        .lines()
        .filter(|line| !line.is_empty() && !line.starts_with("#"))
        .collect::<Vec<_>>()
        .join("\n");

    // Replace `rust_attr` with the `#` character.
    let rust_file = rust_file.replace("rust_attr", "#");

    let out_dir = env::var("OUT_DIR").unwrap();
    let output = Path::new(&out_dir).join(output);
    fs::write(&output, rust_file)
        .expect(format!("failed to write module {}", output.display()).as_str());
}
