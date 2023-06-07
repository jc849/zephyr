/*
 * Copyright (c) 2020 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_miwu

/**
 * @file
 * @brief Nuvoton NPCM4XX MIWU driver
 *
 * The device Multi-Input Wake-Up Unit (MIWU) supports NPCM4XX
 * to exit 'Sleep' or 'Deep Sleep' power state.
 * Also, it provides signal conditioning such as
 * 'Level' and 'Edge' trigger type and grouping of external interrupt sources
 * of NVIC. The NPCM4XX series has three identical MIWU modules: MIWU0, MIWU1,
 * MIWU2. Together, they support a total of over 143 internal and/or external
 * wake-up input (WUI) sources.
 *
 * This driver uses device tree files to present the relationship bewteen
 * MIWU and the other devices in npcm4xx target.
 * it include:
 *  1. npcm4xx-miwus-wui-map.dtsi: it presents relationship between wake-up inputs
 *     (WUI) and its source device such as gpio, timer, eSPI VWs and so on.
 *  2. npcm4xx-miwus-int-map.dtsi: it presents relationship between MIWU group
 *     and NVIC interrupt in npcm4xx target. Basically, it's a 1-to-1 mapping.
 *     There is a group which has 2 interrupts as an exception.
 *
 * INCLUDE FILES: soc_miwu.h
 *
 */

#include <device.h>
#include <kernel.h>
#include <soc.h>
#include <sys/__assert.h>
#include <irq_nextlevel.h>
#include <drivers/gpio.h>

#include "soc_miwu.h"
#include "soc_gpio.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(intc_npcm4xx_miwu, LOG_LEVEL_ERR);

/* MIWU module instances forward declaration */
static const struct device *miwu_devs[];

/* Driver config */
struct intc_miwu_config {
	/* miwu controller base address */
	uintptr_t base;
	/* index of miwu controller */
	uint8_t index;
};

/* Callback functions list for GPIO wake-up inputs */
sys_slist_t cb_list_gpio;

/*
 * Callback functions list for the generic hardware modules  wake-up inputs
 * such as timer, uart, i2c, host interface and so on.
 */
sys_slist_t cb_list_generic;

/* Driver convenience defines */
#define DRV_CONFIG(dev)	\
	((const struct intc_miwu_config *)(dev)->config)

BUILD_ASSERT(sizeof(struct miwu_io_callback) == sizeof(struct gpio_callback),
	     "Size of struct miwu_io_callback must equal to struct gpio_callback");

BUILD_ASSERT(sizeof(struct miwu_io_params) == sizeof(gpio_port_pins_t),
	     "Size of struct miwu_io_params must equal to struct gpio_port_pins_t");

/* MIWU local functions */
static void intc_miwu_dispatch_gpio_isr(uint8_t wui_table,
					uint8_t wui_group, uint8_t wui_bit)
{
	struct miwu_io_callback *cb, *tmp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&cb_list_gpio, cb, tmp, node) {
		/* Pending bit, group and table match the wui item in list */
		if (cb->params.wui.table == wui_table
		    && cb->params.wui.group == wui_group
		    && cb->params.wui.bit == wui_bit) {
			__ASSERT(cb->handler, "No GPIO callback handler!");
			/*
			 * Execute GPIO callback and the other callback might
			 * match the same wui item.
			 */
			/* implemented by the gpio driver
			 * cb->handler(npcm4xx_get_gpio_dev(cb->params.gpio_port),
			 *		(struct gpio_callback *)cb,
			 *		cb->params.pin_mask);
			 */
		}
	}
}

static void intc_miwu_dispatch_generic_isr(uint8_t wui_table,
					   uint8_t wui_group, uint8_t wui_bit)
{
	struct miwu_dev_callback *cb, *tmp;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&cb_list_generic, cb, tmp, node) {
		/* Pending bit, group and table match the wui item in list */
		if (cb->wui.table == wui_table
		    && cb->wui.group == wui_group
		    && cb->wui.bit == wui_bit) {
			__ASSERT(cb->handler, "No Generic callback handler!");
			/*
			 * Execute generic callback and the other callback might
			 * match the same wui item.
			 */
			cb->handler(cb->source, &cb->wui);
		}
	}
}

