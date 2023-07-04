/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_gpio

#include <kernel.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/pinmux.h>
#include <soc.h>

#include "gpio_utils.h"
#include "soc_gpio.h"
#include "soc_miwu.h"
#include "sig_id.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(gpio_npcm4xx, LOG_LEVEL_ERR);

/* GPIO module instances declarations */
static const struct device *gpio_devs[];
static int gpio_devs_count;

/* Driver config */
struct gpio_npcm4xx_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	/* GPIO controller base address */
	uintptr_t base;
	/* IO port */
	int port;
	/* Pin offset */
	uint8_t pin_offset;
	/* Size of wui mapping array */
	int wui_size;
	/* Mapping table between gpio bits and wui */
	struct npcm4xx_wui wui_maps[];
};

/* Driver data */
struct gpio_npcm4xx_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	/* pinmux device */
	const struct device *pinmux;
};

struct npcm4xx_scfg_config {
        /* scfg device base address */
        uintptr_t base_scfg;
};

static const struct npcm4xx_scfg_config npcm4xx_scfg_cfg = {
        .base_scfg = DT_REG_ADDR_BY_NAME(DT_NODELABEL(scfg), scfg),
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct gpio_npcm4xx_config *)(dev)->config)

#define DRV_DATA(dev) ((struct gpio_npcm4xx_data *)(dev)->data)

#define HAL_INSTANCE(dev) (struct gpio_reg *)(DRV_CONFIG(dev)->base)

#define HAL_SFCG_INST() (struct scfg_reg *)(npcm4xx_scfg_cfg.base_scfg)

/* Platform specific GPIO functions */
const struct device *npcm4xx_get_gpio_dev(int port)
{
	if (port >= gpio_devs_count)
		return NULL;

	return gpio_devs[port];
}

void npcm4xx_gpio_enable_io_pads(const struct device *dev, int pin)
{
	const struct gpio_npcm4xx_config *const config = DRV_CONFIG(dev);
	const struct npcm4xx_wui *io_wui = &config->wui_maps[pin];

	/*
	 * If this pin is configurred as a GPIO interrupt source, do not
	 * implement bypass. Or ec cannot wake up via this event.
	 */
	if (pin < NPCM4XX_GPIO_PORT_PIN_NUM && !npcm4xx_miwu_irq_get_state(io_wui)) {
		npcm4xx_miwu_io_enable(io_wui);
	}
}

void npcm4xx_gpio_disable_io_pads(const struct device *dev, int pin)
{
	const struct gpio_npcm4xx_config *const config = DRV_CONFIG(dev);
	const struct npcm4xx_wui *io_wui = &config->wui_maps[pin];

	/*
	 * If this pin is configurred as a GPIO interrupt source, do not
	 * implement bypass. Or ec cannot wake up via this event.
	 */
	if (pin < NPCM4XX_GPIO_PORT_PIN_NUM && !npcm4xx_miwu_irq_get_state(io_wui)) {
		npcm4xx_miwu_io_disable(io_wui);
	}
}

/* GPIO api functions */
static int gpio_npcm4xx_config(const struct device *dev,
			     gpio_pin_t pin, gpio_flags_t flags)
{
	const struct gpio_npcm4xx_config *const config = DRV_CONFIG(dev);
	struct gpio_reg *const inst = HAL_INSTANCE(dev);
	struct scfg_reg *inst_scfg = HAL_SFCG_INST();
	struct gpio_npcm4xx_data *data = DRV_DATA(dev);
	uint32_t mask = BIT(pin);
	uint8_t pin_offset = config->pin_offset;
	int ret;

	/* Set Pin as GPIO */
        ret = pinmux_pin_set(data->pinmux, pin_offset + pin, SIG_GPIO);
        if (ret) {
                return ret;
        }

	/* Don't support simultaneous in/out mode */
	if (((flags & GPIO_INPUT) != 0) && ((flags & GPIO_OUTPUT) != 0)) {
		return -ENOTSUP;
	}

	/* Don't support "open source" mode */
	if (((flags & GPIO_SINGLE_ENDED) != 0) &&
	    ((flags & GPIO_LINE_OPEN_DRAIN) == 0)) {
		return -ENOTSUP;
	}

	/*
	 * Configure pin as input, if requested. Output is configured only
	 * after setting all other attributes, so as not to create a
	 * temporary incorrect logic state 0:input 1:output
	 */
	if ((flags & GPIO_OUTPUT) == 0)
		inst->PDIR &= ~mask;

	/* Select open drain 0:push-pull 1:open-drain */
	if ((flags & GPIO_OPEN_DRAIN) != 0)
		inst->PTYPE |= mask;
	else
		inst->PTYPE &= ~mask;

	/* Select opend drain with pull up need enable GPIO_PULL_EN */
	if (((flags & GPIO_OPEN_DRAIN) != 0) &&
	    ((flags & GPIO_PULL_UP) != 0)) {
		inst_scfg->DEVALTCX |= BIT(NPCM4XX_DEVALTCX_GPIO_PULL_EN);
	}

	/* Select pull-up/down of GPIO 0:pull-up 1:pull-down */
	if ((flags & GPIO_PULL_UP) != 0) {
		inst->PPUD  &= ~mask;
		inst->PPULL |= mask;
	} else if ((flags & GPIO_PULL_DOWN) != 0) {
		inst->PPUD  |= mask;
		inst->PPULL |= mask;
	} else {
		/* disable pull down/up */
		inst->PPULL &= ~mask;
	}

	/* Set level 0:low 1:high */
	if ((flags & GPIO_OUTPUT_INIT_HIGH) != 0)
		inst->PDOUT |= mask;
	else if ((flags & GPIO_OUTPUT_INIT_LOW) != 0)
		inst->PDOUT &= ~mask;

	/* Configure pin as output, if requested 0:input 1:output */
	if ((flags & GPIO_OUTPUT) != 0)
		inst->PDIR |= mask;

	return 0;
}

