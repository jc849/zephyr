/**************************************************************************/ /**
 * @file        system_NCT6694D.h
 * @version     V1.00
 * $Date:       2022/02/24 $
 * @brief       NCT6694D system definition file
 *
 * @note
 * Copyright (C) 2022 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#ifndef __SYSTEM_NPCM4XX_H__
#define __SYSTEM_NPCM4XX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "hfcg_drv.h"

/******************************************************************************/
/*                            Clocks                                          */
/******************************************************************************/
#define LFCG_CORE_CLK 32768 /** Core Clock Low Frequency */

#define APB2_CLK HFCG_Get_APB2_CLK() /** ARM Peripheral Bus 2    */

#define SOURCE_CLK_CR_UART APB2_CLK

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_NPCM4XX_H__ */
/*** (C) COPYRIGHT 2022 Nuvoton Technology Corp. ***/
