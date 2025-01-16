/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_spip

#define LOG_LEVEL CONFIG_SPI_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(spip_npcm4xx);

#include <drivers/clock_control.h>
#include <drivers/spi.h>
#include <soc.h>
#include "spi_context.h"

/* Device constant configuration parameters */
struct npcm4xx_spip_config {
	/* SPIP reg base address */
	uintptr_t base;
	/* clock configuration */
	struct npcm4xx_clk_cfg clk_cfg;
};

/* Device run time data */
struct npcm4xx_spip_data {
	struct spi_context ctx;
	uint32_t apb3;
	/* read/write init flags */
	int rw_init;
	/* read command data */
	struct spi_nor_op_info read_op_info;
	/* write command data */
	struct spi_nor_op_info write_op_info;
};

/* Driver convenience defines */
#define HAL_INSTANCE(dev)									\
	((struct spip_reg *)((const struct npcm4xx_spip_config *)(dev)->config)->base)

static void SPI_SET_SS0_HIGH(const struct device *dev)
{
	struct spip_reg *const inst = HAL_INSTANCE(dev);

	inst->SSCTL &= ~BIT(NPCM4XX_SSCTL_AUTOSS);
	inst->SSCTL |= BIT(NPCM4XX_SSCTL_SSACTPOL);
	inst->SSCTL |= BIT(NPCM4XX_SSCTL_SS);
}

static void SPI_SET_SS0_LOW(const struct device *dev)
{
	struct spip_reg *const inst = HAL_INSTANCE(dev);

	inst->SSCTL &= ~BIT(NPCM4XX_SSCTL_AUTOSS);
	inst->SSCTL &= ~BIT(NPCM4XX_SSCTL_SSACTPOL);
	inst->SSCTL |= BIT(NPCM4XX_SSCTL_SS);
}

static int spip_npcm4xx_configure(const struct device *dev,
				  const struct spi_config *config)
{
	struct npcm4xx_spip_data *data = dev->data;
	struct spip_reg *const inst = HAL_INSTANCE(dev);
	uint32_t u32Div = 0;
	int ret = 0;

	if (SPI_WORD_SIZE_GET(config->operation) != 8) {
		LOG_ERR("Word sizes other than 8 bits are not supported");
		ret = -ENOTSUP;
	} else  {
		inst->CTL &= ~(0x1F << NPCM4XX_CTL_DWIDTH);
		inst->CTL |=  (SPI_WORD_SIZE_GET(config->operation) << NPCM4XX_CTL_DWIDTH);
	}

	if (SPI_MODE_GET(config->operation) & SPI_MODE_CPOL) {
		inst->CTL |= BIT(NPCM4XX_CTL_CLKPOL);
		if (SPI_MODE_GET(config->operation) & SPI_MODE_CPHA) {
			inst->CTL &= ~BIT(NPCM4XX_CTL_TXNEG);
			inst->CTL &= ~BIT(NPCM4XX_CTL_RXNEG);
		} else {
			inst->CTL |= BIT(NPCM4XX_CTL_TXNEG);
			inst->CTL |= BIT(NPCM4XX_CTL_RXNEG);
		}
	} else {
		inst->CTL &= ~BIT(NPCM4XX_CTL_CLKPOL);
	}

	if (config->operation & SPI_TRANSFER_LSB) {
		inst->CTL |= BIT(NPCM4XX_CTL_LSB);
	} else {
		inst->CTL &= ~BIT(NPCM4XX_CTL_LSB);
	}

	if (config->operation & SPI_OP_MODE_SLAVE) {
		LOG_ERR("Slave mode is not supported");
		ret = -ENOTSUP;
	} else {
		inst->CTL &= ~BIT(NPCM4XX_CTL_SLAVE);
	}

	/* Set Bus clock */
	if (config->frequency != 0) {
		if (config->frequency <= data->apb3) {
			u32Div = (data->apb3 / config->frequency) - 1;
			if (data->apb3 % config->frequency) {
				u32Div += 1;
			}
			if (u32Div > 0xFF) {
				u32Div = 0xFF;
			}
		}
	}

	inst->CLKDIV = (inst->CLKDIV & ~0xFF) | u32Div;

	/* spip enable */
	inst->CTL |= BIT(NPCM4XX_CTL_SPIEN);

	return ret;
}

