/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_watchdog

#include <drivers/watchdog.h>
#include <soc.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(wdt_npcm4xx, CONFIG_WDT_LOG_LEVEL);

/* Watchdog operating frequency is fixed to LFCLK (32.768) kHz */
#define NPCM4XX_WDT_CLK LFCLK

/*
 * Maximum watchdog window time. Since the watchdog counter is 8-bits, maximum
 * time supported by npcm4xx watchdog is 256 * (1024 * 32768) / 32768 = 262144 sec.
 */
#define NPCM4XX_WDT_MAX_WND_TIME    250000000UL

/* Timeout for reloading and restarting Timer 0. (Unit:ms) */
#define NPCM4XX_T0CSR_RST_TIMEOUT   50

/* Timeout for stopping watchdog. (Unit:ms) */
#define NPCM4XX_WATCHDOG_STOP_TIMEOUT 1


/* Device config */
struct wdt_npcm4xx_config {
	uintptr_t base; /* wdt controller base address */
};

/* Driver data */
struct wdt_npcm4xx_data {
	/* Timeout callback used to handle watchdog event */
	wdt_callback_t cb;
	/* Watchdog feed timeout in milliseconds */
	uint32_t timeout;
	/* Indicate whether a watchdog timeout is installed */
	bool timeout_installed;
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct wdt_npcm4xx_config *)(dev)->config)
#define DRV_DATA(dev) ((struct wdt_npcm4xx_data *)(dev)->data)
#define HAL_INSTANCE(dev) (struct twd_reg *)(DRV_CONFIG(dev)->base)

