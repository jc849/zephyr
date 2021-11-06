/*
 * Copyright (c) 2021 Aspeed Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/flash.h>
#include <kernel.h>
#include <sys/util.h>

#include <stdlib.h>
#include <string.h>
#include <zephyr.h>

#define LOG_MODULE_NAME spih_read_demo

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define SPIM_TEST_SIZE 0x1000
#define SPIM_TEST_ARR_SIZE 0x1100

static uint8_t __aligned(4) test_arr[SPIM_TEST_ARR_SIZE];

static char *flash_devices[6] = {
	"spi1_cs0",
	"spi1_cs1",
	"spi1_cs2",
	"spi1_cs3"
};

static void spim_dump_buf(uint8_t *buf, uint32_t len)
{
	uint32_t i;

	for (i = 0; i < len; i++) {
		printk("%02x ", buf[i]);
		if (i % 16 == 15)
			printk("\n");
	}
	printk("\n");
}

static int do_erase_write_verify(const struct device *flash_device,
			uint32_t op_addr, uint8_t *write_buf, uint8_t *read_back_buf,
			uint32_t erase_sz)
{
	uint32_t ret = 0;

	ret = flash_erase(flash_device, op_addr, erase_sz);
	if (ret != 0) {
		LOG_ERR("[%s][%d] erase failed at %d.",
			__func__, __LINE__, op_addr);
		goto end;
	}

	ret = flash_write(flash_device, op_addr, write_buf, erase_sz);
	if (ret != 0) {
		LOG_ERR("[%s][%d] write failed at %d.",
			__func__, __LINE__, op_addr);
		goto end;
	}

	flash_read(flash_device, op_addr, read_back_buf, erase_sz);
	if (ret != 0) {
		LOG_ERR("[%s][%d] write failed at %d.",
			__func__, __LINE__, op_addr);
		goto end;
	}

	if (memcmp(write_buf, read_back_buf, erase_sz) != 0) {
		ret = -EINVAL;
		LOG_ERR("ERROR: %s %d fail to write flash at 0x%x",
				__func__, __LINE__, op_addr);
		printk("to be written:\n");
		spim_dump_buf(write_buf, 256);

		printk("readback:\n");
		spim_dump_buf(read_back_buf, 256);

		goto end;
	}

end:
	return ret;
}

static int do_update(const struct device *flash_device,
				off_t offset, uint8_t *buf, size_t len)
{
	int ret = 0;
	uint32_t flash_sz = flash_get_flash_size(flash_device);
	uint32_t sector_sz = flash_get_write_block_size(flash_device);
	uint32_t flash_offset = (uint32_t)offset;
	uint32_t remain, op_addr = 0, end_sector_addr;
	uint8_t *update_ptr = buf, *op_buf = NULL, *read_back_buf = NULL;
	bool update_it = false;

	LOG_INF("Udpating %s...", flash_device->name);

	if (flash_sz < flash_offset + len) {
		LOG_ERR("ERROR: update boundary exceeds flash size. (%d, %d, %d)",
			flash_sz, flash_offset, len);
		ret = -EINVAL;
		goto end;
	}

	op_buf = (uint8_t *)malloc(sector_sz);
	if (op_buf == NULL) {
		LOG_ERR("heap full %d %d", __LINE__, sector_sz);
		ret = -EINVAL;
		goto end;
	}

	read_back_buf = (uint8_t *)malloc(sector_sz);
	if (read_back_buf == NULL) {
		LOG_ERR("heap full %d %d", __LINE__, sector_sz);
		ret = -EINVAL;
		goto end;
	}

	/* initial op_addr */
	op_addr = (flash_offset / sector_sz) * sector_sz;

	/* handle the start part which is not multiple of sector size */
	if (flash_offset % sector_sz != 0) {
		ret = flash_read(flash_device, op_addr, op_buf, sector_sz);
		if (ret != 0)
			goto end;

		remain = MIN(sector_sz - (flash_offset % sector_sz), len);
		memcpy((uint8_t *)op_buf + (flash_offset % sector_sz), update_ptr, remain);
		ret = do_erase_write_verify(flash_device, op_addr, op_buf,
								read_back_buf, sector_sz);
		if (ret != 0)
			goto end;

		op_addr += sector_sz;
		update_ptr += remain;
	}

	end_sector_addr = (flash_offset + len) / sector_sz * sector_sz;
	/* handle body */
	for (; op_addr < end_sector_addr;) {
		ret = flash_read(flash_device, op_addr, op_buf, sector_sz);
		if (ret != 0)
			goto end;

		if (memcmp(op_buf, update_ptr, sector_sz) != 0)
			update_it = true;

		if (update_it) {
			ret = do_erase_write_verify(flash_device, op_addr, update_ptr,
								read_back_buf, sector_sz);
			if (ret != 0)
				goto end;
		}

		op_addr += sector_sz;
		update_ptr += sector_sz;
	}

	/* handle remain part */
	if (end_sector_addr < flash_offset + len) {

		ret = flash_read(flash_device, op_addr, op_buf, sector_sz);
		if (ret != 0)
			goto end;

		remain = flash_offset + len - end_sector_addr;
		memcpy((uint8_t *)op_buf, update_ptr, remain);

		ret = do_erase_write_verify(flash_device, op_addr, op_buf,
								read_back_buf, sector_sz);
		if (ret != 0)
			goto end;

		op_addr += remain;
	}

