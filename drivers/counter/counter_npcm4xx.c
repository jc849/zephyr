/*
 * Copyright (c) 2017 - 2018, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <drivers/counter.h>
#include <common/reg/reg_def.h>
#include <common/reg/reg_access.h>
#include <drivers/clock_control.h>
#include <soc.h>
#include "NPCM4XX.h"


#define DT_DRV_COMPAT nuvoton_npcm4xx_itim_timer

#include <logging/log.h>
LOG_MODULE_REGISTER(itim, LOG_LEVEL_ERR);

#define ITIM32_MAX_CYCLE_TIME_MS    200000
#define ITIM32_MAX_CYCLE_TIME_US    200000

#define DEV_DATA(dev) ((struct counter_npcm4xx_data *)(dev)->data)

static struct itim32_reg *const ITIM32_1 = (struct itim32_reg *)
					   DT_INST_REG_ADDR_BY_NAME(0, itim1);

/* source clock */
enum ITIM32_SOURCE_CLOCK {
	ITIM32_SOURCE_CLOCK_APB2        = 0,
	ITIM32_SOURCE_CLOCK_32K         = 1
};

/* itim32 return code*/
enum  ITIM32_RET {
	ITIM32_RET_OK   = 0,
	ITIM32_RET_FAIL = 1
};

struct counter_npcm4xx_config {
	struct counter_config_info counter_info;
};

struct counter_npcm4xx_data {
	counter_alarm_callback_t alarm_cb;
	uint32_t alarm_tick;
	void *alarm_user_data;
	counter_top_callback_t top_cb;
	uint32_t top_tick;
	void *top_user_data;
	uint32_t npcm4xx_tick;
};

/*-------------------------------------------------------------------------------*/
/**
 * @brief               Set PRE and CNT registers by milliseconds
 * @param [in]          itim32			The timer module (1 ~ 6)
 * @param [in]          clock			The timer source clock (32K or APB2)
 * @param [in]          millisec		Millisecond unit
 * @return							The timer return code (OK or FAIL)
 **
 * @details					    static function called by ITIM32_StartTimer
 */
/*-------------------------------------------------------------------------------*/
enum ITIM32_RET ITIM32_SetRegsByMicroSec(enum ITIM32_SOURCE_CLOCK clock, uint32_t microsec)
{
	uint32_t time_base, cnt, pre, timer_apb2;
	const struct device *const clk_dev = device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	struct npcm4xx_clk_cfg clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(0);
	int ret;

	ret = clock_control_on(clk_dev, (clock_control_subsys_t *)&clk_cfg);

	if (ret < 0) {
		LOG_ERR("Turn on ITIM clock fail %d", ret);
		return ret;
	}

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t *)
				     &clk_cfg, &timer_apb2);

	if (ret < 0) {
		LOG_ERR("Get ITIM clock rate error %d", ret);
		return ret;
	}
	/* Avoid overflow */
	if ((microsec == 0) || (microsec > ITIM32_MAX_CYCLE_TIME_MS)) {
		return ITIM32_RET_FAIL;
	}


	/* Cycle time = (PRE_8 + 1) x (CNT_32 + 1) x TCLK. */
	/* => CLK = PRE * CNT */
	pre = 1;
	time_base = (timer_apb2) / pre;
	cnt = (time_base / 1000000) * microsec;

	/* Set PRE and CNT registers */
	ITIM32_1->PRE = pre - 1;
	ITIM32_1->CNT32 = cnt - 1;

	return ITIM32_RET_OK;
}

/*----------------------------------------------------------*/
/**
 * @brief                                                       Timer starts running
 * @param [in]          itim32			The timer module (1 ~ 6)
 * @return							none
 */
/*----------------------------------------------------------*/
static int counter_TimerRun(const struct device *dev)
{
	ARG_UNUSED(dev);
	/* Enable interrupt */
	ITIM32_1->CTS |= MaskBit(ITIM32_CTS_TO_IE);

	/* Enable the module */
	ITIM32_1->CTS |= MaskBit(ITIM32_CTS_ITEN);

	return ITIM32_RET_OK;
}

/*-----------------------------------------------------------------------*/
/**
 * @brief                                                       Stop the timer module
 * @param [in]          itim32			The timer module (1 ~ 6)
 * @return							The timer return code (OK or FAIL)
 */
/*-----------------------------------------------------------------------*/
static int counter_StopTimer(const struct device *dev)
{
	ARG_UNUSED(dev);
	/* Disable the module */
	ITIM32_1->CTS &= ~(MaskBit(ITIM32_CTS_ITEN));

	/* Waiting until the module is disabled (can take several clocks) */
	while (IsRegBitSet(ITIM32_1->CTS, MaskBit(ITIM32_CTS_ITEN), MaskBit(ITIM32_CTS_ITEN))) {
	}

	/* Disable interrupt */
	ITIM32_1->CTS &= ~(MaskBit(ITIM32_CTS_TO_IE));

	return ITIM32_RET_OK;
}

static int counter_getvalue(const struct device *dev, uint32_t *ticks)
{
	*ticks = DEV_DATA(dev)->npcm4xx_tick;
	return ITIM32_RET_OK;
}

static int counter_setalarm(const struct device *dev, uint8_t chan_id,
			    const struct counter_alarm_cfg *alarm_cfg)
{
	ARG_UNUSED(chan_id);

	if ((alarm_cfg->flags & COUNTER_ALARM_CFG_ABSOLUTE) != 0) {
		DEV_DATA(dev)->alarm_tick = alarm_cfg->ticks;
	} else {
		DEV_DATA(dev)->alarm_tick = alarm_cfg->ticks + DEV_DATA(dev)->npcm4xx_tick;
	}

	DEV_DATA(dev)->alarm_cb = alarm_cfg->callback;
	DEV_DATA(dev)->alarm_user_data = alarm_cfg->user_data;
	return ITIM32_RET_OK;
}

