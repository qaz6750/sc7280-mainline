/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __LINUX_MFD_SM5714_H
#define __LINUX_MFD_SM5714_H

#include <linux/bits.h>
#include <linux/notifier.h>
#include <linux/types.h>

struct device;
struct i2c_client;
struct regmap;

#define SM5714_I2C_ADDR_MUIC		0x25
#define SM5714_I2C_ADDR_CHARGER		0x49
#define SM5714_I2C_ADDR_FUEL_GAUGE	0x71

#define SM5714_CHG_REG_DEVICE_ID		0x50
#define SM5714_MUIC_REG_DEVICE_ID	0x00
#define SM5714_FG_REG_DEVICE_ID		0x00

#define SM5714_CHG_REG_INT_SOURCE	0x00
#define SM5714_CHG_REG_INT1		0x01
#define SM5714_CHG_REG_INT2		0x02
#define SM5714_CHG_REG_INTMSK1		0x07
#define SM5714_CHG_REG_INTMSK2		0x08
#define SM5714_CHG_REG_STATUS1		0x0d
#define SM5714_CHG_REG_STATUS2		0x0e
#define SM5714_CHG_REG_CNTL2		0x14
#define SM5714_CHG_REG_VBUSCNTL		0x15
#define SM5714_CHG_REG_CHGCNTL2		0x18
#define SM5714_CHG_REG_CHGCNTL4		0x1a
#define SM5714_CHG_REG_BSTCNTL1		0x23

#define SM5714_CHG_STATUS1_VBUSPOK	BIT(0)
#define SM5714_CHG_STATUS2_NOBAT		BIT(2)
#define SM5714_CHG_STATUS2_CHGON		BIT(3)
#define SM5714_CHG_STATUS2_TOPOFF	BIT(5)

#define SM5714_CHG_INT1_VBUSPOK		BIT(0)
#define SM5714_CHG_INT2_NOBAT		BIT(2)
#define SM5714_CHG_INT2_CHGON		BIT(3)
#define SM5714_CHG_INT2_TOPOFF		BIT(5)

#define SM5714_CHG_VBUS_LIMIT_MASK	GENMASK(6, 0)
#define SM5714_CHG_BATREG_MASK		GENMASK(5, 0)
#define SM5714_CHG_OP_MODE_MASK		GENMASK(3, 0)
#define SM5714_CHG_BSTOUT_MASK		GENMASK(3, 0)
#define SM5714_CHG_OTG_CURRENT_MASK	GENMASK(7, 6)

#define SM5714_CHG_OP_MODE_CHG_ON_VBUS	0x5
#define SM5714_CHG_OP_MODE_USB_OTG	0x7
#define SM5714_CHG_BSTOUT_5100MV		0x6
#define SM5714_CHG_OTG_CURRENT_900MA	BIT(6)

#define SM5714_FG_REG_SYSTEM_STATUS	0x10
#define SM5714_FG_REG_INT		0x02
#define SM5714_FG_REG_INT_MASK		0x04
#define SM5714_FG_REG_SRAM_RADDR		0x8c
#define SM5714_FG_REG_SRAM_RDATA		0x8d

#define SM5714_FG_SRAM_SOC		0x00
#define SM5714_FG_SRAM_OCV		0x01
#define SM5714_FG_SRAM_VBAT		0x03
#define SM5714_FG_SRAM_CURRENT		0x05
#define SM5714_FG_SRAM_TEMPERATURE	0x07
#define SM5714_FG_SRAM_VBAT_AVG		0x08
#define SM5714_FG_SRAM_CURRENT_AVG	0x09

#define SM5714_IRQ_SOURCE_MUIC		BIT(0)
#define SM5714_IRQ_SOURCE_CHARGER	BIT(1)
#define SM5714_IRQ_SOURCE_FUEL_GAUGE	BIT(2)

enum sm5714_event {
	SM5714_EVENT_CHARGER,
};

struct sm5714 {
	struct device *dev;
	struct i2c_client *charger;
	struct i2c_client *muic;
	struct i2c_client *fuel_gauge;
	struct regmap *charger_regmap;
	struct regmap *muic_regmap;
	struct regmap *fuel_gauge_regmap;
	struct blocking_notifier_head notifier;
};

int sm5714_register_notifier(struct sm5714 *sm5714,
			     struct notifier_block *notifier);
int sm5714_unregister_notifier(struct sm5714 *sm5714,
			       struct notifier_block *notifier);

#endif
