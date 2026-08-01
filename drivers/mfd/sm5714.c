// SPDX-License-Identifier: GPL-2.0-only

#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sm5714.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>

static const struct mfd_cell sm5714_charger_cell = {
	.name = "sm5714-charger",
	.of_compatible = "siliconmitus,sm5714-charger",
};

static const struct mfd_cell sm5714_fuel_gauge_cell = {
	.name = "sm5714-fuel-gauge",
	.of_compatible = "siliconmitus,sm5714-fuel-gauge",
};

static const struct regmap_config sm5714_charger_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SM5714_CHG_REG_DEVICE_ID,
};

static const struct regmap_config sm5714_muic_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x1e,
};

static const struct regmap_config sm5714_fuel_gauge_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
	.max_register = 0x91,
};

static int sm5714_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sm5714 *sm5714;
	unsigned int device_id;
	unsigned int revision;
	int ret;

	sm5714 = devm_kzalloc(dev, sizeof(*sm5714), GFP_KERNEL);
	if (!sm5714)
		return -ENOMEM;

	sm5714->dev = dev;
	sm5714->charger = client;
	i2c_set_clientdata(client, sm5714);

	sm5714->charger_regmap = devm_regmap_init_i2c(client,
						      &sm5714_charger_regmap_config);
	if (IS_ERR(sm5714->charger_regmap))
		return dev_err_probe(dev, PTR_ERR(sm5714->charger_regmap),
				     "failed to initialize charger regmap\n");

	ret = regmap_read(sm5714->charger_regmap, SM5714_CHG_REG_DEVICE_ID,
			  &device_id);
	if (ret)
		return dev_err_probe(dev, ret, "failed to read device ID\n");

	if ((device_id & 0x7) != 0x1)
		return dev_err_probe(dev, -ENODEV,
				     "unexpected charger device ID %#x\n", device_id);
	revision = device_id >> 3;

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO,
				   &sm5714_charger_cell, 1, NULL, 0, NULL);
	if (ret)
		return dev_err_probe(dev, ret, "failed to add charger device\n");

	sm5714->muic = devm_i2c_new_dummy_device(dev, client->adapter,
						  SM5714_I2C_ADDR_MUIC);
	if (IS_ERR(sm5714->muic))
		dev_warn(dev, "failed to create MUIC client: %pe\n", sm5714->muic);

	if (!IS_ERR(sm5714->muic)) {
		sm5714->muic_regmap = devm_regmap_init_i2c(sm5714->muic,
							   &sm5714_muic_regmap_config);
		if (IS_ERR(sm5714->muic_regmap)) {
			dev_warn(dev, "failed to initialize MUIC regmap: %pe\n",
				 sm5714->muic_regmap);
		} else {
			ret = regmap_read(sm5714->muic_regmap,
					  SM5714_MUIC_REG_DEVICE_ID, &device_id);
			if (ret)
				dev_warn(dev, "failed to read MUIC device ID: %d\n", ret);
			else if (device_id != 0x1)
				dev_warn(dev, "unexpected MUIC device ID %#x\n", device_id);
		}
	}

	sm5714->fuel_gauge = devm_i2c_new_dummy_device(dev, client->adapter,
							SM5714_I2C_ADDR_FUEL_GAUGE);
	if (IS_ERR(sm5714->fuel_gauge)) {
		dev_warn(dev, "failed to create fuel-gauge client: %pe\n",
			 sm5714->fuel_gauge);
		goto done;
	}

	sm5714->fuel_gauge_regmap = devm_regmap_init_i2c(sm5714->fuel_gauge,
							 &sm5714_fuel_gauge_regmap_config);
	if (IS_ERR(sm5714->fuel_gauge_regmap)) {
		dev_warn(dev, "failed to initialize fuel-gauge regmap: %pe\n",
			 sm5714->fuel_gauge_regmap);
		goto done;
	}

	ret = regmap_read(sm5714->fuel_gauge_regmap, SM5714_FG_REG_DEVICE_ID,
			  &device_id);
	if (ret) {
		dev_warn(dev, "failed to read fuel-gauge device ID: %d\n", ret);
		goto done;
	}

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO,
				   &sm5714_fuel_gauge_cell, 1, NULL, 0, NULL);
	if (ret)
		dev_warn(dev, "failed to add fuel-gauge device: %d\n", ret);

done:
	if (device_property_read_bool(dev, "wakeup-source")) {
		ret = devm_device_init_wakeup(dev);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to enable wakeup\n");
	}

	dev_info(dev, "SM5714 revision %u detected\n", revision);

	return 0;
}

static const struct of_device_id sm5714_of_match[] = {
	{ .compatible = "siliconmitus,sm5714" },
	{ }
};
MODULE_DEVICE_TABLE(of, sm5714_of_match);

static const struct i2c_device_id sm5714_i2c_id[] = {
	{ "sm5714" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5714_i2c_id);

static struct i2c_driver sm5714_driver = {
	.driver = {
		.name = "sm5714",
		.of_match_table = sm5714_of_match,
	},
	.probe = sm5714_probe,
	.id_table = sm5714_i2c_id,
};
module_i2c_driver(sm5714_driver);

MODULE_DESCRIPTION("Silicon Mitus SM5714 multi-function core driver");
MODULE_LICENSE("GPL");