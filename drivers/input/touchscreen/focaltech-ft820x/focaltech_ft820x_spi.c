// SPDX-License-Identifier: GPL-2.0-only

#include <linux/crc-ccitt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/string.h>

#include "focaltech_ft820x.h"

#define FT820X_SPI_RETRIES		3
#define FT820X_SPI_STATUS_ERROR		0xa0
#define FT820X_SPI_READ_CMD		0xa0
#define FT820X_SPI_WRITE_CMD		0x00
#define FT820X_SPI_HEADER_SIZE		4
#define FT820X_SPI_DUMMY_SIZE		3
#define FT820X_SPI_CRC_SIZE		2

static int ft820x_spi_transfer(struct ft820x_data *data, const void *tx_buf,
			       void *rx_buf, size_t len)
{
	struct spi_transfer transfer = {
		.tx_buf = tx_buf,
		.rx_buf = rx_buf,
		.len = len,
	};

	return spi_sync_transfer(data->spi, &transfer, 1);
}

int ft820x_spi_read(struct ft820x_data *data, u8 reg, void *buf, size_t len)
{
	size_t data_offset = FT820X_SPI_HEADER_SIZE + FT820X_SPI_DUMMY_SIZE;
	size_t transfer_len = data_offset + len + FT820X_SPI_CRC_SIZE;
	u8 *tx_buf = data->tx_buf;
	u8 *rx_buf = data->rx_buf;
	u16 expected_crc;
	u16 received_crc;
	int attempt;
	int error = -EIO;

	if (!buf || !len || len > U16_MAX)
		return -EINVAL;

	if (transfer_len > sizeof(data->tx_buf)) {
		tx_buf = kzalloc(transfer_len, GFP_KERNEL);
		if (!tx_buf)
			return -ENOMEM;

		rx_buf = kzalloc(transfer_len, GFP_KERNEL);
		if (!rx_buf) {
			kfree(tx_buf);
			return -ENOMEM;
		}
	}

	mutex_lock(&data->bus_lock);

	memset(tx_buf, 0, transfer_len);
	tx_buf[0] = reg;
	tx_buf[1] = FT820X_SPI_READ_CMD;
	tx_buf[2] = len >> 8;
	tx_buf[3] = len;

	for (attempt = 0; attempt < FT820X_SPI_RETRIES; attempt++) {
		memset(rx_buf, 0, transfer_len);
		error = ft820x_spi_transfer(data, tx_buf, rx_buf, transfer_len);
		if (error)
			goto retry;

		if (rx_buf[3] & FT820X_SPI_STATUS_ERROR) {
			error = -EIO;
			goto retry;
		}

		expected_crc = crc_ccitt(0xffff, rx_buf + data_offset, len);
		received_crc = rx_buf[data_offset + len] |
			       rx_buf[data_offset + len + 1] << 8;
		if (expected_crc != received_crc) {
			error = -EBADMSG;
			goto retry;
		}

		memcpy(buf, rx_buf + data_offset, len);
		error = 0;
		break;

retry:
		usleep_range(150, 200);
	}

	usleep_range(150, 200);
	mutex_unlock(&data->bus_lock);

	if (tx_buf != data->tx_buf) {
		kfree(rx_buf);
		kfree(tx_buf);
	}

	return error;
}

int ft820x_spi_write(struct ft820x_data *data, u8 reg,
		     const void *buf, size_t len)
{
	size_t data_offset = FT820X_SPI_HEADER_SIZE;
	size_t transfer_len = data_offset + len;
	u8 *tx_buf = data->tx_buf;
	u8 *rx_buf = data->rx_buf;
	int attempt;
	int error = -EIO;

	if ((!buf && len) || len > U16_MAX)
		return -EINVAL;

	if (len) {
		data_offset += FT820X_SPI_DUMMY_SIZE;
		transfer_len += FT820X_SPI_DUMMY_SIZE;
	}

	if (transfer_len > sizeof(data->tx_buf)) {
		tx_buf = kzalloc(transfer_len, GFP_KERNEL);
		if (!tx_buf)
			return -ENOMEM;

		rx_buf = kzalloc(transfer_len, GFP_KERNEL);
		if (!rx_buf) {
			kfree(tx_buf);
			return -ENOMEM;
		}
	}

	mutex_lock(&data->bus_lock);

	memset(tx_buf, 0, transfer_len);
	tx_buf[0] = reg;
	tx_buf[1] = FT820X_SPI_WRITE_CMD;
	tx_buf[2] = len >> 8;
	tx_buf[3] = len;
	if (len)
		memcpy(tx_buf + data_offset, buf, len);

	for (attempt = 0; attempt < FT820X_SPI_RETRIES; attempt++) {
		memset(rx_buf, 0, transfer_len);
		error = ft820x_spi_transfer(data, tx_buf, rx_buf, transfer_len);
		if (!error && !(rx_buf[3] & FT820X_SPI_STATUS_ERROR))
			break;

		if (!error)
			error = -EIO;
		usleep_range(150, 200);
	}

	usleep_range(150, 200);
	mutex_unlock(&data->bus_lock);

	if (tx_buf != data->tx_buf) {
		kfree(rx_buf);
		kfree(tx_buf);
	}

	return error;
}
