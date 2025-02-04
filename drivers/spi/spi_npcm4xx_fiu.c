/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_spi_fiu

#include <drivers/clock_control.h>
#include <drivers/spi.h>
#include <logging/log.h>
#include <soc.h>

LOG_MODULE_REGISTER(spi_npcm4xx_fiu, LOG_LEVEL_ERR);

#include "spi_context.h"

enum npcm4xx_fiu_spi_nor_type {
	PRIVATE_SPI_NOR,
	SHARE_SPI_NOR,
	BACKUP_SPI_NOR,
	MAX_SPI_NOR
};

/* Device config */
struct npcm4xx_spi_fiu_config {
	/* flash interface unit core base address */
	mm_reg_t core_base;
	/* flash interface unit host base address */
	mm_reg_t host_base;
	/* clock configuration */
	struct npcm4xx_clk_cfg clk_cfg;
	/* direct access memory for private */
	mm_reg_t private_mmap_base;
	/* direct access memory for share */
	mm_reg_t share_mmap_base;
	/* direct access memory for backup */
	mm_reg_t backup_mmap_base;
};

/* Device data */
struct npcm4xx_spi_fiu_data {
	struct spi_context ctx;
	/* read/write init status */
	int rw_init[MAX_SPI_NOR];
	/* read command data */
	struct spi_nor_op_info read_op_info[MAX_SPI_NOR];
	/* write command data */
	struct spi_nor_op_info write_op_info[MAX_SPI_NOR];
};

/* Driver convenience defines */
#define HAL_INSTANCE(dev)                                                                          \
	((struct fiu_reg *)((const struct npcm4xx_spi_fiu_config *)(dev)->config)->core_base)
#define HAL_HOST_INSTANCE(dev)                                                                     \
	((struct fiu_reg *)((const struct npcm4xx_spi_fiu_config *)(dev)->config)->host_base)

static inline void spi_npcm4xx_fiu_uma_lock(const struct device *dev)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
#ifdef CONFIG_ESPI
	struct fiu_reg *const host_inst = HAL_HOST_INSTANCE(dev);

	/* Wait until host FIU inactive */
	while (!(host_inst->FIU_MSR_STS & BIT(NPCM4XX_FIU_MSR_STS_MSTR_INACT)));
#endif
	inst->FIU_MSR_IE_CFG |= BIT(NPCM4XX_FIU_MSR_IE_CFG_UMA_BLOCK);
}

static inline void spi_npcm4xx_fiu_uma_release(const struct device *dev)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);

	inst->FIU_MSR_IE_CFG &= ~BIT(NPCM4XX_FIU_MSR_IE_CFG_UMA_BLOCK);
}

static inline void spi_npcm4xx_fiu_cs_level(const struct device *dev,
						const struct spi_config *spi_cfg,
						int level)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;

	/* Set chip select to high/low level */
	if (level == 0) {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->UMA_ECTS &= ~BIT(NPCM4XX_UMA_ECTS_SW_CS0);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->UMA_ECTS &= ~BIT(NPCM4XX_UMA_ECTS_SW_CS1);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->UMA_ECTS &= ~BIT(NPCM4XX_UMA_ECTS_SW_CS2);
		}
	} else {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->UMA_ECTS |= BIT(NPCM4XX_UMA_ECTS_SW_CS0);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->UMA_ECTS |= BIT(NPCM4XX_UMA_ECTS_SW_CS1);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->UMA_ECTS |= BIT(NPCM4XX_UMA_ECTS_SW_CS2);
		}
	}
}

static inline int spi_npcm4xx_fiu_uma_device_select(const struct device *dev,
							const struct spi_config *spi_cfg,
							bool write)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	int cts = 0;

	/* clean backup device */
	inst->UMA_ECTS &= ~BIT(NPCM4XX_UMA_ECTS_DEV_NUM_BACK);

	/* default select private device */
	if (write)
		cts = UMA_CODE_ONLY_WRITE;
	else
		cts = UMA_CODE_ONLY_READ_BYTE(1);

	/* select share device */
        if (spi_nor_type == SHARE_SPI_NOR)
                cts |= UMA_FLD_SHD_SL;

	/* select backup device */
        if (spi_nor_type == BACKUP_SPI_NOR)
                 inst->UMA_ECTS |= BIT(NPCM4XX_UMA_ECTS_DEV_NUM_BACK);

	return cts;
}

