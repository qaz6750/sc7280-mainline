// SPDX-License-Identifier: GPL-2.0-only

#include <linux/mfd/sm5714.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>

#define SM5714_INPUT_CURRENT_MIN_UA	100000
#define SM5714_INPUT_CURRENT_MAX_UA	3275000
#define SM5714_INPUT_CURRENT_STEP_UA	25000

#define SM5714_CHARGE_CURRENT_MIN_UA	109375
#define SM5714_CHARGE_CURRENT_MAX_UA	3500000
#define SM5714_CHARGE_CURRENT_STEP_UA	15625
#define SM5714_CHARGE_CURRENT_MIN_REG	0x07
#define SM5714_CHARGE_CURRENT_MAX_REG	0xe0

#define SM5714_CHARGE_VOLTAGE_MIN_UV	3700000
#define SM5714_CHARGE_VOLTAGE_MAX_UV	4620000

struct sm5714_charger {
	struct regmap *regmap;
	struct power_supply *psy;
};

static int sm5714_charger_vbus_enable(struct regulator_dev *rdev)
{
	struct sm5714_charger *charger = rdev_get_drvdata(rdev);
	int ret;

	ret = regmap_update_bits(charger->regmap, SM5714_CHG_REG_BSTCNTL1,
				 SM5714_CHG_BSTOUT_MASK,
				 SM5714_CHG_BSTOUT_5100MV);
	if (ret)
		return ret;

	ret = regmap_update_bits(charger->regmap, SM5714_CHG_REG_BSTCNTL1,
				 SM5714_CHG_OTG_CURRENT_MASK,
				 SM5714_CHG_OTG_CURRENT_900MA);
	if (ret)
		return ret;

	return regmap_update_bits(charger->regmap, SM5714_CHG_REG_CNTL2,
				  SM5714_CHG_OP_MODE_MASK,
				  SM5714_CHG_OP_MODE_USB_OTG);
}

static int sm5714_charger_vbus_disable(struct regulator_dev *rdev)
{
	struct sm5714_charger *charger = rdev_get_drvdata(rdev);

	return regmap_update_bits(charger->regmap, SM5714_CHG_REG_CNTL2,
				  SM5714_CHG_OP_MODE_MASK,
				  SM5714_CHG_OP_MODE_CHG_ON_VBUS);
}

static int sm5714_charger_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct sm5714_charger *charger = rdev_get_drvdata(rdev);
	unsigned int reg;
	int ret;

	ret = regmap_read(charger->regmap, SM5714_CHG_REG_CNTL2, &reg);
	if (ret)
		return ret;

	return (reg & SM5714_CHG_OP_MODE_MASK) ==
		SM5714_CHG_OP_MODE_USB_OTG;
}

static const struct regulator_ops sm5714_charger_vbus_ops = {
	.enable = sm5714_charger_vbus_enable,
	.disable = sm5714_charger_vbus_disable,
	.is_enabled = sm5714_charger_vbus_is_enabled,
};

static const struct regulator_desc sm5714_charger_vbus_desc = {
	.name = "sm5714-usb-otg-vbus",
	.of_match = "usb-otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sm5714_charger_vbus_ops,
	.fixed_uV = 5100000,
	.n_voltages = 1,
};

static int sm5714_charger_get_status(struct sm5714_charger *charger,
				     int *status)
{
	unsigned int status1;
	unsigned int status2;
	int ret;

	ret = regmap_read(charger->regmap, SM5714_CHG_REG_STATUS1, &status1);
	if (ret)
		return ret;

	ret = regmap_read(charger->regmap, SM5714_CHG_REG_STATUS2, &status2);
	if (ret)
		return ret;

	if (status2 & SM5714_CHG_STATUS2_TOPOFF)
		*status = POWER_SUPPLY_STATUS_FULL;
	else if (status2 & SM5714_CHG_STATUS2_CHGON)
		*status = POWER_SUPPLY_STATUS_CHARGING;
	else if (status1 & SM5714_CHG_STATUS1_VBUSPOK)
		*status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		*status = POWER_SUPPLY_STATUS_DISCHARGING;

	return 0;
}

static int sm5714_charger_get_voltage(unsigned int reg)
{
	reg &= SM5714_CHG_BATREG_MASK;

	if (reg <= 0x03)
		return 3700000 + reg * 50000;
	if (reg <= 0x05)
		return 3900000 + (reg - 0x04) * 100000;

	return 4050000 + (reg - 0x06) * 10000;
}

static int sm5714_charger_voltage_to_reg(int voltage_uv)
{
	if (voltage_uv < SM5714_CHARGE_VOLTAGE_MIN_UV ||
	    voltage_uv > SM5714_CHARGE_VOLTAGE_MAX_UV)
		return -EINVAL;

	if (voltage_uv < 3900000)
		return (voltage_uv - 3700000) / 50000;
	if (voltage_uv < 4050000)
		return 0x04 + (voltage_uv - 3900000) / 100000;

	return 0x06 + (voltage_uv - 4050000) / 10000;
}

static int sm5714_charger_get_property(struct power_supply *psy,
				       enum power_supply_property property,
				       union power_supply_propval *value)
{
	struct sm5714_charger *charger = power_supply_get_drvdata(psy);
	unsigned int reg;
	int ret;

