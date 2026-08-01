# SPDX-License-Identifier: GPL-2.0-only

focaltech-ft820x-y := focaltech_ft820x_core.o focaltech_ft820x_spi.o
focaltech-ft820x-$(CONFIG_TOUCHSCREEN_FOCALTECH_FT820X_FW_DOWNLOAD) += \
	focaltech_ft820x_firmware.o

obj-$(CONFIG_TOUCHSCREEN_FOCALTECH_FT820X) += focaltech-ft820x.o