static int gpio_npcm4xx_port_get_raw(const struct device *dev,
				  gpio_port_value_t *value)
{
	struct gpio_reg *const inst = HAL_INSTANCE(dev);

	/* Get raw bits of GPIO input registers */
	*value = inst->PDIN;

	return 0;
}

static int gpio_npcm4xx_port_set_masked_raw(const struct device *dev,
					  gpio_port_pins_t mask,
					  gpio_port_value_t value)
{
	struct gpio_reg *const inst = HAL_INSTANCE(dev);
	uint8_t out = inst->PDOUT;

	inst->PDOUT = ((out & ~mask) | (value & mask));

	return 0;
}

static int gpio_npcm4xx_port_set_bits_raw(const struct device *dev,
					gpio_port_value_t mask)
{
	struct gpio_reg *const inst = HAL_INSTANCE(dev);

	/* Set raw bits of GPIO output registers */
	inst->PDOUT |= mask;

	return 0;
}

static int gpio_npcm4xx_port_clear_bits_raw(const struct device *dev,
						gpio_port_value_t mask)
{
	struct gpio_reg *const inst = HAL_INSTANCE(dev);

	/* Clear raw bits of GPIO output registers */
	inst->PDOUT &= ~mask;

	return 0;
}

static int gpio_npcm4xx_port_toggle_bits(const struct device *dev,
						gpio_port_value_t mask)
{
	struct gpio_reg *const inst = HAL_INSTANCE(dev);

	/* Toggle raw bits of GPIO output registers */
	inst->PDOUT ^= mask;

	return 0;
}

static int gpio_npcm4xx_pin_interrupt_configure(const struct device *dev,
					     gpio_pin_t pin,
					     enum gpio_int_mode mode,
					     enum gpio_int_trig trig)
{
	const struct gpio_npcm4xx_config *const config = DRV_CONFIG(dev);

	if (config->wui_maps[pin].table == NPCM4XX_MIWU_TABLE_NONE) {
		LOG_ERR("Cannot configure GPIO(%x, %d)", config->port, pin);
		return -EINVAL;
	}

	LOG_DBG("pin_int_conf (%d, %d) match (%d, %d, %d)!!!",
			config->port, pin, config->wui_maps[pin].table,
			config->wui_maps[pin].group,
			config->wui_maps[pin].bit);

	/* Disable irq of wake-up input io-pads before configuring them */
	npcm4xx_miwu_irq_disable(&config->wui_maps[pin]);

	/* Configure and enable interrupt? */
	if (mode != GPIO_INT_MODE_DISABLED) {
		enum miwu_int_mode miwu_mode;
		enum miwu_int_trig miwu_trig;
		int ret = 0;

		/* Determine interrupt is level or edge mode? */
		if (mode == GPIO_INT_MODE_EDGE) {
			miwu_mode = NPCM4XX_MIWU_MODE_EDGE;
		} else {
			miwu_mode = NPCM4XX_MIWU_MODE_LEVEL;
		}

		/* Determine trigger mode is low, high or both? */
		if (trig == GPIO_INT_TRIG_LOW) {
			miwu_trig = NPCM4XX_MIWU_TRIG_LOW;
		} else if (trig == GPIO_INT_TRIG_HIGH) {
			miwu_trig = NPCM4XX_MIWU_TRIG_HIGH;
		} else if (trig == GPIO_INT_TRIG_BOTH) {
			miwu_trig = NPCM4XX_MIWU_TRIG_BOTH;
		} else {
			LOG_ERR("Invalid interrupt trigger type %d", trig);
			return -EINVAL;
		}

		/* Call MIWU routine to setup interrupt configuration */
		ret = npcm4xx_miwu_interrupt_configure(&config->wui_maps[pin],
						miwu_mode, miwu_trig);
		if (ret != 0) {
			LOG_ERR("Configure MIWU interrupt failed");
			return ret;
		}

		/* Enable it after configuration is completed */
		npcm4xx_miwu_irq_enable(&config->wui_maps[pin]);
	}