	switch (property) {
	case POWER_SUPPLY_PROP_STATUS:
		return sm5714_charger_get_status(charger, &value->intval);
	case POWER_SUPPLY_PROP_ONLINE:
		ret = regmap_read(charger->regmap, SM5714_CHG_REG_STATUS1, &reg);
		if (!ret)
			value->intval = !!(reg & SM5714_CHG_STATUS1_VBUSPOK);
		return ret;
	case POWER_SUPPLY_PROP_PRESENT:
		ret = regmap_read(charger->regmap, SM5714_CHG_REG_STATUS2, &reg);
		if (!ret)
			value->intval = !(reg & SM5714_CHG_STATUS2_NOBAT);
		return ret;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = regmap_read(charger->regmap, SM5714_CHG_REG_VBUSCNTL, &reg);
		if (!ret)
			value->intval = SM5714_INPUT_CURRENT_MIN_UA +
				(reg & SM5714_CHG_VBUS_LIMIT_MASK) *
				SM5714_INPUT_CURRENT_STEP_UA;
		return ret;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = regmap_read(charger->regmap, SM5714_CHG_REG_CHGCNTL2, &reg);
		if (ret)
			return ret;
		if (reg <= SM5714_CHARGE_CURRENT_MIN_REG)
			value->intval = SM5714_CHARGE_CURRENT_MIN_UA;
		else if (reg >= SM5714_CHARGE_CURRENT_MAX_REG)
			value->intval = SM5714_CHARGE_CURRENT_MAX_UA;
		else
			value->intval = SM5714_CHARGE_CURRENT_MIN_UA +
				(reg - SM5714_CHARGE_CURRENT_MIN_REG) *
				SM5714_CHARGE_CURRENT_STEP_UA;
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = regmap_read(charger->regmap, SM5714_CHG_REG_CHGCNTL4, &reg);
		if (!ret)
			value->intval = sm5714_charger_get_voltage(reg);
		return ret;
	default:
		return -EINVAL;
	}
}

static int sm5714_charger_set_property(struct power_supply *psy,
				       enum power_supply_property property,
				       const union power_supply_propval *value)
{
	struct sm5714_charger *charger = power_supply_get_drvdata(psy);
	int reg;

	switch (property) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (value->intval < SM5714_INPUT_CURRENT_MIN_UA ||
		    value->intval > SM5714_INPUT_CURRENT_MAX_UA)
			return -EINVAL;
		reg = (value->intval - SM5714_INPUT_CURRENT_MIN_UA) /
			SM5714_INPUT_CURRENT_STEP_UA;
		return regmap_update_bits(charger->regmap,
					  SM5714_CHG_REG_VBUSCNTL,
					  SM5714_CHG_VBUS_LIMIT_MASK, reg);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (value->intval < SM5714_CHARGE_CURRENT_MIN_UA ||
		    value->intval > SM5714_CHARGE_CURRENT_MAX_UA)
			return -EINVAL;
		reg = SM5714_CHARGE_CURRENT_MIN_REG +
			(value->intval - SM5714_CHARGE_CURRENT_MIN_UA) /
			SM5714_CHARGE_CURRENT_STEP_UA;
		return regmap_write(charger->regmap, SM5714_CHG_REG_CHGCNTL2,
				    reg);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		reg = sm5714_charger_voltage_to_reg(value->intval);
		if (reg < 0)
			return reg;
		return regmap_update_bits(charger->regmap,
					  SM5714_CHG_REG_CHGCNTL4,
					  SM5714_CHG_BATREG_MASK, reg);
	default:
		return -EINVAL;
	}
}

static int sm5714_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property property)
{
	switch (property) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property sm5714_charger_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

static const struct power_supply_desc sm5714_charger_desc = {
	.name = "sm5714-charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = sm5714_charger_properties,
	.num_properties = ARRAY_SIZE(sm5714_charger_properties),
	.get_property = sm5714_charger_get_property,
	.set_property = sm5714_charger_set_property,
	.property_is_writeable = sm5714_charger_property_is_writeable,
};

static int sm5714_charger_probe(struct platform_device *pdev)
{
	struct regulator_config regulator_config = { };
	struct power_supply_config psy_config = {};
	struct sm5714 *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_charger *charger;
	struct regulator_dev *rdev;

	if (!sm5714)
		return dev_err_probe(&pdev->dev, -ENODEV,
				     "missing parent device data\n");

	charger = devm_kzalloc(&pdev->dev, sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->regmap = sm5714->charger_regmap;
	psy_config.drv_data = charger;
	psy_config.fwnode = dev_fwnode(&pdev->dev);

	regulator_config.dev = &pdev->dev;
	regulator_config.driver_data = charger;
	regulator_config.regmap = charger->regmap;
	rdev = devm_regulator_register(&pdev->dev,
				       &sm5714_charger_vbus_desc,
				       &regulator_config);
	if (IS_ERR(rdev))
		return dev_err_probe(&pdev->dev, PTR_ERR(rdev),
				     "failed to register OTG VBUS regulator\n");

	dev_info(&pdev->dev, "registered OTG VBUS regulator\n");

	charger->psy = devm_power_supply_register(&pdev->dev,
						  &sm5714_charger_desc,
						  &psy_config);
	if (IS_ERR(charger->psy)) {
		dev_warn(&pdev->dev, "failed to register power supply: %pe\n",
			 charger->psy);
		charger->psy = NULL;
	} else {
		dev_info(&pdev->dev, "registered charger power supply\n");
	}

	platform_set_drvdata(pdev, charger);

	return 0;
}

static const struct of_device_id sm5714_charger_of_match[] = {
	{ .compatible = "siliconmitus,sm5714-charger" },
	{ }
};
MODULE_DEVICE_TABLE(of, sm5714_charger_of_match);

static struct platform_driver sm5714_charger_driver = {
	.driver = {
		.name = "sm5714-charger",
		.of_match_table = sm5714_charger_of_match,
	},
	.probe = sm5714_charger_probe,
};
module_platform_driver(sm5714_charger_driver);

MODULE_DESCRIPTION("Silicon Mitus SM5714 charger driver");
MODULE_LICENSE("GPL");