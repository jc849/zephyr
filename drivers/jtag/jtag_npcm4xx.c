/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_jtag

#include <stdlib.h>
#include <errno.h>
#include <drivers/jtag.h>
#include <device.h>
#include <kernel.h>
#include <init.h>
#include <soc.h>

#define LOG_LEVEL CONFIG_JTAG_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(jtag_npcm4xx);
#include "jtag_npcm4xx.h"
#include <drivers/gpio.h>
#include <drivers/jtag.h>

struct jtag_info {
	enum jtag_npcm4xx_tap_states tap_state;
};

static struct jtag_info gjtag;

struct npcm4xx_jtag_gpio {
        const char *port;
        gpio_pin_t pin;
        gpio_dt_flags_t flags;
};

struct jtag_npcm4xx_cfg {
	struct npcm4xx_jtag_gpio gpio_tck;
	struct npcm4xx_jtag_gpio gpio_tdi;
	struct npcm4xx_jtag_gpio gpio_tdo;
	struct npcm4xx_jtag_gpio gpio_tms;
	uint8_t use_spi;
};

#define DEV_CFG(dev) ((const struct jtag_npcm4xx_cfg *const)(dev)->config)
#define DEV_DATA(dev) ((struct jtag_npcm4xx_data *)(dev)->data)

#define NPCM4XX_JTAG_TCK_GPIO_ELEM(idx, inst) \
	{ \
		DT_INST_GPIO_LABEL_BY_IDX(inst, tck_gpios, idx), \
		DT_INST_GPIO_PIN_BY_IDX(inst, tck_gpios, idx),   \
		DT_INST_GPIO_FLAGS_BY_IDX(inst, tck_gpios, idx), \
	}

#define NPCM4XX_JTAG_TDI_GPIO_ELEM(idx, inst) \
	{ \
		DT_INST_GPIO_LABEL_BY_IDX(inst, tdi_gpios, idx), \
		DT_INST_GPIO_PIN_BY_IDX(inst, tdi_gpios, idx),   \
		DT_INST_GPIO_FLAGS_BY_IDX(inst, tdi_gpios, idx), \
	}

#define NPCM4XX_JTAG_TDO_GPIO_ELEM(idx, inst) \
        { \
                DT_INST_GPIO_LABEL_BY_IDX(inst, tdo_gpios, idx), \
                DT_INST_GPIO_PIN_BY_IDX(inst, tdo_gpios, idx),   \
                DT_INST_GPIO_FLAGS_BY_IDX(inst, tdo_gpios, idx), \
        }

#define NPCM4XX_JTAG_TMS_GPIO_ELEM(idx, inst) \
	{ \
		DT_INST_GPIO_LABEL_BY_IDX(inst, tms_gpios, idx), \
		DT_INST_GPIO_PIN_BY_IDX(inst, tms_gpios, idx),   \
		DT_INST_GPIO_FLAGS_BY_IDX(inst, tms_gpios, idx), \
	}

const struct device *tck_dev = NULL;
const struct device *tdi_dev = NULL;
const struct device *tdo_dev = NULL;
const struct device *tms_dev = NULL;
gpio_pin_t tck_pin = 0;
gpio_pin_t tdi_pin = 0;
gpio_pin_t tdo_pin = 0;
gpio_pin_t tms_pin = 0;

/* return value: tdo */
static inline uint8_t jtag_npcm4xx_tck_cycle(uint8_t tms, uint8_t tdi)
{
	gpio_port_value_t ret_tdo;

	/* TCK = 0 */
	gpio_port_clear_bits_raw(tck_dev, BIT(tck_pin));
	gpio_port_set_masked_raw(tdi_dev, BIT(tdi_pin), (tdi << tdi_pin));
	gpio_port_set_masked_raw(tms_dev, BIT(tms_pin), (tms << tms_pin));

	/* TCK = 1 */
	gpio_port_set_bits_raw(tck_dev, BIT(tck_pin));
	gpio_port_get_raw(tdo_dev, &ret_tdo);
	gpio_port_clear_bits_raw(tck_dev, BIT(tck_pin));

	return ((ret_tdo & BIT(tdo_pin)) >> tdo_pin);
}