end:
	LOG_INF("Update %s.", ret ? "FAILED" : "done");

	if (op_buf != NULL)
		free(op_buf);
	if (read_back_buf != NULL)
		free(read_back_buf);

	return ret;
}

int test_spi_host_read(void)
{
	int ret = 0;
	const struct device *flash_dev;
	uint8_t *op_buf = NULL;
	uint32_t i;

	op_buf = (uint8_t *)malloc(SPIM_TEST_SIZE);
	if (op_buf == NULL) {
		LOG_ERR("heap full %d %d", __LINE__, SPIM_TEST_SIZE);
		ret = -EINVAL;
		goto end;
	}

	for (i = 0; i < SPIM_TEST_ARR_SIZE; i++)
		test_arr[i] = 'a' + (i % 26);

	/* Don't modify flash of spi1_cs0
	 * since it is FMC_CS0 flash of the host.
	 */
	for (i = 1; i < 4; i++) {
		flash_dev = device_get_binding(flash_devices[i]);
		if (!flash_dev) {
			LOG_ERR("No device named %s.", flash_devices[i]);
			return -ENOEXEC;
		}

		ret = flash_read(flash_dev, 0x0, op_buf, SPIM_TEST_SIZE);
		if (ret) {
			LOG_ERR("fail to read flash %s", flash_devices[i]);
			goto end;
		}

		if (memcmp(op_buf, test_arr + (i * 4), SPIM_TEST_SIZE) != 0) {
			ret = do_update(flash_dev, 0x0,
					test_arr + (i * 4), SPIM_TEST_SIZE);
			if (ret != 0) {
				LOG_ERR("fail to update flash %s", flash_devices[i]);
				goto end;
			}
		}
	}

	for (i = 0; i < 4; i++) {
		flash_dev = device_get_binding(flash_devices[i]);
		if (!flash_dev) {
			LOG_ERR("No device named %s.", flash_devices[i]);
			return -ENOEXEC;
		}

		ret = flash_read(flash_dev, 0x0, op_buf, SPIM_TEST_SIZE);
		if (ret) {
			LOG_ERR("fail to read flash %s", flash_devices[i]);
			goto end;
		}

		printk("[%s]read result:\n", flash_devices[i]);
		spim_dump_buf(op_buf, 4);
		spim_dump_buf(op_buf + SPIM_TEST_SIZE - 4, 4);
	}

end:
	return ret;
}