static inline void spi_npcm4xx_fiu_exec_cmd(const struct device *dev,
						uint8_t code,
						uint8_t cts)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);

#ifdef CONFIG_ASSERT
	struct npcm4xx_spi_fiu_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	/* Flash mutex must be held while executing UMA commands */
	__ASSERT((k_sem_count_get(&ctx->lock) == 0), "UMA is not locked");
#endif

	/* set UMA_CODE */
	inst->UMA_CODE = code;

	/* execute UMA flash transaction */
	inst->UMA_CTS = cts;

	while (IS_BIT_SET(inst->UMA_CTS, NPCM4XX_UMA_CTS_EXEC_DONE))
		continue;
}

static int spi_npcm4xx_fiu_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					const struct spi_buf_set *tx_bufs,
					const struct spi_buf_set *rx_bufs)
{
	struct npcm4xx_spi_fiu_data *data = dev->data;
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct spi_context *ctx = &data->ctx;
	size_t cur_xfer_len;
	int error = 0;
	uint8_t cts;

	spi_context_lock(ctx, false, NULL, spi_cfg);
	ctx->config = spi_cfg;

	/* uma block */
	spi_npcm4xx_fiu_uma_lock(dev);
	/* assert chip assert */
	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 0);

	spi_context_buffers_setup(ctx, tx_bufs, rx_bufs, 1);

	if (rx_bufs == NULL) {
		/* write data to SPI flash */
		while (spi_context_tx_buf_on(ctx)) {
			cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);
			spi_npcm4xx_fiu_exec_cmd(dev, *ctx->tx_buf, cts);
			spi_context_update_tx(ctx, 1, 1);
		}
	} else {
		/* write data to SPI flash */
		cur_xfer_len = spi_context_total_tx_len(ctx);
		for (size_t i = 0; i < cur_xfer_len; i++) {
			cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);
			spi_npcm4xx_fiu_exec_cmd(dev, *ctx->tx_buf, cts);
			spi_context_update_tx(ctx, 1, 1);
		}

		/* read data from SPI flash */
		while (spi_context_rx_buf_on(ctx)) {
			cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, false);
			/* execute UMA flash transaction */
			inst->UMA_CTS = cts;
			while (IS_BIT_SET(inst->UMA_CTS,
					  NPCM4XX_UMA_CTS_EXEC_DONE))
				continue;
			/* Get read transaction results */
			*ctx->rx_buf = inst->UMA_DB0;
			spi_context_update_rx(ctx, 1, 1);
		}
	}

	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 1);
	spi_npcm4xx_fiu_uma_release(dev);
	spi_context_release(ctx, error);

	return error;
}

static inline void spi_npcm4xx_host_fiu_set_direct_read_mode(const struct device *dev)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct fiu_reg *const host_inst = HAL_HOST_INSTANCE(dev);

	/* Direct mode, normal, fast, or quad read */
	host_inst->SPI_FL_CFG = inst->SPI_FL_CFG;
	host_inst->RESP_CFG = inst->RESP_CFG;

	/* Direct read command */
	host_inst->RD_CMD_PVT = inst->RD_CMD_PVT;
	host_inst->RD_CMD_SHD = inst->RD_CMD_SHD;
	host_inst->RD_CMD_BACK = inst->RD_CMD_BACK;
	host_inst->SET_CMD_EN = inst->SET_CMD_EN;
	host_inst->ADDR_4B_EN = inst->ADDR_4B_EN;

	/* Burst config, exclude host/slave flag */
	host_inst->BURST_CFG =
		inst->BURST_CFG & ~BIT(NPCM4XX_BURST_CFG_SLAVE);
}

/* set 4 bytes address for
 * 1. Direct access(read/write)
 * 2. UMA(set CTS_A_SIZE case)
 */
