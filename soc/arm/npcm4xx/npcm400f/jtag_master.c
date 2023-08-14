/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "jtag_master.h"
#include <drivers/gpio.h>

/* ----------------------- Configuration -------------------------------*/
/* 0: not using SPI, 1: using SPI */
#define CONFIG_USE_SPI 0

#if (CONFIG_USE_SPI == 1)
/* SPI_CLK */
	#define CONFIG_TCK_DEV   DT_LABEL(DT_NODELABEL(gpio6))
	#define CONFIG_TCK_PIN   6
	#define CONFIG_TCK_FLAG  (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)

/* SPI_MOSI */
	#define CONFIG_TDI_DEV  DT_LABEL(DT_NODELABEL(gpio6))
	#define CONFIG_TDI_PIN  7
	#define CONFIG_TDI_FLAG (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)

/* SPI_MISO */
	#define CONFIG_TDO_DEV  DT_LABEL(DT_NODELABEL(gpio7))
	#define CONFIG_TDO_PIN  0
	#define CONFIG_TDO_FLAG (GPIO_INPUT)

/* SPI_CS */
	#define CONFIG_TMS_DEV  DT_LABEL(DT_NODELABEL(gpio7))
	#define CONFIG_TMS_PIN  3
	#define CONFIG_TMS_FLAG (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)
#else
	#define CONFIG_TCK_DEV   DT_LABEL(DT_NODELABEL(gpio6))
	#define CONFIG_TCK_PIN   6
	#define CONFIG_TCK_FLAG  (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)

	#define CONFIG_TDI_DEV  DT_LABEL(DT_NODELABEL(gpio6))
	#define CONFIG_TDI_PIN  7
	#define CONFIG_TDI_FLAG (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)

	#define CONFIG_TDO_DEV  DT_LABEL(DT_NODELABEL(gpio7))
	#define CONFIG_TDO_PIN  0
	#define CONFIG_TDO_FLAG (GPIO_INPUT)

	#define CONFIG_TMS_DEV  DT_LABEL(DT_NODELABEL(gpio7))
	#define CONFIG_TMS_PIN  3
	#define CONFIG_TMS_FLAG (GPIO_OUTPUT | GPIO_PUSH_PULL | GPIO_OUTPUT_INIT_LOW)
#endif
/* ---------------------------------------------------------------------*/

struct jtag_info {
	uint8_t tap_state;
};

static struct jtag_info gjtag;

/* this structure represents a TMS cycle, as expressed in a set of bits and
 * a count of bits (note: there are no start->end state transitions that
 * require more than 1 byte of TMS cycles)
 */
struct tms_cycle {
	uint8_t tms_bits;
	uint8_t count;
};

/* this is the complete set TMS cycles for going from any TAP state to
 * any other TAP state, following a “shortest path” rule
 */
