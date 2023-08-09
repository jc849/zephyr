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
};

static void SPI_SET_SS0_HIGH(const struct device *dev)
{
	const struct npcm4xx_spip_config *cfg = dev->config;
	struct spip_reg *const spip_regs = (struct spip_reg *) cfg->base;

	spip_regs->SSCTL &= ~BIT(NPCM4XX_SSCTL_AUTOSS);
	spip_regs->SSCTL |= BIT(NPCM4XX_SSCTL_SSACTPOL);
	spip_regs->SSCTL |= BIT(NPCM4XX_SSCTL_SS);
}

static void SPI_SET_SS0_LOW(const struct device *dev)
{
	const struct npcm4xx_spip_config *cfg = dev->config;
	struct spip_reg *const spip_regs = (struct spip_reg *) cfg->base;

	spip_regs->SSCTL &= ~BIT(NPCM4XX_SSCTL_AUTOSS);
	spip_regs->SSCTL &= ~BIT(NPCM4XX_SSCTL_SSACTPOL);
	spip_regs->SSCTL |= BIT(NPCM4XX_SSCTL_SS);
}

static int spip_npcm4xx_configure(const struct device *dev,
				  const struct spi_config *config)
{
	const struct npcm4xx_spip_config *cfg = dev->config;
	struct npcm4xx_spip_data *data = dev->data;
	struct spip_reg *const spip_regs = (struct spip_reg *) cfg->base;
	uint32_t u32Div;
	int ret = 0;

	if (SPI_WORD_SIZE_GET(config->operation) != 8) {
		LOG_ERR("Word sizes other than 8 bits are not supported");
		ret = -ENOTSUP;
	} else  {
		spip_regs->CTL &= ~(0x1F << NPCM4XX_CTL_DWIDTH);
		spip_regs->CTL |=  (SPI_WORD_SIZE_GET(config->operation) << NPCM4XX_CTL_DWIDTH);
	}

	if (SPI_MODE_GET(config->operation) & SPI_MODE_CPOL) {
		spip_regs->CTL |= BIT(NPCM4XX_CTL_CLKPOL);
		if (SPI_MODE_GET(config->operation) & SPI_MODE_CPHA) {
			spip_regs->CTL &= ~BIT(NPCM4XX_CTL_TXNEG);
			spip_regs->CTL &= ~BIT(NPCM4XX_CTL_RXNEG);
		} else {
			spip_regs->CTL |= BIT(NPCM4XX_CTL_TXNEG);
			spip_regs->CTL |= BIT(NPCM4XX_CTL_RXNEG);
		}
	} else {
		spip_regs->CTL &= ~BIT(NPCM4XX_CTL_CLKPOL);
	}

	if (config->operation & SPI_TRANSFER_LSB) {
		spip_regs->CTL |= BIT(NPCM4XX_CTL_LSB);
	} else {
		spip_regs->CTL &= ~BIT(NPCM4XX_CTL_LSB);
	}

	if (config->operation & SPI_OP_MODE_SLAVE) {
		LOG_ERR("Slave mode is not supported");
		ret = -ENOTSUP;
	} else {
		spip_regs->CTL &= ~BIT(NPCM4XX_CTL_SLAVE);
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

	spip_regs->CLKDIV = (spip_regs->CLKDIV & ~0xFF) | u32Div;

	/* spip enable */
	spip_regs->CTL |= BIT(NPCM4XX_CTL_SPIEN);

	return ret;
}

static int spip_npcm4xx_transceive(const struct device *dev,
				   const struct spi_config *config,
				   const struct spi_buf_set *tx_bufs,
				   const struct spi_buf_set *rx_bufs)
{
	const struct npcm4xx_spip_config *cfg = dev->config;
	struct npcm4xx_spip_data *data = dev->data;
	struct spip_reg *const spip_regs = (struct spip_reg *) cfg->base;

	spi_context_lock(&data->ctx, false, NULL, config);
	data->ctx.config = config;

	int ret, error = 0;
	uint8_t txDone;
	/* Configure */
	ret = spip_npcm4xx_configure(dev, config);
	if (ret) {
		return -ENOTSUP;
	}
	ret = 0;
	spi_context_buffers_setup(&data->ctx, tx_bufs, rx_bufs, 1);


	if (config->operation & SPI_OP_MODE_SLAVE) {
		/* Slave */
		LOG_ERR("Slave mode is not supported");
		ret = -ENOTSUP;
	} else {
		/* Master */
		SPI_SET_SS0_LOW(dev);
		if (rx_bufs == NULL) {
			/* write data to SPI flash */
			while (spi_context_tx_buf_on(&data->ctx)) {
				/*TX*/
				if ((spip_regs->STATUS & BIT(NPCM4XX_STATUS_TXFULL)) == 0) {
					spip_regs->TX = *data->ctx.tx_buf;
					spi_context_update_tx(&data->ctx, 1, 1);
				}
			}
		} else   {
			txDone = 0;
			spip_regs->FIFOCTL |= BIT(NPCM4XX_FIFOCTL_RXRST);
			while (1) {
				/*TX*/
				if (spi_context_tx_buf_on(&data->ctx)) {
					if (!(spip_regs->STATUS & BIT(NPCM4XX_STATUS_TXFULL))) {
						spip_regs->TX = *data->ctx.tx_buf;
						spi_context_update_tx(&data->ctx, 1, 1);
					}
				} else if (!(spip_regs->STATUS & BIT(NPCM4XX_STATUS_BUSY))) {
					txDone = 1;
				}

				/*RX*/
				if (spi_context_rx_buf_on(&data->ctx)) {
					if (!(spip_regs->STATUS & BIT(NPCM4XX_STATUS_RXEMPTY))) {
						*data->ctx.rx_buf = spip_regs->RX;
						spi_context_update_rx(&data->ctx, 1, 1);
					} else if (txDone == 1) {
						ret = -EOVERFLOW;
						break;
					}
				} else   {
					if (txDone == 1) {
						break;
					}
				}
			}
		}

		do {
			if ((spip_regs->STATUS & BIT(NPCM4XX_STATUS_BUSY)) == 0) {
				break;
			}
		} while (1);

		SPI_SET_SS0_HIGH(dev);
	}
	spi_context_release(&data->ctx, error);
	if (error != 0) {
		ret = error;
	}

	return ret;
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

static const struct spi_driver_api spip_npcm4xx_driver_api = {
	.transceive = spip_npcm4xx_transceive,
#ifdef CONFIG_SPI_ASYNC
	.transceive_async = spip_npcm4xx_transceive_async,
#endif
	.release = spip_npcm4xx_release,
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
