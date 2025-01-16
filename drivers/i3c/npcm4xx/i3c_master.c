/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <kernel.h>
#include "pub_i3c.h"
#include "i3c_core.h"
#include "i3c_master.h"
#include "i3c_slave.h"
#include "hal_i3c.h"
#include "i3c_drv.h"

LOG_MODULE_REGISTER(npcm4xx_i3c_master, CONFIG_I3C_LOG_LEVEL);

extern struct i3c_npcm4xx_obj *gObj[I3C_PORT_MAX];
extern struct k_work_q npcm4xx_i3c_work_q[I3C_PORT_MAX];
extern struct k_work work_entdaa[I3C_PORT_MAX];

static uint8_t default_assign_address = 0x9;

/**
 * @brief                           Callback for I3C master
 * @param [in]      TaskInfo        Pointer to the running task
 * @param [in]      ErrDetail       task result
 * @return                          final task result
 */

static uint32_t I3C_Master_Callback(uint32_t TaskInfo, uint32_t ErrDetail)
{
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	struct I3C_CallBackResult CallBackResult;

	if (TaskInfo == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTaskInfo = (I3C_TASK_INFO_t *) TaskInfo;

	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pDevice = I3C_Get_INODE(pTaskInfo->Port);
	pBus = pDevice->pOwner;

	/* Indirect Callback, callback defined in create master task */
	/* FIXME: use workqueue to avoid callback blocked */
	if ((pTaskInfo->pCallback != NULL)) {
		CallBackResult.result = pTaskInfo->result;
		CallBackResult.tx_len = *(pTask->pWrLen);
		CallBackResult.rx_len = *(pTask->pRdLen);
		pTaskInfo->pCallback(pTaskInfo->pCallbackData, &CallBackResult);
	}

	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;

	return pTaskInfo->result;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Start to run master's task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Start_Request(uint32_t Parm)
{
	I3C_TASK_INFO_t *pTaskInfo;

	if (Parm == 0) {
		return;
	}

	pTaskInfo = (I3C_TASK_INFO_t *)Parm;
	if (pTaskInfo->pTask == NULL) {
		return;
	}

	hal_I3C_Process_Task(pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           generate STOP then callback
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Stop_Request(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	uint8_t port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_ErrCode_Enum result;
	struct I3C_CallBackResult CallBackResult;
	ptrI3C_RetFunc pCallback = NULL;
	void *pCallbackData;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *) Parm;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return;
	}

	port = pTaskInfo->Port;
	pDevice = I3C_Get_INODE(port);
	result = pTaskInfo->result;
	pBus = pDevice->pOwner;

	if ((pTaskInfo->pCallback != NULL)) {
		pCallback = pTaskInfo->pCallback;
		pCallbackData = pTaskInfo->pCallbackData;
		CallBackResult.result = pTaskInfo->result;
		CallBackResult.tx_len = *(pTask->pWrLen);
		CallBackResult.rx_len = *(pTask->pRdLen);
	}

	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;

	/* Before we emit stop, disable all DMA */
	hal_I3C_Disable_Master_DMA(port);

	hal_I3C_Stop(port);

	/* FIXME: use workqueue to avoid callback blocked */
	if (pCallback!= NULL) {
		pCallback(pCallbackData, &CallBackResult);
	}
}

/*---------------------------------------------------------------------------------*/
/**
 * @brief                           Retry master task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*---------------------------------------------------------------------------------*/
void I3C_Master_Retry_Frame(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_TASK_INFO_t *pTaskInfo;

	if (Parm == 0)
		return;

	pTask = (I3C_TRANSFER_TASK_t *)Parm;

	if (pTask->frame_idx >= pTask->frame_count)
		return;

	if (pTask->pTaskInfo == NULL)
		return;

	pTaskInfo = pTask->pTaskInfo;

	pFrame = &pTask->pFrameList[pTask->frame_idx];
	if ((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) && (pFrame->retry_count >= 1)) {
		pFrame->retry_count--;

		if (pFrame->flag & I3C_TRANSFER_RETRY_WITHOUT_STOP) {
			pFrame->flag |= I3C_TRANSFER_REPEAT_START;
		} else {
			I3C_Master_Stop_Request((uint32_t) pTask);

			/* wait a moment for slave to prepare response data */
			/* k_usleep(WAIT_SLAVE_PREPARE_RESPONSE_TIME); */
		}

		if (pTask->protocol == I3C_TRANSFER_PROTOCOL_CCCr ||
				READ_TRANSFER_PROTOCOL(pTask->protocol)) {
			/* Before we retry read transaction, disable RX DMA */
			hal_I3C_Disable_Master_RX_DMA(pTaskInfo->Port);
		}

		I3C_Master_Start_Request((uint32_t) pTaskInfo);
	} else {
		I3C_Master_Stop_Request((uint32_t) pTask);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Finish master task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_End_Request(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *) Parm;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;

	I3C_Master_Callback((uint32_t)pTaskInfo, pTaskInfo->result);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Keep running next frame of the master task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Run_Next_Frame(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask = NULL;
	I3C_TASK_INFO_t *pTaskInfo = NULL;
	I3C_BUS_INFO_t *pBus = NULL;
	I3C_DEVICE_INFO_SHORT_t *pDev = NULL;
	I3C_DEVICE_ATTRIB_t attr;
	I3C_DEVICE_INFO_t *pDevice = NULL;
	uint8_t rx_buf[8] = {0};
	uint8_t bcr = 0;
	uint8_t dcr = 0;
	uint8_t assign_address = default_assign_address;
	bool found = false;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *)Parm;
	if (pTask->pTaskInfo == NULL) {
		return;
	}
	if (pTask->frame_idx >= pTask->frame_count) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return;
	}

	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	if (pBus) {
		pDevice = pBus->pCurrentMaster;
	} else {
		LOG_ERR("Bus NULL in Run_Next_Frame");
		return;
	}

	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		/* Previous slave dynamic address has been accepted, if (pTask->frame_idx != 0)
		 * we will process them in the ENTDAA's callback
		 *
		 * Now, we try to assign slave dynamic address in according to PID, BCR, DCR and
		 * save the into access_buf[8]
		 */
		memcpy(&rx_buf, pTask->pFrameList[pTask->frame_idx + 1].access_buf, sizeof(rx_buf));

		/* rx_buf[0] ~ rx_buf[5] => pid value */
		bcr = rx_buf[6];
		dcr = rx_buf[7];

		pDev = pBus->pDevList;
		while (pDev != NULL) {
			if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
				pDev = pDev->pNextDev;
				continue;
			}

			if ((pDev->attr.b.present) && (pDev->bcr == bcr) && (pDev->dcr == dcr) &&
				(memcmp(&pDev->pid, (uint8_t *)&rx_buf, 6) == 0)) {

				/* adopt user define in advance */
				pTask->pFrameList[pTask->frame_idx + 1].access_buf[8] = pDev->dynamicAddr;
				found = true;
				break;
			}

			pDev = pDev->pNextDev;
		}

		/* Cannot match any device, assign default dynamic address */
		if (found == false) {
			/* Found a empty dynamic address, suppose max address => 0x77 */
			for (assign_address = default_assign_address; assign_address < 0x78; assign_address++) {
				if (IsValidDynamicAddress(assign_address)) {
					if (GetDevInfoByDynamicAddr(pBus, assign_address)) {
						continue;
					} else {
						break;
					}
				}
			}

			pTask->pFrameList[pTask->frame_idx + 1].access_buf[8] = assign_address;
		}
	}

	pTask->frame_idx++;
	I3C_Master_Start_Request((uint32_t)pTaskInfo);

	/* Print debug message and create device node after emit start */
	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		if (found == true) {
			LOG_INF("Match device, assign address to 0x%x", pDev->dynamicAddr);
		} else {
			LOG_WRN("Cannot match device, assign address to 0x%x", assign_address);

			attr.b.present = 1;
			if (NewDevInfo(pBus, NULL, attr, assign_address, assign_address,
						(uint8_t *)&rx_buf[0], bcr, dcr) == NULL) {
				LOG_ERR("Create device info fail");
			}
		}
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                 Insert event task to handle slave request
 * @param [in]      Parm  i3c master detect the SLVSTART
 * @return                none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_New_Request(uint32_t Parm)
{
	uint8_t port;
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	struct i3c_npcm4xx_obj *obj;
	uint16_t rxLen;

	if (Parm >= I3C_PORT_MAX) {
		return;
	}

	rxLen = IBI_PAYLOAD_SIZE_MAX;

	port = (uint8_t)Parm;

	obj = gObj[port];

	/* must use NOT_HIF */
	I3C_Master_Insert_Task_EVENT(&rxLen, NULL, obj->config->i3c_scl_hz,
		TIMEOUT_TYPICAL, NULL, NULL, port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);

	pBus = Get_Bus_From_Port(port);
	if (pBus == NULL) {
		return;
	}
	if (pBus->pCurrentMaster == NULL) {
		return;
	}
	if (pBus->pCurrentMaster->pTaskListHead == NULL) {
		return;
	}

	pTask = pBus->pCurrentMaster->pTaskListHead;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	pBus->pCurrentTask = pTask;
	I3C_Master_Start_Request((uint32_t)pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                Insert DISEC task to disable slave request
 * @param [in]      Parm Pointer to task
 * @return               none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Insert_DISEC_After_IbiNack(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	uint8_t port;
	I3C_BUS_INFO_t *pBus;
	I3C_IBITYPE_Enum ibiType;
	uint8_t ibiAddress;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_FRAME_t *pFrame;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *) Parm;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return;
	}

	port = pTaskInfo->Port;
	pBus = Get_Bus_From_Port(port);

	ibiType = hal_I3C_get_ibiType(port);
	ibiAddress = hal_I3C_get_ibiAddr(port);

	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_EVENT) {
		pTaskInfo->result = I3C_ERR_OK;
		*pTask->pRdLen = 0;
		I3C_Master_Callback((uint32_t) pTaskInfo, pTaskInfo->result);
	}

	/* Reset DMA and FIFO before IBIAckNack */
	hal_I3C_Disable_Master_DMA(port);
	hal_I3C_Reset_Master_TX(port); /* reset Tx FIFO, for HDRCMD */
	hal_I3C_Nack_IBI(port);

	if (ibiAddress == 0x00) {
		/* Used to patch SPD5118's abnormal behavior
		 * Bus master try to ENTDAA but SPD5118 has already been in I3C mode
		 */
		I3C_CCCb_DISEC(pBus, DISEC_MASK_ENINT, I3C_TASK_POLICY_INSERT_FIRST);
	} else if (ibiType == I3C_IBITYPE_HotJoin) {
		/* It must be broadcast CCC, because slave doesn't have dynamic addrerss yet */
		I3C_CCCb_DISEC(pBus, DISEC_MASK_ENHJ, I3C_TASK_POLICY_INSERT_FIRST);
	} else if (ibiType == I3C_IBITYPE_IBI) {
		I3C_CCCw_DISEC(pBus, ibiAddress, DISEC_MASK_ENINT, I3C_TASK_POLICY_INSERT_FIRST);
	} else if (ibiType == I3C_IBITYPE_MstReq) {
		I3C_CCCw_DISEC(pBus, ibiAddress, DISEC_MASK_ENMR, I3C_TASK_POLICY_INSERT_FIRST);
	}

	/* run the DISEC immediatedly */
	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	if (pDevice->pTaskListHead == NULL) {
		return;
	}

	pTask = pDevice->pTaskListHead;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	if (pTask->frame_idx >= pTask->frame_count) {
		return;
	}

	pFrame = &pTask->pFrameList[pTask->frame_idx];
	pFrame->flag |= I3C_TRANSFER_REPEAT_START;
	pBus->pCurrentTask = pTask;
	I3C_Master_Start_Request((uint32_t)pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Ack the IBI
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_IBIACK(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pSlvDev;
	struct i3c_npcm4xx_obj *obj;
	uint8_t ibiAddress;
	uint16_t rxLen;


	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *)Parm;
	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	port = pTaskInfo->Port;

	pDevice = I3C_Get_INODE(port);
	if (pDevice->pOwner == NULL) {
		return;
	}

	obj = gObj[port];

	pBus = pDevice->pOwner;

	/* Master Transfer but IBIWON */
	if (pTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		hal_I3C_Disable_Master_RX_DMA(port);

		/* Insert Event task and assume event task has running to
		 * restore IBI Type and IBI address
		 */
		rxLen = IBI_PAYLOAD_SIZE_MAX;
		I3C_Master_Insert_Task_EVENT(&rxLen, NULL,
			obj->config->i3c_scl_hz, TIMEOUT_TYPICAL, NULL, NULL,
			port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);

		if (pDevice->pTaskListHead == NULL) {
			return;
		}

		pTask = pDevice->pTaskListHead;
		pBus->pCurrentTask = pTask;

		if (pTask->pTaskInfo == NULL) {
			return;
		}
		pTaskInfo = pTask->pTaskInfo;

		if (Setup_Master_Read_DMA(pDevice) != 0)
			LOG_ERR("Arbitration DMA setup fail");
	}

	ibiAddress = hal_I3C_get_ibiAddr(port);

	pSlvDev = GetDevInfoByDynamicAddr(pBus, ibiAddress);
	if (pSlvDev == NULL) {
		hal_I3C_Nack_IBI(port);
	} else {
		/* suppose we only support ibi with MDB */
		hal_I3C_Ack_IBI_With_MDB(port);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                ack the Master Request, then insert GETACCMST task
 * @param [in]      Parm Pointer to task
 * @return               none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Insert_GETACCMST_After_IbiAckMR(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	uint8_t port;
	uint8_t ibiAddress;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	I3C_TRANSFER_FRAME_t *pFrame;
	struct i3c_npcm4xx_obj *obj;
	uint8_t wrBuf[2];
	uint8_t rdBuf[2];	/* addr + pec */
	uint16_t rxLen;
	I3C_ErrCode_Enum res;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *)Parm;

	if (pTask->pTaskInfo == NULL) {
		return;
	}
	pTaskInfo = pTask->pTaskInfo;

	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return;
	}
	port = pTaskInfo->Port;

	obj = gObj[port];

	hal_I3C_Ack_IBI_Without_MDB(port);
	hal_I3C_Disable_Master_RX_DMA(port);

	pTaskInfo->result = I3C_ERR_MR;
	*pTask->pRdLen = 0;
	I3C_Master_Callback((uint32_t) pTaskInfo, pTaskInfo->result);

	ibiAddress = hal_I3C_get_ibiAddr(port);

	/* Ack the Master-Request --> IBIWON and COMPLETE will set again */
	/* Insert GETACCMST and let GETACCMST start with RESTART */
	pBus = Get_Bus_From_Port(port);
	pDev = NULL;
	pDev = GetDevInfoByDynamicAddr(pBus, ibiAddress);
	if (pDev == NULL) {
		return;
	}

	rxLen = 1;

	wrBuf[0] = ibiAddress;

	/* don't change NOT_HIF to IS_HIF */
	/* We should malloc rdBuf and rxLen for IS_HIF if needed */
	res = I3C_Master_Insert_Task_CCCr(CCC_DIRECT_GETACCMST, 1, 1, &rxLen, wrBuf,
		rdBuf, obj->config->i3c_scl_hz, TIMEOUT_TYPICAL,
		NULL, NULL, port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);

	if (res == I3C_ERR_OK) {
		pTask = pBus->pCurrentMaster->pTaskListHead;
		pTaskInfo = pTask->pTaskInfo;
		pFrame = &pTask->pFrameList[pTask->frame_idx];
		pFrame->flag |= I3C_TRANSFER_REPEAT_START;
		I3C_Master_Start_Request((uint32_t) pTaskInfo);
	}

	/* rdBuf[2] and rxLen might release here */
	/* return data should be saved in variables allocated in I3C_Master_Create_Task */
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           ack the Hot Join, then insert ENTDAA task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Insert_ENTDAA_After_IbiAckHJ(uint32_t Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	struct i3c_npcm4xx_obj *obj;
	uint8_t port;
	uint16_t rxLen;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *)Parm;

	if (pTask->pTaskInfo == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;

	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return;
	}

	port = pTaskInfo->Port;

	obj = gObj[port];

	pDevice = I3C_Get_INODE(port);

	pBus = pDevice->pOwner;

	pTaskInfo->result = I3C_ERR_OK;
	*pTask->pRdLen = 0;

	hal_I3C_Disable_Master_RX_DMA(port);
	hal_I3C_Ack_IBI_Without_MDB(port);

	/* Must to use STOP to notify slave finish hot-join task
	 * Can't use RESTART + ENTDAA, that will cause MERRWARN.INVREQ
	 */
	I3C_Master_Stop_Request((uint32_t)pTask);

	/* Master Transfer but IBIWON */
	if (pTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		/* Insert Event task and assume event task has running to
		 * restore IBI Type and IBI address
		 */
		rxLen = IBI_PAYLOAD_SIZE_MAX;
		I3C_Master_Insert_Task_EVENT(&rxLen, NULL,
				obj->config->i3c_scl_hz, TIMEOUT_TYPICAL, NULL, NULL,
				port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);

		if (pDevice->pTaskListHead == NULL) {
			return;
		}

		pTask = pDevice->pTaskListHead;
		pBus->pCurrentTask = pTask;

		if (pTask->pTaskInfo == NULL) {
			return;
		}
		pTaskInfo = pTask->pTaskInfo;
	}

	/* entdaa use system workqueue */
	k_work_submit(&work_entdaa[port]);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           configure PDMA for master write transfer
 * @param [in]      pDevice         Pointer to device object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum Setup_Master_Write_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;


	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pDevice->port >= I3C_PORT_MAX) {
		return I3C_ERR_TASK_INVALID;
	}
	if (pDevice->pTaskListHead == NULL) {
		return I3C_ERR_TASK_INVALID;
	}

	pTask = pDevice->pTaskListHead;
	if (pTask->frame_idx >= pTask->frame_count) {
		return I3C_ERR_TASK_INVALID;
	}

	pFrame = &pTask->pFrameList[pTask->frame_idx];
	if ((pFrame->access_len - pFrame->access_idx) == 0) {
		return I3C_ERR_OK;
	}

	hal_I3C_DMA_Write(pDevice->port, pDevice->mode, &pFrame->access_buf[pFrame->access_idx],
		pFrame->access_len - pFrame->access_idx);
	pFrame->access_idx = pFrame->access_len;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           configure PDMA for master read transfer
 * @param [in]      pDevice         Pointer to device object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum Setup_Master_Read_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;

	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pDevice->port >= I3C_PORT_MAX) {
		return I3C_ERR_TASK_INVALID;
	}
	if (pDevice->pTaskListHead == NULL) {
		return I3C_ERR_TASK_INVALID;
	}

	pTask = pDevice->pTaskListHead;

	if (pTask->frame_idx >= pTask->frame_count) {
		return I3C_ERR_TASK_INVALID;
	}

	pFrame = &pTask->pFrameList[pTask->frame_idx];

	if ((pFrame->access_len - pFrame->access_idx) == 0) {
		return I3C_ERR_OK;
	}

	hal_I3C_DMA_Read(pDevice->port, pDevice->mode, &pFrame->access_buf[pFrame->access_idx],
		pFrame->access_len - pFrame->access_idx);
	return I3C_ERR_OK;
}
