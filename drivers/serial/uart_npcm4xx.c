/*
 * Copyright (c) 2020 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_uart

#include <sys/__assert.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <drivers/clock_control.h>
#include <kernel.h>
#include <pm/device.h>
#include <soc.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(uart_npcm4xx, LOG_LEVEL_ERR);

/* Driver config */
struct uart_npcm4xx_config {
	struct uart_device_config uconf;
	/* clock configuration */
	struct npcm4xx_clk_cfg clk_cfg;
};

/* Driver data */
struct uart_npcm4xx_data {
	/* Baud rate */
	struct uart_config ucfg;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t user_cb;
	void *user_data;
#endif
#ifdef CONFIG_PM_DEVICE
	uint32_t pm_state;
#endif
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct uart_npcm4xx_config *)(dev)->config)

#define DRV_DATA(dev) ((struct uart_npcm4xx_data *)(dev)->data)

#define HAL_INSTANCE(dev) (struct uart_reg *)(DRV_CONFIG(dev)->uconf.base)

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static int uart_npcm4xx_tx_fifo_ready(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	/* True if the Tx FIFO contains some space available */
	return !(GET_FIELD(inst->UTXFLV, NPCM4XX_UTXFLV_TFL) >= 16);
}

static int uart_npcm4xx_rx_fifo_available(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	/* True if at least one byte is in the Rx FIFO */
	return !(GET_FIELD(inst->URXFLV, NPCM4XX_URXFLV_RFL) == 0);
}

static void uart_npcm4xx_dis_all_tx_interrupts(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	/* Disable ETI (Enable Transmit Interrupt) interrupt */
	inst->UICTRL &= ~(BIT(NPCM4XX_UICTRL_ETI));
}

static void uart_npcm4xx_clear_rx_fifo(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);
	uint8_t scratch;

	/* Read all dummy bytes out from Rx FIFO */
	while (uart_npcm4xx_rx_fifo_available(dev))
		scratch = inst->URBUF;
}

static int uart_npcm4xx_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);
	uint8_t tx_bytes = 0U;

	/* If Tx FIFO is still ready to send */
	while ((size - tx_bytes > 0) && uart_npcm4xx_tx_fifo_ready(dev)) {
		/* Put a character into Tx FIFO */
		inst->UTBUF = tx_data[tx_bytes++];
	}

	return tx_bytes;
}

static int uart_npcm4xx_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);
	unsigned int rx_bytes = 0U;

	/* If least one byte is in the Rx FIFO */
	while ((size - rx_bytes > 0) && uart_npcm4xx_rx_fifo_available(dev)) {
		/* Receive one byte from Rx FIFO */
		rx_data[rx_bytes++] = inst->URBUF;
	}

	return rx_bytes;
}

static void uart_npcm4xx_irq_tx_enable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL |= BIT(NPCM4XX_UICTRL_ETI);
}

static void uart_npcm4xx_irq_tx_disable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL &= ~(BIT(NPCM4XX_UICTRL_ETI));
}

static int uart_npcm4xx_irq_tx_ready(const struct device *dev)
{
	return uart_npcm4xx_tx_fifo_ready(dev);
}

static int uart_npcm4xx_irq_tx_complete(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	/* Tx FIFO is empty or last byte is sending */
	return !IS_BIT_SET(inst->USTAT, NPCM4XX_USTAT_XMIP);
}

static void uart_npcm4xx_irq_rx_enable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL |= BIT(NPCM4XX_UICTRL_ERI);
}

static void uart_npcm4xx_irq_rx_disable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL &= ~(BIT(NPCM4XX_UICTRL_ERI));
}

static int uart_npcm4xx_irq_rx_ready(const struct device *dev)
{
	return uart_npcm4xx_rx_fifo_available(dev);
}

static void uart_npcm4xx_irq_err_enable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL |= BIT(NPCM4XX_UICTRL_EEI);
}

static void uart_npcm4xx_irq_err_disable(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	inst->UICTRL &= ~(BIT(NPCM4XX_UICTRL_EEI));
}

static int uart_npcm4xx_irq_is_pending(const struct device *dev)
{
	return (uart_npcm4xx_irq_tx_ready(dev) || uart_npcm4xx_irq_rx_ready(dev));
}

static int uart_npcm4xx_irq_update(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 1;
}

static void uart_npcm4xx_irq_callback_set(const struct device *dev,
					 uart_irq_callback_user_data_t cb,
					 void *cb_data)
{
	struct uart_npcm4xx_data *data = DRV_DATA(dev);

	data->user_cb = cb;
	data->user_data = cb_data;
}

static void uart_npcm4xx_isr(const struct device *dev)
{
	struct uart_npcm4xx_data *data = DRV_DATA(dev);

	if (data->user_cb) {
		data->user_cb(dev, data->user_data);
	}
}

/*
 * Poll-in implementation for interrupt driven config, forward call to
 * uart_npcm4xx_fifo_read().
 */
static int uart_npcm4xx_poll_in(const struct device *dev, unsigned char *c)
{
	return uart_npcm4xx_fifo_read(dev, c, 1) ? 0 : -1;
}

