/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_SLAVE_H__
#define __I3C_SLAVE_H__

#include "pub_i3c.h"

#ifdef __cplusplus
"C"
{
#endif

/* response */
uint32_t I3C_DO_NACK_SLVSTART(I3C_TASK_INFO_t *pTaskInfo);

/* Operation */
void I3C_Slave_Start_Request(uint32_t Parm);
void I3C_Slave_End_Request(uint32_t Parm);

I3C_ErrCode_Enum I3C_Slave_Prepare_Response(I3C_DEVICE_INFO_t *pDevice,
	uint16_t wrLen, uint8_t *pWrBuf);
I3C_ErrCode_Enum I3C_Slave_Update_Pending(I3C_DEVICE_INFO_t *pDevice, uint8_t mask);
I3C_ErrCode_Enum I3C_Slave_Finish_Response(I3C_DEVICE_INFO_t *pDevice);
I3C_ErrCode_Enum I3C_Slave_Check_Response_Complete(I3C_DEVICE_INFO_t *pDevice);
I3C_ErrCode_Enum I3C_Slave_Check_IBIDIS(I3C_DEVICE_INFO_t *pDevice, bool *bRet);

/* PDMA */
I3C_ErrCode_Enum Setup_Slave_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
I3C_ErrCode_Enum Setup_Slave_IBI_DMA(I3C_DEVICE_INFO_t *pDevice);

uint8_t I3C_Update_Dynamic_Address(uint32_t Parm);
void I3C_Prepare_To_Read_Command(uint32_t Parm);
uint8_t GetCmdWidth(uint8_t width_type);

#ifdef __cplusplus
}
#endif

#endif /* __I3C_SLAVE_H__ */
