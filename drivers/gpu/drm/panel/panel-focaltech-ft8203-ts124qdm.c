// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 XiaoYeZi

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

#include <video/mipi_display.h>

struct ft8203_ts124qdm_wqxga {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct regulator_bulk_data *supplies;
	struct regulator *power;
};

static const struct regulator_bulk_data ft8203_ts124qdm_wqxga_supplies[] = {
	{ .supply = "vddi" },
	{ .supply = "avdd" },
	{ .supply = "avee" },
};

#define ft8203_dcs_write_seq_long_multi(ctx, seq...)                    \
	do {                                                              \
		static const u8 d[] = { seq };                              \
		mipi_dsi_dcs_write_buffer_long_multi(ctx, d, ARRAY_SIZE(d)); \
	} while (0)

#define ft8203_dcs_write_var_seq_long_multi(ctx, seq...)                \
	do {                                                              \
		const u8 d[] = { seq };                                     \
		mipi_dsi_dcs_write_buffer_long_multi(ctx, d, ARRAY_SIZE(d)); \
	} while (0)

static inline
struct ft8203_ts124qdm_wqxga *to_ft8203_ts124qdm_wqxga(struct drm_panel *panel)
{
	return container_of_const(panel, struct ft8203_ts124qdm_wqxga, panel);
}

static int ft8203_ts124qdm_wqxga_power_cycle(struct ft8203_ts124qdm_wqxga *ctx)
{
	int ret;

	ret = regulator_enable(ctx->power);
	if (ret < 0)
		return ret;

	usleep_range(5000, 6000);
	ret = regulator_disable(ctx->power);
	if (ret < 0)
		return ret;

	usleep_range(4000, 5000);
	ret = regulator_enable(ctx->power);
	if (ret < 0)
		return ret;

	msleep(12);

	return 0;
}

