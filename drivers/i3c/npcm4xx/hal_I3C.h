/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __HAL_I3C_H__
#define __HAL_I3C_H__

#include "pub_I3C.h"
#include "i3c_drv.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* called by I3C_Setup_Internal_Device */
void hal_I3C_Config_Internal_Device(I3C_PORT_Enum port, I3C_DEVICE_INFO_t *pDevice);
I3C_ErrCode_Enum hal_I3C_Config_Device(I3C_DEVICE_INFO_t *pDevice);

I3C_ErrCode_Enum hal_I3C_enable_interface(I3C_PORT_Enum Port);
I3C_ErrCode_Enum hal_I3C_disable_interface(I3C_PORT_Enum Port);

I3C_ErrCode_Enum hal_I3C_enable_interrupt(I3C_PORT_Enum Port);
I3C_ErrCode_Enum hal_I3C_disable_interrupt(I3C_PORT_Enum Port);

uint32_t hal_I3C_get_MAXRD(I3C_PORT_Enum Port);
void hal_I3C_set_MAXRD(I3C_PORT_Enum Port, uint32_t val);

uint32_t hal_I3C_get_MAXWR(I3C_PORT_Enum Port);
void hal_I3C_set_MAXWR(I3C_PORT_Enum Port, uint32_t val);

uint32_t hal_I3C_get_mstatus(I3C_PORT_Enum Port);
void hal_I3C_set_mstatus(I3C_PORT_Enum Port, uint32_t val);
I3C_IBITYPE_Enum hal_I3C_get_ibiType(I3C_PORT_Enum Port);
uint8_t hal_I3C_get_ibiAddr(I3C_PORT_Enum Port);
bool hal_I3C_Is_Master_Idle(I3C_PORT_Enum port);
bool hal_I3C_Is_Slave_Idle(I3C_PORT_Enum port);
bool hal_I3C_Is_Master_DAA(I3C_PORT_Enum port);
bool hal_I3C_Is_Slave_DAA(I3C_PORT_Enum port);
bool hal_I3C_Is_Master_SLVSTART(I3C_PORT_Enum port);
bool hal_I3C_Is_Slave_SLVSTART(I3C_PORT_Enum port);
bool hal_I3C_Is_Master_NORMAL(I3C_PORT_Enum port);
bool hal_I3C_Is_Master_DDR(I3C_PORT_Enum port);

void hal_I3C_Nack_IBI(I3C_PORT_Enum port);
void hal_I3C_Ack_IBI_Without_MDB(I3C_PORT_Enum port);
void hal_I3C_Ack_IBI_With_MDB(I3C_PORT_Enum port);

void hal_I3C_Disable_Master_TX_DMA(I3C_PORT_Enum port);
void hal_I3C_Disable_Master_RX_DMA(I3C_PORT_Enum port);
void hal_I3C_Disable_Master_DMA(I3C_PORT_Enum port);
void hal_I3C_Reset_Master_TX(I3C_PORT_Enum port);

bool hal_I3C_TxFifoNotFull(I3C_PORT_Enum Port);
void hal_I3C_WriteByte(I3C_PORT_Enum Port, uint8_t val);
bool hal_I3C_DMA_Write(I3C_PORT_Enum Port, I3C_DEVICE_MODE_Enum mode,
	uint8_t *txBuf, uint16_t txLen);
bool hal_I3C_DMA_Read(I3C_PORT_Enum Port, I3C_DEVICE_MODE_Enum mode,
	uint8_t *rxBuf, uint16_t rxLen);

uint8_t hal_i3c_get_dynamic_address(I3C_PORT_Enum port);

bool hal_i3c_get_capability_support_master(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_slave(I3C_PORT_Enum port);
uint16_t hal_i3c_get_capability_Tx_Fifo_Len(I3C_PORT_Enum port);
uint16_t hal_i3c_get_capability_Rx_Fifo_Len(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_DMA(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_INT(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_DDR(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_ASYNC0(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_IBI(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_Master_Request(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_Hot_Join(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_Offline(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_V10(I3C_PORT_Enum port);
bool hal_i3c_get_capability_support_V11(I3C_PORT_Enum port);

uint8_t hal_I3C_get_event_support_status(I3C_PORT_Enum Port);
uint32_t hal_I3C_get_status(I3C_PORT_Enum Port);
bool hal_I3C_run_ASYN0(I3C_PORT_Enum port);

I3C_ErrCode_Enum hal_I3C_Process_Task(I3C_TASK_INFO_t *pTaskInfo);
I3C_ErrCode_Enum hal_I3C_Stop(I3C_PORT_Enum port);

I3C_ErrCode_Enum hal_I3C_Slave_TX_Free(I3C_PORT_Enum port);
I3C_ErrCode_Enum hal_I3C_Set_Pending(I3C_PORT_Enum port, uint8_t mask);
I3C_ErrCode_Enum hal_I3C_Slave_Query_TxLen(I3C_PORT_Enum port, uint16_t *txLen);
I3C_ErrCode_Enum hal_I3C_Stop_Slave_TX(I3C_DEVICE_INFO_t *pDevice);

I3C_ErrCode_Enum hal_I3C_Start_IBI(I3C_TASK_INFO_t *pTaskInfo);
I3C_ErrCode_Enum hal_I3C_Start_Master_Request(I3C_TASK_INFO_t *pTaskInfo);
I3C_ErrCode_Enum hal_I3C_Start_HotJoin(I3C_TASK_INFO_t *pTaskInfo);

void *hal_I3C_MemAlloc(uint32_t Size);
void hal_I3C_MemFree(void *pv);

void hal_I3C_Master_Stall(I3C_BUS_INFO_t *pBus, I3C_PORT_Enum port);

#ifdef __cplusplus
}
#endif

#endif
