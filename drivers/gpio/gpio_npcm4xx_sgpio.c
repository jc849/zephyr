/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_sgpio

#include <kernel.h>
#include <device.h>
#include <drivers/clock_control.h>
#include <drivers/gpio.h>
#include <soc.h>

#include "gpio_utils.h"
#include "soc_gpio.h"
#include "soc_miwu.h"
#include "sig_id.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(sgpio_npcm4xx);

typedef void (*sgpio_irq_config_func_t)(const struct device *dev);

#define IXOEVCFG_MASK 0x3
#define IXOEVCFG_BOTH 0x3
#define IXOEVCFG_FALLING 0x2
#define IXOEVCFG_RISING 0x1

static const uint8_t npcm4xx_CLK_SEL[] = {
	0x00, 0x05, 0x06, 0x07, 0x0C, 0x0D
};

static const int npcm4xx_SFT_CLK[] = {
	1024, 32, 16, 8, 4, 3
};

/* Driver config */
struct device_array {
	const struct device *dev;
};

struct sgpio_npcm4xx_parent_config {
	/* SGPIO controller base address */
	struct sgpio_reg *base;
	struct device_array *child_dev;
	uint32_t child_num;
	/* clock configuration */
	struct npcm4xx_clk_cfg clk_cfg;
	sgpio_irq_config_func_t irq_conf_func;
	struct k_spinlock lock;
	uint32_t sgpio_freq;
	uint8_t nin_sgpio;
	uint8_t nout_sgpio;
	uint8_t in_port;
	uint8_t out_port;
};

struct sgpio_npcm4xx_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	/* Parent device handler for shared resource */
	const struct device *parent;
	/* SGPIO controller base address */
	struct sgpio_reg *base;
	uint8_t pin_offset;
};

/* Driver data */
struct sgpio_npcm4xx_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	/* list of callbacks */
	sys_slist_t cb;
};

struct npcm4xx_scfg_config {
        /* scfg device base address */
        uintptr_t base_scfg;
};

static const struct npcm4xx_scfg_config npcm4xx_scfg_cfg = {
        .base_scfg = DT_REG_ADDR_BY_NAME(DT_NODELABEL(scfg), scfg),
};

/* Driver convenience defines */
#define DRV_PARENT_CFG(dev) ((struct sgpio_npcm4xx_parent_config *)(dev)->config)

#define DRV_CONFIG(dev) ((struct sgpio_npcm4xx_config *)(dev)->config)

#define DRV_DATA(dev) ((struct sgpio_npcm4xx_data *)(dev)->data)

#define HAL_PARENT_INSTANCE(dev) (struct sgpio_reg *)(DRV_PARENT_CFG(dev)->base)

#define HAL_INSTANCE(dev) (struct sgpio_reg *)(DRV_CONFIG(dev)->base)

#define HAL_SFCG_INST() (struct scfg_reg *)(npcm4xx_scfg_cfg.base_scfg)


static void npcm_sgpio_setup_enable(const struct device *parent, bool enable)
{
	struct sgpio_reg *const inst = HAL_PARENT_INSTANCE(parent);
	uint8_t reg = 0;

	reg = inst->IOXCTS;
	reg = reg & ~NPCM4XX_IOXCTS_RD_MODE_MASK;
	reg |= NPCM4XX_IOXCTS_RD_MODE_CONTINUOUS;

	if (enable) {
		reg |= NPCM4XX_IOXCTS_IOXIF_EN;
		inst->IOXCTS = reg;
	} else {
		reg &= ~NPCM4XX_IOXCTS_IOXIF_EN;
		inst->IOXCTS = reg;
	}
}

static int npcm_sgpio_init_port(const struct device *parent)
{
	uint8_t in_port, out_port, set_port, reg;
	struct sgpio_npcm4xx_parent_config *const config = DRV_PARENT_CFG(parent);
	struct sgpio_reg *const inst = HAL_PARENT_INSTANCE(parent);

	in_port = config->nin_sgpio / 8;
	if((config->nin_sgpio % 8) > 0 )
		in_port++;

	out_port = config->nout_sgpio / 8;
	if((config->nout_sgpio % 8) > 0 )
		out_port++;

	config->in_port = in_port;
	config->out_port = out_port;

	set_port = ((out_port & 0xf) << 4) | (in_port & 0xf);
	inst->IOXCFG2 = set_port;
	reg = inst->IOXCFG2;

	if(reg == set_port)
		return 0;

	return -1;
}

