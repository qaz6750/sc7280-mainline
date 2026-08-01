// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/role.h>
#include <linux/usb/typec.h>

#define SM5714_REG_INT1			0x01
#define SM5714_REG_INT_MASK1		0x06
#define SM5714_REG_STATUS1		0x0b
#define SM5714_REG_CORR_CNTL4		0x23
#define SM5714_REG_CORR_CNTL5		0x24
#define SM5714_REG_CC_STATUS		0x28
#define SM5714_REG_CC_CNTL1		0x29
#define SM5714_REG_PD_CNTL2		0x39

#define SM5714_INT_ATTACH		BIT(3)
#define SM5714_INT_DETACH		BIT(4)
#define SM5714_STATUS_ATTACHED		BIT(3)

#define SM5714_CC_ATTACH_TYPE		GENMASK(2, 0)
#define SM5714_CC_ATTACH_SOURCE		1
#define SM5714_CC_ATTACH_SINK		2
#define SM5714_CC_ATTACH_AUDIO		3
#define SM5714_CC_CABLE_FLIP		BIT(5)

#define SM5714_PD_DATA_ROLE_DFP		BIT(0)
#define SM5714_PD_POWER_ROLE_SOURCE	BIT(1)
#define SM5714_PD_ROLE_MASK		GENMASK(1, 0)

struct sm5714_typec {
	struct device *dev;
	struct regmap *regmap;
	struct typec_port *port;
	struct typec_partner *partner;
	struct usb_role_switch *role_sw;
	struct regulator *vbus;
	enum usb_role usb_role;
	bool vbus_on;
};

static const struct regmap_config sm5714_typec_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};

static void sm5714_typec_detach(struct sm5714_typec *typec)
{
	usb_role_switch_set_role(typec->role_sw, USB_ROLE_NONE);
	typec->usb_role = USB_ROLE_NONE;
	if (typec->vbus_on) {
		regulator_disable(typec->vbus);
		typec->vbus_on = false;
	}

	if (typec->partner) {
		typec_unregister_partner(typec->partner);
		typec->partner = NULL;
	}

	typec_set_orientation(typec->port, TYPEC_ORIENTATION_NONE);
}

static int sm5714_typec_attach(struct sm5714_typec *typec,
			      unsigned int cc_status)
{
	struct typec_partner_desc desc = { };
	enum typec_orientation orientation;
	enum typec_data_role data_role;
	enum typec_role power_role;
	enum usb_role usb_role;
	unsigned int attach_type;
	int ret;

	attach_type = FIELD_GET(SM5714_CC_ATTACH_TYPE, cc_status);
	switch (attach_type) {
	case SM5714_CC_ATTACH_SOURCE:
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		usb_role = USB_ROLE_DEVICE;
		break;
	case SM5714_CC_ATTACH_SINK:
		power_role = TYPEC_SOURCE;
		data_role = TYPEC_HOST;
		usb_role = USB_ROLE_HOST;
		break;
	case SM5714_CC_ATTACH_AUDIO:
		desc.accessory = TYPEC_ACCESSORY_AUDIO;
		usb_role = USB_ROLE_NONE;
		power_role = TYPEC_SINK;
		data_role = TYPEC_DEVICE;
		break;
	default:
		dev_dbg(typec->dev, "unsupported CC attach type %#x\n",
			attach_type);
		sm5714_typec_detach(typec);
		return 0;
	}

	orientation = cc_status & SM5714_CC_CABLE_FLIP ?
		TYPEC_ORIENTATION_REVERSE : TYPEC_ORIENTATION_NORMAL;

	if (typec->partner && typec->usb_role == usb_role) {
		typec_set_orientation(typec->port, orientation);
		return 0;
	}

	sm5714_typec_detach(typec);
	ret = regmap_update_bits(typec->regmap, SM5714_REG_PD_CNTL2,
				 SM5714_PD_ROLE_MASK,
				 usb_role == USB_ROLE_HOST ?
				 SM5714_PD_DATA_ROLE_DFP |
				 SM5714_PD_POWER_ROLE_SOURCE : 0);
	if (ret)
		return ret;

	if (usb_role == USB_ROLE_HOST) {
		ret = regulator_enable(typec->vbus);
		if (ret)
			return ret;
		typec->vbus_on = true;
	}

	ret = usb_role_switch_set_role(typec->role_sw, usb_role);
	if (ret) {
		if (typec->vbus_on) {
			regulator_disable(typec->vbus);
			typec->vbus_on = false;
		}
		return ret;
	}

	typec_set_pwr_role(typec->port, power_role);
	typec_set_data_role(typec->port, data_role);
	typec_set_pwr_opmode(typec->port, TYPEC_PWR_MODE_USB);
	typec_set_orientation(typec->port, orientation);

	desc.usb_pd = false;
	typec->partner = typec_register_partner(typec->port, &desc);
	if (IS_ERR(typec->partner)) {
		ret = PTR_ERR(typec->partner);
		typec->partner = NULL;
		usb_role_switch_set_role(typec->role_sw, USB_ROLE_NONE);
		if (typec->vbus_on) {
			regulator_disable(typec->vbus);
			typec->vbus_on = false;
		}
		typec_set_orientation(typec->port, TYPEC_ORIENTATION_NONE);
		return ret;
	}

	typec->usb_role = usb_role;

	return 0;
}