static void jtag_npcm4xx_set_tap_state(enum jtag_npcm4xx_tap_states from, enum jtag_npcm4xx_tap_states to)
{
	uint8_t i;
	uint8_t tmsbits;
	uint8_t count;

	if (from == to) {
		return;
	}
	if (from == NPCM4XX_TAP_CURRENT_STATE) {
		from = gjtag.tap_state;
	}

	if (from > NPCM4XX_TAP_CURRENT_STATE || to > NPCM4XX_TAP_CURRENT_STATE) {
		return;
	}

	/* Reset to Test-Logic Reset state:
	 * Also notice that in whatever state the TAP controller may be at,
	 * it will goes back to this state if TMS is set to 1 for 5 consecutive TCK cycles.
	 */
	if (to == NPCM4XX_TAP_RESET) {
		for (i = 0; i < 9; i++) {
			jtag_npcm4xx_tck_cycle(1, 0);
		}
		gjtag.tap_state = NPCM4XX_TAP_RESET;
		return;
	}

	tmsbits = _tms_cycle_lookup[from][to].tms_bits;
	count = _tms_cycle_lookup[from][to].count;

	if (count == 0) {
		return;
	}

	for (i = 0; i < count; i++) {
		jtag_npcm4xx_tck_cycle((tmsbits & 1), 1);
		tmsbits >>= 1;
	}

	gjtag.tap_state = to;
}

static enum jtag_npcm4xx_tap_states jtag_npcm4xx_get_tap_state(void)
{
	return gjtag.tap_state;
}

static void jtag_npcm4xx_xfer_spi(uint32_t out_bytes_len, const uint8_t *out_data,
			   uint32_t in_byte_len, uint8_t *in_data)
{

}

static void jtag_npcm4xx_xfer_gpio(uint32_t out_bits_len, const uint8_t *out_data,
			    uint32_t in_bits_len, uint8_t *in_data, uint8_t last_data)
{
	uint32_t bits_len;
	uint32_t bits_idx = 0;
	uint8_t tdi, tms;

	bits_len = (out_bits_len > in_bits_len) ? out_bits_len : in_bits_len;

	tms = 0;

	/* clear first byte, others bytes will clear in while loop */
	if (in_data)
		*in_data = 0x0;

	while (bits_idx < bits_len) {
		if (out_bits_len) {
			tdi = (*out_data & BIT(bits_idx)) >> bits_idx;
		} else {
			tdi = 0;
		}

		if (bits_idx == bits_len - 1) { /* last bit */
			if (last_data) {
				tms = 1;
			}
		}

		if (in_bits_len) {
			*in_data |= (jtag_npcm4xx_tck_cycle(tms, tdi) << bits_idx);
		} else {
			jtag_npcm4xx_tck_cycle(tms, tdi);
		}

		out_bits_len = (out_bits_len) ? (out_bits_len - 1) : 0;
		in_bits_len = (in_bits_len) ? (in_bits_len - 1) : 0;

		if (++bits_idx == 8) {
			bits_idx = 0;
			bits_len -= 8;
			out_data++;
			if (in_bits_len) {
				*(++in_data) = 0;
			}
		}
	}
}