static int counter_cancelalarm(const struct device *dev, uint8_t chan_id)
{
	ARG_UNUSED(chan_id);

	DEV_DATA(dev)->alarm_cb = NULL;
	DEV_DATA(dev)->alarm_user_data = NULL;

	LOG_DBG("%p Counter alarm canceled", dev);
	return ITIM32_RET_OK;
}

static int counter_settopvalue(const struct device *dev,
			       const struct counter_top_cfg *cfg)
{
	DEV_DATA(dev)->top_cb = cfg->callback;
	DEV_DATA(dev)->top_user_data = cfg->user_data;
	DEV_DATA(dev)->top_tick = cfg->ticks;

	return ITIM32_RET_OK;
}

static uint32_t counter_getpendingint(const struct device *dev)
{
	return ITIM32_RET_OK;
}

static uint32_t counter_gettopvalue(const struct device *dev)
{
	return DEV_DATA(dev)->top_tick;
}

/*-----------------------------------------------------------------------*/
/**
 * @brief                                                       Configure the timer module
 * @param [in]          itim32			The timer module (1 ~ 6)
 * @param [in]          clock			The timer source clock (32K or APB2)
 * @param [in]          millisec		Millisecond unit
 * @param [in]          callback_func	The callback function
 * @return							The timer return code (OK or FAIL)
 */
/*-----------------------------------------------------------------------*/
enum ITIM32_RET ITIM32_TimerConfig(enum ITIM32_SOURCE_CLOCK clock, uint32_t microsec)
{
	/* Stop timer first */
	if (counter_StopTimer(ITIM32_1) != ITIM32_RET_OK) {
		return ITIM32_RET_FAIL;
	}

	/* Select source clock */
	if (clock == ITIM32_SOURCE_CLOCK_APB2) {
		RegClrBit(ITIM32_1->CTS, MaskBit(ITIM32_CTS_CKSEL));
	} else if (clock == ITIM32_SOURCE_CLOCK_32K) {
		RegSetBit(ITIM32_1->CTS, MaskBit(ITIM32_CTS_CKSEL));
	}

	/* Set PRE and CNT registers */
	if (ITIM32_SetRegsByMicroSec(clock, microsec) != ITIM32_RET_OK) {
		return ITIM32_RET_FAIL;
	}

	return ITIM32_RET_OK;
}

static void NPCM4XX_itim_isr(const struct device *dev)
{
	static struct itim32_reg *const ITIM32_1 = (struct itim32_reg *)
						   DT_INST_REG_ADDR_BY_NAME(0, itim1);

	/* clear sts */
	RegSetBit(ITIM32_1->CTS, MaskBit(ITIM32_CTS_TO_STS));

	DEV_DATA(dev)->npcm4xx_tick++;

	/* top value isr */
	if (DEV_DATA(dev)->npcm4xx_tick == DEV_DATA(dev)->top_tick) {
		if (DEV_DATA(dev)->top_cb != NULL) {
			DEV_DATA(dev)->top_cb(dev, DEV_DATA(dev)->top_user_data);
		}
	}
	/* alarm isr */
	if (DEV_DATA(dev)->npcm4xx_tick == DEV_DATA(dev)->alarm_tick) {
		if (DEV_DATA(dev)->alarm_cb != NULL) {
			DEV_DATA(dev)->alarm_cb(dev, 0, DEV_DATA(dev)->npcm4xx_tick,
						DEV_DATA(dev)->alarm_user_data);
		}
	}
}

/* ---------------------------------*/
/* (ITIM32_1) Timer initial         */
/* ---------------------------------*/
static int Timer_Init(const struct device *dev)
{
	static struct itim32_reg *const ITIM32_1 = (struct itim32_reg *)
						   DT_INST_REG_ADDR_BY_NAME(0, itim1);

	DEV_DATA(dev)->npcm4xx_tick = 0;

	/* Create Timer 10ms */
	ITIM32_TimerConfig(ITIM32_SOURCE_CLOCK_APB2, 10000);
	/* Enable timeout wake-up */
	RegSetBit(ITIM32_1->CTS, MaskBit(ITIM32_CTS_TO_WUE));

	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		    NPCM4XX_itim_isr, DEVICE_DT_INST_GET(0), 0);
	irq_enable(DT_INST_IRQN(0));

	return ITIM32_RET_OK;

}

static const struct counter_driver_api counter_npcm4xx_api = {
	.start = counter_TimerRun,
	.stop = counter_StopTimer,
	.get_value = counter_getvalue,
	.set_alarm = counter_setalarm,
	.cancel_alarm = counter_cancelalarm,
	.set_top_value = counter_settopvalue,
	.get_pending_int = counter_getpendingint,
	.get_top_value = counter_gettopvalue,
};

static struct counter_npcm4xx_data counter_npcm4xx_data;
static const struct counter_npcm4xx_config counter_npcm4xx_config = {
	.counter_info = {
		.max_top_value = UINT32_MAX,
		.freq = 100,
		.flags = COUNTER_CONFIG_INFO_COUNT_UP,
		.channels = 1
	}
};

DEVICE_DT_INST_DEFINE(0, Timer_Init, NULL,
		      &counter_npcm4xx_data, &counter_npcm4xx_config,
		      PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      &counter_npcm4xx_api);
