// SPDX-License-Identifier: GPL-2.0-only
/*
 * FocalTech FT820x low-level RAM firmware download support.
 *
 * This source retains the FT8203 ROM boot, PRAM write, ECC verification,
 * and application start algorithm from the downstream driver. The normal
 * touchscreen runtime deliberately has no call sites for this helper. A
 * future explicit update interface must stop IRQ handling and serialize all
 * bus access before calling ft820x_ram_firmware_download().
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/types.h>

#include "focaltech_ft820x.h"

#define FT820X_FW_MIN_SIZE		0x120
#define FT820X_FW_MAX_SIZE		(96 * 1024)
#define FT820X_FW_APP_INFO_OFFSET	0x100

#define FT820X_ROM_BOOT_ID_H		0x82
#define FT820X_ROM_BOOT_ID_L		0x01
#define FT820X_ROM_BOOT_RETRIES		30
#define FT820X_ROM_ID_RETRIES		3
#define FT820X_ROM_BOOT_DELAY_MS	8

#define FT820X_CMD_START_BOOT		0x55
#define FT820X_CMD_READ_BOOT_ID		0x90
#define FT820X_CMD_SET_PRAM_ADDRESS	0xad
#define FT820X_CMD_WRITE_PRAM		0xae
#define FT820X_CMD_START_APP		0x08
#define FT820X_CMD_ECC_CALCULATE	0xcc
#define FT820X_CMD_ECC_FINISHED		0xce
#define FT820X_CMD_ECC_READ		0xcd

#define FT820X_ECC_FINISHED_VALUE	0xa5
#define FT820X_ECC_FINISH_RETRIES	100
#define FT820X_ECC_POLYNOMIAL		0x8408

#define FT820X_PRAM_PACKET_SIZE		(32 * 1024 - 16)

static int ft820x_enter_rom_boot(struct ft820x_data *data)
{
	u8 boot_id[2];
	int attempt;
	int id_attempt;
	int error;

	for (attempt = 0; attempt < FT820X_ROM_BOOT_RETRIES; attempt++) {
		ft820x_reset(data, 0);
		msleep(FT820X_ROM_BOOT_DELAY_MS);

		for (id_attempt = 0; id_attempt < FT820X_ROM_ID_RETRIES;
		     id_attempt++) {
			error = ft820x_spi_write(data, FT820X_CMD_START_BOOT, NULL, 0);
			if (error)
				continue;

			msleep(FT820X_ROM_BOOT_DELAY_MS);
			error = ft820x_spi_read(data, FT820X_CMD_READ_BOOT_ID,
						boot_id, sizeof(boot_id));
			if (!error && boot_id[0] == FT820X_ROM_BOOT_ID_H &&
			    boot_id[1] == FT820X_ROM_BOOT_ID_L)
				return 0;
		}
	}

	return -EIO;
}

static int ft820x_write_pram(struct ft820x_data *data, const u8 *firmware,
			     size_t size)
{
	size_t offset = 0;
	u8 address[3];
	int error = 0;

	while (offset < size) {
		size_t length = min_t(size_t, FT820X_PRAM_PACKET_SIZE,
				      size - offset);

		address[0] = offset >> 16;
		address[1] = offset >> 8;
		address[2] = offset;
		error = ft820x_spi_write(data, FT820X_CMD_SET_PRAM_ADDRESS,
					 address, sizeof(address));
		if (error)
			break;

		error = ft820x_spi_write(data, FT820X_CMD_WRITE_PRAM,
					 firmware + offset, length);
		if (error)
			break;

		offset += length;
	}

	return error;
}

static u16 ft820x_calculate_ecc(const u8 *firmware, size_t size)
{
	u16 ecc = 0;
	size_t index;
	int bit;

	for (index = 0; index < size; index += 2) {
		ecc ^= firmware[index] << 8 | firmware[index + 1];
		for (bit = 0; bit < 16; bit++) {
			if (ecc & 1)
				ecc = ecc >> 1 ^ FT820X_ECC_POLYNOMIAL;
			else
				ecc >>= 1;
		}
	}

	return ecc;
}

static int ft820x_verify_ecc(struct ft820x_data *data, const u8 *firmware,
			     size_t size)
{
	u8 command[6] = { 0, 0, 0, size >> 16, size >> 8, size };
	u8 controller_ecc[2];
	u8 status;
	u16 host_ecc;
	int attempt;
	int error;

	error = ft820x_spi_write(data, FT820X_CMD_ECC_CALCULATE, command,
				 sizeof(command));
	if (error)
		return error;

	usleep_range(2000, 3000);
	for (attempt = 0; attempt < FT820X_ECC_FINISH_RETRIES; attempt++) {
		error = ft820x_spi_read_u8(data, FT820X_CMD_ECC_FINISHED,
					   &status);
		if (error)
			return error;
		if (status == FT820X_ECC_FINISHED_VALUE)
			break;
		usleep_range(1000, 2000);
	}
	if (attempt == FT820X_ECC_FINISH_RETRIES)
		return -ETIMEDOUT;

	error = ft820x_spi_read(data, FT820X_CMD_ECC_READ, controller_ecc,
				sizeof(controller_ecc));
	if (error)
		return error;

	host_ecc = ft820x_calculate_ecc(firmware, size);
	if (host_ecc != (controller_ecc[0] << 8 | controller_ecc[1]))
		return -EBADMSG;

	return 0;
}

static int ft820x_download_once(struct ft820x_data *data, const u8 *firmware,
				size_t size)
{
	int error;

	error = ft820x_enter_rom_boot(data);
	if (error)
		return error;

	error = ft820x_write_pram(data, firmware, size);
	if (error)
		return error;

	error = ft820x_verify_ecc(data, firmware, size);
	if (error)
		return error;

	error = ft820x_spi_write(data, FT820X_CMD_START_APP, NULL, 0);
	if (error)
		return error;

	usleep_range(10000, 11000);
	return 0;
}

int ft820x_ram_firmware_download(struct ft820x_data *data, const u8 *firmware,
				 size_t size)
{
	u16 code_size;
	u16 code_size_complement;
	u32 app_size;
	int attempt;
	int error = -EIO;

	if (!firmware || size < FT820X_FW_MIN_SIZE ||
	    size > FT820X_FW_MAX_SIZE)
		return -EINVAL;

	code_size = firmware[FT820X_FW_APP_INFO_OFFSET] << 8 |
		    firmware[FT820X_FW_APP_INFO_OFFSET + 1];
	code_size_complement = firmware[FT820X_FW_APP_INFO_OFFSET + 2] << 8 |
			       firmware[FT820X_FW_APP_INFO_OFFSET + 3];
	app_size = (u32)code_size * 2;
	if ((u32)code_size + code_size_complement != U16_MAX ||
	    app_size < FT820X_FW_MIN_SIZE || app_size > size || app_size & 1)
		return -EINVAL;

	for (attempt = 0; attempt < 3; attempt++) {
		error = ft820x_download_once(data, firmware, app_size);
		if (!error)
			return 0;
	}

	return error;
}