/* WDT local inline functions */
static inline int wdt_t0out_reload(const struct device *dev)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);
	uint64_t st;

	/* Reload and restart T0 timer */
	inst->T0CSR |= BIT(TWD_T0CSR_WDRST_STS) | BIT(TWD_T0CSR_RST);
	/* Wait for timer is loaded and restart */
	st = k_uptime_get();
	while (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_RST)) {
		if (k_uptime_get() - st > NPCM4XX_T0CSR_RST_TIMEOUT) {
			/* RST bit is still set? */
			if (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_RST)) {
				LOG_ERR("Timeout: reload T0 timer!");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

static inline int wdt_wait_stopped(const struct device *dev)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);
	uint64_t st;

	st = k_uptime_get();
	/* If watchdog is still running? */
	while (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_WD_RUN)) {
		if (k_uptime_get() - st > NPCM4XX_WATCHDOG_STOP_TIMEOUT) {
			/* WD_RUN bit is still set? */
			if (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_WD_RUN)) {
				LOG_ERR("Timeout: stop watchdog timer!");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

static inline int wdt_convert_timeout(const struct device *dev)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);
	struct wdt_npcm4xx_data *const data = DRV_DATA(dev);
	uint32_t twd_div;
	uint32_t wdt_div;
	uint32_t twdt0;
	uint32_t wdcnt;
	int32_t rv;

	if (data->timeout < 1000) {
		/*
		 * Milliseconds
		 * - T0 Timer freq is LFCLK/32 Hz
		 * - Watchdog freq is LFCLK/32*32 Hz (ie. LFCLK/1024 Hz)
		 */
		inst->TWCP = 0x05;      /* Prescaler is 32 in T0 Timer */
		inst->WDCP = 0x05;      /* Prescaler is 32 in Watchdog Timer */

		/*
		 * One clock period of T0 timer is 32/32768 Hz = 0.976 ms.
		 * One clock period of watchdog is 32*32/32768 Hz = 31.25 ms.
		 */
		twd_div = (1 << inst->TWCP);
		wdt_div = (1 << inst->WDCP);
		twdt0 = MAX(ceiling_fraction((data->timeout * NPCM4XX_WDT_CLK),
				(twd_div * 1000)), 1);
		wdcnt = MIN(ceiling_fraction((data->timeout * NPCM4XX_WDT_CLK),
					     (twd_div * wdt_div * 1000)) +
			    CONFIG_WDT_NPCM4XX_DELAY_CYCLES, 0xFF);
	} else if ((data->timeout >= 1000) && (data->timeout < 60000)) {
		/*
		 * Seconds
		 * - T0 Timer freq is LFCLK/256 Hz
		 * - Watchdog freq is T0CLK/32 Hz (ie. LFCLK/8192 Hz)
		 */
		inst->TWCP = 0x08;      /* Prescaler is 256 in T0 Timer */
		inst->WDCP = 0x05;      /* Prescaler is 32 in Watchdog Timer */
		/*
		 * One clock period of T0 timer is 256/32768 Hz = 7.8125 ms.
		 * One clock period of watchdog is 256*32/32768 Hz = 250 ms.
		 */
		twd_div = (1 << inst->TWCP);
		wdt_div = (1 << inst->WDCP);
		twdt0 = MAX(ceiling_fraction((data->timeout * NPCM4XX_WDT_CLK),
				(twd_div * 1000)), 1);
		wdcnt = MIN(ceiling_fraction((data->timeout * NPCM4XX_WDT_CLK),
				(twd_div * wdt_div * 1000)) + CONFIG_WDT_NPCM4XX_DELAY_CYCLES,
				0xFF);
	} else if ((data->timeout >= 60000) && (data->timeout < 1800000)) {
		/*
		 * Minutes
		 * - T0 Timer freq is LFCLK/1024 Hz
		 * - Watchdog freq is T0CLK/256 Hz (ie. LFCLK/262144 Hz)
		 */
		inst->TWCP = 0x0A;      /* Prescaler is 1024 in T0 Timer */
		inst->WDCP = 0x08;      /* Prescaler is 256 in Watchdog Timer */
		/*
		 * One clock period of T0 timer is 1024/32768 Hz = 31.25 ms.
		 * One clock period of watchdog is 1024*256/32768 Hz = 8 sec.
		 */
		twd_div = (1 << inst->TWCP);
		wdt_div = (1 << inst->WDCP);
		twdt0 = MIN(ceiling_fraction(data->timeout, 1000) *
			    ceiling_fraction(NPCM4XX_WDT_CLK, twd_div), 0xFFFF);
		wdcnt = MIN(ceiling_fraction(data->timeout,
				ceiling_fraction(twd_div * wdt_div * 1000, NPCM4XX_WDT_CLK)) +
			    CONFIG_WDT_NPCM4XX_DELAY_CYCLES, 0xFF);
	} else if ((data->timeout >= 1800000) && (data->timeout < NPCM4XX_WDT_MAX_WND_TIME)) {
		/*
		 * Half hour
		 * - T0 Timer freq is LFCLK/1024 Hz
		 * - Watchdog freq is T0CLK/32768 Hz (ie. LFCLK/33554432 Hz)
		 */
		inst->TWCP = 0x0A;      /* Prescaler is 1024 in T0 Timer */
		inst->WDCP = 0x0F;      /* Prescaler is 32768 in Watchdog Timer */
		/*
		 * One clock period of T0 timer is 1024/32768 Hz = 31.25 ms.
		 * One clock period of watchdog is 1024*32768/32768 Hz = 1024 sec.
		 */
		twd_div = (1 << inst->TWCP);
		wdt_div = (1 << inst->WDCP);
		twdt0 = MIN(ceiling_fraction(data->timeout, 1000) *
			    ceiling_fraction(NPCM4XX_WDT_CLK, twd_div), 0xFFFF);
		wdcnt = MIN(ceiling_fraction(data->timeout, twd_div * 1000), 0xFF);
	} else {
		/* should not be here */
		LOG_ERR("WDT timeout value is invalid!");
		return -EOVERFLOW;
	}

	/* Configure 16-bit timer0 counter */
	inst->TWDT0 = twdt0;
	/* Reload and restart T0 timer */
	rv = wdt_t0out_reload(dev);
	if (rv != 0) {
		return -ETIMEDOUT;
	}

	/* Configure 8-bit watchdog counter */
	inst->WDCNT = wdcnt;

	LOG_DBG("WDT setup: TWDT0, WDCNT are %d, %d", inst->TWDT0, inst->WDCNT);

	return 0;
}

/* WDT local functions */
static void wdt_t0out_isr(const struct device *dev)
{
	struct wdt_npcm4xx_data *const data = DRV_DATA(dev);

	LOG_DBG("WDT reset will issue after %d delay cycle!",
		CONFIG_WDT_NPCM4XX_DELAY_CYCLES);

	/* Handle watchdog event here. */
	if (data->cb) {
		data->cb(dev, 0);
	}
}

