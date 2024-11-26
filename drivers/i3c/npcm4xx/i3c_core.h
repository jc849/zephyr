/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __I3C_CORE_H__
#define __I3C_CORE_H__

#include "pub_I3C.h"

/*
 * variable
 */
extern I3C_DEVICE_INFO_t gI3c_dev_node_internal[];

#define MAX_READ_LEN I3C_PAYLOAD_SIZE_MAX
extern uint8_t slvRxId[I3C_PORT_MAX];
extern uint8_t slvRxBuf[I3C_PORT_MAX * 2][MAX_READ_LEN];
extern uint16_t slvRxLen[I3C_PORT_MAX];
extern uint16_t slvRxOffset[I3C_PORT_MAX * 2];


#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Used for project customizztion
 */
I3C_DEVICE_INFO_t *I3C_Get_INODE(I3C_PORT_Enum port);
I3C_DEVICE_INFO_t *Get_Current_Master_From_Port(I3C_PORT_Enum port);
I3C_PORT_Enum I3C_Get_IPORT(I3C_DEVICE_INFO_t *pDevice);
bool IsValidDynamicAddress(uint8_t addr);
bool IsValidStaticAddress(uint8_t addr);
I3C_ErrCode_Enum I3C_Port_Default_Setting(I3C_PORT_Enum port);
void I3C_Setup_External_Device(void);


/*
 * Used for bus operations
 */
I3C_BUS_INFO_t *Get_Bus_From_BusNo(uint8_t busno);
I3C_BUS_INFO_t *Get_Bus_From_Port(I3C_PORT_Enum port);
I3C_ErrCode_Enum I3C_connect_bus(I3C_PORT_Enum port, uint8_t busNo);

void BUS_RESET_PORT1(void);
void BUS_RESET_PORT2(void);
void BUS_CLEAR_PORT1(void);
void BUS_CLEAR_PORT2(void);

/* bus status check */
bool I3C_IS_BUS_DURING_DAA(I3C_BUS_INFO_t *pBus);
bool I3C_IS_BUS_DETECT_SLVSTART(I3C_BUS_INFO_t *pBus);
bool I3C_IS_BUS_WAIT_STOP_OR_RETRY(I3C_BUS_INFO_t *pBus);

/* dev info */
I3C_DEVICE_INFO_SHORT_t *NewDevInfo(I3C_BUS_INFO_t *pBus, void *pDevice,
	I3C_DEVICE_ATTRIB_t attr, uint8_t prefferedAddr, uint8_t dynamicAddr, uint8_t pid[],
	uint8_t bcr, uint8_t dcr);
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByStaticAddr(I3C_BUS_INFO_t *pBus, uint8_t slaveAddr);
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByDynamicAddr(I3C_BUS_INFO_t *pBus, uint8_t slaveAddr);
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByTask(I3C_BUS_INFO_t *pBus, I3C_TRANSFER_TASK_t *pTask);

bool IS_Internal_DEVICE(void *pDevice);


void I3C_Reset(uint8_t busNo);
void RemoveDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo);
void ResetDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo);

void I3C_CCCb_DISEC(I3C_BUS_INFO_t *pBus, uint8_t mask, I3C_TASK_POLICY_Enum policy);
void I3C_CCCw_DISEC(I3C_BUS_INFO_t *pBus, uint8_t addr, uint8_t mask, I3C_TASK_POLICY_Enum policy);

/*
 * Used to limit task amount
 */
#define I3C_TASK_MAX	10

I3C_TASK_INFO_t *I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, uint8_t Addr,
	uint8_t HSize, uint16_t *pWrCnt, uint16_t *pRdCnt, uint8_t *WrBuf, uint8_t *RdBuf, uint32_t Baudrate,
	uint32_t Timeout, ptrI3C_RetFunc callback, uint8_t PortId, I3C_TASK_POLICY_Enum Policy,
	bool bHIF);
I3C_TASK_INFO_t *I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, uint8_t Addr,
	uint16_t *pWrCnt, uint16_t *pRdCnt, uint8_t *WrBuf, uint8_t *RdBuf, uint32_t Timeout,
	ptrI3C_RetFunc callback, uint8_t PortId, bool bHIF);
