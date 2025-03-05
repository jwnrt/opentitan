#include <stdbool.h>

#include "sw/device/examples/bridge/screen.h"
#include "sw/device/lib/crypto/drivers/entropy.h"
#include "sw/device/lib/dif/dif_spi_device.h"
#include "sw/device/lib/runtime/ibex.h"
#include "sw/device/lib/testing/spi_device_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/silicon_creator/lib/dbg_print.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"

/**
 * # OpenTitan SPI MMIO bridge
 *
 * This program gives external devices access to OpenTitan's entire address
 * space through the SPI device. Never *ever* sign this program with a
 * production silicon owner key, it breaks tonnes of OpenTitan security rules.
 *
 * ## The protocol
 *
 * We're mostly using SPI flash commands except reads are split into two. We're
 * using CPOL=0, CPHA=0, MSB-first, and 4-byte addresses. All commands must
 * start and end by pulling chip select low and high respectively. The
 * connection has been tested up to 10MHz.
 *
 * Writing to memory:
 *
 * - Pull chip-select low.
 * - Send the PageProgram command opcode (0x02).
 * - Send the four-byte address to program (big-endian).
 * - Send up to 256 bytes of data to program in series.
 * - Pull chip-select high.
 *
 * Reading from memory:
 *
 * - Pull chip-select low.
 * - Send the Read4b command opcode (0x13).
 * - Send the four-byte address to read from (big-endian).
 * - (optional) Send the number of bytes to read (up to 255). Defaults to 1.
 * - Pull chip-select high.
 * - Wait for the device to be ready (see below).
 * - Pull chip-select low.
 * - Send the ReadNormal command opcode (0x03).
 * - Send a four-byte address from the buffer that was read (normally 0).
 * - Read out the correct number of bytes.
 * - Pull chip-select high.
 *
 * You must check that the device is ready before sending a read or write
 * command:
 *
 * - Pull chip-select low.
 * - Send the ReadStatus1 command opcode (0x05).
 * - Read one byte. If the lowest bit is 0, the device is ready.
 * - Pull chip-select high.
 * - Repeat until ready.
 *
 * You may also test the connection by asking for the JEDEC ID:
 *
 * - Pull chip-select low.
 * - Send the ReadJedec command opcode (0x9f).
 * - Read three bytes..
 * - Pull chip-select high.
 *
 * If the bridge is working, the ID should come back as three bytes (0x4A, 0x5A,
 * 0x4F) which correspond to the manufacturer ID ('J') and device ID ('O', 'T').
 */

#include "sw/device/lib/crypto/drivers/entropy.h"

static void configure_spi_device(dif_spi_device_handle_t *spi_device);
static void event_loop(dif_spi_device_handle_t *spi_device,
                       screen_context_t *screen_ctx);

void bare_metal_main(void) {
  dbg_printf("Entropy complex configured\r\n");
  CHECK_STATUS_OK(entropy_complex_init());

  dif_spi_device_handle_t spi_device;
  configure_spi_device(&spi_device);
  dbg_printf("Bridge configured\r\n");

  screen_context_t screen_ctx;
  screen_init(&screen_ctx);
  screen_draw_logo(&screen_ctx);
  dbg_printf("Screen initialised\r\n");

  dbg_printf("Starting event loop...\r\n");
  event_loop(&spi_device, &screen_ctx);
}

void interrupt_handler(void) { dbg_printf("Interrupt!\r\n"); }