static inline void spi_npcm4xx_fiu_set_address_mode(const struct device *dev,
							const struct spi_config *spi_cfg,
							struct spi_nor_op_info *op_info)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;

	if (op_info->addr_len == NPCM4XX_FIU_ADDR_4B_LENGTH) {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->ADDR_4B_EN |= BIT(NPCM4XX_ADDR_4B_EN_PVT_4B);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->ADDR_4B_EN |= BIT(NCPM4XX_ADDR_4B_EN_SHD_4B);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->ADDR_4B_EN |= BIT(NCPM4XX_ADDR_4B_EN_BACK_4B);
		}
	} else {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->ADDR_4B_EN &= ~BIT(NPCM4XX_ADDR_4B_EN_PVT_4B);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->ADDR_4B_EN &= ~BIT(NCPM4XX_ADDR_4B_EN_SHD_4B);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->ADDR_4B_EN &= ~BIT(NCPM4XX_ADDR_4B_EN_BACK_4B);
		}
	}
}

static inline uint32_t spi_npcm4xx_fiu_mmap_address(const struct device *dev,
						const struct spi_config *spi_cfg)
{
	const struct npcm4xx_spi_fiu_config *const config = dev->config;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	uint32_t mmap_address = 0;

	/* get fiu device memory mapping address */
	if (spi_nor_type == PRIVATE_SPI_NOR) {
		mmap_address = (uint32_t) config->private_mmap_base;
	} else if (spi_nor_type == SHARE_SPI_NOR) {
		mmap_address = (uint32_t) config->share_mmap_base;
	} else if (spi_nor_type == BACKUP_SPI_NOR) {
		mmap_address = (uint32_t) config->backup_mmap_base;
	}

	return mmap_address;
}

static inline void spi_npcm4xx_fiu_set_direct_read_mode(const struct device *dev,
							const struct spi_config *spi_cfg)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	struct spi_nor_op_info *op_info = NULL;

	/* read op information */
	op_info = &data->read_op_info[spi_nor_type];

	/* npcm4xx fiu read access support 111, 111_fast, 122 dual, 144 quad. */
	if (op_info->mode == JESD216_MODE_144) {
		SET_FIELD(inst->SPI_FL_CFG, NPCM4XX_SPI_FL_CFG_RD_MODE,
				NPCM4XX_SPI_FL_CFG_RD_MODE_FAST_DUAL);
		inst->RESP_CFG |= BIT(NCPM4XX_RESP_CFG_QUAD_EN);
	} else if (op_info->mode == JESD216_MODE_122) {
		SET_FIELD(inst->SPI_FL_CFG, NPCM4XX_SPI_FL_CFG_RD_MODE,
				NPCM4XX_SPI_FL_CFG_RD_MODE_FAST_DUAL);
	} else if (op_info->mode == JESD216_MODE_111_FAST) {
		SET_FIELD(inst->SPI_FL_CFG, NPCM4XX_SPI_FL_CFG_RD_MODE,
				NPCM4XX_SPI_FL_CFG_RD_MODE_FAST);
	} else {
		SET_FIELD(inst->SPI_FL_CFG, NPCM4XX_SPI_FL_CFG_RD_MODE,
				NPCM4XX_SPI_FL_CFG_RD_MODE_NORMAL);
	}

	/* configure direct read command for 4 bytes address mode */
	if (op_info->addr_len == NPCM4XX_FIU_ADDR_4B_LENGTH) {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->RD_CMD_PVT = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NPCM4XX_SET_CMD_EN_PVT_CMD_EN);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->RD_CMD_SHD = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NCPM4XX_SET_CMD_EN_SHD_CMD_EN);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->RD_CMD_BACK = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NCPM4XX_SET_CMD_EN_BACK_CMD_EN);
		}
	} else {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NPCM4XX_SET_CMD_EN_PVT_CMD_EN);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NCPM4XX_SET_CMD_EN_SHD_CMD_EN);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NCPM4XX_SET_CMD_EN_BACK_CMD_EN);
		}
	}

	/* set read max burst 16 bytes */
	SET_FIELD(inst->BURST_CFG, NPCM4XX_BURST_CFG_R_BURST,
			NPCM4XX_BURST_CFG_R_BURST_16B);

	/* configure 3/4 bytes address */
	spi_npcm4xx_fiu_set_address_mode(dev, spi_cfg, op_info);