/*
 * Poll-out implementation for interrupt driven config, forward call to
 * uart_npcm4xx_fifo_fill().
 */
static void uart_npcm4xx_poll_out(const struct device *dev, unsigned char c)
{
	while (!uart_npcm4xx_fifo_fill(dev, &c, 1)) {
		continue;
	}
}

#else /* !CONFIG_UART_INTERRUPT_DRIVEN */

/*
 * Poll-in implementation for byte mode config, read byte from URBUF if
 * available.
 */
static int uart_npcm4xx_poll_in(const struct device *dev, unsigned char *c)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	if (!IS_BIT_SET(inst->UICTRL, NPCM4XX_UICTRL_RBF)) {
		return -1;
	}

	*c = inst->URBUF;
	return 0;
}

/*
 * Poll-out implementation for byte mode config, write byte to UTBUF if empty.
 */
static void uart_npcm4xx_poll_out(const struct device *dev, unsigned char c)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);

	while (!IS_BIT_SET(inst->UICTRL, NPCM4XX_UICTRL_TBE)) {
		continue;
	}
	inst->UTBUF = c;
}
#endif /* !CONFIG_UART_INTERRUPT_DRIVEN */

/* UART api functions */
static int uart_npcm4xx_err_check(const struct device *dev)
{
	struct uart_reg *const inst = HAL_INSTANCE(dev);
	uint32_t err = 0U;

	uint8_t stat = inst->USTAT;

	if (IS_BIT_SET(stat, NPCM4XX_USTAT_DOE))
		err |= UART_ERROR_OVERRUN;

	if (IS_BIT_SET(stat, NPCM4XX_USTAT_PE))
		err |= UART_ERROR_PARITY;

	if (IS_BIT_SET(stat, NPCM4XX_USTAT_FE))
		err |= UART_ERROR_FRAMING;

	return err;
}

/* UART driver registration */
static const struct uart_driver_api uart_npcm4xx_driver_api = {
	.poll_in = uart_npcm4xx_poll_in,
	.poll_out = uart_npcm4xx_poll_out,
	.err_check = uart_npcm4xx_err_check,
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = uart_npcm4xx_fifo_fill,
	.fifo_read = uart_npcm4xx_fifo_read,
	.irq_tx_enable = uart_npcm4xx_irq_tx_enable,
	.irq_tx_disable = uart_npcm4xx_irq_tx_disable,
	.irq_tx_ready = uart_npcm4xx_irq_tx_ready,
	.irq_tx_complete = uart_npcm4xx_irq_tx_complete,
	.irq_rx_enable = uart_npcm4xx_irq_rx_enable,
	.irq_rx_disable = uart_npcm4xx_irq_rx_disable,
	.irq_rx_ready = uart_npcm4xx_irq_rx_ready,
	.irq_err_enable = uart_npcm4xx_irq_err_enable,
	.irq_err_disable = uart_npcm4xx_irq_err_disable,
	.irq_is_pending = uart_npcm4xx_irq_is_pending,
	.irq_update = uart_npcm4xx_irq_update,
	.irq_callback_set = uart_npcm4xx_irq_callback_set,
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

static int uart_npcm4xx_init(const struct device *dev)
{
	uint32_t div, opt_dev, min_deviation, clk, calc_baudrate, deviation;
	uint8_t prescalar, opt_prescalar, i;
	const struct uart_npcm4xx_config *const config = DRV_CONFIG(dev);
	struct uart_npcm4xx_data *const data = DRV_DATA(dev);
	struct uart_reg *const inst = HAL_INSTANCE(dev);
	const struct device *const clk_dev =
					device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	uint32_t uart_rate;
	int ret;

	/* Calculated UART baudrate , clock source from APB2 */
	opt_prescalar = opt_dev = 0;
	prescalar = 10;
	min_deviation = 0xFFFFFFFF;

	/* Turn on device clock first and get source clock freq. */
	ret = clock_control_on(clk_dev, (clock_control_subsys_t *)
							&config->clk_cfg);
	if (ret < 0) {
		LOG_ERR("Turn on UART clock fail %d", ret);
		return ret;
	}

	/*
	 * If apb2's clock is not 15MHz, we need to find the other optimized
	 * values of UPSR and UBAUD for baud rate 115200.
	 */
	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t *)
		&config->clk_cfg, &uart_rate);
	if (ret < 0) {
		LOG_ERR("Get UART clock rate error %d", ret);
		return ret;
	}

	clk = uart_rate;

	for (i = 1; i <= 31; i++) {
		div = (clk * 10) / (16 * data->ucfg.baudrate * prescalar);
		if (div == 0) {
			div = 1;
		}

		calc_baudrate = (clk * 10) / (16 * div * prescalar);
		deviation = (calc_baudrate > data->ucfg.baudrate) ?
				    (calc_baudrate - data->ucfg.baudrate) :
				    (data->ucfg.baudrate - calc_baudrate);
		if (deviation < min_deviation) {
			min_deviation = deviation;
			opt_prescalar = i;
			opt_dev = div;
		}
		prescalar += 5;
	}
	opt_dev--;
	inst->UPSR = ((opt_prescalar << 3) & 0xF8) | ((opt_dev >> 8) & 0x7);
	inst->UBAUD = (uint8_t)opt_dev;

	/*
	 * 8-N-1, FIFO enabled.  Must be done after setting
	 * the divisor for the new divisor to take effect.
	 */
	inst->UFRS = 0x00;
