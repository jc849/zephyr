/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_MASTER_H__
#define __I3C_MASTER_H__

#include "pub_i3c.h"

#ifdef __cplusplus
"C"
{
#endif

uint32_t I3C_DO_NACK(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_WRABT(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SLVSTART(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SW_TIMEOUT(I3C_TASK_INFO_t *pTaskInfo);

uint32_t I3C_DO_IBI(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_HOT_JOIN(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_MASTER_REQUEST(I3C_TASK_INFO_t *pTaskInfo);

uint32_t I3C_DO_ENEC(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_DISEC(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_RSTDAA(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_ENTDAA(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SETAASA(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SETHID(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SETDASA(I3C_TASK_INFO_t *pTaskInfo);
uint32_t I3C_DO_SETNEWDA(I3C_TASK_INFO_t *pTaskInfo);

/* Operation */
void I3C_Master_Start_Request(uint32_t Parm);
void I3C_Master_Stop_Request(uint32_t Parm);
void I3C_Master_Retry_Frame(uint32_t Parm);
void I3C_Master_End_Request(uint32_t Parm);
void I3C_Master_Run_Next_Frame(uint32_t Parm);
void I3C_Master_New_Request(uint32_t Parm);
void I3C_Master_Insert_DISEC_After_IbiNack(uint32_t Parm);
void I3C_Master_IBIACK(uint32_t Parm);
void I3C_Master_Insert_GETACCMST_After_IbiAckMR(uint32_t Parm);
void I3C_Master_Insert_ENTDAA_After_IbiAckHJ(uint32_t Parm);

/* PDMA */
I3C_ErrCode_Enum Setup_Master_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
I3C_ErrCode_Enum Setup_Master_Read_DMA(I3C_DEVICE_INFO_t *pDevice);

#ifdef __cplusplus
}
#endif

#endif /* __I3C_MASTER_H__ */