static int sgpio_npcm4xx_port_get_raw(const struct device *dev,
				  gpio_port_value_t *value)
{
	struct sgpio_reg *inst = HAL_INSTANCE(dev);

	uint8_t pin_offset = DRV_CONFIG(dev)->pin_offset - MAX_NR_HW_SGPIO;
	uint32_t group_idx = pin_offset >> 3;

	// Input an sgpio port which is not an input one.
	if(pin_offset < 0) {
		LOG_ERR("%s Invalid pinoffset #%d", __func__, DRV_CONFIG(dev)->pin_offset);
		return -EINVAL;
	}

	*value = (gpio_port_value_t)NPCM4XX_XDIN((uintptr_t)inst, group_idx);
	return 0;
}

static int sgpio_npcm4xx_port_set_masked_raw(const struct device *dev,
					  gpio_port_pins_t mask,
					  gpio_port_value_t value)
{
	struct sgpio_reg *inst = HAL_INSTANCE(dev);
	uint8_t pin_offset = DRV_CONFIG(dev)->pin_offset;
	uint32_t group_idx = pin_offset >> 3;
	uint8_t out = NPCM4XX_XDOUT((uintptr_t)inst, group_idx);
	uint8_t pin;
	bool pin_valid = false;

	// Input an sgpio port which is not an output one.
	if(pin_offset >= MAX_NR_HW_SGPIO) {
		LOG_ERR("%s Invalid pinoffset #%d", __func__, DRV_CONFIG(dev)->pin_offset);
		return -EINVAL;
	}

	// The max pin number is 7 in one sgpio port since it's 0-based.
	if(mask >= BIT(8)) {
		LOG_ERR("%s Invalid mask 0x%x", __func__, mask);
		return -EINVAL;
	}

	// find the pin which is at MSB
	for(pin = 7; pin >= 0; pin--) {
		if(mask & BIT(pin)) {
			pin_valid = true;
			break;
		}
	}

	if(!pin_valid) {
		LOG_ERR("%s Invalid pin #%d", __func__, pin);
		return -EINVAL;
	}

	// Input a pin number which is greater than the output pin number configured in the device tree.
	if((pin + pin_offset) >= DRV_PARENT_CFG(DRV_CONFIG(dev)->parent)->nout_sgpio) {
		LOG_ERR("%s Invalid sgpio pin #%d", __func__, (pin + pin_offset));
		return -EINVAL;
	}

	NPCM4XX_XDOUT((uintptr_t)inst, group_idx) = ((out & ~mask) | (value & mask));

	return 0;
}

static int sgpio_npcm4xx_port_set_bits_raw(const struct device *dev,
					gpio_port_value_t mask)
{
	return sgpio_npcm4xx_port_set_masked_raw(dev, mask, mask);
}

static int sgpio_npcm4xx_port_clear_bits_raw(const struct device *dev,
						gpio_port_value_t mask)
{
	struct sgpio_reg *inst = HAL_INSTANCE(dev);
	uint8_t pin_offset = DRV_CONFIG(dev)->pin_offset;
	uint32_t group_idx = pin_offset >> 3;
	uint8_t pin;
	bool pin_valid = false;

	// Input an sgpio port which is not an output one.
	if(pin_offset >= MAX_NR_HW_SGPIO) {
		LOG_ERR("%s Invalid pinoffset #%d", __func__, DRV_CONFIG(dev)->pin_offset);
		return -EINVAL;
	}

	// The max pin number is 7 in one sgpio port since it's 0-based.
	if(mask >= BIT(8)) {
		LOG_ERR("%s Invalid mask 0x%x", __func__, mask);
		return -EINVAL;
	}

	// find the pin which is at MSB
	for(pin = 7; pin >= 0; pin--) {
		if(mask & BIT(pin)) {
			pin_valid = true;
			break;
		}
	}

	if(!pin_valid) {
		LOG_ERR("%s Invalid pin #%d", __func__, pin);
		return -EINVAL;
	}

	// Input a pin number which is greater than the output pin number configured in the device tree.
	if((pin + pin_offset) >= DRV_PARENT_CFG(DRV_CONFIG(dev)->parent)->nout_sgpio) {
		LOG_ERR("%s Invalid sgpio pin #%d", __func__, (pin + pin_offset));
		return -EINVAL;
	}

	/* Clear raw bits of SGPIO output registers */
	NPCM4XX_XDOUT((uintptr_t)inst, group_idx) &= ~mask;

	return 0;
}

