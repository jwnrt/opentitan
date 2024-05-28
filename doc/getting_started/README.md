# Getting started

Welcome!
This guide will help you get OpenTitan up and running.

## Execution environments

Running OpenTitan software requires an environment to run it in.
The following environments are supported:

* [Verilator simulation][].
* [FPGA synthesis][] ([CW305][], [CW310][], and [CW340][] boards supported).
* [DV simulation][] in commercial simulators such as VCS and XCelium.
* Engineering sample silicon with the "teacup" board.

This page will cover the steps required for all environments.
See the next chapters for help with each individual environment.

If you are new to the project, we recommend simulation with Verilator.
This uses only free tools and does not require any additional hardware such as an FPGA.

## Preparing a development environment

### Step 0: cloning the OpenTitan repository

First, clone the OpenTitan repository:

```sh
git clone https://github.com/lowRISC/opentitan
cd opentitan
```

Throughout this documentation we will refer to this directory as `$REPO_TOP`.

### Step 1: installing dependencies

We officially support Ubuntu 20.04 LTS which is where we run our automated tests.
If you're not using Ubuntu, we recommend using our experimental [Docker container][].

The following guides may help you with other Linux distributions, but we do not officially support them:

* [RedHat / Fedora][./unofficial/fedora.md]

To install package dependencies on apt-based distros like Ubuntu:

```sh
sed '/^#/d' ./apt-requirements.txt | xargs sudo apt install -y
```

To install Python dependencies:

```sh
pip3 install --user -r python-requirements.txt --require-hashes
```

The `--user` flag installs dependencies to `~/.local/bin` by default.
To prepend this directory to your path, add the following to `~/.bashrc` or equivalent:

```sh
export PATH="${HOME}/.local/bin:${PATH}"
```

[Verilator simulation]: ./setup_verilator.md
[FPGA synthesis]: ./setup_fpga.md
[CW305]: https://rtfm.newae.com/Targets/CW305%20Artix%20FPGA/
[CW310]: https://rtfm.newae.com/Targets/CW310%20Bergen%20Board/
[CW340]: https://rtfm.newae.com/Targets/CW340%20Luna%20Board/
[DV simulation]: ./setup_dv.md
[Docker container]: https://github.com/lowRISC/opentitan/tree/master/util/container
