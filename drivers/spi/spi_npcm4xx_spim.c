/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_spi_spim

#include <drivers/clock_control.h>
#include <drivers/spi.h>
#include <logging/log.h>
#include <soc.h>
#include <header.h>
#include <kernel.h>

LOG_MODULE_REGISTER(spi_npcm4xx_spim, LOG_LEVEL_ERR);

#include "spi_context.h"

#ifdef CONFIG_XIP
#include <linker/linker-defs.h>
#define RAMFUNC __ramfunc
#else
#define RAMFUNC
#endif

#ifdef CONFIG_XIP
extern struct FIRMWARE_HEDAER_TYPE fw_header;

/* record fw update device and spi config */
const struct device *npcm4xx_fw_update_device = NULL;
const struct spi_config *npcm4xx_fw_update_spi_cfg = NULL;

/* fw update data */
bool npcm4xx_fw_update = false;
uint32_t npcm4xx_fw_update_start = 0;
uint32_t npcm4xx_fw_update_size = 0;
struct npcm4xx_fw_write_bitmap fw_write_bitmap;
uint8_t *npcm4xx_update_buffer = NULL;
#endif

/* Device config */
struct npcm4xx_spi_spim_config {
	/* flash interface unit base address */
	mm_reg_t base;
	/* clock configuration */
	struct npcm4xx_clk_cfg clk_cfg;
	/* direct access memory for spim */
	mm_reg_t spi_mmap_base;
};

/* Device data */
struct npcm4xx_spi_spim_data {
	struct spi_context ctx;
	/* read/write init status */
	int rw_init;
	/* read command data */
	struct spi_nor_op_info read_op_info;
	/* write command data */
	struct spi_nor_op_info write_op_info;
};

enum npcm4xx_spim_spi_nor_type {
	SPIM_FW_SPI_NOR,
	SPIM_MAX_SPI_NOR
};
/* Driver convenience defines */
#define HAL_INSTANCE(dev)                                                                          \
	((struct spim_reg *)((const struct npcm4xx_spi_spim_config *)(dev)->config)->base)

RAMFUNC static void spi_npcm4xx_spim_cs_level(const struct device *dev,
						const struct spi_config *spi_cfg,
						int level)
{
	struct spim_reg *const inst = HAL_INSTANCE(dev);

	/* Set chip select to high/low level */
	if (level == 0) {
		inst->SPIM_CTL1 &= ~BIT(NPCM4XX_SPIM_CTL1_SS);
	} else {
		inst->SPIM_CTL1 |= BIT(NPCM4XX_SPIM_CTL1_SS);
	}
}

RAMFUNC static void spi_npcm4xx_spim_exec_cmd(const struct device *dev,
						uint8_t code,
						bool write)
{
	struct spim_reg *const inst = HAL_INSTANCE(dev);

	uint32_t ctrl_value = 0;

#ifndef CONFIG_XIP
#ifdef CONFIG_ASSERT
	struct npcm4xx_spi_spim_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	/* Flash mutex must be held while executing UMA commands */
	__ASSERT((k_sem_count_get(&ctx->lock) == 0), "UMA is not locked");
#endif
#endif

	/* normal io mode */
	SET_FIELD(ctrl_value, NPCM4XX_SPIM_CTL0_OPMODE,
				NPCM4XX_SPIM_CTL0_OPMODE_NORMAL_IO);
	/* bit mode to standard */
	SET_FIELD(ctrl_value, NPCM4XX_SPIM_CTL0_BITMODE,
				NPCM4XX_SPIM_CTL0_BITMODE_STANDARD);
	/* data transmit width 8 bits */
	SET_FIELD(ctrl_value, NPCM4XX_SPIM_CTL0_DWIDTH,
				NPCM4XX_SPIM_CTL0_DWIDTH_8);
	/* transaction one byte in one transfer */
	SET_FIELD(ctrl_value, NPCM4XX_SPIM_CTL0_BURSTNUM,
				NPCM4XX_SPIM_CTL0_BURSTNUM_1);

	/* disable cipher */
	ctrl_value |= BIT(NPCM4XX_SPIM_CTL0_CIPHOFF);

	if (write) {
		/* output direction */
		ctrl_value |= BIT(NPCM4XX_SPIM_CTL0_QDIODIR);
		inst->SPIM_TX0 = code;
	}

	/* update control register */
	inst->SPIM_CTL0 = ctrl_value;

	/* execute transaction */
	inst->SPIM_CTL1 |= BIT(NPCM4XX_SPIM_CTL1_SPIMEN);

	while (inst->SPIM_CTL1 & BIT(NPCM4XX_SPIM_CTL1_SPIMEN))
		continue;

	/* write 1 to clear interupt flag */
	inst->SPIM_CTL0 |= BIT(NPCM4XX_SPIM_CTL0_IF);
}