static int spip_npcm4xx_transceive(const struct device *dev,
				   const struct spi_config *config,
				   const struct spi_buf_set *tx_bufs,
				   const struct spi_buf_set *rx_bufs)
{
	struct spip_reg *const inst = HAL_INSTANCE(dev);
	struct npcm4xx_spip_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	int ret = 0, error = 0;
	uint8_t tx_done = 0;

	spi_context_lock(ctx, false, NULL, config);
	ctx->config = config;

	/* Configure */
	ret = spip_npcm4xx_configure(dev, config);
	if (ret) {
		ret = -ENOTSUP;
		goto spip_transceive_done;
	}

	spi_context_buffers_setup(ctx, tx_bufs, rx_bufs, 1);

	if (config->operation & SPI_OP_MODE_SLAVE) {
		/* Slave */
		LOG_ERR("Slave mode is not supported");
		ret = -ENOTSUP;
		goto spip_transceive_done;
	} else {
		/* Master */
		SPI_SET_SS0_LOW(dev);
		if (rx_bufs == NULL) {
			/* write data to SPI flash */
			while (spi_context_tx_buf_on(ctx)) {
				/*TX*/
				if ((inst->STATUS & BIT(NPCM4XX_STATUS_TXFULL)) == 0) {
					inst->TX = (uint8_t)*ctx->tx_buf;
					spi_context_update_tx(ctx, 1, 1);
				}
			}
		} else {
			inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);
			while (1) {
				/*TX*/
				if (spi_context_tx_buf_on(ctx)) {
					if (!(inst->STATUS & BIT(NPCM4XX_STATUS_TXFULL))) {
						inst->TX = (uint8_t)*ctx->tx_buf;
						spi_context_update_tx(ctx, 1, 1);
					}
				} else if (!(inst->STATUS & BIT(NPCM4XX_STATUS_BUSY))) {
					tx_done = 1;
				}

				/*RX*/
				if (spi_context_rx_buf_on(ctx)) {
					if (!(inst->STATUS & BIT(NPCM4XX_STATUS_RXEMPTY))) {
						*ctx->rx_buf = (uint8_t)inst->RX;
						spi_context_update_rx(ctx, 1, 1);
					} else if (tx_done == 1) {
						ret = -EOVERFLOW;
						break;
					}
				} else   {
					if (tx_done == 1) {
						break;
					}
				}
			}
		}

		do {
			if ((inst->STATUS & BIT(NPCM4XX_STATUS_BUSY)) == 0) {
				break;
			}
		} while (1);

		SPI_SET_SS0_HIGH(dev);
	}

spip_transceive_done:
	spi_context_release(ctx, error);

	if (ret)
		return ret;
	else
		return error;
}

#ifdef CONFIG_SPI_ASYNC
static int spip_npcm4xx_transceive_async(const struct device *dev,
					 const struct spi_config *config,
					 const struct spi_buf_set *tx_bufs,
					 const struct spi_buf_set *rx_bufs)
{
	return spip_npcm4xx_transceive(dev, config, tx_bufs, rx_bufs);
}
#endif /* CONFIG_SPI_ASYNC */

