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
#define TEST_IBI_PAYLOAD_SIZE 256
#define MAX_DATA_SIZE		256		/* < config->msg_size;*/

/* external reference */
extern int i3c_slave_mqueue_read(const struct device *dev, uint8_t *dest, int budget);
extern int i3c_slave_mqueue_write(const struct device *dev, uint8_t *src, int size);

#define TEST_I3C_SLAVE_THREAD_STACK_SIZE	1024
#define TEST_I3C_SLAVE_THREAD_PRIO		CONFIG_ZTEST_THREAD_PRIORITY

K_THREAD_STACK_DEFINE(test_i3c_slave_thread_stack_area, TEST_I3C_SLAVE_THREAD_STACK_SIZE);
static struct k_thread test_i3c_slave_thread;

static uint8_t test_data_tx_mst[MAX_DATA_SIZE];
static uint8_t test_data_rx_mst[MAX_DATA_SIZE];
static uint8_t test_data_tx_slv[MAX_DATA_SIZE];
static uint8_t test_data_rx_slv[MAX_DATA_SIZE];
static struct i3c_ibi_payload i3c_payload;
static struct k_sem ibi_complete;
static volatile uint8_t mdb;

static struct i3c_ibi_payload *test_ibi_write_requested(struct i3c_dev_desc *desc)
{
	/* reset rx buffer */
	/* memset(test_data_rx_mst, 0x00, sizeof(test_data_rx_mst)); */

	i3c_payload.buf = test_data_rx_mst;
	i3c_payload.size = 0;
	i3c_payload.max_payload_size = MAX_DATA_SIZE;

	return &i3c_payload;
}

static void test_ibi_write_done(struct i3c_dev_desc *desc)
{
	if (IS_MDB_PENDING_READ_NOTIFY(mdb)) {
		k_sem_give(&ibi_complete);
	}
}

static struct i3c_ibi_callbacks i3c_ibi_def_callbacks = {
	.write_requested = test_ibi_write_requested,
	.write_done = test_ibi_write_done,
};

static void prepare_test_data(uint8_t *data, int nbytes)
{
	uint32_t value;
	int i, shift;

	value = sys_rand32_get();
	for (i = 0; i < nbytes; i++) {
		shift = (i & 0x3) * 8;
		data[i] = (value >> shift) & 0xff;
		if ((i & 0x3) == 0x3) {
			value = sys_rand32_get();
		}
	}
}

static void test_i3c_slave_task(void *arg1, void *arg2, void *arg3)
{
	const struct device *slave_mq = (const struct device *)arg1;
	int ret = 0;

	int counter = 0; /* for debug only */

	for (;;) {
		/* Test part 1: read and compare the data */

		/* reset rx buffer */
		memset(test_data_rx_slv, 0x00, sizeof(test_data_rx_slv));

		while (1) {
			ret = i3c_slave_mqueue_read(slave_mq, (uint8_t *)test_data_rx_slv,
				TEST_PRIV_XFER_SIZE);
			if (ret) {
				if (memcmp(test_data_tx_mst, test_data_rx_slv, TEST_PRIV_XFER_SIZE)
					== 0) {
					break;
				}
				LOG_WRN("$");
			}

			k_sleep(K_USEC(1));
		}

		ast_zassert_mem_equal(test_data_tx_mst, test_data_rx_slv, TEST_PRIV_XFER_SIZE,
			"i3c private write test fail: data mismatch %d %X %X", counter,
			test_data_tx_mst[0], test_data_rx_slv[0]);

		/* Test part 2: send IBI to notify the master device to get the pending data */
		/* prepare_test_data(test_data_tx_slv, TEST_IBI_PAYLOAD_SIZE); */

		/* for debug only */
		memcpy(test_data_tx_slv, test_data_rx_slv, TEST_IBI_PAYLOAD_SIZE);
		ret = i3c_slave_mqueue_write(slave_mq, test_data_tx_slv, TEST_IBI_PAYLOAD_SIZE);
		ast_zassert_equal(ret, 0, "failed to do slave mqueue write");
		counter++;
	}
}