RAMFUNC static int spi_npcm4xx_spim_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					const struct spi_buf_set *tx_bufs,
					const struct spi_buf_set *rx_bufs)
{
	struct npcm4xx_spi_spim_data *data = dev->data;
	struct spim_reg *const inst = HAL_INSTANCE(dev);
	struct spi_context *ctx = &data->ctx;
	size_t cur_xfer_len = 0;
	uint32_t backup_ctrl0 = 0;
#ifdef CONFIG_XIP
	unsigned int key = 0;
	uintptr_t vtor;
#else
	int error = 0;
#endif

#ifdef CONFIG_XIP
	/* if execute in the place, disable IRQ to avoid interrupt and context
	 * switch, to make sure always execute code in the RAM(SRAM) when use
	 * spim driver. npcm4xx only have one core, we don't need use lock to
	 * handle race condition.
	 */
	key = irq_lock();
	vtor = npcm4xx_vector_table_save();
#else
	spi_context_lock(ctx, false, NULL, spi_cfg);
	ctx->config = spi_cfg;
#endif

	/* store ctrl0 setting */
	backup_ctrl0 = inst->SPIM_CTL0;

	/* assert chip assert */
	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 0);

	spi_context_buffers_setup(ctx, tx_bufs, rx_bufs, 1);

	if (rx_bufs == NULL) {
		/* write data to SPI */
		while (spi_context_tx_buf_on(ctx)) {
			spi_npcm4xx_spim_exec_cmd(dev, *ctx->tx_buf, true);
			spi_context_update_tx(ctx, 1, 1);
		}
	} else {
		/* write data to SPI */
		cur_xfer_len = spi_context_total_tx_len(ctx);

		for (size_t i = 0; i < cur_xfer_len; i++) {
			spi_npcm4xx_spim_exec_cmd(dev, *ctx->tx_buf, true);
			spi_context_update_tx(ctx, 1, 1);
		}

		/* read data from SPI */
		while (spi_context_rx_buf_on(ctx)) {
			spi_npcm4xx_spim_exec_cmd(dev, 0x0, false);
			*ctx->rx_buf = (uint8_t) inst->SPIM_RX0;
			spi_context_update_rx(ctx, 1, 1);
		}
	}

	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 1);

	/* restore ctrl0 setting */
	inst->SPIM_CTL0 = backup_ctrl0;

#ifdef CONFIG_XIP
	npcm4xx_vector_table_restore(vtor);
	irq_unlock(key);
	return 0;
#else
	spi_context_release(ctx, error);
	return error;
#endif
}

static inline bool spi_nor_npcm4xx_spim_is_erase_command(uint8_t opcode)
{
	switch (opcode) {
		case SPI_NOR_CMD_SE:
		case SPI_NOR_CMD_SE_4B:
		case SPI_NOR_CMD_BE:
		case SPI_NOR_CMD_BE_4B:
		case SPI_NOR_CMD_BE_32K:
		case SPI_NOR_CMD_BE_32K_4B:
			return true;
		default:
			break;
	}
	return false;
}

/* must be call when disable irq */
RAMFUNC static void spi_nor_npcm4xx_spim_wait_ready(const struct device *dev, const struct spi_config *spi_cfg)
{
	struct spim_reg *const inst = HAL_INSTANCE(dev);
	uint32_t backup_ctrl0 = 0;
	uint8_t reg = 0xff, rdsr_cmd = SPI_NOR_CMD_RDSR;

	backup_ctrl0 = inst->SPIM_CTL0;
wait_again:
	/* assert chip assert */
	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 0);

	/* send command */
	spi_npcm4xx_spim_exec_cmd(dev, rdsr_cmd, true);

	/* receive data from spi flash to RX buffer */
	spi_npcm4xx_spim_exec_cmd(dev, 0x0, false);

	/* get data from RX buffer */
	reg = (uint8_t) inst->SPIM_RX0;

	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 1);

	/* check WIP(BUSY) bit */
	if (reg & SPI_NOR_WIP_BIT)
		goto wait_again;

	inst->SPIM_CTL0 = backup_ctrl0;
}