#ifdef CONFIG_ESPI
	spi_npcm4xx_host_fiu_set_direct_read_mode(dev);
#endif
}

static void spi_nor_npcm4xx_fiu_direct_read_transceive(const struct device *dev,
							const struct spi_config *spi_cfg,
							struct spi_nor_op_info op_info)
{
	uint32_t start_addr = 0;

	/* set direct read mode */
	spi_npcm4xx_fiu_set_direct_read_mode(dev, spi_cfg);

	start_addr = spi_npcm4xx_fiu_mmap_address(dev, spi_cfg) + op_info.addr;

	/* CPU memory copy to read buffer */
	memcpy(op_info.buf, (void *)start_addr, op_info.data_len);
}

static inline void spi_npcm4xx_fiu_quad_program_data(const struct device *dev,
						const struct spi_config *spi_cfg,
						bool enable) {

	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	struct spi_nor_op_info *op_info = NULL;

	/* write op information */
	op_info = &data->write_op_info[spi_nor_type];

	/* enable quad program data, support UMA to send address as data part
	 * for 1_4_4 mode, otherwise only support 1_1_4 program */
	if (enable) {
		if ((op_info->mode == JESD216_MODE_144) ||
				(op_info->mode == JESD216_MODE_114)) {
			inst->Q_P_EN |= BIT(NPCM4XX_Q_P_EN_QUAD_P_EN);
		} else {
			inst->Q_P_EN &= ~BIT(NPCM4XX_Q_P_EN_QUAD_P_EN);
		}
	} else {
		inst->Q_P_EN &= ~BIT(NPCM4XX_Q_P_EN_QUAD_P_EN);
	}
}

static void spi_nor_npcm4xx_fiu_uma_transceive(const struct device *dev,
						const struct spi_config *spi_cfg,
						struct spi_nor_op_info op_info)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct spi_nor_op_info *uma_op_info = NULL;
	uint8_t *buf_data = NULL;
	uint8_t sub_addr = 0;
	int index = 0;
	uint8_t cts = 0;

	/* op information */
	uma_op_info = &op_info;

	spi_npcm4xx_fiu_uma_lock(dev);

	/* Assert chip assert */
	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 0);

	cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);

	/* send command */
	spi_npcm4xx_fiu_exec_cmd(dev, uma_op_info->opcode, cts);

	/* send address */
	index = uma_op_info->addr_len;
	while (index) {
		index = index - 1;
		sub_addr = (uma_op_info->addr >> (8 * index)) & 0xff;
		spi_npcm4xx_fiu_exec_cmd(dev, sub_addr, cts);
	}

	/* only support single dummy byte */
	if ((uma_op_info->dummy_cycle % NPCM4XX_FIU_SINGLE_DUMMY_BYTE) != 0) {
		LOG_ERR("UMA dummy must multi by %d",
				NPCM4XX_FIU_SINGLE_DUMMY_BYTE);
		return;
	}

	cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);

	/* send dummy bytes */
	for (index = 0; index < uma_op_info->dummy_cycle;
				index += NPCM4XX_FIU_SINGLE_DUMMY_BYTE) {
		spi_npcm4xx_fiu_exec_cmd(dev, 0x0, cts);
	}

	buf_data = uma_op_info->buf;

	if (buf_data == NULL)
		goto spi_nor_uma_done;

	/* read data from SPI flash */
	if (uma_op_info->data_direct == SPI_NOR_DATA_DIRECT_IN) {
		for (index = 0; index < uma_op_info->data_len; index++) {
			cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, false);
			/* execute UMA flash transaction */
			inst->UMA_CTS = cts;
			while (IS_BIT_SET(inst->UMA_CTS, NPCM4XX_UMA_CTS_EXEC_DONE))
				continue;
			/* Get read transaction results */
			*(buf_data + index) = inst->UMA_DB0;
		}
	} else if (uma_op_info->data_direct == SPI_NOR_DATA_DIRECT_OUT) {
		for (index = 0; index < uma_op_info->data_len; index++) {
			cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);
			/* execute UMA flash transaction */
			spi_npcm4xx_fiu_exec_cmd(dev, *(buf_data + index), cts);
		}
	}