static int ft8203_ts124qdm_wqxga_on(struct ft8203_ts124qdm_wqxga *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xff, 0x82, 0x01, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xff, 0x82, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x93);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x16);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x97);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x16);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9e);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x25);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x25);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb6);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x07, 0x07);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x1b, 0x1b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xd8, 0xaa, 0xaa);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x82);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x95);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x83);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x07);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xe1,
				     0x05, 0x0e, 0x23, 0x37, 0x42, 0x4f, 0x62,
				     0x70, 0x73, 0x81, 0x83, 0x98, 0x6c, 0x59,
				     0x5a, 0x50);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xe1,
				     0x49, 0x3f, 0x32, 0x29, 0x22, 0x14, 0x06,
				     0x02);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xe2,
				     0x05, 0x0e, 0x23, 0x37, 0x42, 0x4f, 0x62,
				     0x70, 0x73, 0x81, 0x83, 0x98, 0x6c, 0x59,
				     0x5a, 0x50);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xe2,
				     0x49, 0x3f, 0x32, 0x29, 0x22, 0x14, 0x06,
				     0x02);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xa4, 0x8c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf3, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa1);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb3, 0x06, 0x40);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa3);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb3, 0x0a, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb3, 0x00, 0x13);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc1, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x3f, 0x33, 0x30, 0x3f, 0x30, 0x33, 0x30);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x87);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb, 0x3f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x88);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x00, 0x3f, 0x33, 0x33, 0x33, 0x30, 0x3f,
				     0x3f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x00, 0x33, 0x33, 0x33, 0x30, 0x30, 0x3f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x97);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb, 0x33);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x98);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0xd7, 0x14, 0x14, 0xd4, 0x14, 0x14, 0x14,
				     0xd7);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x00, 0xfc, 0x14, 0x14, 0x14, 0x14, 0xeb,
				     0xd4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x28, 0x14, 0x14, 0x14, 0x14, 0x14, 0xc0,
				     0x14);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb7);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb, 0xff);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
				     0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc7);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb,
				     0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
				     0x03, 0x03, 0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13,
				     0x07);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x88);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x08, 0x2c, 0x17, 0x2b, 0x2b, 0x01, 0x23,
				     0x23);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x11, 0x12, 0x0f, 0x10, 0x2c, 0x2c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13,
				     0x07);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x88);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x08, 0x2c, 0x17, 0x2b, 0x2b, 0x01, 0x23,
				     0x23);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x11, 0x12, 0x0f, 0x10, 0x2c, 0x2c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13,
				     0x08);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x07, 0x17, 0x2c, 0x2b, 0x2b, 0x01, 0x23,
				     0x23);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcc,
				     0x10, 0x0f, 0x12, 0x11, 0x2c, 0x2c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x38, 0x2d, 0x2d, 0x2d, 0x13, 0x13, 0x13,
				     0x08);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x07, 0x17, 0x2c, 0x2b, 0x2b, 0x01, 0x23,
				     0x23);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcd,
				     0x10, 0x0f, 0x12, 0x11, 0x2c, 0x2c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x81);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2, 0x40);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2, 0x84, 0x02, 0x58, 0x93);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x94);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2, 0x83, 0x02, 0x58, 0x93);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2,
				     0x8b, 0x09, 0x01, 0x61, 0x93, 0x00, 0x00,
				     0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2,
				     0x8a, 0x0a, 0x01, 0x61, 0x93, 0x00, 0x00,
				     0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xf0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2,
				     0x89, 0x07, 0x01, 0x61, 0x93, 0x00, 0x00,
				     0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xf8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc2,
				     0x88, 0x08, 0x01, 0x61, 0x93, 0x00, 0x00,
				     0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc3, 0x36, 0x24, 0x00, 0xc2);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc3, 0x35, 0x24, 0x00, 0x76);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc3, 0x35, 0x24, 0x00, 0x76);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc3, 0x01, 0xaa, 0x0a, 0x0f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xfd);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcb, 0x82);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0,
				     0x00, 0x79, 0x00, 0x10, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0,
				     0x00, 0x79, 0x00, 0x10, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0,
				     0x00, 0xf0, 0x00, 0x10, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0,
				     0x00, 0x79, 0x00, 0x10, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa3);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc1, 0x2d, 0x21, 0x04);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce,
				     0x01, 0x81, 0xff, 0xff, 0x00, 0x88, 0x00,
				     0xd0, 0x00, 0x54, 0x00, 0x68);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce,
				     0x00, 0x87, 0x0e, 0x00, 0x00, 0x87, 0x80,
				     0xff, 0xff, 0x00, 0x06, 0x00, 0x17, 0x0f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce, 0x20, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd1);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce,
				     0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xe1);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce,
				     0x09, 0x02, 0x30, 0x02, 0x30, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xf0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce,
				     0x80, 0x17, 0x0b, 0x01, 0x10, 0x01, 0xa0,
				     0x00, 0x20, 0x25);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0x00, 0x00, 0x46, 0x4a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0x05, 0x05, 0x00, 0x04);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0x09, 0x09, 0xec, 0xf0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0x00, 0x0a, 0x08, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc4, 0x88);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x92);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc4, 0xc0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc3, 0x00, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd6);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc1, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc1, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xbf);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0, 0x04);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0, 0xf1);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x91);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x4c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd7);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x94);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x46);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x98);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x64);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x65);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9d);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x65);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0xff);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x82);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xa5, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x8c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xcf, 0x40, 0x40);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa2);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf5, 0x1f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xc1);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0, 0x11);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf5, 0x3f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x9c);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf5, 0x1e);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb6);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc0, 0x02);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xca, 0x09, 0x09, 0x04);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xca, 0x03);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xca,
				     0xf0, 0xd9, 0xc8, 0xba, 0xaf, 0xa6, 0x9e,
				     0x98, 0x92, 0x8d, 0x88, 0x84);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xca, 0xfb, 0xff, 0x33);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xca, 0x06);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x00, 0x04, 0x08, 0x0c, 0x00, 0x10, 0x14,
				     0x18, 0x1c, 0x40, 0x20, 0x28, 0x30, 0x38,
				     0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x10);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x40, 0x48, 0x4f, 0x57, 0xf0, 0x5f, 0x67,
				     0x6f, 0x77, 0xff, 0x7f, 0x87, 0x8f, 0x97,
				     0xff);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x20);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x9f, 0xa7, 0xaf, 0xb7, 0xff, 0xbf, 0xc7,
				     0xcf, 0xd7, 0xaf, 0xdf, 0xe7, 0xee, 0xf6,
				     0xfa);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x30);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec, 0xfa, 0xfc, 0xfd, 0x3f);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x40);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x00, 0x04, 0x08, 0x0c, 0x00, 0x10, 0x14,
				     0x18, 0x1c, 0x00, 0x20, 0x28, 0x30, 0x38,
				     0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x50);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x40, 0x48, 0x50, 0x58, 0x00, 0x60, 0x68,
				     0x70, 0x78, 0x00, 0x80, 0x88, 0x90, 0x98,
				     0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x60);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0xa0, 0xa8, 0xb0, 0xb8, 0x00, 0xc0, 0xc8,
				     0xd0, 0xd8, 0x00, 0xe0, 0xe8, 0xf0, 0xf8,
				     0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x70);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec, 0xfc, 0xfe, 0xff, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x00, 0x03, 0x07, 0x0b, 0x6c, 0x0f, 0x13,
				     0x16, 0x1a, 0xb1, 0x1e, 0x26, 0x2e, 0x35,
				     0xc5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x90);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x3d, 0x45, 0x4d, 0x54, 0x86, 0x5c, 0x64,
				     0x6b, 0x73, 0x61, 0x7b, 0x82, 0x8a, 0x91,
				     0xd8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec,
				     0x99, 0xa1, 0xa8, 0xb0, 0x72, 0xb8, 0xbf,
				     0xc7, 0xcf, 0x6c, 0xd7, 0xde, 0xe7, 0xee,
				     0xcc);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xec, 0xf2, 0xf4, 0xf5, 0x2b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb4,
				     0x00, 0x28, 0x02, 0x00, 0x04, 0x9f, 0x00,
				     0x0b, 0x02, 0x77, 0x01, 0xb1, 0x10, 0xd0);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xbe);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb4, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xd5);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc1, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xb0, 0x92);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x1c, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf3, 0x0b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xfa, 0x02);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x09);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xcb);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb6);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x07, 0x07);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x1b, 0x1b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x91);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xa5, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb2);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce, 0x79);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf3, 0x0b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xfa, 0x5a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf3, 0x0b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xfa, 0x01);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x09);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xcb);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x09);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb6);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x05, 0x05);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb8);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xc5, 0x19, 0x19);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x91);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xa5, 0x40);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xb2);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xce, 0x7a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0xa4);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xf3, 0x0b);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xfa, 0x5a);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xff, 0x00, 0x00, 0x00);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0x00, 0x80);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, 0xff, 0x00, 0x00);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 128);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	ft8203_dcs_write_seq_long_multi(&dsi_ctx, MIPI_DCS_SET_TEAR_ON, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 32);

	return dsi_ctx.accum_err;
}

