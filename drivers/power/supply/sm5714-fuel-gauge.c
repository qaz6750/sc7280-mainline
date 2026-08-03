// SPDX-License-Identifier: GPL-2.0-only

#include <linux/math64.h>
#include <linux/mfd/sm5714.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>

struct sm5714_fuel_gauge {
	struct regmap *regmap;
	struct mutex sram_lock;
	struct power_supply *psy;
};

static int sm5714_fg_read_sram(struct sm5714_fuel_gauge *fuel_gauge,
			       unsigned int address, unsigned int *value)
{
	int ret;

	mutex_lock(&fuel_gauge->sram_lock);

	ret = regmap_write(fuel_gauge->regmap, SM5714_FG_REG_SRAM_RADDR,
			   address);
	if (!ret)
		ret = regmap_read(fuel_gauge->regmap, SM5714_FG_REG_SRAM_RDATA,
				  value);

	mutex_unlock(&fuel_gauge->sram_lock);

	return ret;
}

static int sm5714_fg_voltage_to_uv(unsigned int raw)
{
	int delta_uv = div_u64((u64)(raw & 0x7fff) * 10000, 109);

	return raw & BIT(15) ? 2700000 - delta_uv : 2700000 + delta_uv;
}

static int sm5714_fg_current_to_ua(unsigned int raw)
{
	int current_ua = div_u64((u64)(raw & 0x7fff) * 1000000, 2044);

	return raw & BIT(15) ? -current_ua : current_ua;
}

static int sm5714_fg_temperature_to_deci_c(unsigned int raw)
{
	int temperature = ((raw & 0x7fff) * 10 * 2989) >> 19;

	return raw & BIT(15) ? -temperature : temperature;
}

static int sm5714_fg_get_property(struct power_supply *psy,
				  enum power_supply_property property,
				  union power_supply_propval *value)
{
	struct sm5714_fuel_gauge *fuel_gauge = power_supply_get_drvdata(psy);
	struct power_supply *charger;
	unsigned int address;
	unsigned int raw;
	int ret;

	switch (property) {
	case POWER_SUPPLY_PROP_STATUS:
		charger = power_supply_get_by_reference(psy->dev.fwnode,
							"power-supplies");
		if (IS_ERR_OR_NULL(charger))
			return charger ? PTR_ERR(charger) : -ENODEV;

		ret = power_supply_get_property(charger, property, value);
		power_supply_put(charger);
		return ret;
	case POWER_SUPPLY_PROP_CAPACITY:
		address = SM5714_FG_SRAM_SOC;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		address = SM5714_FG_SRAM_VBAT;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		address = SM5714_FG_SRAM_VBAT_AVG;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		address = SM5714_FG_SRAM_OCV;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		address = SM5714_FG_SRAM_CURRENT;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		address = SM5714_FG_SRAM_CURRENT_AVG;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		address = SM5714_FG_SRAM_TEMPERATURE;
		break;
	default:
		return -EINVAL;
	}

	ret = sm5714_fg_read_sram(fuel_gauge, address, &raw);
	if (ret)
		return ret;

	switch (property) {
	case POWER_SUPPLY_PROP_CAPACITY:
		value->intval = clamp_val(DIV_ROUND_CLOSEST(raw, 256), 0, 100);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		value->intval = sm5714_fg_voltage_to_uv(raw);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		value->intval = ((u64)raw * 1000000) >> 11;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		value->intval = sm5714_fg_current_to_ua(raw);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		value->intval = sm5714_fg_temperature_to_deci_c(raw);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property sm5714_fg_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
};

static const struct power_supply_desc sm5714_fg_desc = {
	.name = "sm5714-battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = sm5714_fg_properties,
	.num_properties = ARRAY_SIZE(sm5714_fg_properties),
	.get_property = sm5714_fg_get_property,
	.external_power_changed = power_supply_changed,
};

static int sm5714_fg_probe(struct platform_device *pdev)
{
	struct power_supply_config psy_config = {};
	struct sm5714 *sm5714 = dev_get_drvdata(pdev->dev.parent);
	struct sm5714_fuel_gauge *fuel_gauge;

	if (!sm5714)
		return dev_err_probe(&pdev->dev, -ENODEV,
				     "missing parent device data\n");

	fuel_gauge = devm_kzalloc(&pdev->dev, sizeof(*fuel_gauge), GFP_KERNEL);
	if (!fuel_gauge)
		return -ENOMEM;

	fuel_gauge->regmap = sm5714->fuel_gauge_regmap;
	mutex_init(&fuel_gauge->sram_lock);

	psy_config.drv_data = fuel_gauge;
	psy_config.fwnode = dev_fwnode(&pdev->dev);

	fuel_gauge->psy = devm_power_supply_register(&pdev->dev,
						     &sm5714_fg_desc,
						     &psy_config);
	if (IS_ERR(fuel_gauge->psy))
		return dev_err_probe(&pdev->dev, PTR_ERR(fuel_gauge->psy),
				     "failed to register power supply\n");

	platform_set_drvdata(pdev, fuel_gauge);

	return 0;
}

static const struct of_device_id sm5714_fg_of_match[] = {
	{ .compatible = "siliconmitus,sm5714-fuel-gauge" },
	{ }
};
MODULE_DEVICE_TABLE(of, sm5714_fg_of_match);

static struct platform_driver sm5714_fg_driver = {
	.driver = {
		.name = "sm5714-fuel-gauge",
		.of_match_table = sm5714_fg_of_match,
	},
	.probe = sm5714_fg_probe,
};
module_platform_driver(sm5714_fg_driver);

MODULE_DESCRIPTION("Silicon Mitus SM5714 fuel-gauge driver");
MODULE_LICENSE("GPL");