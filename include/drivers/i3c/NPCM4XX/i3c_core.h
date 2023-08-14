/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_CORE_H__
#define __I3C_CORE_H__

#include <drivers/i3c/NPCM4XX/pub_I3C.h>

/*
 * variable
 */
extern I3C_BUS_INFO_t gBus[];
extern I3C_DEVICE_INFO_t gI3c_dev_node_internal[];

#define MAX_READ_LEN I3C_PAYLOAD_SIZE_MAX
extern uint8_t slvRxBuf[/*I3C_PORT_MAX*/][MAX_READ_LEN];
extern uint16_t slvRxLen[];
extern uint16_t slvRxOffset[];


#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Used for project customizztion
 */
extern void I3C_Setup_Internal_Device(void);
	extern I3C_DEVICE_INFO_t *I3C_Get_INODE(I3C_PORT_Enum port);
	extern I3C_DEVICE_INFO_t *Get_Current_Master_From_Port(I3C_PORT_Enum port);
	extern I3C_PORT_Enum I3C_Get_IPORT(I3C_DEVICE_INFO_t *pDevice);

	extern _Bool IsValidDynamicAddress(__u8 addr);
	extern _Bool IsValidStaticAddress(__u8 addr);

	extern I3C_ErrCode_Enum I3C_Port_Default_Setting(I3C_PORT_Enum port);

extern void I3C_Setup_External_Device(void);
extern I3C_ErrCode_Enum I3C_Setup_Bus(void);


/*
 * Used for bus operations
 */
extern I3C_BUS_INFO_t *Get_Bus_From_BusNo(__u8 busno);
extern I3C_BUS_INFO_t *Get_Bus_From_Port(I3C_PORT_Enum port);

extern void BUS_RESET_PORT1(void);
extern void BUS_RESET_PORT2(void);
extern void BUS_CLEAR_PORT1(void);
extern void BUS_CLEAR_PORT2(void);

/* bus status check */
extern _Bool I3C_IS_BUS_KEEP_IDLE(I3C_BUS_INFO_t *pBus);
extern _Bool I3C_IS_BUS_DURING_DAA(I3C_BUS_INFO_t *pBus);
extern _Bool I3C_IS_BUS_DETECT_SLVSTART(I3C_BUS_INFO_t *pBus);
extern _Bool I3C_IS_BUS_WAIT_STOP_OR_RETRY(I3C_BUS_INFO_t *pBus);

/* dev info */
extern I3C_DEVICE_INFO_SHORT_t *NewDevInfo(I3C_BUS_INFO_t *pBus, void *pDevice,
	I3C_DEVICE_ATTRIB_t attr, __u8 prefferedAddr, __u8 dynamicAddr, __u8 pid[],
	__u8 bcr, __u8 dcr);
extern I3C_DEVICE_INFO_SHORT_t *GetDevInfoByStaticAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr);
extern I3C_DEVICE_INFO_SHORT_t *GetDevInfoByDynamicAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr);
extern I3C_DEVICE_INFO_SHORT_t *GetDevInfoByTask(I3C_BUS_INFO_t *pBus, I3C_TRANSFER_TASK_t *pTask);

extern _Bool IS_Internal_DEVICE(void *pDevice);

extern void I3C_Reset(__u8 busNo);
extern void RemoveDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo);
extern void ResetDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo);

/* For bus enumeration */
extern _Bool IS_I3C_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus);
extern _Bool IS_RSTDAA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus);
extern _Bool IS_SETHID_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus);
extern _Bool IS_SETDASA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus);
extern _Bool IS_SETAASA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus);

/* CCC for bus enumeration */
extern void I3C_CCCb_ENEC(I3C_BUS_INFO_t *pBus, __u8 mask, I3C_TASK_POLICY_Enum policy);
extern void I3C_CCCb_DISEC(I3C_BUS_INFO_t *pBus, __u8 mask, I3C_TASK_POLICY_Enum policy);
extern void I3C_CCCb_RSTDAA(I3C_BUS_INFO_t *pBus);
extern void I3C_CCCb_ENTDAA(I3C_BUS_INFO_t *pBus);
extern void I3C_CCCb_SETAASA(I3C_BUS_INFO_t *pBus);
extern void I3C_CCCb_SETHID(I3C_BUS_INFO_t *pBus);
extern void I3C_CCCw_ENEC(I3C_BUS_INFO_t *pBus, __u8 addr, __u8 mask, I3C_TASK_POLICY_Enum policy);
extern void I3C_CCCw_DISEC(I3C_BUS_INFO_t *pBus, __u8 addr, __u8 mask,
	I3C_TASK_POLICY_Enum policy);
