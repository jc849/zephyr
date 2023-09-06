/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_SLAVE_H__
#define __I3C_SLAVE_H__

#include <drivers/i3c/NPCM4XX/pub_I3C.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern __u32 I3C_Slave_Callback(__u32 TaskInfo, __u32 ErrDetail);

/* response */
extern __u32 I3C_DO_NACK_SLVSTART(I3C_TASK_INFO_t *pTaskInfo);

/* Operation */
extern void I3C_Slave_Start_Request(__u32 Parm);
extern void I3C_Slave_End_Request(__u32 Parm);

extern I3C_ErrCode_Enum I3C_Slave_Prepare_Response(I3C_DEVICE_INFO_t *pDevice,
	__u16 wrLen, __u8 *pWrBuf);
extern I3C_ErrCode_Enum I3C_Slave_Update_Pending(I3C_DEVICE_INFO_t *pDevice, __u8 mask);
extern I3C_ErrCode_Enum I3C_Slave_Finish_Response(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum I3C_Slave_Check_Response_Complete(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum I3C_Slave_Check_IBIDIS(I3C_DEVICE_INFO_t *pDevice, _Bool *bRet);

/* PDMA */
extern I3C_ErrCode_Enum Setup_Slave_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum Setup_Slave_Read_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum Setup_Slave_IBI_DMA(I3C_DEVICE_INFO_t *pDevice);

extern uint8_t I3C_Update_Dynamic_Address(__u32 Parm);
extern void I3C_Prepare_To_Read_Command(__u32 Parm);
extern uint8_t GetCmdWidth(uint8_t width_type);

#ifdef __cplusplus
}
#endif

#endif /* __I3C_SLAVE_H__ */