void I3C_Complete_Task(I3C_TASK_INFO_t *pTaskInfo);
bool I3C_Clean_Slave_Task(I3C_DEVICE_INFO_t *pDevice);

void *NewTaskInfo(void);
void InitTaskInfo(I3C_TASK_INFO_t *pTaskInfo);
void FreeTaskNode(I3C_TRANSFER_TASK_t *pTask);

I3C_ErrCode_Enum ValidateProtocol(I3C_TRANSFER_PROTOCOL_Enum Protocol);
I3C_ErrCode_Enum ValidateBaudrate(I3C_TRANSFER_PROTOCOL_Enum Protocol, uint32_t BaudRate);
I3C_ErrCode_Enum ValidateBuffer(I3C_TRANSFER_PROTOCOL_Enum Protocol, uint8_t Address,
	uint8_t HSize, uint16_t WrCnt, uint16_t RdCnt, uint8_t *WrBuf, uint8_t *RdBuf, bool bHIF);

I3C_ErrCode_Enum CreateTaskNode(I3C_TASK_INFO_t *pTaskInfo,
	I3C_TRANSFER_PROTOCOL_Enum Protocol, uint32_t Baudrate, uint8_t Addr, uint8_t HSize,
	uint16_t *pWrCnt, uint8_t *WrBuf, uint16_t *pRdCnt, uint8_t *RdBuf, ptrI3C_RetFunc callback,
	bool bHIF);
I3C_ErrCode_Enum InsertTaskNode(I3C_DEVICE_INFO_t *pDevice,
	I3C_TRANSFER_TASK_t *pNewTask, uint8_t bType);

/* Sample to create master task and might be used in the driver */
I3C_ErrCode_Enum I3C_Master_Insert_Task_ENTDAA(uint16_t *rxbuf_size, uint8_t *rxbuf,
	uint32_t Baudrate, uint32_t Timeout, ptrI3C_RetFunc callback, uint8_t PortId,
	I3C_TASK_POLICY_Enum Policy, bool bHIF);
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCb(uint8_t CCC, uint16_t buf_size, uint8_t *buf,
	uint32_t Baudrate, uint32_t Timeout, ptrI3C_RetFunc callback, uint8_t PortId,
	I3C_TASK_POLICY_Enum Policy, bool bHIF);
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCw(uint8_t CCC, uint8_t HSize,
	uint16_t buf_size, uint8_t *buf, uint32_t Baudrate, uint32_t Timeout, ptrI3C_RetFunc callback,
	uint8_t PortId, I3C_TASK_POLICY_Enum Policy, bool bHIF);
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCr(uint8_t CCC, uint8_t HSize,
	uint16_t txbuf_size, uint16_t *rxbuf_size, uint8_t *txbuf, uint8_t *rxbuf, uint32_t Baudrate,
	uint32_t Timeout, ptrI3C_RetFunc callback, uint8_t PortId, I3C_TASK_POLICY_Enum Policy,
	bool bHIF);
I3C_ErrCode_Enum I3C_Master_Insert_Task_EVENT(uint16_t *rxbuf_size, uint8_t *rxbuf,
	uint32_t Baudrate, uint32_t Timeout, ptrI3C_RetFunc callback, uint8_t PortId,
	I3C_TASK_POLICY_Enum Policy, bool bHIF);

/* Sample to create slave task and might be used in the driver */
I3C_ErrCode_Enum I3C_Slave_Insert_Task_HotJoin(I3C_PORT_Enum port);
I3C_ErrCode_Enum I3C_Slave_Insert_Task_MasterRequest(I3C_PORT_Enum port);
I3C_ErrCode_Enum I3C_Slave_Insert_Task_IBI(I3C_PORT_Enum port,
	uint16_t txbuf_size, uint8_t *txbuf);

/* to support pec */
/*extern uint8_t I3C_crc8_fast(uint8_t crc, uint8_t const *mem, size_t len);*/

#ifdef __cplusplus
}
#endif

#endif /* __I3C_CORE_H__ */