/* GPIO api functions */
static int sgpio_npcm4xx_config(const struct device *dev,
			     gpio_pin_t pin, gpio_flags_t flags)
{
	if((DRV_CONFIG(dev)->common.port_pin_mask & (gpio_port_pins_t)BIT(pin)) == 0)
		return -ENOTSUP;

	/* Don't support simultaneous in/out mode */
	if (((flags & GPIO_INPUT) != 0) && ((flags & GPIO_OUTPUT) != 0))
		return -ENOTSUP;

	/* Don't support "open source" mode */
	if (((flags & GPIO_SINGLE_ENDED) != 0) &&
	    ((flags & GPIO_LINE_OPEN_DRAIN) == 0))
		return -ENOTSUP;

	/* Does not support pull-up/pull-down */
	if ((flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) != 0U)
		return -ENOTSUP;

	/* Set level 0:low 1:high */
	if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0)
		sgpio_npcm4xx_port_set_bits_raw(dev, BIT(pin));
	else if ((flags & GPIO_OUTPUT_INIT_LOW) != 0)
		sgpio_npcm4xx_port_clear_bits_raw(dev, BIT(pin));

	return 0;
}

static int sgpio_npcm4xx_port_toggle_bits(const struct device *dev,
						gpio_port_value_t mask)
{
	struct sgpio_reg *inst = HAL_INSTANCE(dev);
	uint8_t pin_offset = DRV_CONFIG(dev)->pin_offset;
	uint32_t group_idx = pin_offset >> 3;
	uint8_t pin;
	bool pin_valid = false;

	// Input an sgpio port which is not an output one.
	if(pin_offset >= MAX_NR_HW_SGPIO) {
		LOG_ERR("%s Invalid pinoffset #%d", __func__, DRV_CONFIG(dev)->pin_offset);
		return -EINVAL;
	}

	// The max pin number is 7 in one sgpio port since it's 0-based.
	if(mask >= BIT(8)) {
		LOG_ERR("%s Invalid mask 0x%x", __func__, mask);
		return -EINVAL;
	}

	// find the pin which is at MSB
	for(pin = 7; pin >= 0; pin--) {
		if(mask & BIT(pin)) {
			pin_valid = true;
			break;
		}
	}

	if(!pin_valid) {
		LOG_ERR("%s Invalid pin #%d", __func__, pin);
		return -EINVAL;
	}

	// Input a pin number which is greater than the output pin number configured in the device tree.
	if((pin + pin_offset) >= DRV_PARENT_CFG(DRV_CONFIG(dev)->parent)->nout_sgpio) {
		LOG_ERR("%s Invalid sgpio pin #%d", __func__, (pin + pin_offset));
		return -EINVAL;
	}

	/* Toggle raw bits of SGPIO output registers */
	NPCM4XX_XDOUT((uintptr_t)inst, group_idx) ^= mask;

	return 0;
}

static int sgpio_npcm4xx_pin_interrupt_configure(const struct device *dev,
					     gpio_pin_t pin,
					     enum gpio_int_mode mode,
					     enum gpio_int_trig trig)
{
	struct sgpio_reg *inst = HAL_INSTANCE(dev);
	uint8_t pin_offset = DRV_CONFIG(dev)->pin_offset - MAX_NR_HW_SGPIO;
	uint32_t group_idx;
	uint16_t reg, val;

