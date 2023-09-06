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

LOG_MODULE_REGISTER(npcm4xx_i3c_master, CONFIG_I3C_LOG_LEVEL);

extern struct k_work_q npcm4xx_i3c_work_q[I3C_PORT_MAX];
extern struct k_work work_retry[I3C_PORT_MAX];
extern struct k_work work_rcv_ibi[I3C_PORT_MAX];

/**
 * @brief                           Callback for I3C master
 * @param [in]      TaskInfo        Pointer to the running task
 * @param [in]      ErrDetail       task result
 * @return                          final task result
 */

__u32 I3C_Master_Callback(__u32 TaskInfo, __u32 ErrDetail)
{
	const struct device *dev;
	struct i3c_npcm4xx_obj *obj;
	struct i3c_npcm4xx_xfer *xfer;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	__u32 ret;

	if (TaskInfo == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTaskInfo = (I3C_TASK_INFO_t *) TaskInfo;

	if (pTaskInfo->Port == I3C1_IF) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c0), okay)
		dev = DEVICE_DT_GET(DT_NODELABEL(i3c0));
		__ASSERT_NO_MSG(device_is_ready(dev));
#endif
	} else if (pTaskInfo->Port == I3C2_IF) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c1), okay)
		dev = DEVICE_DT_GET(DT_NODELABEL(i3c1));
		__ASSERT_NO_MSG(device_is_ready(dev));