static int spip_npcm4xx_release(const struct device *dev,
				const struct spi_config *config)
{
	struct npcm4xx_spip_data *data = dev->data;

	spi_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static int spip_npcm4xx_init(const struct device *dev)
{
	const struct npcm4xx_spip_config *cfg = dev->config;
	struct npcm4xx_spip_data *data = dev->data;
	const struct device *const clk_dev =
		device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	uint32_t spip_apb3;
	int ret;

	if (!device_is_ready(clk_dev)) {
		LOG_ERR("%s device not ready", clk_dev->name);
		return -ENODEV;
	}
	/* Turn on device clock first and get source clock freq. */
	ret = clock_control_on(clk_dev,
			       (clock_control_subsys_t)&cfg->clk_cfg);
	if (ret < 0) {
		LOG_ERR("Turn on SPIP clock fail %d", ret);
		return ret;
	}

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t *)
				     &cfg->clk_cfg, &spip_apb3);

	if (ret < 0) {
		LOG_ERR("Get ITIM clock rate error %d", ret);
		return ret;
	}

	data->apb3 = spip_apb3;

	spi_context_unlock_unconditionally(&data->ctx);
	return 0;
}

static inline void spi_npcm4xx_spip_write_data(const struct device *dev, uint32_t code)
{
	struct spip_reg *const inst = HAL_INSTANCE(dev);

	while ((inst->STATUS & BIT(NPCM4XX_STATUS_TXFULL)));

	inst->TX = code;

	while (inst->STATUS & BIT(NPCM4XX_STATUS_BUSY));
}

static void spi_nor_npcm4xx_spip_fifo_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct spip_reg *const inst = HAL_INSTANCE(dev);
	struct spi_nor_op_info *normal_op_info = NULL;
	uint32_t index = 0, dummy_write = 0;
	uint8_t *buf_data = NULL;
	uint8_t sub_addr = 0;
	uint8_t dummy_clk_per_byte = 0;

	normal_op_info = &op_info;

	/* clear tx/rx fifo buffer */
	inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_TXRST);
	inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);

	SPI_SET_SS0_LOW(dev);

	/* send command */
	spi_npcm4xx_spip_write_data(dev, (uint32_t)normal_op_info->opcode);

	/* In 144 or 122 mode, address and dummy cycle need enable quad or dual mode.
	 * We only support 8bit WIDTH, so 144 mode send one dummy byte need 2 clocks,
	 * 122 mode need 4 clocks, others case need 8 clocks to send one dummy byte.
	 */
	if (op_info.mode == JESD216_MODE_144) {
		inst->CTL &= ~BIT(NPCM4XX_CTL_DUALIOEN);
		inst->CTL |= BIT(NPCM4XX_CTL_QUADIOEN);
		inst->CTL |= BIT(NPCM4XX_CTL_QDIODIR);
		dummy_clk_per_byte = 2;
	} else if (op_info.mode == JESD216_MODE_122) {
		inst->CTL |= BIT(NPCM4XX_CTL_DUALIOEN);
		inst->CTL &= ~BIT(NPCM4XX_CTL_QUADIOEN);
		inst->CTL |= BIT(NPCM4XX_CTL_QDIODIR);
		dummy_clk_per_byte = 4;
	} else {
		dummy_clk_per_byte = 8;
	}

	/* send address */
	index = normal_op_info->addr_len;

	while (index) {
		index = index - 1;
		sub_addr = (normal_op_info->addr >> (8 * index)) & 0xff;
		spi_npcm4xx_spip_write_data(dev, (uint32_t)sub_addr);
	}

	/* send dummy bytes */
	for (index = 0; index < normal_op_info->dummy_cycle;
				index += dummy_clk_per_byte) {
		spi_npcm4xx_spip_write_data(dev, dummy_write);
	}

	/* clear rx fifo buffer */
	inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);

	buf_data = normal_op_info->buf;

	if (buf_data == NULL) {
		inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);
		goto spi_nor_normal_done;
	}

	/* If 114 or 112 mode, enable quad or dual mode after send dummy bytes */
	if (op_info.mode == JESD216_MODE_114) {
		inst->CTL &= ~BIT(NPCM4XX_CTL_DUALIOEN);
		inst->CTL |= BIT(NPCM4XX_CTL_QUADIOEN);
	} else if (op_info.mode == JESD216_MODE_112) {
		inst->CTL |= BIT(NPCM4XX_CTL_DUALIOEN);
		inst->CTL &= ~BIT(NPCM4XX_CTL_QUADIOEN);
	}

	/* For read, change direct to input mode */
	if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_IN) {
		if (inst->CTL & BIT(NPCM4XX_CTL_QDIODIR)) {
			inst->CTL &= ~BIT(NPCM4XX_CTL_QDIODIR);
		}
	}

	/* read data from SPI flash */
	if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_IN) {
		for (index = 0; index < normal_op_info->data_len; index++) {
			/* write dummy data out*/
			spi_npcm4xx_spip_write_data(dev, dummy_write);
			/* wait received data */
			while((inst->STATUS & BIT(NPCM4XX_STATUS_RXEMPTY)));

			*(buf_data + index) = (uint8_t)inst->RX;
		}
	} else if (normal_op_info->data_direct == SPI_NOR_DATA_DIRECT_OUT) {
		for (index = 0; index < normal_op_info->data_len; index++) {
			/* write data to SPI flash */
			spi_npcm4xx_spip_write_data(dev, (uint32_t)*(buf_data + index));
		}
		/* clear rx fifo buffer */
		inst->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);
	}

