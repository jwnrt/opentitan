// Copyright lowRISC contributors (OpenTitan project).
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

//! Board file for LowRISC OpenTitan RISC-V development platform.
//!
//! - <https://opentitan.org/>

#![no_std]
// Disable this attribute when documenting, as a workaround for
// https://github.com/rust-lang/rust/issues/62184.
#![cfg_attr(not(doc), no_main)]
#![feature(custom_test_frameworks)]
#![test_runner(test_runner)]
#![reexport_test_harness_main = "test_main"]

use crate::hil::symmetric_encryption::AES128_BLOCK_SIZE;
use crate::otbn::OtbnComponent;
use crate::pinmux_layout::BoardPinmuxLayout;
use capsules_aes_gcm::aes_gcm;
use capsules_core::rng::{Entropy32ToRandom, RngDriver};
use capsules_core::virtualizers::virtual_aes_ccm;
use capsules_core::virtualizers::virtual_alarm::{MuxAlarm, VirtualMuxAlarm};
use capsules_system::process_policies::PanicFaultPolicy;
use capsules_system::process_printer::ProcessPrinterText;
use earlgrey::chip::EarlGreyDefaultPeripherals;
use earlgrey::chip_config::EarlGreyConfig;
use earlgrey::epmp::EPMPDebugConfig;
use earlgrey::pinmux_config::EarlGreyPinmuxConfig;
use kernel::capabilities;
use kernel::component::Component;
use kernel::hil;
use kernel::hil::entropy::Entropy32;
use kernel::hil::i2c::I2CMaster;
use kernel::hil::led::LedHigh;
use kernel::hil::rng::Rng;
use kernel::hil::symmetric_encryption::AES128;
use kernel::platform::scheduler_timer::VirtualSchedulerTimer;
use kernel::platform::{KernelResources, SyscallDriverLookup, TbfHeaderFilterDefaultAllow};
use kernel::scheduler::priority::PrioritySched;
use kernel::utilities::registers::interfaces::ReadWriteable;
use kernel::{create_capability, debug, static_init};
use lowrisc::csrng::CsRng;
use rv32i::csr;

pub mod io;
mod otbn;
mod pinmux_layout;

const NUM_PROCS: usize = 4;

