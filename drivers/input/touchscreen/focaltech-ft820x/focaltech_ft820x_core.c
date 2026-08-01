// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/spi/spi.h>

#include "focaltech_ft820x.h"

#define FT820X_REG_TOUCH_DATA		0x01
#define FT820X_REG_CHIP_ID_H		0xa3
#define FT820X_REG_CHIP_ID_L		0x9f
#define FT820X_REG_POWER_MODE		0xa5

#define FT820X_CHIP_ID_H			0x82
#define FT820X_CHIP_ID_L			0x03
#define FT820X_CHIP_CID_L_A		0x0a
#define FT820X_CHIP_CID_L_B		0x0b
#define FT820X_POWER_MODE_SLEEP		0x03

#define FT820X_TOUCH_COUNT_OFFSET	2
#define FT820X_FIRST_TOUCH_OFFSET	3
#define FT820X_TOUCH_RECORD_SIZE		6
#define FT820X_EVENT_DOWN		0
#define FT820X_EVENT_UP			1
#define FT820X_EVENT_CONTACT		2
#define FT820X_INVALID_TOUCH_ID		0x0a

static int ft820x_input_open(struct input_dev *input);
static void ft820x_input_close(struct input_dev *input);

void ft820x_reset(struct ft820x_data *data, unsigned int delay_ms)
{
	gpiod_set_value_cansleep(data->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(data->reset_gpio, 0);
	if (delay_ms)
		msleep(delay_ms);
}

static bool ft820x_chip_id_valid(u8 chip_id_h, u8 chip_id_l)
{
	if (chip_id_h != FT820X_CHIP_ID_H)
		return false;

	return chip_id_l == FT820X_CHIP_ID_L ||
	       chip_id_l == FT820X_CHIP_CID_L_A ||
	       chip_id_l == FT820X_CHIP_CID_L_B;
}

static int ft820x_wait_ready(struct ft820x_data *data)
{
	u8 chip_id_h = 0;
	u8 chip_id_l = 0;
	int error = -ENODEV;
	int attempt;

	for (attempt = 0; attempt < 10; attempt++) {
		error = ft820x_spi_read_u8(data, FT820X_REG_CHIP_ID_H,
					   &chip_id_h);
		if (!error)
			error = ft820x_spi_read_u8(data, FT820X_REG_CHIP_ID_L,
						   &chip_id_l);
		if (!error && ft820x_chip_id_valid(chip_id_h, chip_id_l)) {
			dev_dbg(&data->spi->dev, "found chip ID %02x:%02x\n",
				chip_id_h, chip_id_l);
			return 0;
		}

		msleep(20);
	}

	dev_err(&data->spi->dev, "unexpected chip ID %02x:%02x\n",
		chip_id_h, chip_id_l);
	return error ?: -ENODEV;
}

static bool ft820x_touch_active(u8 event)
{
	return event == FT820X_EVENT_DOWN || event == FT820X_EVENT_CONTACT;
}

static int ft820x_report_touch(struct ft820x_data *data)
{
	struct ft820x_contact {
		u16 x;
		u16 y;
		u8 id;
		u8 event;
		u8 area;
	} contacts[FT820X_MAX_TOUCHES];
	struct input_dev *input = data->input;
	unsigned long seen_ids = 0;
	u8 touch_count;
	int active_touches = 0;
	int records = 0;
	int index;

	touch_count = data->touch_data[FT820X_TOUCH_COUNT_OFFSET] & 0x0f;
	if (touch_count > FT820X_MAX_TOUCHES)
		return -EPROTO;

	for (index = 0; index < FT820X_MAX_TOUCHES; index++) {
		size_t offset = FT820X_FIRST_TOUCH_OFFSET +
				index * FT820X_TOUCH_RECORD_SIZE;
		u8 event = data->touch_data[offset] >> 6;
		u8 touch_id = data->touch_data[offset + 2] >> 4;

		if (touch_id >= FT820X_INVALID_TOUCH_ID)
			break;
		if (test_and_set_bit(touch_id, &seen_ids))
			return -EPROTO;
		if (event > FT820X_EVENT_CONTACT)
			return -EPROTO;

		contacts[records].x =
			(data->touch_data[offset] & 0x0f) << 8 |
			data->touch_data[offset + 1];
		contacts[records].y =
			(data->touch_data[offset + 2] & 0x0f) << 8 |
			data->touch_data[offset + 3];
		contacts[records].id = touch_id;
		contacts[records].event = event;
		contacts[records].area = data->touch_data[offset + 5];

		if (ft820x_touch_active(event))
			active_touches++;
		records++;
	}

	if (active_touches != touch_count)
		return -EPROTO;

	for (index = 0; index < records; index++) {
		bool active = ft820x_touch_active(contacts[index].event);

		input_mt_slot(input, contacts[index].id);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, active);
		if (!active)
			continue;

		touchscreen_report_pos(input, &data->prop, contacts[index].x,
				       contacts[index].y, true);
		input_report_abs(input, ABS_MT_TOUCH_MAJOR,
				 max_t(u8, contacts[index].area, 1));
	}

	input_mt_sync_frame(input);
	input_mt_report_pointer_emulation(input, true);
	input_sync(input);

	return 0;
}

static irqreturn_t ft820x_irq_thread(int irq, void *dev_id)
{
	struct ft820x_data *data = dev_id;
	int error;

	if (!READ_ONCE(data->running))
		return IRQ_HANDLED;

	data->touch_data[0] = FT820X_REG_TOUCH_DATA;
	error = ft820x_spi_read(data, FT820X_REG_TOUCH_DATA,
				data->touch_data + 1,
				sizeof(data->touch_data) - 1);
	if (!error)
		error = ft820x_report_touch(data);
	if (error)
		dev_err_ratelimited(&data->spi->dev,
				    "failed to read touch report: %d\n", error);

	return IRQ_HANDLED;
}

