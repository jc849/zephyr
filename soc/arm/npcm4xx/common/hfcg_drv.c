/*****************************************************************************/
/* @file        hfcg_drv.c                                                   */
/* @version     V1.00                                                        */
/* $Date:       2022/02/24 $                                                 */
/* @brief       NCT6694D High Frequency Clock Generator driver source file   */
/*                                                                           */
/* @note                                                                     */
/* Copyright (C) 2022 Nuvoton Technology Corp. All rights reserved.          */
/*****************************************************************************/

#include "NPCM4XX.h"
#include "system_NPCM4XX.h"
#include "hfcg_drv.h"

/*---------------------------------------------------------------------------*/
/**
 * @brief       Get the high-frequency multiplier clock (OFMCLK)
 * @return      High-frequency multiplier clock (OFMCLK) frequency,
 *              in 1 MHz resolution
 *
 * @details	This routine retrieves the high-frequency multiplier
 *              clock (OFMCLK), which is based on the 32 KHz Clock domain
 *              (32.768 KHz, low frequency (LFCLK) clock signal).
 *              OFMCLK is rounded to 1 MHz resolution.
 */
/*---------------------------------------------------------------------------*/
uint32_t HFCG_Get_OFMCLK(void)
{
	uint32_t N, division;
	uint32_t M;
	uint32_t freq, residue;

	N = HFCG->N & HFCG_N_HFCGN5_0_Msk;
	M = (HFCG->MH << 8) | HFCG->ML;

	/* OFMCLK = LFCG_CORE_CLK * (M / N) */
	freq = (LFCG_CORE_CLK * M) / N;

	/* Round frequency to a resolution of _1MHz_ */
	division = freq / 1000000;
	residue = freq % 1000000;

	/* In case the remainder is more than 50%, round up the division */
	if (residue >= (1000000 / 2)) {
		division += 1;
	}

	return (division * 1000000);
}

/*---------------------------------------------------------------------------------------------*/
/**
 * @brief                                               Get APB2 clock frequency
 * @return                                              APB2 clock frequency
 *
 * @details                                             This routine retrieves the APB2 clock.
 */
/*---------------------------------------------------------------------------------------------*/
uint32_t HFCG_Get_APB2_CLK(void)
{
	uint32_t OFMCLK;
	uint32_t APB2_clk;
	uint8_t APB2_div;

	OFMCLK = HFCG_Get_OFMCLK();
	APB2_div = ((HFCG->BCD & HFCG_BCD_APB2DIV_Msk) >> HFCG_BCD_APB2DIV_Pos) + 1;
	APB2_clk = OFMCLK / APB2_div;

	return APB2_clk;
}
