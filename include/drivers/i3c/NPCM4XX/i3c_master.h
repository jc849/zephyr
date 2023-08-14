/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_MASTER_H__
#define __I3C_MASTER_H__

#include <drivers/i3c/NPCM4XX/pub_I3C.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern __u32 I3C_Master_Callback(__u32 TaskInfo, __u32 ErrDetail);

extern __u32 I3C_DO_NACK(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_WRABT(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SLVSTART(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SW_TIMEOUT(I3C_TASK_INFO_t *pTaskInfo);

extern __u32 I3C_DO_IBI(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_HOT_JOIN(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_MASTER_REQUEST(I3C_TASK_INFO_t *pTaskInfo);

extern __u32 I3C_DO_ENEC(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_DISEC(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_RSTDAA(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_ENTDAA(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SETAASA(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SETHID(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SETDASA(I3C_TASK_INFO_t *pTaskInfo);
extern __u32 I3C_DO_SETNEWDA(I3C_TASK_INFO_t *pTaskInfo);

/* Operation */
extern void I3C_Master_Start_Request(__u32 Parm);
extern void I3C_Master_Stop_Request(__u32 Parm);
extern void I3C_Master_End_Request(__u32 Parm);
extern void I3C_Master_Run_Next_Frame(__u32 Parm);
extern void I3C_Master_New_Request(__u32 Parm);
extern void I3C_Master_Insert_DISEC_After_IbiNack(__u32 Parm);
extern void I3C_Master_IBIACK(__u32 Parm);
extern void I3C_Master_Insert_GETACCMST_After_IbiAckMR(__u32 Parm);
extern void I3C_Master_Insert_ENTDAA_After_IbiAckHJ(__u32 Parm);

/* PDMA */
extern I3C_ErrCode_Enum Setup_Master_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum Setup_Master_Read_DMA(I3C_DEVICE_INFO_t *pDevice);

#ifdef __cplusplus
}
#endif

#endif /* __I3C_MASTER_H__ */