	// Input an sgpio port which is not an input one.
	if(pin_offset < 0) {
		LOG_ERR("%s Invalid pinoffset #%d", __func__, DRV_CONFIG(dev)->pin_offset);
		return -EINVAL;
	}

	if (pin >= 8) {
		LOG_ERR("Invalid sgpio pin #%d", pin);
		return -EINVAL;
	}

	// Input a pin number which is greater than the input pin number configured in the device tree.
	if((pin + pin_offset) >= DRV_PARENT_CFG(DRV_CONFIG(dev)->parent)->nin_sgpio) {
		LOG_ERR("%s Invalid sgpio pin #%d", __func__, (pin + pin_offset));
		return -EINVAL;
	}

	group_idx = pin_offset >> 3;

	/* Configure and enable interrupt? */
	if (mode != GPIO_INT_MODE_DISABLED) {
		if (mode == GPIO_INT_MODE_LEVEL) {
			if (trig == GPIO_INT_TRIG_LOW) {
				val = IXOEVCFG_FALLING;
			} else if (trig == GPIO_INT_TRIG_HIGH) {
				val = IXOEVCFG_RISING;
			} else {
				return -ENOTSUP;
			}
		} else {
			if (trig == GPIO_INT_TRIG_LOW) {
				val = IXOEVCFG_FALLING;
			} else if (trig == GPIO_INT_TRIG_HIGH) {
				val = IXOEVCFG_RISING;
			} else {
				val = IXOEVCFG_BOTH;
			}
		}
	} else
		val = 0;


	npcm_sgpio_setup_enable(DRV_CONFIG(dev)->parent, false);
	reg = NPCM4XX_XEVCFG((uintptr_t)inst, group_idx);
	reg &= ~(IXOEVCFG_MASK << (pin * 2));
	reg |= val << (pin * 2);
	NPCM4XX_XEVCFG((uintptr_t)inst, group_idx) = reg;
	npcm_sgpio_setup_enable(DRV_CONFIG(dev)->parent, true);

	return 0;
}

static int sgpio_npcm4xx_manage_callback(const struct device *dev,
				      struct gpio_callback *callback, bool set)
{
	struct sgpio_npcm4xx_data *const data = DRV_DATA(dev);

	return gpio_manage_callback(&data->cb, callback, set);
}

/* GPIO driver registration */
static const struct gpio_driver_api sgpio_npcm4xx_driver = {
	.pin_configure = sgpio_npcm4xx_config,
	.port_get_raw = sgpio_npcm4xx_port_get_raw,
	.port_set_masked_raw = sgpio_npcm4xx_port_set_masked_raw,
	.port_set_bits_raw = sgpio_npcm4xx_port_set_bits_raw,
	.port_clear_bits_raw = sgpio_npcm4xx_port_clear_bits_raw,
	.port_toggle_bits = sgpio_npcm4xx_port_toggle_bits,
	.pin_interrupt_configure = sgpio_npcm4xx_pin_interrupt_configure,
	.manage_callback = sgpio_npcm4xx_manage_callback,
};

static void sgpio_npcm4xx_isr(const void *arg)
{
	const struct sgpio_npcm4xx_parent_config *cfg;
	const struct device *parent = arg;
	struct sgpio_npcm4xx_data *data;
	struct sgpio_reg *inst;
	const struct device *dev;
	uint8_t gpio_pin, int_pendding;
	uint8_t index, group_idx;

	cfg = DRV_PARENT_CFG(parent);
	for (index = 8; index < cfg->child_num; index++) {
		dev = cfg->child_dev[index].dev;
		inst = HAL_INSTANCE(dev);
		data = DRV_DATA(dev);
		group_idx = (DRV_CONFIG(dev)->pin_offset - MAX_NR_HW_SGPIO) >> 3;
		int_pendding = NPCM4XX_XEVSTS((uintptr_t)inst, group_idx);
		gpio_pin = 0;
		while (int_pendding) {
			if (int_pendding & 0x1) {
				gpio_fire_callbacks(&data->cb, dev, BIT(gpio_pin));
				NPCM4XX_XEVSTS((uintptr_t)inst, group_idx) &= BIT(gpio_pin);
			}
			gpio_pin++;
			int_pendding >>= 1;
		}
	}
}