static void intc_miwu_isr_pri(int wui_table, int wui_group)
{
	int wui_bit;
	const uint32_t base = DRV_CONFIG(miwu_devs[wui_table])->base;
	uint8_t mask = NPCM4XX_WKPND(base, wui_group) & NPCM4XX_WKEN(base, wui_group);

	/* Clear pending bits before dispatch ISR */
	if (mask) {
		NPCM4XX_WKPCL(base, wui_group) = mask;
	}

	for (wui_bit = 0; wui_bit < 8; wui_bit++) {
		if (mask & BIT(wui_bit)) {
			LOG_DBG("miwu_isr %d %d %d!\n", wui_table,
				wui_group, wui_bit);
			/* Dispatch registed gpio and generic isrs */
			intc_miwu_dispatch_gpio_isr(wui_table,
						    wui_group, wui_bit);
			intc_miwu_dispatch_generic_isr(wui_table,
						       wui_group, wui_bit);
		}
	}
}

/* Platform specific MIWU functions */
void npcm4xx_miwu_irq_enable(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;

	NPCM4XX_WKEN(base, wui->group) |= BIT(wui->bit);
}

void npcm4xx_miwu_irq_disable(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;

	NPCM4XX_WKEN(base, wui->group) &= ~BIT(wui->bit);
}

void npcm4xx_miwu_io_enable(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;

	NPCM4XX_WKINEN(base, wui->group) |= BIT(wui->bit);
}

void npcm4xx_miwu_io_disable(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;

	NPCM4XX_WKINEN(base, wui->group) &= ~BIT(wui->bit);
}

bool npcm4xx_miwu_irq_get_state(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;

	return IS_BIT_SET(NPCM4XX_WKEN(base, wui->group), wui->bit);
}

bool npcm4xx_miwu_irq_get_and_clear_pending(const struct npcm4xx_wui *wui)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;
	bool pending = IS_BIT_SET(NPCM4XX_WKPND(base, wui->group), wui->bit);

	if (pending) {
		NPCM4XX_WKPCL(base, wui->group) = BIT(wui->bit);
	}

	return pending;
}

int npcm4xx_miwu_interrupt_configure(const struct npcm4xx_wui *wui,
				     enum miwu_int_mode mode, enum miwu_int_trig trig)
{
	const uint32_t base = DRV_CONFIG(miwu_devs[wui->table])->base;
	uint8_t pmask = BIT(wui->bit);

	/* Disable interrupt of wake-up input source before configuring it */
	npcm4xx_miwu_irq_disable(wui);

	/* Handle interrupt for level trigger */
	if (mode == NPCM4XX_MIWU_MODE_LEVEL) {
		/* Set detection mode to level */
		NPCM4XX_WKMOD(base, wui->group) |= pmask;
		switch (trig) {
		/* Enable interrupting on level high */
		case NPCM4XX_MIWU_TRIG_HIGH:
			NPCM4XX_WKEDG(base, wui->group) &= ~pmask;
			break;
		/* Enable interrupting on level low */
		case NPCM4XX_MIWU_TRIG_LOW:
			NPCM4XX_WKEDG(base, wui->group) |= pmask;
			break;
		default:
			return -EINVAL;
		}
		/* Handle interrupt for edge trigger */
	} else {
		/* Set detection mode to edge */
		NPCM4XX_WKMOD(base, wui->group) &= ~pmask;
		switch (trig) {
		/* Handle interrupting on falling edge */
		case NPCM4XX_MIWU_TRIG_LOW:
			NPCM4XX_WKAEDG(base, wui->group) &= ~pmask;
			NPCM4XX_WKEDG(base, wui->group) |= pmask;
			break;
		/* Handle interrupting on rising edge */
		case NPCM4XX_MIWU_TRIG_HIGH:
			NPCM4XX_WKAEDG(base, wui->group) &= ~pmask;
			NPCM4XX_WKEDG(base, wui->group) &= ~pmask;
			break;
		/* Handle interrupting on both edges */
		case NPCM4XX_MIWU_TRIG_BOTH:
			/* Enable any edge */
			NPCM4XX_WKAEDG(base, wui->group) |= pmask;
			break;
		default:
			return -EINVAL;
		}
	}

	/* Enable wake-up input sources */
	NPCM4XX_WKINEN(base, wui->group) |= pmask;

	/*
	 * Clear pending bit since it might be set if WKINEN bit is
	 * changed.
	 */
	NPCM4XX_WKPCL(base, wui->group) |= pmask;

	return 0;
}

void npcm4xx_miwu_init_gpio_callback(struct miwu_io_callback *callback,
				     const struct npcm4xx_wui *io_wui, int port)
{
	/* Initialize WUI and GPIO settings in unused bits field */
	callback->params.wui.table = io_wui->table;
	callback->params.wui.group = io_wui->group;
	callback->params.wui.bit = io_wui->bit;
	callback->params.gpio_port = port;
}

