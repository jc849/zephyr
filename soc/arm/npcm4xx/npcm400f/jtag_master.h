/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JTAG_MASTER_H_
#define _JTAG_MASTER_H_

#include <soc.h>

enum jtag_tap_states {
	TAP_RESET,      /* 0: Test-Logic Reset */
	TAP_IDLE,       /* 1: Run-Test/ Idle */

	TAP_SELECT_DR,  /* 2: Select-DR/ Scan */
	TAP_CAPTURE_DR, /* 3: Capture-DR */
	TAP_SHIFT_DR,   /* 4: Shift-DR */
	TAP_EXIT1_DR,   /* 5: Exit1-DR */
	TAP_PAUSE_DR,   /* 6: Pause-DR */
	TAP_EXIT2_DR,   /* 7: Exit2-DR */
	TAP_UPDATE_DR,  /* 8: Update-DR */

	TAP_SELECT_IR,  /* 9: Select-IR/ Scan */
	TAP_CAPTURE_IR, /* 10: Capture-IR */
	TAP_SHIFT_IR,   /* 11: Shift-IR */
	TAP_EXIT1_IR,   /* 12: Exit1-IR */
	TAP_PAUSE_IR,   /* 13: Pause-IR */
	TAP_EXIT2_IR,   /* 14: Exit2-IR */
	TAP_UPDATE_IR,  /* 15: Update-IR */

	TAP_CURRENT_STATE
};

#ifdef __cplusplus
extern "C" {
#endif

void jtag_npcm4xx_init(void);
void jtag_npcm4xx_xfer_gpio(uint32_t out_bits_len, uint8_t *out_data,
			    uint32_t in_bits_len, uint8_t *in_data, uint8_t last_data);
void jtag_npcm4xx_set_tap_state(enum jtag_tap_states from, enum jtag_tap_states to);
void jtag_npcm4xx_ir_scan(uint32_t bits_len, uint8_t *out_data, uint8_t *in_data,
			  enum jtag_tap_states end_state);
void jtag_npcm4xx_dr_scan(uint32_t bits_len, uint8_t *out_data, uint8_t *in_data,
			  enum jtag_tap_states end_state);
enum jtag_tap_states jtag_npcm4xx_get_tap_state(void);

#ifdef __cplusplus
}
#endif


#endif /* _JTAG_MASTER_H_ */