spi_nor_uma_done:
	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 1);
	spi_npcm4xx_fiu_uma_release(dev);
}

#ifdef CONFIG_SPI_NPCM4XX_FIU_DIRECT_WRITE
static inline void spi_npcm4xx_fiu_direct_write_lock(const struct device *dev)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
#ifdef CONFIG_ESPI
	struct fiu_reg *const host_inst = HAL_HOST_INSTANCE(dev);

	/* Wait until host FIU inactive */
	while (!(host_inst->FIU_MSR_STS & BIT(NPCM4XX_FIU_MSR_STS_MSTR_INACT)));
#endif
	/* set direct write block */
	inst->DIRECT_WR_CFG |= BIT(NPCM4XX_DIRECT_WR_CFG_DIRECT_WR_BLOCK);
	/* enable direct write behavior, default use 0x2 (PP) */
	inst->BURST_CFG |= BIT(NPCM4XX_BURST_CFG_SPI_WR_EN);
}

static inline void spi_npcm4xx_fiu_direct_write_release(const struct device *dev)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);

	/* disable direct write */
	inst->BURST_CFG &= ~BIT(NPCM4XX_BURST_CFG_SPI_WR_EN);
	/* set direct write unblock */
	inst->DIRECT_WR_CFG &= ~BIT(NPCM4XX_DIRECT_WR_CFG_DIRECT_WR_BLOCK);
}

static inline void spi_npcm4xx_fiu_direct_write_cs_level(const struct device *dev,
							const struct spi_config *spi_cfg,
							int level)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;

	/* Set chip select to high/low level for direct write */
	if (level == 0) {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->DIRECT_WR_CFG &= ~BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS0);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->DIRECT_WR_CFG &= ~BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS1);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->DIRECT_WR_CFG &= ~BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS2);
		}
	} else {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->DIRECT_WR_CFG |= BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS0);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->DIRECT_WR_CFG |= BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS1);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->DIRECT_WR_CFG |= BIT(NPCM4XX_DIRECT_WR_CFG_DW_CS2);
		}
	}
}

static inline void spi_npcm4xx_fiu_set_direct_write_mode(const struct device *dev,
							const struct spi_config *spi_cfg)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	struct spi_nor_op_info *op_info = NULL;

	/* write op information */
	op_info = &data->write_op_info[spi_nor_type];

	/* configure direct write command for 4 bytes address mode,
	 * default use PP(0x2) for 3 bytes address mode */
	if (op_info->addr_len == NPCM4XX_FIU_ADDR_4B_LENGTH) {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->RD_CMD_PVT = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NPCM4XX_SET_CMD_EN_PVT_CMD_EN);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->RD_CMD_SHD = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NCPM4XX_SET_CMD_EN_SHD_CMD_EN);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->RD_CMD_BACK = op_info->opcode;
			inst->SET_CMD_EN |= BIT(NCPM4XX_SET_CMD_EN_BACK_CMD_EN);
		}
	} else {
		if (spi_nor_type == PRIVATE_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NPCM4XX_SET_CMD_EN_PVT_CMD_EN);
		} else if (spi_nor_type == SHARE_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NCPM4XX_SET_CMD_EN_SHD_CMD_EN);
		} else if (spi_nor_type == BACKUP_SPI_NOR) {
			inst->SET_CMD_EN &= ~BIT(NCPM4XX_SET_CMD_EN_BACK_CMD_EN);
		}
	}

	/* configure 3/4 bytes address */
	spi_npcm4xx_fiu_set_address_mode(dev, spi_cfg, op_info);
}

static inline void spi_nor_npcm4xx_fiu_direct_write_transceive(const struct device *dev,
								const struct spi_config *spi_cfg,
								struct spi_nor_op_info op_info)
{
	struct spi_nor_op_info *dw_op_info = NULL;
	uint32_t start_addr = 0;

