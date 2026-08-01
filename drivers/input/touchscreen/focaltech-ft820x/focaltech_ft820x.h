/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _FOCALTECH_FT820X_H
#define _FOCALTECH_FT820X_H

#include <linux/input/touchscreen.h>
#include <linux/mutex.h>
#include <linux/types.h>

struct gpio_desc;
struct input_dev;
struct spi_device;

#define FT820X_MAX_TOUCHES		10
#define FT820X_TOUCH_DATA_SIZE		63
#define FT820X_SPI_BUFFER_SIZE		128

struct ft820x_data {
	struct spi_device *spi;
	struct input_dev *input;
	struct gpio_desc *reset_gpio;
	const char *firmware_name;
	struct touchscreen_properties prop;
	/* Serializes SPI transfers and access to the reusable bus buffers. */
	struct mutex bus_lock;
	u8 tx_buf[FT820X_SPI_BUFFER_SIZE];
	u8 rx_buf[FT820X_SPI_BUFFER_SIZE];
	u8 touch_data[FT820X_TOUCH_DATA_SIZE];
	bool powered;
	bool running;
};

int ft820x_spi_read(struct ft820x_data *data, u8 reg, void *buf, size_t len);
int ft820x_spi_write(struct ft820x_data *data, u8 reg,
		     const void *buf, size_t len);
void ft820x_reset(struct ft820x_data *data, unsigned int delay_ms);

#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_FT820X_FW_DOWNLOAD
int ft820x_ram_firmware_download(struct ft820x_data *data, const u8 *firmware,
				 size_t size);
#endif

static inline int ft820x_spi_read_u8(struct ft820x_data *data, u8 reg,
				     u8 *value)
{
	return ft820x_spi_read(data, reg, value, sizeof(*value));
}

static inline int ft820x_spi_write_u8(struct ft820x_data *data, u8 reg,
				      u8 value)
{
	return ft820x_spi_write(data, reg, &value, sizeof(value));
}

#endif
