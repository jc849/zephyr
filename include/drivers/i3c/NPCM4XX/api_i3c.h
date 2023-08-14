/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef	API_I3C_H
#define	API_I3C_H

#include <drivers/i3c/NPCM4XX/pub_I3C.h>

#ifdef __cplusplus
extern "C"{
#endif

/* To Init I3C Bus */
extern void api_I3C_Reset(__u8 busNo);
extern I3C_ErrCode_Enum api_I3C_START(void);

extern I3C_ErrCode_Enum api_I3C_bus_reset_done(__u8 busNo);
extern I3C_ErrCode_Enum api_I3C_bus_clear_done(__u8 busNo);

extern I3C_ErrCode_Enum api_I3C_connect_bus(I3C_PORT_Enum port, __u8 busNo);
extern I3C_ErrCode_Enum api_I3C_set_pReg(I3C_PORT_Enum port, I3C_REG_ITEM_t *pReg);

/* Device Node Management */
extern I3C_DEVICE_INFO_SHORT_t *api_I3C_GetDevInfoByStaticAddr(I3C_BUS_INFO_t *pBus,
	__u8 slaveAddr);
extern I3C_DEVICE_INFO_SHORT_t *api_I3C_GetDevInfoByDynamicAddr(I3C_BUS_INFO_t *pBus,
	__u8 slaveAddr);

extern I3C_DEVICE_INFO_t *api_I3C_Get_INODE(I3C_PORT_Enum port);
extern I3C_PORT_Enum api_I3C_Get_IPORT(I3C_DEVICE_INFO_t *pDevice);

extern I3C_DEVICE_INFO_t *api_I3C_Get_Current_Master_From_Port(I3C_PORT_Enum port);
extern I3C_BUS_INFO_t *api_I3C_Get_Bus_From_Port(I3C_PORT_Enum port);

/* Task Process */
extern __u8 I3C_Task_Engine(void);

extern void api_I3C_Master_Start_Request(__u32 Parm);
extern void api_I3C_Master_Stop(__u32 Parm);
extern void api_I3C_Master_Run_Next_Frame(__u32 Parm);

extern void api_I3C_Master_New_Request(__u32 Parm);
extern void api_I3C_Master_Insert_DISEC_After_IbiNack(__u32 Parm);
extern void api_I3C_Master_IBIACK(__u32 Parm);
extern void api_I3C_Master_Insert_GETACCMST_After_IbiAckMR(__u32 Parm);
extern void api_I3C_Master_Insert_ENTDAA_After_IbiAckHJ(__u32 Parm);

extern void api_I3C_Master_End_Request(__u32 Parm);

extern I3C_ErrCode_Enum api_I3C_Master_Callback(__u32 TaskInfo, I3C_ErrCode_Enum ErrDetail);

extern void api_I3C_Slave_Start_Request(__u32 Parm);

extern I3C_ErrCode_Enum api_I3C_Slave_Prepare_Response(I3C_DEVICE_INFO_t *pDevice, __u16 wrLen,
	__u8 *pWrBuf);
extern I3C_ErrCode_Enum api_I3C_Slave_Update_Pending(I3C_DEVICE_INFO_t *pDevice, __u8 mask);
extern I3C_ErrCode_Enum api_I3C_Slave_Finish_Response(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum api_I3C_Slave_Check_Response_Complete(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum api_I3C_Slave_Check_IBIDIS(I3C_DEVICE_INFO_t *pDevice, _Bool *bRet);

extern void api_I3C_Slave_End_Request(__u32 Parm);
extern I3C_ErrCode_Enum api_I3C_Slave_Callback(__u32 TaskInfo, I3C_ErrCode_Enum ErrDetail);


/* Task Creation */
extern I3C_TASK_INFO_t *api_I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u8 HSize, __u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF);
extern I3C_TASK_INFO_t *api_I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, _Bool bHIF);

extern I3C_ErrCode_Enum api_I3C_Master_Insert_Task_ENTDAA(__u16 rxbuf_size, __u8 *rxbuf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCb(__u8 CCC, __u16 buf_size, __u8 *buf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCw(__u8 CCC, __u8 HSize, __u16 buf_size,
	__u8 *buf, __u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCr(__u8 CCC, __u8 HSize, __u16 txbuf_size,
	__u16 rxbuf_size, __u8 *txbuf, __u8 *rxbuf, __u32 Baudrate, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy, _Bool bHIF);

extern I3C_ErrCode_Enum api_ValidateBuffer(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Address,
	__u8 HSize, __u16 WrCnt, __u16 RdCnt, __u8 *WrBuf, __u8 *RdBuf, _Bool bHIF);

/* DMA */
extern I3C_ErrCode_Enum api_I3C_Setup_Master_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum api_I3C_Setup_Master_Read_DMA(I3C_DEVICE_INFO_t *pDevice);

extern I3C_ErrCode_Enum api_I3C_Setup_Slave_Write_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum api_I3C_Setup_Slave_Read_DMA(I3C_DEVICE_INFO_t *pDevice);
extern I3C_ErrCode_Enum api_I3C_Setup_Slave_IBI_DMA(I3C_DEVICE_INFO_t *pDevice);

#ifdef __cplusplus
}
#endif

#endif	/* End of #ifndef API_I3C_H */