	/* write op information */
	dw_op_info = &op_info;

	/* set direct write lock */
	spi_npcm4xx_fiu_direct_write_lock(dev);

	/* change to direct write mode */
	spi_npcm4xx_fiu_set_direct_write_mode(dev, spi_cfg);

	/* enable quad program data if need */
	spi_npcm4xx_fiu_quad_program_data(dev, spi_cfg, true);

	/* select direct write device */
	spi_npcm4xx_fiu_direct_write_cs_level(dev, spi_cfg, 0);

	start_addr = spi_npcm4xx_fiu_mmap_address(dev, spi_cfg) + dw_op_info->addr;

	/* CPU memory copy data to SPI flash mapping address  */
	memcpy((void *)start_addr, dw_op_info->buf, dw_op_info->data_len);

	spi_npcm4xx_fiu_direct_write_cs_level(dev, spi_cfg, 1);

	/* disable quad program if need */
	spi_npcm4xx_fiu_quad_program_data(dev, spi_cfg, false);

	spi_npcm4xx_fiu_direct_write_release(dev);
}
#else
static void spi_nor_npcm4xx_fiu_write_enable(const struct device *dev,
					const struct spi_config *spi_cfg)
{
	struct spi_nor_op_info wren_op_info =
				SPI_NOR_OP_INFO(JESD216_MODE_111, SPI_NOR_CMD_WREN,
				0, 0, 0, NULL, 0, SPI_NOR_DATA_DIRECT_OUT);

	spi_nor_npcm4xx_fiu_uma_transceive(dev, spi_cfg, wren_op_info);
}


static void spi_nor_npcm4xx_fiu_wait_ready(const struct device *dev,
						const struct spi_config *spi_cfg)
{
	uint8_t reg;
	struct spi_nor_op_info wait_op_info =
		SPI_NOR_OP_INFO(JESD216_MODE_111, SPI_NOR_CMD_RDSR,
		0, 0, 0, &reg, sizeof(reg), SPI_NOR_DATA_DIRECT_IN);

	/* wait wait command finish */
	do {
		spi_nor_npcm4xx_fiu_uma_transceive(dev, spi_cfg, wait_op_info);
	} while (reg & SPI_NOR_WIP_BIT);
}

static void spi_nor_npcm4xx_fiu_uma_write_transceive(const struct device *dev,
						const struct spi_config *spi_cfg,
						struct spi_nor_op_info op_info)
{
	struct fiu_reg *const inst = HAL_INSTANCE(dev);
	uint32_t total_len = 0, write_len = 0, write_count = 0;
	struct spi_nor_op_info *uma_op_info = NULL;
	uint8_t *buf_data = NULL;
	uint8_t cts = 0;
	bool last_write = false;

	/* write op information */
	uma_op_info = &op_info;

	spi_npcm4xx_fiu_uma_lock(dev);

	/* enable quad program data if need */
	spi_npcm4xx_fiu_quad_program_data(dev, spi_cfg, true);