int sgpio_npcm4xx_init(const struct device *dev)
{
	return 0;
}

int sgpio_npcm4xx_parent_init(const struct device *parent)
{
	struct sgpio_npcm4xx_parent_config *const config = DRV_PARENT_CFG(parent);
	struct sgpio_reg *const inst = HAL_PARENT_INSTANCE(parent);
	const struct device *const clk_dev =
					device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	struct scfg_reg *inst_scfg = HAL_SFCG_INST();
	uint32_t clk, val;
	uint8_t i, size, tmp;
	int ret;

	if((config->nin_sgpio > MAX_NR_HW_SGPIO) || (config->nout_sgpio > MAX_NR_HW_SGPIO)) {
		LOG_ERR("Number of GPIOs exceeds the maximum of %d: input: %d output: %d\n",
				MAX_NR_HW_SGPIO, config->nin_sgpio, config->nout_sgpio);
		return -1;
	}

	/* Turn on device clock first and get source clock freq. */
	ret = clock_control_on(clk_dev, (clock_control_subsys_t *)
							&config->clk_cfg);
	if (ret < 0) {
		LOG_ERR("Turn on SGPIO clock fail %d", ret);
		return ret;
	}

	ret = clock_control_get_rate(clk_dev, (clock_control_subsys_t *)
		&config->clk_cfg, &clk);
	if (ret < 0) {
		LOG_ERR("Get SGPIO clock rate error %d", ret);
		return ret;
	}

	size = ARRAY_SIZE(npcm4xx_CLK_SEL);
	tmp = (inst->IOXCFG1) & ~NPCM4XX_IOXCFG1_SFT_CLK_MASK;

	for(i = 0; i < size; i++) {
		val = clk / npcm4xx_SFT_CLK[i];
		if ((config->sgpio_freq < val) && (i != 0)) {
			inst->IOXCFG1 = tmp | (npcm4xx_CLK_SEL[i-1]);
			break;
		} else if ((i == (size-1)) && (config->sgpio_freq > val)) {
			inst->IOXCFG1 = tmp | (npcm4xx_CLK_SEL[i]);
		}
	}

	ret = npcm_sgpio_init_port(parent);
	if(ret) {
		LOG_ERR("SGPIO init port fails");
		return ret;
	}

	npcm_sgpio_setup_enable(parent, false);

	/* Disable IRQ and clear Interrupt status registers for all SGPIO Pins. */
	for(i = 0; i < NPCM4XX_SGPIO_PORT_PIN_NUM; i++) {
		NPCM4XX_XEVCFG((uintptr_t)inst, i) = 0x0000;
		NPCM4XX_XEVSTS((uintptr_t)inst, i) = 0xff;
	}

	config->irq_conf_func(parent);
	NPCM4XX_DEVALT(inst_scfg, NPCM4XX_DEVALT6A_OFFSET) |= (BIT(NPCM4XX_DEVALT6A_SIOX1_PU_EN) | BIT(NPCM4XX_DEVALT6A_SIOX2_PU_EN));
	npcm_sgpio_setup_enable(parent, true);


	return 0;
}

struct device_cont {
	const struct sgpio_npcm4xx_config *cfg;
	struct sgpio_npcm4xx_data *data;
};


#define NPCM4XX_SGPIO_IRQ_CONFIG_FUNC_DECL(inst)                                   		    \
	static void sgpio_npcm4xx_irq_config_##inst(const struct device *dev)
#define NPCM4XX_SGPIO_IRQ_CONFIG_FUNC_INIT(inst) .irq_conf_func = sgpio_npcm4xx_irq_config_##inst,
#define NPCM4XX_SGPIO_IRQ_CONFIG_FUNC(inst)                                                         \
	static void sgpio_npcm4xx_irq_config_##inst(const struct device *dev)                       \
	{                                                                                           \
		IRQ_CONNECT(DT_INST_IRQN(inst), DT_INST_IRQ(inst, priority), sgpio_npcm4xx_isr,     \
			    DEVICE_DT_INST_GET(inst), 0);                                           \
		irq_enable(DT_INST_IRQN(inst));                                                     \
	}

