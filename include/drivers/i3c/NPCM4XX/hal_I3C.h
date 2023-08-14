/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __HAL_I3C_H__
#define __HAL_I3C_H__

#include <drivers/i3c/NPCM4XX/pub_I3C.h>
#include <drivers/i3c/NPCM4XX/i3c_drv.h>

#if (CHIP_ID_NPCM4XX == 5832)
    /*#define I3C_PORT1_MODE      I3C_DEVICE_MODE_CURRENT_MASTER*/
    /*#define I3C_PORT1_ADDR      0x20*/
    #define I3C_PORT1_PARTNO    0x12345678
    #define I3C_PORT1_BCR       0x66
    #define I3C_PORT1_DCR  0xCC
    /*#define I3C_PORT1_BUSNO 0*/

    /*#define I3C_PORT2_MODE      I3C_DEVICE_MODE_CURRENT_MASTER*/
    /*#define I3C_PORT2_ADDR      0x20*/
    #define I3C_PORT2_PARTNO    0x12345678
    #define I3C_PORT2_BCR       0x66
    #define I3C_PORT2_DCR  0xCC
    /*#define I3C_PORT2_BUSNO 1*/

#else
    #define I3C_PORT1_MODE  I3C_DEVICE_MODE_CURRENT_MASTER
    #define I3C_PORT1_ADDR  0x20
    #define I3C_PORT1_PARTNO  0x12345678
    #define I3C_PORT1_BCR  0x66
    #define I3C_PORT1_DCR  0xCC
    #define I3C_PORT1_BUSNO 0

    #define I3C_PORT2_MODE  I3C_DEVICE_MODE_SLAVE_ONLY
    #define I3C_PORT2_ADDR  0x21
    #define I3C_PORT2_PARTNO  0x12345678
    #define I3C_PORT2_BCR  0x66
    #define I3C_PORT2_DCR  0xCC
    #define I3C_PORT2_BUSNO 0

    #define I3C_PORT3_MODE  I3C_DEVICE_MODE_CURRENT_MASTER
    #define I3C_PORT3_ADDR  0x22
    #define I3C_PORT3_PARTNO  0x12345678
    #define I3C_PORT3_BCR  0x66
    #define I3C_PORT3_DCR  0xCC
    #define I3C_PORT3_BUSNO 1

    #define I3C_PORT4_MODE  I3C_DEVICE_MODE_SLAVE_ONLY
    #define I3C_PORT4_ADDR  0x23
    #define I3C_PORT4_PARTNO  0x12345678
    #define I3C_PORT4_BCR  0x66
    #define I3C_PORT4_DCR  0xCC
    #define I3C_PORT4_BUSNO 1

    #define I3C_PORT5_MODE  I3C_DEVICE_MODE_CURRENT_MASTER
    #define I3C_PORT5_ADDR  0x24
    #define I3C_PORT5_PARTNO  0x12345678
    #define I3C_PORT5_BCR  0x66
    #define I3C_PORT5_DCR  0xCC
    #define I3C_PORT5_BUSNO 2

    #define I3C_PORT6_MODE  I3C_DEVICE_MODE_SLAVE_ONLY
    #define I3C_PORT6_ADDR  0x25
    #define I3C_PORT6_PARTNO  0x12345678
    #define I3C_PORT6_BCR  0x66
    #define I3C_PORT6_DCR  0xCC
    #define I3C_PORT6_BUSNO 2
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* called by I3C_Setup_Internal_Device */
extern void hal_I3C_Config_Internal_Device(I3C_PORT_Enum port, I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum hal_I3C_Config_Device(I3C_DEVICE_INFO_t *pDevice);

extern I3C_ErrCode_Enum hal_I3C_enable_interface(I3C_PORT_Enum Port);
extern I3C_ErrCode_Enum hal_I3C_disable_interface(I3C_PORT_Enum Port);

extern I3C_ErrCode_Enum hal_I3C_enable_interrupt(I3C_PORT_Enum Port);
extern I3C_ErrCode_Enum hal_I3C_disable_interrupt(I3C_PORT_Enum Port);

extern __u32 hal_I3C_get_mstatus(I3C_PORT_Enum Port);
extern void hal_I3C_set_mstatus(I3C_PORT_Enum Port, __u32 val);
extern I3C_IBITYPE_Enum hal_I3C_get_ibiType(I3C_PORT_Enum Port);
extern __u8 hal_I3C_get_ibiAddr(I3C_PORT_Enum Port);
extern _Bool hal_I3C_Is_Master_Idle(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Slave_Idle(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Master_DAA(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Slave_DAA(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Master_SLVSTART(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Slave_SLVSTART(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Master_NORMAL(I3C_PORT_Enum port);
extern _Bool hal_I3C_Is_Master_DDR(I3C_PORT_Enum port);

extern void hal_I3C_Nack_IBI(I3C_PORT_Enum port);
extern void hal_I3C_Ack_IBI_Without_MDB(I3C_PORT_Enum port);
extern void hal_I3C_Ack_IBI_With_MDB(I3C_PORT_Enum port);

extern void hal_I3C_Disable_Master_TX_DMA(I3C_PORT_Enum port);
extern void hal_I3C_Disable_Master_RX_DMA(I3C_PORT_Enum port);
extern void hal_I3C_Disable_Master_DMA(I3C_PORT_Enum port);
extern void hal_I3C_Reset_Master_TX(I3C_PORT_Enum port);

extern _Bool hal_I3C_TxFifoNotFull(I3C_PORT_Enum Port);
extern void hal_I3C_WriteByte(I3C_PORT_Enum Port, __u8 val);
extern _Bool hal_I3C_DMA_Write(I3C_PORT_Enum Port, I3C_DEVICE_MODE_Enum mode,
	__u8 *txBuf, __u16 txLen);
extern _Bool hal_I3C_DMA_Read(I3C_PORT_Enum Port, I3C_DEVICE_MODE_Enum mode,
	__u8 *rxBuf, __u16 rxLen);

extern __u8 hal_i3c_get_dynamic_address(I3C_PORT_Enum port);

extern _Bool hal_i3c_get_capability_support_master(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_slave(I3C_PORT_Enum port);
extern __u16 hal_i3c_get_capability_Tx_Fifo_Len(I3C_PORT_Enum port);
extern __u16 hal_i3c_get_capability_Rx_Fifo_Len(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_DMA(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_INT(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_DDR(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_ASYNC0(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_IBI(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_Master_Request(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_Hot_Join(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_Offline(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_V10(I3C_PORT_Enum port);
extern _Bool hal_i3c_get_capability_support_V11(I3C_PORT_Enum port);

extern __u8 hal_I3C_get_event_support_status(I3C_PORT_Enum Port);
extern __u32 hal_I3C_get_status(I3C_PORT_Enum Port);
extern _Bool hal_I3C_run_ASYN0(I3C_PORT_Enum port);

extern I3C_ErrCode_Enum hal_I3C_Process_Task(I3C_TASK_INFO_t *pTaskInfo);
extern I3C_ErrCode_Enum hal_I3C_Stop(I3C_PORT_Enum port);

extern I3C_ErrCode_Enum hal_I3C_Slave_TX_Free(I3C_PORT_Enum port);
extern I3C_ErrCode_Enum hal_I3C_Set_Pending(I3C_PORT_Enum port, __u8 mask);
extern I3C_ErrCode_Enum hal_I3C_Slave_Query_TxLen(I3C_PORT_Enum port, uint16_t *txLen);
extern I3C_ErrCode_Enum hal_I3C_Stop_Slave_TX(I3C_DEVICE_INFO_t *pDevice);

extern I3C_ErrCode_Enum hal_I3C_Start_IBI(I3C_TASK_INFO_t *pTaskInfo);
extern I3C_ErrCode_Enum hal_I3C_Start_Master_Request(I3C_TASK_INFO_t *pTaskInfo);
extern I3C_ErrCode_Enum hal_I3C_Start_HotJoin(I3C_TASK_INFO_t *pTaskInfo);

extern void *hal_I3C_MemAlloc(__u32 Size);
extern void hal_I3C_MemFree(void *pv);

extern void hal_I3C_Master_Stall(I3C_BUS_INFO_t *pBus, I3C_PORT_Enum port);

#ifdef __cplusplus
}
#endif

#endif