static int ft8203_ts124qdm_wqxga_off(struct ft8203_ts124qdm_wqxga *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int ft8203_ts124qdm_wqxga_prepare(struct drm_panel *panel)
{
	struct ft8203_ts124qdm_wqxga *ctx = to_ft8203_ts124qdm_wqxga(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ft8203_ts124qdm_wqxga_supplies),
				    ctx->supplies);
	if (ret < 0)
		return ret;

	ret = ft8203_ts124qdm_wqxga_power_cycle(ctx);
	if (ret < 0)
		goto disable_supplies;

	ret = ft8203_ts124qdm_wqxga_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		goto disable_power;
	}

	return 0;

disable_power:
	regulator_disable(ctx->power);
disable_supplies:
	regulator_bulk_disable(ARRAY_SIZE(ft8203_ts124qdm_wqxga_supplies),
			       ctx->supplies);
	return ret;
}

static int ft8203_ts124qdm_wqxga_unprepare(struct drm_panel *panel)
{
	struct ft8203_ts124qdm_wqxga *ctx = to_ft8203_ts124qdm_wqxga(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = ft8203_ts124qdm_wqxga_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	regulator_disable(ctx->power);
	regulator_bulk_disable(ARRAY_SIZE(ft8203_ts124qdm_wqxga_supplies),
			       ctx->supplies);

	return 0;
}

static const struct drm_display_mode ft8203_ts124qdm_wqxga_mode = {
	.clock = (1600 + 90 + 8 + 89) * (2560 + 16 + 8 + 13) * 60 / 1000,
	.hdisplay = 1600,
	.hsync_start = 1600 + 90,
	.hsync_end = 1600 + 90 + 8,
	.htotal = 1600 + 90 + 8 + 89,
	.vdisplay = 2560,
	.vsync_start = 2560 + 16,
	.vsync_end = 2560 + 16 + 8,
	.vtotal = 2560 + 16 + 8 + 13,
	.width_mm = 166,
	.height_mm = 266,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int ft8203_ts124qdm_wqxga_get_modes(struct drm_panel *panel,
					   struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &ft8203_ts124qdm_wqxga_mode);
}

static const struct drm_panel_funcs ft8203_ts124qdm_wqxga_panel_funcs = {
	.prepare = ft8203_ts124qdm_wqxga_prepare,
	.unprepare = ft8203_ts124qdm_wqxga_unprepare,
	.get_modes = ft8203_ts124qdm_wqxga_get_modes,
};

static u16 ft8203_ts124qdm_wqxga_brightness_to_pwm(u16 brightness)
{
	if (brightness > 255)
		return 3976;

	if (brightness > 125)
		return (brightness - 125) * (3231 - 1200) /
		       (255 - 125) + 1200;

	if (brightness > 2)
		return (brightness - 2) * (1200 - 32) /
		       (125 - 2) + 32;

	return brightness > 1 ? 32 : 0;
}

static int ft8203_ts124qdm_wqxga_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };
	u16 brightness = backlight_get_brightness(bl);
	u16 pwm = ft8203_ts124qdm_wqxga_brightness_to_pwm(brightness);

	ft8203_dcs_write_var_seq_long_multi(&dsi_ctx,
					     MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
					     pwm >> 4, pwm & 0x0f);
	ft8203_dcs_write_var_seq_long_multi(&dsi_ctx,
					     MIPI_DCS_WRITE_CONTROL_DISPLAY,
					     brightness > 14 ? 0x2c : 0x24);

	return dsi_ctx.accum_err;
}