//
// Actual memory for holding the active process structures. Need an empty list
// at least.
static mut PROCESSES: [Option<&'static dyn kernel::process::Process>; 4] = [None; NUM_PROCS];

// Test access to the peripherals
#[cfg(test)]
static mut PERIPHERALS: Option<&'static EarlGreyDefaultPeripherals<ChipConfig, BoardPinmuxLayout>> =
    None;
// Test access to board
#[cfg(test)]
static mut BOARD: Option<&'static kernel::Kernel> = None;
// Test access to platform
#[cfg(test)]
static mut PLATFORM: Option<&'static EarlGrey> = None;
// Test access to main loop capability
#[cfg(test)]
static mut MAIN_CAP: Option<&dyn kernel::capabilities::MainLoopCapability> = None;
// Test access to alarm
static mut ALARM: Option<
    &'static MuxAlarm<'static, earlgrey::timer::RvTimer<'static, ChipConfig>>,
> = None;
// Test access to AES
static mut AES: Option<
    &aes_gcm::Aes128Gcm<
        'static,
        virtual_aes_ccm::VirtualAES128CCM<'static, earlgrey::aes::Aes<'static>>,
    >,
> = None;
// Test access to SipHash
static mut SIPHASH: Option<&capsules_extra::sip_hash::SipHasher24<'static>> = None;
// Test access to RSA
static mut RSA_HARDWARE: Option<&lowrisc::rsa::OtbnRsa<'static>> = None;

// Test access to a software SHA256
#[cfg(test)]
static mut SHA256SOFT: Option<&capsules_extra::sha256::Sha256Software<'static>> = None;

static mut CHIP: Option<&'static EarlGreyChip> = None;
static mut PROCESS_PRINTER: Option<&'static ProcessPrinterText> = None;

// How should the kernel respond when a process faults.
const FAULT_RESPONSE: PanicFaultPolicy = PanicFaultPolicy {};

/// Dummy buffer that causes the linker to reserve enough space for the stack.
#[no_mangle]
#[link_section = ".stack_buffer"]
pub static mut STACK_MEMORY: [u8; 0x1400] = [0; 0x1400];

enum ChipConfig {}

#[cfg(feature = "fpga")]
impl EarlGreyConfig for ChipConfig {
    const NAME: &'static str = "fpga_cw310";
    const CPU_FREQ: u32 = 24_000_000;
    const PERIPHERAL_FREQ: u32 = 6_000_000;
    const AON_TIMER_FREQ: u32 = 250_000;
    const UART_BAUDRATE: u32 = 115200;
}

#[cfg(feature = "silicon")]
impl EarlGreyConfig for ChipConfig {
    const NAME: &'static str = "silicon";
    const CPU_FREQ: u32 = 100_000_000;
    const PERIPHERAL_FREQ: u32 = 24_000_000;
    const AON_TIMER_FREQ: u32 = 200_000;
    const UART_BAUDRATE: u32 = 115200;
}

type EarlGreyChip = earlgrey::chip::EarlGrey<
    'static,
    { earlgrey::epmp::EPMPDebugDisable::TOR_USER_REGIONS },
    EarlGreyDefaultPeripherals<'static, ChipConfig, BoardPinmuxLayout>,
    ChipConfig,
    BoardPinmuxLayout,
    earlgrey::epmp::EarlGreyEPMP<false, earlgrey::epmp::EPMPDebugDisable>,
>;

/// A structure representing this platform that holds references to all
/// capsules for this platform. We've included an alarm and console.
struct EarlGrey {
    led: &'static capsules_core::led::LedDriver<
        'static,
        LedHigh<'static, earlgrey::gpio::GpioPin<'static, earlgrey::pinmux::PadConfig>>,
        8,
    >,
    gpio: &'static capsules_core::gpio::GPIO<
        'static,
        earlgrey::gpio::GpioPin<'static, earlgrey::pinmux::PadConfig>,
    >,
    console: &'static capsules_core::console::Console<'static>,
    alarm: &'static capsules_core::alarm::AlarmDriver<
        'static,
        VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<'static, ChipConfig>>,
    >,
    hmac: &'static capsules_extra::hmac::HmacDriver<'static, lowrisc::hmac::Hmac<'static>, 32>,
    lldb: &'static capsules_core::low_level_debug::LowLevelDebug<
        'static,
        capsules_core::virtualizers::virtual_uart::UartDevice<'static>,
    >,
    i2c_master:
        &'static capsules_core::i2c_master::I2CMasterDriver<'static, lowrisc::i2c::I2c<'static>>,
    spi_controller: &'static capsules_core::spi_controller::Spi<
        'static,
        capsules_core::virtualizers::virtual_spi::VirtualSpiMasterDevice<
            'static,
            lowrisc::spi_host::SpiHost<'static>,
        >,
    >,
    rng: &'static RngDriver<'static, Entropy32ToRandom<'static, CsRng<'static>>>,
    aes: &'static capsules_extra::symmetric_encryption::aes::AesDriver<
        'static,
        aes_gcm::Aes128Gcm<
            'static,
            virtual_aes_ccm::VirtualAES128CCM<'static, earlgrey::aes::Aes<'static>>,
        >,
    >,
    syscall_filter: &'static TbfHeaderFilterDefaultAllow,
    scheduler: &'static PrioritySched,
    scheduler_timer: &'static VirtualSchedulerTimer<
        VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<'static, ChipConfig>>,
    >,
    watchdog: &'static lowrisc::aon_timer::AonTimer,
}

/// Mapping of integer syscalls to objects that implement syscalls.
impl SyscallDriverLookup for EarlGrey {
    fn with_driver<F, R>(&self, driver_num: usize, f: F) -> R
    where
        F: FnOnce(Option<&dyn kernel::syscall::SyscallDriver>) -> R,
    {
        match driver_num {
            capsules_core::led::DRIVER_NUM => f(Some(self.led)),
            capsules_extra::hmac::DRIVER_NUM => f(Some(self.hmac)),
            capsules_core::gpio::DRIVER_NUM => f(Some(self.gpio)),
            capsules_core::console::DRIVER_NUM => f(Some(self.console)),
            capsules_core::alarm::DRIVER_NUM => f(Some(self.alarm)),
            capsules_core::low_level_debug::DRIVER_NUM => f(Some(self.lldb)),
            capsules_core::i2c_master::DRIVER_NUM => f(Some(self.i2c_master)),
            capsules_core::spi_controller::DRIVER_NUM => f(Some(self.spi_controller)),
            capsules_core::rng::DRIVER_NUM => f(Some(self.rng)),
            capsules_extra::symmetric_encryption::aes::DRIVER_NUM => f(Some(self.aes)),
            _ => f(None),
        }
    }
}

impl KernelResources<EarlGreyChip> for EarlGrey {
    type SyscallDriverLookup = Self;
    type SyscallFilter = TbfHeaderFilterDefaultAllow;
    type ProcessFault = ();
    type Scheduler = PrioritySched;
    type SchedulerTimer = VirtualSchedulerTimer<
        VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<'static, ChipConfig>>,
    >;
    type WatchDog = lowrisc::aon_timer::AonTimer;
    type ContextSwitchCallback = ();

    fn syscall_driver_lookup(&self) -> &Self::SyscallDriverLookup {
        &self
    }
    fn syscall_filter(&self) -> &Self::SyscallFilter {
        &self.syscall_filter
    }
    fn process_fault(&self) -> &Self::ProcessFault {
        &()
    }
    fn scheduler(&self) -> &Self::Scheduler {
        self.scheduler
    }
    fn scheduler_timer(&self) -> &Self::SchedulerTimer {
        &self.scheduler_timer
    }
    fn watchdog(&self) -> &Self::WatchDog {
        &self.watchdog
    }
    fn context_switch_callback(&self) -> &Self::ContextSwitchCallback {
        &()
    }
}

unsafe fn setup() -> (
    &'static kernel::Kernel,
    &'static EarlGrey,
    &'static EarlGreyChip,
    &'static EarlGreyDefaultPeripherals<'static, ChipConfig, BoardPinmuxLayout>,
) {
    // Ibex-specific handler
    earlgrey::chip::configure_trap_handler();

    let earlgrey_epmp = earlgrey::epmp::EarlGreyEPMP::new(
        earlgrey::epmp::FlashRegion(
            rv32i::pmp::NAPOTRegionSpec::new(
                core::ptr::addr_of!(_sflash),
                core::ptr::addr_of!(_eflash) as usize - core::ptr::addr_of!(_sflash) as usize,
            )
            .unwrap(),
        ),
        earlgrey::epmp::RAMRegion(
            rv32i::pmp::NAPOTRegionSpec::new(
                core::ptr::addr_of!(_ssram),
                core::ptr::addr_of!(_esram) as usize - core::ptr::addr_of!(_ssram) as usize,
            )
            .unwrap(),
        ),
        earlgrey::epmp::MMIORegion(
            rv32i::pmp::NAPOTRegionSpec::new(
                0x40000000 as *const u8, // start
                0x10000000,              // size
            )
            .unwrap(),
        ),
        earlgrey::epmp::KernelTextRegion(
            rv32i::pmp::TORRegionSpec::new(
                core::ptr::addr_of!(_stext),
                core::ptr::addr_of!(_etext),
            )
            .unwrap(),
        ),
    )
    .unwrap();

    BoardPinmuxLayout::setup();

    // initialize capabilities
    let process_mgmt_cap = create_capability!(capabilities::ProcessManagementCapability);
    let memory_allocation_cap = create_capability!(capabilities::MemoryAllocationCapability);

    let board_kernel = static_init!(kernel::Kernel, kernel::Kernel::new(&PROCESSES));

    let peripherals = static_init!(
        EarlGreyDefaultPeripherals<ChipConfig, BoardPinmuxLayout>,
        EarlGreyDefaultPeripherals::new()
    );
    peripherals.init();

    // Configure kernel debug gpios as early as possible
    kernel::debug::assign_gpios(
        Some(&peripherals.gpio_port[7]), // First LED
        None,
        None,
    );

    // Create a shared UART channel for the console and for kernel debug.
    let uart_mux =
        components::console::UartMuxComponent::new(&peripherals.uart0, ChipConfig::UART_BAUDRATE)
            .finalize(components::uart_mux_component_static!());

    // LEDs
    // Start with half on and half off
    let led = components::led::LedsComponent::new().finalize(components::led_component_static!(
        LedHigh<'static, earlgrey::gpio::GpioPin<earlgrey::pinmux::PadConfig>>,
        LedHigh::new(&peripherals.gpio_port[8]),
        LedHigh::new(&peripherals.gpio_port[9]),
        LedHigh::new(&peripherals.gpio_port[10]),
        LedHigh::new(&peripherals.gpio_port[11]),
        LedHigh::new(&peripherals.gpio_port[12]),
        LedHigh::new(&peripherals.gpio_port[13]),
        LedHigh::new(&peripherals.gpio_port[14]),
        LedHigh::new(&peripherals.gpio_port[15]),
    ));

    let gpio = components::gpio::GpioComponent::new(
        board_kernel,
        capsules_core::gpio::DRIVER_NUM,
        components::gpio_component_helper!(
            earlgrey::gpio::GpioPin<earlgrey::pinmux::PadConfig>,
            0 => &peripherals.gpio_port[0],
            1 => &peripherals.gpio_port[1],
            2 => &peripherals.gpio_port[2],
            3 => &peripherals.gpio_port[3],
            4 => &peripherals.gpio_port[4],
            5 => &peripherals.gpio_port[5],
            6 => &peripherals.gpio_port[6],
            7 => &peripherals.gpio_port[15]
        ),
    )
    .finalize(components::gpio_component_static!(
        earlgrey::gpio::GpioPin<earlgrey::pinmux::PadConfig>
    ));

    let hardware_alarm = static_init!(
        earlgrey::timer::RvTimer<ChipConfig>,
        earlgrey::timer::RvTimer::new()
    );
    hardware_alarm.setup();

    // Create a shared virtualization mux layer on top of a single hardware
    // alarm.
    let mux_alarm = static_init!(
        MuxAlarm<'static, earlgrey::timer::RvTimer<ChipConfig>>,
        MuxAlarm::new(hardware_alarm)
    );
    hil::time::Alarm::set_alarm_client(hardware_alarm, mux_alarm);

    ALARM = Some(mux_alarm);

    // Alarm
    let virtual_alarm_user = static_init!(
        VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<ChipConfig>>,
        VirtualMuxAlarm::new(mux_alarm)
    );
    virtual_alarm_user.setup();

    let scheduler_timer_virtual_alarm = static_init!(
        VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<ChipConfig>>,
        VirtualMuxAlarm::new(mux_alarm)
    );
    scheduler_timer_virtual_alarm.setup();

    let alarm = static_init!(
        capsules_core::alarm::AlarmDriver<
            'static,
            VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<ChipConfig>>,
        >,
        capsules_core::alarm::AlarmDriver::new(
            virtual_alarm_user,
            board_kernel.create_grant(capsules_core::alarm::DRIVER_NUM, &memory_allocation_cap)
        )
    );
    hil::time::Alarm::set_alarm_client(virtual_alarm_user, alarm);

    let scheduler_timer = static_init!(
        VirtualSchedulerTimer<
            VirtualMuxAlarm<'static, earlgrey::timer::RvTimer<'static, ChipConfig>>,
        >,
        VirtualSchedulerTimer::new(scheduler_timer_virtual_alarm)
    );

    let chip = static_init!(
        EarlGreyChip,
        earlgrey::chip::EarlGrey::new(peripherals, hardware_alarm, earlgrey_epmp)
    );
    CHIP = Some(chip);

    // Need to enable all interrupts for Tock Kernel
    chip.enable_plic_interrupts();
    // enable interrupts globally
    csr::CSR.mie.modify(
        csr::mie::mie::msoft::SET + csr::mie::mie::mtimer::CLEAR + csr::mie::mie::mext::SET,
    );
    csr::CSR.mstatus.modify(csr::mstatus::mstatus::mie::SET);

    // Setup the console.
    let console = components::console::ConsoleComponent::new(
        board_kernel,
        capsules_core::console::DRIVER_NUM,
        uart_mux,
    )
    .finalize(components::console_component_static!());
    // Create the debugger object that handles calls to `debug!()`.
    components::debug_writer::DebugWriterComponent::new(uart_mux)
        .finalize(components::debug_writer_component_static!());

    let lldb = components::lldb::LowLevelDebugComponent::new(
        board_kernel,
        capsules_core::low_level_debug::DRIVER_NUM,
        uart_mux,
    )
    .finalize(components::low_level_debug_component_static!());

    let hmac = components::hmac::HmacComponent::new(
        board_kernel,
        capsules_extra::hmac::DRIVER_NUM,
        &peripherals.hmac,
    )
    .finalize(components::hmac_component_static!(lowrisc::hmac::Hmac, 32));

    let i2c_master_buffer = static_init!(
        [u8; capsules_core::i2c_master::BUFFER_LENGTH],
        [0; capsules_core::i2c_master::BUFFER_LENGTH]
    );
    let i2c_master = static_init!(
        capsules_core::i2c_master::I2CMasterDriver<'static, lowrisc::i2c::I2c<'static>>,
        capsules_core::i2c_master::I2CMasterDriver::new(
            &peripherals.i2c0,
            i2c_master_buffer,
            board_kernel.create_grant(
                capsules_core::i2c_master::DRIVER_NUM,
                &memory_allocation_cap
            )
        )
    );

    peripherals.i2c0.set_master_client(i2c_master);

    //SPI
    let mux_spi = components::spi::SpiMuxComponent::new(&peripherals.spi_host0).finalize(
        components::spi_mux_component_static!(lowrisc::spi_host::SpiHost),
    );

    let spi_controller = components::spi::SpiSyscallComponent::new(
        board_kernel,
        mux_spi,
        0,
        capsules_core::spi_controller::DRIVER_NUM,
    )
    .finalize(components::spi_syscall_component_static!(
        lowrisc::spi_host::SpiHost
    ));

    let process_printer = components::process_printer::ProcessPrinterTextComponent::new()
        .finalize(components::process_printer_text_component_static!());
    PROCESS_PRINTER = Some(process_printer);

    // USB support is currently broken in the OpenTitan hardware
    // See https://github.com/lowRISC/opentitan/issues/2598 for more details
    // let usb = components::usb::UsbComponent::new(
    //     board_kernel,
    //     capsules_extra::usb::usb_user::DRIVER_NUM,
    //     &peripherals.usb,
    // )
    // .finalize(components::usb_component_static!(earlgrey::usbdev::Usb));

    // Kernel storage region, allocated with the storage_volume!
    // macro in common/utils.rs
    extern "C" {
        /// Beginning on the ROM region containing app images.
        static _sstorage: u8;
        static _estorage: u8;
    }

    // Flash
    let mux_flash = components::flash::FlashMuxComponent::new(&peripherals.flash_ctrl).finalize(
        components::flash_mux_component_static!(lowrisc::flash_ctrl::FlashCtrl),
    );

    // SipHash
    let sip_hash = static_init!(
        capsules_extra::sip_hash::SipHasher24,
        capsules_extra::sip_hash::SipHasher24::new()
    );
    kernel::deferred_call::DeferredCallClient::register(sip_hash);
    SIPHASH = Some(sip_hash);

    hil::flash::HasClient::set_client(&peripherals.flash_ctrl, mux_flash);

    let mux_otbn = crate::otbn::AccelMuxComponent::new(&peripherals.otbn)
        .finalize(otbn_mux_component_static!());

    let otbn = OtbnComponent::new(&mux_otbn).finalize(crate::otbn_component_static!());

    let otbn_rsa_internal_buf = static_init!([u8; 512], [0; 512]);

    // Use the OTBN to create an RSA engine
    if let Ok((rsa_imem_start, rsa_imem_length, rsa_dmem_start, rsa_dmem_length)) =
        crate::otbn::find_app(
            "otbn-rsa",
            core::slice::from_raw_parts(
                &_sapps as *const u8,
                &_eapps as *const u8 as usize - &_sapps as *const u8 as usize,
            ),
        )
    {
        let rsa_hardware = static_init!(
            lowrisc::rsa::OtbnRsa<'static>,
            lowrisc::rsa::OtbnRsa::new(
                otbn,
                lowrisc::rsa::AppAddresses {
                    imem_start: rsa_imem_start,
                    imem_size: rsa_imem_length,
                    dmem_start: rsa_dmem_start,
                    dmem_size: rsa_dmem_length
                },
                otbn_rsa_internal_buf,
            )
        );
        peripherals.otbn.set_client(rsa_hardware);
        RSA_HARDWARE = Some(rsa_hardware);
    } else {
        debug!("Unable to find otbn-rsa, disabling RSA support");
    }

    // Convert hardware RNG to the Random interface.
    let entropy_to_random = static_init!(
        Entropy32ToRandom<'static, CsRng<'static>>,
        Entropy32ToRandom::new(&peripherals.rng)
    );
    peripherals.rng.set_client(entropy_to_random);
    // Setup RNG for userspace
    let rng = static_init!(
        RngDriver<'static, Entropy32ToRandom<'static, CsRng<'static>>>,
        RngDriver::new(
            entropy_to_random,
            board_kernel.create_grant(capsules_core::rng::DRIVER_NUM, &memory_allocation_cap)
        )
    );
    entropy_to_random.set_client(rng);

    const CRYPT_SIZE: usize = 7 * AES128_BLOCK_SIZE;

    let ccm_mux = static_init!(
        virtual_aes_ccm::MuxAES128CCM<'static, earlgrey::aes::Aes<'static>>,
        virtual_aes_ccm::MuxAES128CCM::new(&peripherals.aes)
    );
    kernel::deferred_call::DeferredCallClient::register(ccm_mux);
    peripherals.aes.set_client(ccm_mux);

    let ccm_client = components::aes::AesVirtualComponent::new(ccm_mux).finalize(
        components::aes_virtual_component_static!(earlgrey::aes::Aes<'static>),
    );

    let crypt_buf2 = static_init!([u8; CRYPT_SIZE], [0x00; CRYPT_SIZE]);
    let gcm_client = static_init!(
        aes_gcm::Aes128Gcm<
            'static,
            virtual_aes_ccm::VirtualAES128CCM<'static, earlgrey::aes::Aes<'static>>,
        >,
        aes_gcm::Aes128Gcm::new(ccm_client, crypt_buf2)
    );
    ccm_client.set_client(gcm_client);

    let aes = components::aes::AesDriverComponent::new(
        board_kernel,
        capsules_extra::symmetric_encryption::aes::DRIVER_NUM,
        gcm_client,
    )
    .finalize(components::aes_driver_component_static!(
        aes_gcm::Aes128Gcm<
            'static,
            virtual_aes_ccm::VirtualAES128CCM<'static, earlgrey::aes::Aes<'static>>,
        >,
    ));

    AES = Some(gcm_client);

    #[cfg(test)]
    {
        use capsules_extra::sha256::Sha256Software;

        let sha_soft = static_init!(Sha256Software<'static>, Sha256Software::new());
        kernel::deferred_call::DeferredCallClient::register(sha_soft);

        SHA256SOFT = Some(sha_soft);
    }

    hil::symmetric_encryption::AES128GCM::set_client(gcm_client, aes);
    hil::symmetric_encryption::AES128::set_client(gcm_client, ccm_client);

    // These symbols are defined in the linker script.
    extern "C" {
        /// Beginning of the ROM region containing app images.
        static _sapps: u8;
        /// End of the ROM region containing app images.
        static _eapps: u8;
        /// Beginning of the RAM region for app memory.
        static mut _sappmem: u8;
        /// End of the RAM region for app memory.
        static _eappmem: u8;
        /// The start of the kernel text (Included only for kernel PMP)
        static _stext: u8;
        /// The end of the kernel text (Included only for kernel PMP)
        static _etext: u8;
        /// The start of flash storage (matches the start of the ROM_EXT).
        static _sflash: u8;
        /// The end of the kernel / app / storage flash (Included only for kernel PMP)
        static _eflash: u8;
        /// The start of the kernel / app RAM (Included only for kernel PMP)
        static _ssram: u8;
        /// The end of the kernel / app RAM (Included only for kernel PMP)
        static _esram: u8;
        /// The start of the OpenTitan manifest
        static _manifest: u8;
    }

    let syscall_filter = static_init!(TbfHeaderFilterDefaultAllow, TbfHeaderFilterDefaultAllow {});
    let scheduler = components::sched::priority::PriorityComponent::new(board_kernel)
        .finalize(components::priority_component_static!());
    let watchdog = &peripherals.watchdog;

    let earlgrey = static_init!(
        EarlGrey,
        EarlGrey {
            gpio,
            led,
            console,
            alarm,
            hmac,
            rng,
            lldb: lldb,
            i2c_master,
            spi_controller,
            aes,
            syscall_filter,
            scheduler,
            scheduler_timer,
            watchdog,
        }
    );

    kernel::process::load_processes(
        board_kernel,
        chip,
        core::slice::from_raw_parts(
            &_sapps as *const u8,
            &_eapps as *const u8 as usize - &_sapps as *const u8 as usize,
        ),
        core::slice::from_raw_parts_mut(
            &mut _sappmem as *mut u8,
            &_eappmem as *const u8 as usize - &_sappmem as *const u8 as usize,
        ),
        &mut PROCESSES,
        &FAULT_RESPONSE,
        &process_mgmt_cap,
    )
    .unwrap_or_else(|err| {
        debug!("Error loading processes!");
        debug!("{:?}", err);
    });
    debug!("OpenTitan (downstream) initialisation complete. Entering main loop");

    (board_kernel, earlgrey, chip, peripherals)
}

/// Main function.
///
/// This function is called from the arch crate after some very basic RISC-V
/// setup and RAM initialization.
#[no_mangle]
pub unsafe extern "C" fn main() {
    #[cfg(test)]
    test_main();

    #[cfg(not(test))]
    {
        let (board_kernel, earlgrey, chip, _peripherals) = setup();

        let main_loop_cap = create_capability!(capabilities::MainLoopCapability);

        board_kernel.kernel_loop(earlgrey, chip, None::<&kernel::ipc::IPC<0>>, &main_loop_cap);
    }
}

#[cfg(test)]
use kernel::platform::watchdog::WatchDog;

#[cfg(test)]
fn test_runner(tests: &[&dyn Fn()]) {
    unsafe {
        let (board_kernel, earlgrey, _chip, peripherals) = setup();

        BOARD = Some(board_kernel);
        PLATFORM = Some(&earlgrey);
        PERIPHERALS = Some(peripherals);
        MAIN_CAP = Some(&create_capability!(capabilities::MainLoopCapability));

        PLATFORM.map(|p| {
            p.watchdog().setup();
        });

        for test in tests {
            test();
        }
    }

    // Exit QEMU with a return code of 0
    crate::tests::semihost_command_exit_success()
}