static void ft820x_release_all(struct ft820x_data *data)
{
	int slot;

	for (slot = 0; slot < FT820X_MAX_TOUCHES; slot++) {
		input_mt_slot(data->input, slot);
		input_mt_report_slot_state(data->input, MT_TOOL_FINGER, false);
	}
	input_mt_sync_frame(data->input);
	input_mt_report_pointer_emulation(data->input, true);
	input_sync(data->input);
}

static int ft820x_input_init(struct ft820x_data *data)
{
	struct device *dev = &data->spi->dev;
	struct input_dev *input;
	int error;

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	input->name = "FocalTech FT820x Touchscreen";
	input->id.bustype = BUS_SPI;
	input->open = ft820x_input_open;
	input->close = ft820x_input_close;
	input_set_drvdata(input, data);

	input_set_capability(input, EV_KEY, BTN_TOUCH);
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, 1599, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, 2559, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, U8_MAX, 0, 0);
	touchscreen_parse_properties(input, true, &data->prop);

	error = input_mt_init_slots(input, FT820X_MAX_TOUCHES,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error)
		return error;

	data->input = input;
	return 0;
}

static void ft820x_power_off(void *context)
{
	struct ft820x_data *data = context;

	if (data->reset_gpio)
		gpiod_set_value_cansleep(data->reset_gpio, 1);
	data->powered = false;
}

static int ft820x_power_on(struct ft820x_data *data)
{
	int error;

	if (data->powered)
		return 0;

	ft820x_reset(data, 200);
	error = ft820x_wait_ready(data);
	if (error) {
		ft820x_power_off(data);
		return error;
	}

	data->powered = true;
	return 0;
}

static int ft820x_start(struct ft820x_data *data)
{
	int error;

	error = ft820x_power_on(data);
	if (error)
		return error;

	WRITE_ONCE(data->running, true);
	enable_irq(data->spi->irq);

	return 0;
}

static void ft820x_stop(struct ft820x_data *data)
{
	int error;

	if (!data->running)
		return;

	WRITE_ONCE(data->running, false);
	disable_irq(data->spi->irq);

	error = ft820x_spi_write_u8(data, FT820X_REG_POWER_MODE,
				    FT820X_POWER_MODE_SLEEP);
	if (error)
		dev_warn(&data->spi->dev,
			 "failed to enter sleep mode: %d\n", error);

	ft820x_release_all(data);
	ft820x_power_off(data);
}

static int ft820x_input_open(struct input_dev *input)
{
	return ft820x_start(input_get_drvdata(input));
}

static void ft820x_input_close(struct input_dev *input)
{
	ft820x_stop(input_get_drvdata(input));
}

static int ft820x_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ft820x_data *data;
	int error;

	if (spi->irq <= 0)
		return dev_err_probe(dev, -EINVAL, "missing interrupt\n");

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->spi = spi;
	mutex_init(&data->bus_lock);
	spi_set_drvdata(spi, data);

	data->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(data->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(data->reset_gpio),
				     "failed to get reset GPIO\n");

	error = devm_add_action_or_reset(dev, ft820x_power_off, data);
	if (error)
		return error;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	error = spi_setup(spi);
	if (error)
		return dev_err_probe(dev, error, "failed to configure SPI\n");

	error = ft820x_power_on(data);
	if (error)
		return dev_err_probe(dev, error, "controller did not start\n");

	error = ft820x_input_init(data);
	if (error)
		return dev_err_probe(dev, error,
				     "failed to register input device\n");

	error = devm_request_threaded_irq(dev, spi->irq, NULL,
					  ft820x_irq_thread,
					   IRQF_ONESHOT | IRQF_NO_AUTOEN,
					   dev_name(dev), data);
	if (error)
		return dev_err_probe(dev, error, "failed to request IRQ\n");

	ft820x_power_off(data);

	error = input_register_device(data->input);
	if (error)
		return dev_err_probe(dev, error,
				     "failed to register input device\n");

	dev_info(dev, "FT8203 touchscreen initialized\n");
	return 0;
}

static int ft820x_suspend(struct device *dev)
{
	struct ft820x_data *data = dev_get_drvdata(dev);

	mutex_lock(&data->input->mutex);
	if (input_device_enabled(data->input))
		ft820x_stop(data);
	mutex_unlock(&data->input->mutex);

	return 0;
}

static int ft820x_resume(struct device *dev)
{
	struct ft820x_data *data = dev_get_drvdata(dev);
	int error;

	mutex_lock(&data->input->mutex);
	if (input_device_enabled(data->input))
		error = ft820x_start(data);
	else
		error = 0;
	mutex_unlock(&data->input->mutex);

	return error;
}

static DEFINE_SIMPLE_DEV_PM_OPS(ft820x_pm_ops, ft820x_suspend, ft820x_resume);

static const struct of_device_id ft820x_of_match[] = {
	{ .compatible = "focaltech,ft8203" },
	{ }
};
MODULE_DEVICE_TABLE(of, ft820x_of_match);

static struct spi_driver ft820x_driver = {
	.driver = {
		.name = "focaltech-ft820x",
		.of_match_table = ft820x_of_match,
		.pm = pm_sleep_ptr(&ft820x_pm_ops),
	},
	.probe = ft820x_probe,
};
module_spi_driver(ft820x_driver);

MODULE_AUTHOR("FocalTech Systems");
MODULE_DESCRIPTION("FocalTech FT820x touchscreen driver");
MODULE_LICENSE("GPL");