#endif
	} else {

	}

	obj = DEV_DATA(dev);
	xfer = obj->curr_xfer;

	if (ErrDetail == I3C_ERR_NACK) {
		return I3C_DO_NACK(pTaskInfo);
	}

	/* if (ErrDetail == I3C_ERR_WRABT) { return I3C_DO_WRABT(pTaskInfo); } */
	if (ErrDetail == I3C_ERR_SLVSTART) {
		return I3C_DO_SLVSTART(pTaskInfo);
	}
	if (ErrDetail == I3C_ERR_SW_TIMEOUT) {
		return I3C_DO_SW_TIMEOUT(pTaskInfo);
	}

	/*
	 * IBI / Hot-Join / MasterRequest tasks are forked by SLV_START
	 * So, there is no xfer to return status and data.
	 * We should define callback functions for these cases.
	 */
	if (ErrDetail == I3C_ERR_IBI) {
		return I3C_DO_IBI(pTaskInfo);
	}
	if (ErrDetail == I3C_ERR_HJ) {
		return I3C_DO_HOT_JOIN(pTaskInfo);
	}
	if (ErrDetail == I3C_ERR_MR) {
		return I3C_DO_MASTER_REQUEST(pTaskInfo);
	}

	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pDevice = I3C_Get_INODE(pTaskInfo->Port);
	pBus = pDevice->pOwner;

	if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		I3C_DO_ENTDAA(pTaskInfo);
	}

	if (CCC_TRANSFER_PROTOCOL(pTask->protocol)) {
		if (pTask->pWrBuf[0] == CCC_BROADCAST_ENEC) {
			I3C_DO_ENEC(pTaskInfo);
		}
		if (pTask->pWrBuf[0] == CCC_BROADCAST_DISEC) {
			I3C_DO_DISEC(pTaskInfo);
		}
		/*
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTAS0)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTAS1)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTAS2)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTAS3)
		 */

		if (pTask->pWrBuf[0] == CCC_BROADCAST_RSTDAA) {
			I3C_DO_RSTDAA(pTaskInfo);
		}

		/*
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_DEFSLVS)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_SETMWL)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_SETMRL)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTTM)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_SETBUSCON)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENDXFER)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR0)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR1)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR2)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR3)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR4)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR5)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR6)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_ENTHDR7)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_SETXTIME)
		 */

		if (pTask->pWrBuf[0] == CCC_BROADCAST_SETAASA) {
			I3C_DO_SETAASA(pTaskInfo);
		}

		/*
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_RSTACT)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_DEFGRPA)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_RSTGRPA)
		 * if (pTask->pWrBuf[0] == CCC_BROADCAST_MLANE)
		 */

		if (pTask->pWrBuf[0] == CCC_BROADCAST_SETHID) {
			I3C_DO_SETHID(pTaskInfo);
		}
		/* if (pTask->pWrBuf[0] == CCC_BROADCAST_DEVCTRL) */

		/* I3C_TRANSFER_PROTOCOL_CCCw */
		if (pTask->pWrBuf[0] == CCC_DIRECT_ENEC) {
			I3C_DO_ENEC(pTaskInfo);
		}
		if (pTask->pWrBuf[0] == CCC_DIRECT_DISEC) {
			I3C_DO_DISEC(pTaskInfo);
		}

		/*
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_ENTAS0)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_ENTAS1)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_ENTAS2)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_ENTAS3)
		 **/

		if (pTask->pWrBuf[0] == CCC_DIRECT_RSTDAA) {
			I3C_DO_RSTDAA(pTaskInfo);
		}
		if (pTask->pWrBuf[0] == CCC_DIRECT_SETDASA) {
			I3C_DO_SETDASA(pTaskInfo);
		}

		/*
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETNEWDA)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETMWL)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETMRL)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_ENDXFER)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETBRGTGT)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETROUTE)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETXTIME)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_RSTACT)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_SETGRPA)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_RSTGRPA)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_MLANE)
		 */

		/* I3C_TRANSFER_PROTOCOL_CCCr */
		/*
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETMWL)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETMRL)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETPID)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETBCR)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETDCR)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETSTATUS)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETACCCR)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETMXDS)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETCAPS)
		 * if (pTask->pWrBuf[0] == CCC_DIRECT_GETXTIME)
		 */
	}

	/* Indirect Callback, callback defined in create master task */
	if ((pTaskInfo->callback != NULL) && (pTaskInfo->callback != I3C_Master_Callback)) {
		pTaskInfo->callback((__u32)pTaskInfo, ErrDetail);
	}

	ret = pTaskInfo->result;
	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;
	pBus->busState = I3C_BUS_STATE_IDLE;

	xfer->ret = ret;
	k_sem_give(&xfer->sem);
	return ret;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                Callback if meet any one of the following cases
 *                       1. address nack (device not present) -> stop
 *                       2. slave tx data not ready for read transfer -> retry -> stop
 *                       3. slave does not accept cmd/data (ex: ENTDAA -> not implement yet)
 * @param [in] pTaskInfo Pointer to the running task
 * @return               task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_NACK(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_BUS_INFO_t *pBus;
	I3C_BUS_STATE_Enum busNextState;
	__u32 ret;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;

	if (pTask->frame_idx >= pTask->frame_count) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];

	if (pDevice->pOwner == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pFrame = &pTask->pFrameList[pTask->frame_idx];
	pBus = pDevice->pOwner;

	ret = I3C_ERR_OK;

	if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		busNextState = I3C_BUS_STATE_IDLE;

		if ((pFrame->direction == I3C_TRANSFER_DIR_WRITE) && (pTask->frame_idx != 0)) {
			ret = I3C_Notify(pTaskInfo);

			/* update busNextState */
			if (pTask->protocol == I3C_TRANSFER_PROTOCOL_CCCb) {
				if (pTask->pWrBuf[0] == CCC_BROADCAST_RSTDAA) {
					busNextState = I3C_BUS_STATE_INIT;
				} else if (pTask->pWrBuf[0] == CCC_BROADCAST_SETHID) {
					busNextState = I3C_BUS_STATE_INIT;
				} else if (pTask->pWrBuf[0] == CCC_BROADCAST_SETAASA) {
					busNextState = I3C_BUS_STATE_INIT;
				}
			} else if (pTask->protocol == I3C_TRANSFER_PROTOCOL_CCCw) {
				if (pTask->pWrBuf[0] == CCC_DIRECT_SETDASA) {
					busNextState = I3C_BUS_STATE_INIT;
				}
			}

			I3C_Complete_Task(pTaskInfo);
			pBus->pCurrentTask = NULL;
			pBus->busState = busNextState;
			return ret;
		}

		/* task not success, but we should not retry it again */
		if (((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) == 0)
				|| (pFrame->retry_count == 0)) {
			if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
				return I3C_DO_ENTDAA(pTaskInfo);
			}

			I3C_Notify(pTaskInfo);
			I3C_Complete_Task(pTaskInfo);
			pBus->pCurrentTask = NULL;
			pBus->busState = I3C_BUS_STATE_IDLE;
			return I3C_ERR_NACK;
		}

		/* task not success, but we will retry it again */
		pFrame->retry_count--;
		pTaskInfo->result = I3C_ERR_PENDING;
		ret = I3C_ERR_PENDING;

		if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			pFrame->flag |= I3C_TRANSFER_REPEAT_START;
			I3C_Master_Start_Request((__u32)pTaskInfo);
		}
	} else if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) ||
			   (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		/* TODO: retry if not disabled */
	}

	return ret;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                Callback when slave nack I2C write is happened
 *                       1. Try to update read only register in I2C mode
 *                       2. Try to access I3C device with I2C write transfer
 * @param [in] pTaskInfo Pointer to the running task
 * @return               task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_WRABT(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	if (pDevice->pOwner == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	/* I3C_Notify(pTaskInfo); */
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when SLVSTART is happened and not to retry again
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SLVSTART(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;

	if (pTask->frame_idx >= pTask->frame_count) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pFrame = &pTask->pFrameList[pTask->frame_idx];
	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;

	if (((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) == 0) || (pFrame->retry_count == 0)) {
		I3C_Notify(pTaskInfo);
		return pTaskInfo->result;
	}

	pFrame->retry_count--;
	pTaskInfo->result = I3C_ERR_PENDING;
	pBus->pCurrentTask = NULL;
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when timeout is happened
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SW_TIMEOUT(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	__u32 ret;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;

	ret = I3C_Notify(pTaskInfo);
	if (ret == 0) {
		I3C_Complete_Task(pTaskInfo);
		FreeTaskNode(pTask);
		pBus->pCurrentTask = NULL;
		return I3C_ERR_OK;
	}

	return ret;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when IBI is happened
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_IBI(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_BUS_INFO_t *pBus;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pTaskInfo->callback != NULL) {
		/* pTaskInfo->callback(TaskInfo, ErrDetail); */
	}

	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	pBus->pCurrentMaster->bAbort = FALSE;

	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;
	pBus->busState = I3C_BUS_STATE_IDLE;
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when Hot Join is happened
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_HOT_JOIN(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_FRAME_t *pFrame;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];

	if (pDevice->pOwner == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pBus = pDevice->pOwner;
	pFrame = &pTask->pFrameList[pTask->frame_idx];

	if (((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) == 0) || (pFrame->retry_count == 0)) {
		I3C_Notify(pTaskInfo);
		return pTaskInfo->result;
	}

	pFrame->retry_count--;
	pTaskInfo->result = I3C_ERR_PENDING;
	pBus->pCurrentTask = NULL;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when Master Request is happened
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_MASTER_REQUEST(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_BUS_INFO_t *pBus;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pBus = Get_Bus_From_Port(pTaskInfo->Port);

	/* we can't control the external slave device which will change to current master */
	/* we will update the internal slave device in slave's callback */

	I3C_Complete_Task(pTaskInfo);
	pBus->pCurrentTask = NULL;
	pBus->busState = I3C_BUS_STATE_IDLE;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack ENEC task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_ENEC(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_PROTOCOL_Enum protocol;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u8 i;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask->pWrBuf == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	pTask = pTaskInfo->pTask;
	protocol = pTask->protocol;

	if (protocol == I3C_TRANSFER_PROTOCOL_CCCb) {
		pDev = pBus->pDevList;

		while (pDev != NULL)
			pDev = pDev->pNextDev;
	} else {
		for (i = 1; i <= pTask->frame_idx; i++) {
			pDev = GetDevInfoByDynamicAddr(pBus, pTask->pFrameList[i].address);
			if (pDev == NULL) {
				continue;
			}
		}
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack DISEC task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_DISEC(I3C_TASK_INFO_t *pTaskInfo)
{
	return I3C_DO_ENEC(pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack RSTDAA task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_RSTDAA(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;
	pDev = pBus->pDevList;

	while (pDev != NULL) {
		if (pDev->attr.b.present) {
			if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
				pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;

				if ((pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER)
						|| (pDevice->mode == I3C_DEVICE_MODE_DISABLE)) {
					pDev = pDev->pNextDev;
					continue;
				}

				pDevice->bRunI3C = FALSE;
				pDevice->dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
				pDevice->ackIBI = FALSE;
				I3C_Clean_Slave_Task(pDevice);
			}

			pDev->attr.b.runI3C = 0;
			pDev->dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
			if ((pDev->attr.b.reqRSTDAA) && (pDev->attr.b.doneRSTDAA == 0)) {
				pDev->attr.b.doneRSTDAA = 1;
			}
		}

		pDev = pDev->pNextDev;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack ENTDAA task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_ENTDAA(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	I3C_DEVICE_INFO_t *pDevice;

	__u8 i;
	__u8 *pid;
	__u8 bcr;
	__u8 dcr;
	__u8 dyn_addr;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	if (pBus == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	for (i = 1; i <= pTask->frame_idx; i++) {
		pid = pTask->pFrameList[pTask->frame_idx].access_buf;
		bcr = pTask->pFrameList[pTask->frame_idx].access_buf[6];
		dcr = pTask->pFrameList[pTask->frame_idx].access_buf[7];
		dyn_addr = pTask->pFrameList[pTask->frame_idx].access_buf[8];

		pDev = pBus->pDevList;
		while (pDev != NULL) {
			if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
				pDevice = pDev->pDeviceInfo;

				if ((pDev->bcr == bcr) && (pDev->dcr == dcr)
						&& (((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY)
							|| (pDevice->mode ==
								I3C_DEVICE_MODE_SECONDARY_MASTER))
							&& (hal_i3c_get_dynamic_address(
								pDevice->port) == dyn_addr))) {
					pDevice->bRunI3C = TRUE;
					pDevice->dynamicAddr = dyn_addr;

					if (((pDev->bcr & 0x06) == 0x02)
							|| ((pDev->bcr & 0x06) == 0x06)) {
						pDevice->ackIBI = TRUE;
					} else {
						pDevice->ackIBI = FALSE;
					}

					pDev->attr.b.runI3C = 1;
					pDev->dynamicAddr = dyn_addr;
					break;
				}
			} else {
				if ((pDev->bcr == bcr) && (pDev->dcr == dcr)
						&& (memcmp(pDev->pid, pid, 6) == 0)) {
					pDev->attr.b.runI3C = 1;
					pDev->dynamicAddr = dyn_addr;
					break;
				}
			}

			pDev = pDev->pNextDev;
		}
	}

	if (pTaskInfo->result == I3C_ERR_MEMORY_RAN_OUT) {
		/* reuse task to do ENTDAA again */
		pTask->frame_idx = 0;
		I3C_Master_Start_Request((__u32)pTaskInfo);
		return I3C_ERR_OK;
	}

	if (pTaskInfo->result == I3C_ERR_NACK) {
		/* slave reject address, reuse this task to retry again */
		/* after we release address with higher priority */
		pTask->frame_idx = 0;
	}

	/* Remove from master's task */
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack SETAASA task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SETAASA(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;
	pDev = pBus->pDevList;

	while (pDev != NULL) {
		/* only SPD5118 support SETAASA */
		if ((pDev->attr.b.present) && /* IS_SPD5118_DEVICE(pDev->pDeviceInfo) */
			(pDev->staticAddr >= 0x50) && (pDev->staticAddr <= 0x57) &&
			(pDev->attr.b.reqSETAASA)) {
			/* SPD5118_DO_SETAASA(pDev->dynamicAddr & 0x07); */

			pDev->attr.b.runI3C = TRUE;
			pDev->dynamicAddr = pDev->staticAddr;

			if ((pDev->attr.b.reqSETAASA) && (pDev->attr.b.doneSETAASA == 0))
				pDev->attr.b.doneSETAASA = 1;
		}

		pDev = pDev->pNextDev;
	}

	/* Remove from master's task */
	/* pBus->busState = I3C_BUS_STATE_INIT; */
	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack SETHID task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SETHID(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u32 result;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;
	pDev = pBus->pDevList;
	result = pTaskInfo->result;

	while (pDev != NULL) {
		pDev = pDev->pNextDev;
	}

	return result;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack SETDASA task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SETDASA(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u8 i;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;
	pTask = pTaskInfo->pTask;

	for (i = 1; i < pTask->frame_count; i++) {
		pFrame = &pTask->pFrameList[i];
		pDev = GetDevInfoByStaticAddr(pBus, pFrame->address);

		if (pDev->attr.b.present) {
			/* SPD5118 does not support SETDASA */
			if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
				pDevice = (I3C_DEVICE_INFO_t *) pDev->pDeviceInfo;
				if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
					continue;
				}
				if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
					continue;
				}

				pDevice->bRunI3C = TRUE;
				pDevice->dynamicAddr = pDevice->staticAddr;
				pDevice->ackIBI = TRUE;
			}

			pDev->attr.b.runI3C = 1;
			pDev->dynamicAddr = pDev->staticAddr;

			if ((pDev->attr.b.reqSETDASA) && (pDev->attr.b.doneSETDASA == 0)) {
				pDev->attr.b.doneSETDASA = 1;
			}
		}
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Callback when slave ack SETNEWDA task
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_DO_SETNEWDA(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	__u8 i;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->pTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	if (pTaskInfo->Port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	pBus = Get_Bus_From_Port(pTaskInfo->Port);
	if (pBus->pCurrentMaster == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = pBus->pCurrentMaster;
	for (i = 1; i < pTask->frame_count; i++) {
		pFrame = &pTask->pFrameList[i];
		pDev = GetDevInfoByDynamicAddr(pBus, pFrame->address);

		if (pDev->attr.b.present) {
			/* SPD5118 doesn't support SETNEWDA */
			if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
				pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;
				if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
					continue;
				}
				if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
					continue;
				}

				pDevice->dynamicAddr = pFrame->access_buf[0];
				pDev->dynamicAddr = pFrame->access_buf[0];
			}
		}
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Start to run master's task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Start_Request(__u32 Parm)
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
		/* hal_I3C_Master_Stall(pBus, pTaskInfo->Port); */

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
void I3C_Master_Stop_Request(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	__u8 port;
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

	I3C_Master_Callback((uint32_t) pTaskInfo, pTaskInfo->result);
	hal_I3C_Stop(port);
	pDevice->bAbort = FALSE;

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
void I3C_Master_Retry_Frame(__u32 Parm)
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
			I3C_Master_Stop_Request((__u32) pTask);

			/* wait a moment for slave to prepare response data */
			/* k_usleep(WAIT_SLAVE_PREPARE_RESPONSE_TIME); */
		}

		I3C_Master_Start_Request((__u32) pTaskInfo);
	} else {
		I3C_Master_Stop_Request((__u32) pTask);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Finish master task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_End_Request(__u32 Parm)
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

	I3C_Master_Callback((__u32)pTaskInfo, pTaskInfo->result);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Keep running next frame of the master task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
#define MACRO pTask->pFrameList[pTask->frame_idx + 1].access_buf[8] = pDev->staticAddr;

void I3C_Master_Run_Next_Frame(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u8 pid[6];
	__u8 bcr;
	__u8 dcr;
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
						MACRO;
						break;
					}

					pDev = pDev->pNextDev;
					continue;
				}

				/* adopt user define in advance */
				MACRO;
				break;
			}

			pDev = pDev->pNextDev;
		}
	}

	pTask->frame_idx++;
	I3C_Master_Start_Request((__u32)pTaskInfo);
}
#undef MACRO

/*------------------------------------------------------------------------------*/
/**
 * @brief                 Insert event task to handle slave request
 * @param [in]      Parm  i3c master detect the SLVSTART
 * @return                none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_New_Request(__u32 Parm)
{
	__u8 port;
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;

	if (Parm >= I3C_PORT_MAX) {
		return;
	}

	port = (__u8)Parm;
	I3C_Master_Insert_Task_EVENT(IBI_PAYLOAD_SIZE_MAX, NULL, I3C_TRANSFER_SPEED_SDR_IBI,
		TIMEOUT_TYPICAL, I3C_Master_Callback, port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);

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
	pBus->busState = I3C_BUS_STATE_IDLE;
	I3C_Master_Start_Request((__u32)pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                Insert DISEC task to disable slave request
 * @param [in]      Parm Pointer to task
 * @return               none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Insert_DISEC_After_IbiNack(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	__u8 port;
	I3C_BUS_INFO_t *pBus;
	I3C_IBITYPE_Enum ibiType;
	__u8 ibiAddress;
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
	I3C_Master_Start_Request((__u32)pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Ack the IBI
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_IBIACK(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	uint8_t ibiAddress;
	I3C_DEVICE_INFO_SHORT_t *pSlvDev;

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
	if (pTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		hal_I3C_Disable_Master_RX_DMA(port);

		/* Insert Event task and assume event task has running to
		 * restore IBI Type and IBI address
		 */
		I3C_Master_Insert_Task_EVENT(IBI_PAYLOAD_SIZE_MAX, NULL,
			I3C_TRANSFER_SPEED_SDR_IBI, TIMEOUT_TYPICAL, I3C_Master_Callback,
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
void I3C_Master_Insert_GETACCMST_After_IbiAckMR(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	uint8_t port;
	uint8_t ibiAddress;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	I3C_TRANSFER_FRAME_t *pFrame;
	__u8 wrBuf[2];
	__u8 *rdBuf;

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

	rdBuf = hal_I3C_MemAlloc(1);
	wrBuf[0] = ibiAddress;
	I3C_Master_Insert_Task_CCCr(CCC_DIRECT_GETACCMST, 1, 1, 1, wrBuf, rdBuf,
		I3C_TRANSFER_SPEED_SDR_IBI, TIMEOUT_TYPICAL,
		I3C_Master_Callback, port, I3C_TASK_POLICY_INSERT_FIRST, IS_HIF);

	pTask = pBus->pCurrentMaster->pTaskListHead;
	pTaskInfo = pTask->pTaskInfo;
	pFrame = &pTask->pFrameList[pTask->frame_idx];
	pFrame->flag |= I3C_TRANSFER_REPEAT_START;
	I3C_Master_Start_Request((uint32_t) pTaskInfo);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           ack the Hot Join, then insert ENTDAA task
 * @param [in]      Parm            Pointer to task
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_Master_Insert_ENTDAA_After_IbiAckHJ(__u32 Parm)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	__u8 port;


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

	pTaskInfo->result = I3C_ERR_OK;
	*pTask->pRdLen = 0;

	hal_I3C_Disable_Master_RX_DMA(port);
	hal_I3C_Ack_IBI_Without_MDB(port);

	/* Must to use STOP to notify slave finish hot-join task
	 * Can't use RESTART + ENTDAA, that will cause MERRWARN.INVREQ
	 */
	I3C_Master_Stop_Request((__u32)pTask);
	I3C_Master_Insert_Task_ENTDAA(63, NULL, I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL,
		NULL, port, I3C_TASK_POLICY_INSERT_FIRST, NOT_HIF);
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