	/* Assert chip assert */
	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 0);

	/* configure command and address */
	inst->UMA_CODE = uma_op_info->opcode;
	inst->UMA_AB0_3 = uma_op_info->addr;
	SET_FIELD(inst->UMA_ECTS, NPCM4XX_UMA_ECTS_UMA_ADDR_SIZE,
			uma_op_info->addr_len);

	/* select device */
	cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);

	/* if only write one byte, combine with command and address */
	if (uma_op_info->data_len == 1) {
		buf_data = (uint8_t *)uma_op_info->buf;
		inst->UMA_DB0 = buf_data[0];
		SET_FIELD(cts, NPCM4XX_UMA_CTS_D_SIZE, UMA_FIELD_DATA_1);
		write_count++;
	}

	/* execute UMA flash transaction */
	inst->UMA_CTS = cts;
	while (IS_BIT_SET(inst->UMA_CTS, NPCM4XX_UMA_CTS_EXEC_DONE))
                        continue;

	/* clean address size */
	SET_FIELD(inst->UMA_ECTS, NPCM4XX_UMA_ECTS_UMA_ADDR_SIZE, UMA_FIELD_ADDR_0);

	/* start to send remaining write bytes */
	if (uma_op_info->data_len) {
		total_len = uma_op_info->data_len - write_count;
		buf_data = (uint8_t *)uma_op_info->buf + write_count;
	}

	while (total_len) {
		/* select device */
		cts = spi_npcm4xx_fiu_uma_device_select(dev, spi_cfg, true);
		/* EXT_DB only work when set NO_CMD flag */
		cts |= UMA_FLD_NO_CMD;

		/* max NPCM4XX_FIU_EXT_DB_SIZE per UMA flash write transaction */
		if (total_len > NPCM4XX_FIU_EXT_DB_SIZE) {
			write_len = NPCM4XX_FIU_EXT_DB_SIZE;
		} else {
			write_len = total_len;
		}

		/* last write byte */
		if (write_len == 1) {
			last_write = true;
			break;
		}

		/* configure EXT DB data */
		inst->EXT_DB_CFG = BIT(NPCM4XX_EXT_DB_CFG_EXT_DB_EN);
                SET_FIELD(inst->EXT_DB_CFG, NPCM4XX_EXT_DB_CFG_D_SIZE_DB,
				write_len);

		/* copy data to EXT_DB according to write length */
		memcpy((void *)inst->EXT_DB_F_0, buf_data, write_len);
		/* execute UMA flash write transaction */
		inst->UMA_CTS = cts;
		while (IS_BIT_SET(inst->UMA_CTS, NPCM4XX_UMA_CTS_EXEC_DONE))
			continue;

		buf_data = buf_data + write_len;
		total_len = total_len - write_len;
		write_count = write_count + write_len;
	}

	/* clean EXT DB setting */
	inst->EXT_DB_CFG &= ~BIT(NPCM4XX_EXT_DB_CFG_EXT_DB_EN);
	SET_FIELD(inst->EXT_DB_CFG, NPCM4XX_EXT_DB_CFG_D_SIZE_DB, 0x0);

	spi_npcm4xx_fiu_cs_level(dev, spi_cfg, 1);

	/* disable quad program data if need */
	spi_npcm4xx_fiu_quad_program_data(dev, spi_cfg, false);

	spi_npcm4xx_fiu_uma_release(dev);

	if (last_write) {
		struct spi_nor_op_info write_op_info;

		/* wait WIP clear */
		spi_nor_npcm4xx_fiu_wait_ready(dev, spi_cfg);

		/* write enable */
		spi_nor_npcm4xx_fiu_write_enable(dev, spi_cfg);

		/* write the last byte */
		memcpy(&write_op_info, (void *)uma_op_info, sizeof(write_op_info));

		write_op_info.addr = write_op_info.addr + write_count;
		write_op_info.buf = buf_data;
		write_op_info.data_len = 1;

		/* write the last byte */
		spi_nor_npcm4xx_fiu_uma_write_transceive(dev, spi_cfg, write_op_info);
	}
}
#endif

static int spi_nor_npcm4xx_fiu_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;
	struct spi_context *ctx = &data->ctx;
	int error = 0;

	spi_context_lock(ctx, false, NULL, spi_cfg);
	ctx->config = spi_cfg;

	if (op_info.opcode == data->read_op_info[spi_nor_type].opcode) {
		if ((data->rw_init[spi_nor_type] & NPCM4XX_FIU_SPI_NOR_READ_INIT_OK) != 0) {
			spi_nor_npcm4xx_fiu_direct_read_transceive(dev, spi_cfg, op_info);
		} else {
			spi_nor_npcm4xx_fiu_uma_transceive(dev, spi_cfg, op_info);
		}
	} else if (op_info.opcode == data->write_op_info[spi_nor_type].opcode) {
		if ((data->rw_init[spi_nor_type] & NPCM4XX_FIU_SPI_NOR_WRITE_INIT_OK) != 0) {
#ifdef CONFIG_SPI_NPCM4XX_FIU_DIRECT_WRITE
			spi_nor_npcm4xx_fiu_direct_write_transceive(dev, spi_cfg, op_info);
#else
			spi_nor_npcm4xx_fiu_uma_write_transceive(dev, spi_cfg, op_info);
#endif
		} else {
			spi_nor_npcm4xx_fiu_uma_transceive(dev, spi_cfg, op_info);
		}
	} else {
		spi_nor_npcm4xx_fiu_uma_transceive(dev, spi_cfg, op_info);
	}

	spi_context_release(ctx, error);

	return error;
}