RAMFUNC static void spi_nor_npcm4xx_spim_normal_transceive(const struct device *dev,
						const struct spi_config *spi_cfg,
						struct spi_nor_op_info op_info)
{
	struct spim_reg *const inst = HAL_INSTANCE(dev);
	struct spi_nor_op_info *normal_op_info = NULL;
	uint32_t backup_ctrl0 = 0, index = 0;
	uint8_t *buf_data = NULL;
	uint8_t sub_addr = 0;

	normal_op_info = &op_info;

	/* store ctrl0 setting */
	backup_ctrl0 = inst->SPIM_CTL0;

	/* assert chip assert */
	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 0);

	/* send command */
	spi_npcm4xx_spim_exec_cmd(dev, normal_op_info->opcode, true);

	/* send address */
	index = normal_op_info->addr_len;
	while (index) {
		index = index - 1;
		sub_addr = (normal_op_info->addr >> (8 * index)) & 0xff;
		spi_npcm4xx_spim_exec_cmd(dev, sub_addr, true);
	}

	/* only support single mode dummy byte */
	if ((normal_op_info->dummy_cycle % NPCM4XX_SPIM_SINGLE_DUMMY_BYTE) != 0)
		return;

	/* send dummy bytes */
	for (index = 0; index < normal_op_info->dummy_cycle;
				index += NPCM4XX_SPIM_SINGLE_DUMMY_BYTE) {
		spi_npcm4xx_spim_exec_cmd(dev, 0x0, true);
	}

	buf_data = normal_op_info->buf;

	if (buf_data == NULL)
		goto spi_nor_normal_done;

	/* read data from SPI flash */
	if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_IN) {
		for (index = 0; index < normal_op_info->data_len; index++) {
			spi_npcm4xx_spim_exec_cmd(dev, 0x0, false);
			/* Get read transaction results */
			*(buf_data + index) = (uint8_t) inst->SPIM_RX0;
		}
	} else if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_OUT) {
		for (index = 0; index < normal_op_info->data_len; index++) {
			/* write data to SPI flash */
			spi_npcm4xx_spim_exec_cmd(dev, *(buf_data + index), true);
		}
	}

spi_nor_normal_done:

	spi_npcm4xx_spim_cs_level(dev, spi_cfg, 1);

	/* restore ctrl0 setting */
	inst->SPIM_CTL0 = backup_ctrl0;

#ifdef CONFIG_XIP
	/* when execute program, earse, chip erase and write statuis commands,
	 * SPI flash will ignore any commands in the BUSY state, if change to
	 * XIP flow, core cannot fetch execution code from SPI flash.
	 * Waiting SPI flash become READY status (WIP clear) in RAM(SRAM) code,
	 * and set invalid bit to clear cache for SPIM HW after become READY.
	 */
	if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_OUT) {
		/* wait WIP(BUSY) clear */
		spi_nor_npcm4xx_spim_wait_ready(dev, spi_cfg);
		/* invalid cache */
		inst->SPIM_CTL1 |= BIT(NPCM4XX_SPIM_CTL1_CDINVAL);
		/* wait hw clear invalid bit */
		while(inst->SPIM_CTL1 & BIT(NPCM4XX_SPIM_CTL1_CDINVAL));
	}
#endif
}

RAMFUNC static int spi_nor_npcm4xx_spim_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_spim_data *data = dev->data;
#ifdef CONFIG_XIP
	unsigned int key = 0;
	uintptr_t vtor;
	uint32_t flash_used = 0;
#else
	struct spi_context *ctx = &data->ctx;
	int error = 0;
#endif

#ifdef CONFIG_XIP
	/* not allow erase all */
	if (op_info.opcode == SPI_NOR_CMD_CE) {
		return -EINVAL;
	}

	/* not allow write execute code space */
	if (op_info.opcode == data->write_op_info.opcode ||
			spi_nor_npcm4xx_spim_is_erase_command(op_info.opcode)) {
		flash_used = ROUND_UP((uint32_t) _flash_used, SPI_NOR_SECTOR_SIZE);
		if (op_info.addr < flash_used) {
			return -EINVAL;
		}
	}

	/* if execute in the place, disable IRQ to avoid interrupt and context
	 * switch, to make sure always execute code in the RAM(SRAM) when use
	 * spim driver. npcm4xx only have one core, we don't need use lock to
	 * handle race condition.
	 */
	key = irq_lock();
	vtor = npcm4xx_vector_table_save();