void npcm4xx_miwu_init_dev_callback(struct miwu_dev_callback *callback,
				    const struct npcm4xx_wui *dev_wui,
				    miwu_dev_callback_handler_t handler,
				    const struct device *source)
{
	/* Initialize WUI and input device settings */
	callback->wui.table = dev_wui->table;
	callback->wui.group = dev_wui->group;
	callback->wui.bit = dev_wui->bit;
	callback->handler = handler;
	callback->source = source;
}

int npcm4xx_miwu_manage_gpio_callback(struct miwu_io_callback *cb, bool set)
{
	if (!sys_slist_is_empty(&cb_list_gpio)) {
		if (!sys_slist_find_and_remove(&cb_list_gpio, &cb->node)) {
			if (!set) {
				return -EINVAL;
			}
		}
	}

	if (set) {
		sys_slist_prepend(&cb_list_gpio, &cb->node);
	}

	return 0;
}

int npcm4xx_miwu_manage_dev_callback(struct miwu_dev_callback *cb, bool set)
{
	if (!sys_slist_is_empty(&cb_list_generic)) {
		if (!sys_slist_find_and_remove(&cb_list_generic, &cb->node)) {
			if (!set) {
				return -EINVAL;
			}
		}
	}

	if (set) {
		sys_slist_prepend(&cb_list_generic, &cb->node);
	}

	return 0;
}

/* MIWU driver registration */
#define NPCM4XX_MIWU_ISR_FUNC(index) _CONCAT(intc_miwu_isr, index)
#define NPCM4XX_MIWU_INIT_FUNC(inst) _CONCAT(intc_miwu_init, inst)
#define NPCM4XX_MIWU_INIT_FUNC_DECL(inst) \
	static int intc_miwu_init##inst(const struct device *dev)

/* MIWU ISR implementation */
#define NPCM4XX_MIWU_ISR_FUNC_IMPL(inst)			   \
	static void intc_miwu_isr##inst(void *arg)		   \
	{							   \
		uint8_t grp_mask = (uint32_t)arg;		   \
		int group = 0;					   \
								   \
		/* Check all MIWU groups belong to the same irq */ \
		do {						   \
			if (grp_mask & 0x01) {			   \
				intc_miwu_isr_pri(inst, group); }  \
			group++;				   \
			grp_mask = grp_mask >> 1;		   \
								   \
		} while (grp_mask != 0);			   \
	}

/* MIWU init function implementation */
#define NPCM4XX_MIWU_INIT_FUNC_IMPL(inst)				      \
	static int intc_miwu_init##inst(const struct device *dev)	      \
	{								      \
		int i;							      \
		const uint32_t base = DRV_CONFIG(dev)->base;		      \
									      \
		/* Clear all MIWUs' pending and enable bits of MIWU device */ \
		for (i = 0; i < NPCM4XX_MIWU_GROUP_COUNT; i++) {	      \
			NPCM4XX_WKEN(base, i) = 0;			      \
			NPCM4XX_WKPCL(base, i) = 0xFF;			      \
		}							      \
									      \
		/* Config IRQ and MWIU group directly */		      \
		DT_FOREACH_CHILD(NPCM4XX_DT_NODE_FROM_MIWU_MAP(inst),	      \
				 NPCM4XX_DT_MIWU_IRQ_CONNECT_IMPL_CHILD_FUNC) \
		return 0;						      \
	}								      \

#define NPCM4XX_MIWU_INIT(inst)						  \
	NPCM4XX_MIWU_INIT_FUNC_DECL(inst);				  \
									  \
	static const struct intc_miwu_config miwu_config_##inst = {	  \
		.base = DT_REG_ADDR(DT_NODELABEL(miwu##inst)),		  \
		.index = DT_PROP(DT_NODELABEL(miwu##inst), index),	  \
	};								  \
									  \
	DEVICE_DT_INST_DEFINE(inst,					  \
			      NPCM4XX_MIWU_INIT_FUNC(inst),		  \
			      NULL,					  \
			      NULL, &miwu_config_##inst,		  \
			      PRE_KERNEL_1,				  \
			      CONFIG_KERNEL_INIT_PRIORITY_OBJECTS, NULL); \
									  \
	NPCM4XX_MIWU_ISR_FUNC_IMPL(inst)				  \
									  \
	NPCM4XX_MIWU_INIT_FUNC_IMPL(inst)

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_MIWU_INIT)

/* MIWU module instances */
#define NPCM4XX_MIWU_DEV(inst) DEVICE_DT_INST_GET(inst),

static const struct device *miwu_devs[] = {
	DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_MIWU_DEV)
};

BUILD_ASSERT(ARRAY_SIZE(miwu_devs) == NPCM4XX_MIWU_TABLE_COUNT,
	     "Size of miwu_devs array must equal to NPCM4XX_MIWU_TABLE_COUNT");
