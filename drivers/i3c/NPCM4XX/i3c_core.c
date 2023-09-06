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

I3C_BUS_INFO_t gBus[I3C_BUS_COUNT_MAX] = {
	{.busno = 0, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
	{.busno = 1, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
#if (I3C_BUS_COUNT_MAX > 2)
	{.busno = 2, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
	{.busno = 3, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
	{.busno = 4, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
	{.busno = 5, .DevCount = 0, .pDevList = NULL, .pCurrentMaster = NULL,
		.busState = I3C_BUS_STATE_DEFAULT, .pCurrentTask = NULL},
#endif
};

I3C_DEVICE_INFO_t gI3c_dev_node_internal[I3C_PORT_MAX] = {
	{.port = I3C1_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
	{.port = I3C2_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
#if (I3C_BUS_COUNT_MAX > 2)
	{.port = I3C3_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
	{.port = I3C4_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
	{.port = I3C5_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
	{.port = I3C6_IF, .mode = I3C_DEVICE_MODE_DISABLE, .callback = NULL,
		.pOwner = NULL, .pDevInfo = NULL, .task_count = 0, .pTaskListHead = NULL},
#endif
};

uint8_t slvRxBuf[I3C_PORT_MAX][MAX_READ_LEN];

uint16_t slvRxLen[I3C_PORT_MAX] = {
	MAX_READ_LEN, MAX_READ_LEN,
#if (I3C_BUS_COUNT_MAX > 2)
	MAX_READ_LEN, MAX_READ_LEN, MAX_READ_LEN, MAX_READ_LEN
#endif
};

uint16_t slvRxOffset[I3C_PORT_MAX] = {
	0, 0,
#if (I3C_BUS_COUNT_MAX > 2)
	0, 0, 0, 0
#endif
};


/*-----------------------------------------------------------------------------------------*/
/**
 * @brief       Used to customize Internal Devices for both I3C1 & I3C2
 * @return      None
 */
/*------------------------------------------------------------------------------------------*/
void I3C_Setup_Internal_Device(void)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		I3C_Port_Default_Setting(port);

		pDevice = &gI3c_dev_node_internal[port];
		hal_I3C_Config_Internal_Device(port, pDevice);

		/* validate mode setting */
		if ((pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) &&
			(pDevice->capability.MASTER == FALSE)) {
			return;
		}

		if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) &&
			(pDevice->capability.SLAVE == FALSE)) {
			return;
		}

		if ((pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER) &&
			((pDevice->capability.MASTER == FALSE) ||
			 (pDevice->capability.SLAVE == FALSE))) {
			return;
		}

		/* validate for i3c/i2c device */
		if (pDevice->bRunI3C) {
			if (!IsValidDynamicAddress(pDevice->dynamicAddr)) {
				return;
			}
		} else {
			if (!IsValidStaticAddress(pDevice->staticAddr)) {
				return;
			}
		}

		if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
			pDevice->callback = NULL;
		} else if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
			pDevice->disableTimeout = TRUE;
			pDevice->enableOpenDrainHigh = TRUE;
			pDevice->enableOpenDrainStop = FALSE;
			pDevice->callback = I3C_Master_Callback;
		} else if (pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) {
			pDevice->callback = I3C_Slave_Callback;
		} else if (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER) {
			pDevice->callback = I3C_Slave_Callback;
		}

		hal_I3C_Config_Device(pDevice);
	}
}

/*----------------------------------------------------------------------*/
/**
 * @brief   User can define External Devices
 * @return  None
 */
/*----------------------------------------------------------------------*/
void I3C_Setup_External_Device(void)
{
}

/*----------------------------------------------------------------------*/
/**
 * @brief    Used to update bus state for bus enumeration or hot join
 * @return   None
 */
/*----------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Setup_Bus(void)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		pDevice = &gI3c_dev_node_internal[port];
		pBus = pDevice->pOwner;

		if (pBus->busState == I3C_BUS_STATE_DEFAULT) {
			if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
				continue;
			}

			if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
				/* bus master should perform bus enumeration later */
				if (pDevice == pBus->pCurrentMaster) {
					pBus->busState = I3C_BUS_STATE_INIT;
				}
			} else {
				/* slave might init by bus master or hot-join */
			}
		}
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief   return internal device object by port no
 * @return  device object
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_t *I3C_Get_INODE(I3C_PORT_Enum port)
{
	return &gI3c_dev_node_internal[port];
}

/*------------------------------------------------------------------------------*/
/**
 * @brief           return the bus master specified by the port
 * @param [in] port port
 * @return          pointer to the device object which is current master of the bus
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_t *Get_Current_Master_From_Port(I3C_PORT_Enum port)
{
	I3C_BUS_INFO_t *pBus;

	if (port >= I3C_PORT_MAX) {
		return NULL;
	}

	pBus = Get_Bus_From_Port(port);
	if (pBus == NULL) {
		return NULL;
	}

	return pBus->pCurrentMaster;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief          return port no by its device object
 * @return         port no
 */
/*------------------------------------------------------------------------------*/
I3C_PORT_Enum I3C_Get_IPORT(I3C_DEVICE_INFO_t *pDevice)
{
	I3C_PORT_Enum port;

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		if (pDevice == &gI3c_dev_node_internal[port]) {
			return port;
		}
	}

	return I3C_PORT_MAX;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Validate dynamic address setting
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
_Bool IsValidDynamicAddress(__u8 addr)
{
	if ((addr >= 0x08) && (addr <= 0x3D)) {
		return TRUE;
	}

	if ((addr >= 0x3F) && (addr <= 0x5D)) {
		return TRUE;
	}

	if ((addr >= 0x5F) && (addr <= 0x6D)) {
		return TRUE;
	}

	if ((addr >= 0x6F) && (addr <= 0x75)) {
		return TRUE;
	}

	if (addr == 0x77) {
		return TRUE;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Validate static address setting
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
_Bool IsValidStaticAddress(__u8 addr)
{
	if (addr == 0x7E) {
		return FALSE;
	}

	return TRUE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Collect internal device's capability
 * @param [in]      port            port
 * @return                          Pass if return I3C_ERR_OK
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Port_Default_Setting(I3C_PORT_Enum port)
{
	I3C_DEVICE_INFO_t *pDevice;

	if (port >= I3C_PORT_MAX) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pDevice = &gI3c_dev_node_internal[port];
	pDevice->port = port;

	pDevice->capability.MASTER = hal_i3c_get_capability_support_master(port);
	pDevice->capability.SLAVE = hal_i3c_get_capability_support_slave(port);

	pDevice->capability.FIFO_TX_LEN = hal_i3c_get_capability_Tx_Fifo_Len(port);
	pDevice->capability.FIFO_RX_LEN = hal_i3c_get_capability_Rx_Fifo_Len(port);

	pDevice->capability.DMA = hal_i3c_get_capability_support_DMA(port);
	pDevice->capability.INT = hal_i3c_get_capability_support_INT(port);
	pDevice->capability.HDR_DDR = hal_i3c_get_capability_support_DDR(port);
	pDevice->capability.ASYNC0 = hal_i3c_get_capability_support_ASYNC0(port);
	pDevice->capability.IBI = hal_i3c_get_capability_support_IBI(port);
	pDevice->capability.MASTER_REQUEST = hal_i3c_get_capability_support_Master_Request(port);
	pDevice->capability.HOT_JOIN = hal_i3c_get_capability_support_Hot_Join(port);

	pDevice->capability.OFFLINE = hal_i3c_get_capability_support_Offline(port);

	pDevice->capability.I3C_VER_1p0 = hal_i3c_get_capability_support_V10(port);
	pDevice->capability.I3C_VER_1p1 = hal_i3c_get_capability_support_V11(port);

	pDevice->mode = I3C_DEVICE_MODE_DISABLE;
	pDevice->callback = NULL;
	pDevice->bRunI3C = FALSE;

	pDevice->staticAddr = I2C_STATIC_ADDR_DEFAULT_7BIT;
	pDevice->dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;

	memset(pDevice->pid, 0x00, sizeof(pDevice->pid));

	pDevice->bcr = 0x00;
	pDevice->dcr = 0x00;
	pDevice->vendorID = 0;
	pDevice->partNumber = 0;

	pDevice->max_rd_len = hal_I3C_get_MAXRD(port);
	pDevice->max_wr_len = hal_I3C_get_MAXWR(port);

	pDevice->regCnt = 0;
	pDevice->pReg = NULL;
	pDevice->cmdIndex = CMD_DEFAULT;
	pDevice->stopSplitRead = FALSE;

	pDevice->enableOpenDrainHigh = FALSE;
	pDevice->enableOpenDrainStop = FALSE;
	pDevice->disableTimeout = TRUE;
	pDevice->ackIBI = FALSE;

	/* pDevice->maxDataRate = 0; */

	pDevice->pOwner = NULL;
	pDevice->pDevInfo = NULL;
	pDevice->task_count = 0;
	pDevice->pTaskListHead = NULL;

	pDevice->pRxBuf = NULL;
	pDevice->rxLen = 0;
	pDevice->rxOffset = 0;

	pDevice->pTxBuf = NULL;
	pDevice->txLen = 0;
	pDevice->txOffset = 0;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           return bus object by bus no
 * @param [in]      busno           bus no
 * @return                          pointer to the bus object specified by the bus no
 */
/*------------------------------------------------------------------------------*/
I3C_BUS_INFO_t *Get_Bus_From_BusNo(__u8 busno)
{
	if (busno >= I3C_BUS_COUNT_MAX) {
		return NULL;
	}

	return &gBus[busno];
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           return bus object by port
 * @param [in]      port            I3C port
 * @return                          pointer to the bus object specified by the port
 */
/*------------------------------------------------------------------------------*/
I3C_BUS_INFO_t *Get_Bus_From_Port(I3C_PORT_Enum port)
{
	I3C_DEVICE_INFO_t *pDevice;

	if (port >= I3C_PORT_MAX) {
		return NULL;
	}

	pDevice = &gI3c_dev_node_internal[port];
	return pDevice->pOwner;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                 validate bus state is idle or not ?
 * @param [in]      pBus  bus object
 * @return                true, if the bus is idle or STOP
 */
/*------------------------------------------------------------------------------*/
_Bool I3C_IS_BUS_KEEP_IDLE(I3C_BUS_INFO_t *pBus)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	if (pBus == NULL) {
		return FALSE;
	}

	if (pBus->pCurrentMaster != NULL) {
		pDevice = pBus->pCurrentMaster;
		port = pDevice->port;
		return hal_I3C_Is_Master_Idle(port);
	}

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		if (Get_Bus_From_Port(port) == pBus) {
			return hal_I3C_Is_Slave_Idle(port);
		}
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                        validate bus state is DAA or not ?
 * @param [in]      pBus         bus object
 * @return                       true, if the bus is during DAA
 */
/*------------------------------------------------------------------------------*/
_Bool I3C_IS_BUS_DURING_DAA(I3C_BUS_INFO_t *pBus)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	if (pBus == NULL) {
		return FALSE;
	}

	if (pBus->pCurrentMaster != NULL) {
		pDevice = pBus->pCurrentMaster;
		port = pDevice->port;
		return hal_I3C_Is_Master_DAA(port);
	}

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		if (Get_Bus_From_Port(port) == pBus) {
			return hal_I3C_Is_Slave_DAA(port);
		}
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                        validate SLVSTART is detected or not ?
 * @param [in]      pBus         bus object
 * @return                       true, if the SLVSTART is detected
 */
/*------------------------------------------------------------------------------*/
_Bool I3C_IS_BUS_DETECT_SLVSTART(I3C_BUS_INFO_t *pBus)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	if (pBus == NULL) {
		return FALSE;
	}

	if (pBus->pCurrentMaster != NULL) {
		pDevice = pBus->pCurrentMaster;
		port = pDevice->port;
		return hal_I3C_Is_Master_SLVSTART(port);
	}

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		if (Get_Bus_From_Port(port) == pBus) {
			return hal_I3C_Is_Slave_SLVSTART(port);
		}
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                        validate bus is waiting for the STOP or RESTART ?
 * @param [in]      pBus         bus object
 * @return                       true, if bus is waiting for the STOP or RESTART
 */
/*------------------------------------------------------------------------------*/
_Bool I3C_IS_BUS_WAIT_STOP_OR_RETRY(I3C_BUS_INFO_t *pBus)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;

	if (pBus == NULL) {
		return FALSE;
	}

	if (pBus->pCurrentMaster != NULL) {
		pDevice = pBus->pCurrentMaster;
		port = pDevice->port;
		return hal_I3C_Is_Master_NORMAL(port);
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create a simplified device info to describe device in the bus
 * @param [in]      pBus            The bus the device belongs to
 * @param [in]      pDevice         pointer to the device object
 * @param [in]      attr            describe the device is present or not and how to initialize
 * @param [in]      prefferedAddr   Used to define the preferred dynamic address for I3C, or
 *                                  static address for I2C
 * @param [in]      dynamicAddr     Used to record the dynamic address of the device
 * @param [in]      pid             pid of the device object
 * @param [in]      bcr             bcr of the device object
 * @param [in]      dcr             dcr of the device object
 * @return                          pointer to the simplified device info
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_SHORT_t *NewDevInfo(I3C_BUS_INFO_t *pBus, void *pDevice, I3C_DEVICE_ATTRIB_t attr,
	__u8 prefferedAddr, __u8 dynamicAddr, __u8 pid[], __u8 bcr, __u8 dcr)
{
	I3C_DEVICE_INFO_SHORT_t *pNewDev;
	I3C_DEVICE_INFO_SHORT_t *pThisDev;

	if (pBus == NULL) {
		return NULL;
	}

	if (pDevice == NULL) {
		return NULL;
	}

	pNewDev = (I3C_DEVICE_INFO_SHORT_t *)hal_I3C_MemAlloc(sizeof(I3C_DEVICE_INFO_SHORT_t));
	if (pNewDev == NULL) {
		return NULL;
	}

	pNewDev->pDeviceInfo = pDevice;
	pNewDev->attr = attr;
	pNewDev->dynamicAddr = dynamicAddr;
	memcpy(pNewDev->pid, pid, 6);
	pNewDev->bcr = bcr;
	pNewDev->dcr = dcr;
	pNewDev->staticAddr = prefferedAddr;
	pNewDev->pNextDev = NULL;

	if (pBus->DevCount == 0) {
		pBus->pDevList = pNewDev;
		pBus->DevCount = 1;
		return pNewDev;
	}

	pThisDev = pBus->pDevList;
	while (pThisDev->pNextDev != NULL) {
		pThisDev = pThisDev->pNextDev;
	}

	pThisDev->pNextDev = pNewDev;
	pBus->DevCount++;
	return pNewDev;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Get the device info from the bus
 * @param [in]      pBus            pointer to the bus object
 * @param [in]      slaveAddr       static address of the device object
 * @return                          Return device info with specified static address
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByStaticAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return NULL;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if (pDev->staticAddr == slaveAddr) {
			return pDev;
		}

		pDev = pDev->pNextDev;
	}

	return NULL;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Get the device info from the bus
 * @param [in]      pBus            pointer to the bus object
 * @param [in]      slaveAddr       dynamic address of the device object
 * @return                          Return device info with specified dynamic address
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByDynamicAddr(I3C_BUS_INFO_t *pBus, __u8 slaveAddr)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return NULL;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if (pDev->dynamicAddr == slaveAddr) {
			return pDev;
		}

		pDev = pDev->pNextDev;
	}

	return NULL;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                       Get the device info from the bus with specified slave task
 * @param [in]      pBus        pointer to the bus object
 * @param [in]      pTask       task
 * @return                      Return device info with specified task
 */
/*------------------------------------------------------------------------------*/
I3C_DEVICE_INFO_SHORT_t *GetDevInfoByTask(I3C_BUS_INFO_t *pBus, I3C_TRANSFER_TASK_t *pTask)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pThisTask;

	if (pBus == NULL) {
		return NULL;
	}

	if (pTask == NULL) {
		return NULL;
	}

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		pDevice = &gI3c_dev_node_internal[port];

		if (pDevice->pOwner != pBus) {
			continue;
		}

		if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
			continue;
		}

		if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
			continue;
		}

		pThisTask = pDevice->pTaskListHead;
		while (pThisTask != NULL) {
			if (pThisTask == pTask) {
				return pDevice->pDevInfo;
			}

			pThisTask = pThisTask->pNextTask;
		}
	}

	return NULL;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to check the device is internal device or not
 * @param [in]      pDevice         pointer to the device object
 * @return                          TRUE, if the device is an internal device
 */
/*------------------------------------------------------------------------------*/
_Bool IS_Internal_DEVICE(void *pDevice)
{
	I3C_PORT_Enum port;

	for (port = I3C1_IF; port < I3C_PORT_MAX; port++) {
		if (pDevice == &gI3c_dev_node_internal[port]) {
			return TRUE;
		}
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to reset devices on the bus
 * @param [in]      pBus            pointer to the bus object
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void I3C_Reset(__u8 busNo)
{
	I3C_BUS_INFO_t *pBus = NULL;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_DEVICE_INFO_SHORT_t *pDev = NULL;
	I3C_DEVICE_INFO_SHORT_t *pRemoveDev;
	I3C_TRANSFER_TASK_t *pTask;

	pBus = &gBus[busNo];
	pDev = pBus->pDevList;
	pBus->busState = I3C_BUS_STATE_WAIT_RESET_DONE;

	while (pDev != NULL) {
		pRemoveDev = pDev;

		if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
			pDevice = pDev->pDeviceInfo;

			while (pDevice->task_count) {
				if (pDevice->pTaskListHead == NULL) {
					break;
				}

				pTask = pDevice->pTaskListHead;
				I3C_Complete_Task(pTask->pTaskInfo);
			};

			/* used to disable I3C */
			pDevice->mode = I3C_DEVICE_MODE_DISABLE;
			pDevice->callback = NULL;
			pDevice->pOwner = NULL;
			pDevice->pDevInfo = NULL;
			hal_I3C_Config_Device(pDevice);
		}

		/* clear devices info on the bus */
		pDev = pDev->pNextDev;
		RemoveDevInfo(pBus, pRemoveDev);
	}

	pBus->pCurrentTask = NULL;
	pBus->pCurrentMaster = NULL;
	pBus->busState = I3C_BUS_STATE_DEFAULT;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Remove a specific device info from the bus
 * @param [in]      pBus            The bus the device belongs to
 * @param [in]      pDevInfo        Specify what should be removed
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void RemoveDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo)
{
	I3C_DEVICE_INFO_SHORT_t *pThisDev;
	I3C_DEVICE_INFO_SHORT_t *pPrevDev;

	if (pBus == NULL) {
		return;
	}

	if (pDevInfo == NULL) {
		return;
	}

	if (pBus->DevCount == 0) {
		return;
	}

	if (pBus->pDevList == NULL) {
		return;
	}

	pThisDev = pBus->pDevList;
	if (pThisDev == pDevInfo) {
		pBus->pDevList = pThisDev->pNextDev;
		hal_I3C_MemFree(pThisDev);
		pBus->DevCount--;
		return;
	}

	while (pThisDev->pNextDev != NULL) {
		pPrevDev = pThisDev;
		pThisDev = pPrevDev->pNextDev;

		if (pThisDev == pDevInfo) {
			pPrevDev->pNextDev = pThisDev->pNextDev;
			hal_I3C_MemFree(pThisDev);
			pBus->DevCount--;
			return;
		}
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Reset a specific device info from the bus
 * @param [in]      pBus            The bus the device belongs to
 * @param [in]      pDevInfo        Specify what should be removed
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void ResetDevInfo(I3C_BUS_INFO_t *pBus, I3C_DEVICE_INFO_SHORT_t *pDevInfo)
{
	I3C_DEVICE_INFO_SHORT_t *pThisDev;

	if (pBus == NULL) {
		return;
	}

	if (pDevInfo == NULL) {
		return;
	}

	if (pBus->DevCount == 0) {
		return;
	}

	if (pBus->pDevList == NULL) {
		return;
	}

	pThisDev = pBus->pDevList;
	if (pThisDev == pDevInfo) {
		pThisDev->dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
		return;
	}

	while (pThisDev->pNextDev != NULL) {
		pThisDev = pThisDev->pNextDev;

		if (pThisDev == pDevInfo) {
			pThisDev->dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
			return;
		}
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Any device in the bus running in I3C mode ?
 * @param [in]      pBus            bus object
 * @return                          TRUE, if at least one slave device works in I3C mode
 */
/*------------------------------------------------------------------------------*/
_Bool IS_I3C_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return FALSE;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if (IS_Internal_DEVICE(pDev->pDeviceInfo)) {
			pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;
			if (pDevice->bRunI3C) {
				return TRUE;
			}
		}

		pDev = pDev->pNextDev;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to check device in the bus needs to do RSTDAA
 * @param [in]      pBus            bus object
 * @return                          TRUE, if at least one device need to do RSTDAA
 */
/*------------------------------------------------------------------------------*/
_Bool IS_RSTDAA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return FALSE;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqRSTDAA) &&
			(pDev->attr.b.doneRSTDAA == 0)) {
			return TRUE;
		}

		pDev = pDev->pNextDev;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to check device in the bus needs to do SETHID
 * @param [in]      pBus            bus object
 * @return                          TRUE, if at least one device need to do SETHID
 */
/*------------------------------------------------------------------------------*/
_Bool IS_SETHID_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return FALSE;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETHID) &&
			(pDev->attr.b.doneSETHID == 0)) {
			return TRUE;
		}

		pDev = pDev->pNextDev;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to check device in the bus needs to do SETDASA
 * @param [in]      pBus            bus object
 * @return                          TRUE, if at least one device need to do SETDASA
 */
/*------------------------------------------------------------------------------*/
_Bool IS_SETDASA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return FALSE;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETDASA) &&
			(pDev->attr.b.doneSETDASA == 0)) {
			return TRUE;
		}

		pDev = pDev->pNextDev;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Used to check device in the bus needs to do SETAASA
 * @param [in]      pBus            bus object
 * @return                          TRUE, if at least one device need to do SETAASA
 */
/*------------------------------------------------------------------------------*/
_Bool IS_SETAASA_DEVICE_PRESENT(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return FALSE;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETAASA) &&
			(pDev->attr.b.doneSETAASA == 0)) {
			return TRUE;
		}

		pDev = pDev->pNextDev;
	}

	return FALSE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast ENEC task
 * @param [in]      pBus            bus object
 * @param [in]      mask            enable setting
 * @param [in]      policy          task insertion policy
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_ENEC(I3C_BUS_INFO_t *pBus, __u8 mask, I3C_TASK_POLICY_Enum policy)
{
	I3C_DEVICE_INFO_t *pDevice;
	__u16 TxLen;
	__u8 TxBuf[1];

	if (pBus == NULL) {
		return;
	}

	if (mask == 0) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	if (pDevice == NULL) {
		return;
	}

	TxLen = 1;
	TxBuf[0] = mask;
	I3C_Master_Insert_Task_CCCb(CCC_BROADCAST_ENEC, TxLen, TxBuf, I3C_TRANSFER_SPEED_SDR_1MHZ,
		TIMEOUT_TYPICAL, I3C_Master_Callback, pDevice->port, policy, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast DISEC task
 * @param [in]      pBus            bus object
 * @param [in]      mask            disable setting
 * @param [in]      policy          task insertion policy
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_DISEC(I3C_BUS_INFO_t *pBus, __u8 mask, I3C_TASK_POLICY_Enum policy)
{
	I3C_DEVICE_INFO_t *pDevice;
	__u16 TxLen;
	__u8 TxBuf[1];

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	if (mask == 0) {
		return;
	}


	pDevice = pBus->pCurrentMaster;
	TxLen = 1;
	TxBuf[0] = mask;
	I3C_Master_Insert_Task_CCCb(CCC_BROADCAST_DISEC, TxLen, TxBuf, I3C_TRANSFER_SPEED_SDR_1MHZ,
		TIMEOUT_TYPICAL, I3C_Master_Callback, pDevice->port, policy, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast RSTDAA task
 * @param [in]      pBus            bus object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_RSTDAA(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqRSTDAA) &&
			(pDev->attr.b.doneRSTDAA == 0)) {
			I3C_Master_Insert_Task_CCCb(CCC_BROADCAST_RSTDAA, 0, NULL,
				I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL, I3C_Master_Callback,
				pDevice->port, I3C_TASK_POLICY_APPEND_LAST, NOT_HIF);
			return;
		}

		pDev = pDev->pNextDev;
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast ENTDAA task
 * @param [in]      pBus            bus object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_ENTDAA(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_t *pDevice;

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	I3C_Master_Insert_Task_ENTDAA(63, NULL, I3C_TRANSFER_SPEED_I2C_1MHZ, TIMEOUT_TYPICAL,
		I3C_Master_Callback, pDevice->port, I3C_TASK_POLICY_APPEND_LAST, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast SETAASA task
 * @param [in]      pBus            bus object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_SETAASA(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	_Bool bNeedSETAASA;

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	pDev = pBus->pDevList;

	bNeedSETAASA = FALSE;
	while ((pDev != NULL) && (bNeedSETAASA == FALSE)) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETAASA) &&
			(pDev->attr.b.doneSETAASA == 0)) {
			bNeedSETAASA = TRUE;
		}

		pDev = pDev->pNextDev;
	}

	if (bNeedSETAASA) {
		I3C_Master_Insert_Task_CCCb(CCC_BROADCAST_SETAASA, 0, NULL,
			I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL, I3C_Master_Callback,
			pDevice->port, I3C_TASK_POLICY_APPEND_LAST, NOT_HIF);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create brocast SETHID task
 * @param [in]      pBus            bus object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCb_SETHID(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;

	if (pBus == NULL) {
		return;
	}

	pDev = pBus->pDevList;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETHID) &&
			(pDev->attr.b.doneSETHID == 0)) {
		}

		pDev = pDev->pNextDev;
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create direct ENEC task
 * @param [in]      pBus            bus object
 * @param [in]      addr            target device
 * @param [in]      mask            enable setting
 * @param [in]      policy          task insertion policy
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCw_ENEC(I3C_BUS_INFO_t *pBus, __u8 addr, __u8 mask, I3C_TASK_POLICY_Enum policy)
{
	I3C_DEVICE_INFO_t *pDevice;
	__u16 TxLen;
	__u8 TxBuf[2];

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;

	TxLen = 2;
	TxBuf[0] = addr;
	TxBuf[1] = mask;
	I3C_Master_Insert_Task_CCCw(CCC_DIRECT_ENEC, 1, TxLen, TxBuf, I3C_TRANSFER_SPEED_SDR_1MHZ,
		TIMEOUT_TYPICAL, I3C_Master_Callback, pDevice->port, policy, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create direct DISEC task
 * @param [in]      pBus            bus object
 * @param [in]      addr            target device
 * @param [in]      mask            disable setting
 * @param [in]      policy          task insertion policy
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCw_DISEC(I3C_BUS_INFO_t *pBus, __u8 addr, __u8 mask, I3C_TASK_POLICY_Enum policy)
{
	I3C_DEVICE_INFO_t *pDevice;
	__u16 TxLen;
	__u8 TxBuf[2];

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;

	TxLen = 2;
	TxBuf[0] = addr;
	TxBuf[1] = mask;
	I3C_Master_Insert_Task_CCCw(CCC_DIRECT_DISEC, 1, TxLen, TxBuf, I3C_TRANSFER_SPEED_SDR_1MHZ,
		TIMEOUT_TYPICAL, I3C_Master_Callback, pDevice->port, policy, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create direct SETDASA task
 * @param [in]      pBus            bus object
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCw_SETDASA(I3C_BUS_INFO_t *pBus)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u8 devCnt;
	__u8 TxBuf[62];
	__u16 TxLen;

	if (pBus == NULL) {
		return;
	}

	if (pBus->pCurrentMaster == NULL) {
		return;
	}

	pDevice = pBus->pCurrentMaster;
	pDev = pBus->pDevList;

	devCnt = 0;
	while (pDev != NULL) {
		if ((pDev->attr.b.present) && (pDev->attr.b.reqSETDASA) &&
			(pDev->attr.b.doneSETDASA == 0)) {
			TxBuf[devCnt * 2 + 0] = pDev->staticAddr;
			TxBuf[devCnt * 2 + 1] = (pDev->staticAddr << 1);
			devCnt++;

			if (devCnt >= 31) {
				break;
			}
		}

		pDev = pDev->pNextDev;
	}

	if (devCnt) {
		TxLen = 2 * devCnt;
		I3C_Master_Insert_Task_CCCw(CCC_DIRECT_SETDASA, 1, TxLen, TxBuf,
			I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL, I3C_Master_Callback,
			pDevice->port, I3C_TASK_POLICY_APPEND_LAST, NOT_HIF);
	}
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create direct SETNEWDA task
 * @param [in]      pBus            bus object
 * @param [in]      dyn_addr_old    old address
 * @param [in]      dyn_addr_new    new address
 * @return                          none
 */
/*------------------------------------------------------------------------------*/
void I3C_CCCw_SETNEWDA(I3C_BUS_INFO_t *pBus, __u8 dyn_addr_old, __u8 dyn_addr_new)
{
	I3C_DEVICE_INFO_SHORT_t *pDev;
	__u16 TxLen;
	__u8 TxBuf[2];

	if (pBus == NULL) {
		return;
	}

	pDev = pBus->pDevList;
	while (1) {
		if (pDev == NULL) {
			return;
		}

		if (pDev->dynamicAddr == dyn_addr_old) {
			break;
		}

		pDev = pDev->pNextDev;
	}

	TxLen = 2;
	TxBuf[0] = dyn_addr_old;
	TxBuf[1] = dyn_addr_new << 1;
	I3C_Master_Insert_Task_CCCw(CCC_DIRECT_SETNEWDA, 1, TxLen, TxBuf,
		I3C_TRANSFER_SPEED_SDR_1MHZ, TIMEOUT_TYPICAL, I3C_Master_Callback,
		pBus->pCurrentMaster->port, I3C_TASK_POLICY_APPEND_LAST, NOT_HIF);
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Inform slave device while task is complete or end with error
 * @param [in]      pTaskInfo       Pointer to the running task
 * @return                          task result
 */
/*------------------------------------------------------------------------------*/
__u32 I3C_Notify(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_PROTOCOL_Enum protocol;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	__u32 ret;
	I3C_DEVICE_INFO_SHORT_t *pDev;
	I3C_TRANSFER_FRAME_t *pThisFrame;

	__u8 i;

	if (pTaskInfo == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pTask = pTaskInfo->pTask;
	protocol = pTask->protocol;
	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];
	pBus = pDevice->pOwner;
	ret = I3C_ERR_OK;

	/* Find slave to be notified */
	/* IBINACKed by master, only internal device */
	if (pTaskInfo->result == I3C_ERR_NACK_SLVSTART) {
		pDev = GetDevInfoByTask(pBus, pTask);
		pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;
		if (pDevice->callback != NULL) {
			ret = pDevice->callback((__u32)pTaskInfo, pTaskInfo->result);
			return ret;
		}
	}

	if (protocol == I3C_TRANSFER_PROTOCOL_HOT_JOIN) {
		return pTaskInfo->result;
	}

	if (protocol == I3C_TRANSFER_PROTOCOL_IBI) {
		/* IBIACKed by master */
		pDev = GetDevInfoByTask(pBus, pTask);
		pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;
		if (pDevice->callback != NULL) {
			ret = pDevice->callback((__u32)pTaskInfo, pTaskInfo->result);
			return ret;
		}
	}

	if (protocol == I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) {
		/* 1. IBINACKed by master ? */
		/* 2. slave request with higher priority ? */
		pDev = GetDevInfoByTask(pBus, pTask);
		pDevice = (I3C_DEVICE_INFO_t *)pDev->pDeviceInfo;
		if (pDevice->callback != NULL) {
			ret = pDevice->callback((__u32)pTaskInfo, pTaskInfo->result);
			return ret;
		}
	}

	/* Master Task */
	for (i = pTask->frame_idx; i < pTask->frame_count; i++) {
		pThisFrame = &pTask->pFrameList[i];

		if ((pTask->protocol == I3C_TRANSFER_PROTOCOL_CCCb) && (i == 0)) {
			/*
			 * SLVSTART: CCCb
			 * SETHID / SETAASA / DEVCTRL
			 */
			continue;
		}

		if (pThisFrame->address == I3C_BROADCAST_ADDR) {
			continue;
		}

		/* specific address */
		if (pThisFrame->type == I3C_TRANSFER_TYPE_I2C) {
			pDev = GetDevInfoByStaticAddr(pBus, pThisFrame->address);
		} else {
			pDev = GetDevInfoByDynamicAddr(pBus, pThisFrame->address);
		}

		if (pDev == NULL) {
			continue;
		}
	}

	return ret;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create master task
 * @param[in]       Protocol        Traffic format
 * @param[in]       Addr            specify target address for non-CCC traffic,
 *                                  For CCCb, it describes the data length including
 *                                  CCC + Write data + [PEC]
 *                                  For CCCw, it describes the data length including
 *                                  Target Address + Write data + [PEC], for each target
 *                                  For CCCr, it describes the data length including
 *                                  Target Address + read data + [PEC], for each target
 * @param[in]       HSize           Used to specify the data length of the first frame
 * @param[in]       pWrCnt          Used to specify write data length
 * @param[in]       pRdCnt          Used to specify read data length
 * @param[in]       WrBuf           Used to specify write data buffer
 * @param[in]       RdBuf           Used to specify read data buffer
 * @param[in]       Baudrate        Used to specify baudrate
 * @param[in]       Timeout         Used to specify timeout (unit: 10ms)
 * @param[in]       callback        Used to handle task result
 * @param[in]       PortId          perform task in which i3c port
 * @param[in]       Policy          task insert policy, insert in the head for the urgent
 *                                  case (event), tail for normal case
 * @param[in]       bHIF            0b, memory allocated in the callee, free data buffer
 *                                  if task is finished
 *                                  1b, memory allocated in the caller, should not free
 *                                  data buffer even if task is finished
 * @return                          allocated task info if it is not NULL
 */
/*------------------------------------------------------------------------------*/
I3C_TASK_INFO_t *I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol,
	__u8 Addr, __u8 HSize, __u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback,
	__u8 PortId, I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TASK_INFO_t *pTaskInfo;

	if (PortId >= I3C_PORT_MAX) {
		return NULL;
	}

	pDevice = &gI3c_dev_node_internal[PortId];
	if (pDevice->task_count >= I3C_TASK_MAX) {
		return NULL;
	}

	pTaskInfo = (I3C_TASK_INFO_t *)NewTaskInfo();
	if (pTaskInfo == NULL) {
		return NULL;
	}

	InitTaskInfo(pTaskInfo);

	if (ValidateProtocol(Protocol) != I3C_ERR_OK) {
		hal_I3C_MemFree(pTaskInfo);
		return NULL;
	}

	if (ValidateBaudrate(Protocol, Baudrate) != I3C_ERR_OK) {
		hal_I3C_MemFree(pTaskInfo);
		return NULL;
	}

	if (ValidateBuffer(Protocol, Addr, HSize, *pWrCnt, *pRdCnt, WrBuf, RdBuf, bHIF) !=
		I3C_ERR_OK) {
		hal_I3C_MemFree(pTaskInfo);
		return NULL;
	}

	if (CreateTaskNode(pTaskInfo, Protocol, Baudrate, Addr, HSize, pWrCnt, WrBuf, pRdCnt,
		RdBuf, callback, bHIF) != I3C_ERR_OK) {
		hal_I3C_MemFree(pTaskInfo);
		return NULL;
	}

	pTaskInfo->MasterRequest = TRUE;
	pTaskInfo->Port = PortId;
	pTaskInfo->u32SwTimeout = Timeout;

	/* Later, I3C Task Engine will pick it from device's task list */
	InsertTaskNode(pDevice, pTaskInfo->pTask, Policy);

	pDevice->task_count++;
	pTaskInfo->result = I3C_ERR_PENDING;
	return pTaskInfo;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create slave task
 * @param[in]       Protocol        Traffic format
 * @param[in]       Addr            specify data length return for IBI,
 * @param[in]       pWrCnt          Used to specify write data length
 * @param[in]       pRdCnt          Used to specify read data length
 * @param[in]       WrBuf           Used to specify write data buffer
 * @param[in]       RdBuf           Used to specify read data buffer
 * @param[in]       Timeout         Used to specify timeout (unit: 10ms)
 * @param[in]       callback        Used to handle task result
 * @param[in]       PortId          perform task in which i3c port
 * @param[in]       bHIF            0b, memory allocated in the callee,
 *                                  free data buffer if task is finished
 *                                  1b, memory allocated in the caller,
 *                                  should not free data buffer even if task is finished
 * @return                          allocated task info if it is not NULL
 */
/*------------------------------------------------------------------------------*/
I3C_TASK_INFO_t *I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_Enum Protocol,
	__u8 Addr, __u16 *pWrCnt, __u16 *pRdCnt, __u8 *WrBuf, __u8 *RdBuf,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, _Bool bHIF)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TASK_INFO_t *pTaskInfo;

	if (PortId >= I3C_PORT_MAX) {
		return NULL;
	}

	pDevice = &gI3c_dev_node_internal[PortId];
	if (pDevice->task_count > I3C_TASK_MAX) {
		return NULL;
	}

	pTaskInfo = NewTaskInfo();
	if (pTaskInfo == NULL) {
		return NULL;
	}

	InitTaskInfo(pTaskInfo);

	if ((ValidateProtocol(Protocol) != I3C_ERR_OK) ||
		(ValidateBuffer(Protocol, Addr, 0, *pWrCnt, *pRdCnt, WrBuf, RdBuf, bHIF) !=
			I3C_ERR_OK) ||
		(CreateTaskNode(pTaskInfo, Protocol, pDevice->baudrate.i3cSdr, Addr, 0, pWrCnt,
			WrBuf, 0, NULL, callback, bHIF) != I3C_ERR_OK)) {
		hal_I3C_MemFree(pTaskInfo);
		return NULL;
	}

	pTaskInfo->MasterRequest = FALSE;
	pTaskInfo->Port = PortId;
	pTaskInfo->u32SwTimeout = Timeout;

	InsertTaskNode(pDevice, pTaskInfo->pTask, I3C_TASK_POLICY_APPEND_LAST);

	pDevice->task_count++;
	pTaskInfo->result = I3C_ERR_PENDING;
	return pTaskInfo;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Remove task from device's task list
 * @param [in]      pTaskInfo       Specify removed task info
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void I3C_Complete_Task(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_TASK_t *pParentTask;

	if (pTaskInfo == NULL) {
		return;
	}

	if (pTaskInfo->pTask == NULL) {
		return;
	}

	pTask = pTaskInfo->pTask;
	pDevice = &gI3c_dev_node_internal[pTaskInfo->Port];

	if (pDevice->pTaskListHead == NULL) {
		return;
	}

	if (pTask == pDevice->pTaskListHead) {
		pDevice->pTaskListHead = pTask->pNextTask;
	} else {
		pParentTask = pDevice->pTaskListHead;
		while (pParentTask != NULL) {
			if (pParentTask->pNextTask == pTask) {
				break;
			}

			pParentTask = pParentTask->pNextTask;
		}

		if ((pParentTask == NULL) || (pParentTask->pNextTask != pTask)) {
			return;
		}

		pParentTask->pNextTask = pTask->pNextTask;
	}

	pDevice->task_count--;
	FreeTaskNode(pTask);

	hal_I3C_MemFree(pTaskInfo);
	pTaskInfo = NULL;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Free I3C tasks for a specific slave device
 * @param [in]      pDevice         Pointer to the slave device
 * @return                          TRUE
 */
/*------------------------------------------------------------------------------*/
_Bool I3C_Clean_Slave_Task(I3C_DEVICE_INFO_t *pDevice)
{
	if (pDevice == NULL) {
		return TRUE;
	}

	while (pDevice->pTaskListHead != NULL) {
		I3C_Complete_Task(pDevice->pTaskListHead->pTaskInfo);
	}

	return TRUE;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Allocate memory to save task info
 * @return                          pointer to the new task info
 */
/*------------------------------------------------------------------------------*/
void *NewTaskInfo(void)
{
	I3C_TASK_INFO_t *pNewTaskInfo;

	pNewTaskInfo = (I3C_TASK_INFO_t *)hal_I3C_MemAlloc(sizeof(I3C_TASK_INFO_t));
	return pNewTaskInfo;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Set init value for task info
 * @param [in]      pTaskInfo       pointer to the task info
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void InitTaskInfo(I3C_TASK_INFO_t *pTaskInfo)
{
	pTaskInfo->pTask = NULL;
	pTaskInfo->result = I3C_ERR_OK;
	pTaskInfo->MasterRequest = FALSE;
	pTaskInfo->bHIF = FALSE;
	pTaskInfo->Port = 0xFF;
	pTaskInfo->u32SwTimeout = TIMEOUT_TYPICAL;
	pTaskInfo->callback = NULL;
	pTaskInfo->pParentTaskInfo = NULL;

	pTaskInfo->idx = 0;
	pTaskInfo->fmt = 0;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Free mem for a specific task
 * @param [in]      pTask           Pointer to the task
 * @return                          None
 */
/*------------------------------------------------------------------------------*/
void FreeTaskNode(I3C_TRANSFER_TASK_t *pTask)
{
	I3C_TASK_INFO_t *pTaskInfo;

	if (pTask == NULL) {
		return;
	}

	pTaskInfo = pTask->pTaskInfo;
	if ((pTaskInfo != NULL) && (pTaskInfo->bHIF == FALSE)) {
		if (pTask->pRdLen != NULL) {
			hal_I3C_MemFree(pTask->pRdLen);
			pTask->pRdLen = NULL;
		}

		if (pTask->pRdBuf != NULL) {
			hal_I3C_MemFree(pTask->pRdBuf);
			pTask->pRdBuf = NULL;
		}
	}

	if (pTask->pWrLen != NULL) {
		hal_I3C_MemFree(pTask->pWrLen);
		pTask->pWrLen = NULL;
	}

	if (pTask->pWrBuf != NULL) {
		hal_I3C_MemFree(pTask->pWrBuf);
		pTask->pWrBuf = NULL;
	}

	if (pTask->pFrameList != NULL) {
		hal_I3C_MemFree(pTask->pFrameList);
		pTask->pFrameList = NULL;
	}

	hal_I3C_MemFree(pTask);
	pTask = NULL;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Validate for support protocol
 * @param [in]      Protocol        Protocol
 * @return                          I3C_ERR_OK, if the specified protocol is support
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum ValidateProtocol(I3C_TRANSFER_PROTOCOL_Enum Protocol)
{
	/* slave task */
	if (Protocol == I3C_TRANSFER_PROTOCOL_IBI) {
		return I3C_ERR_OK;
	}

	if (Protocol == I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) {
		return I3C_ERR_OK;
	}

	if (Protocol == I3C_TRANSFER_PROTOCOL_HOT_JOIN) {
		return I3C_ERR_OK;
	}

	/* master task */
	if ((Protocol >= I3C_TRANSFER_PROTOCOL_I2C_WRITE) &&
		(Protocol <= I3C_TRANSFER_PROTOCOL_I2C_WRITEnREAD)) {
		return I3C_ERR_OK;
	}

	if ((Protocol >= I3C_TRANSFER_PROTOCOL_I3C_WRITE) &&
		(Protocol <= I3C_TRANSFER_PROTOCOL_I3C_W7EnREAD)) {
		return I3C_ERR_OK;
	}

	if ((Protocol >= I3C_TRANSFER_PROTOCOL_DDR_WRITE) &&
		(Protocol <= I3C_TRANSFER_PROTOCOL_DDR_READ)) {
		return I3C_ERR_OK;
	}

	if ((Protocol >= I3C_TRANSFER_PROTOCOL_CCCb) &&
		(Protocol <= I3C_TRANSFER_PROTOCOL_CCCr)) {
		return I3C_ERR_OK;
	}

	if (Protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		return I3C_ERR_OK;
	}

	if (Protocol == I3C_TRANSFER_PROTOCOL_EVENT) {
		return I3C_ERR_OK;
	}

	return I3C_ERR_PARAMETER_INVALID;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Validate baudrate setting
 * @param [in]      Protocol        Protocol
 * @param [in]      BaudRate        Baudrate
 * @return                          I3C_ERR_OK, if the specified baudrate is good
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum ValidateBaudrate(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u32 BaudRate)
{
	if (EVENT_TRANSFER_PROTOCOL(Protocol)) {
		return I3C_ERR_OK;
	} else if (I2C_TRANSFER_PROTOCOL(Protocol)) {
		if (BaudRate == I3C_TRANSFER_SPEED_I2C_1MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_I2C_400KHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_I2C_100KHZ) {
			return I3C_ERR_OK;
		}
	} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
		if (BaudRate == I3C_TRANSFER_SPEED_SDR_12p5MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_8MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_6MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_4MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_2MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_1MHZ) {
			return I3C_ERR_OK;
		}
	} else {
		if (BaudRate == I3C_TRANSFER_SPEED_I2C_1MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_I2C_400KHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_12p5MHZ)	{
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_8MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_6MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_4MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_2MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_1MHZ) {
			return I3C_ERR_OK;
		}

		if (BaudRate == I3C_TRANSFER_SPEED_SDR_IBI)	{
			return I3C_ERR_OK;
		}
	}

	return I3C_ERR_PARAMETER_INVALID;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Validate buffer
 * @param [in]      Protocol        Protocol
 * @param [in]      Address         Specify target address or data length for CCC
 * @param [in]      HSize           Specify the data length for the first frame
 * @param [in]      WrCnt           Specify the data length for the write buffer
 * @param [in]      RdCnt           Specify the data length for the read buffer
 * @param [in]      WrBuf           Specify the data buffer for the write buffer
 * @param [in]      RdBuf           Specify the data buffer for the read buffer
 * @param [in]      bHIF            task create via HIF ?
 * @return                          I3C_ERR_OK, if the specified buffer and buffer count are good
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum ValidateBuffer(I3C_TRANSFER_PROTOCOL_Enum Protocol, __u8 Address,
	__u8 HSize, __u16 WrCnt, __u16 RdCnt, __u8 *WrBuf, __u8 *RdBuf, _Bool bHIF)
{
	if (DDR_TRANSFER_PROTOCOL(Protocol)) {
		if (READ_TRANSFER_PROTOCOL(Protocol)) {
			if (WrCnt < 1) {
				return I3C_ERR_PARAMETER_INVALID;	/* DDR_CMD */
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if ((WrBuf[0] & 0x80) == 0)	{
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (RdCnt == 0) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		} else {
			if (WrCnt < 2) {
				return I3C_ERR_PARAMETER_INVALID;	/* DDR_CMD + DDR_DATA */
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (WrBuf[0] & 0x80) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		}
	} else if (CCC_TRANSFER_PROTOCOL(Protocol)) {
		if (Protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			if (WrCnt < 1) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (WrBuf[0] != CCC_BROADCAST_ENTDAA) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			/* at least 9 bytes for 1 slave's info */
			if (RdCnt < 9) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCr) {
			if (WrCnt < 2) {
				return I3C_ERR_PARAMETER_INVALID;	/* CCC + 1 target address */
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (CCC_BROADCAST(WrBuf[0])) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCw) {
			if (WrCnt < 2) {
				return I3C_ERR_PARAMETER_INVALID; /* CCC + 1 target address */
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (CCC_BROADCAST(WrBuf[0])) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (((WrCnt - HSize) % Address) != 0) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCb) {
			if (WrCnt < 1) {
				return I3C_ERR_PARAMETER_INVALID;	/* CCC */
			}

			if (WrBuf == NULL) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (!CCC_BROADCAST(WrBuf[0])) {
				return I3C_ERR_PARAMETER_INVALID;
			}

			if (WrCnt != HSize) {
				return I3C_ERR_PARAMETER_INVALID;
			}
		}
	} else if (Protocol == I3C_TRANSFER_PROTOCOL_I2C_WRITE) {
		if ((WrCnt < 1) || (WrBuf == NULL)) {
			return I3C_ERR_PARAMETER_INVALID;
		}
	} else if (Protocol == I3C_TRANSFER_PROTOCOL_I2C_READ) {
		/* Although RdCnt could be 0 before message transfer,
		 * master should allocate enough space to restore the receiving data
		 */
		if (RdCnt == 0) {
			return I3C_ERR_PARAMETER_INVALID;
		}
	} else if (Protocol == I3C_TRANSFER_PROTOCOL_I2C_WRITEnREAD) {
		if ((WrCnt < 1) || (WrBuf == NULL) || (RdCnt == 0))	{
			return I3C_ERR_PARAMETER_INVALID;
		}
	} else if ((Protocol == I3C_TRANSFER_PROTOCOL_I3C_WRITE) ||
			   (Protocol == I3C_TRANSFER_PROTOCOL_I3C_W7E)) {
		if ((WrCnt < 1) || (WrBuf == NULL)) {
			return I3C_ERR_PARAMETER_INVALID;
		}
	} else if ((Protocol == I3C_TRANSFER_PROTOCOL_I3C_READ) ||
			   (Protocol == I3C_TRANSFER_PROTOCOL_I3C_R7E)) {
		if (RdCnt == 0) {
			return I3C_ERR_PARAMETER_INVALID;
		}
	} else if ((Protocol == I3C_TRANSFER_PROTOCOL_I3C_WRITEnREAD) ||
			   (Protocol == I3C_TRANSFER_PROTOCOL_I3C_W7EnREAD)) {
		if ((WrCnt < 1) || (WrBuf == NULL) || (RdCnt == 0))	{
			return I3C_ERR_PARAMETER_INVALID;
		}
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create Task
 * @param [in]      pTaskInfo       Specify task info
 * @param [in]      Protocol        Protocol for a specific task
 * @param [in]      Baudrate        Baudrate setting for a specific task
 * @param [in]      Addr            target address
 * @param [in]      HSize           Specify the data length for the first frame
 * @param [in]      pWrCnt          Specify the data length for the write buffer
 * @param [in]      WrBuf           Specify the write buffer
 * @param [in]      pRdCnt          Specify the data length for the read buffer
 * @param [in]      RdBuf           Specify the read buffer
 * @param [in]      callback        Specify the callback function
 * @param [in]      bHIF            data buffer will be free automatically ?
 * @return                          I3C_ERR_OK, if the task create successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum CreateTaskNode(I3C_TASK_INFO_t *pTaskInfo,
	I3C_TRANSFER_PROTOCOL_Enum Protocol, __u32 Baudrate, __u8 Addr, __u8 HSize,
	__u16 *pWrCnt, __u8 *WrBuf, __u16 *pRdCnt, __u8 *RdBuf, ptrI3C_RetFunc callback, _Bool bHIF)
{
	__u16 i;
	__u8 *pTxBuf = NULL;
	__u8 *pRxBuf = NULL;
	I3C_TRANSFER_FRAME_t *pFrameNode = NULL;
	I3C_TRANSFER_TASK_t *pNewTask;

	/* build task and frame in according to input parameters */
	pNewTask = (I3C_TRANSFER_TASK_t *)hal_I3C_MemAlloc(sizeof(I3C_TRANSFER_TASK_t));
	if (pNewTask == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if (bHIF) {
		/* do not free rx data buffer */
		pRxBuf = RdBuf;
		pNewTask->pRdBuf = RdBuf;
		pNewTask->pRdLen = pRdCnt;
	} else {
		pNewTask->pRdLen = NULL;
		pNewTask->pRdBuf = NULL;

		if ((pRdCnt != NULL) && (*pRdCnt != 0)) {
			pNewTask->pRdLen = (__u16 *)hal_I3C_MemAlloc(1);
			pRxBuf = (__u8 *)hal_I3C_MemAlloc(*pRdCnt);

			if ((pNewTask->pRdLen == NULL) || (pRxBuf == NULL)) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pNewTask->pRdBuf = pRxBuf;
			*pNewTask->pRdLen = *pRdCnt;
		}
	}

	/* we need to modify write buffer and write len, so we should always allocate memory */
	pNewTask->pWrBuf = NULL;
	pNewTask->pWrLen = NULL;

	if ((pWrCnt != NULL) && (*pWrCnt != 0) && (WrBuf != NULL)) {
		pNewTask->pWrLen = (__u16 *)hal_I3C_MemAlloc(1);
		pTxBuf = (__u8 *)hal_I3C_MemAlloc(*pWrCnt);

		if ((pNewTask->pWrLen == NULL) || (pTxBuf == NULL)) {
			FreeTaskNode(pNewTask);
			return I3C_ERR_OUT_OF_MEMORY;
		}

		pNewTask->pWrBuf = pTxBuf;
		memcpy(pTxBuf, WrBuf, *pWrCnt);
		*pNewTask->pWrLen = *pWrCnt;
	}

	/* setup task content */
	/* retry used in i3c for slave power down, i3c read and interrupted by slave start */
	pNewTask->pFrameList = NULL;
	pNewTask->frame_count = 0;

	if (CCC_TRANSFER_PROTOCOL(Protocol)) {
		if (Protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			/* PID[6] + BCR[1] + DCR[1] + ADDR[1] */
			pNewTask->frame_count = 1 + (*pRdCnt / 9);
			pFrameNode = (I3C_TRANSFER_FRAME_t *) hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			i = 0;
			while (1) {
				pNewTask->frame_idx = 1 + i;
				if (pNewTask->frame_idx == pNewTask->frame_count) {
					break;
				}

				/* Stop automatically for slave NAK or master stop */
				pFrameNode[pNewTask->frame_idx].flag = I3C_TRANSFER_NORMAL |
					I3C_TRANSFER_REPEAT_START;
				if (pNewTask->frame_idx == (pNewTask->frame_count - 1)) {
					pFrameNode[pNewTask->frame_idx].pNextFrame = NULL;
				} else {
					pFrameNode[pNewTask->frame_idx].pNextFrame =
						&pFrameNode[pNewTask->frame_idx + 1];
				}

				pFrameNode[pNewTask->frame_idx].pOwner = pNewTask;
				pFrameNode[pNewTask->frame_idx].type = I3C_TRANSFER_TYPE_SDR;
				pFrameNode[pNewTask->frame_idx].baudrate = Baudrate;
				pFrameNode[pNewTask->frame_idx].address = I3C_BROADCAST_ADDR;
				pFrameNode[pNewTask->frame_idx].direction = I3C_TRANSFER_DIR_WRITE;
				pFrameNode[pNewTask->frame_idx].access_len = 8;
				pFrameNode[pNewTask->frame_idx].access_idx = 0;
				pFrameNode[pNewTask->frame_idx].access_buf = &pRxBuf[i * 9];

				/* if slave nak or no slave ack, master should terminate ENTDAA */
				pFrameNode[pNewTask->frame_idx].retry_count = 0;

				i++;
			}
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCb) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = HSize;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCw) {
			pNewTask->frame_count = 1 + ((*pWrCnt - HSize) / Addr);
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE |
				I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = HSize;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			i = 0;
			while (1) {
				pNewTask->frame_idx = 1 + i;
				if (pNewTask->frame_idx == pNewTask->frame_count) {
					break;
				}

				if (pNewTask->frame_idx == (pNewTask->frame_count - 1)) {
					pFrameNode[pNewTask->frame_idx].flag = I3C_TRANSFER_NORMAL |
						I3C_TRANSFER_REPEAT_START;
					pFrameNode[pNewTask->frame_idx].pNextFrame = NULL;
				} else {
					pFrameNode[pNewTask->frame_idx].flag = I3C_TRANSFER_NORMAL |
						I3C_TRANSFER_REPEAT_START | I3C_TRANSFER_NO_STOP;
					pFrameNode[pNewTask->frame_idx].pNextFrame =
						&pFrameNode[pNewTask->frame_idx + 1];
				}

				pFrameNode[pNewTask->frame_idx].pOwner = pNewTask;
				pFrameNode[pNewTask->frame_idx].type = I3C_TRANSFER_TYPE_SDR;
				pFrameNode[pNewTask->frame_idx].baudrate = Baudrate;

				/* addr = WrBuf[HSize + i * Format] */
				pFrameNode[pNewTask->frame_idx].address = pTxBuf[HSize + i * Addr];
				pFrameNode[pNewTask->frame_idx].direction = I3C_TRANSFER_DIR_WRITE;
				pFrameNode[pNewTask->frame_idx].access_len = Addr - 1;
				pFrameNode[pNewTask->frame_idx].access_idx = 0;

				if (Addr == 1) {
					/* address only, no optional write data */
					pFrameNode[pNewTask->frame_idx].access_buf = NULL;
				} else {
					pFrameNode[pNewTask->frame_idx].access_buf =
						&pTxBuf[HSize + i * Addr + 1];
				}

				/* only retry at the first frame */
				pFrameNode[pNewTask->frame_idx].retry_count = 0;

				i++;
			}
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_CCCr) {
			pNewTask->frame_count = (*pWrCnt - HSize) + 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE |
				I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = HSize;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;	/* CCC */
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			i = 0;
			while (1) {
				pNewTask->frame_idx = 1 + i;
				if (pNewTask->frame_idx == pNewTask->frame_count) {
					break;
				}

				if (pNewTask->frame_idx == (pNewTask->frame_count - 1)) {
					pFrameNode[pNewTask->frame_idx].flag = I3C_TRANSFER_NORMAL |
						I3C_TRANSFER_REPEAT_START |
						I3C_TRANSFER_RETRY_ENABLE |
						I3C_TRANSFER_RETRY_WITHOUT_STOP;
					pFrameNode[pNewTask->frame_idx].pNextFrame = NULL;
				} else {
					pFrameNode[pNewTask->frame_idx].flag = I3C_TRANSFER_NORMAL |
						I3C_TRANSFER_REPEAT_START |
						I3C_TRANSFER_RETRY_ENABLE |
						I3C_TRANSFER_RETRY_WITHOUT_STOP |
						I3C_TRANSFER_NO_STOP;
					pFrameNode[pNewTask->frame_idx].pNextFrame =
						&pFrameNode[pNewTask->frame_idx + 1];
				}

				pFrameNode[pNewTask->frame_idx].pOwner = pNewTask;
				pFrameNode[pNewTask->frame_idx].type = I3C_TRANSFER_TYPE_SDR;
				pFrameNode[pNewTask->frame_idx].baudrate = Baudrate;
				pFrameNode[pNewTask->frame_idx].address = pTxBuf[HSize + i];
				pFrameNode[pNewTask->frame_idx].direction = I3C_TRANSFER_DIR_READ;

				/* Format = Read Data Length per CCC */
				pFrameNode[pNewTask->frame_idx].access_len = Addr;
				pFrameNode[pNewTask->frame_idx].access_idx = 0;
				pFrameNode[pNewTask->frame_idx].access_buf = &pRxBuf[i * Addr];
				pFrameNode[pNewTask->frame_idx].retry_count = 1;

				i++;
			}
		}
	} else if (EVENT_TRANSFER_PROTOCOL(Protocol)) {
		if (Protocol == I3C_TRANSFER_PROTOCOL_EVENT) {
			/* For IBI, we should read back MDB + Extended IBI payload.
			 * For MCTP, we should read back MDB and stop. Then fork a new read transfer
			 * to read back response data.
			 *
			 * For Hot Join, we should accept the request and start ENTDAA after STOP.
			 * Must STOP to notify slave finish hot-join task.
			 * Can't use RESTART + ENTDAA, that will cause MERRWARN.INVREQ
			 *
			 * For Master Request, we should accept the request and
			 * start GETACCMST with RESTART
			 */

			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			/* we don't identify IBI type now, and
			 * HJ can interrupt IBI and MR, IBI can interrupt MR
			 * when master doesn't want to ack slave request,
			 * we need to sent disec with RESTART
			 */
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;

			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[0].access_len = *pRdCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pRxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_IBI) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = (Addr == 0) ? 0 : *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = (Addr == 0) ? NULL : pTxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (Protocol == I3C_TRANSFER_PROTOCOL_HOT_JOIN) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);
			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		}
	} else if (WRITE_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);
			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (DDR_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_DDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].hdrcmd = pTxBuf[0];
			pFrameNode[0].access_len = *pWrCnt - 1;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = &pTxBuf[1];
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		}
	} else if (READ_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[0].access_len = *pRdCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pRxBuf;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[0].access_len = *pRdCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pRxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		} else if (DDR_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 1;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(sizeof(
				I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_DDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[0].hdrcmd = pTxBuf[0];
			pFrameNode[0].access_len = *pRdCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pRxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = NULL;
		}
	} else if (WRITEnREAD_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[1].access_len = *pRdCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pRxBuf;
			pFrameNode[1].retry_count = 3;
			pFrameNode[1].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = Addr;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = *pWrCnt;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = pTxBuf;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[1].access_len = *pRdCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pRxBuf;
			pFrameNode[1].retry_count = 3;
			pFrameNode[1].pNextFrame = NULL;
		}
	} else if (W7E_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[1].access_len = *pWrCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pTxBuf;
			pFrameNode[1].retry_count = 0;
			pFrameNode[1].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[1].access_len = *pWrCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pTxBuf;
			pFrameNode[1].retry_count = 0;
			pFrameNode[1].pNextFrame = NULL;
		}
	} else if (R7E_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);
			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[1].access_len = *pRdCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pRxBuf;
			pFrameNode[1].retry_count = 0;
			pFrameNode[1].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 2;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[1].access_len = *pRdCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pRxBuf;
			pFrameNode[1].retry_count = 3;
			pFrameNode[1].pNextFrame = NULL;
		}
	} else if (W7EnREAD_TRANSFER_PROTOCOL(Protocol)) {
		if (I2C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 3;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 0;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_NO_STOP;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[1].access_len = *pWrCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pTxBuf;
			pFrameNode[1].retry_count = 0;
			pFrameNode[1].pNextFrame = &pFrameNode[2];

			pFrameNode[2].pOwner = pNewTask;
			pFrameNode[2].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START;
			pFrameNode[2].type = I3C_TRANSFER_TYPE_I2C;
			pFrameNode[2].baudrate = Baudrate;
			pFrameNode[2].address = Addr;
			pFrameNode[2].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[2].access_len = *pRdCnt;
			pFrameNode[2].access_idx = 0;
			pFrameNode[2].access_buf = pRxBuf;
			pFrameNode[2].retry_count = 0;
			pFrameNode[2].pNextFrame = NULL;
		} else if (I3C_TRANSFER_PROTOCOL(Protocol)) {
			pNewTask->frame_count = 3;
			pFrameNode = (I3C_TRANSFER_FRAME_t *)hal_I3C_MemAlloc(
				sizeof(I3C_TRANSFER_FRAME_t) * pNewTask->frame_count);

			if (pFrameNode == NULL) {
				FreeTaskNode(pNewTask);
				return I3C_ERR_OUT_OF_MEMORY;
			}

			pFrameNode[0].pOwner = pNewTask;
			pFrameNode[0].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_NO_STOP |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[0].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[0].baudrate = Baudrate;
			pFrameNode[0].address = I3C_BROADCAST_ADDR;
			pFrameNode[0].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[0].access_len = 0;
			pFrameNode[0].access_idx = 0;
			pFrameNode[0].access_buf = NULL;
			pFrameNode[0].retry_count = 3;
			pFrameNode[0].pNextFrame = &pFrameNode[1];

			pFrameNode[1].pOwner = pNewTask;
			pFrameNode[1].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_NO_STOP | I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[1].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[1].baudrate = Baudrate;
			pFrameNode[1].address = Addr;
			pFrameNode[1].direction = I3C_TRANSFER_DIR_WRITE;
			pFrameNode[1].access_len = *pWrCnt;
			pFrameNode[1].access_idx = 0;
			pFrameNode[1].access_buf = pTxBuf;
			pFrameNode[1].retry_count = 3;
			pFrameNode[1].pNextFrame = &pFrameNode[2];

			pFrameNode[2].pOwner = pNewTask;
			pFrameNode[2].flag = I3C_TRANSFER_NORMAL | I3C_TRANSFER_REPEAT_START |
				I3C_TRANSFER_RETRY_ENABLE;
			pFrameNode[2].type = I3C_TRANSFER_TYPE_SDR;
			pFrameNode[2].baudrate = Baudrate;
			pFrameNode[2].address = Addr;
			pFrameNode[2].direction = I3C_TRANSFER_DIR_READ;
			pFrameNode[2].access_len = *pRdCnt;
			pFrameNode[2].access_idx = 0;
			pFrameNode[2].access_buf = pRxBuf;
			pFrameNode[2].retry_count = 3;
			pFrameNode[2].pNextFrame = NULL;
		}
	}

	if (pFrameNode == NULL) {
		FreeTaskNode(pNewTask);
		return I3C_ERR_PARAMETER_INVALID;
	}

	pNewTask->frame_idx = 0;
	pNewTask->pFrameList = pFrameNode;
	pNewTask->pTaskInfo = pTaskInfo;
	pNewTask->protocol = Protocol;
	pNewTask->address = Addr;
	pNewTask->baudrate = Baudrate;
	pNewTask->pNextTask = NULL;
	pTaskInfo->bHIF = bHIF;
	pTaskInfo->pTask = pNewTask;
	pTaskInfo->callback = callback;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Insert task into device's task list
 * @param [in]      pDevice         Specify device
 * @param [in]      pNewTask        Task to be inserted
 * @param [in]      bType           insert policy, 0: first, 1: last
 * @return                          I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum InsertTaskNode(I3C_DEVICE_INFO_t *pDevice,
	I3C_TRANSFER_TASK_t *pNewTask, __u8 bType)
{
	I3C_TRANSFER_TASK_t *pNextTask;
	I3C_TRANSFER_TASK_t *pThisTask;

	if (pDevice == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (pNewTask == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pThisTask = pDevice->pTaskListHead;
	if (pThisTask == NULL) {
		pDevice->pTaskListHead = pNewTask;
		return I3C_ERR_OK;
	}

	if (bType == 0) {
		pNewTask->pNextTask = pThisTask;
		pDevice->pTaskListHead = pNewTask;
		return I3C_ERR_OK;
	}

	if (bType == 1) {
		while (pThisTask->pNextTask != NULL) {
			pThisTask = pThisTask->pNextTask;
		}

		pThisTask->pNextTask = pNewTask;
		return I3C_ERR_OK;
	}

	if (pThisTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		pNewTask->pNextTask = pThisTask;
		pDevice->pTaskListHead = pNewTask;
		return I3C_ERR_OK;
	}

CHECK_NEXT:
	pNextTask = pThisTask->pNextTask;
	if (pNextTask == NULL) {
		pThisTask->pNextTask = pNewTask;
		return I3C_ERR_OK;
	}

	if (pNextTask->protocol != I3C_TRANSFER_PROTOCOL_EVENT) {
		pNewTask->pNextTask = pThisTask->pNextTask;
		pThisTask->pNextTask = pNewTask;
		return I3C_ERR_OK;
	}

	pThisTask = pNextTask;
	goto CHECK_NEXT;

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                           Create ENTDAA task into device's task list
 * @param [in]      rxbuf_size      receive buffer length
 * @param [in]      rxbuf           receive buffer to store participated slave device info
 * @param [in]      Baudrate        baudrate to generate ENTDAA
 * @param [in]      Timeout         task timeout value
 * @param [in]      callback        callback
 * @param [in]      PortId
 * @param [in]      Policy          task insertion policy
 * @param [in]      bHIF            bHIF = 1, the receive buffer will be free automatically
 *                                  if task is finished
 * @return                          I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Master_Insert_Task_ENTDAA(__u16 rxbuf_size, __u8 *rxbuf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	__u8 TxBuf[1];
	__u16 TxLen;
	__u16 RxLen;
	I3C_ErrCode_Enum result;

	if (rxbuf_size < 9) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if ((bHIF == IS_HIF) && (rxbuf == NULL)) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	TxBuf[0] = CCC_BROADCAST_ENTDAA;
	TxLen = 1;
	RxLen = (rxbuf_size / 9) * 9;
	pNewTaskInfo = I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_ENTDAA, 0, TxLen,
		&TxLen, &RxLen, TxBuf, rxbuf, Baudrate, Timeout, I3C_Master_Callback, PortId,
		Policy, bHIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) && (pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                Create CCCb type task into device's task list
 * @param[in]  CCC       CCC
 * @param[in]  buf_size  write data length, not including CCC
 * @param[in]  buf       write data buffer, not including CCC
 * @param [in] Baudrate  baudrate to generate CCCb task
 * @param [in] Timeout   task timeout value
 * @param [in] callback  callback
 * @param [in] PortId
 * @param [in] Policy    task insertion policy
 * @param [in] bHIF      bHIF = 1, the receive buffer will be free automatically
 *                       if task is finished
 * @return               I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCb(__u8 CCC, __u16 buf_size, __u8 *buf,
	__u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	__u8 TxBuf[I3C_PAYLOAD_SIZE_MAX];
	__u16 TxLen;
	__u16 RxLen;
	I3C_ErrCode_Enum result;
	__u16 i;

	if ((buf_size > 0) && (buf == NULL)) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (buf_size > 68) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	TxBuf[0] = CCC;
	TxLen = 1;
	RxLen = 0;

	if (buf_size != 0) {
		for (i = 0; i < buf_size; i++) {
			TxBuf[i + 1] = buf[i];
		}

		TxLen = TxLen + buf_size;
	}

	pNewTaskInfo = I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_CCCb, 0,
		TxLen, &TxLen, &RxLen, TxBuf, NULL, Baudrate, Timeout, callback, PortId,
		Policy, bHIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) &&
		(pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 *
 * @brief               Create CCCw type task into device's task list
 * @param[in]  CCC      CCC
 * @param[in]  HSize    write data length in the first frame including CCC
 * @param[in]  buf_size write data length, not including CCC
 * @param[in]  buf      write data buffer, not including CCC
 * @param [in] Baudrate baudrate to generate CCCw task
 * @param [in] Timeout  task timeout value
 * @param [in] callback callback
 * @param [in] PortId
 * @param [in] Policy   task insertion policy
 * @param [in] bHIF     bHIF = 1, the receive buffer will be free automatically
 *                      if task is finished
 * @return              I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCw(__u8 CCC, __u8 HSize, __u16 buf_size,
	__u8 *buf, __u32 Baudrate, __u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId,
	I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	__u8 TxBuf[70];
	__u16 TxLen;
	__u16 RxLen;
	I3C_ErrCode_Enum result;
	__u16 i;
	__u8 fmt;
	__u16 dev_count;
	__u8 remainder;

	if (HSize < 1) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (buf_size == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (buf == NULL) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if ((HSize - 1) > buf_size) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (buf_size > 63) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	TxBuf[0] = CCC;
	TxLen = 1;
	RxLen = 0;

	if (buf_size) {
		for (i = 0; i < buf_size; i++) {
			TxBuf[i + 1] = buf[i];
		}

		TxLen = 1 + buf_size;
	}

	if ((CCC == CCC_DIRECT_ENTAS0) || (CCC == CCC_DIRECT_ENTAS1) ||
		(CCC == CCC_DIRECT_ENTAS2) || (CCC == CCC_DIRECT_ENTAS3) ||
		(CCC == CCC_DIRECT_RSTDAA) || (CCC == CCC_DIRECT_RSTACT) ||
		(CCC == CCC_DIRECT_RSTGRPA)) {
		if ((CCC == CCC_DIRECT_RSTACT) && (HSize <= 1)) {
			return I3C_ERR_PARAMETER_INVALID;
		}

		fmt = 1;
	} else if ((CCC == CCC_DIRECT_ENEC) || (CCC == CCC_DIRECT_DISEC) ||
			   (CCC == CCC_DIRECT_SETDASA) || (CCC == CCC_DIRECT_SETNEWDA) ||
			   (CCC == CCC_DIRECT_ENDXFER) || (CCC == CCC_DIRECT_SETGRPA) ||
			   (CCC == CCC_DIRECT_MLANE)) {
		if ((CCC == CCC_DIRECT_ENDXFER) && (HSize <= 1)) {
			return I3C_ERR_PARAMETER_INVALID;
		}

		fmt = 2;
	} else if (CCC == CCC_DIRECT_SETMWL) {
		fmt = 3;
	} else if (CCC == CCC_DIRECT_SETMRL) {
		/*fmt = 3;*/
		fmt = 4;
	} else if ((CCC == CCC_DIRECT_SETBRGTGT) || (CCC == CCC_DIRECT_SETROUTE) ||
			 (CCC == CCC_DIRECT_SETXTIME)) {
		fmt = buf_size - (HSize - 1);
	} else if (CCC == CCC_DIRECT_MLANE) {
		return I3C_ERR_HW_NOT_SUPPORT;
	} else {
		return I3C_ERR_HW_NOT_SUPPORT;
	}

RETRY:
	dev_count = (buf_size - (HSize - 1)) / fmt;
	remainder = (buf_size - (HSize - 1)) % fmt;

	if (remainder != 0) {
		/* try another fmt */
		if ((CCC == CCC_DIRECT_SETMRL) && (fmt == 4)) {
			fmt = 3;
			goto RETRY;
		}
	}

	if (dev_count < 1) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	pNewTaskInfo = I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_CCCw, fmt, HSize,
		&TxLen, &RxLen, TxBuf, NULL, Baudrate, Timeout, callback, PortId, Policy, bHIF);
	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) && (pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                 Create CCCr type task into device's task list
 * @param[in]  CCC        CCC
 * @param[in]  HSize      write data length in the first frame including CCC
 * @param[in]  txbuf_size write data length, not including CCC
 * @param[in]  rxbuf_size read data length
 * @param[in]  txbuf      write data buffer, not including CCC
 * @param[in]  rxbuf      read data buffer
 * @param [in] Baudrate   baudrate to generate CCCr task
 * @param [in] Timeout    task timeout value
 * @param [in] callback   callback
 * @param [in] PortId
 * @param [in] Policy     task insertion policy
 * @param [in] bHIF       bHIF = 1, the receive buffer will be free automatically
 *                        if task is finished
 * @return                I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Master_Insert_Task_CCCr(__u8 CCC, __u8 HSize, __u16 txbuf_size,
	__u16 rxbuf_size, __u8 *txbuf, __u8 *rxbuf, __u32 Baudrate, __u32 Timeout,
	ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy, _Bool bHIF)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	__u8 TxBuf[70];
	__u16 TxLen;
	__u16 RxLen;
	I3C_ErrCode_Enum result;
	__u16 i;
	__u8 fmt = 0;
	__u16 dev_count;

	if (HSize < 1) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if ((HSize - 1) > txbuf_size) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (txbuf_size == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (txbuf_size > 63) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (rxbuf_size == 0) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	if (rxbuf_size > 64) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	dev_count = txbuf_size - (HSize - 1);
	if (dev_count > rxbuf_size) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	TxBuf[0] = CCC;
	TxLen = 1;

	if (txbuf_size) {
		for (i = 0; i < txbuf_size; i++) {
			TxBuf[i + 1] = txbuf[i];
		}

		TxLen = 1 + txbuf_size;
	}

	if ((CCC == CCC_DIRECT_GETBCR) || (CCC == CCC_DIRECT_GETDCR) ||
		(CCC == CCC_DIRECT_GETACCCR)) {
		fmt = 1;
	} else if (CCC == CCC_DIRECT_GETMWL) {
		fmt = 2;
	} else if (CCC == CCC_DIRECT_GETMRL) {
		fmt = 3;
	} else if (CCC == CCC_DIRECT_GETXTIME) {
		fmt = 4;
	} else if (CCC == CCC_DIRECT_GETPID) {
		fmt = 6;
	} else if (CCC == CCC_DIRECT_GETSTATUS) {
		if (HSize == 1)	{
			fmt = 2;
		} else if (TxBuf[1] == 0x00) {
			fmt = 2;
		} else if (TxBuf[1] == 0x91) {
			fmt = 2;
		} else {
			fmt = rxbuf_size / dev_count;
		}
	} else if (CCC == CCC_DIRECT_GETMXDS) {
		if (HSize == 1) {
			fmt = 5;
		} else if (TxBuf[1] == 0x91) {
			fmt = 1;
		} else {
			fmt = rxbuf_size / dev_count;
		}
	} else if (CCC == CCC_DIRECT_GETCAPS) {
		if (HSize == 1)	{
			fmt = 4;
		} else {
			fmt = rxbuf_size / dev_count;
		}
	}

	if ((dev_count * fmt) < rxbuf_size) {
		return I3C_ERR_PARAMETER_INVALID;
	}
	RxLen = rxbuf_size;

	pNewTaskInfo = I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_CCCr, fmt, HSize,
		&TxLen, &RxLen, TxBuf, rxbuf, Baudrate, Timeout, callback, PortId, Policy, bHIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) &&
		(pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                  Create Event task into device's task list
 * @param[in]  rxbuf_size  read data length
 * @param[in]  rxbuf       read data buffer
 * @param [in] Baudrate    baudrate to generate Event task
 * @param [in] Timeout     task timeout value
 * @param [in] callback    callback
 * @param [in] PortId
 * @param [in] Policy      task insertion policy
 * @param [in] bHIF        bHIF = 1, the receive buffer will be free automatically
 *                         if task is finished
 * @return                 I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Master_Insert_Task_EVENT(__u16 rxbuf_size, __u8 *rxbuf, __u32 Baudrate,
	__u32 Timeout, ptrI3C_RetFunc callback, __u8 PortId, I3C_TASK_POLICY_Enum Policy,
	_Bool bHIF)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	__u16 TxLen;
	__u16 RxLen;
	I3C_ErrCode_Enum result;

	if ((bHIF == IS_HIF) && (rxbuf == NULL)) {
		return I3C_ERR_PARAMETER_INVALID;
	}

	TxLen = 0;

	/* ibi payload should not more than 255 byte specified in GETMRL if supported */
	RxLen = rxbuf_size;
	pNewTaskInfo = I3C_Master_Create_Task(I3C_TRANSFER_PROTOCOL_EVENT, I3C_BROADCAST_ADDR, 0,
		&TxLen, &RxLen, NULL, rxbuf, Baudrate, Timeout, callback, PortId, Policy, bHIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) && (pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief      Create Hot-Join task into device's task list
 * @param [in] port
 * @return     I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Slave_Insert_Task_HotJoin(I3C_PORT_Enum port)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	I3C_ErrCode_Enum result;
	__u16 TxLen, RxLen;

	TxLen = 0;
	RxLen = 0;
	pNewTaskInfo = I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_HOT_JOIN, 0, &TxLen, &RxLen,
		NULL, NULL, TIMEOUT_TYPICAL, I3C_Slave_Callback, port, NOT_HIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) && (pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief           Create Master-Request task into device's task list
 * @param [in] port
 * @return          I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Slave_Insert_Task_MasterRequest(I3C_PORT_Enum port)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	I3C_ErrCode_Enum result;
	__u16 TxLen, RxLen;

	TxLen = 0;
	RxLen = 0;
	pNewTaskInfo = I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_MASTER_REQUEST, 0,
		&TxLen, &RxLen, NULL, NULL, TIMEOUT_TYPICAL, I3C_Slave_Callback, port, NOT_HIF);
	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) &&
		(pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

/*------------------------------------------------------------------------------*/
/**
 * @brief                 Create IBI task into device's task list
 * @param [in] port
 * @param [in] txbuf_size write data length
 * @param [in] txbuf      write data buffer
 * @return                I3C_ERR_OK, if the task insert successfully
 */
/*------------------------------------------------------------------------------*/
I3C_ErrCode_Enum I3C_Slave_Insert_Task_IBI(I3C_PORT_Enum port, __u16 txbuf_size, __u8 *txbuf)
{
	I3C_TASK_INFO_t *pNewTaskInfo;
	I3C_ErrCode_Enum result;
	__u16 TxLen, RxLen;

	if (hal_I3C_run_ASYN0(port)) {
		TxLen = txbuf_size + 3;
	} else {
		TxLen = txbuf_size;
	}

	RxLen = 0;

	pNewTaskInfo = I3C_Slave_Create_Task(I3C_TRANSFER_PROTOCOL_IBI, 1,
		&TxLen, &RxLen, txbuf, NULL, TIMEOUT_TYPICAL, NULL, port, NOT_HIF);

	if (pNewTaskInfo == NULL) {
		return I3C_ERR_OUT_OF_MEMORY;
	}

	if ((pNewTaskInfo->result != I3C_ERR_PENDING) &&
		(pNewTaskInfo->result != I3C_ERR_OK)) {
		result = pNewTaskInfo->result;
		hal_I3C_MemFree(pNewTaskInfo);
		return result;
	}

	return I3C_ERR_OK;
}

void BUS_RESET_PORT1(void)
{
}

void BUS_RESET_PORT2(void)
{
}

void BUS_CLEAR_PORT1(void)
{
}

void BUS_CLEAR_PORT2(void)
{
}
