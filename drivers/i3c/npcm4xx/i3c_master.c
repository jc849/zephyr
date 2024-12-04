/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pub_I3C.h"
#include "i3c_core.h"
#include "i3c_master.h"
#include "i3c_slave.h"
#include "hal_I3C.h"
#include "i3c_drv.h"

LOG_MODULE_REGISTER(npcm4xx_i3c_master, CONFIG_I3C_LOG_LEVEL);

extern struct i3c_npcm4xx_obj *gObj[I3C_PORT_MAX];
extern struct k_work_q npcm4xx_i3c_work_q[I3C_PORT_MAX];
extern struct k_work work_retry[I3C_PORT_MAX];
extern struct k_work work_rcv_ibi[I3C_PORT_MAX];

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
	if ((pTaskInfo->pCallback != NULL)) {
		pTaskInfo->pCallback((uint32_t)pTaskInfo, pTaskInfo->pCallbackData);
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
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_PROTOCOL_Enum protocol;

	if (Parm == 0) {
		return;
	}

	pTaskInfo = (I3C_TASK_INFO_t *)Parm;
	if (pTaskInfo->pTask == NULL) {
		return;
	}

	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	pTask = pTaskInfo->pTask;
	protocol = pTask->protocol;

	if (I3C_IS_BUS_DETECT_SLVSTART(pBus)) {
		/* try to run master's task but SLVSTART is happened,
		 * and we should process it with higher priority
		 * abort current task and Event task will be inserted in ISR
		 */
		if (protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
			pBus->pCurrentTask = NULL;
			return;
		}

		hal_I3C_Process_Task(pTaskInfo);
	} else if (I3C_IS_BUS_DURING_DAA(pBus)) {
		if (protocol != I3C_TRANSFER_PROTOCOL_ENTDAA) {
			return;
		}
		hal_I3C_Process_Task(pTaskInfo);
	} else if (I3C_IS_BUS_WAIT_STOP_OR_RETRY(pBus)) {
		hal_I3C_Process_Task(pTaskInfo);
	} else {
		/* run retry after isr, let slave's isr can complete its task in time */

		if (I3C_IS_BUS_DETECT_SLVSTART(pBus) && (protocol != I3C_TRANSFER_PROTOCOL_EVENT)) {
			pBus->pCurrentTask = NULL;
			return;
		}

		hal_I3C_Process_Task(pTaskInfo);
	}
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
	I3C_ErrCode_Enum result;

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

	if (result == I3C_ERR_IBI) {
		pTask->address = hal_I3C_get_ibiAddr(port);
	}

	hal_I3C_Stop(port);

	I3C_Master_Callback((uint32_t) pTaskInfo, pTaskInfo->result);

	if (result == I3C_ERR_IBI) {
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[port], &work_rcv_ibi[port]);
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
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	uint8_t pid[6];
	uint8_t bcr;
	uint8_t dcr;
	I3C_DEVICE_INFO_t *pDevice;


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

	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		pBus = Get_Bus_From_Port(pTaskInfo->Port);

		/* previous slave dynamic address has been accepted, if (pTask->frame_idx != 0)
		 * we will process them in the ENTDAA's callback
		 *
		 * Now, we try to assign slave dynamic address in according to PID, BCR, DCR and
		 * save the into access_buf[8]
		 */
		memcpy(pid, pTask->pFrameList[pTask->frame_idx + 1].access_buf, 6);
		bcr = pTask->pFrameList[pTask->frame_idx + 1].access_buf[6];
		dcr = pTask->pFrameList[pTask->frame_idx + 1].access_buf[7];

		pDev = pBus->pDevList;
		while (pDev != NULL) {
			if ((pDev->attr.b.present) && (!IsValidDynamicAddress(pDev->dynamicAddr))
					&& (pDev->bcr == bcr) && (pDev->dcr == dcr)
					&& (memcmp(pDev->pid, pid, 6) == 0)) {
				if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
					pDevice = pDev->pDeviceInfo;

					if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY)
						|| (pDevice->mode
							== I3C_DEVICE_MODE_SECONDARY_MASTER)) {
						pTask->pFrameList[pTask->frame_idx + 1].access_buf[8] = pDev->staticAddr;
						break;
					}

					pDev = pDev->pNextDev;
					continue;
				}

				/* adopt user define in advance */
				pTask->pFrameList[pTask->frame_idx + 1].access_buf[8] = pDev->staticAddr;
				break;
			}

			pDev = pDev->pNextDev;
		}
	}

	pTask->frame_idx++;
	I3C_Master_Start_Request((uint32_t)pTaskInfo);
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

	uint16_t rxLen;

	if (Parm >= I3C_PORT_MAX) {
		return;
	}

	rxLen = IBI_PAYLOAD_SIZE_MAX;

	port = (uint8_t)Parm;
	/* must use NOT_HIF */
	I3C_Master_Insert_Task_EVENT(&rxLen, NULL, I3C_TRANSFER_SPEED_SDR_IBI,
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
	uint8_t ibiAddress;
	I3C_DEVICE_INFO_SHORT_t *pSlvDev;
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

	pBus = pDevice->pOwner;

	/* Master Transfer but IBIWON */
	if (pTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		hal_I3C_Disable_Master_RX_DMA(port);

		/* Insert Event task and assume event task has running to
		 * restore IBI Type and IBI address
		 */
		rxLen = IBI_PAYLOAD_SIZE_MAX;
		I3C_Master_Insert_Task_EVENT(&rxLen, NULL,
			I3C_TRANSFER_SPEED_SDR_IBI, TIMEOUT_TYPICAL, NULL, NULL,
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

	ibiAddress = hal_I3C_get_ibiAddr(port);

	pSlvDev = GetDevInfoByDynamicAddr(pBus, ibiAddress);
	if (pSlvDev == NULL) {
		/* slave device info should be init after
		 * ENTDAA/SETAASA/SETDASA/SETNEWDA
		 */
	} else {
		if ((pSlvDev->bcr & 0x06) == 0x06) {
			hal_I3C_Ack_IBI_With_MDB(port);
		} else if ((pSlvDev->bcr & 0x06) == 0x02) {
			hal_I3C_Ack_IBI_Without_MDB(port);
			hal_I3C_Disable_Master_RX_DMA(port);
			pTaskInfo->result = I3C_ERR_OK;
			*pTask->pRdLen = 0;
		}
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
		rdBuf, I3C_TRANSFER_SPEED_SDR_IBI, TIMEOUT_TYPICAL,
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
				I3C_TRANSFER_SPEED_SDR_IBI, TIMEOUT_TYPICAL, NULL, NULL,
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

//	rxLen = 63;
//	I3C_Master_Insert_Task_ENTDAA(&rxLen, NULL, I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL,
//		NULL, NULL, port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);
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