#else
	spi_context_lock(ctx, false, NULL, spi_cfg);
	ctx->config = spi_cfg;
#endif
	/* normal io mode */
	spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, op_info);

#ifdef CONFIG_XIP
	npcm4xx_vector_table_restore(vtor);
	irq_unlock(key);
	return 0;
#else
	spi_context_release(ctx, error);
	return error;
#endif
}

RAMFUNC static int spi_nor_npcm4xx_spim_read_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_spim_data *data = dev->data;

	/* record read command from jesd216 */
	memcpy(&data->read_op_info, &op_info, sizeof(op_info));

	data->rw_init |= NPCM4XX_SPIM_SPI_NOR_READ_INIT_OK;

	return 0;
}

RAMFUNC static int spi_nor_npcm4xx_spim_write_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_spim_data *data = dev->data;
#ifdef CONFIG_XIP
	enum npcm4xx_spim_spi_nor_type spi_nor_type = spi_cfg->slave;
#endif

	/* record read command from jesd216 */
	memcpy(&data->write_op_info, &op_info, sizeof(op_info));

#ifdef CONFIG_XIP
	/* accept update fw flow when write init */
	if (spi_nor_type == SPIM_FW_SPI_NOR) {
		npcm4xx_fw_update_device = dev;
		npcm4xx_fw_update_spi_cfg = spi_cfg;
		memset(&fw_write_bitmap, 0x0, sizeof(struct npcm4xx_fw_write_bitmap));
	}
#endif

	data->rw_init |= NPCM4XX_SPIM_SPI_NOR_WRITE_INIT_OK;

	return 0;
}

RAMFUNC int spi_npcm4xx_spim_release(const struct device *dev, const struct spi_config *config)
{
	struct npcm4xx_spi_spim_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	if (!spi_context_configured(ctx, config)) {
		return -EINVAL;
	}

#ifndef CONFIG_XIP
	spi_context_unlock_unconditionally(ctx);
#endif
	return 0;
}

#ifdef CONFIG_XIP
static uint8_t spi_nor_npcm4xx_spim_set_update_fw_address(uint32_t fw_img_start,
							uint32_t fw_img_size,
							struct npcm4xx_fw_write_bitmap *bitmap)
{
	uint32_t flash_used = 0;
	uint32_t flash_max_size = BITMAP_ARRAY_PER_SIZE * BITMAP_LIST_SIZE * SPI_NOR_SECTOR_SIZE;

	/* update device not exist */
	if (npcm4xx_fw_update_device == NULL || npcm4xx_fw_update_spi_cfg == NULL) {
		LOG_ERR("fw update device or cfg not exist");
		return -1;
	}

	/* fw start and size should be legal setting */
	if (fw_img_size == 0x0 || (!SPI_NOR_IS_SECTOR_ALIGNED(fw_img_start))) {
		LOG_ERR("setting invalid, addr=0x%x(should align 0x%x) size=0x%x",
					fw_img_start, SPI_NOR_SECTOR_SIZE, fw_img_size);
		return -1;
	}

	/* cannot set range overlap running firmware */
	flash_used = ROUND_UP((uint32_t) _flash_used, SPI_NOR_SECTOR_SIZE);
	if (fw_img_start < flash_used) {
		LOG_ERR("should not overlap running firmware, used = 0x%x", flash_used);
		return -1;
	}

	/* allocate buffer for update */
	if (npcm4xx_update_buffer == NULL) {
		npcm4xx_update_buffer = k_malloc(SPI_NOR_SECTOR_SIZE);
		if (!npcm4xx_update_buffer) {
			LOG_ERR("allocate buffer for update failed.");
			return -1;
		}
	}

	npcm4xx_fw_update_start = fw_img_start;
	npcm4xx_fw_update_size = fw_img_size;

	if (npcm4xx_fw_update_size > flash_max_size) {
		LOG_ERR("new firmware image over size.");
		return -1;
	}

	/* partial write or update all */
	if (bitmap)
		memcpy(&fw_write_bitmap, (void *)bitmap,
				sizeof(struct npcm4xx_fw_write_bitmap));
	else
		memset(&fw_write_bitmap, 0xff, sizeof(struct npcm4xx_fw_write_bitmap));

	npcm4xx_fw_update = true;

	return 0;
}