#define SGPIO_ENUM(node_id) node_id,
#define SGPIO_NPCM4XX_DEV_DATA(node_id) {},
#define SGPIO_NPCM4XX_DEV_CFG(node_id) {					     \
		.common = {							     \
			.port_pin_mask =					     \
				GPIO_PORT_PIN_MASK_FROM_DT_NODE(node_id)	     \
		},								     \
		.parent = DEVICE_DT_GET(DT_PARENT(node_id)),			     \
		.base = (struct sgpio_reg *)DT_REG_ADDR(DT_PARENT(node_id)), \
		.pin_offset = DT_PROP(node_id, pin_offset),			     \
},
#define SGPIO_NPCM4XX_DT_DEFINE(node_id)                                                           \
	DEVICE_DT_DEFINE(node_id, sgpio_npcm4xx_init, NULL, &DT_PARENT(node_id).data[node_id],     \
			 &DT_PARENT(node_id).cfg[node_id], POST_KERNEL, 0, &sgpio_npcm4xx_driver);

#define SGPIO_NPCM4XX_DEV_DECLARE(node_id) { .dev = DEVICE_DT_GET(node_id) },

#define NPCM4XX_SGPIO_DEVICE_INIT(inst)                                                            \
	NPCM4XX_SGPIO_IRQ_CONFIG_FUNC_DECL(inst);						   \
												   \
	static struct device_array child_dev_##inst[] = { DT_FOREACH_CHILD(                        \
		DT_DRV_INST(inst), SGPIO_NPCM4XX_DEV_DECLARE) };                                   \
	static const struct sgpio_npcm4xx_parent_config sgpio_npcm4xx_parent_cfg_##inst = {        \
		.base = (struct sgpio_reg *)DT_INST_REG_ADDR(inst),                        	   \
		NPCM4XX_SGPIO_IRQ_CONFIG_FUNC_INIT(inst)			   		   \
		.clk_cfg = NPCM4XX_DT_CLK_CFG_ITEM(inst),                          		   \
		.nin_sgpio = DT_INST_PROP(inst, nuvoton_input_ngpios),             		   \
		.nout_sgpio = DT_INST_PROP(inst, nuvoton_output_ngpios),           		   \
		.sgpio_freq = DT_INST_PROP(inst, nuvoton_bus_freq),                		   \
		.child_dev = child_dev_##inst,                                                     \
		.child_num = ARRAY_SIZE(child_dev_##inst),                                         \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, sgpio_npcm4xx_parent_init, NULL, NULL,                         \
			      &sgpio_npcm4xx_parent_cfg_##inst, POST_KERNEL,                       \
			      CONFIG_GPIO_NPCM4XX_SGPIO_INIT_PRIORITY, NULL);                      \
	static const struct sgpio_npcm4xx_config sgpio_npcm4xx_cfg_##inst[] = { DT_FOREACH_CHILD(  \
		DT_DRV_INST(inst), SGPIO_NPCM4XX_DEV_CFG) };                                       \
	static struct sgpio_npcm4xx_data sgpio_npcm4xx_data_##inst[] = { DT_FOREACH_CHILD(         \
		DT_DRV_INST(inst), SGPIO_NPCM4XX_DEV_DATA) };                                      \
	static const struct device_cont DT_DRV_INST(inst) = {                                      \
		.cfg = sgpio_npcm4xx_cfg_##inst,                                                   \
		.data = sgpio_npcm4xx_data_##inst,                                                 \
	};                                                                                         \
	enum { DT_FOREACH_CHILD(DT_DRV_INST(inst), SGPIO_ENUM) };                                  \
	DT_FOREACH_CHILD(DT_DRV_INST(inst), SGPIO_NPCM4XX_DT_DEFINE)				   \
												   \
NPCM4XX_SGPIO_IRQ_CONFIG_FUNC(inst)

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_SGPIO_DEVICE_INIT)
