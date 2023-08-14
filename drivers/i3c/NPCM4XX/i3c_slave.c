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
#include <drivers/i3c/NPCM4XX/api_i3c.h>

I3C_REG_ITEM_t *pSlaveReg[I3C_PORT_MAX] = { NULL, NULL };

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback for I3C slave
 * @param [in]      TaskInfo        Pointer to the running task
 * @param [in]      ErrDetail       task result
 * @return                          final task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_Slave_Callback(__u32 TaskInfo, __u32 ErrDetail)
{
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	__u32 ret;

	if (TaskInfo == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTaskInfo = (I3C_TASK_INFO_t *)TaskInfo;

	if (ErrDetail == I3C_ERR_HW_NOT_SUPPORT) {
	} else if (ErrDetail == I3C_ERR_NACK) {
		/* Master nack the slave task */
	} else if (ErrDetail == I3C_ERR_NACK_SLVSTART) {
		return I3C_DO_NACK_SLVSTART(pTaskInfo);
	} else if (ErrDetail == I3C_ERR_OK) {
		if (pTaskInfo->callback != NULL) {
		}
	}

	pDevice = I3C_Get_INODE(pTaskInfo->Port);

	if (pDevice->pOwner == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pBus = pDevice->pOwner;

	ret = pTaskInfo->result;
	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;
	pBus->busState = I3C_BUS_STATE_IDLE;
	return ret;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback if master nack any slave request
 * @param [in]      TaskInfo        Pointer to the running task
 * @return                          result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_NACK_SLVSTART(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;

	I3C_Notify(pTaskInfo);
	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;
	pBus->busState = I3C_BUS_STATE_IDLE;

	return I3C_ERR_NACK_SLVSTART;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Start to run slave's task
 * @param [in]      Parm            Pointer to taskinfo
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Slave_Start_Request(__u32 Parm)
{
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_TRANSFER_TASK_t *pTask;

	if (Parm == 0) {
		return;
	}

	pTaskInfo = (I3C_TASK_INFO_t *)Parm;

	if (pTaskInfo->pTask == NULL) {
		return;
	}

	pTask = pTaskInfo->pTask;

	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_IBI) {
		hal_I3C_Start_IBI(pTaskInfo);
	} else if (pTask->protocol == I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) {
		hal_I3C_Start_Master_Request(pTaskInfo);
	} else if (pTask->protocol == I3C_TRANSFER_PROTOCOL_HOT_JOIN) {
		hal_I3C_Start_HotJoin(pTaskInfo);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Finish slave task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Slave_End_Request(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;

	if (Parm == 0) {
		return;
	}

	pTask = (I3C_TRANSFER_TASK_t *)Parm;
	pTaskInfo = pTask->pTaskInfo;
	I3C_Slave_Callback((__u32)pTaskInfo, pTaskInfo->result);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Assigned data to response
 * @param [in]      pDevice         Pointer to the slave device
 * @param [in]      wrLen           response data length
 * @param [in]      pWrBuf          response data buffer
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Slave_Prepare_Response(I3C_DEVICE_INFO_t *pDevice, __u16 wrLen, __u8 *pWrBuf)
{
	I3C_ErrCode_Enum result;
	__u8 *pTxBuf;

	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if ((pWrBuf == NULL) && (wrLen != 0)) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (IS_Internal_DEVICE(pDevice) == FALSE) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	result = hal_I3C_Slave_TX_Free(pDevice->port);
	if (result != I3C_ERR_OK) {
		hal_I3C_Stop_Slave_TX(pDevice);
	}

	pTxBuf = (__u8 *)hal_I3C_MemAlloc(wrLen);
	if (pTxBuf == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	memcpy(pTxBuf, pWrBuf, wrLen);
	pDevice->pTxBuf = pTxBuf;
	pDevice->txOffset = 0;
	pDevice->txLen = wrLen;

	return Setup_Slave_Write_DMA(pDevice);
}

I3C_ErrCode_Enum I3C_Slave_Update_Pending(I3C_DEVICE_INFO_t *pDevice, __u8 mask)
{
	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (IS_Internal_DEVICE(pDevice) == FALSE) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY)
		&& (pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	return hal_I3C_Set_Pending(pDevice->port, mask);
}

I3C_ErrCode_Enum I3C_Slave_Finish_Response(I3C_DEVICE_INFO_t *pDevice)
{
	if (pDevice == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	if (IS_Internal_DEVICE(pDevice) == FALSE)
		return I3C_ERR_PARAMETER_INVALID;

	if (pDevice->txLen == 0)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice->txOffset = pDevice->txLen;

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum I3C_Slave_Check_Response_Complete(I3C_DEVICE_INFO_t *pDevice)
{
	I3C_ErrCode_Enum result;
	__u16 txNotSendLen;

	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (IS_Internal_DEVICE(pDevice) == FALSE) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pDevice->txLen == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	result = hal_I3C_Slave_Query_TxLen(pDevice->port, &txNotSendLen);
	if ((result == I3C_ERR_OK) && (txNotSendLen == 0)) {
		if (pDevice->pTxBuf != NULL) {
			hal_I3C_MemFree(pDevice->pTxBuf);
			pDevice->pTxBuf = NULL;
			pDevice->txLen = 0;
		}

		return I3C_ERR_OK;
	}

	return I3C_ERR_PENDING;
}

I3C_ErrCode_Enum I3C_Slave_Check_IBIDIS(I3C_DEVICE_INFO_t *pDevice, _Bool *bRet)
{
	uint8_t eventSupportMask;

	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (bRet == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (IS_Internal_DEVICE(pDevice) == FALSE) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	eventSupportMask = hal_I3C_get_event_support_status(pDevice->port);
	if (eventSupportMask & DISEC_MASK_ENINT) {
		*bRet = TRUE;
	} else {
		*bRet = FALSE;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           configure PDMA for slave write transfer
 * @param [in]      pDevice         Pointer to device object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum Setup_Slave_Write_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pDevice->port >= I3C_PORT_MAX) {
		return I3C_ERR_TASK_INVALID;
	}

	if ((pDevice->txLen - pDevice->txOffset) == 0) {
		return I3C_ERR_OK;
	}

	hal_I3C_DMA_Write(pDevice->port, pDevice->mode, &(pDevice->pTxBuf[pDevice->txOffset]),
		pDevice->txLen - pDevice->txOffset);
	pDevice->txOffset = pDevice->txLen;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           configure PDMA for slave read transfer
 * @param [in]      pDevice         Pointer to device object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum Setup_Slave_Read_DMA(I3C_DEVICE_INFO_t *pDevice)
{
	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pDevice->port >= I3C_PORT_MAX) {
		return I3C_ERR_TASK_INVALID;
	}

	if ((pDevice->rxLen - pDevice->rxOffset) == 0) {
		return I3C_ERR_OK;
	}

	hal_I3C_DMA_Read(pDevice->port, pDevice->mode, &(pDevice->pRxBuf[pDevice->rxOffset]),
		pDevice->rxLen - pDevice->rxOffset);
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           configure PDMA for IBI (slave write) transfer
 * @param [in]      pDevice         Pointer to device object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum Setup_Slave_IBI_DMA(I3C_DEVICE_INFO_t *pDevice)
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
	pTask = pDevice->pTaskListHead;

	if (pTask->frame_idx >= pTask->frame_count) {
		return I3C_ERR_PARAMETER_INVALID;
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

void I3C_Update_Dynamic_Address(__u32 Parm)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	port = (I3C_PORT_Enum)Parm;
	pDevice = api_I3C_Get_INODE(port);
	pDevice->dynamicAddr = hal_i3c_get_dynamic_address(port);
	pDevice->bRunI3C = TRUE;
}

void I3C_Prepare_To_Read_Command(__u32 Parm)
{
	if (Parm >= I3C_PORT_MAX)
		return;

	I3C_PORT_Enum port = (I3C_PORT_Enum) Parm;
	I3C_DEVICE_INFO_t *pDevice = api_I3C_Get_INODE(port);

	slvRxOffset[port] = 0;
	slvRxLen[port] = MAX_READ_LEN;
	hal_I3C_DMA_Read(port, pDevice->mode, slvRxBuf[port], slvRxLen[port]);
}

uint8_t GetCmdWidth(uint8_t width_type)
{
	if (width_type == 0)
		return 1;
	if (width_type == 1)
		return 2;
	return 0;
}
