use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::time::Duration;

use anyhow::{bail, ensure, Context};
use serde::Deserialize;
use serialport::TTYPort;

/// Interface to QEMU's monitor.
///
/// The monitor is expected to be configured in `control` mode for the JSON QMP
/// protocol, not "human" mode.
pub struct Monitor {
    /// TTY port connected to QEMU's monitor.
    tty: BufReader<TTYPort>,

    /// Incrementing ID attached to each command and checked with each response.
    id_counter: usize,
}

impl Monitor {
    /// Connect to the QEMU monitor over a given TTY.
    pub fn new<P: AsRef<Path>>(tty_path: P) -> anyhow::Result<Self> {
        let tty = serialport::new(
            tty_path.as_ref().to_str().context("TTY path not UTF8")?,
            115200,
        )
        .timeout(Duration::from_secs(1))
        .open_native()
        .context("failed to open QEMU monitor PTY")?;

        let mut tty = BufReader::new(tty);

        // QMP sends us a greeting line on every connection:
        let mut greeting = String::new();
        tty.read_line(&mut greeting)
            .context("expected greeting line from QEMU monitor")?;

        // Check the greeting:
        let Greeting {
            qmp: Qmp { version, .. },
        } = serde_json::from_str(greeting.as_str()).context("failed to parse QEMU QMP greeting")?;
        log::info!(
            "connected to QEMU version {major}.{minor}.{micro}",
            major = version.qemu.major,
            minor = version.qemu.minor,
            micro = version.qemu.micro
        );

        let mut monitor = Monitor { tty, id_counter: 0 };

        // Negotiate capabilities.
        // We don't need any, but the protocol requires us to do this.
        monitor.send_cmd("qmp_capabilities", None)?;

        Ok(monitor)
    }

    /// Create a PTY for the given chardev ID, returning its path.
    pub fn create_pty(&mut self, id: &str) -> anyhow::Result<PathBuf> {
        let arguments =
            format!(r#"{{ "id": "{id}", "backend": {{ "type": "pty", "data": {{}} }} }}"#);
        let response = self.send_cmd("chardev-change", Some(&arguments))?;

        let tty = response
            .get("pty")
            .context("expected `pty` response")?
            .as_str()
            .context("expected `pty` response to be string")?;

        Ok(PathBuf::from(tty))
    }

    /// Send a continue command either starting or resuming the emulation.
    pub fn r#continue(&mut self) -> anyhow::Result<()> {
        self.send_cmd("cont", None)?;

        Ok(())
    }

    /// Stop the emulation (resumable, does not quit QEMU).
    pub fn stop(&mut self) -> anyhow::Result<()> {
        self.send_cmd("stop", None)?;

        Ok(())
    }

    /// Reset the system within the emulation.
    pub fn reset(&mut self) -> anyhow::Result<()> {
        self.send_cmd("system_reset", None)?;

        Ok(())
    }

    /// Gracefully shut down QEMU and terminate the process.
    pub fn quit(&mut self) -> anyhow::Result<()> {
        self.send_cmd("quit", None)?;

        Ok(())
    }

    /// List the IDs of the currently configured `chardev`s.
    pub fn query_chardev(&mut self) -> anyhow::Result<Vec<String>> {
        let response = self.send_cmd("query-chardev", None)?;

        let mut chardevs = Vec::new();
        for chardev in response.as_array().context("expected array of chardevs")? {
            let label = chardev
                .get("label")
                .context("expected chardev to have label")?
                .as_str()
                .context("expected chardev label to be string")?;
            chardevs.push(label.to_string());
        }

        Ok(chardevs)
    }

    /// Send a command over the JSON QMK interface.
    ///
    /// The protocol goes:
    ///
    /// 1. Send a command with the form `{ "execute": <cmd>, "arguments": <obj>, "id": <val> }`.
    /// 2. Skip any asyncronous event responses that arrived before the command.
    /// 3. Check the response for success (with optional value) or error.
    fn send_cmd(&mut self, cmd: &str, args: Option<&str>) -> anyhow::Result<serde_json::Value> {
        let id = self.id_counter;

        write!(self.tty.get_mut(), r#"{{ "execute": "{cmd}""#)?;
        if let Some(args) = args {
            write!(self.tty.get_mut(), r#", "arguments": {args}"#)?;
        }
        writeln!(self.tty.get_mut(), r#", "id": {id} }}"#)?;

        // Increment the ID for the next message.
        self.id_counter += 1;

        // Find the response for this message, skipping over asyncronous events that came in
        // before we sent our command.
        let response = loop {
            let mut line = String::new();
            self.tty.read_line(&mut line)?;

            let response: MonitorResponse = serde_json::from_str(&line)
                .with_context(|| format!("unexpected response: {line}"))?;

            // Skip asyncronous event responses.
            if let MonitorResponse::Event { .. } = response {
                continue;
            }

            break response;
        };

        // Check for success/failure, extracting the return value for successes.
        match response {
            MonitorResponse::Success {
                r#return,
                id: resp_id,
            } => {
                ensure!(id == resp_id, "response ID did not match request ID");
                Ok(r#return)
            }
            MonitorResponse::Error { id: resp_id, error } => {
                ensure!(id == resp_id, "response ID did not match request ID");
                bail!("monitor returned error: {error:#?}");
            }
            MonitorResponse::Event { .. } => unreachable!("should have been skipped"),
        }
    }
}

/// Possible responses from the server.
#[derive(Deserialize)]
#[serde(untagged)]
enum MonitorResponse {
    /// Command with `id` was successful and has optional return value.
    Success {
        id: usize,
        r#return: serde_json::Value,
    },
    /// Command with `id` gave an error with some extra details.
    Error { id: usize, error: serde_json::Value },
    /// Asyncronous event arrived outside of our commands.
    Event {
        #[serde(rename = "event")]
        _event: String,
        #[serde(rename = "timestamp")]
        _timestamp: serde_json::Value,
        #[serde(rename = "data", default)]
        _data: serde_json::Value,
    },
}

/// Greeting message when connecting to QEMU.
#[derive(Deserialize)]
struct Greeting {
    #[serde(alias = "QMP")]
    qmp: Qmp,
}

/// QMP protocol information.
#[derive(Deserialize)]
struct Qmp {
    /// Current version.
    version: Version,
    /// Optional capabilities of the monitor.
    #[serde(rename = "capabilities")]
    _capabilities: Vec<String>,
}

/// QMP version information.
#[derive(Deserialize)]
struct Version {
    qemu: QemuVersion,

    #[serde(rename = "package")]
    _package: String,
}

/// QEMU version information.
#[derive(Deserialize)]
struct QemuVersion {
    major: usize,
    minor: usize,
    micro: usize,
}
