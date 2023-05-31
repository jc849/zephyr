/***************************************************************************/
/* @file        hfcg_drv.h                                                 */
/* @version     V1.00                                                      */
/* $Date:       2022/02/24 $                                               */
/* @brief       NCT6694D High Frequency Clock Generator driver header file */
/*                                                                         */
/* @note                                                                   */
/* Copyright (C) 2022 Nuvoton Technology Corp. All rights reserved.        */
/***************************************************************************/

#ifndef __HFCG_DRV_H__
#define __HFCG_DRV_H__

#ifdef __cplusplus
extern "C" {
#endif

/***** Definitions *********************************************************/
enum OFMCLK_TYPE { HFCG_OFMCLK_48M = 0, HFCG_OFMCLK_40M, HFCG_OFMCLK_33M };

/**
 * @brief                                               Get APB2 clock frequency
 * @return                                              APB2 clock frequency
 *
 * @details                                             This routine retrieves the APB2 clock.
 */
uint32_t HFCG_Get_APB2_CLK(void);

#ifdef __cplusplus
}
#endif

#endif /* __HFCG_DRV_H__ */

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