spi_nor_normal_done:
	inst->CTL &= ~BIT(NPCM4XX_CTL_QUADIOEN);
	inst->CTL &= ~BIT(NPCM4XX_CTL_DUALIOEN);
	inst->CTL &= ~BIT(NPCM4XX_CTL_QDIODIR);

	SPI_SET_SS0_HIGH(dev);
}

static int spi_nor_npcm4xx_spip_transceive(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spip_data *data = dev->data;
	struct spi_context *ctx = &data->ctx;
	int ret = 0, error = 0;

	ret = spip_npcm4xx_configure(dev, spi_cfg);
	if (ret)
		return ret;

	spi_context_lock(ctx, false, NULL, spi_cfg);
	ctx->config = spi_cfg;

	spi_nor_npcm4xx_spip_fifo_transceive(dev, spi_cfg, op_info);

	spi_context_release(ctx, error);

	return error;
}

static int spi_nor_npcm4xx_spip_read_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spip_data *data = dev->data;

	/* record read command from jesd216 */
	memcpy(&data->read_op_info, &op_info, sizeof(op_info));

	data->rw_init |= NPCM4XX_SPIP_SPI_NOR_READ_INIT_OK;

	return 0;
}

static int spi_nor_npcm4xx_spip_write_init(const struct device *dev,
					const struct spi_config *spi_cfg,
					struct spi_nor_op_info op_info)
{
	struct npcm4xx_spip_data *data = dev->data;

	/* record read command from jesd216 */
	memcpy(&data->write_op_info, &op_info, sizeof(op_info));

	data->rw_init |= NPCM4XX_SPIP_SPI_NOR_WRITE_INIT_OK;

	return 0;
}

static const struct spi_nor_ops npcm4xx_spip_spi_nor_ops = {
	.transceive = spi_nor_npcm4xx_spip_transceive,
	.read_init = spi_nor_npcm4xx_spip_read_init,
	.write_init = spi_nor_npcm4xx_spip_write_init,
};

static const struct spi_driver_api spip_npcm4xx_driver_api = {
	.transceive = spip_npcm4xx_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spip_npcm4xx_transceive_async,
#endif
	.release = spip_npcm4xx_release,
	.spi_nor_op = &npcm4xx_spip_spi_nor_ops,
};


static const struct npcm4xx_spip_config spip_npcm4xx_config = {
	.base = DT_INST_REG_ADDR(0),
	.clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(0),
};

static struct npcm4xx_spip_data spip_npcm4xx_dev_data = {
	SPI_CONTEXT_INIT_LOCK(spip_npcm4xx_dev_data, ctx),
};


DEVICE_DT_INST_DEFINE(0, &spip_npcm4xx_init, NULL,
		      &spip_npcm4xx_dev_data,
		      &spip_npcm4xx_config, POST_KERNEL,
		      CONFIG_SPI_INIT_PRIORITY, &spip_npcm4xx_driver_api);