static void configure_spi_device(dif_spi_device_handle_t *spi_device) {
  mmio_region_t base_addr =
      mmio_region_from_addr(TOP_EARLGREY_SPI_DEVICE_BASE_ADDR);
  CHECK_DIF_OK(dif_spi_device_init_handle(base_addr, spi_device));

  dif_spi_device_config_t spi_config = {
      .clock_polarity = kDifSpiDeviceEdgePositive,
      .data_phase = kDifSpiDeviceEdgeNegative,
      .tx_order = kDifSpiDeviceBitOrderMsbToLsb,
      .rx_order = kDifSpiDeviceBitOrderMsbToLsb,
      .device_mode = kDifSpiDeviceModeFlashEmulation,
  };
  CHECK_DIF_OK(dif_spi_device_configure(spi_device, spi_config));

  // Clear the payload FIFO - the hardware doesn't do this for us and the
  // parity is checked on the first SW access, so there's a 50/50 chance
  // of getting an integrity error.
  uint8_t zeroes[256];
  memset(zeroes, 0, sizeof(zeroes));
  CHECK_DIF_OK(dif_spi_device_write_flash_buffer(
      spi_device, kDifSpiDeviceFlashBufferTypePayload, 0, sizeof(zeroes),
      zeroes));

  dif_spi_device_flash_command_t read_status1_cmd = {
      .opcode = kSpiDeviceFlashOpReadStatus1,
      .address_type = kDifSpiDeviceFlashAddrDisabled,
      .dummy_cycles = 0,
      .payload_io_type = kDifSpiDevicePayloadIoSingle,
      .payload_dir_to_host = true,
  };

  dif_spi_device_flash_command_t read_jedec_cmd = {
      .opcode = kSpiDeviceFlashOpReadJedec,
      .address_type = kDifSpiDeviceFlashAddrDisabled,
      .dummy_cycles = 0,
      .payload_io_type = kDifSpiDevicePayloadIoSingle,
      .payload_dir_to_host = true,
  };

  dif_spi_device_flash_command_t read_4b_cmd = {
      .opcode = kSpiDeviceFlashOpRead4b,
      .address_type = kDifSpiDeviceFlashAddr4Byte,
      .dummy_cycles = 0,
      .payload_io_type = kDifSpiDevicePayloadIoSingle,
      .upload = true,
      .set_busy_status = true,
  };

  dif_spi_device_flash_command_t read_normal_cmd = {
      .opcode = kSpiDeviceFlashOpReadNormal,
      .address_type = kDifSpiDeviceFlashAddr4Byte,
      .dummy_cycles = 0,
      .payload_io_type = kDifSpiDevicePayloadIoSingle,
      .payload_dir_to_host = true,
  };

  dif_spi_device_flash_command_t write_cmd = {
      .opcode = kSpiDeviceFlashOpPageProgram,
      .address_type = kDifSpiDeviceFlashAddr4Byte,
      .dummy_cycles = 0,
      .payload_io_type = kDifSpiDevicePayloadIoSingle,
      .payload_dir_to_host = false,
      .upload = true,
      .set_busy_status = true,
  };

  CHECK_DIF_OK(dif_spi_device_set_flash_command_slot(
      spi_device, kSpiDeviceReadCommandSlotBase, kDifToggleEnabled,
      read_status1_cmd));
  CHECK_DIF_OK(dif_spi_device_set_flash_command_slot(
      spi_device, kSpiDeviceReadCommandSlotBase + 3, kDifToggleEnabled,
      read_jedec_cmd));
  CHECK_DIF_OK(dif_spi_device_set_flash_command_slot(
      spi_device, kSpiDeviceReadCommandSlotBase + 5, kDifToggleEnabled,
      read_normal_cmd));
  CHECK_DIF_OK(dif_spi_device_set_flash_command_slot(
      spi_device, kSpiDeviceWriteCommandSlotBase, kDifToggleEnabled,
      write_cmd));
  CHECK_DIF_OK(dif_spi_device_set_flash_command_slot(
      spi_device, kSpiDeviceWriteCommandSlotBase + 1, kDifToggleEnabled,
      read_4b_cmd));

  // Enable 4-byte addresses so host doesn't have to send the `EN4B` command.
  CHECK_DIF_OK(
      dif_spi_device_set_4b_address_mode(spi_device, kDifToggleEnabled));

  dif_spi_device_flash_id_t flash_id = {
      .device_id = 0x544F,      // OT_le
      .manufacturer_id = 0x4A,  // J :)
  };
  CHECK_DIF_OK(dif_spi_device_set_flash_id(spi_device, flash_id));
}

static void event_loop(dif_spi_device_handle_t *spi_device,
                       screen_context_t *screen_ctx) {
  enum { kTimeout = 800 * 1000 };
  ibex_timeout_t deadline = ibex_timeout_init(kTimeout);
  CHECK_DIF_OK(dif_spi_device_clear_flash_busy_bit(spi_device));
  while (true) {
    if (ibex_timeout_check(&deadline)) {
      deadline = ibex_timeout_init(kTimeout);
      screen_keep_alive(screen_ctx);
    }

    bool upload_pending;
    CHECK_DIF_OK(dif_spi_device_irq_is_pending(
        &spi_device->dev, kDifSpiDeviceIrqUploadCmdfifoNotEmpty,
        &upload_pending));

    if (!upload_pending) {
      continue;
    }

    upload_info_t upload_info;
    CHECK_STATUS_OK(
        spi_device_testutils_wait_for_upload(spi_device, &upload_info));

    if (!upload_info.has_address) {
      dbg_printf("cmd had no address\r\n");
      break;
    }
    deadline = ibex_timeout_init(kTimeout);

    mmio_region_t base_addr = mmio_region_from_addr(upload_info.address);

    switch (upload_info.opcode) {
      case kSpiDeviceFlashOpRead4b:
        if (upload_info.data_len == 0) {
          uint8_t data8 = mmio_region_read8(base_addr, 0);
          CHECK_DIF_OK(dif_spi_device_write_flash_buffer(
              spi_device, kDifSpiDeviceFlashBufferTypeEFlash, 0, 1, &data8));
          break;
        }

        for (uint16_t i = 0; i < upload_info.data[0]; i += 4) {
          uint32_t data32 = mmio_region_read32(base_addr, i);
          CHECK_DIF_OK(dif_spi_device_write_flash_buffer(
              spi_device, kDifSpiDeviceFlashBufferTypeEFlash, i, sizeof(data32),
              (uint8_t *)&data32));
        }

        break;
      case kSpiDeviceFlashOpPageProgram:
        if (upload_info.data_len == 0) {
          dbg_printf("program cmd to addr %p had no data\r\n", base_addr.base);
          break;
        }

        if (upload_info.data_len == 1) {
          mmio_region_write8(base_addr, 0, upload_info.data[0]);
        } else {
          for (uint16_t i = 0; i < upload_info.data_len; i += 4) {
            uint32_t word = (uint32_t)upload_info.data[i];
            if (i + 1 < upload_info.data_len)
              word += (uint32_t)(upload_info.data[i + 1]) << 8;
            if (i + 2 < upload_info.data_len)
              word += (uint32_t)(upload_info.data[i + 2]) << 16;
            if (i + 3 < upload_info.data_len)
              word += (uint32_t)(upload_info.data[i + 3]) << 24;
            mmio_region_write32(base_addr, i, word);
          }
        }

        break;
      default:
        dbg_printf("unknown opcode: %x\r\n", upload_info.opcode);
    }
    CHECK_DIF_OK(dif_spi_device_clear_flash_busy_bit(spi_device));
  }
  dbg_printf("Finishing loop\r\n");
}
