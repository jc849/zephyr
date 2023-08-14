/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/i3c/NPCM4XX/pub_I3C.h>
#include <drivers/i3c/NPCM4XX/i3c_core.h>
#include <drivers/i3c/NPCM4XX/i3c_master.h>
#include <drivers/i3c/NPCM4XX/i3c_slave.h>
#include <drivers/i3c/NPCM4XX/hal_I3C.h>
#include <drivers/i3c/NPCM4XX/i3c_drv.h>

extern I3C_BUS_INFO_t gBus[];
extern I3C_DEVICE_INFO_t gI3c_dev_node_internal[];
extern I3C_REG_ITEM_t *pSlaveReg[];

void api_I3C_Reset(__u8 busNo)
{
	if (busNo >= I3C_BUS_COUNT_MAX) {
		return;
	}

	I3C_Reset(busNo);
}

I3C_ErrCode_Enum api_I3C_START(void)
{
	I3C_Setup_Internal_Device();
	I3C_Setup_External_Device();
	I3C_Setup_Bus();

	return I3C_ERR_OK;
}

__u8 I3C_Task_Engine(void)
{
	_Bool bTaskExist = FALSE;

	I3C_DEVICE_INFO_SHORT_t *pDev;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		pDevice = &gI3c_dev_node_internal[port];
		if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
			continue;
		}

		/* continue if internal devices connect to bus */
		if (pDevice->pOwner == NULL) {
			continue;
		}

		pBus = pDevice->pOwner;

		/* wait until bus is ready */
		if (pBus->busState == I3C_BUS_STATE_DEFAULT) {
			if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) ||
			    (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
				/* dynamic address is assigned ? hot join ? */
				pDevice->dynamicAddr = hal_i3c_get_dynamic_address(port);
				pDevice->bRunI3C = TRUE;
			}

			continue;
		}

		if (pBus->busState == I3C_BUS_STATE_WAIT_RESET_DONE) {
			continue;
		}

		if (pBus->busState == I3C_BUS_STATE_WAIT_CLEAR_DONE) {
			continue;
		}

		/* bus is busy, keep running current task until task done or timeout */
		if (pBus->pCurrentTask != NULL) {
			continue;
		}

		/* bus is free */
		if (pBus->busState == I3C_BUS_STATE_INIT) {
			if (pBus->pCurrentMaster == pDevice) {
				/* bus enumeration */
				if (IS_RSTDAA_DEVICE_PRESENT(pBus)) {
					bTaskExist = TRUE;
					I3C_CCCb_RSTDAA(pBus);
				} else if (IS_SETHID_DEVICE_PRESENT(pBus)) {
					bTaskExist = TRUE;
					I3C_CCCb_SETHID(pBus);
				} else if (IS_SETDASA_DEVICE_PRESENT(pBus)) {
					bTaskExist = TRUE;
					I3C_CCCw_SETDASA(pBus);
				} else if (IS_SETAASA_DEVICE_PRESENT(pBus)) {
					bTaskExist = TRUE;
					I3C_CCCb_SETAASA(pBus);
				} else   {
					bTaskExist = TRUE;
					I3C_CCCb_ENTDAA(pBus);
				}
			} else   {
				if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) ||
				    (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
					/* dynamic address is assigned ? */
					pDevice->dynamicAddr = hal_i3c_get_dynamic_address(port);
					pDevice->bRunI3C = TRUE;

					pDev = pDevice->pDevInfo;
					if (pDev == NULL) {
						continue;
					}

					if (pDev->attr.b.reqHotJoin == 1) {
						pDev->attr.b.reqHotJoin = 0;
					}
				}
			}
		}

		/* ==============================================================
		 * POST INIT TASK
		 * ==============================================================
		 */
		if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
			if (pDevice->pTaskListHead == NULL) {
				pDev = pBus->pDevList;
				while (pDev != NULL) {
					if ((pDev->attr.b.reqPostInit) &&
						(pDev->attr.b.donePostInit == 0)) {
						/* run 1 post init task once in the task engine */
						/* if (IS_SPD5118_DEVICE(pDev->pDeviceInfo)) {
						 *	bTaskExist = TRUE;
						 *	SPD5118_Post_Init(pDev->staticAddr);
						 *	break;
						 *}
						 */
						/* if (IS_LSM6DSO_DEVICE(pDev->pDeviceInfo)) {
						 *	bTaskExist = TRUE;
						 *	LSM6DSO_Post_Init(pDev);
						 *	break;
						 *}
						 */
					}

					pDev = pDev->pNextDev;
				}
			}
		} else if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) ||
					(pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
			/* hot-join */
			pDev = pDevice->pDevInfo;
			if (pDev == NULL) {
				continue;
			}

			if ((pDev->attr.b.reqHotJoin == 1) && (pDev->attr.b.doneHotJoin == 0)) {
				/* slave should try to do hot-join */
			}
		}

		if (pDevice->pTaskListHead == NULL) {
			continue;
		}

		bTaskExist = TRUE;
		pTask = pDevice->pTaskListHead;
		pBus->pCurrentTask = pTask;

		pTaskInfo = pTask->pTaskInfo;
		if (pTaskInfo->MasterRequest) {
			I3C_Master_Start_Request((__u32)pTaskInfo);
		} else {
			I3C_Slave_Start_Request((__u32)pTaskInfo);
		}
	}

	return (bTaskExist == TRUE) ? 1 : 0;
}

