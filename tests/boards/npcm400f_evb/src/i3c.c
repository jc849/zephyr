/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <tc_util.h>
#include <kernel.h>
#include <device.h>
#include <drivers/i3c/i3c.h>
#include <random/rand32.h>
#include <soc.h>
#include "ast_test.h"

#define LOG_MODULE_NAME i3c_test

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);


#define TEST_PRIV_XFER_SIZE	256
#define TEST_IBI_PAYLOAD_SIZE	128
#define MAX_DATA_SIZE		256

static uint8_t test_data_tx[MAX_DATA_SIZE];
static uint8_t test_data_rx[MAX_DATA_SIZE];

/*#define TEST_LOOPBACK*/
#define TEST_WITH_LSM6DSO
#define TEST_DBG

static void test_i3c_ci(int count)
{
	const struct device *dev_master, *dev_slave;
	const struct device *dev = NULL;
	struct i3c_dev_desc *slave = NULL;
	struct i3c_priv_xfer xfer[2];
	int ret, i;

#if defined(TEST_LOOPBACK)
	struct i3c_dev_desc slave_i3c;

	slave = &slave_i3c;
#elif defined(TEST_WITH_LSM6DSO)
	struct i3c_dev_desc slave_lsm6dso;

	slave = &slave_lsm6dso;
#endif

	dev_master = device_get_binding(DT_LABEL(DT_NODELABEL(i3c0)));
	dev_slave = device_get_binding(DT_LABEL(DT_NODELABEL(i3c1)));

	/* Not ready, do not use */
#ifndef TEST_DBG
	if (!device_is_ready(dev_master) || !device_is_ready(dev_slave))
		return;
#else
	if (dev_master == NULL) {
		dev = DEVICE_DT_GET(DT_NODELABEL(i3c0));
		if (dev == NULL) {
			LOG_INF("I3C0 is not found in device tree !\n");
			return;
		}

		if (!device_is_ready(dev)) {
			if (dev->state->initialized == false) {
				LOG_INF("I3C0 is found in device tree, but not init !\n");
				return;
			}

			LOG_INF("I3C0 is initialized, but fail !\n");
			return;
		}
	}

	if (dev_slave == NULL) {
		dev = DEVICE_DT_GET(DT_NODELABEL(i3c1));
		if (dev == NULL) {
			LOG_INF("I3C1 is not found in device tree !\n");
			return;
		}

		if (!device_is_ready(dev)) {
			if (dev->state->initialized == false) {
				LOG_INF("I3C1 is found in device tree, but not initialized !\n");
				return;
			}

			LOG_INF("I3C1 is initialized, but fail !\n");
			return;
		}
	}
#endif

#if defined(TEST_LOOPBACK)
	slave->info.static_addr = DT_PROP(DT_NODELABEL(i3c1), assigned_address);
	slave->info.assigned_dynamic_addr = slave->info.static_addr;
	slave->info.i2c_mode = 1;	/* default run i2c mode */
#elif defined(TEST_WITH_LSM6DSO)
	/* attach lsm6dso */
	#define LSM6DSO_ADDR    0x6B
	slave->info.static_addr = LSM6DSO_ADDR;
	slave->info.assigned_dynamic_addr = LSM6DSO_ADDR;
	slave->info.i2c_mode = 1;
#endif

	if (slave == NULL)
		return;

	i3c_master_attach_device(dev_master, slave);
	i3c_master_send_rstdaa(dev_master);

	for (i = 0; i < count; i++) {
		/*
		 * Test part 1:
		 * master --- i2c private write transfer ---> slave
		 */
		xfer[0].rnw = 0;
		xfer[0].len = 1;
		xfer[0].data.out = test_data_tx;
		test_data_tx[0] = 0x0F;

		ret = i3c_master_priv_xfer(slave, xfer, 1);

		/*
		 * Test part 2:
		 * master --- i2c private read transfer ---> slave
		 */
		xfer[0].rnw = 1;
		xfer[0].len = 1;
		xfer[0].data.in = test_data_rx;

		ret = i3c_master_priv_xfer(slave, xfer, 1);
		__ASSERT(test_data_rx[0] == 0x6C, "Read Data Fail !!!\n\n");

		/*
		 * Test part 3: for those who support stopsplitread
		 * master --- i2c private write then read transfer ---> slave
		 */
		memset(test_data_rx, 0x00, sizeof(test_data_rx));

		i3c_i2c_read(slave, 0x0F, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0x6C, "Read Data Fail !!!\n\n");

		/*
		 * Test part 4:
		 * master --- i2c API ---> slave
		 */
		test_data_tx[0] = 0xC0;
		ret = i3c_i2c_write(slave, 0x01, test_data_tx, 1);
		ret = i3c_i2c_read(slave, 0x01, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0xC0, "Read Data Fail !!!\n\n");

		test_data_tx[0] = 0x00;
		ret = i3c_i2c_write(slave, 0x01, test_data_tx, 1);
		ret = i3c_i2c_read(slave, 0x01, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0x00, "Read Data Fail !!!\n\n");
	}

	i3c_master_send_entdaa(slave);
	/* i3c_master_send_aasa(master); */
	slave->info.i2c_mode = 0;

	i3c_master_send_getpid(dev_master, slave->info.dynamic_addr, &slave->info.pid);
	i3c_master_send_getbcr(dev_master, slave->info.dynamic_addr, &slave->info.bcr);

	for (i = 0; i < count; i++) {
		/*
		 * Test part 1:
		 * master --- i3c private write transfer ---> slave
		 */
		xfer[0].rnw = 0;
		xfer[0].len = 1;
		xfer[0].data.out = test_data_tx;
		test_data_tx[0] = 0x0F;
		ret = i3c_master_priv_xfer(slave, xfer, 1);

		/*
		 * Test part 2:
		 * master --- i3c private read transfer ---> slave
		 */
		xfer[0].rnw = 1;
		xfer[0].len = 1;
		xfer[0].data.in = test_data_rx;

		ret = i3c_master_priv_xfer(slave, xfer, 1);
		__ASSERT(test_data_rx[0] == 0x6C, "Read Data Fail !!!\n\n");

		/*
		 * Test part 3: for those who support stopsplitread
		 * master --- i3c private write then read transfer ---> slave
		 */
		memset(test_data_rx, 0x00, sizeof(test_data_rx));

		test_data_tx[0] = 0x0F;	/* WHO_AM_I, lsm6dso */
		ret = i3c_jesd403_read(slave, test_data_tx, 1, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0x6C, "Read Data Fail !!!\n\n");

		/*
		 * Test part 4:
		 * master --- i3c API ---> slave
		 */
		memset(test_data_rx, 0x00, sizeof(test_data_rx));
		test_data_tx[0] = 0x01;
		test_data_tx[1] = 0xC0;
		ret = i3c_jesd403_write(slave, test_data_tx, 1, &test_data_tx[1], 1);
		ret = i3c_jesd403_read(slave, test_data_tx, 1, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0xC0, "Read Data Fail !!!\n\n");

		test_data_tx[1] = 0x00;
		ret = i3c_jesd403_write(slave, test_data_tx, 1, &test_data_tx[1], 1);
		ret = i3c_jesd403_read(slave, test_data_tx, 1, test_data_rx, 1);
		__ASSERT(test_data_rx[0] == 0x00, "Read Data Fail !!!\n\n");

		/*
		 * Test part 2:
		 * if MDB group ID is pending read notification:
		 *    master <--- IBI notification --------- slave
		 *    master ---- private read transfer ---> slave
		 * else:
		 *    master <--- IBI with data --------- slave
		 */
	}
}

int test_i3c(int count, enum aspeed_test_type type)
{
	printk("%s, count: %d, type: %d\n", __func__, count, type);

	if (type == AST_TEST_CI) {
		test_i3c_ci(count);
		return ast_ztest_result();
	}

	/* Not support FT yet */
	return AST_TEST_PASS;
}