static void jtag_npcm4xx_readwrite_scan(const struct device *dev, int bits_len, const uint8_t *out_data,
					uint8_t *in_data, enum jtag_npcm4xx_tap_states end_state)
{
	const struct jtag_npcm4xx_cfg *config = DEV_CFG(dev);
	uint32_t remain_bits = bits_len;
	uint32_t spi_xfer_bytes = 0;

	if (config->use_spi)
		spi_xfer_bytes = (bits_len / 8) - 1;

	if (spi_xfer_bytes) {
		jtag_npcm4xx_xfer_spi(spi_xfer_bytes, out_data, spi_xfer_bytes, in_data);
		remain_bits -= spi_xfer_bytes * 8;
	}

	if (remain_bits) {
		if (end_state != NPCM4XX_TAP_SHIFT_DR && end_state != NPCM4XX_TAP_SHIFT_IR &&
		    end_state != NPCM4XX_TAP_CURRENT_STATE) {
			jtag_npcm4xx_xfer_gpio(remain_bits, out_data + spi_xfer_bytes,
					       remain_bits, in_data + spi_xfer_bytes, 1);
			gjtag.tap_state = (gjtag.tap_state == NPCM4XX_TAP_SHIFT_DR) ?
					  NPCM4XX_TAP_EXIT1_DR : NPCM4XX_TAP_EXIT1_IR;
		} else {
			jtag_npcm4xx_xfer_gpio(remain_bits, out_data + spi_xfer_bytes,
					       remain_bits, in_data + spi_xfer_bytes, 0);
		}
	}

	jtag_npcm4xx_set_tap_state(NPCM4XX_TAP_CURRENT_STATE, end_state);
}

static void jtag_npcm4xx_ir_scan(const struct device *dev, int bits_len, const uint8_t *out_data,
				uint8_t *in_data, enum jtag_npcm4xx_tap_states end_state)
{
	if (bits_len == 0) {
		return;
	}

	jtag_npcm4xx_set_tap_state(NPCM4XX_TAP_CURRENT_STATE, NPCM4XX_TAP_SHIFT_IR);

	jtag_npcm4xx_readwrite_scan(dev, bits_len, out_data, in_data, end_state);

}

static void jtag_npcm4xx_dr_scan(const struct device *dev, int bits_len, const uint8_t *out_data,
		uint8_t *in_data, enum jtag_npcm4xx_tap_states end_state)
{
	if (bits_len == 0) {
		return;
	}

	jtag_npcm4xx_set_tap_state(NPCM4XX_TAP_CURRENT_STATE, NPCM4XX_TAP_SHIFT_DR);

	jtag_npcm4xx_readwrite_scan(dev, bits_len, out_data, in_data, end_state);

}

#if 0
/* ============= (reference) hal for Open BIC ============= */
void jtag_set_tap(uint8_t data, uint8_t bitlength)
{
	uint8_t tms, index;

	for (index = 0; index < bitlength; index++) {
		tms = data & 0x01;
		jtag_npcm4xx_tck_cycle(tms, 0);
		data = data >> 1;
	}
}

void jtag_shift_data(uint16_t Wbit, uint8_t *Wdate,
		     uint16_t Rbit, uint8_t *Rdate,
		     uint8_t lastidx)
{
	jtag_npcm4xx_xfer_gpio(Wbit, Wdate, Rbit, Rdate, lastidx);
}
#endif

/* implement general function for device API */

static enum jtag_npcm4xx_tap_states jtag_npcm4xx_covert_tap_state(enum tap_state state)
{
	enum jtag_npcm4xx_tap_states npcm4xx_state = NPCM4XX_TAP_CURRENT_STATE;