/* WDT api functions */
static int wdt_npcm4xx_setup(const struct device *dev, uint8_t options)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);
	struct wdt_npcm4xx_data *const data = DRV_DATA(dev);
	int32_t rv;

	/* Disable irq of t0-out expired event first */
	irq_disable(DT_INST_IRQN(0));

	if (data->timeout_installed == false) {
		LOG_ERR("No valid WDT timeout installed");
		return -EINVAL;
	}

	if (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_WD_RUN)) {
		LOG_ERR("WDT timer is busy");
		return -EBUSY;
	}

	if (IS_BIT_SET(options, WDT_OPT_PAUSE_IN_SLEEP) != 0) {
		LOG_ERR("WDT_OPT_PAUSE_IN_SLEEP is not supported");
		return -ENOTSUP;
	}

	if (IS_BIT_SET(options, WDT_OPT_PAUSE_HALTED_BY_DBG) != 0) {
		LOG_ERR("WDT_OPT_PAUSE_HALTED_BY_DBG is not supported");
		return -ENOTSUP;
	}

	/* Convert timeout */
	rv = wdt_convert_timeout(dev);
	if (rv != 0) {
		return -ETIMEDOUT;
	}

	/* Configure t0 timer interrupt and its isr. */
	/* Configure twd ISR */
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
			wdt_t0out_isr, DEVICE_DT_INST_GET(0), 0);
	/* Enable twd interrupt */
	irq_enable(DT_INST_IRQN(0));

	/* Enable TWD */
	inst->T0CSR |= BIT(TWD_T0CSR_T0EN);

	return 0;
}

static int wdt_npcm4xx_disable(const struct device *dev)
{
	struct wdt_npcm4xx_data *const data = DRV_DATA(dev);
	struct twd_reg *const inst = HAL_INSTANCE(dev);

	/*
	 * Stop and unlock watchdog by writing 87h, 61h and 63h
	 * sequence bytes to WDSDM register
	 */
	inst->WDSDM = 0x87;
	inst->WDSDM = 0x61;
	inst->WDSDM = 0x63;

	/* Disable irq of t0-out expired event and mark it uninstalled */
	irq_disable(DT_INST_IRQN(0));
	data->timeout_installed = false;

	/* Wait for watchdof is stopped. */
	return wdt_wait_stopped(dev);
}

static int wdt_npcm4xx_install_timeout(const struct device *dev,
				       const struct wdt_timeout_cfg *cfg)
{
	struct wdt_npcm4xx_data *const data = DRV_DATA(dev);
	struct twd_reg *const inst = HAL_INSTANCE(dev);

	/* If watchdog is already running */
	if (IS_BIT_SET(inst->T0CSR, TWD_T0CSR_WD_RUN)) {
		return -EBUSY;
	}

	/* No window watchdog support */
	if (cfg->window.min != 0) {
		data->timeout_installed = false;
		return -EINVAL;
	}

	/*
	 * Since the watchdog counter in npcx series is 8-bits, maximum time
	 * supported by it is 256 * (32 * 32) / 32768 = 8 sec. This makes the
	 * allowed range of 1-8000 in milliseconds. Check if the provided value
	 * is within this range.
	 */
	if (cfg->window.max > NPCM4XX_WDT_MAX_WND_TIME || cfg->window.max == 0) {
		data->timeout_installed = false;
		return -EINVAL;
	}

	/* Save watchdog timeout */
	data->timeout = cfg->window.max;

	/* Install user timeout isr */
	data->cb = cfg->callback;
	data->timeout_installed = true;

	return 0;
}

static int wdt_npcm4xx_feed(const struct device *dev, int channel_id)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);

	ARG_UNUSED(channel_id);

	/* Feed watchdog by writing 5Ch to WDSDM */
	inst->WDSDM = 0x5C;

	/* Reload and restart T0 timer */
	return wdt_t0out_reload(dev);

	return 0;
}

static int wdt_npcm4xx_init(const struct device *dev)
{
	struct twd_reg *const inst = HAL_INSTANCE(dev);

	/* Feed watchdog by writing 5Ch, Select T0IN as clock */
	inst->TWCFG |= BIT(TWD_TWCFG_WDSDME) | BIT(TWD_TWCFG_WDCT0I);
	/* Disable early touch functionality */
	inst->T0CSR |= BIT(TWD_T0CSR_TESDIS) | BIT(TWD_T0CSR_WDRST_STS);

	return 0;
}

static const struct wdt_npcm4xx_config wdt_npcm4xx_cfg_0 = {
	.base = DT_INST_REG_ADDR(0),
};

static struct wdt_npcm4xx_data wdt_npcm4xx_data_0;

static const struct wdt_driver_api wdt_npcm4xx_driver_api = {
	.setup = wdt_npcm4xx_setup,
	.disable = wdt_npcm4xx_disable,
	.install_timeout = wdt_npcm4xx_install_timeout,
	.feed = wdt_npcm4xx_feed,
};

DEVICE_DT_INST_DEFINE(0, wdt_npcm4xx_init, NULL,
		      &wdt_npcm4xx_data_0, &wdt_npcm4xx_cfg_0,
		      PRE_KERNEL_1,
		      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		      &wdt_npcm4xx_driver_api);