const struct tms_cycle _tms_cycle_lookup[][16] = {
/*      RESET       IDLE       SELECT_DR  CAPTURE_DR SHIFT_DR   */
/*      EXIT1_DR    PAUSE_DR   EXIT2_DR   UPDATE_DR  SELECT_IR  */
/*      CAPTURE_IR  SHIFT_IR   EXIT1_IR   PAUSE_IR   EXIT2_IR   */
/*      UPDATE_IR                                               */
/* RESET */
	{
		{ 0x01, 1 }, { 0x00, 1 }, { 0x02, 2 }, { 0x02, 3 }, { 0x02, 4 },
		{ 0x0a, 4 }, { 0x0a, 5 }, { 0x2a, 6 }, { 0x1a, 5 }, { 0x06, 3 },
		{ 0x06, 4 }, { 0x06, 5 }, { 0x16, 5 }, { 0x16, 6 }, { 0x56, 7 },
		{ 0x36, 6 }
	},
/* IDLE */
	{
		{ 0x07, 3 }, { 0x00, 1 }, { 0x01, 1 }, { 0x01, 2 }, { 0x01, 3 },
		{ 0x05, 3 }, { 0x05, 4 }, { 0x15, 5 }, { 0x0d, 4 }, { 0x03, 2 },
		{ 0x03, 3 }, { 0x03, 4 }, { 0x0b, 4 }, { 0x0b, 5 }, { 0x2b, 6 },
		{ 0x1b, 5 }
	},
/* SELECT_DR */
	{
		{ 0x03, 2 }, { 0x03, 3 }, { 0x00, 0 }, { 0x00, 1 }, { 0x00, 2 },
		{ 0x02, 2 }, { 0x02, 3 }, { 0x0a, 4 }, { 0x06, 3 }, { 0x01, 1 },
		{ 0x01, 2 }, { 0x01, 3 }, { 0x05, 3 }, { 0x05, 4 }, { 0x15, 5 },
		{ 0x0d, 4 }
	},
/* CAPTURE_DR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x00, 0 }, { 0x00, 1 },
		{ 0x01, 1 }, { 0x01, 2 }, { 0x05, 3 }, { 0x03, 2 }, { 0x0f, 4 },
		{ 0x0f, 5 }, { 0x0f, 6 }, { 0x2f, 6 }, { 0x2f, 7 }, { 0xaf, 8 },
		{ 0x6f, 7 }
	},
/* SHIFT_DR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x07, 4 }, { 0x00, 0 },
		{ 0x01, 1 }, { 0x01, 2 }, { 0x05, 3 }, { 0x03, 2 }, { 0x0f, 4 },
		{ 0x0f, 5 }, { 0x0f, 6 }, { 0x2f, 6 }, { 0x2f, 7 }, { 0xaf, 8 },
		{ 0x6f, 7 }
	},
/* EXIT1_DR */
	{
		{ 0x0f, 4 }, { 0x01, 2 }, { 0x03, 2 }, { 0x03, 3 }, { 0x02, 3 },
		{ 0x00, 0 }, { 0x00, 1 }, { 0x02, 2 }, { 0x01, 1 }, { 0x07, 3 },
		{ 0x07, 4 }, { 0x07, 5 }, { 0x17, 5 }, { 0x17, 6 }, { 0x57, 7 },
		{ 0x37, 6 }
	},
/* PAUSE_DR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x07, 4 }, { 0x01, 2 },
		{ 0x05, 3 }, { 0x00, 1 }, { 0x01, 1 }, { 0x03, 2 }, { 0x0f, 4 },
		{ 0x0f, 5 }, { 0x0f, 6 }, { 0x2f, 6 }, { 0x2f, 7 }, { 0xaf, 8 },
		{ 0x6f, 7 }
	},
/* EXIT2_DR */
	{
		{ 0x0f, 4 }, { 0x01, 2 }, { 0x03, 2 }, { 0x03, 3 }, { 0x00, 1 },
		{ 0x02, 2 }, { 0x02, 3 }, { 0x00, 0 }, { 0x01, 1 }, { 0x07, 3 },
		{ 0x07, 4 }, { 0x07, 5 }, { 0x17, 5 }, { 0x17, 6 }, { 0x57, 7 },
		{ 0x37, 6 }
	},
/* UPDATE_DR */
	{
		{ 0x07, 3 }, { 0x00, 1 }, { 0x01, 1 }, { 0x01, 2 }, { 0x01, 3 },
		{ 0x05, 3 }, { 0x05, 4 }, { 0x15, 5 }, { 0x00, 0 }, { 0x03, 2 },
		{ 0x03, 3 }, { 0x03, 4 }, { 0x0b, 4 }, { 0x0b, 5 }, { 0x2b, 6 },
		{ 0x1b, 5 }
	},
/* SELECT_IR */
	{
		{ 0x01, 1 }, { 0x01, 2 }, { 0x05, 3 }, { 0x05, 4 }, { 0x05, 5 },
		{ 0x15, 5 }, { 0x15, 6 }, { 0x55, 7 }, { 0x35, 6 }, { 0x00, 0 },
		{ 0x00, 1 }, { 0x00, 2 }, { 0x02, 2 }, { 0x02, 3 }, { 0x0a, 4 },
		{ 0x06, 3 }
	},
/* CAPTURE_IR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x07, 4 }, { 0x07, 5 },
		{ 0x17, 5 }, { 0x17, 6 }, { 0x57, 7 }, { 0x37, 6 }, { 0x0f, 4 },
		{ 0x00, 0 }, { 0x00, 1 }, { 0x01, 1 }, { 0x01, 2 }, { 0x05, 3 },
		{ 0x03, 2 }
	},
/* SHIFT_IR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x07, 4 }, { 0x07, 5 },
		{ 0x17, 5 }, { 0x17, 6 }, { 0x57, 7 }, { 0x37, 6 }, { 0x0f, 4 },
		{ 0x0f, 5 }, { 0x00, 0 }, { 0x01, 1 }, { 0x01, 2 }, { 0x05, 3 },
		{ 0x03, 2 }
	},
/* EXIT1_IR */
	{
		{ 0x0f, 4 }, { 0x01, 2 }, { 0x03, 2 }, { 0x03, 3 }, { 0x03, 4 },
		{ 0x0b, 4 }, { 0x0b, 5 }, { 0x2b, 6 }, { 0x1b, 5 }, { 0x07, 3 },
		{ 0x07, 4 }, { 0x02, 3 }, { 0x00, 0 }, { 0x00, 1 }, { 0x02, 2 },
		{ 0x01, 1 }
	},
/* PAUSE_IR */
	{
		{ 0x1f, 5 }, { 0x03, 3 }, { 0x07, 3 }, { 0x07, 4 }, { 0x07, 5 },
		{ 0x17, 5 }, { 0x17, 6 }, { 0x57, 7 }, { 0x37, 6 }, { 0x0f, 4 },
		{ 0x0f, 5 }, { 0x01, 2 }, { 0x05, 3 }, { 0x00, 1 }, { 0x01, 1 },
		{ 0x03, 2 }
	},
/* EXIT2_IR */
	{
		{ 0x0f, 4 }, { 0x01, 2 }, { 0x03, 2 }, { 0x03, 3 }, { 0x03, 4 },
		{ 0x0b, 4 }, { 0x0b, 5 }, { 0x2b, 6 }, { 0x1b, 5 }, { 0x07, 3 },
		{ 0x07, 4 }, { 0x00, 1 }, { 0x02, 2 }, { 0x02, 3 }, { 0x00, 0 },
		{ 0x01, 1 }
	},
/* UPDATE_IR */
	{
		{ 0x07, 3 }, { 0x00, 1 }, { 0x01, 1 }, { 0x01, 2 }, { 0x01, 3 },
		{ 0x05, 3 }, { 0x05, 4 }, { 0x15, 5 }, { 0x0d, 4 }, { 0x03, 2 },
		{ 0x03, 3 }, { 0x03, 4 }, { 0x0b, 4 }, { 0x0b, 5 }, { 0x2b, 6 },
		{ 0x00, 0 }
	},
};