void api_I3C_Master_Start_Request(__u32 Parm)
{
	I3C_Master_Start_Request(Parm);
}

void api_I3C_Slave_Start_Request(__u32 Parm)
{
	I3C_Slave_Start_Request(Parm);
}

void api_I3C_Master_Stop(__u32 Parm)
{
	I3C_Master_Stop_Request(Parm);
}

void api_I3C_Master_Run_Next_Frame(__u32 Parm)
{
	I3C_Master_Run_Next_Frame(Parm);
}

void api_I3C_Master_New_Request(__u32 Parm)
{
	I3C_Master_New_Request(Parm);
}

void api_I3C_Master_Insert_DISEC_After_IbiNack(__u32 Parm)
{
	I3C_Master_Insert_DISEC_After_IbiNack(Parm);
}

void api_I3C_Master_IBIACK(__u32 Parm)
{
	I3C_Master_IBIACK(Parm);
}

void api_I3C_Master_Insert_GETACCMST_After_IbiAckMR(__u32 Parm)
{
	I3C_Master_Insert_GETACCMST_After_IbiAckMR(Parm);
}

void api_I3C_Master_Insert_ENTDAA_After_IbiAckHJ(__u32 Parm)
{
	I3C_Master_Insert_ENTDAA_After_IbiAckHJ(Parm);
}

I3C_ErrCode_Enum api_I3C_Master_Callback(__u32 TaskInfo, I3C_ErrCode_Enum ErrDetail)
{
	I3C_Master_Callback(TaskInfo, ErrDetail);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum api_I3C_Slave_Callback(__u32 TaskInfo, I3C_ErrCode_Enum ErrDetail)
{
	I3C_Slave_Callback(TaskInfo, ErrDetail);
	return I3C_ERR_OK;
}

void api_I3C_Master_End_Request(__u32 Parm)
{
	I3C_Master_End_Request(Parm);
}

void api_I3C_Slave_End_Request(__u32 Parm)
{
	I3C_Slave_End_Request(Parm);
}

I3C_ErrCode_Enum api_I3C_connect_bus(I3C_PORT_Enum port, __u8 busNo)
{
	I3C_DEVICE_ATTRIB_t attr;

	if (port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (busNo >= I3C_BUS_COUNT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;

	pDevice = &gI3c_dev_node_internal[port];
	pBus = &gBus[busNo];

	/* Build DevInfo for Bus */
	if (pDevice->mode != I3C_DEVICE_MODE_DISABLE) {
		attr.U16 = 0;
		attr.b.present = 1;

		if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
			if (pBus->pCurrentMaster == NULL) {
				pBus->pCurrentMaster = pDevice;
			}

			attr.b.suppMST = 1;
			attr.b.defaultMST = 1;

			pDevice->pDevInfo = NewDevInfo(pBus, pDevice, attr, pDevice->staticAddr,
				pDevice->dynamicAddr, pDevice->pid, pDevice->bcr, pDevice->dcr);
		} else   {
			attr.b.suppSLV = 1;

			attr.b.suppENTDAA = 1;
			if (pDevice->staticAddr != I2C_STATIC_ADDR_DEFAULT_7BIT) {
				attr.b.reqSETDASA = 1;
			}

			pDevice->pDevInfo = NewDevInfo(pBus, pDevice, attr, pDevice->staticAddr,
				I3C_DYNAMIC_ADDR_DEFAULT_7BIT, pDevice->pid, pDevice->bcr,
				pDevice->dcr);
		}
	}

	pDevice->pOwner = pBus;

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum api_I3C_set_pReg(I3C_PORT_Enum port, I3C_REG_ITEM_t *pReg)
{
	I3C_DEVICE_INFO_t *pDevice;

	if (port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[port];
	pDevice->pReg = pReg;
	return I3C_ERR_OK;
}

/*
 * I3C_ErrCode_Enum api_I3C_declare_lsm6dso(__u8 busNo, I3C_PORT_Enum port,
 *	__u8 address, __u16 InitMode)
 * {
 *	return LSM6DSO_START(busNo, port, address, InitMode);
 * }
 *
 * I3C_ErrCode_Enum api_I3C_declare_spd5118(__u8 busNo, I3C_PORT_Enum port,
 *	__u16 InitMode)
 * {
 *	return SPD5118_START(busNo, port, InitMode);
 * }
 */

I3C_ErrCode_Enum api_I3C_bus_reset_done(__u8 busNo)
{
	return I3C_ERR_OK;
}

extern __u8 bus_clear_counter;
I3C_ErrCode_Enum api_I3C_bus_clear_done(__u8 busNo)
{
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum api_I3C_Slave_Prepare_Response(I3C_DEVICE_INFO_t *pDevice, __u16 wrLen,
	__u8 *pWrBuf)
{
	return I3C_Slave_Prepare_Response(pDevice, wrLen, pWrBuf);
}

I3C_ErrCode_Enum api_I3C_Slave_Update_Pending(I3C_DEVICE_INFO_t *pDevice, __u8 mask)
{
	return I3C_Slave_Update_Pending(pDevice, mask);
}

I3C_ErrCode_Enum api_I3C_Slave_Finish_Response(I3C_DEVICE_INFO_t *pDevice)
{
	return I3C_Slave_Finish_Response(pDevice);
}

I3C_ErrCode_Enum api_I3C_Slave_Check_Response_Complete(I3C_DEVICE_INFO_t *pDevice)
{
	return I3C_Slave_Check_Response_Complete(pDevice);
}

I3C_ErrCode_Enum api_I3C_Slave_Check_IBIDIS(I3C_DEVICE_INFO_t *pDevice, _Bool *bRet)
{
	return I3C_Slave_Check_IBIDIS(pDevice, bRet);
}

I3C_DEVICE_INFO_t *api_I3C_Get_Current_Master_From_Port(I3C_PORT_Enum port)
{
	return Get_Current_Master_From_Port(port);
}

I3C_BUS_INFO_t *api_I3C_Get_Bus_From_Port(I3C_PORT_Enum port)
{
	return Get_Bus_From_Port(port);
}

I3C_ErrCode_Enum api_I3C_Setup_Master_Write_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	return Setup_Master_Write_DMA(pDevice);
}

I3C_ErrCode_Enum api_I3C_Setup_Master_Read_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	return Setup_Master_Read_DMA(pDevice);
}

I3C_ErrCode_Enum api_I3C_Setup_Slave_Write_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	return Setup_Slave_Write_DMA(pDevice);
}

I3C_ErrCode_Enum api_I3C_Setup_Slave_Read_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	return Setup_Slave_Read_DMA(pDevice);
}

I3C_ErrCode_Enum api_I3C_Setup_Slave_IBI_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	return Setup_Slave_IBI_DMA(pDevice);
}