	return 0;
}

static int gpio_npcm4xx_manage_callback(const struct device *dev,
				      struct gpio_callback *callback, bool set)
{
	const struct gpio_npcm4xx_config *const config = DRV_CONFIG(dev);
	struct miwu_io_callback *miwu_cb = (struct miwu_io_callback *)callback;
	int pin = find_lsb_set(callback->pin_mask) - 1;

	/* pin_mask should not be zero */
	if (pin < 0)
		return -EINVAL;

	/* Has the IO pin valid MIWU input source? */
	if (config->wui_maps[pin].table == NPCM4XX_MIWU_TABLE_NONE) {
		LOG_ERR("Cannot manage GPIO(%x, %d) callback!", config->port,
				pin);
		return -EINVAL;
	}

	/* Initialize WUI information in unused bits field */
	npcm4xx_miwu_init_gpio_callback(miwu_cb, &config->wui_maps[pin],
			config->port);

	/* Insert or remove a IO callback which being called in MIWU ISRs */
	return npcm4xx_miwu_manage_gpio_callback(miwu_cb, set);
}

/* GPIO driver registration */
static const struct gpio_driver_api gpio_npcm4xx_driver = {
	.pin_configure = gpio_npcm4xx_config,
	.port_get_raw = gpio_npcm4xx_port_get_raw,
	.port_set_masked_raw = gpio_npcm4xx_port_set_masked_raw,
	.port_set_bits_raw = gpio_npcm4xx_port_set_bits_raw,
	.port_clear_bits_raw = gpio_npcm4xx_port_clear_bits_raw,
	.port_toggle_bits = gpio_npcm4xx_port_toggle_bits,
	.pin_interrupt_configure = gpio_npcm4xx_pin_interrupt_configure,
	.manage_callback = gpio_npcm4xx_manage_callback,
};

int gpio_npcm4xx_init(const struct device *dev)
{
	struct gpio_npcm4xx_data *data = DRV_DATA(dev);

	__ASSERT(DRV_CONFIG(dev)->wui_size == NPCM4XX_GPIO_PORT_PIN_NUM,
			"wui_maps array size must equal to its pin number");

	data->pinmux = DEVICE_DT_GET(DT_NODELABEL(pinmux));

	return 0;
}

#define NPCM4XX_GPIO_DEVICE_INIT(inst)                                             \
	static const struct gpio_npcm4xx_config gpio_npcm4xx_cfg_##inst = {        \
		.common = {						           \
			.port_pin_mask =                                           \
			GPIO_PORT_PIN_MASK_FROM_NGPIOS(NPCM4XX_GPIO_PORT_PIN_NUM), \
		},                                                                 \
		.base = DT_INST_REG_ADDR(inst),                                    \
		.port = inst,                                                      \
		.pin_offset = DT_PROP(DT_DRV_INST(inst), pin_offset),		   \
		.wui_size = NPCM4XX_DT_WUI_ITEMS_LEN(inst),                        \
		.wui_maps = NPCM4XX_DT_WUI_ITEMS_LIST(inst)                        \
	};                                                                         \
										   \
	static struct gpio_npcm4xx_data gpio_npcm4xx_data_##inst;		   \
										   \
	DEVICE_DT_INST_DEFINE(inst,						   \
			    gpio_npcm4xx_init,					   \
			    NULL,						   \
			    &gpio_npcm4xx_data_##inst,				   \
			    &gpio_npcm4xx_cfg_##inst,				   \
			    POST_KERNEL,					   \
			    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,		   \
			    &gpio_npcm4xx_driver);

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_GPIO_DEVICE_INIT)

/* GPIO module instances */
#define NPCM4XX_GPIO_DEV(inst) DEVICE_DT_INST_GET(inst),
static const struct device *gpio_devs[] = {
	DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_GPIO_DEV)
};

static int gpio_devs_count = ARRAY_SIZE(gpio_devs);