static const struct backlight_ops ft8203_ts124qdm_wqxga_bl_ops = {
	.update_status = ft8203_ts124qdm_wqxga_bl_update_status,
};

static struct backlight_device *
ft8203_ts124qdm_wqxga_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 125,
		.max_brightness = 425,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &ft8203_ts124qdm_wqxga_bl_ops, &props);
}

static int ft8203_ts124qdm_wqxga_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ft8203_ts124qdm_wqxga *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct ft8203_ts124qdm_wqxga, panel,
				   &ft8203_ts124qdm_wqxga_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(dev,
					 ARRAY_SIZE(ft8203_ts124qdm_wqxga_supplies),
					 ft8203_ts124qdm_wqxga_supplies,
					 &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->power = devm_regulator_get(dev, "power");
	if (IS_ERR(ctx->power))
		return dev_err_probe(dev, PTR_ERR(ctx->power),
				     "failed to get power supply\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = ft8203_ts124qdm_wqxga_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;
	dsi->dsc_slice_per_pkt = 2;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;

	ctx->dsc.slice_height = 40;
	ctx->dsc.slice_width = 800;
	ctx->dsc.slice_count = 2;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void ft8203_ts124qdm_wqxga_remove(struct mipi_dsi_device *dsi)
{
	struct ft8203_ts124qdm_wqxga *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ft8203_ts124qdm_wqxga_of_match[] = {
	{ .compatible = "boe,ts124qdm" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ft8203_ts124qdm_wqxga_of_match);

static struct mipi_dsi_driver ft8203_ts124qdm_wqxga_driver = {
	.probe = ft8203_ts124qdm_wqxga_probe,
	.remove = ft8203_ts124qdm_wqxga_remove,
	.driver = {
		.name = "panel-ft8203-ts124qdm-wqxga",
		.of_match_table = ft8203_ts124qdm_wqxga_of_match,
	},
};
module_mipi_dsi_driver(ft8203_ts124qdm_wqxga_driver);

MODULE_DESCRIPTION("DRM driver for BOE TS124QDM panels with FocalTech FT8203");
MODULE_LICENSE("GPL");