static int spi_nor_npcm4xx_fiu_read_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;

	/* record read command from jesd216 */
	memcpy(&data->read_op_info[spi_nor_type], &op_info, sizeof(op_info));

	/* default set direct read mode */
	spi_npcm4xx_fiu_set_direct_read_mode(dev, spi_cfg);

	data->rw_init[spi_nor_type] |= NPCM4XX_FIU_SPI_NOR_READ_INIT_OK;

	return 0;
}

static int spi_nor_npcm4xx_fiu_write_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spi_fiu_data *data = dev->data;
	enum npcm4xx_fiu_spi_nor_type spi_nor_type = spi_cfg->slave;

	/* record read command from jesd216 */
	memcpy(&data->write_op_info[spi_nor_type], &op_info, sizeof(op_info));

	data->rw_init[spi_nor_type] |= NPCM4XX_FIU_SPI_NOR_WRITE_INIT_OK;

	return 0;
}

int spi_npcm4xx_fiu_release(const struct device *dev, const struct spi_config *config)
{
	struct npcm4xx_spi_fiu_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;

	if (!spi_context_configured(ctx, config)) {
		return -EINVAL;
	}

	spi_context_unlock_unconditionally(ctx);
	return 0;
}

static int spi_npcm4xx_fiu_init(const struct device *dev)
{
	const struct npcm4xx_spi_fiu_config *const config = dev->config;
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

	/* Make sure the context is unlocked */
	spi_context_unlock_unconditionally(&((struct npcm4xx_spi_fiu_data *)dev->data)->ctx);

	return 0;
}

static const struct spi_nor_ops npcm4xx_fiu_spi_nor_ops = {
        .transceive = spi_nor_npcm4xx_fiu_transceive,
        .read_init = spi_nor_npcm4xx_fiu_read_init,
        .write_init = spi_nor_npcm4xx_fiu_write_init,
};

static struct spi_driver_api spi_npcm4xx_fiu_api = {
	.transceive = spi_npcm4xx_fiu_transceive,
	.release = spi_npcm4xx_fiu_release,
	.spi_nor_op = &npcm4xx_fiu_spi_nor_ops,
};

#define NPCM4XX_FIU_INIT(inst)                                                                      \
		static struct npcm4xx_spi_fiu_config npcm4xx_spi_fiu_config_##inst = {              \
		.core_base = DT_INST_REG_ADDR_BY_NAME(inst, core_reg),                              \
		.host_base = DT_INST_REG_ADDR_BY_NAME(inst, host_reg),                              \
		.private_mmap_base = DT_INST_REG_ADDR_BY_NAME(inst, private_mmap),                  \
		.share_mmap_base = DT_INST_REG_ADDR_BY_NAME(inst, share_mmap),                      \
		.backup_mmap_base = DT_INST_REG_ADDR_BY_NAME(inst, backup_mmap),                    \
		.clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(inst),		                            \
	};                                                                                          \
                                                                                                    \
	static struct npcm4xx_spi_fiu_data npcm4xx_spi_fiu_data_##inst = {                          \
		SPI_CONTEXT_INIT_LOCK(npcm4xx_spi_fiu_data_##inst, ctx),                            \
	};                                                                                          \
                                                                                                    \
	DEVICE_DT_INST_DEFINE(inst, &spi_npcm4xx_fiu_init,                                          \
							NULL,                                       \
							&npcm4xx_spi_fiu_data_##inst,               \
							&npcm4xx_spi_fiu_config_##inst, POST_KERNEL,\
							CONFIG_SPI_INIT_PRIORITY,                   \
							&spi_npcm4xx_fiu_api);                      \

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_FIU_INIT)
