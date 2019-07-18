/*
 * at24.c - handle most I2C EEPROMs
 *
 * Copyright (C) 2005-2007 David Brownell
 * Copyright (C) 2008 Wolfram Sang, Pengutronix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/log2.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/property.h>
#include <linux/i2c.h>
#include <linux/of_device.h>


/* 
 * struct ina260_data - ina260 device private data
 */
struct ina260_data {
	/* current */
	struct device_attribute current_attribute;
	/* voltage */
	struct device_attribute voltage_attribute;
	/* power */
	struct device_attribute power_attribute;
	
	/* read and write lock */
	struct mutex lock; 
};

/*
 * The device supported by the driver
 */
static const struct i2c_device_id ina260_ids[] = {
	{ "ina260", 0 },
	{ /* END OF LIST */ }
};
MODULE_DEVICE_TABLE(i2c, ina260_ids);



/*-------------------------------------------------------------------------*/
static ssize_t current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina260_data *ina260 = dev_get_drvdata(dev);
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* current */
	unsigned char i_reg = 0x01; // Bus Voltage Register
	unsigned short i_data = 0; // CONTENTS
	unsigned int current_i = 0;

	struct i2c_msg i_msgs[2] =
	{
		{.addr = client->addr, .len = sizeof(i_reg), .buf = &i_reg}, /* msgs[0] */
		{.addr = client->addr, .flags = I2C_M_RD, .len = sizeof(i_data), .buf = (void *)&i_data} /* msgs[1] */
	};

	mutex_lock(&ina260->lock);

	i2c_transfer(client->adapter, i_msgs, sizeof(i_msgs) / sizeof(struct i2c_msg));

	mutex_unlock(&ina260->lock);
	
	/* htons(v_data) * 1.25 / 1000.0 */
	current_i = htons(i_data) * 125;
	
   return sprintf(buf, "%u.%u\n", current_i / 100000, current_i % 100000);

}


static ssize_t voltage_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina260_data *ina260 = dev_get_drvdata(dev);
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	/* voltage */
	unsigned char v_reg = 0x02; // Bus Voltage Register
	unsigned short v_data = 0; // CONTENTS
	unsigned int voltage = 0;

	struct i2c_msg v_msgs[2] =
	{
		{.addr = client->addr, .len = sizeof(v_reg), .buf = &v_reg}, /* msgs[0] */
		{.addr = client->addr, .flags = I2C_M_RD, .len = sizeof(v_data), .buf = (void *)&v_data} /* msgs[1] */
	};

	mutex_lock(&ina260->lock);

	i2c_transfer(client->adapter, v_msgs, sizeof(v_msgs) / sizeof(struct i2c_msg));

	mutex_unlock(&ina260->lock);
	
	/* htons(v_data) * 1.25 / 1000.0 */
	voltage = htons(v_data) * 125;
	
   return sprintf(buf, "%u.%u\n", voltage / 100000, voltage % 100000);

}

//ssize_t voltage_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
//{
//
//   return count;
//}


static ssize_t power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ina260_data *ina260 = dev_get_drvdata(dev);
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	unsigned char p_reg = 0x03; // Power Register
	unsigned short p_data = 0; // CONTENTS
	unsigned int power = 0;

	struct i2c_msg p_msgs[2] =
	{
		{.addr = client->addr, .len = sizeof(p_reg), .buf = &p_reg}, /* msgs[0] */
		{.addr = client->addr, .flags = I2C_M_RD, .len = sizeof(p_data), .buf = (void *)&p_data} /* msgs[1] */
	};

	mutex_lock(&ina260->lock);

	i2c_transfer(client->adapter, p_msgs, sizeof(p_msgs) / sizeof(struct i2c_msg));

	mutex_unlock(&ina260->lock);

	/* htons(p_data) * 10.0 / 1000.0 */
	power = htons(p_data) * 1000;
		
	return sprintf(buf, "%u.%u\n", power / 100000, power % 100000);
}



static int ina260_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ina260_data *ina260;

	dev_info(&client->dev, "Device Tree Probing\n");		

	ina260 = devm_kmalloc(&client->dev, sizeof(struct ina260_data), GFP_KERNEL | __GFP_ZERO);
	if (!ina260) {
		dev_err(&client->dev, "Cound not allocate ina260 device\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&client->dev, ina260);

	mutex_init(&ina260->lock);

	/* crate current attribute file */
	ina260->current_attribute.attr.name = "total_current";
	ina260->current_attribute.attr.mode = 0444;
	ina260->current_attribute.show = current_show;
	device_create_file(&client->dev, &ina260->current_attribute);

	/* crate voltage attribute file */
	ina260->voltage_attribute.attr.name = "total_voltage";
	ina260->voltage_attribute.attr.mode = 0444;
	ina260->voltage_attribute.show = voltage_show;
	//ina260->voltage_attribute.store = NULL;
	device_create_file(&client->dev, &ina260->voltage_attribute);

	/* crate power attribute file */
	ina260->power_attribute.attr.name = "total_power";
	ina260->power_attribute.attr.mode = 0444;
	ina260->power_attribute.show = power_show;
	device_create_file(&client->dev, &ina260->power_attribute);
	
	return 0;
}

static int ina260_remove(struct i2c_client *client)
{
	struct ina260_data *ina260 = dev_get_drvdata(&client->dev);

	/* remove current attribute file */
	device_remove_file(&client->dev, &ina260->current_attribute);	

	/* remove voltage attribute file */
	device_remove_file(&client->dev, &ina260->voltage_attribute);	

	/* remove power attribute file */
	device_remove_file(&client->dev, &ina260->power_attribute);	

	dev_set_drvdata(&client->dev, NULL);

	return 0;
}

//#define OF_MATCH_TABLE

#ifdef OF_MATCH_TABLE
static struct of_device_id ina260_of_match[] = {
	{ .compatible = "ti,ina260", },
	{ /* end of list */ },
}; 
#endif

static struct i2c_driver ina260_driver = {
	.driver = {
		.name = "ina260",
		.owner = THIS_MODULE,
#ifdef OF_MATCH_TABLE
		.of_match_table	= ina260_of_match,
#endif
	},
	.probe = ina260_probe,
	.remove = ina260_remove,
	.id_table = ina260_ids,
};

static int __init ina260_init(void)
{
	return i2c_add_driver(&ina260_driver);
}
module_init(ina260_init);

static void __exit ina260_exit(void)
{
	i2c_del_driver(&ina260_driver);
}
module_exit(ina260_exit);

MODULE_DESCRIPTION("Driver for ina260");
MODULE_AUTHOR("Dingwave Inc");
MODULE_LICENSE("GPL");