	switch(state) {
		case TAP_INVALID:
			npcm4xx_state = NPCM4XX_TAP_CURRENT_STATE;
			break;
		case TAP_DREXIT2:
			npcm4xx_state = NPCM4XX_TAP_EXIT2_DR;
			break;
		case TAP_DREXIT1:
			npcm4xx_state = NPCM4XX_TAP_EXIT1_DR;
			break;
		case TAP_DRSHIFT:
			npcm4xx_state = NPCM4XX_TAP_SHIFT_DR;
			break;
		case TAP_DRPAUSE:
			npcm4xx_state = NPCM4XX_TAP_PAUSE_DR;
			break;
		case TAP_IRSELECT:
			npcm4xx_state = NPCM4XX_TAP_SELECT_IR;
			break;
		case TAP_DRUPDATE:
			npcm4xx_state = NPCM4XX_TAP_UPDATE_DR;
			break;
		case TAP_DRCAPTURE:
			npcm4xx_state = NPCM4XX_TAP_CAPTURE_DR;
			break;
		case TAP_DRSELECT:
			npcm4xx_state = NPCM4XX_TAP_SELECT_DR;
			break;
		case TAP_IREXIT2:
			npcm4xx_state = NPCM4XX_TAP_EXIT2_IR;
			break;
		case TAP_IREXIT1:
			npcm4xx_state = NPCM4XX_TAP_EXIT1_IR;
			break;
		case TAP_IRSHIFT:
			npcm4xx_state = NPCM4XX_TAP_SHIFT_IR;
			break;
		case TAP_IRPAUSE:
			npcm4xx_state = NPCM4XX_TAP_PAUSE_IR;
			break;
		case TAP_IDLE:
			npcm4xx_state = NPCM4XX_TAP_IDLE;
			break;
		case TAP_IRUPDATE:
			npcm4xx_state = NPCM4XX_TAP_UPDATE_IR;
			break;
		case TAP_IRCAPTURE:
			npcm4xx_state = NPCM4XX_TAP_CAPTURE_IR;
			break;
		case TAP_RESET:
			npcm4xx_state = NPCM4XX_TAP_RESET;
			break;
		default:
			npcm4xx_state = NPCM4XX_TAP_CURRENT_STATE;
			break;
	}

	return npcm4xx_state;
}

static enum tap_state jtag_npcm4xx_covert_npcm4xx_tap_state(enum jtag_npcm4xx_tap_states npcm4xx_tap_state)
{
	enum tap_state state = TAP_INVALID;

	switch(npcm4xx_tap_state) {
		case NPCM4XX_TAP_RESET:
			state = TAP_RESET;
			break;
		case NPCM4XX_TAP_IDLE:
			state = TAP_IDLE;
			break;
		case NPCM4XX_TAP_SELECT_DR:
			state = TAP_DRSELECT;
			break;
		case NPCM4XX_TAP_CAPTURE_DR:
			state = TAP_DRCAPTURE;
			break;
		case NPCM4XX_TAP_SHIFT_DR:
			state = TAP_DRSHIFT;
			break;
		case NPCM4XX_TAP_EXIT1_DR:
			state = TAP_DREXIT1;
			break;
		case NPCM4XX_TAP_PAUSE_DR:
			state = TAP_DRPAUSE;
			break;
		case NPCM4XX_TAP_EXIT2_DR:
			state = TAP_DREXIT2;
			break;
		case NPCM4XX_TAP_UPDATE_DR:
			state = TAP_DRUPDATE;
			break;
		case NPCM4XX_TAP_SELECT_IR:
			state = TAP_IRSELECT;
			break;
		case NPCM4XX_TAP_CAPTURE_IR:
			state = TAP_IRCAPTURE;
			break;
		case NPCM4XX_TAP_SHIFT_IR:
			state = TAP_IRSHIFT;
			break;
		case NPCM4XX_TAP_EXIT1_IR:
			state = TAP_IREXIT1;
			break;
		case NPCM4XX_TAP_PAUSE_IR:
			state = TAP_IRPAUSE;
			break;
		case NPCM4XX_TAP_EXIT2_IR:
			state = TAP_IREXIT2;
			break;
		case NPCM4XX_TAP_UPDATE_IR:
			state = TAP_IRUPDATE;
			break;
		default:
			state = TAP_INVALID;
			break;
	}

	return state;
}

int jtag_npcm4xx_freq_get(const struct device *dev, uint32_t *freq)
{
	return -ENOTSUP;
}

int jtag_npcm4xx_freq_set(const struct device *dev, uint32_t freq)
{
	return -ENOTSUP;
}