static void test_i3c_ci(int count)
{
	const struct device *dev_master, *dev_slave, *slave_mq;
	struct i3c_dev_desc slave_i3c;
	struct i3c_dev_desc *slave;
	struct i3c_priv_xfer xfer[2];
	int ret, i;

	dev_master = device_get_binding(DT_LABEL(DT_NODELABEL(i3c0)));
	dev_slave = device_get_binding(DT_LABEL(DT_NODELABEL(i3c1)));
	slave_mq = device_get_binding(DT_LABEL(DT_NODELABEL(i3c1_smq)));

	/* Not ready, do not use */
	if (!device_is_ready(dev_master) || !device_is_ready(dev_slave) ||
		!device_is_ready(slave_mq)) {
		return;
	}

	/* prepare slave device info for attach */
	slave = &slave_i3c;
	slave->info.static_addr = DT_PROP(DT_BUS(DT_NODELABEL(i3c1_smq)), assigned_address);
	slave->info.assigned_dynamic_addr = slave->info.static_addr;
	slave->info.pid = 0x063212341567;
	slave->info.i2c_mode = 1;	/* default run i2c mode */

	/* example to attach lsm6dso */
	/*
	 * #define LSM6DSO_ADDR    0x6B
	 *
	 * slave->info.static_addr = LSM6DSO_ADDR;
	 * slave->info.assigned_dynamic_addr = LSM6DSO_ADDR;
	 * slave->info.i2c_mode = 1;
	 */

	if (slave == NULL)
		return;

	i3c_master_attach_device(dev_master, slave);

	/* try to enter i3c mode */
	/* assign dynamic address with setdasa or entdaa, doesn't support setaasa in slave mode */
	i3c_master_send_rstdaa(dev_master);
	i3c_master_send_aasa(dev_master); /* Compatibility test for JESD403 */
	i3c_master_send_entdaa(slave);
	slave->info.i2c_mode = 0;

	/* must wait for a while, for slave finish sir_allowed_worker ? */
	/* k_sleep(K_USEC(1)); */

	/* try to collect more slave info if called setaasa in the former */
	i3c_master_send_getpid(dev_master, slave->info.dynamic_addr, &slave->info.pid);
	i3c_master_send_getbcr(dev_master, slave->info.dynamic_addr, &slave->info.bcr);

	/* setup callback function to receive mdb */
	ret = i3c_master_request_ibi(slave, &i3c_ibi_def_callbacks);
	ast_zassert_equal(ret, 0, "failed to request sir");

	/* sent enec to slave to enable ibi feature */
	ret = i3c_master_enable_ibi(slave);
	ast_zassert_equal(ret, 0, "failed to enable sir");

	/* get request from message queue and write back response message with ibi */
	k_thread_create(&test_i3c_slave_thread, test_i3c_slave_thread_stack_area,
		TEST_I3C_SLAVE_THREAD_STACK_SIZE, test_i3c_slave_task, (void *)slave_mq,
		NULL, NULL, TEST_I3C_SLAVE_THREAD_PRIO, 0, K_NO_WAIT);

	mdb = DT_PROP(DT_NODELABEL(i3c1_smq), mandatory_data_byte);

	/* create semaphore to synchronize master and slave between ibi */
	if (IS_MDB_PENDING_READ_NOTIFY(mdb)) {
		k_sem_init(&ibi_complete, 0, 1);
	}

	for (i = 0; i < count; i++) {
		/* prepare request message */
		prepare_test_data(test_data_tx_mst, TEST_PRIV_XFER_SIZE);
		/* test_data_tx_mst[0] = i; */ /* for debug only */
		/* k_usleep(100); */ /* debug only */

		/* Requester send request message */
		xfer[0].rnw = 0;
		xfer[0].len = TEST_PRIV_XFER_SIZE;
		xfer[0].data.out = test_data_tx_mst;
		ret = i3c_master_priv_xfer(slave, xfer, 1);

		/*
		 * if MDB group ID is pending read notification:
		 *    master <--- IBI notification --------- slave
		 *    master ---- private read transfer ---> slave
		 * else:
		 *    master <--- IBI with data --------- slave
		 */

		if (IS_MDB_PENDING_READ_NOTIFY(mdb)) {
			/* master waits IBI from the slave */
			k_sem_take(&ibi_complete, K_FOREVER);

			/* init the flag for the next loop */
			k_sem_init(&ibi_complete, 0, 1);

			/* check result */
			ast_zassert_equal(mdb, test_data_rx_mst[0],
				"IBI MDB mismatch: %02x %02x\n",
				ret, test_data_rx_mst[0]);
		}

		/* initiate a private read transfer to read the pending data */
		xfer[0].rnw = 1;
		xfer[0].len = TEST_IBI_PAYLOAD_SIZE;
		xfer[0].data.in = test_data_rx_mst;
		k_yield();
		ret = i3c_master_priv_xfer(slave, &xfer[0], 1);
		ast_zassert_mem_equal(test_data_tx_slv, test_data_rx_mst, TEST_IBI_PAYLOAD_SIZE,
			"data mismatch %d %X %X", i, test_data_tx_slv[0], test_data_rx_mst[0]);
		/*k_usleep(100);*/ /* debug only */
	}
}

int test_i3c(int count, enum aspeed_test_type type)
{
	printk("%s, count: %d, type: %d\n", __func__, count, type);

	if (type == AST_TEST_CI) {
		test_i3c_ci(count);
		return ast_ztest_result();
	}

	return AST_TEST_PASS;
}