RAMFUNC static void spi_nor_npcm4xx_spim_write_enable(const struct device *dev,
						const struct spi_config *spi_cfg)
{
	struct spi_nor_op_info wren_op_info;

	wren_op_info.mode = JESD216_MODE_111;
	wren_op_info.opcode = SPI_NOR_CMD_WREN;
	wren_op_info.addr = 0;
	wren_op_info.addr_len = 0;
	wren_op_info.dummy_cycle = 0;
	wren_op_info.buf = NULL;
	wren_op_info.data_len = 0;
	wren_op_info.data_direct = SPI_NOR_DATA_DIRECT_OUT;

	spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, wren_op_info);
}

RAMFUNC static void spi_nor_npcm4xx_spim_invalid_cache(const struct device *dev,
						const struct spi_config *spi_cfg)
{
	struct spim_reg *const inst = HAL_INSTANCE(dev);

	inst->SPIM_CTL1 |= BIT(NPCM4XX_SPIM_CTL1_CDINVAL);
	/* wait hw clear invalid bit */
	while(inst->SPIM_CTL1 & BIT(NPCM4XX_SPIM_CTL1_CDINVAL));
}

RAMFUNC static void spi_nor_npcm4xx_spim_erase(const struct device *dev,
						const struct spi_config *spi_cfg,
						uint32_t address)
{
	struct spi_nor_op_info erase_op_info;

	/* suppose update internal flash fw only need 3 bytes address mode */
	erase_op_info.mode = JESD216_MODE_111;
	erase_op_info.opcode = SPI_NOR_CMD_SE;
	erase_op_info.addr = address;
	erase_op_info.addr_len = 3;
	erase_op_info.dummy_cycle = 0;
	erase_op_info.buf = NULL;
	erase_op_info.data_len = 0;
	erase_op_info.data_direct = SPI_NOR_DATA_DIRECT_OUT;

	spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, erase_op_info);
}

RAMFUNC void static spi_nor_npcm4xx_spim_fw_update(int type)
{
	const struct device *dev = NULL;
	const struct spi_config *spi_cfg = NULL;
	struct npcm4xx_spi_spim_data *data = NULL;
	struct spi_nor_op_info read_opinfo;
	struct spi_nor_op_info write_opinfo;
	uint32_t write_start_address = 0, read_start_address = 0;
	uint32_t index = 0, read_end_address = 0;
	uint32_t flash_used = 0;
	uint32_t *fw_bitmap = NULL, bitmap_index = 0;

	ARG_UNUSED(type);

	if (!npcm4xx_fw_update)
		return;

	if (!npcm4xx_update_buffer)
		return;

	dev = npcm4xx_fw_update_device;
	spi_cfg = npcm4xx_fw_update_spi_cfg;

	spi_nor_npcm4xx_spim_invalid_cache(dev, spi_cfg);

	/* must access before erase FW spi flash */
	flash_used = ROUND_UP((uint32_t) _flash_used, SPI_NOR_SECTOR_SIZE);

	/* init read/write opinfo */
	data = dev->data;
	memcpy(&read_opinfo, &data->read_op_info, sizeof(write_opinfo));
	memcpy(&write_opinfo, &data->write_op_info, sizeof(write_opinfo));

	read_opinfo.addr = npcm4xx_fw_update_start;
	read_opinfo.data_len = sizeof(fw_header.hImageTag);
	read_opinfo.buf = (void *)npcm4xx_update_buffer;
	spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, read_opinfo);

	if (memcmp((void *)npcm4xx_update_buffer, (void *)fw_header.hImageTag,
				sizeof(fw_header.hImageTag)))
		return;

	read_start_address = npcm4xx_fw_update_start;

	read_end_address = npcm4xx_fw_update_start + npcm4xx_fw_update_size;
	read_end_address = ROUND_UP(read_end_address, SPI_NOR_SECTOR_SIZE);

	/* initial bitmap index */
	fw_bitmap = &fw_write_bitmap.bitmap_lists[0];
	bitmap_index = 0;

	while (read_start_address != read_end_address) {
		/* check bit set or not */
		if (!(*fw_bitmap & BIT(bitmap_index))) {
			goto fw_update_next_sector;
		}

		/* erase flash */
		spi_nor_npcm4xx_spim_write_enable(dev, spi_cfg);
		spi_nor_npcm4xx_spim_erase(dev, spi_cfg, write_start_address);

		/* read data */
		read_opinfo.addr = read_start_address;
		read_opinfo.data_len = SPI_NOR_SECTOR_SIZE;
		read_opinfo.buf = (void *)npcm4xx_update_buffer;
		spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, read_opinfo);

		/* write data */
		for (index = 0; index < SPI_NOR_SECTOR_SIZE; index += SPI_NOR_PAGE_SIZE) {
			/* write enable */
			spi_nor_npcm4xx_spim_write_enable(dev, spi_cfg);
			/* write data per SPI_NOR_PAGE_SIZE */
			write_opinfo.addr = write_start_address + index;
			write_opinfo.data_len = SPI_NOR_PAGE_SIZE;
			write_opinfo.buf = (void *)(npcm4xx_update_buffer + index);
			spi_nor_npcm4xx_spim_normal_transceive(dev, spi_cfg, write_opinfo);
		}