const struct device *tck_dev, *tdi_dev, *tdo_dev, *tms_dev;
const gpio_pin_t tck_pin = CONFIG_TCK_PIN;
const gpio_pin_t tdi_pin = CONFIG_TDI_PIN;
const gpio_pin_t tdo_pin = CONFIG_TDO_PIN;
const gpio_pin_t tms_pin = CONFIG_TMS_PIN;


void jtag_npcm4xx_init(void)
{
	tck_dev = device_get_binding(CONFIG_TCK_DEV);
	tdi_dev = device_get_binding(CONFIG_TDI_DEV);
	tdo_dev = device_get_binding(CONFIG_TDO_DEV);
	tms_dev = device_get_binding(CONFIG_TMS_DEV);

	gpio_pin_configure(tck_dev, tck_pin, CONFIG_TCK_FLAG);
	gpio_pin_configure(tdi_dev, tdi_pin, CONFIG_TDI_FLAG);
	gpio_pin_configure(tdo_dev, tdo_pin, CONFIG_TDO_FLAG);
	gpio_pin_configure(tms_dev, tms_pin, CONFIG_TMS_FLAG);

	/* Test-Logic Reset state */
	gjtag.tap_state = TAP_RESET;
}

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

void jtag_npcm4xx_set_tap_state(enum jtag_tap_states from, enum jtag_tap_states to)
{
	uint8_t i;
	uint8_t tmsbits;
	uint8_t count;

	if (from == to) {
		return;
	}
	if (from == TAP_CURRENT_STATE) {
		from = gjtag.tap_state;
	}

	if (from > TAP_CURRENT_STATE || to > TAP_CURRENT_STATE) {
		return;
	}

	/* Reset to Test-Logic Reset state:
	 * Also notice that in whatever state the TAP controller may be at,
	 * it will goes back to this state if TMS is set to 1 for 5 consecutive TCK cycles.
	 */
	if (to == TAP_RESET) {
		for (i = 0; i < 9; i++) {
			jtag_npcm4xx_tck_cycle(1, 0);
		}
		gjtag.tap_state = TAP_RESET;
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

enum jtag_tap_states jtag_npcm4xx_get_tap_state(void)
{
	return gjtag.tap_state;
}

void jtag_npcm4xx_xfer_spi(uint32_t out_bytes_len, uint8_t *out_data,
			   uint32_t in_byte_len, uint8_t *in_data)
{

}

void jtag_npcm4xx_xfer_gpio(uint32_t out_bits_len, uint8_t *out_data,
			    uint32_t in_bits_len, uint8_t *in_data, uint8_t last_data)
{
	uint32_t bits_len;
	uint32_t bits_idx = 0;
	uint8_t tdi, tms;

	bits_len = (out_bits_len > in_bits_len) ? out_bits_len : in_bits_len;

	tms = 0;
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

void jtag_npcm4xx_readwrite_scan(uint32_t bits_len, uint8_t *out_data, uint8_t *in_data,
				 enum jtag_tap_states end_state)
{
#if (CONFIG_USE_SPI == 1)
	uint32_t spi_xfer_bytes = (bits_len / 8) - 1;
#else
	uint32_t spi_xfer_bytes = 0;
#endif
	uint32_t remain_bits = bits_len;

	if (spi_xfer_bytes) {
		jtag_npcm4xx_xfer_spi(spi_xfer_bytes, out_data, spi_xfer_bytes, in_data);
		remain_bits -= spi_xfer_bytes * 8;
	}

	if (remain_bits) {
		if (end_state != TAP_SHIFT_DR && end_state != TAP_SHIFT_IR &&
		    end_state != TAP_CURRENT_STATE) {
			jtag_npcm4xx_xfer_gpio(remain_bits, out_data + spi_xfer_bytes,
					       remain_bits, in_data + spi_xfer_bytes, 1);
			gjtag.tap_state = (gjtag.tap_state == TAP_SHIFT_DR) ?
					  TAP_EXIT1_DR : TAP_EXIT1_IR;
		} else {
			jtag_npcm4xx_xfer_gpio(remain_bits, out_data + spi_xfer_bytes,
					       remain_bits, in_data + spi_xfer_bytes, 0);
		}
	}

	jtag_npcm4xx_set_tap_state(TAP_CURRENT_STATE, end_state);
}

void jtag_npcm4xx_ir_scan(uint32_t bits_len, uint8_t *out_data, uint8_t *in_data,
			  enum jtag_tap_states end_state)
{
	if (bits_len == 0) {
		return;
	}

	jtag_npcm4xx_set_tap_state(TAP_CURRENT_STATE, TAP_SHIFT_IR);

	jtag_npcm4xx_readwrite_scan(bits_len, out_data, in_data, end_state);

}

void jtag_npcm4xx_dr_scan(uint32_t bits_len, uint8_t *out_data, uint8_t *in_data,
			  enum jtag_tap_states end_state)
{
	if (bits_len == 0) {
		return;
	}

	jtag_npcm4xx_set_tap_state(TAP_CURRENT_STATE, TAP_SHIFT_DR);

	jtag_npcm4xx_readwrite_scan(bits_len, out_data, in_data, end_state);

}

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