I3C_DEVICE_INFO_SHORT_t *api_I3C_GetDevInfoByStaticAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr)
{
	return GetDevInfoByStaticAddr(pBus, slaveAddr);
}

I3C_DEVICE_INFO_SHORT_t *api_I3C_GetDevInfoByDynamicAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr)
{
	return GetDevInfoByDynamicAddr(pBus, slaveAddr);
}

I3C_DEVICE_INFO_t *api_I3C_Get_INODE(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX) {
		return NULL;
	}

	return I3C_Get_INODE(port);
}

I3C_PORT_Enum api_I3C_Get_IPORT(I3C_DEVICE_INFO_t *pDevice)
{
	return I3C_Get_IPORT(pDevice);
}

I3C_TASK_INFO_t *api_I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u8 HSize, __u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF)
{
	return I3C_Master_Create_Task(Protocol, Addr, HSize, pWrCnt, pRdCnt, WrBuf, RdBuf, Baudrate,
		Timeout, callback, PortId, Policy, bHIF);
}

I3C_TASK_INFO_t *api_I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Addr,
	__u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, _Bool bHIF)
{
	return I3C_Slave_Create_Task(Protocol, Addr, pWrCnt, pRdCnt, WrBuf, RdBuf, Timeout,
		callback, PortId, bHIF);
}

I3C_ErrCode_Enum api_I3C_Master_Insert_Task_ENTDAA(__u16 rxbuf_size, __u8 *rxbuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF)
{
	return I3C_Master_Insert_Task_ENTDAA(rxbuf_size, rxbuf, Baudrate, Timeout, callback, PortId,
		Policy, bHIF);
}

I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCb(__u8 CCC, __u16 buf_size, __u8 *buf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	return I3C_Master_Insert_Task_CCCb(CCC, buf_size, buf, Baudrate, Timeout, callback, PortId,
		Policy, bHIF);
}

I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCw(__u8 CCC, __u8 HSize, __u16 buf_size, __u8 *buf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	return I3C_Master_Insert_Task_CCCw(CCC, HSize, buf_size, buf, Baudrate, Timeout, callback,
		PortId, Policy, bHIF);
}

I3C_ErrCode_Enum api_I3C_Master_Insert_Task_CCCr(__u8 CCC, __u8 HSize, __u16 txbuf_size,
	__u16 rxbuf_size, __u8 *txbuf, __u8 *rxbuf, __u32 Baudrate, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	return I3C_Master_Insert_Task_CCCr(CCC, HSize, txbuf_size, rxbuf_size, txbuf, rxbuf,
		Baudrate, Timeout, callback, PortId, Policy, bHIF);
}

I3C_ErrCode_Enum api_ValidateBuffer(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Address, __u8 HSize,
	__u16 WrCnt, __u16 RdCnt, __u8 *WrBuf, __u8 *RdBuf, _Bool bHIF)
{
	return ValidateBuffer(Protocol, Address, HSize, WrCnt, RdCnt, WrBuf, RdBuf, bHIF);
}