int jtag_npcm4xx_tap_get(const struct device *dev, enum tap_state *state)
{
	*state = jtag_npcm4xx_covert_npcm4xx_tap_state(jtag_npcm4xx_get_tap_state());

	return 0;
}

static int jtag_npcm4xx_tap_set(const struct device *dev, enum tap_state state)
{
	enum jtag_npcm4xx_tap_states end_state = NPCM4XX_TAP_CURRENT_STATE;

	end_state = jtag_npcm4xx_covert_tap_state(state);

	if (end_state == NPCM4XX_TAP_CURRENT_STATE)
		return -EINVAL;

	jtag_npcm4xx_set_tap_state(NPCM4XX_TAP_CURRENT_STATE, end_state);

	return 0;
}

static int jtag_npcm4xx_tck_run(const struct device *dev, uint32_t run_count)
{
	uint32_t i;
	uint8_t dummy;

	for (i = 0; i < run_count; i++)
		dummy = jtag_npcm4xx_tck_cycle(0, 0);

	return 0;
}

static int jtag_npcm4xx_xfer(const struct device *dev, struct scan_command_s *scan)
{
	struct scan_field_s *fields;
	enum jtag_npcm4xx_tap_states npcm4xx_state = NPCM4XX_TAP_CURRENT_STATE;

	fields = &scan->fields;

	npcm4xx_state = jtag_npcm4xx_covert_tap_state(scan->end_state);

	if (npcm4xx_state == NPCM4XX_TAP_CURRENT_STATE)
		return -1;

	if (scan->ir_scan) {
		jtag_npcm4xx_ir_scan(dev, fields->num_bits,
				fields->out_value, fields->in_value, npcm4xx_state);
	} else {
		jtag_npcm4xx_dr_scan(dev, fields->num_bits,
				fields->out_value, fields->in_value, npcm4xx_state);
	}

	return 0;
}

static int jtag_npcm4xx_sw_xfer(const struct device *dev, enum jtag_pin pin,
				uint8_t value)
{
	switch (pin) {
		case JTAG_TDI:
			gpio_port_set_masked_raw(tdi_dev, BIT(tdi_pin), (value << tdi_pin));
			break;
		case JTAG_TCK:
			if (value == 0)
				gpio_port_clear_bits_raw(tck_dev, BIT(tck_pin));
			else
				gpio_port_set_bits_raw(tck_dev, BIT(tck_pin));
			break;
		case JTAG_TMS:
			gpio_port_set_masked_raw(tms_dev, BIT(tms_pin), (value << tms_pin));
			break;
		case JTAG_ENABLE:
		default:
			return -EINVAL;
	}

	return 0;
}

static int jtag_npcm4xx_tdo_get(const struct device *dev, uint8_t *value)
{
	gpio_port_value_t ret_tdo;

	gpio_port_get_raw(tdo_dev, &ret_tdo);

	*value = (ret_tdo & BIT(tdo_pin)) >> tdo_pin;

	return 0;
}