static int sm5714_typec_update(struct sm5714_typec *typec)
{
	unsigned int cc_status, status1;
	int ret;

	ret = regmap_read(typec->regmap, SM5714_REG_STATUS1, &status1);
	if (ret)
		return ret;

	if (!(status1 & SM5714_STATUS_ATTACHED)) {
		sm5714_typec_detach(typec);
		return 0;
	}

	ret = regmap_read(typec->regmap, SM5714_REG_CC_STATUS, &cc_status);
	if (ret)
		return ret;

	return sm5714_typec_attach(typec, cc_status);
}

static irqreturn_t sm5714_typec_irq(int irq, void *data)
{
	struct sm5714_typec *typec = data;
	u8 interrupts[5];
	int ret;

	ret = regmap_bulk_read(typec->regmap, SM5714_REG_INT1,
			       interrupts, ARRAY_SIZE(interrupts));
	if (ret) {
		dev_err_ratelimited(typec->dev,
				    "failed to read interrupts: %d\n", ret);
		return IRQ_HANDLED;
	}

	if (interrupts[0] & (SM5714_INT_ATTACH | SM5714_INT_DETACH)) {
		ret = sm5714_typec_update(typec);
		if (ret)
			dev_err_ratelimited(typec->dev,
					    "failed to update port: %d\n", ret);
	}

	return IRQ_HANDLED;
}

static int sm5714_typec_hw_init(struct sm5714_typec *typec)
{
	u8 interrupts[5];
	u8 masks[5] = {
		0xff & ~(SM5714_INT_ATTACH | SM5714_INT_DETACH),
		0xff, 0xff, 0xff, 0xff,
	};
	int ret;

	ret = regmap_write(typec->regmap, SM5714_REG_CC_CNTL1, 0x41);
	if (ret)
		return ret;

	ret = regmap_write(typec->regmap, SM5714_REG_CORR_CNTL5, 0x00);
	if (ret)
		return ret;

	ret = regmap_write(typec->regmap, SM5714_REG_CORR_CNTL4, 0x00);
	if (ret)
		return ret;

	ret = regmap_bulk_read(typec->regmap, SM5714_REG_INT1,
			       interrupts, ARRAY_SIZE(interrupts));
	if (ret)
		return ret;

	return regmap_bulk_write(typec->regmap, SM5714_REG_INT_MASK1,
				 masks, ARRAY_SIZE(masks));
}

static int sm5714_typec_probe(struct i2c_client *client)
{
	struct typec_capability capability = { };
	struct fwnode_handle *connector;
	struct sm5714_typec *typec;
	int ret;

	typec = devm_kzalloc(&client->dev, sizeof(*typec), GFP_KERNEL);
	if (!typec)
		return -ENOMEM;

	typec->dev = &client->dev;
	typec->regmap = devm_regmap_init_i2c(client,
					     &sm5714_typec_regmap_config);
	if (IS_ERR(typec->regmap))
		return PTR_ERR(typec->regmap);

	typec->vbus = devm_regulator_get(typec->dev, "vbus");
	if (IS_ERR(typec->vbus))
		return dev_err_probe(typec->dev, PTR_ERR(typec->vbus),
				     "failed to get VBUS regulator\n");

	connector = device_get_named_child_node(typec->dev, "connector");
	if (!connector)
		return dev_err_probe(typec->dev, -ENODEV,
				     "connector node is missing\n");

	typec->role_sw = fwnode_usb_role_switch_get(connector);
	if (IS_ERR(typec->role_sw)) {
		ret = dev_err_probe(typec->dev, PTR_ERR(typec->role_sw),
				    "failed to get USB role switch\n");
		goto put_connector;
	}

	ret = typec_get_fw_cap(&capability, connector);
	if (ret)
		goto put_role_sw;

	capability.revision = USB_TYPEC_REV_1_2;
	capability.orientation_aware = true;
	capability.driver_data = typec;
	typec->port = typec_register_port(typec->dev, &capability);
	if (IS_ERR(typec->port)) {
		ret = PTR_ERR(typec->port);
		goto put_role_sw;
	}

	i2c_set_clientdata(client, typec);
	typec->usb_role = USB_ROLE_NONE;

	ret = sm5714_typec_hw_init(typec);
	if (ret)
		goto unregister_port;

	ret = devm_request_threaded_irq(typec->dev, client->irq, NULL,
					 sm5714_typec_irq, IRQF_ONESHOT,
					 dev_name(typec->dev), typec);
	if (ret)
		goto unregister_port;

	ret = sm5714_typec_update(typec);
	if (ret)
		goto unregister_port;

	device_init_wakeup(typec->dev, true);
	fwnode_handle_put(connector);

	return 0;

unregister_port:
	typec_unregister_port(typec->port);
put_role_sw:
	usb_role_switch_put(typec->role_sw);
put_connector:
	fwnode_handle_put(connector);

	return ret;
}

static void sm5714_typec_remove(struct i2c_client *client)
{
	struct sm5714_typec *typec = i2c_get_clientdata(client);

	sm5714_typec_detach(typec);
	typec_unregister_port(typec->port);
	usb_role_switch_put(typec->role_sw);
}

static const struct of_device_id sm5714_typec_of_match[] = {
	{ .compatible = "siliconmitus,sm5714-typec" },
	{ }
};
MODULE_DEVICE_TABLE(of, sm5714_typec_of_match);

static struct i2c_driver sm5714_typec_driver = {
	.driver = {
		.name = "sm5714-typec",
		.of_match_table = sm5714_typec_of_match,
	},
	.probe = sm5714_typec_probe,
	.remove = sm5714_typec_remove,
};
module_i2c_driver(sm5714_typec_driver);

MODULE_DESCRIPTION("Silicon Mitus SM5714 Type-C port controller driver");
MODULE_LICENSE("GPL");