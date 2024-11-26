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

/*==========================================================================*/
#define I3C_GET_REG_MCONFIG(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCONFIG)
#define I3C_SET_REG_MCONFIG(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MCONFIG)

#define I3C_GET_REG_CONFIG(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CONFIG)
#define I3C_SET_REG_CONFIG(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_CONFIG)

#define I3C_GET_REG_STATUS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_STATUS)
#define I3C_SET_REG_STATUS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_STATUS)

#define I3C_GET_REG_CTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CTRL)
#define I3C_SET_REG_CTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_CTRL)

#define I3C_GET_REG_INTSET(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_INTSET)
#define I3C_SET_REG_INTSET(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_INTSET)

#define I3C_SET_REG_INTCLR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_INTCLR)

#define I3C_GET_REG_INTMASKED(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_INTMASKED)

#define I3C_GET_REG_ERRWARN(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_ERRWARN)
#define I3C_SET_REG_ERRWARN(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_ERRWARN)

#define I3C_GET_REG_DMACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DMACTRL)
#define I3C_SET_REG_DMACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_DMACTRL)

#define I3C_GET_REG_DATACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DATACTRL)
#define I3C_SET_REG_DATACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_DATACTRL)

#define I3C_SET_REG_WDATAB(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAB)
#define I3C_SET_REG_WDATABE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATABE)
#define I3C_SET_REG_WDATAH(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAH)
#define I3C_SET_REG_WDATAHE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAHE)

#define I3C_GET_REG_RDATAB(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_RDATAB)
#define I3C_GET_REG_RDATAH(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_RDATAH)

#define I3C_SET_REG_WDATAB1(port, val) sys_write8(val, I3C_BASE_ADDR(port) + OFFSET_WDATAB1)

#define I3C_GET_REG_CAPABILITIES(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CAPABILITIES)
#define I3C_GET_REG_DYNADDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DYNADDR)

#define I3C_GET_REG_MAXLIMITS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MAXLIMITS)
#define I3C_SET_REG_MAXLIMITS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MAXLIMITS)

#define I3C_GET_REG_PARTNO(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_PARTNO)
#define I3C_SET_REG_PARTNO(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_PARTNO)

#define I3C_GET_REG_IDEXT(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IDEXT)
#define I3C_SET_REG_IDEXT(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IDEXT)

#define I3C_GET_REG_VENDORID(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_VENDORID)
#define I3C_SET_REG_VENDORID(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_VENDORID)

#define I3C_GET_REG_TCCLOCK(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_TCCLOCK)
#define I3C_SET_REG_TCCLOCK(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_TCCLOCK)

#define I3C_GET_REG_MCTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCTRL)
#define I3C_SET_REG_MCTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MCTRL)

#define I3C_GET_REG_MSTATUS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MSTATUS)
#define I3C_SET_REG_MSTATUS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MSTATUS)

#define I3C_GET_REG_IBIRULES(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIRULES)
#define I3C_SET_REG_IBIRULES(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIRULES)

#define I3C_GET_REG_MINTSET(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MINTSET)
#define I3C_SET_REG_MINTSET(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MINTSET)

#define I3C_SET_REG_MINTCLR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MINTCLR)

#define I3C_GET_REG_MINTMASKED(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MINTMASKED)

#define I3C_GET_REG_MERRWARN(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MERRWARN)
#define I3C_SET_REG_MERRWARN(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MERRWARN)

#define I3C_GET_REG_MDMACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDMACTRL)
#define I3C_SET_REG_MDMACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDMACTRL)

#define I3C_GET_REG_MDATACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDATACTRL)
#define I3C_SET_REG_MDATACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDATACTRL)

#define I3C_SET_REG_MWDATAB(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAB)
#define I3C_SET_REG_MWDATABE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATABE)
#define I3C_SET_REG_MWDATAH(port, val) sys_write32(val. I3C_BASE_ADDR(port) + OFFSET_MWDATAH)
#define I3C_SET_REG_MWDATAHE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAHE)

#define I3C_GET_REG_MRDATAB(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRDATAB)
#define I3C_GET_REG_MRDATAH(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRDATAH)

#define I3C_SET_REG_MWDATAB1(port, val) sys_write8(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAB1)

#define I3C_SET_REG_MWMSG_SDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWMSG_SDR)

#define I3C_GET_REG_MRMSG_SDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRMSG_SDR)

#define I3C_SET_REG_MWMSG_DDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWMSG_DDR)

#define I3C_GET_REG_MRMSG_DDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MWMSG_DDR)

#define I3C_GET_REG_MDYNADDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDYNADDR)
#define I3C_SET_REG_MDYNADDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDYNADDR)

#define I3C_GET_REG_HDRCMD(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_HDRCMD)

#define I3C_GET_REG_IBIEXT1(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIEXT1)
#define I3C_SET_REG_IBIEXT1(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIEXT1)

#define I3C_GET_REG_IBIEXT2(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIEXT2)
#define I3C_SET_REG_IBIEXT2(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIEXT2)

#define I3C_GET_REG_ID(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_ID)
/*==========================================================================*/


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

#ifdef __cplusplus
}
#endif

#endif