static int jtag_npcm4xx_init(const struct device *dev)
{
	const struct jtag_npcm4xx_cfg *config = DEV_CFG(dev);

	gpio_flags_t flags = 0;

	tck_dev = device_get_binding(config->gpio_tck.port);
	tck_pin = config->gpio_tck.pin;

	tdi_dev = device_get_binding(config->gpio_tdi.port);
	tdi_pin = config->gpio_tdi.pin;

	tdo_dev = device_get_binding(config->gpio_tdo.port);
	tdo_pin = config->gpio_tdo.pin;

	tms_dev = device_get_binding(config->gpio_tms.port);
	tms_pin = config->gpio_tms.pin;

	/* setup tck */
	flags = GPIO_OUTPUT;

	if (config->gpio_tck.flags & GPIO_ACTIVE_LOW)
		flags |= GPIO_OUTPUT_INIT_HIGH;
	else
		flags |= GPIO_OUTPUT_INIT_LOW;

	if (config->gpio_tck.flags & GPIO_OPEN_DRAIN)
		flags |= GPIO_OPEN_DRAIN;
	else
		flags |= GPIO_PUSH_PULL;

	gpio_pin_configure(tck_dev, tck_pin, flags);

	/* setup tdi */
	flags = GPIO_OUTPUT;

	if (config->gpio_tdi.flags & GPIO_ACTIVE_LOW)
		flags |= GPIO_OUTPUT_INIT_HIGH;
	else
		flags |= GPIO_OUTPUT_INIT_LOW;

	if (config->gpio_tdi.flags & GPIO_OPEN_DRAIN)
		flags |= GPIO_OPEN_DRAIN;
	else
		flags |= GPIO_PUSH_PULL;

	gpio_pin_configure(tdi_dev, tdi_pin, flags);

	/* setup tdo */
	flags = GPIO_INPUT;

	gpio_pin_configure(tdo_dev, tdo_pin, flags);

	/* setup tms */
	flags = GPIO_OUTPUT;

	if (config->gpio_tms.flags & GPIO_ACTIVE_LOW)
		flags |= GPIO_OUTPUT_INIT_HIGH;
	else
		flags |= GPIO_OUTPUT_INIT_LOW;

	if (config->gpio_tms.flags & GPIO_OPEN_DRAIN)
		flags |= GPIO_OPEN_DRAIN;
	else
		flags |= GPIO_PUSH_PULL;

	gpio_pin_configure(tms_dev, tms_pin, flags);

	LOG_INF("TCK = %s PIN = %d", tck_dev->name, tck_pin);
	LOG_INF("TDI = %s PIN = %d", tdi_dev->name, tdi_pin);
	LOG_INF("TDO = %s PIN = %d", tdo_dev->name, tdo_pin);
	LOG_INF("TMS = %s PIN = %d", tms_dev->name, tms_pin);

	/* Test-Logic Reset state */
	gjtag.tap_state = NPCM4XX_TAP_RESET;

	return 0;
}

static struct jtag_driver_api jtag_npcm4xx_api = {
	.freq_get = jtag_npcm4xx_freq_get,
	.freq_set = jtag_npcm4xx_freq_set,
	.tap_get = jtag_npcm4xx_tap_get,
	.tap_set = jtag_npcm4xx_tap_set,
	.tck_run = jtag_npcm4xx_tck_run,
	.xfer = jtag_npcm4xx_xfer,
	.sw_xfer = jtag_npcm4xx_sw_xfer,
	.tdo_get = jtag_npcm4xx_tdo_get,
};

#define NPCM4XX_JTAG_INIT(n)											\
	static const struct jtag_npcm4xx_cfg jtag_npcm4xx_cfg_##n = {						\
		.use_spi = DT_INST_PROP(n, use_spi),								\
		.gpio_tck = UTIL_LISTIFY(DT_INST_PROP_LEN(n, tck_gpios), NPCM4XX_JTAG_TCK_GPIO_ELEM, n),	\
		.gpio_tdi = UTIL_LISTIFY(DT_INST_PROP_LEN(n, tdi_gpios), NPCM4XX_JTAG_TDI_GPIO_ELEM, n),	\
		.gpio_tdo = UTIL_LISTIFY(DT_INST_PROP_LEN(n, tdo_gpios), NPCM4XX_JTAG_TDO_GPIO_ELEM, n),	\
		.gpio_tms = UTIL_LISTIFY(DT_INST_PROP_LEN(n, tms_gpios), NPCM4XX_JTAG_TMS_GPIO_ELEM, n),	\
	};													\
	DEVICE_DT_INST_DEFINE(n, jtag_npcm4xx_init, NULL,                       				\
                              NULL, &jtag_npcm4xx_cfg_##n,    							\
                              POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,  				\
                              &jtag_npcm4xx_api);

DT_INST_FOREACH_STATUS_OKAY(NPCM4XX_JTAG_INIT)