#if CONFIG_UART_INTERRUPT_DRIVEN
	inst->UFCTRL |= BIT(NPCM4XX_UFCTRL_FIFOEN);

	/* Disable all UART tx FIFO interrupts */
	uart_npcm4xx_dis_all_tx_interrupts(dev);

	/* Clear UART rx FIFO */
	uart_npcm4xx_clear_rx_fifo(dev);

	/* Configure UART interrupts */
	config->uconf.irq_config_func(dev);
#endif

	return 0;
}

#ifdef CONFIG_PM_DEVICE
static inline bool uart_npcm4xx_device_is_transmitting(const struct device *dev)
{
	if (IS_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN)) {
		/* The transmitted transaction is completed? */
		return !uart_npcm4xx_irq_tx_complete(dev);
	}

	/* No need for polling mode */
	return 0;
}

static inline int uart_npcm4xx_get_power_state(const struct device *dev, uint32_t *state)
{
	const struct uart_npcm4xx_data *const data = DRV_DATA(dev);

	*state = data->pm_state;
	return 0;
}

static inline int uart_npcm4xx_set_power_state(const struct device *dev, uint32_t next_state)
{
	struct uart_npcm4xx_data *const data = DRV_DATA(dev);

	/* If next device power state is LOW or SUSPEND power state */
	if (next_state == PM_DEVICE_STATE_LOW_POWER || next_state == PM_DEVICE_STATE_SUSPEND) {
		/*
		 * If uart device is busy with transmitting, the driver will
		 * stay in while loop and wait for the transaction is completed.
		 */
		while (uart_npcm4xx_device_is_transmitting(dev)) {
			continue;
		}
	}

	data->pm_state = next_state;
	return 0;
}

/* Implements the device power management control functionality */
static int uart_npcm4xx_pm_control(const struct device *dev, uint32_t ctrl_command, uint32_t *state,
				pm_device_cb cb, void *arg)
{
	int ret = 0;

	switch (ctrl_command) {
	case PM_DEVICE_STATE_SET:
		ret = uart_npcm4xx_set_power_state(dev, *state);
		break;
	case PM_DEVICE_STATE_GET:
		ret = uart_npcm4xx_get_power_state(dev, state);
		break;
	default:
		ret = -EINVAL;
	}

	if (cb != NULL) {
		cb(dev, ret, state, arg);
	}
	return ret;
}
#endif /* CONFIG_PM_DEVICE */

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
#define NPCM4XX_UART_IRQ_CONFIG_FUNC_DECL(inst)                                                    \
	static void uart_npcm4xx_irq_config_##inst(const struct device *dev)
#define NPCM4XX_UART_IRQ_CONFIG_FUNC_INIT(inst) .irq_config_func = uart_npcm4xx_irq_config_##inst,
#define NPCM4XX_UART_IRQ_CONFIG_FUNC(inst)                                                         \
	static void uart_npcm4xx_irq_config_##inst(const struct device *dev)                       \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), uart_npcm4xx_isr,     \
			    DEVICE_DT_INST_GET(inst), 0);                                          \
		irq_enable(DT_INST_IRQN(inst));                                                    \
	}
#else
#define NPCM4XX_UART_IRQ_CONFIG_FUNC_DECL(inst)
#define NPCM4XX_UART_IRQ_CONFIG_FUNC_INIT(inst)
#define NPCM4XX_UART_IRQ_CONFIG_FUNC(inst)
#endif

#define NPCM4XX_UART_INIT(inst)                                                                    \
	NPCM4XX_UART_IRQ_CONFIG_FUNC_DECL(inst);						   \
												   \
	static const struct uart_npcm4xx_config uart_npcm4xx_cfg_##inst = {                        \
		.uconf =                                                                           \
		{                                                                                  \
			.base = (uint8_t *)DT_INST_REG_ADDR(inst),                                 \
			NPCM4XX_UART_IRQ_CONFIG_FUNC_INIT(inst)					   \
		},                                                                                 \
		.clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(inst),                                          \
	};                                                                                         \
												   \
	static struct uart_npcm4xx_data uart_npcm4xx_data_##inst = {                               \
		.ucfg = { .baudrate = DT_INST_PROP(inst, current_speed) },                         \
	};                                                                                         \
												   \
	DEVICE_DT_INST_DEFINE(inst, &uart_npcm4xx_init, NULL, &uart_npcm4xx_data_##inst,           \
			      &uart_npcm4xx_cfg_##inst, PRE_KERNEL_1,                              \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &uart_npcm4xx_driver_api);	   \
												   \
NPCM4XX_UART_IRQ_CONFIG_FUNC(inst)

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_UART_INIT)