extern void I3C_CCCw_SETDASA(I3C_BUS_INFO_t *pBus);
extern void I3C_CCCw_SETNEWDA(I3C_BUS_INFO_t *pBus, __u8 dyn_addr_old, __u8 dyn_addr_new);

/* Response */
extern __u32 I3C_Notify(I3C_TASK_INFO_t *pTaskInfo);

/*
 * Used to limit task amount
 */
#define I3C_TASK_MAX	10

extern I3C_TASK_INFO_t *I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u8 HSize, __u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF);
extern I3C_TASK_INFO_t *I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, _Bool bHIF);
extern void I3C_Complete_Task(I3C_TASK_INFO_t *pTaskInfo);
extern _Bool I3C_Clean_Slave_Task(I3C_DEVICE_INFO_t *pDevice);

extern void *NewTaskInfo(void);
extern void InitTaskInfo(I3C_TASK_INFO_t *pTaskInfo);
extern void FreeTaskNode(I3C_TRANSFER_TASK_t *pTask);

extern I3C_ErrCode_Enum ValidateProtocol(I3C_TRANSFER_PROTOCOL_Enum Protocol);
extern I3C_ErrCode_Enum ValidateBaudrate(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u32 BaudRate);
extern I3C_ErrCode_Enum ValidateBuffer(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Address,
	__u8 HSize, __u16 WrCnt, __u16 RdCnt, __u8 *WrBuf, __u8 *RdBuf, _Bool bHIF);

extern I3C_ErrCode_Enum CreateTaskNode(I3C_TASK_INFO_t *pTaskInfo,
	I3C_TRANSFER_PROTOCOL_Enum Protocol, __u32 Baudrate, __u8 Addr, __u8 HSize,
	__u16 *pWrCnt, __u8 *WrBuf, __u16 *pRdCnt, __u8 *RdBuf, ptrI3C_RetFunc callback,
	_Bool bHIF);
extern I3C_ErrCode_Enum InsertTaskNode(I3C_DEVICE_INFO_t *pDevice,
	I3C_TRANSFER_TASK_t *pNewTask, __u8 bType);

/* Sample to create master task and might be used in the driver */
extern I3C_ErrCode_Enum I3C_Master_Insert_Task_ENTDAA(__u16 rxbuf_size, __u8 *rxbuf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCb(__u8 CCC, __u16 buf_size, __u8 *buf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCw(__u8 CCC, __u8 HSize,
	__u16 buf_size, __u8 *buf, __u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback,
	__u8 PortId, I3C_TASK_POLICY_Enum Policy, _Bool bHIF);
extern I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCr(__u8 CCC, __u8 HSize,
	__u16 txbuf_size, __u16 rxbuf_size, __u8 *txbuf, __u8 *rxbuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF);
extern I3C_ErrCode_Enum I3C_Master_Insert_Task_EVENT(__u16 rxbuf_size, __u8 *rxbuf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF);

/* Sample to create slave task and might be used in the driver */
extern I3C_ErrCode_Enum I3C_Slave_Insert_Task_HotJoin(I3C_PORT_Enum port);
extern I3C_ErrCode_Enum I3C_Slave_Insert_Task_MasterRequest(I3C_PORT_Enum port);
extern I3C_ErrCode_Enum I3C_Slave_Insert_Task_IBI(I3C_PORT_Enum port,
	__u16 txbuf_size, __u8 *txbuf);

/* to support pec */
/*extern __u8 I3C_crc8_fast(__u8 crc, __u8 const *mem, size_t len);*/

#ifdef __cplusplus
}
#endif

#endif /* __I3C_CORE_H__ */