fw_update_next_sector:
		read_start_address = read_start_address + SPI_NOR_SECTOR_SIZE;
		write_start_address = write_start_address + SPI_NOR_SECTOR_SIZE;
		bitmap_index++;

		/* shift to next fw_bitmap */
		if (bitmap_index == BITMAP_ARRAY_PER_SIZE) {
			fw_bitmap++;
			bitmap_index = 0;
		}
	}

#if 0
	/* erase useless execute code if exist */
	while(write_start_address < flash_used) {
		/* erase flash */
		spi_nor_npcm4xx_spim_write_enable(dev, spi_cfg);
		spi_nor_npcm4xx_spim_erase(dev, spi_cfg, write_start_address);
		write_start_address = write_start_address + SPI_NOR_SECTOR_SIZE;
	}
#endif

	return;
}
#endif

RAMFUNC static int spi_npcm4xx_spim_init(const struct device *dev)
{
	const struct npcm4xx_spi_spim_config *const config = dev->config;
	const struct device *const clk_dev =
					device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	int ret;

	if (!device_is_ready(clk_dev)) {
		LOG_ERR("%s device not ready", clk_dev->name);
		return -ENODEV;
	}

	/* Turn on device clock first and get source clock freq. */
	ret = clock_control_on(clk_dev,
				(clock_control_subsys_t)&config->clk_cfg);
	if (ret < 0) {
		LOG_ERR("Turn on FIU clock fail %d", ret);
		return ret;
	}

#ifndef CONFIG_XIP
	/* Make sure the context is unlocked */
	spi_context_unlock_unconditionally(&((struct npcm4xx_spi_spim_data *)dev->data)->ctx);
#endif

#ifdef CONFIG_XIP
	/* setup fw update callback */
	if (!npcm4xx_spi_nor_set_update_fw_address)
		npcm4xx_spi_nor_set_update_fw_address = spi_nor_npcm4xx_spim_set_update_fw_address;

	if (!npcm4xx_spi_nor_do_fw_update)
		npcm4xx_spi_nor_do_fw_update = spi_nor_npcm4xx_spim_fw_update;
#endif
        return 0;
}

static const struct spi_nor_ops npcm4xx_spim_spi_nor_ops = {
        .transceive = spi_nor_npcm4xx_spim_transceive,
        .read_init = spi_nor_npcm4xx_spim_read_init,
        .write_init = spi_nor_npcm4xx_spim_write_init,
};

static struct spi_driver_api spi_npcm4xx_spim_api = {
	.transceive = spi_npcm4xx_spim_transceive,
	.release = spi_npcm4xx_spim_release,
	.spi_nor_op = &npcm4xx_spim_spi_nor_ops,
};

#define NPCM4XX_SPIM_INIT(inst)                                                                       \
		static struct npcm4xx_spi_spim_config npcm4xx_spi_spim_config_##inst = {              \
		.base = DT_INST_REG_ADDR_BY_NAME(inst, ctrl_reg),                                     \
		.spi_mmap_base = DT_INST_REG_ADDR_BY_NAME(inst, spi_mmap),                            \
		.clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(inst),		                              \
	};                                                                                            \
                                                                                                      \
	static struct npcm4xx_spi_spim_data npcm4xx_spi_spim_data_##inst = {                          \
		SPI_CONTEXT_INIT_LOCK(npcm4xx_spi_spim_data_##inst, ctx),                             \
	};                                                                                            \
                                                                                                      \
	DEVICE_DT_INST_DEFINE(inst, &spi_npcm4xx_spim_init,                                           \
							NULL,                                         \
							&npcm4xx_spi_spim_data_##inst,                \
							&npcm4xx_spi_spim_config_##inst, POST_KERNEL, \
							CONFIG_SPI_INIT_PRIORITY,                     \
							&spi_npcm4xx_spim_api);                       \

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_SPIM_INIT)
