/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/sys_io.h>
#include <sys/crc.h>

#include <sys/util.h>
#include <kernel.h>
#include <init.h>
#include <irq.h>
#include <kernel/thread_stack.h>

#include <portability/cmsis_os2.h>

#include <drivers/i3c/NPCM4XX/pub_I3C.h>
#include <drivers/i3c/NPCM4XX/i3c_core.h>
#include <drivers/i3c/NPCM4XX/i3c_master.h>
#include <drivers/i3c/NPCM4XX/i3c_slave.h>
#include <drivers/i3c/NPCM4XX/hal_I3C.h>
#include <drivers/i3c/NPCM4XX/i3c_drv.h>
#include <drivers/i3c/NPCM4XX/api_i3c.h>

#include <stdlib.h>
#include <string.h>

#include "sig_id.h"

LOG_MODULE_REGISTER(npcm4xx_i3c_drv, CONFIG_I3C_LOG_LEVEL);

#define NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE 1024
#define NPCM4XX_I3C_WORK_QUEUE_PRIORITY -2
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area0, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area1, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);

#if (I3C_PORT_MAX == 6)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area2, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area3, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area4, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area5, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif

struct k_work_q npcm4xx_i3c_work_q[I3C_PORT_MAX];

struct k_work work_stop[I3C_PORT_MAX];
struct k_work work_next[I3C_PORT_MAX];
struct k_work work_retry[I3C_PORT_MAX];
struct k_work work_send_ibi[I3C_PORT_MAX];
struct k_work work_rcv_ibi[I3C_PORT_MAX];

void work_stop_fun(struct k_work *item)
{
	uint8_t i;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_stop[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pDevice = api_I3C_Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = api_I3C_Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	pTask = pDevice->pTaskListHead;
	if (pTask == NULL)
		return;

	api_I3C_Master_Stop((uint32_t)pTask);
}

void work_next_fun(struct k_work *item)
{
	uint8_t i;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_next[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pDevice = api_I3C_Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = api_I3C_Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	pTask = pDevice->pTaskListHead;
	if (pTask == NULL)
		return;

	api_I3C_Master_Run_Next_Frame((uint32_t)pTask);
}

void work_retry_fun(struct k_work *item)
{
	uint8_t i;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_retry[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pDevice = api_I3C_Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = api_I3C_Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	pTask = pDevice->pTaskListHead;
	if (pTask == NULL)
		return;

	api_I3C_Master_Retry((uint32_t)pTask);
}

void work_send_ibi_fun(struct k_work *item)
{
	uint8_t i;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDeviceSlv;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_send_ibi[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pBus = api_I3C_Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	if (pBus->pCurrentTask != NULL)	{
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[i], item);
		return;
	}

	pDeviceSlv = api_I3C_Get_INODE(i);

	pTask = pDeviceSlv->pTaskListHead;
	if (pTask == NULL)
		return;

	pTask = pDeviceSlv->pTaskListHead;
	pBus->pCurrentTask = pTask; /* task with higher priority will insert in the front */

	pTaskInfo = pTask->pTaskInfo;
	I3C_Slave_Start_Request((__u32)pTaskInfo);
}

void work_rcv_ibi_fun(struct k_work *item)
{
	uint8_t i;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDeviceMst;

	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_rcv_ibi[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pBus = api_I3C_Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	/* wait until STOP complete */
	if (pBus->pCurrentTask != NULL)	{
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[i], item);
		return;
	}

	pDeviceMst = api_I3C_Get_INODE(i);
	pTask = pDeviceMst->pTaskListHead;
	if (pTask == NULL)
		return;

	pTask = pDeviceMst->pTaskListHead;
	pBus->pCurrentTask = pTask; /* task with higher priority will insert in the front */

	pTaskInfo = pTask->pTaskInfo;
	I3C_Master_Start_Request((__u32)pTaskInfo);
}

const struct device *GetDevNodeFromPort(I3C_PORT_Enum port)
{
	switch (port) {
	case I3C1_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c0)));

	case I3C2_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c1)));

#if (I3C_PORT_MAX == 6)
	case I3C3_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c2)));

	case I3C4_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c3)));

	case I3C5_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c4)));

	case I3C6_IF:
		return device_get_binding(DT_LABEL(DT_NODELABEL(i3c5)));
#endif

	default:
		return NULL;
	}
}

#define I3C_NPCM4XX_CCC_TIMEOUT		K_MSEC(10)
#define I3C_NPCM4XX_XFER_TIMEOUT	K_MSEC(10)
#define I3C_NPCM4XX_SIR_TIMEOUT		K_MSEC(10)

#define I3C_BUS_I2C_STD_TLOW_MIN_NS	4700
#define I3C_BUS_I2C_STD_THIGH_MIN_NS	4000
#define I3C_BUS_I2C_STD_TR_MAX_NS	1000
#define I3C_BUS_I2C_STD_TF_MAX_NS	300
#define I3C_BUS_I2C_FM_TLOW_MIN_NS	1300
#define I3C_BUS_I2C_FM_THIGH_MIN_NS	600
#define I3C_BUS_I2C_FM_TR_MAX_NS	300
#define I3C_BUS_I2C_FM_TF_MAX_NS	300
#define I3C_BUS_I2C_FMP_TLOW_MIN_NS	500
#define I3C_BUS_I2C_FMP_THIGH_MIN_NS	260
#define I3C_BUS_I2C_FMP_TR_MAX_NS	120
#define I3C_BUS_I2C_FMP_TF_MAX_NS	120

/*==========================================================================*/
#ifndef DEVALT10
#define DEVALT10 0x0B
#endif

#ifndef DEVALT0
#define DEVALT0 0x10
#endif

#ifndef DEVALT5
#define DEVALT5	0x15
#endif

#ifndef DEVALT7
#define DEVALT7 0x17
#endif

#ifndef DEVALTA
#define DEVALTA	0x1A
#endif

#ifndef DEVPD1
#define DEVPD1	0x29
#endif

#ifndef DEVPD3
#define DEVPD3	0x7B
#endif

int8_t i3c_npcm4xx_hw_enable_pue(int port)
{
	return 0;
}

int8_t i3c_npcm4xx_hw_disable_pue(int port)
{
	return 0;
}

void I3C_Enable_Interrupt(I3C_PORT_Enum port)
{
}

void I3C_Disable_Interrupt(I3C_PORT_Enum port)
{
}

void I3C_Enable_Interface(I3C_PORT_Enum port)
{
}

void I3C_Disable_Interface(I3C_PORT_Enum port)
{
}

uint8_t PDMA_I3C_CH[I3C_PORT_MAX * 2] = {
#if (I3C_PORT_MAX == I3C3_IF)
	PDMA_I3C1_TX, PDMA_I3C2_TX, PDMA_I3C1_RX, PDMA_I3C2_RX
#else
	PDMA_I3C1_TX, PDMA_I3C2_TX, PDMA_I3C3_TX, PDMA_I3C4_TX, PDMA_I3C5_TX, PDMA_I3C6_TX,
	PDMA_I3C1_RX, PDMA_I3C2_RX, PDMA_I3C3_RX, PDMA_I3C4_RX, PDMA_I3C5_RX, PDMA_I3C6_RX
#endif
};

/* declare 16 pdma descriptors to handle master/slave tx
 * in scatter gather mode
 * Rx always use basic mode
 */
struct SCAT {
	struct dsct_reg SCAT_DSCT[16] __aligned(16);
};

#define SCAT_T struct SCAT

SCAT_T I3C_SCATTER_GATHER_TABLE __aligned(256);
uint32_t PDMA_TxBuf_END[I3C_PORT_MAX] __aligned(4);

#define I3C_DSCT I3C_SCATTER_GATHER_TABLE.SCAT_DSCT

/* typedef struct pdma_reg PDMA_T; */

#define PDMA_T struct pdma_reg

#define PDMA ((PDMA_T *)PDMA_BASE_ADDR)
/*==========================================================================*/
#define I3C_GET_REG_MCONFIG(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCONFIG)
#define I3C_SET_REG_MCONFIG(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MCONFIG)

#define I3C_GET_REG_CONFIG(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CONFIG)
#define I3C_SET_REG_CONFIG(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_CONFIG)

#define I3C_GET_REG_STATUS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_STATUS)
#define I3C_SET_REG_STATUS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_STATUS)

#define I3C_GET_REG_CTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CTRL)
#define I3C_SET_REG_CTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_CTRL)

#define I3C_GET_REG_INTSET(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_INTSET)
#define I3C_SET_REG_INTSET(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_INTSET)

#define I3C_SET_REG_INTCLR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_INTCLR)

#define I3C_GET_REG_INTMASKED(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_INTMASKED)

#define I3C_GET_REG_ERRWARN(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_ERRWARN)
#define I3C_SET_REG_ERRWARN(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_ERRWARN)

#define I3C_GET_REG_DMACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DMACTRL)
#define I3C_SET_REG_DMACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_DMACTRL)

#define I3C_GET_REG_DATACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DATACTRL)
#define I3C_SET_REG_DATACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_DATACTRL)

#define I3C_SET_REG_WDATAB(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAB)
#define I3C_SET_REG_WDATABE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATABE)
#define I3C_SET_REG_WDATAH(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAH)
#define I3C_SET_REG_WDATAHE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_WDATAHE)

#define I3C_GET_REG_RDATAB(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_RDATAB)
#define I3C_GET_REG_RDATAH(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_RDATAH)

#define I3C_SET_REG_WDATAB1(port, val) sys_write8(val, I3C_BASE_ADDR(port) + OFFSET_WDATAB1)

#define I3C_GET_REG_CAPABILITIES(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_CAPABILITIES)
#define I3C_GET_REG_DYNADDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_DYNADDR)

#define I3C_GET_REG_MAXLIMITS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MAXLIMITS)
#define I3C_SET_REG_MAXLIMITS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MAXLIMITS)

#define I3C_GET_REG_PARTNO(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_PARTNO)
#define I3C_SET_REG_PARTNO(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_PARTNO)

#define I3C_GET_REG_IDEXT(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IDEXT)
#define I3C_SET_REG_IDEXT(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IDEXT)

#define I3C_GET_REG_VENDORID(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_VENDORID)
#define I3C_SET_REG_VENDORID(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_VENDORID)

#define I3C_GET_REG_TCCLOCK(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_TCCLOCK)
#define I3C_SET_REG_TCCLOCK(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_TCCLOCK)

#define I3C_GET_REG_MCTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCTRL)
#define I3C_SET_REG_MCTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MCTRL)

#define I3C_GET_REG_MSTATUS(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MSTATUS)
#define I3C_SET_REG_MSTATUS(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MSTATUS)

#define I3C_GET_REG_IBIRULES(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIRULES)
#define I3C_SET_REG_IBIRULES(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIRULES)

#define I3C_GET_REG_MINTSET(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MINTSET)
#define I3C_SET_REG_MINTSET(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MINTSET)

#define I3C_SET_REG_MINTCLR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MINTCLR)

#define I3C_GET_REG_MINTMASKED(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MINTMASKED)

#define I3C_GET_REG_MERRWARN(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MERRWARN)
#define I3C_SET_REG_MERRWARN(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MERRWARN)

#define I3C_GET_REG_MDMACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDMACTRL)
#define I3C_SET_REG_MDMACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDMACTRL)

#define I3C_GET_REG_MDATACTRL(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDATACTRL)
#define I3C_SET_REG_MDATACTRL(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDATACTRL)

#define I3C_SET_REG_MWDATAB(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAB)
#define I3C_SET_REG_MWDATABE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATABE)
#define I3C_SET_REG_MWDATAH(port, val) sys_write32(val. I3C_BASE_ADDR(port) + OFFSET_MWDATAH)
#define I3C_SET_REG_MWDATAHE(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAHE)

#define I3C_GET_REG_MRDATAB(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRDATAB)
#define I3C_GET_REG_MRDATAH(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRDATAH)

#define I3C_SET_REG_MWDATAB1(port, val) sys_write8(val, I3C_BASE_ADDR(port) + OFFSET_MWDATAB1)

#define I3C_SET_REG_MWMSG_SDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWMSG_SDR)

#define I3C_GET_REG_MRMSG_SDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MRMSG_SDR)

#define I3C_SET_REG_MWMSG_DDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MWMSG_DDR)

#define I3C_GET_REG_MRMSG_DDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MWMSG_DDR)

#define I3C_GET_REG_MDYNADDR(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_MDYNADDR)
#define I3C_SET_REG_MDYNADDR(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_MDYNADDR)

#define I3C_GET_REG_HDRCMD(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_HDRCMD)

#define I3C_GET_REG_IBIEXT1(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIEXT1)
#define I3C_SET_REG_IBIEXT1(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIEXT1)

#define I3C_GET_REG_IBIEXT2(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_IBIEXT2)
#define I3C_SET_REG_IBIEXT2(port, val) sys_write32(val, I3C_BASE_ADDR(port) + OFFSET_IBIEXT2)

#define I3C_GET_REG_ID(port) sys_read32(I3C_BASE_ADDR(port) + OFFSET_ID)
/*==========================================================================*/

/*
 * Customize Register layout
 */
#define I3C_REGS_COUNT_PORT	2

#define CMDIdx_MSG               0x01
#define CMD_BUF_LEN_MSG          64

#define CMDIdx_ID                0x0F
#define CMD_BUF_LEN_ID           1

uint8_t I3C_REGs_BUF_CMD_MSG[CMD_BUF_LEN_MSG];
uint8_t I3C_REGs_BUF_CMD_ID[CMD_BUF_LEN_ID] = {
	0x6C
};

/* Used to define slave's register table */
I3C_REG_ITEM_t I3C_REGs_PORT_SLAVE[I3C_REGS_COUNT_PORT] = { {
	.cmd.cmd8 = CMDIdx_MSG, .len = CMD_BUF_LEN_MSG, .buf = I3C_REGs_BUF_CMD_MSG,
	.attr.width = 0, .attr.read = TRUE, .attr.write = TRUE }, {
	.cmd.cmd8 = CMDIdx_ID, .len = CMD_BUF_LEN_ID, .buf = I3C_REGs_BUF_CMD_ID,
	.attr.width = 0, .attr.read = TRUE, .attr.write = FALSE },
};

void hal_I3C_Config_Internal_Device(I3C_PORT_Enum port, I3C_DEVICE_INFO_t *pDevice)
{
	if (port >= I3C_PORT_MAX)
		return;
	if (pDevice == NULL)
		return;

	/* move to dtsi and i3c_npcm4xx_init() */
}

/*--------------------------------------------------------------------------------------*/
/**
 * @brief                           Update I3C register settings for the specific internal device
 * @param [in]      pDevice         The I3C port
 * @return                          None
 */
/*--------------------------------------------------------------------------------------*/
I3C_ErrCode_Enum hal_I3C_Config_Device(I3C_DEVICE_INFO_t *pDevice)
{
	I3C_PORT_Enum port = api_I3C_Get_IPORT(pDevice);
	I3C_ErrCode_Enum result = I3C_ERR_OK;
	uint32_t mconfig;
	uint32_t sconfig;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	if ((pDevice->capability.OFFLINE == FALSE) && (pDevice->bcr & 0x08))
		return I3C_ERR_PARAMETER_INVALID;

	result = I3C_ERR_OK;

	/* SKEW = 0, HKEEP = 3 */
	mconfig = ((I3C_GET_REG_MCONFIG(port) & 0xF1FFFF7B) |
		I3C_MCONFIG_HKEEP(I3C_MCONFIG_HKEEP_EXTBOTH));
	sconfig = I3C_GET_REG_CONFIG(port) & 0xFE7F071F;

	if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
		I3C_Disable_Interrupt(port);
		I3C_Enable_Interface(port);

		I3C_SET_REG_MDYNADDR(port, I3C_GET_REG_MDYNADDR(port) & 0xFE);
		I3C_SET_REG_CONFIG(port, 0x00);
		I3C_SET_REG_MCONFIG(port, 0x30);
		return result;
	}

	I3C_SET_REG_MDATACTRL(port, I3C_GET_REG_MDATACTRL(port) | I3C_MDATACTRL_FLUSHTB_MASK |
		I3C_MDATACTRL_FLUSHFB_MASK);

	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK |
		I3C_DATACTRL_FLUSHFB_MASK);

	I3C_SET_REG_MSTATUS(port, 0xFFFFFFFFul);
	I3C_SET_REG_STATUS(port, 0xFFFFFFFFul);

	if (pDevice->capability.DMA) {
		I3C_SET_REG_MDMACTRL(port, I3C_MDMACTRL_DMAWIDTH(1));
		I3C_SET_REG_DMACTRL(port, I3C_DMACTRL_DMAWIDTH(1));

		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + port)
			| MaskBit(PDMA_OFFSET + I3C_PORT_MAX + port);

#if (I3C_PORT_MAX == I3C3_IF)
		if (port == I3C1_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0xFF00FF00U) | (PDMA_I3C1_TX << 0)
				| (PDMA_I3C1_RX << 16);
		} else {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0x00FF00FFU) | (PDMA_I3C2_TX << 8)
				| (PDMA_I3C2_RX << 24);
		}
#elif (PDMA_OFFSET == 0)
		if (port == I3C1_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0xFFFFFF00U) | (PDMA_I3C1_TX << 0);
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFF00FFFFU) | (PDMA_I3C1_RX << 16);
		} else if (port == I3C2_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0xFFFF00FFU) | (PDMA_I3C2_TX << 8);
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0x00FFFFFFU) | (PDMA_I3C2_RX << 24);
		} else if (port == I3C3_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0xFF00FFFFU) | (PDMA_I3C3_TX << 16);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFFFFFF00U) | (PDMA_I3C3_RX << 0);
		} else if (port == I3C4_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0x00FFFFFFU) | (PDMA_I3C4_TX << 24);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFFFF00FFU) | (PDMA_I3C4_RX << 8);
		} else if (port == I3C5_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFFFFFF00U) | (PDMA_I3C5_TX << 0);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFF00FFFFU) | (PDMA_I3C5_RX << 16);
		} else if (port == I3C6_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFFFF00FFU) | (PDMA_I3C6_TX << 8);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0x00FFFFFFU) | (PDMA_I3C6_RX << 24);
		}
#elif (PDMA_OFFSET == 2)
		if (port == I3C1_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0xFF00FFFFU) | (PDMA_I3C1_TX << 16);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFFFFFF00U) | (PDMA_I3C1_RX << 0);
		} else if (port == I3C2_IF) {
			PDMA->REQSEL0_3 = (PDMA->REQSEL0_3 & 0x00FFFFFFU) | (PDMA_I3C2_TX << 24);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFFFF00FFU) | (PDMA_I3C2_RX << 8);
		} else if (port == I3C3_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFFFFFF00U) | (PDMA_I3C3_TX << 0);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0xFF00FFFFU) | (PDMA_I3C3_RX << 16);
		} else if (port == I3C4_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFFFF00FFU) | (PDMA_I3C4_TX << 8);
			PDMA->REQSEL8_11 = (PDMA->REQSEL8_11 & 0x00FFFFFFU) | (PDMA_I3C4_RX << 24);
		} else if (port == I3C5_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0xFF00FFFFU) | (PDMA_I3C5_TX << 16);
			PDMA->REQSEL12_15 = (PDMA->REQSEL12_15 & 0xFFFFFF00U) | (PDMA_I3C5_RX << 0);
		} else if (port == I3C6_IF) {
			PDMA->REQSEL4_7 = (PDMA->REQSEL4_7 & 0x00FFFFFFU) | (PDMA_I3C6_TX << 24);
			PDMA->REQSEL12_15 = (PDMA->REQSEL12_15 & 0xFFFF00FFU) | (PDMA_I3C6_RX << 8);
		}
#endif

		PDMA->INTEN = 0;
		PDMA->SCATBA = (uint32_t) I3C_SCATTER_GATHER_TABLE.SCAT_DSCT;
	}

	hal_I3C_set_MAXRD(port, pDevice->max_rd_len);
	hal_I3C_set_MAXWR(port, pDevice->max_wr_len);

	/* Used for the Mixed bus to send the first START frame */
	mconfig &= ~I3C_MCONFIG_ODHPP_MASK;
	if (pDevice->enableOpenDrainHigh)
		mconfig |= I3C_MCONFIG_ODHPP_MASK;

	/* baudrate default setting
	 * I3C PP: 12.5 MHz, PP_High = 40ns, PP_Low = 40ns
	 * I3C_OD: 1MHz, OD_High = 40ns, OD_Low = 960ns
	 * I2C_OD: 100 KHz, OD_High = 5000ns, OD_Low = 5000ns
	 */
	/* mconfig &= ~(I3C_MCONFIG_I2CBAUD_MASK | I3C_MCONFIG_ODBAUD_MASK |
	 *	I3C_MCONFIG_PPLOW_MASK | I3C_MCONFIG_PPBAUD_MASK);
	 * mconfig |= I3C_MCONFIG_I2CBAUD(I2CBAUD) | I3C_MCONFIG_ODBAUD(ODBAUD) |
	 *	I3C_MCONFIG_PPLOW(PPLOW) | I3C_MCONFIG_PPBAUD(PPBAUD);
	 */

	/* Whether to emit open-drain speed STOP. */
	/* if (pDevice->enableOpenDrainStop)   mconfig |= I3C_MCONFIG_ODSTOP_MASK; */
	/* else                                mconfig &= ~I3C_MCONFIG_ODSTOP_MASK; */

	/* disable timeout, we can't guarantee I3C task engine always process task in time. */
	mconfig = mconfig & ~I3C_MCONFIG_DISTO_MASK;
	if (pDevice->disableTimeout)
		mconfig |= I3C_MCONFIG_DISTO_MASK;

	/* Always handle IBI by IBIRULES to prevent slave nack */
	/* Sould enable TIMEOUT only when we try to handle SLVSTART manually */

	/* Init IBIRULES for those slaves support IBI without mandatory data byte */
	/* I3C_SET_REG_IBIRULES(port, I3C_IBIRULES_NOBYTE_MASK); */

	/* OFFLINE -> BCR[3], this flag only tells master the slave might not response in time */
	sconfig &= ~I3C_CONFIG_OFFLINE_MASK;
	if (pDevice->capability.OFFLINE)
		sconfig |= I3C_CONFIG_OFFLINE_MASK;

	/* BAMATCH */
	sconfig &= ~I3C_CONFIG_BAMATCH_MASK;
	if ((pDevice->capability.IBI) || (pDevice->capability.ASYNC0)) {
		/* prevent slave assert 100 us timeout error */
		/*sconfig |= I3C_CONFIG_BAMATCH(I3C_CLOCK_SLOW_FREQUENCY / I3C_1MHz_VAL_CONST);*/
		sconfig |= I3C_CONFIG_BAMATCH(0x7F);

		/* if support ASYNC-0, update TCCLOCK */
		/* I3C_SET_REG_TCCLOCK(port, I3C_TCCLOCK_FREQ(2 * (I3C_CLOCK_FREQUENCY / */
		/*	I3C_1MHz_VAL_CONST)) | I3C_TCCLOCK_ACCURACY(30)); */
	}

	/* HDRCMD, always enable HDRCMD to detect command process too slow */
	sconfig |= I3C_CONFIG_HDRCMD_MASK;

	/* DDROK */
	sconfig &= ~I3C_CONFIG_DDROK_MASK;
	if (pDevice->capability.HDR_DDR)
		sconfig |= I3C_CONFIG_DDROK_MASK;

	/* master can split read message into several small pieces to meet
	 * slave's write length limit
	 * If master try to terminate "the read", it can send STOP.
	 * If slave doesn't support stopSplitRead, data keep in buffer until
	 * all of them are read out or new "Index" register is assigned
	 */
	if (pDevice->stopSplitRead == FALSE) {
	}

	/* assign static address */
	sconfig &= ~I3C_CONFIG_SADDR_MASK;
	if (pDevice->staticAddr != 0x00)
		sconfig |= I3C_CONFIG_SADDR(pDevice->staticAddr);

	sconfig &= ~I3C_CONFIG_S0IGNORE_MASK;
	sconfig &= ~I3C_CONFIG_MATCHSS_MASK;
	sconfig &= ~I3C_CONFIG_NACK_MASK;

	/* 0: Fixed, 1: Random */
	if (pDevice->pid[1] & 0x01) {
		sconfig |= I3C_CONFIG_IDRAND_MASK;
	} else {
		sconfig &= ~I3C_CONFIG_IDRAND_MASK;

		/* Part ID[15:0] + Instance ID[3:0] + Vendor[11:0] */
		I3C_SET_REG_PARTNO(port, pDevice->partNumber);
	}


	I3C_SET_REG_IDEXT(port, I3C_GET_REG_IDEXT(port) &
		~(I3C_IDEXT_BCR_MASK | I3C_IDEXT_DCR_MASK));
	I3C_SET_REG_IDEXT(port, I3C_GET_REG_IDEXT(port) |
		(I3C_IDEXT_BCR(pDevice->bcr) | I3C_IDEXT_DCR(pDevice->dcr)));

	I3C_SET_REG_MINTCLR(port, I3C_MINTCLR_NOWMASTER_MASK | I3C_MINTCLR_ERRWARN_MASK |
		I3C_MINTCLR_IBIWON_MASK | I3C_MINTCLR_TXNOTFULL_MASK | I3C_MINTCLR_RXPEND_MASK |
		I3C_MINTCLR_COMPLETE_MASK | I3C_MINTCLR_MCTRLDONE_MASK |
		I3C_MINTCLR_SLVSTART_MASK);

	I3C_SET_REG_MINTSET(port, I3C_MINTSET_NOWMASTER_MASK | I3C_MINTSET_ERRWARN_MASK |
		I3C_MINTSET_IBIWON_MASK | I3C_MINTSET_COMPLETE_MASK | I3C_MINTSET_SLVSTART_MASK);

	I3C_SET_REG_INTCLR(port, I3C_INTCLR_EVENT_MASK | I3C_INTCLR_CHANDLED_MASK |
		I3C_INTCLR_DDRMATCHED_MASK | I3C_INTCLR_ERRWARN_MASK | I3C_INTCLR_CCC_MASK |
		I3C_INTCLR_DACHG_MASK | I3C_INTCLR_TXNOTFULL_MASK | I3C_INTCLR_RXPEND_MASK |
		I3C_INTCLR_STOP_MASK | I3C_INTCLR_MATCHED_MASK | I3C_INTCLR_START_MASK);

	I3C_SET_REG_INTSET(port, /*I3C_INTSET_CHANDLED_MASK | */I3C_INTSET_DDRMATCHED_MASK |
		I3C_INTSET_ERRWARN_MASK | I3C_INTSET_CCC_MASK | I3C_INTSET_DACHG_MASK |
		I3C_INTSET_STOP_MASK | I3C_INTSET_START_MASK);

	if (pDevice->mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		I3C_SET_REG_MDYNADDR(port, (pDevice->dynamicAddr << I3C_MDYNADDR_DADDR_SHIFT) |
			I3C_MDYNADDR_DAVALID_MASK);
		I3C_SET_REG_CONFIG(port, sconfig |
			I3C_CONFIG_SLVENA(I3C_CONFIG_SLVENA_SLAVE_OFF));
		I3C_SET_REG_MCONFIG(port, mconfig |
			I3C_MCONFIG_MSTENA(I3C_MCONFIG_MSTENA_MASTER_ON));
	} else if (pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY) {
		I3C_SET_REG_MCONFIG(port, mconfig |
			I3C_MCONFIG_MSTENA(I3C_MCONFIG_MSTENA_MASTER_OFF));

		I3C_SET_REG_CONFIG(port, sconfig);
		sconfig |= I3C_CONFIG_SLVENA(I3C_CONFIG_SLVENA_SLAVE_ON);
		I3C_SET_REG_CONFIG(port, sconfig);


	} else if (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER) {
		I3C_SET_REG_MCONFIG(port, mconfig |
			I3C_MCONFIG_MSTENA(I3C_MCONFIG_MSTENA_MASTER_CAPABLE));

		I3C_SET_REG_CONFIG(port, sconfig);
		sconfig |= I3C_CONFIG_SLVENA(I3C_CONFIG_SLVENA_SLAVE_ON);
		I3C_SET_REG_CONFIG(port, sconfig);
	}

	I3C_Enable_Interface(port);
	I3C_Enable_Interrupt(port);

	if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY)
		|| (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		I3C_Prepare_To_Read_Command((uint32_t) port);
	}

	return result;
}

int8_t LoadBaudrateSetting(I3C_TRANSFER_TYPE_Enum type, uint32_t baudrate, uint32_t *pPPBAUD,
	uint32_t *pPPLOW, uint32_t *pODBAUD, uint32_t *pI2CBAUD)
{
	if (type == I3C_TRANSFER_TYPE_I2C) {
		switch (baudrate) {
		case I3C_TRANSFER_SPEED_I2C_1MHZ:
			*pPPBAUD = 1;
			*pPPLOW = 0;
			*pODBAUD = 5;
			*pI2CBAUD = 2;
			break;
		case I3C_TRANSFER_SPEED_I2C_400KHZ:
			*pPPBAUD = 1;
			*pPPLOW = 0;
			*pODBAUD = 5;
			*pI2CBAUD = 8;
			break;
		default: /* if (pFrame->baudrate == I3C_TRANSFER_SPEED_I2C_100KHZ) */
			*pPPBAUD = 1;
			*pPPLOW = 0;
			*pODBAUD = 15;
			*pI2CBAUD = 13;
		}

		return 0;
	}

	switch (baudrate) {
	case I3C_TRANSFER_SPEED_SDR_12p5MHZ:
	/* I3C PP=12MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 1;
		*pPPLOW = 0;
		*pODBAUD = 4;
		*pI2CBAUD = 3;
		break;
	case I3C_TRANSFER_SPEED_SDR_8MHZ:
	/* I3C PP=8MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 1;
		*pPPLOW = 2;
		*pODBAUD = 4;
		*pI2CBAUD = 3;
		break;
	case I3C_TRANSFER_SPEED_SDR_6MHZ:
	/* I3C PP=6MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 1;
		*pPPLOW = 4;
		*pODBAUD = 4;
		*pI2CBAUD = 3;
		break;
	case I3C_TRANSFER_SPEED_SDR_4MHZ:
	/* I3C PP=4MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 1;
		*pPPLOW = 8;
		*pODBAUD = 4;
		*pI2CBAUD = 3;
		break;
	case I3C_TRANSFER_SPEED_SDR_2MHZ:
	/* I3C PP=2MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 1;
		*pPPLOW = 16;
		*pODBAUD = 4;
		*pI2CBAUD = 3;
		break;
	case I3C_TRANSFER_SPEED_SDR_1MHZ:
	/* I3C PP=1MHz, OD Freq = 4MHz, I2C FM+ (H=400ns, L=600ns) */
		*pPPBAUD = 5;
		*pPPLOW = 12;
		*pODBAUD = 0;
		*pI2CBAUD = 6;
		break;
	default:
	/* PP=4MHz, OD Freq = 1MHz for IBI */
		*pPPBAUD = 5;
		*pPPLOW = 0;
		*pODBAUD = 6;
		*pI2CBAUD = 8;
	}

	return 0;
}

/*
 * I3C PP High Width = (PPBAUD + 1) * fclk_width
 * I3C PP Low Width = (PPLOW + PPBAUD + 1) * fclk_width
 *
 * if ODHPP = 1, I3C OD High Width = (PPBAUD + 1) * fclk_width
 *               I3C OD Low Width = (ODBAUD + 1) * (PPBAUD + 1) * fclk_width
 * if ODHPP = 0, I3C OD High Width = (ODBAUD + 1) * (PPBAUD + 1) * fclk_width
 *               I3C OD Low Width = (ODBAUD + 1) * (PPBAUD + 1) * fclk_width
 *
 * I2C Low Period = I2C High Period = ((I2CBAUD / 2) + 1) * (ODBAUD + 1) * (PPBAUD + 1) * fclk_width
 */
void I3C_SetXferRate(I3C_TASK_INFO_t *pTaskInfo)
{
	bool bBroadcast = FALSE;
	bool bSlaveFound = FALSE;
	uint32_t PPBAUD;
	uint32_t PPLOW;
	uint32_t ODBAUD;
	uint32_t I2CBAUD;
	uint32_t ODHPP = 1;
	I3C_DEVICE_INFO_t *pMasterDevice;

	uint32_t mconfig;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_PROTOCOL_Enum protocol;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_DEVICE_INFO_SHORT_t *pSlaveDevInfo = NULL;

	if (pTaskInfo == NULL)
		return;

	mconfig = I3C_GET_REG_MCONFIG(pTaskInfo->Port);
	mconfig &= ~(I3C_MCONFIG_I2CBAUD_MASK | I3C_MCONFIG_ODHPP_MASK | I3C_MCONFIG_ODBAUD_MASK
		| I3C_MCONFIG_PPLOW_MASK | I3C_MCONFIG_PPBAUD_MASK);

	pMasterDevice = api_I3C_Get_INODE(pTaskInfo->Port);

	pTask = pTaskInfo->pTask;
	if (pTask->frame_idx != 0)
		return;

	protocol = pTask->protocol;
	pFrame = &(pTask->pFrameList[pTask->frame_idx]);

	bBroadcast = (pTask->address == I3C_BROADCAST_ADDR)
		|| (protocol == I3C_TRANSFER_PROTOCOL_CCCb)
		|| (protocol == I3C_TRANSFER_PROTOCOL_CCCw)
		|| (protocol == I3C_TRANSFER_PROTOCOL_CCCr)
		|| (protocol == I3C_TRANSFER_PROTOCOL_ENTDAA)
		|| (protocol == I3C_TRANSFER_PROTOCOL_EVENT);

	if (bBroadcast && (pFrame->type != I3C_TRANSFER_TYPE_I2C)
		&& ((pFrame->flag & I3C_TRANSFER_REPEAT_START) == 0)) {
		ODHPP = 0;
	}

	if (!bBroadcast) {
		pSlaveDevInfo = pMasterDevice->pOwner->pDevList;
		while (pSlaveDevInfo != NULL) {
			if (pSlaveDevInfo->attr.b.runI3C) {
				if (pSlaveDevInfo->dynamicAddr == pTask->address)
					break;
			} else {
				if (pSlaveDevInfo->staticAddr == pTask->address)
					break;
			}

			pSlaveDevInfo = pSlaveDevInfo->pNextDev;
		}

		bSlaveFound = (pSlaveDevInfo != NULL);
	}

	if (bBroadcast) {
		/* Can't make sure all slave device run in i3c mode ?*/
		/* PP=4MHz, OD Freq = 1MHz for I3C device before get dynamic address */
		PPBAUD = 5;
		PPLOW = 0;
		ODBAUD = 3;
		I2CBAUD = 8;
	} else if (bSlaveFound) {
		if ((pSlaveDevInfo->attr.b.runI3C) == (pFrame->type != I3C_TRANSFER_TYPE_I2C)) {
			LoadBaudrateSetting(pFrame->type, pFrame->baudrate, &PPBAUD, &PPLOW,
			&ODBAUD, &I2CBAUD);
		}
	}

	mconfig |= I3C_MCONFIG_I2CBAUD(I2CBAUD) | I3C_MCONFIG_ODHPP(ODHPP)
		| I3C_MCONFIG_ODBAUD(ODBAUD) | I3C_MCONFIG_PPLOW(PPLOW)
		| I3C_MCONFIG_PPBAUD(PPBAUD);
	I3C_SET_REG_MCONFIG(pTaskInfo->Port, mconfig);
}

I3C_ErrCode_Enum hal_I3C_enable_interface(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	I3C_Enable_Interface(port);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_disable_interface(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	I3C_Disable_Interface(port);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_enable_interrupt(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	I3C_Disable_Interrupt(port);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_disable_interrupt(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	I3C_Enable_Interrupt(port);
	return I3C_ERR_OK;
}

__u32 hal_I3C_get_MAXRD(I3C_PORT_Enum port)
{
	__u32 regVal;

	if (port >= I3C_PORT_MAX)
		return 0;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal = (regVal & I3C_MAXLIMITS_MAXRD_MASK) >> I3C_MAXLIMITS_MAXRD_SHIFT;
	return regVal;
}

void hal_I3C_set_MAXRD(I3C_PORT_Enum port, __u32 val)
{
	__u32 regVal;

	if (port >= I3C_PORT_MAX)
		return;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal &= I3C_MAXLIMITS_MAXWR_MASK;
	val = ((val << I3C_MAXLIMITS_MAXRD_SHIFT) & I3C_MAXLIMITS_MAXRD_MASK);
	regVal = regVal | val;
	I3C_SET_REG_MAXLIMITS(port, regVal);
}

__u32 hal_I3C_get_MAXWR(I3C_PORT_Enum port)
{
	__u32 regVal;

	if (port >= I3C_PORT_MAX)
		return 0;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal = (regVal & I3C_MAXLIMITS_MAXWR_MASK) >> I3C_MAXLIMITS_MAXWR_SHIFT;
	return regVal;
}

void hal_I3C_set_MAXWR(I3C_PORT_Enum port, __u32 val)
{
	__u32 regVal;

	if (port >= I3C_PORT_MAX)
		return;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal &= I3C_MAXLIMITS_MAXRD_MASK;
	val = ((val << I3C_MAXLIMITS_MAXWR_SHIFT) & I3C_MAXLIMITS_MAXWR_MASK);
	regVal = regVal | val;
	I3C_SET_REG_MAXLIMITS(port, regVal);
}

__u32 hal_I3C_get_mstatus(I3C_PORT_Enum port)
{
	return I3C_GET_REG_MSTATUS(port);
}

void hal_I3C_set_mstatus(I3C_PORT_Enum port, __u32 val)
{
	I3C_SET_REG_MSTATUS(port, val);
}

I3C_IBITYPE_Enum hal_I3C_get_ibiType(I3C_PORT_Enum port)
{
	uint32_t mstatus = I3C_GET_REG_MSTATUS(port);
	uint8_t ibi_type = (uint8_t)(
		(mstatus & I3C_MSTATUS_IBITYPE_MASK) >> I3C_MSTATUS_IBITYPE_SHIFT);
	return ibi_type;
}

__u8 hal_I3C_get_ibiAddr(I3C_PORT_Enum port)
{
	uint32_t mstatus = I3C_GET_REG_MSTATUS(port);
	uint8_t ibi_addr = (uint8_t)(
		(mstatus & I3C_MSTATUS_IBIADDR_MASK) >> I3C_MSTATUS_IBIADDR_SHIFT);
	return ibi_addr;
}

_Bool hal_I3C_Is_Master_Idle(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_IDLE) ?
		TRUE : FALSE;
}

_Bool hal_I3C_Is_Slave_Idle(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return FALSE;
	if ((I3C_GET_REG_STATUS(port) & I3C_STATUS_STNOTSTOP_MASK) == 0)
		return TRUE;
	return FALSE;
}

_Bool hal_I3C_Is_Master_DAA(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DAA) ?
		TRUE : FALSE;
}

_Bool hal_I3C_Is_Slave_DAA(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return FALSE;
	if (I3C_GET_REG_STATUS(port) & I3C_STATUS_STDAA_MASK)
		return TRUE;
	return FALSE;
}

_Bool hal_I3C_Is_Master_SLVSTART(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_SLVREQ) ?
		TRUE : FALSE;
}

_Bool hal_I3C_Is_Slave_SLVSTART(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return FALSE;
	if (I3C_GET_REG_STATUS(port) & I3C_STATUS_EVENT_MASK)
		return TRUE;
	return FALSE;
}

_Bool hal_I3C_Is_Master_NORMAL(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_NORMACT) ?
		TRUE : FALSE;
}

void hal_I3C_Nack_IBI(I3C_PORT_Enum port)
{
	I3C_SET_REG_MCTRL(port, I3C_MCTRL_REQUEST_IBI_MANUAL | I3C_MCTRL_IBIRESP(1));
}

void hal_I3C_Ack_IBI_Without_MDB(I3C_PORT_Enum port)
{
	I3C_SET_REG_MCTRL(port, I3C_MCTRL_REQUEST_IBI_MANUAL);
}

void hal_I3C_Ack_IBI_With_MDB(I3C_PORT_Enum port)
{
	/* if Master ack IBI too late, Slave might nack */
	I3C_SET_REG_MCTRL(port, I3C_MCTRL_REQUEST_IBI_MANUAL | I3C_MCTRL_IBIRESP(2));
}

uint8_t Get_PDMA_Channel(I3C_PORT_Enum port, I3C_TRANSFER_DIR_Enum direction)
{
	/* invalid channel, 6694 only support 14 channels, 0 - 13
	 * I3C PDMA channel,
	 * 0: I3C1_TX, 1: I3C2_TX, 2: I3C3_TX, 3: I3C4_TX, 4: I3C5_TX, 5: I3C6_TX,
	 * 6: I3C1_RX, 7: I3C2_RX, 8: I3C3_RX, 9: I3C4_RX, 10: I3C5_RX, 11: I3C6_RX,
	 */

	if (port >= I3C_PORT_MAX)
		return 0xFF;

	if (direction == I3C_TRANSFER_DIR_WRITE)
		return port;
	else
		return (I3C_PORT_MAX + port);
}

void hal_I3C_Disable_Master_TX_DMA(I3C_PORT_Enum port)
{
	__u8 pdma_ch;

	I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMATB_MASK);

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);
	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
}

void hal_I3C_Disable_Master_RX_DMA(I3C_PORT_Enum port)
{
	__u8 pdma_ch;

	I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMAFB_MASK);

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);
	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
}

void hal_I3C_Disable_Master_DMA(I3C_PORT_Enum port)
{
	I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~(I3C_MDMACTRL_DMAFB_MASK |
		I3C_MDMACTRL_DMATB_MASK));
}

void hal_I3C_Reset_Master_TX(I3C_PORT_Enum port)
{
	I3C_SET_REG_MDATACTRL(port, I3C_GET_REG_MDATACTRL(port) | I3C_MDATACTRL_FLUSHTB_MASK);
}

_Bool hal_I3C_TxFifoNotFull(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_TXNOTFULL_MASK) ? TRUE : FALSE;
}

void hal_I3C_WriteByte(I3C_PORT_Enum port, __u8 val)
{
	I3C_SET_REG_MWDATABE(port, val);
}

_Bool hal_I3C_DMA_Write(I3C_PORT_Enum port, I3C_DEVICE_MODE_Enum mode, __u8 *txBuf, __u16 txLen)
{
	__u8 pdma_ch;

	if (txLen == 0)
		return TRUE;
	if (port >= I3C_PORT_MAX)
		return FALSE;
	if ((mode != I3C_DEVICE_MODE_CURRENT_MASTER) && (mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		(mode != I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		return FALSE;
	}

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);
	if (mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		if (txLen == 1) {
			/* case 1: Write Data Len = 1 and Tx FIFO not full */
			if (I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_TXNOTFULL_MASK) {
				I3C_SET_REG_MWDATABE(port, *txBuf);
				return TRUE;
			}

			/* case 2: Write Data Len = 1, use Basic Mode */
			PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
			I3C_SET_REG_MDMACTRL(port,
				I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMATB_MASK);

			/* can't flush Tx FIFO data for DDR */
			/* 2-2. Start DMA */
			PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
			PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

			PDMA_TxBuf_END[pdma_ch] = txBuf[0];

			PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = (((uint32_t) 0)
				<< PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_INC | PDMA_DAR_FIX |
				PDMA_WIDTH_32 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
				(uint32_t)&PDMA_TxBuf_END[pdma_ch];
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA =
				(uint32_t)&(((I3C_T *) I3C_BASE_ADDR(port))->MWDATABE);
		} else {
			/* case 3: Write Data Buffer > 1, use Scatter Gather Mode */
			PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
			I3C_SET_REG_MDMACTRL(port,
				I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMATB_MASK);

			/* can't flush Tx FIFO data for DDR */
			PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
			PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

			PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = PDMA_OP_SCATTER;
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2];

			I3C_DSCT[pdma_ch * 2].CTL = (((uint32_t) txLen - 2)
				<< PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_INC | PDMA_DAR_FIX |
				PDMA_WIDTH_8 | PDMA_OP_SCATTER | PDMA_REQ_SINGLE;
			I3C_DSCT[pdma_ch * 2].ENDSA = (uint32_t)&txBuf[0];

			I3C_DSCT[pdma_ch * 2].ENDDA =
				(uint32_t)&(((I3C_T *)I3C_BASE_ADDR(port))->MWDATAB1);
			I3C_DSCT[pdma_ch * 2].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2 + 1];

			PDMA_TxBuf_END[pdma_ch] = txBuf[txLen - 1];
			I3C_DSCT[pdma_ch * 2 + 1].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos) |
				PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC |
				PDMA_REQ_SINGLE;
			I3C_DSCT[pdma_ch * 2 + 1].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
			I3C_DSCT[pdma_ch * 2 + 1].ENDDA =
				(uint32_t)&(((I3C_T *) I3C_BASE_ADDR(port))->MWDATABE);
		}

		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) | I3C_MDMACTRL_DMATB(2));
		return TRUE;
	}

	/* Prepare for Master read slave's register, or Slave send IBI data */
	if (txLen == 1) {
		if (I3C_GET_REG_STATUS(port) & I3C_STATUS_TXNOTFULL_MASK) {
			I3C_SET_REG_WDATABE(port, *txBuf);
			return TRUE;
		}

		PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
		I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);
		I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

		PDMA_TxBuf_END[pdma_ch] = txBuf[0];

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos)
				| PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC
				| PDMA_REQ_SINGLE;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA =
			(uint32_t) &(((I3C_T *)I3C_BASE_ADDR(port))->WDATABE);
	} else {
		PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
		I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);
		I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = PDMA_OP_SCATTER;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2];

		I3C_DSCT[pdma_ch * 2].CTL = (((uint32_t) txLen - 2) << PDMA_DSCT_CTL_TXCNT_Pos) |
			PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_8 | PDMA_OP_SCATTER |
			PDMA_REQ_SINGLE;
		I3C_DSCT[pdma_ch * 2].ENDSA = (uint32_t)&txBuf[0];
		I3C_DSCT[pdma_ch * 2].ENDDA =
			(uint32_t)&(((I3C_T *)I3C_BASE_ADDR(port))->WDATAB1);
		I3C_DSCT[pdma_ch * 2].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2 + 1];

		PDMA_TxBuf_END[pdma_ch] = txBuf[txLen - 1];

		I3C_DSCT[pdma_ch * 2 + 1].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos) |
			PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC |
			PDMA_REQ_SINGLE;
		I3C_DSCT[pdma_ch * 2 + 1].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
		I3C_DSCT[pdma_ch * 2 + 1].ENDDA =
			(uint32_t)&(((I3C_T *)I3C_BASE_ADDR(port))->WDATABE);
	}

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) | I3C_DMACTRL_DMATB(2));
	return TRUE;
}

_Bool hal_I3C_DMA_Read(I3C_PORT_Enum port, I3C_DEVICE_MODE_Enum mode, __u8 *rxBuf, __u16 rxLen)
{
	__u8 pdma_ch;

	if (rxLen == 0)
		return TRUE;
	if (port >= I3C_PORT_MAX)
		return FALSE;
	if ((mode != I3C_DEVICE_MODE_CURRENT_MASTER) && (mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		(mode != I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		return FALSE;
	}

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);

	if (mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMAFB_MASK);
		I3C_SET_REG_MDATACTRL(port, I3C_GET_REG_MDATACTRL(port) |
			I3C_MDATACTRL_FLUSHFB_MASK);

		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL =
			(((uint32_t)rxLen - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_FIX |
				PDMA_DAR_INC | PDMA_WIDTH_8 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
			(uint32_t)&(((I3C_T *)I3C_BASE_ADDR(port))->MRDATAB);
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA = (uint32_t)&rxBuf[0];

		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) | I3C_MDMACTRL_DMAFB(2));
		return TRUE;
	}

	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMAFB_MASK);
	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHFB_MASK);

	PDMA->CHCTL |= MaskBit(PDMA_OFFSET + pdma_ch);
	PDMA->TDSTS = MaskBit(PDMA_OFFSET + pdma_ch);

	PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL =
		(((uint32_t)rxLen - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_FIX | PDMA_DAR_INC |
			PDMA_WIDTH_8 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
	PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
		(uint32_t)&(((I3C_T *)I3C_BASE_ADDR(port))->RDATAB);
	PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA = (uint32_t)&rxBuf[0];

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) | I3C_DMACTRL_DMAFB(2));
	return TRUE;
}

uint8_t hal_i3c_get_dynamic_address(I3C_PORT_Enum port)
{
	uint32_t temp;

	temp = I3C_GET_REG_DYNADDR(port);

	if ((temp & 0x01) == 0)
		return I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
	return (uint8_t)(temp >> 1);
}

_Bool hal_i3c_get_capability_support_master(I3C_PORT_Enum port)
{
	return TRUE;
}

_Bool hal_i3c_get_capability_support_slave(I3C_PORT_Enum port)
{
	return TRUE;
}

uint16_t hal_i3c_get_capability_Tx_Fifo_Len(I3C_PORT_Enum port)
{
	uint8_t tx_fifo_len_setting;

	tx_fifo_len_setting = (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_FIFOTX_MASK) >>
		I3C_CAPABILITIES_FIFOTX_SHIFT;

	if (tx_fifo_len_setting == 1)
		return 4;
	if (tx_fifo_len_setting == 2)
		return 8;
	if (tx_fifo_len_setting == 3)
		return 16;
	return 0;
}

uint16_t hal_i3c_get_capability_Rx_Fifo_Len(I3C_PORT_Enum port)
{
	uint8_t rx_fifo_len_setting;

	rx_fifo_len_setting = (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_FIFORX_MASK) >>
		I3C_CAPABILITIES_FIFORX_SHIFT;

	if (rx_fifo_len_setting == 1)
		return 4;
	if (rx_fifo_len_setting == 2)
		return 8;
	if (rx_fifo_len_setting == 3)
		return 16;
	return 0;
}

bool hal_i3c_get_capability_support_DMA(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_DMA_MASK) >>
		I3C_CAPABILITIES_DMA_SHIFT;
}

bool hal_i3c_get_capability_support_INT(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_INT_MASK) >>
		I3C_CAPABILITIES_INT_SHIFT;
}

bool hal_i3c_get_capability_support_DDR(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_HDRSUPP_MASK) >>
		I3C_CAPABILITIES_HDRSUPP_SHIFT;
}

bool hal_i3c_get_capability_support_ASYNC0(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_TIMECTRL_MASK) >>
		I3C_CAPABILITIES_TIMECTRL_SHIFT;
}

bool hal_i3c_get_capability_support_IBI(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_IBI_SUPP_MASK) >>
		I3C_CAPABILITIES_IBI_SUPP_SHIFT;
}

bool hal_i3c_get_capability_support_Master_Request(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_MASTER_REQUEST_SUPP_MASK) >>
		I3C_CAPABILITIES_MASTER_REQUEST_SUPP_SHIFT;
}

bool hal_i3c_get_capability_support_Hot_Join(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_CAPABILITIES(port) & I3C_CAPABILITIES_HOTJOIN_SUPP_MASK) >>
		I3C_CAPABILITIES_HOTJOIN_SUPP_SHIFT;
}

bool hal_i3c_get_capability_support_Offline(I3C_PORT_Enum port)
{
	return TRUE;
}

bool hal_i3c_get_capability_support_V10(I3C_PORT_Enum port)
{
	return TRUE;
}

bool hal_i3c_get_capability_support_V11(I3C_PORT_Enum port)
{
	return FALSE;
}

uint8_t hal_I3C_get_event_support_status(I3C_PORT_Enum port)
{
	uint32_t status;

	status = I3C_GET_REG_STATUS(port);
	return (uint8_t)(status >> 24);
}

__u32 hal_I3C_get_status(I3C_PORT_Enum port)
{
	return I3C_GET_REG_STATUS(port);
}

bool hal_I3C_run_ASYN0(I3C_PORT_Enum port)
{
	uint32_t status;

	status = I3C_GET_REG_STATUS(port);
	if ((status & I3C_STATUS_TIMECTRL_MASK) == I3C_STATUS_TIMECTRL(0x01))
		return TRUE;
	return FALSE;
}

/*
 *static void CLK_SysTickDelay(uint32_t us)
 *{
 *	k_usleep(us);
 *}
 */

I3C_ErrCode_Enum hal_I3C_Process_Task(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_PROTOCOL_Enum protocol;
	I3C_TRANSFER_FRAME_t *pFrame;
	I3C_DEVICE_INFO_t *pDevice;
	uint32_t mctrl;
	uint16_t rdterm;
	I3C_BUS_INFO_t *pBus;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;
	if (pTaskInfo->Port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pTask = pTaskInfo->pTask;

	if (pTask == NULL)
		return I3C_ERR_PARAMETER_INVALID;
	if (pTask->frame_idx >= pTask->frame_count)
		return I3C_ERR_PARAMETER_INVALID;

	protocol = pTask->protocol;
	pFrame = &pTask->pFrameList[pTask->frame_idx];
	pDevice = api_I3C_Get_Current_Master_From_Port(pTaskInfo->Port);

	mctrl = I3C_MCTRL_ADDR(pFrame->address) | I3C_MCTRL_DIR(pFrame->direction) |
		I3C_MCTRL_TYPE(pFrame->type) | I3C_MCTRL_IBIRESP(3);

	I3C_SET_REG_MSTATUS(pTaskInfo->Port, I3C_MSTATUS_MCTRLDONE_MASK);
	I3C_SET_REG_MINTSET(pTaskInfo->Port, I3C_GET_REG_MINTSET(pTaskInfo->Port) |
		I3C_MINTSET_MCTRLDONE_MASK);

	if (protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
		mctrl |= I3C_MCTRL_REQUEST(I3C_MCTRL_REQUEST_DO_ENTDAA);
		if (pTask->frame_idx != 0)
			I3C_SET_REG_MWDATAB1(pTaskInfo->Port, pFrame->access_buf[8]);
	} else {
		/* !!! Don't setup Rx DMA in MCTRLDONE, because RX fifo will ORUN */
		if (pFrame->direction == I3C_TRANSFER_DIR_READ) {
			if (protocol == I3C_TRANSFER_PROTOCOL_EVENT) {
				/* ack IBI with IBIRULE to prevent 100us TIMEOUT */
				mctrl &= ~I3C_MCTRL_IBIRESP_MASK;
				rdterm = (pFrame->access_len - pFrame->access_idx);
			} else if (pFrame->type == I3C_TRANSFER_TYPE_DDR) {
				rdterm = 2 + (pFrame->access_len - pFrame->access_idx) / 2;
				if (rdterm <= I3C_MCTRL_RDTERM_MAX)
					mctrl |= I3C_MCTRL_RDTERM(rdterm);
			} else {
				rdterm = pFrame->access_len - pFrame->access_idx;
				if (rdterm <= I3C_MCTRL_RDTERM_MAX)
					mctrl |= I3C_MCTRL_RDTERM(rdterm);
			}

			if ((I3C_GET_REG_MDMACTRL(pTaskInfo->Port) & I3C_MDMACTRL_DMAFB_MASK) == 0)
				api_I3C_Setup_Master_Read_DMA(pDevice);
		}

		mctrl |= I3C_MCTRL_REQUEST(I3C_MCTRL_REQUEST_EMIT_START);
	}

	if ((pDevice->bAbort == FALSE) || (protocol == I3C_TRANSFER_PROTOCOL_EVENT)) {
		I3C_SetXferRate(pTaskInfo);
		I3C_SET_REG_MCTRL(pTaskInfo->Port, mctrl);
		return I3C_ERR_PENDING;
	} else if (pDevice->bAbort) {
		/* flush HDRCMD */
		I3C_SET_REG_MDATACTRL(pTaskInfo->Port, I3C_GET_REG_MDATACTRL(pTaskInfo->Port) |
			I3C_MDATACTRL_FLUSHTB_MASK);
		I3C_SET_REG_MDMACTRL(pTaskInfo->Port, I3C_GET_REG_MDMACTRL(pTaskInfo->Port) &
			~(I3C_MDMACTRL_DMAFB_MASK | I3C_MDMACTRL_DMATB_MASK));

		pBus = pDevice->pOwner;
		pBus->pCurrentTask = NULL;
		pDevice->bAbort = FALSE;
		return I3C_ERR_SLVSTART;
	}

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Stop(I3C_PORT_Enum port)
{
	uint32_t mctrl;
	uint32_t mstatus;
	uint32_t mconfig;

	while ((I3C_GET_REG_MCTRL(port) & I3C_MCTRL_REQUEST_MASK) != I3C_MCTRL_REQUEST_NONE) {
	}

	mctrl = I3C_GET_REG_MCTRL(port);
	mstatus = I3C_GET_REG_MSTATUS(port);

	if ((mctrl & I3C_MCTRL_TYPE_MASK) == I3C_MCTRL_TYPE(I3C_TRANSFER_TYPE_I2C)) {
		mconfig = I3C_GET_REG_MCONFIG(port);
		mconfig |= I3C_MCONFIG_ODSTOP_MASK;
		I3C_SET_REG_MCONFIG(port, mconfig);
	}

	if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DDR)
		mctrl |= I3C_MCTRL_REQUEST_EXIT_DDR;
	else
		mctrl |= I3C_MCTRL_REQUEST_EMIT_STOP;

	I3C_SET_REG_MINTCLR(port, I3C_MINTCLR_MCTRLDONE_MASK);
	I3C_SET_REG_MCTRL(port, mctrl);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Start_IBI(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	uint32_t ctrl;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	port = pTaskInfo->Port;
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = api_I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	pTask = pTaskInfo->pTask;
	pFrame = &pTask->pFrameList[pTask->frame_idx];

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl &= ~(I3C_CTRL_IBIDATA_MASK | I3C_CTRL_EXTDATA_MASK | I3C_CTRL_EVENT_MASK);

	/* BCR[2] = 1, then set Mandatory data byte */
	if (pDevice->bcr & 0x04) {
		ctrl |= I3C_CTRL_IBIDATA(pFrame->access_buf[0]);
		pFrame->access_idx++;

		/* Extended IBI data */
		if (((pFrame->access_len - pFrame->access_idx) == 0) ||
			((pFrame->access_len - pFrame->access_idx) > 7)) {
			ctrl |= I3C_CTRL_EXTDATA(0);
			I3C_SET_REG_IBIEXT1(port, I3C_IBIEXT1_MAX(0));
			api_I3C_Setup_Slave_IBI_DMA(pDevice);
		} else {
			ctrl |= I3C_CTRL_EXTDATA(1);

			/* MAX = data len, CNT = 0 -> use TX FIFO */
			I3C_SET_REG_IBIEXT1(port,
				I3C_IBIEXT1_MAX(pFrame->access_len - pFrame->access_idx));
			api_I3C_Setup_Slave_IBI_DMA(pDevice);
		}
	}

	ctrl |= I3C_CTRL_EVENT(1);
	I3C_SET_REG_CTRL(port, ctrl);

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Start_Master_Request(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	__u8 pdma_ch;
	uint32_t ctrl;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	port = pTaskInfo->Port;
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = api_I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);
	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & I3C_DMACTRL_DMATB_MASK);
	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

	pDevice->txLen = 0;
	pDevice->txOffset = 0;
	pDevice->rxLen = 0;
	pDevice->rxOffset = 0;

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl |= I3C_CTRL_EVENT(2);
	I3C_SET_REG_CTRL(port, ctrl);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Start_HotJoin(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	__u8 pdma_ch;
	uint32_t ctrl;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	port = pTaskInfo->Port;
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = api_I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);
	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pdma_ch);
	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);
	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

	pDevice->txLen = 0;
	pDevice->txOffset = 0;
	pDevice->rxLen = 0;
	pDevice->rxOffset = 0;

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl &= ~(I3C_CTRL_IBIDATA_MASK | I3C_CTRL_EXTDATA_MASK | I3C_CTRL_EVENT_MASK);
	ctrl |= I3C_CTRL_EVENT(3);

	I3C_SET_REG_CONFIG(port, I3C_GET_REG_CONFIG(port) & ~I3C_CONFIG_SLVENA_MASK);
	I3C_SET_REG_CTRL(port, ctrl);
	I3C_SET_REG_CONFIG(port, I3C_GET_REG_CONFIG(port) | I3C_CONFIG_SLVENA_MASK);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Slave_TX_Free(I3C_PORT_Enum port)
{
	I3C_DEVICE_INFO_t *pDevice;
	uint16_t txDataLen;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = api_I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	txDataLen = (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_TXCOUNT_MASK) >>
		I3C_DATACTRL_TXCOUNT_SHIFT;
	return (txDataLen == 0) ? I3C_ERR_OK : I3C_ERR_PENDING;
}

/* return the amount of tx data has not send out */
I3C_ErrCode_Enum hal_I3C_Slave_Query_TxLen(I3C_PORT_Enum port, uint16_t *txLen)
{
	I3C_DEVICE_INFO_t *pDevice;
	uint16_t txDataLen;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;
	if (txLen == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = api_I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	txDataLen = (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_TXCOUNT_MASK) >>
		I3C_DATACTRL_TXCOUNT_SHIFT;

	*txLen = txDataLen;
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Stop_Slave_TX(I3C_DEVICE_INFO_t *pDevice)
{
	uint32_t dmactrl = I3C_GET_REG_DMACTRL(pDevice->port);
	uint32_t datactrl = I3C_GET_REG_DATACTRL(pDevice->port);

	dmactrl &= ~I3C_DMACTRL_DMATB_MASK;
	I3C_SET_REG_DMACTRL(pDevice->port, dmactrl);

	PDMA->TDSTS = MaskBit(PDMA_OFFSET + pDevice->port);
	PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + pDevice->port);

	datactrl |= I3C_MDATACTRL_FLUSHFB_MASK;
	I3C_SET_REG_DATACTRL(pDevice->port, datactrl);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Set_Pending(I3C_PORT_Enum port, __u8 mask)
{
	uint32_t ctrl;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl &= ~I3C_CTRL_PENDINT_MASK;
	ctrl |= I3C_CTRL_PENDINT(mask);
	I3C_SET_REG_CTRL(port, ctrl);
	return I3C_ERR_OK;
}

void *hal_I3C_MemAlloc(__u32 Size)
{
	return k_malloc(Size);
}

void hal_I3C_MemFree(void *pv)
{
	k_free(pv);
}

/* static uint32_t tIMG = 50; */
void hal_I3C_Master_Stall(I3C_BUS_INFO_t *pBus, I3C_PORT_Enum port)
{
	/*CLK_SysTickDelay(tIMG);*/
}

I3C_ErrCode_Enum hal_I3C_bus_reset(I3C_PORT_Enum Port)
{
	if (Port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_bus_clear(I3C_PORT_Enum Port, uint8_t counter)
{
	if (Port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	return I3C_ERR_OK;
}

/*==========================================================================*/

#define I3CG_REG1(x)			((x * 0x10) + 0x14)
#define SDA_OUT_SW_MODE_EN		BIT(31)
#define SCL_OUT_SW_MODE_EN		BIT(30)
#define SDA_IN_SW_MODE_EN		BIT(29)
#define SCL_IN_SW_MODE_EN		BIT(28)
#define SDA_IN_SW_MODE_VAL		BIT(27)
#define SDA_OUT_SW_MODE_VAL		BIT(25)
#define SDA_SW_MODE_OE			BIT(24)
#define SCL_IN_SW_MODE_VAL		BIT(23)
#define SCL_OUT_SW_MODE_VAL		BIT(21)
#define SCL_SW_MODE_OE			BIT(20)

void i3c_npcm4xx_isolate_scl_sda(int inst_id, bool iso)
{
}

void i3c_npcm4xx_gen_tbits_low(struct i3c_npcm4xx_obj *obj)
{
}

void i3c_npcm4xx_toggle_scl_in(int inst_id)
{
}

void i3c_npcm4xx_gen_start_to_internal(int inst_id)
{
}

void i3c_npcm4xx_gen_stop_to_internal(int inst_id)
{
}

static int i3c_npcm4xx_init(const struct device *dev);

static uint8_t *pec_append(const struct device *dev, uint8_t *ptr, uint16_t len)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port;
	uint8_t *xfer_buf;
	uint8_t pec_v;
	uint8_t addr_rnw;

	port = config->inst_id;
	addr_rnw = (uint8_t)(I3C_GET_REG_DYNADDR(port) << 1) | 0x1;
	xfer_buf = k_malloc(len + 1);
	memcpy(xfer_buf, ptr, len);

	pec_v = crc8_ccitt(0, &addr_rnw, 1);
	pec_v = crc8_ccitt(pec_v, xfer_buf, len);
	LOG_DBG("pec = %x", pec_v);
	xfer_buf[len] = pec_v;

	return xfer_buf;
}

static int pec_valid(const struct device *dev, uint8_t *ptr, uint16_t len)
{
	struct i3c_npcm4xx_config *config;
	uint8_t pec_v;
	uint8_t address;
	uint8_t addr_rnw;
	int ret;

	if (len == 0 || ptr == NULL)
		return -EINVAL;

	config = DEV_CFG(dev);

	ret = i3c_slave_get_dynamic_addr(dev, &address);
	if (ret)
		return -EINVAL;

	addr_rnw = address << 1;

	pec_v = crc8_ccitt(0, &addr_rnw, 1);
	pec_v = crc8_ccitt(pec_v, ptr, len - 1);
	LOG_DBG("pec = %x %x", pec_v, ptr[len - 1]);
	return (pec_v == ptr[len - 1]) ? 0 : -EIO;
}

/**
 * @brief validate slave device exist in the bus ?
 * @param obj pointer to the bus controller
 * @param addr target address
 * @return
 */
static int i3c_npcm4xx_get_pos(struct i3c_npcm4xx_obj *obj, uint8_t addr)
{
	int pos;
	int maxdevs = DEVICE_COUNT_MAX;

	for (pos = 0; pos < maxdevs; pos++) {
		if (addr == obj->dev_addr_tbl[pos]) {
			return pos;
		}
	}

	return -1;
}

static void i3c_npcm4xx_wr_tx_fifo(struct i3c_npcm4xx_obj *obj, uint8_t *bytes, int nbytes)
{
	struct i3c_npcm4xx_config *config;
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_ErrCode_Enum ret;

	config = obj->config;
	port = config->inst_id;
	pDevice = api_I3C_Get_INODE(port);

	ret = api_I3C_Slave_Prepare_Response(pDevice, nbytes, bytes);
}

static void i3c_npcm4xx_start_xfer(struct i3c_npcm4xx_obj *obj, struct i3c_npcm4xx_xfer *xfer)
{
	struct i3c_npcm4xx_config *config;
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_BUS_INFO_t *pBus;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TASK_INFO_t *pTaskInfo;
	k_spinlock_key_t key;

	config = obj->config;
	port = config->inst_id;
	pDevice = api_I3C_Get_INODE(port);
	pBus = api_I3C_Get_Bus_From_Port(port);

	while (pDevice->pTaskListHead != NULL) {
		if (pBus->pCurrentTask == NULL) {
			pTask = pDevice->pTaskListHead;
			pBus->pCurrentTask = pTask;

			key = k_spin_lock(&obj->lock);
			obj->curr_xfer = xfer;

			pTaskInfo = pTask->pTaskInfo;
			if (pTaskInfo->MasterRequest)
				I3C_Master_Start_Request((__u32)pTaskInfo);
			else
				I3C_Slave_Start_Request((__u32)pTaskInfo);

			k_spin_unlock(&obj->lock, key);

			/* wait until current task complete */
			while (pBus->pCurrentTask)
				k_usleep(0);
			return;
		}
	}
}

/**
 * @brief Build i3c_dev_desc and add connection to bus controller
 * @param dev
 * @param slave
 * @return
 */
int i3c_npcm4xx_master_attach_device(const struct device *dev, struct i3c_dev_desc *slave)
{
	struct i3c_npcm4xx_obj *obj = NULL;
	struct i3c_npcm4xx_config *config = NULL;
	I3C_PORT_Enum port;

	struct i3c_npcm4xx_dev_priv *priv;
	int i, pos;
	I3C_BUS_INFO_t *pBus = NULL;
	I3C_DEVICE_INFO_t *pDevice = NULL;
	I3C_DEVICE_INFO_t *pDeviceSlv = NULL;
	I3C_DEVICE_INFO_SHORT_t *pDevInfo = NULL;

	/* allocate private data of the device */
	priv = (struct i3c_npcm4xx_dev_priv *)k_calloc(sizeof(struct i3c_npcm4xx_dev_priv), 1);
	__ASSERT(priv, "failed to allocat device private data\n");

	priv->pos = -1;
	priv->ibi.enable = FALSE;
	priv->ibi.callbacks = NULL;
	priv->ibi.context = slave;
	priv->ibi.incomplete = NULL;
	slave->priv_data = priv;

	if (slave->info.i2c_mode) {
		slave->info.dynamic_addr = slave->info.static_addr;
	} else if (slave->info.assigned_dynamic_addr) {
		slave->info.dynamic_addr = slave->info.assigned_dynamic_addr;
	}

	/* master might not exist in devicetree */
	/* but try to get bus from bus master first */
	if (dev != NULL) {
		obj = DEV_DATA(dev);
		config = obj->config;
		port = config->inst_id;
		pDevice = api_I3C_Get_INODE(port);
		if (pDevice == NULL)
			return -ENODEV;
		pBus = pDevice->pOwner;

		/* assign dev as slave's bus controller */
		slave->bus = dev;

		/* find a free position from master's hw_dat_free_pos */
		for (i = 0; i < DEVICE_COUNT_MAX; i++) {
			if (obj->hw_dat_free_pos & BIT(i))
				break;
		}

		/* can't attach if no free space */
		if (i == DEVICE_COUNT_MAX)
			return i;

		pos = i3c_npcm4xx_get_pos(obj, slave->info.dynamic_addr);
		if (pos >= 0) {
			LOG_WRN("addr %x has been registered at %d\n",
				slave->info.dynamic_addr, pos);
			return pos;
		}

		priv->pos = i;

		obj->dev_addr_tbl[i] = slave->info.dynamic_addr;
		obj->dev_descs[i] = slave;
		obj->hw_dat_free_pos &= ~BIT(i);
	} else {
		/* slave device must be internal device, and match pid */
		for (i = 0; i < I3C_PORT_MAX; i++) {
			pDeviceSlv = api_I3C_Get_INODE(i);
			if (pDeviceSlv->pid[5] != (uint8_t) slave->info.pid)
				continue;
			if (pDeviceSlv->pid[4] != (uint8_t)(slave->info.pid >> 8))
				continue;
			if (pDeviceSlv->pid[3] != (uint8_t)(slave->info.pid >> 16))
				continue;
			if (pDeviceSlv->pid[2] != (uint8_t)(slave->info.pid >> 24))
				continue;
			if (pDeviceSlv->pid[1] != (uint8_t)(slave->info.pid >> 32))
				continue;
			if (pDeviceSlv->pid[0] != (uint8_t)(slave->info.pid >> 48))
				continue;

			pDevInfo = pDeviceSlv->pDevInfo;
			break;
		}

		if (pDevInfo == NULL)
			return -ENODEV;
		pBus = pDeviceSlv->pOwner;

		/* bus owner outside the devicetree */
		slave->bus = NULL;
	}

	if (pBus == NULL)
		return -ENXIO;

	if (slave->bus == NULL)
		return 0;

	if ((slave->info.static_addr == 0x6A) || (slave->info.static_addr == 0x6B)) {
		LSM6DSO_DEVICE_INFO_t lsm6dso;
		I3C_DEVICE_ATTRIB_t attr;

		lsm6dso.initMode = INITMODE_RSTDAA | INITMODE_SETDASA;
		lsm6dso.state = I3C_LSM6DSO_STATE_DEFAULT;
		lsm6dso.post_init_state = LSM6DSO_POST_INIT_STATE_Default;

		lsm6dso.i3c_device.mode = I3C_DEVICE_MODE_SLAVE_ONLY;
		lsm6dso.i3c_device.pOwner = pBus;
		lsm6dso.i3c_device.port = port;
		lsm6dso.i3c_device.bRunI3C = FALSE;
		lsm6dso.i3c_device.staticAddr = slave->info.static_addr;
		lsm6dso.i3c_device.dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
		lsm6dso.i3c_device.pid[0] = 0x02;
		lsm6dso.i3c_device.pid[1] = 0x08;
		lsm6dso.i3c_device.pid[2] = 0x00;
		lsm6dso.i3c_device.pid[3] = 0x6C;
		lsm6dso.i3c_device.pid[4] = 0x10;
		lsm6dso.i3c_device.pid[5] = 0x0B;
		lsm6dso.i3c_device.bcr = 0x07;
		lsm6dso.i3c_device.dcr = 0x44;
		lsm6dso.i3c_device.ackIBI = FALSE;
		lsm6dso.i3c_device.pReg = NULL;
		lsm6dso.i3c_device.regCnt = 0;

		attr.U16 = 0;
		attr.b.suppSLV = 1;
		attr.b.present = 1;
		attr.b.runI3C = 0;

		if (lsm6dso.initMode & INITMODE_POST_INIT)
			attr.b.reqPostInit = 0;
		if (lsm6dso.initMode & INITMODE_RSTDAA)
			attr.b.reqRSTDAA = 1;
		if (lsm6dso.initMode & INITMODE_SETDASA)
			attr.b.reqSETDASA = 1;
		if (lsm6dso.initMode & INITMODE_ENTDAA)
			attr.b.suppENTDAA = 1;

		lsm6dso.i3c_device.pDevInfo = NewDevInfo(pBus, &lsm6dso, attr,
			lsm6dso.i3c_device.staticAddr, lsm6dso.i3c_device.dynamicAddr,
			lsm6dso.i3c_device.pid, lsm6dso.i3c_device.bcr, lsm6dso.i3c_device.dcr);
		if (lsm6dso.i3c_device.pDevInfo == NULL)
			return -ENXIO;
	} else if ((slave->info.static_addr >= 0x50) && (slave->info.static_addr <= 0x57)) {
		SPD5118_DEVICE_INFO_t spd5118;
		I3C_DEVICE_ATTRIB_t attr;

		spd5118.initMode = INITMODE_RSTDAA | INITMODE_SETAASA;
		spd5118.state = SPD5118_STATE_DEFAULT;
		spd5118.post_init_state = SPD5118_POST_INIT_STATE_Default;

		spd5118.i3c_device.mode = I3C_DEVICE_MODE_SLAVE_ONLY;
		spd5118.i3c_device.pOwner = pBus;
		spd5118.i3c_device.port = port;
		spd5118.i3c_device.bRunI3C = FALSE;
		spd5118.i3c_device.staticAddr = slave->info.static_addr;
		spd5118.i3c_device.dynamicAddr = I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
		spd5118.i3c_device.pid[0] = 0x00;
		spd5118.i3c_device.pid[1] = 0x00;
		spd5118.i3c_device.pid[2] = 0x00;
		spd5118.i3c_device.pid[3] = 0x00;
		spd5118.i3c_device.pid[4] = 0x00;
		spd5118.i3c_device.pid[5] = 0x00;
		spd5118.i3c_device.bcr = 0x00;
		spd5118.i3c_device.dcr = 0x00;
		spd5118.i3c_device.ackIBI = FALSE;
		spd5118.i3c_device.pReg = NULL;
		spd5118.i3c_device.regCnt = 0;

		attr.U16 = 0;
		attr.b.suppSLV = 1;
		attr.b.present = 1;
		attr.b.runI3C = 0;

		if (spd5118.initMode & INITMODE_POST_INIT)
			attr.b.reqPostInit = 0;
		if (spd5118.initMode & INITMODE_RSTDAA)
			attr.b.reqRSTDAA = 1;
		if (spd5118.initMode & INITMODE_SETAASA)
			attr.b.reqSETAASA = 1;
		if (spd5118.initMode & INITMODE_ENTDAA)
			attr.b.suppENTDAA = 1;

		spd5118.i3c_device.pDevInfo = NewDevInfo(pBus, &spd5118, attr,
			spd5118.i3c_device.staticAddr, spd5118.i3c_device.dynamicAddr,
			spd5118.i3c_device.pid, spd5118.i3c_device.bcr, spd5118.i3c_device.dcr);
		if (spd5118.i3c_device.pDevInfo == NULL)
			return -ENXIO;
	}

	return 0;
}

int i3c_npcm4xx_master_detach_device(const struct device *dev, struct i3c_dev_desc *slave)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(dev);
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(slave);
	int pos;

	pos = priv->pos;
	if (pos < 0) {
		return pos;
	}

	obj->hw_dat_free_pos |= BIT(pos);
	obj->dev_addr_tbl[pos] = 0;

	k_free(slave->priv_data);
	obj->dev_descs[pos] = (struct i3c_dev_desc *) NULL;
	return 0;
}

int i3c_npcm4xx_master_request_ibi(struct i3c_dev_desc *i3cdev, struct i3c_ibi_callbacks *cb)
{
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(i3cdev);

	priv->ibi.callbacks = cb;
	priv->ibi.context = i3cdev;
	priv->ibi.incomplete = NULL;

	return 0;
}

int i3c_npcm4xx_master_enable_ibi(struct i3c_dev_desc *i3cdev)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(i3cdev->bus);
	/* struct i3c_register_s *i3c_register = obj->config->base; */
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(i3cdev);
	/* union i3c_dev_addr_tbl_s dat; */
	/* union i3c_intr_s intr_reg; */
	/* uint32_t dat_addr, sir_reject; */
	int pos = 0;

	/* pos should be assigned in i3c_master_attach_device() */
	pos = priv->pos;
	if (pos < 0) {
		return pos;
	}

	/* let master accept IBI
	 *
	 * sir_reject = i3c_register->sir_reject;
	 * sir_reject &= ~BIT(pos);
	 * i3c_register->sir_reject = sir_reject;
	 */

	/* update master's setting for slave device's BCR */
	/* dat_addr = (uint32_t)obj->config->base + obj->hw_dat.fields.start_addr + (pos << 2); */
	/* dat.value = sys_read32(dat_addr); */
	/* if (i3cdev->info.bcr & I3C_BCR_IBI_PAYLOAD) dat.fields.ibi_with_data = 1; */
	/* if (obj->hw_feature.ibi_pec_force_enable) dat.fields.ibi_pec_en = 1; */
	/* dat.fields.sir_reject = 0; */
	/* sys_write32(dat.value, dat_addr); */


	/* update IBIRULES to receive MDB within 100us */
	/* at most 5 slave devices */
	/* address[7] must be 0 ? */
	I3C_PORT_Enum port;
	uint32_t ibirules;

	port = obj->config->inst_id;
	ibirules = I3C_GET_REG_IBIRULES(port);
	switch (pos) {
	case 0:
		ibirules &= ~I3C_IBIRULES_ADDR0_MASK;
		ibirules |= I3C_IBIRULES_ADDR0(i3cdev->info.dynamic_addr & 0x3F);
		break;
	case 1:
		ibirules &= ~I3C_IBIRULES_ADDR1_MASK;
		ibirules |= I3C_IBIRULES_ADDR1(i3cdev->info.dynamic_addr & 0x3F);
		break;
	case 2:
		ibirules &= ~I3C_IBIRULES_ADDR2_MASK;
		ibirules |= I3C_IBIRULES_ADDR2(i3cdev->info.dynamic_addr & 0x3F);
		break;
	case 3:
		ibirules &= ~I3C_IBIRULES_ADDR3_MASK;
		ibirules |= I3C_IBIRULES_ADDR3(i3cdev->info.dynamic_addr & 0x3F);
		break;
	case 4:
		ibirules &= ~I3C_IBIRULES_ADDR4_MASK;
		ibirules |= I3C_IBIRULES_ADDR4(i3cdev->info.dynamic_addr & 0x3F);
		break;
	default:
		LOG_WRN("Can't set address to IBIRULES register !!!");
		return -E2BIG;
	}

	I3C_SET_REG_IBIRULES(port, ibirules);

	priv->ibi.enable = 1;

	/* enable ibi interrupt and slave_start detect ?
	 *
	 * intr_reg.value = i3c_register->intr_status_en.value;
	 * intr_reg.fields.ibi_thld = 1;
	 * i3c_register->intr_status_en.value = intr_reg.value;
	 *
	 * intr_reg.value = i3c_register->intr_signal_en.value;
	 * intr_reg.fields.ibi_thld = 1;
	 * i3c_register->intr_signal_en.value = intr_reg.value;
	 */

	return i3c_master_send_enec(i3cdev->bus, i3cdev->info.dynamic_addr, I3C_CCC_EVT_SIR);
}

int i3c_npcm4xx_slave_register(const struct device *dev, struct i3c_slave_setup *slave_data)
{
	struct i3c_npcm4xx_obj *obj;

	obj = DEV_DATA(dev);

	__ASSERT(slave_data->max_payload_len <= I3C_PAYLOAD_SIZE_MAX,
		"msg_size should less than %d.\n", I3C_PAYLOAD_SIZE_MAX);

	obj->slave_data.max_payload_len = slave_data->max_payload_len;
	obj->slave_data.callbacks = slave_data->callbacks;
	obj->slave_data.dev = slave_data->dev;
	return 0;
}

/*
 * slave send mdb
 */
int i3c_npcm4xx_slave_put_read_data(const struct device *dev, struct i3c_slave_payload *data,
	struct i3c_ibi_payload *ibi_notify)
{
	struct i3c_npcm4xx_config *config;
	struct i3c_npcm4xx_obj *obj;
	I3C_PORT_Enum port;
	uint32_t event_en;
	int ret;
	uint8_t *xfer_buf;
	I3C_DEVICE_INFO_t *pDevice;

	__ASSERT_NO_MSG(data);
	__ASSERT_NO_MSG(data->buf);
	__ASSERT_NO_MSG(data->size);

	config = DEV_CFG(dev);
	port = config->inst_id;
	pDevice = api_I3C_Get_INODE(port);

	k_mutex_lock(&pDevice->lock, K_FOREVER);

	obj = DEV_DATA(dev);

	if (config->priv_xfer_pec) {
	/*
	 *	uint8_t pec_v;
	 *	uint8_t addr_rnw;
	 *
	 *	addr_rnw = (uint8_t)I3C_GET_REG_DYNADDR(port) >> 1;
	 *	pec_v = crc8_ccitt(0, &addr_rnw, 1);
	 *
	 *	xfer_buf = (uint8_t *)&data->buf[1];
	 *	pec_v = crc8_ccitt(pec_v, xfer_buf, data->size - 1);
	 *	LOG_DBG("pec = %x", pec_v);
	 *	xfer_buf = (uint8_t *)&data->buf[0];
	 *	xfer_buf[data->size] = pec_v;
	 *	i3c_npcm4xx_wr_tx_fifo(obj, data->buf, data->size + 1);
	 */
	} else {
		i3c_npcm4xx_wr_tx_fifo(obj, data->buf, data->size);
	}

	if (ibi_notify) {
		if (obj->sir_allowed_by_sw == 0) {
			LOG_ERR("SIR is not allowed by software\n");
			k_mutex_unlock(&pDevice->lock);
			return -EACCES;
		}

		__ASSERT(ibi_notify->size == 1, "IBI Notify data length Fail !!!\n\n");

		ret = i3c_slave_get_event_enabling(dev, &event_en);
		if (ret || !(event_en & I3C_SLAVE_EVENT_SIR)) {
			/* master should polling pending interrupt by GetSTATUS */
			api_I3C_Slave_Update_Pending(pDevice, 0x01);
			k_mutex_unlock(&pDevice->lock);
			return 0;
		}

		/* init ibi complete sem */
		k_sem_init(&pDevice->ibi_complete, 0, 1);

		/* osEventFlagsClear(obj->ibi_event, ~osFlagsError); */

		__u16 txlen;
		__u16 rxlen = 0;
		__u8 TxBuf[2];
		I3C_TRANSFER_PROTOCOL_Enum protocol = I3C_TRANSFER_PROTOCOL_IBI;
		__u32 timeout = TIMEOUT_TYPICAL;

		txlen = (uint16_t)ibi_notify->size;	 /* ibi_notify->size >= 0 */
		TxBuf[0] = ibi_notify->buf[0];       /* MDB */

		if (config->ibi_append_pec) {
			xfer_buf = pec_append(dev, ibi_notify->buf, ibi_notify->size);
			txlen = 2;
			/* i3c_npcm4xx_wr_tx_fifo(obj, xfer_buf, ibi_notify->size + 1); */
			/* k_free(xfer_buf); */
		} else {
			txlen = 1;
			/* i3c_npcm4xx_wr_tx_fifo(obj, ibi_notify->buf, ibi_notify->size); */
		}

		/* let slave drive SLVSTART until bus idle */
		api_I3C_Slave_Create_Task(protocol, txlen, &txlen, &rxlen, TxBuf, NULL,
			timeout, NULL, port, NOT_HIF);
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[port], &work_send_ibi[port]);

		/* wait ibi master read complete done */
		k_sem_take(&pDevice->ibi_complete, K_FOREVER);
	}

	/*
	 * osEventFlagsClear(obj->data_event, ~osFlagsError);
	 * if (config->priv_xfer_pec) {
	 *   xfer_buf = pec_append(dev, data->buf, data->size);
	 *   i3c_npcm4xx_wr_tx_fifo(obj, xfer_buf, data->size + 1);
	 *   k_free(xfer_buf);
	 * } else {
	 *   i3c_npcm4xx_wr_tx_fifo(obj, data->buf, data->size);
	 * }
	 */

	k_mutex_unlock(&pDevice->lock);

	return 0;
}

int i3c_npcm4xx_slave_send_sir(const struct device *dev, struct i3c_ibi_payload *payload)
{
	return -ENOSYS;
}

int i3c_npcm4xx_slave_get_dynamic_addr(const struct device *dev, uint8_t *dynamic_addr)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port;
	uint32_t addr;

	port = config->inst_id;
	addr = I3C_GET_REG_DYNADDR(port);
	if ((addr & I3C_DYNADDR_DAVALID_MASK) == 0)
		return -1;

	*dynamic_addr = (uint8_t)(addr >> 1);
	return 0;
}

int i3c_npcm4xx_slave_get_event_enabling(const struct device *dev, uint32_t *event_en)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port;
	uint32_t status;

	port = config->inst_id;
	status = I3C_GET_REG_STATUS(port);

	*event_en = 0;
	if ((status & I3C_STATUS_IBIDIS_MASK) == 0)
		*event_en |= I3C_SLAVE_EVENT_SIR;
	if ((status & I3C_STATUS_MRDIS_MASK) == 0)
		*event_en |= I3C_SLAVE_EVENT_MR;
	if ((status & I3C_STATUS_HJDIS_MASK) == 0)
		*event_en |= I3C_SLAVE_EVENT_HJ;

	return 0;
}

static void i3c_npcm4xx_master_rx_ibi(struct i3c_npcm4xx_obj *obj)
{
	struct i3c_dev_desc *i3cdev;
	struct i3c_npcm4xx_dev_priv *priv;
	struct i3c_ibi_payload *payload;
	uint32_t pos;

	I3C_PORT_Enum port;
	uint8_t ibi_addr;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;

	port = obj->config->inst_id;

	ibi_addr = hal_I3C_get_ibiAddr(port);
	pos = i3c_npcm4xx_get_pos(obj, ibi_addr);
	if (pos < 0) {
		LOG_ERR("unregistered IBI source: 0x%x\n", ibi_addr);
		return;
	}

	i3cdev = obj->dev_descs[pos];
	priv = DESC_PRIV(i3cdev);

	/* get return data structure, payload from private data */
	payload = priv->ibi.callbacks->write_requested(priv->ibi.context);

	pDevice = api_I3C_Get_INODE(port);
	pTask = pDevice->pTaskListHead;
	payload->size = *pTask->pRdLen;
	memcpy(payload->buf, pTask->pRdBuf, payload->size);

	/* notify, ibi task has been processed */
	priv->ibi.callbacks->write_done(priv->ibi.context);
}

union i3c_device_cmd_queue_port_s {
	volatile uint32_t value;

#define COMMAND_PORT_XFER_CMD		0x0
#define COMMAND_PORT_XFER_ARG		0x1
#define COMMAND_PORT_SHORT_ARG		0x2
#define COMMAND_PORT_ADDR_ASSIGN	0x3
#define COMMAND_PORT_SLAVE_DATA_CMD	0x0

#define COMMAND_PORT_SPEED_I3C_SDR	0
#define COMMAND_PORT_SPEED_I3C_I2C_FM	7
#define COMMAND_PORT_SPEED_I2C_FM	0
#define COMMAND_PORT_SPEED_I2C_FMP	1
	struct {
		volatile uint32_t cmd_attr :3; /* bit[2:0] */
		volatile uint32_t tid :4; /* bit[6:3] */
		volatile uint32_t cmd :8; /* bit[14:7] */
		volatile uint32_t cp :1; /* bit[15] */
		volatile uint32_t dev_idx :5; /* bit[20:16] */
		volatile uint32_t speed :3; /* bit[23:21] */
		volatile uint32_t reserved0 :2; /* bit[25:24] */
		volatile uint32_t roc :1; /* bit[26] */
		volatile uint32_t sdap :1; /* bit[27] */
		volatile uint32_t rnw :1; /* bit[28] */
		volatile uint32_t reserved1 :1; /* bit[29] */
		volatile uint32_t toc :1; /* bit[30] */
		volatile uint32_t pec :1; /* bit[31] */
	} xfer_cmd;

	struct {
		volatile uint32_t cmd_attr :3; /* bit[2:0] */
		volatile uint32_t reserved0 :5; /* bit[7:3] */
		volatile uint32_t db :8; /* bit[15:8] */
		volatile uint32_t dl :16; /* bit[31:16] */
	} xfer_arg;

	struct {
		volatile uint32_t cmd_attr :3; /* bit[2:0] */
		volatile uint32_t byte_strb :3; /* bit[5:3] */
		volatile uint32_t reserved0 :2; /* bit[7:6] */
		volatile uint32_t db0 :8; /* bit[15:8] */
		volatile uint32_t db1 :8; /* bit[23:16] */
		volatile uint32_t db2 :8; /* bit[31:24] */
	} short_data_arg;

	struct {
		volatile uint32_t cmd_attr :3; /* bit[2:0] */
		volatile uint32_t tid :4; /* bit[6:3] */
		volatile uint32_t cmd :8; /* bit[14:7] */
		volatile uint32_t cp :1; /* bit[15] */
		volatile uint32_t dev_idx :5; /* bit[20:16] */
		volatile uint32_t dev_cnt :3; /* bit[23:21] */
		volatile uint32_t reserved0 :2; /* bit[25:24] */
		volatile uint32_t roc :1; /* bit[26] */
		volatile uint32_t reserved1 :3; /* bit[29:27] */
		volatile uint32_t toc :1; /* bit[30] */
		volatile uint32_t reserved2 :1; /* bit[31] */
	} addr_assign_cmd;

	struct {
		volatile uint32_t cmd_attr :3; /* bit[2:0] */
		volatile uint32_t tid :3; /* bit[5:3] */
		volatile uint32_t reserved0 :10; /* bit[15:5] */
		volatile uint32_t dl :16; /* bit[31:16] */
	} slave_data_cmd;
};
/* offset 0x0c */

int i3c_npcm4xx_master_send_ccc(const struct device *dev, struct i3c_ccc_cmd *ccc)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(dev);
	struct i3c_npcm4xx_config *config;
	struct i3c_npcm4xx_xfer xfer;
	struct i3c_npcm4xx_cmd cmd;
	union i3c_device_cmd_queue_port_s cmd_hi, cmd_lo;
	int pos = 0;

	/* To construct task */
	__u8 CCC;
	__u32 Baudrate = 0;
	__u32 Timeout = TIMEOUT_TYPICAL;
	ptrI3C_RetFunc callback = NULL;
	__u8 PortId;
	I3C_TASK_POLICY_Enum Policy = I3C_TASK_POLICY_APPEND_LAST;
	bool bHIF = NOT_HIF;

	__u16 TxLen = 0;
	__u8 TxBuf[I3C_PAYLOAD_SIZE_MAX] = { 0 };
	__u16 RxLen = 0;
	__u8 *RxBuf = NULL;

	if (ccc->id & I3C_CCC_DIRECT) {
		pos = i3c_npcm4xx_get_pos(obj, ccc->addr);
		if (pos < 0) {
			return pos;
		}
	}

	xfer.ncmds = 1;
	xfer.cmds = &cmd;
	xfer.ret = 0;

	cmd.ret = 0;
	if (ccc->rnw) {
		cmd.rx_buf = ccc->payload.data;
		cmd.rx_length = ccc->payload.length;
		cmd.tx_length = 0;
	} else {
		cmd.tx_buf = ccc->payload.data;
		cmd.tx_length = ccc->payload.length;
		cmd.rx_length = 0;
	}

	cmd_hi.value = 0;
	cmd_hi.xfer_arg.cmd_attr = COMMAND_PORT_XFER_ARG;
	cmd_hi.xfer_arg.dl = ccc->payload.length;

	cmd_lo.value = 0;
	cmd_lo.xfer_cmd.cmd_attr = COMMAND_PORT_XFER_CMD;
	cmd_lo.xfer_cmd.cp = 1;
	cmd_lo.xfer_cmd.dev_idx = pos;
	cmd_lo.xfer_cmd.cmd = ccc->id;
	cmd_lo.xfer_cmd.roc = 1;
	cmd_lo.xfer_cmd.rnw = ccc->rnw;
	cmd_lo.xfer_cmd.toc = 1;
	if (ccc->id == I3C_CCC_SETHID || ccc->id == I3C_CCC_DEVCTRL) {
		cmd_lo.xfer_cmd.speed = COMMAND_PORT_SPEED_I3C_I2C_FM;
	}
	cmd.cmd_hi = cmd_hi.value;
	cmd.cmd_lo = cmd_lo.value;

	config = obj->config;

	CCC = ccc->id;
	PortId = (__u8)(config->inst_id);
	Baudrate = I3C_TRANSFER_SPEED_SDR_1MHZ;
	Policy = I3C_TASK_POLICY_APPEND_LAST;
	callback = NULL;
	Timeout = TIMEOUT_TYPICAL;
	bHIF = IS_HIF;

	if (ccc->id & I3C_CCC_DIRECT) {
		if ((CCC == CCC_DIRECT_ENEC) || (CCC == CCC_DIRECT_DISEC)) {
			if (cmd.tx_length != 1)
				return -1;
			if (cmd.tx_buf == NULL)
				return -1;

			TxLen = 2;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], cmd.tx_buf, 1);
			I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_RSTDAA) {
			if (cmd.tx_length != 0)
				return -1;

			TxLen = 1;
			TxBuf[0] = ccc->addr;
			I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETMWL) {
			if (cmd.tx_length != 2)
				return -1;
			if (cmd.tx_buf == NULL)
				return -1;

			TxLen = 3;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], cmd.tx_buf, 2);
			I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETMRL) {
			if ((cmd.tx_length != 2) && (cmd.tx_length != 3))
				return -1;
			if (cmd.tx_buf == NULL)
				return -1;

			TxLen = cmd.tx_length + 1;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], cmd.tx_buf, cmd.tx_length);
			I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETDASA) {
			if (cmd.tx_length != 1)
				return -1;
			if (cmd.tx_buf == NULL)
				return -1;

			TxLen = 2;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], cmd.tx_buf, cmd.tx_length);
			I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETMWL) {
			TxLen = 1;
			RxLen = 2;
			TxBuf[0] = ccc->addr;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, RxLen, TxBuf, RxBuf, Baudrate,
				Timeout, callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETMRL) {
			TxLen = 1;
			RxLen = 3;
			TxBuf[0] = ccc->addr;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, RxLen, TxBuf, RxBuf, Baudrate,
				Timeout, callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETPID) {
			TxLen = 1;
			RxLen = 6;
			TxBuf[0] = ccc->addr;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, RxLen, TxBuf, RxBuf, Baudrate,
				Timeout, callback, PortId, Policy, bHIF);
		} else if ((CCC == CCC_DIRECT_GETBCR) || (CCC == CCC_DIRECT_GETDCR)
			|| (CCC == CCC_DIRECT_GETACCMST)) {
			TxLen = 1;
			RxLen = 1;
			TxBuf[0] = ccc->addr;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, RxLen, TxBuf, RxBuf, Baudrate,
				Timeout, callback, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETSTATUS) {
			TxLen = 1;
			RxLen = 2;
			TxBuf[0] = ccc->addr;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, RxLen, TxBuf, RxBuf, Baudrate,
				Timeout, callback, PortId, Policy, bHIF);
		} else {
			return -2;
		}
	} else {
		if (CCC == CCC_BROADCAST_ENTDAA) {
			RxLen = (sizeof(cmd.rx_buf) >= 63) ? 63 : (sizeof(cmd.rx_buf) % 9) * 9;
			RxBuf = cmd.rx_buf;
			I3C_Master_Insert_Task_ENTDAA(63, RxBuf, Baudrate, Timeout, callback,
				PortId, Policy, bHIF);
		} else {
			if ((CCC == CCC_BROADCAST_ENEC) || (CCC == CCC_BROADCAST_DISEC)) {
				if (cmd.tx_length != 1)
					return -1;
				if (cmd.tx_buf == NULL)
					return -1;

				TxLen = cmd.tx_length;
				memcpy(TxBuf, cmd.tx_buf, TxLen);
			} else if ((CCC == CCC_BROADCAST_RSTDAA) ||
				(CCC == CCC_BROADCAST_SETAASA)) {
				TxLen = 0;
			} else if (CCC == CCC_BROADCAST_SETMWL) {
				if (cmd.tx_length != 2)
					return -1;
				if (cmd.tx_buf == NULL)
					return -1;

				TxLen = cmd.tx_length;
				memcpy(TxBuf, cmd.tx_buf, TxLen);
			} else if (CCC == CCC_BROADCAST_SETMRL) {
				if ((cmd.tx_length != 1) || (cmd.tx_length != 3))
					return -1;
				if (cmd.tx_buf == NULL)
					return -1;

				TxLen = cmd.tx_length;
				memcpy(TxBuf, cmd.tx_buf, TxLen);
			} else if (CCC == CCC_BROADCAST_SETHID) {
				TxLen = 1;
				TxBuf[0] = 0x00;
			} else if (CCC == CCC_BROADCAST_DEVCTRL) {
				TxLen = 3;

				TxBuf[0] = 0xE0;
				TxBuf[1] = 0x00;
				TxBuf[2] = 0x00;
			} else {
				return -2;
			}

			I3C_Master_Insert_Task_CCCb(CCC, TxLen, TxBuf, Baudrate, Timeout, callback,
				PortId, Policy, bHIF);
		}
	}

	k_sem_init(&xfer.sem, 0, 1);
	xfer.ret = -ETIMEDOUT;

	i3c_npcm4xx_start_xfer(obj, &xfer);

	/* wait done, xfer.ret will be changed in ISR */
	k_sem_take(&xfer.sem, I3C_NPCM4XX_CCC_TIMEOUT);

	/* stop worker thread*/

	return xfer.ret;
}

/* i3cdev -> target device */
/* xfers -> send out data to slave or read in data from slave */
int i3c_npcm4xx_master_priv_xfer(struct i3c_dev_desc *i3cdev, struct i3c_priv_xfer *xfers,
	int nxfers)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(i3cdev->bus);
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(i3cdev);
	struct i3c_npcm4xx_xfer xfer;
	struct i3c_npcm4xx_cmd *cmds, *cmd;
	union i3c_device_cmd_queue_port_s cmd_hi, cmd_lo;
	int pos;
	int i, ret;

	I3C_TASK_INFO_t *pTaskInfo;
	bool bWnR;
	I3C_TRANSFER_PROTOCOL_Enum Protocol;
	__u8 Addr;
	__u16 TxLen = 0;
	__u16 RxLen = 0;
	__u8 *TxBuf = NULL;
	__u8 *RxBuf = NULL;
	__u32 Baudrate;
	__u32 Timeout = TIMEOUT_TYPICAL;
	ptrI3C_RetFunc callback = NULL;
	__u8 PortId = obj->config->inst_id;
	I3C_TASK_POLICY_Enum Policy = I3C_TASK_POLICY_APPEND_LAST;
	bool bHIF = IS_HIF;

	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pMaster;
	I3C_DEVICE_INFO_SHORT_t *pSlaveDev;

	if (!nxfers) {
		return 0;
	}

	pBus = api_I3C_Get_Bus_From_Port(PortId);
	pMaster = pBus->pCurrentMaster;

	/* get target address from master->dev_addr_tbl[pos],
	 * pos get from i3cdev->priv_data->pos, init during device attach
	 */
	if (priv == NULL) {
		return DEVICE_COUNT_MAX;
	}

	pos = priv->pos;
	if (pos < 0) {
		return pos;
	}

	Addr = obj->dev_addr_tbl[pos];

	/* find device node */
	pSlaveDev = pBus->pDevList;
	while (pSlaveDev != NULL) {
		if ((pSlaveDev->dynamicAddr == Addr) && (pSlaveDev->attr.b.runI3C == TRUE))
			break;
		if ((pSlaveDev->staticAddr == Addr) && (pSlaveDev->attr.b.runI3C == FALSE))
			break;
		pSlaveDev = pSlaveDev->pNextDev;
	}

	if (pSlaveDev == NULL) {
		return DEVICE_COUNT_MAX;
	}

	cmds = (struct i3c_npcm4xx_cmd *)k_calloc(sizeof(struct i3c_npcm4xx_cmd), nxfers);
	__ASSERT(cmds, "failed to allocat cmd\n");

	xfer.ncmds = nxfers;
	xfer.cmds = cmds;
	xfer.ret = 0;

	for (i = 0; i < nxfers; i++) {
		cmd = &xfer.cmds[i];

		cmd_hi.value = 0;
		cmd_hi.xfer_arg.cmd_attr = COMMAND_PORT_XFER_ARG;
		cmd_hi.xfer_arg.dl = xfers[i].len;

		cmd_lo.value = 0;
		if (xfers[i].rnw) {
			cmd->rx_buf = xfers[i].data.in;
			cmd->rx_length = xfers[i].len;
			cmd_lo.xfer_cmd.rnw = 1;
		} else {
			cmd->tx_buf = xfers[i].data.out;
			cmd->tx_length = xfers[i].len;
		}

		cmd_lo.xfer_cmd.tid = i;
		cmd_lo.xfer_cmd.dev_idx = pos;
		cmd_lo.xfer_cmd.roc = 1;
		if (i == nxfers - 1) {
			cmd_lo.xfer_cmd.toc = 1;
		}

		cmd->cmd_hi = cmd_hi.value;
		cmd->cmd_lo = cmd_lo.value;
	}

	for (i = 0; i < nxfers; i++) {
		bWnR = (((i + 1) < nxfers) && (xfers[i].rnw == 0) && (xfers[i + 1].rnw == 1));

		Baudrate = (pSlaveDev->attr.b.runI3C) ? I3C_TRANSFER_SPEED_SDR_12p5MHZ :
			I3C_TRANSFER_SPEED_I2C_100KHZ;

		if (!bWnR) {
			if (xfers[i].rnw) {
				Protocol = (pSlaveDev->attr.b.runI3C) ?
					I3C_TRANSFER_PROTOCOL_I3C_READ :
					I3C_TRANSFER_PROTOCOL_I2C_READ;
				RxBuf = xfers[i].data.in;
				RxLen = xfers[i].len;
			} else {
				Protocol = (pSlaveDev->attr.b.runI3C) ?
					I3C_TRANSFER_PROTOCOL_I3C_WRITE :
					I3C_TRANSFER_PROTOCOL_I2C_WRITE;
				TxBuf = xfers[i].data.out;
				TxLen = xfers[i].len;
			}
		} else {
			Protocol = (pSlaveDev->attr.b.runI3C) ?
				I3C_TRANSFER_PROTOCOL_I3C_WRITEnREAD :
				I3C_TRANSFER_PROTOCOL_I2C_WRITEnREAD;
			TxBuf = xfers[i].data.out;
			TxLen = xfers[i].len;
			RxBuf = xfers[i + 1].data.in;
			RxLen = xfers[i + 1].len;
		}

		pTaskInfo = I3C_Master_Create_Task(Protocol, Addr, 0, &TxLen, &RxLen, TxBuf, RxBuf,
			Baudrate, Timeout, callback, PortId, Policy, bHIF);
		if (pTaskInfo != NULL) {
			k_sem_init(&xfer.sem, 0, 1);
			xfer.ret = -ETIMEDOUT;
			i3c_npcm4xx_start_xfer(obj, &xfer);

			/* wait done, xfer.ret will be changed in ISR */
			k_sem_take(&xfer.sem, I3C_NPCM4XX_XFER_TIMEOUT);

			/* report actual read length */
			if (bWnR)
				i++;
		}
	}

	ret = xfer.ret;
	k_free(cmds);

	return ret;
}

int i3c_npcm4xx_master_send_entdaa(struct i3c_dev_desc *i3cdev)
{
	struct i3c_npcm4xx_config *config;
	I3C_PORT_Enum port;

	uint16_t rxlen = 63;
	uint8_t RxBuf_expected[63];

	config = DEV_CFG(i3cdev->bus);
	port = config->inst_id;

	struct i3c_npcm4xx_obj *obj = DEV_DATA(i3cdev->bus);
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(i3cdev);
	struct i3c_npcm4xx_xfer xfer;
	struct i3c_npcm4xx_cmd cmd;
	union i3c_device_cmd_queue_port_s cmd_hi, cmd_lo;

	cmd_hi.value = 0;
	cmd_hi.xfer_arg.cmd_attr = COMMAND_PORT_XFER_ARG;

	cmd_lo.value = 0;
	cmd_lo.addr_assign_cmd.cmd = I3C_CCC_ENTDAA;
	cmd_lo.addr_assign_cmd.cmd_attr = COMMAND_PORT_ADDR_ASSIGN;
	cmd_lo.addr_assign_cmd.toc = 1;
	cmd_lo.addr_assign_cmd.roc = 1;

	cmd_lo.addr_assign_cmd.dev_cnt = 1;
	cmd_lo.addr_assign_cmd.dev_idx = priv->pos;

	cmd.cmd_hi = cmd_hi.value;
	cmd.cmd_lo = cmd_lo.value;
	cmd.rx_length = 0;
	cmd.tx_length = 0;

	api_I3C_Master_Insert_Task_ENTDAA(rxlen, RxBuf_expected, I3C_TRANSFER_SPEED_SDR_1MHZ,
		TIMEOUT_TYPICAL, NULL, port, I3C_TASK_POLICY_APPEND_LAST, IS_HIF);

	k_sem_init(&xfer.sem, 0, 1);
	xfer.ret = -ETIMEDOUT;
	xfer.ncmds = 1;
	xfer.cmds = &cmd;
	i3c_npcm4xx_start_xfer(obj, &xfer);

	/* wait done, xfer.ret will be changed in ISR */
	if (k_sem_take(&xfer.sem, I3C_NPCM4XX_CCC_TIMEOUT) == 0) {
		if (xfer.ret == 0) {
			i3cdev->info.i2c_mode = 0;
			/*i3cdev->info.dynamic_addr = i3cdev->info.assigned_dynamic_addr;*/
		}
	}

	return 0;
}

/* MDB[7:5] = 111b, reserved */
volatile uint8_t MDB = 0xFF;

void I3C_Slave_Handle_DMA(__u32 Parm);
uint32_t I3C_Slave_Register_Access(I3C_PORT_Enum port, uint16_t rx_cnt, uint8_t *pRx_buff,
	bool bHDR);

#define ENTER_MASTER_ISR()	{ /* GPIO_Set_Data(GPIOC, 4, GPIO_DATA_LOW); */ }
#define EXIT_MASTER_ISR()	{ /* GPIO_Set_Data(GPIOC, 4, GPIO_DATA_HIGH); */ }

#define ENTER_SLAVE_ISR()	{ /* GPIO_Set_Data(GPIOC, 5, GPIO_DATA_LOW); */ }
#define EXIT_SLAVE_ISR()	{ /* GPIO_Set_Data(GPIOC, 5, GPIO_DATA_HIGH); */ }


/* MDMA */
#define I3C_MDMA_STOP_TX(p) {\
	I3C_SET_REG_MDMACTRL(p, I3C_GET_REG_MDMACTRL(p) & ~I3C_MDMACTRL_DMATB_MASK); }

#define I3C_MDMA_FLUSH_TX(p) {\
	I3C_SET_REG_MDATACTRL(p, I3C_GET_REG_MDATACTRL(p) | I3C_MDATACTRL_FLUSHTB_MASK); }

#define UpdateTaskInfoResult(t, r) { t->result = r; }
#define UpdateTaskResult(t, r) { UpdateTaskInfoResult(t->pTaskInfo, r); }

void I3C_Master_ISR(uint8_t I3C_IF)
{
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;

	uint32_t mctrl;
	uint32_t mstatus;
	uint32_t mconfig;
	uint32_t mintmask;
	uint32_t merrwarn;

	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	uint8_t ibi_type;

	ENTER_MASTER_ISR();

	pDevice = api_I3C_Get_Current_Master_From_Port(I3C_IF);
	pBus = api_I3C_Get_Bus_From_Port(I3C_IF);

	mintmask = I3C_GET_REG_MINTMASKED(I3C_IF);
	if (mintmask == 0)
		return;

	if (mintmask & I3C_MINTMASKED_ERRWARN_MASK) {
		merrwarn = I3C_GET_REG_MERRWARN(I3C_IF);
		I3C_SET_REG_MERRWARN(I3C_IF, merrwarn);
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_MCTRLDONE_MASK);

		pTask = pDevice->pTaskListHead;
		switch (merrwarn) {
		case I3C_MERRWARN_NACK_MASK:
			mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
			if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DAA) {
				UpdateTaskResult(pTask, I3C_ERR_NACK);
			} else {
				mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
				I3C_SET_REG_MSTATUS(I3C_IF, mstatus | I3C_MSTATUS_COMPLETE_MASK);

				if (mstatus & I3C_MSTATUS_SLVSTART_MASK) {
					I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);
					I3C_MDMA_STOP_TX(I3C_IF);
					I3C_MDMA_FLUSH_TX(I3C_IF);
					UpdateTaskResult(pTask, I3C_ERR_SLVSTART);
					I3C_SET_REG_MINTSET(I3C_IF,
						I3C_MINTSET_SLVSTART_MASK);
					k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_stop[I3C_IF]);
				} else if (pTask->pFrameList[pTask->frame_idx].direction
					== I3C_TRANSFER_DIR_WRITE) {
					UpdateTaskResult(pTask, I3C_ERR_NACK);
					k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_stop[I3C_IF]);
					EXIT_MASTER_ISR();
					return;
				} else {
					UpdateTaskResult(pTask, I3C_ERR_NACK);
					k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_retry[I3C_IF]);
				}
			}
			break;

		case I3C_MERRWARN_WRABT_MASK:
			pTaskInfo = pTask->pTaskInfo;
			pTaskInfo->result = I3C_ERR_WRABT;

			I3C_SET_REG_MDMACTRL(I3C_IF,
				I3C_GET_REG_MDMACTRL(I3C_IF) & ~I3C_MDMACTRL_DMATB_MASK);
			I3C_SET_REG_MDATACTRL(I3C_IF,
					I3C_GET_REG_MDATACTRL(I3C_IF) | I3C_MDATACTRL_FLUSHTB_MASK);
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
			break;

		default:
			LOG_INF("MErr:%X\r\n", merrwarn);
		}
	}

	if (mintmask & I3C_MINTMASKED_IBIWON_MASK) {
		pTask = pBus->pCurrentTask;
		pTaskInfo = pTask->pTaskInfo;

		/* IBIWON will enter ISR again afetr IBIAckNack */
		mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
		ibi_type = ((mstatus & I3C_MSTATUS_IBITYPE_MASK) >> I3C_MSTATUS_IBITYPE_SHIFT);

		if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_IBIACK) {
			I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_IBIWON_MASK |
			I3C_MSTATUS_COMPLETE_MASK | I3C_MSTATUS_MCTRLDONE_MASK);

			if (pDevice->ackIBI == FALSE) {
				api_I3C_Master_Insert_DISEC_After_IbiNack((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			}

			/* To prevent ERRWARN.SPAR, we must ack/nak IBI ASAP */
			if (ibi_type == I3C_MSTATUS_IBITYPE_IBI) {
				api_I3C_Master_IBIACK((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			} else if (ibi_type == I3C_MSTATUS_IBITYPE_MstReq) {
				api_I3C_Master_Insert_GETACCMST_After_IbiAckMR((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			} else if (ibi_type == I3C_MSTATUS_IBITYPE_HotJoin) {
				api_I3C_Master_Insert_ENTDAA_After_IbiAckHJ((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			}

			api_I3C_Master_Insert_DISEC_After_IbiNack((uint32_t) pTask);
			EXIT_MASTER_ISR();
			return;
		}

		if (ibi_type == I3C_MSTATUS_IBITYPE_IBI) {
			mctrl = I3C_GET_REG_MCTRL(I3C_IF);
			if ((mctrl & I3C_MCTRL_IBIRESP_MASK) == I3C_MCTRL_IBIRESP(0x01)) {
				/* NaK IBI */
				I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_IBIWON_MASK |
				I3C_MSTATUS_COMPLETE_MASK | I3C_MSTATUS_MCTRLDONE_MASK);
				mintmask = I3C_MINTMASKED_MCTRLDONE_MASK;
			} else if (ibi_type == I3C_MSTATUS_IBITYPE_IBI) {
				/* ACK with MDB */

				I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_MCTRLDONE_MASK);
				pTaskInfo->result = I3C_ERR_IBI;
			}
		} else if (ibi_type == I3C_MSTATUS_IBITYPE_MstReq) {
		} else if (ibi_type == I3C_MSTATUS_IBITYPE_HotJoin) {
		} else {
			/* Master nacked the SLVSTART, but can't stop immediatedly */
		}

		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_IBIWON_MASK);
	}

	if (mintmask & I3C_MINTMASKED_MCTRLDONE_MASK) {
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_MCTRLDONE_MASK);
		I3C_SET_REG_MINTCLR(I3C_IF, I3C_MINTCLR_MCTRLDONE_MASK);

		mconfig = I3C_GET_REG_MCONFIG(I3C_IF);
		if ((mconfig & I3C_MCONFIG_ODHPP(1)) == 0) {
			mconfig |= I3C_MCONFIG_ODHPP(1);
			I3C_SET_REG_MCONFIG(I3C_IF, mconfig);
		}

		mstatus = I3C_GET_REG_MSTATUS(I3C_IF);

		pTask = pDevice->pTaskListHead;
		pTaskInfo = pTask->pTaskInfo;
		pFrame = &pTask->pFrameList[pTask->frame_idx];

		if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DAA) {
			if ((pTask->frame_idx + 1) >= pTask->frame_count) {
				/* drop rx data, because we don't have enough memory
				 * to keep slave's info
				 */
				I3C_SET_REG_MDATACTRL(I3C_IF,
					I3C_GET_REG_MDATACTRL(I3C_IF) |
					I3C_MDATACTRL_FLUSHFB_MASK);

				if (mstatus & I3C_MSTATUS_BETWEEN_MASK) {
					pTaskInfo->result = I3C_ERR_OK;
					k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_stop[I3C_IF]);
				}
			} else {
				while ((pTask->pFrameList[pTask->frame_idx + 1].access_idx <
					pTask->pFrameList[pTask->frame_idx + 1].access_len)) {
					pTask->pFrameList[pTask->frame_idx + 1].access_buf[
						pTask->pFrameList[pTask->frame_idx + 1].access_idx]
						= I3C_GET_REG_MRDATAB(I3C_IF);
					pTask->pFrameList[pTask->frame_idx + 1].access_idx++;
				}

				k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
					&work_next[I3C_IF]);
			}
		} else if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			/* no slave want to participate ENTDAA, but slave ack 7E+Wr
			 * HW will send STOP automatically, and we should not drive STOP
			 * to prevent MSGERR
			 */
		} else if (pFrame->direction == I3C_TRANSFER_DIR_WRITE) {
			if (pFrame->access_len == 0) {
				/* 7E+Wr / CCCw */
				if (pTask->frame_count == (pTask->frame_idx + 1)) {
					k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_stop[I3C_IF]);
					EXIT_MASTER_ISR();
					return;
				}

				k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
					&work_next[I3C_IF]);
			} else {
				mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
				if (mstatus & I3C_MSTATUS_SLVSTART_MASK) {
					/* Private write win the bus arbitration ==>
					 * address in the private write is prior than
					 * IBI request
					 * We should complete the private write task
					 * before handling IBI request
					 */
					I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);
				}

				if (pFrame->type == I3C_TRANSFER_TYPE_DDR)
					I3C_SET_REG_MWDATAB1(I3C_IF, pFrame->hdrcmd);
				api_I3C_Setup_Master_Write_DMA(pDevice);
			}
		} else {
			if (pFrame->type == I3C_TRANSFER_TYPE_DDR) {
				I3C_SET_REG_MWDATAB1(I3C_IF, pFrame->hdrcmd);
			}
		}
	}

	if (mintmask & I3C_MINTMASKED_COMPLETE_MASK) {
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_COMPLETE_MASK | I3C_MSTATUS_MCTRLDONE_MASK);

		pTask = pDevice->pTaskListHead;
		pTaskInfo = pTask->pTaskInfo;
		pFrame = &pTask->pFrameList[pTask->frame_idx];

		/* Update the receive data length for read transfer, except ENTDAA */
		if (pFrame->direction == I3C_TRANSFER_DIR_READ) {
			if (PDMA->TDSTS & MaskBit(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX)) {
				PDMA->TDSTS = MaskBit(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX);
				*pTask->pRdLen = pFrame->access_len;
			} else if (I3C_GET_REG_MDMACTRL(I3C_IF) & I3C_MDMACTRL_DMAFB_MASK) {
				*pTask->pRdLen = pFrame->access_len -
					((uint16_t)((PDMA->DSCT[PDMA_OFFSET + I3C_IF +
						I3C_PORT_MAX].CTL & PDMA_DSCT_CTL_TXCNT_Msk) >>
					PDMA_DSCT_CTL_TXCNT_Pos) + 1);
			}
		}

		if (PDMA->TDSTS & MaskBit(PDMA_OFFSET + I3C_IF)) {
			PDMA->TDSTS = MaskBit(PDMA_OFFSET + I3C_IF);
		} else if (I3C_GET_REG_MDMACTRL(I3C_IF) & I3C_MDMACTRL_DMATB_MASK) {
			*pTask->pWrLen = pFrame->access_len -
				((uint16_t)((PDMA->DSCT[PDMA_OFFSET + I3C_IF].CTL &
					PDMA_DSCT_CTL_TXCNT_Msk) >>
					PDMA_DSCT_CTL_TXCNT_Pos) + 1);
		}

		I3C_SET_REG_MDMACTRL(I3C_IF, I3C_GET_REG_MDMACTRL(I3C_IF) &
			~I3C_MDMACTRL_DMAFB_MASK);
		PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX);
		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX);

		I3C_SET_REG_MDMACTRL(I3C_IF, I3C_GET_REG_MDMACTRL(I3C_IF) &
			~I3C_MDMACTRL_DMATB_MASK);
		I3C_SET_REG_MDATACTRL(I3C_IF, I3C_GET_REG_MDATACTRL(I3C_IF) |
			I3C_MDATACTRL_FLUSHTB_MASK);
		PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + I3C_IF);
		PDMA->CHCTL |= MaskBit(PDMA_OFFSET + I3C_IF);

		/* Error has been caught, but complete also assert */
		if (pTaskInfo->result == I3C_ERR_WRABT) {
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
			EXIT_MASTER_ISR();
			return;
		}

		if (pTaskInfo->result == I3C_ERR_IBI) {
			const struct device *dev;
			struct i3c_npcm4xx_obj *obj;

			dev = GetDevNodeFromPort(I3C_IF);
			obj = DEV_DATA(dev);
			i3c_npcm4xx_master_rx_ibi(obj);
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
			EXIT_MASTER_ISR();
			return;
		}

		if (pTaskInfo->result == I3C_ERR_MR) {
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
			EXIT_MASTER_ISR();
			return;
		}

		if (pTaskInfo->result == I3C_ERR_HJ) {
			EXIT_MASTER_ISR();
			return;
		}

		if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			if (pTaskInfo->result == I3C_ERR_NACK) {
			}

			*pTask->pRdLen = pTask->frame_idx * 9;

			pTaskInfo->result = I3C_ERR_OK;
			api_I3C_Master_End_Request((uint32_t) pTask);
			EXIT_MASTER_ISR();
			return;
		}

		if (pFrame->flag & I3C_TRANSFER_NO_STOP) {
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
				&work_next[I3C_IF]);
			EXIT_MASTER_ISR();
			return;
		}

		pTaskInfo->result = I3C_ERR_OK;

		if (pTask->protocol == I3C_TRANSFER_PROTOCOL_CCCr) {
			pFrame = &pTask->pFrameList[0];
			if ((pFrame->address == I3C_BROADCAST_ADDR) &&
				(pFrame->direction == I3C_TRANSFER_DIR_WRITE) &&
				(pFrame->access_buf[0] == CCC_DIRECT_GETACCMST)) {
				api_I3C_Master_End_Request((uint32_t) pTask);

				I3C_SET_REG_MCONFIG(I3C_IF, I3C_GET_REG_MCONFIG(I3C_IF) &
					~I3C_MCONFIG_MSTENA_MASK);
				I3C_SET_REG_MCONFIG(I3C_IF, I3C_GET_REG_MCONFIG(I3C_IF) |
					I3C_MCONFIG_MSTENA(I3C_MCONFIG_MSTENA_MASTER_CAPABLE));

				I3C_SET_REG_CONFIG(I3C_IF, I3C_GET_REG_CONFIG(I3C_IF) |
					I3C_CONFIG_SLVENA_MASK);

				pDevice->mode = I3C_DEVICE_MODE_SECONDARY_MASTER;
				EXIT_MASTER_ISR();
				return;
			}
		}

		k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
		EXIT_MASTER_ISR();
		return;
	}

	if (mintmask & I3C_MINTMASKED_NOWMASTER_MASK) {
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_NOWMASTER_MASK | I3C_MSTATUS_SLVSTART_MASK);

		pDevice = api_I3C_Get_INODE(I3C_IF);
		pDevice->mode = I3C_DEVICE_MODE_CURRENT_MASTER;
		pBus->pCurrentMaster = pDevice;

		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_EVENT_MASK | I3C_STATUS_CHANDLED_MASK |
			I3C_STATUS_STOP_MASK);
		I3C_SET_REG_DMACTRL(I3C_IF, I3C_GET_REG_DMACTRL(I3C_IF) &
			~(I3C_DMACTRL_DMATB_MASK | I3C_DMACTRL_DMAFB_MASK));
		I3C_SET_REG_DATACTRL(I3C_IF, I3C_GET_REG_DATACTRL(I3C_IF) |
			(I3C_DATACTRL_FLUSHFB_MASK | I3C_DATACTRL_FLUSHTB_MASK));
		I3C_SET_REG_CONFIG(I3C_IF, I3C_GET_REG_CONFIG(I3C_IF) & ~I3C_CONFIG_SLVENA_MASK);

		pTask = pDevice->pTaskListHead;
		pTaskInfo = pTask->pTaskInfo;
		pFrame = &pTask->pFrameList[pTask->frame_idx];

		pTaskInfo->result = I3C_ERR_OK;
		api_I3C_Master_End_Request((uint32_t) pTask);
	}

	/* SLVSTART might asserted right after the current task has complete (STOP).
	 * So, we should do complete before SLVSTART
	 */
	if (I3C_GET_REG_MINTMASKED(I3C_IF) & I3C_MINTMASKED_SLVSTART_MASK) {
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);

		if ((pBus->pCurrentTask != NULL) &&
			(pBus->pCurrentTask->protocol != I3C_TRANSFER_PROTOCOL_IBI) &&
			(pBus->pCurrentTask->protocol != I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) &&
			(pBus->pCurrentTask->protocol != I3C_TRANSFER_PROTOCOL_HOT_JOIN)) {
			pDevice->bAbort = TRUE;
		}

		api_I3C_Master_New_Request((uint32_t)I3C_IF);
	}

	EXIT_MASTER_ISR();
}

void I3C_Slave_ISR(uint8_t I3C_IF)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TASK_INFO_t *pTaskInfo = NULL;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	uint32_t intmasked;
	uint8_t evdet;
	uint32_t errwarn;

	ENTER_SLAVE_ISR();

	pDevice = api_I3C_Get_INODE(I3C_IF);
	intmasked = I3C_GET_REG_INTMASKED(I3C_IF);
	if (intmasked == 0)
		return;

	if (intmasked & I3C_INTMASKED_START_MASK) {
		evdet = (uint8_t)((I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_EVDET_MASK) >>
			I3C_STATUS_EVDET_SHIFT);

		/* IBI_NACKED and follow DISEC with RESTART */
		if (evdet == 0x02) {
			pTask = pDevice->pTaskListHead;
			if (pTask != NULL) {
				pTaskInfo = pTask->pTaskInfo;
				pTaskInfo->result = I3C_ERR_NACK_SLVSTART;
				api_I3C_Slave_End_Request((uint32_t) pTask);
			}
		}

		/*
		 * for RESTART
		 * 1. We must terminate previous RX DMA / FIFO before master send "Index" / Data
		 * 2. We must init RX DMA to get "Index" and data
		 */
		if (intmasked & I3C_INTMASKED_STOP_MASK) {
			/* update tx buffer here, only when slave doesn't get STOP */
			I3C_Slave_Handle_DMA((uint32_t)pDevice);
		}

		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_START_MASK);
	}

	/* !!! Don't remove to leave ISR after slave wakeup */
	if (intmasked & I3C_INTMASKED_MATCHED_MASK) {
		I3C_SET_REG_STATUS(I3C_IF, I3C_INTMASKED_MATCHED_MASK);
	}

	if (intmasked & I3C_INTMASKED_CCC_MASK) {
		/* Can't reset here, will validate CCC in I3C_Slave_Handle_DMA() */
		/* I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_CCC_MASK); */
	}

	if (intmasked & I3C_INTMASKED_DDRMATCHED_MASK) {
/*
		tmp = I3C_Slave_Register_Access(I3C_IF, slvRxOffset[I3C_IF],
			slvRxBuf[I3C_IF], TRUE);

		if (tmp == 0xFFFFFFFC) {
			I3C_SET_REG_INTCLR(I3C_IF, I3C_INTCLR_DDRMATCHED_MASK);
		} else {
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_DDRMATCH_MASK);
		}
*/
	}

	if (intmasked & I3C_INTMASKED_RXPEND_MASK) {
	}

	if (intmasked & I3C_INTMASKED_ERRWARN_MASK) {
		errwarn = I3C_GET_REG_ERRWARN(I3C_IF);

		if (errwarn & I3C_ERRWARN_SPAR_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_SPAR_MASK);
		}

		if (errwarn & I3C_ERRWARN_URUNNACK_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_URUNNACK_MASK);
			if (errwarn == I3C_ERRWARN_URUNNACK_MASK) {
				EXIT_SLAVE_ISR();
				return;
			}
		}

		if (errwarn & I3C_ERRWARN_INVSTART_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_INVSTART_MASK);
			if (errwarn & I3C_ERRWARN_INVSTART_MASK) {
				EXIT_SLAVE_ISR();
				return;
			}
		}

		if (errwarn & I3C_ERRWARN_OWRITE_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_OWRITE_MASK);
			LOG_WRN("@E0");
		}
		if (errwarn & I3C_ERRWARN_OREAD_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_OREAD_MASK);
			LOG_WRN("@E1");
		}

		if (errwarn & I3C_ERRWARN_HCRC_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_HCRC_MASK);
			LOG_WRN("@E3");
		}
		if (errwarn & I3C_ERRWARN_HPAR_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_HPAR_MASK);
			LOG_WRN("@E4");
		}
		if (errwarn & I3C_ERRWARN_ORUN_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_ORUN_MASK);
			LOG_WRN("@E5");
		}
		if (errwarn & I3C_ERRWARN_TERM_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_TERM_MASK);
		}
		if (errwarn & I3C_ERRWARN_S0S1_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_S0S1_MASK);
			LOG_WRN("@E7");
		}
		if (errwarn & I3C_ERRWARN_URUN_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_URUN_MASK);
			LOG_WRN("@EA");
		}
	}

	if (intmasked & I3C_INTMASKED_STOP_MASK) {
		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_STOP_MASK);

		pTask = pDevice->pTaskListHead;
		if (pTask != NULL) {
			pTaskInfo = pTask->pTaskInfo;
			pFrame = &pTask->pFrameList[pTask->frame_idx];
		}

		I3C_Slave_Handle_DMA((uint32_t) pDevice);

		if (pDevice->stopSplitRead) {
			I3C_SET_REG_DMACTRL(I3C_IF, I3C_GET_REG_DMACTRL(I3C_IF) &
				~I3C_DMACTRL_DMATB_MASK);
			PDMA->CHCTL &= ~MaskBit(PDMA_OFFSET + I3C_IF);

			if (PDMA->TDSTS & MaskBit(PDMA_OFFSET + I3C_IF)) {
				PDMA->TDSTS = MaskBit(PDMA_OFFSET + I3C_IF);
			} else {
			}

			I3C_SET_REG_DATACTRL(I3C_IF, I3C_GET_REG_DATACTRL(I3C_IF) |
				I3C_DATACTRL_FLUSHTB_MASK);
		} else {
		}

		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_EVENT_MASK) {
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_EVENT_MASK);
			if (pTask->protocol == I3C_TRANSFER_PROTOCOL_MASTER_REQUEST) {
			} else {
				if (pTaskInfo != NULL) {
					pTaskInfo->result = I3C_ERR_OK;
					api_I3C_Slave_End_Request((uint32_t) pTask);
				}
			}
		} else if ((pTask != NULL) && ((I3C_GET_REG_STATUS(I3C_IF) &
			I3C_STATUS_EVDET_MASK) == I3C_STATUS_EVDET(2))) {
			if ((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) &&
				(pFrame->retry_count >= 1)) {
				pFrame->retry_count--;
				api_I3C_Slave_Start_Request((uint32_t) pTaskInfo);
			} else {
				I3C_SET_REG_CTRL(I3C_IF, I3C_MCTRL_REQUEST_NONE);

				pTaskInfo->result = I3C_ERR_NACK;
				api_I3C_Slave_End_Request((uint32_t) pTask);
			}
		}

		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_CHANDLED_MASK)
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_CHANDLED_MASK);
		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_MATCHED_MASK)
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_MATCHED_MASK);
		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_DDRMATCH_MASK) {
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_DDRMATCH_MASK);
			I3C_SET_REG_INTSET(I3C_IF, I3C_INTSET_DDRMATCHED_MASK);
		}

		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_DACHG_MASK) {
			LOG_DBG("dynamic address assigned\n");

			uint8_t addr;
			const struct device *dev;
			struct i3c_npcm4xx_obj *obj;

			addr = I3C_Update_Dynamic_Address((uint32_t) I3C_IF);
			if (addr) {
				dev = GetDevNodeFromPort(I3C_IF);
				obj = DEV_DATA(dev);
				k_work_submit(&obj->work);
			}

			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_DACHG_MASK);
		}

		if (I3C_GET_REG_STATUS(I3C_IF) & (I3C_STATUS_START_MASK | I3C_STATUS_STOP_MASK))
			LOG_DBG("\r\nRE-ENTRY\r\n");

		I3C_Prepare_To_Read_Command((uint32_t) I3C_IF);
	}

	EXIT_SLAVE_ISR();
}

void I3C_Slave_Handle_DMA(__u32 Parm)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_PORT_Enum port;
	uint16_t txDataLen;
	bool bRet;

	pDevice = (I3C_DEVICE_INFO_t *)Parm;
	if (pDevice == NULL)
		return;

	port = pDevice->port;

	/* Slave Rcv data ?
	 * 1. Rx DMA is started, and TXCNT change
	 * 2. DDR matched
	 * 3. vendor CCC, not implement yet
	 */
	if ((I3C_GET_REG_STATUS(port) & I3C_STATUS_DDRMATCH_MASK) ||
		((I3C_GET_REG_DMACTRL(port) & I3C_DMACTRL_DMAFB_MASK))) {
		/* Update receive data length */
		if (PDMA->TDSTS & MaskBit(port + I3C_PORT_MAX)) {
			/* PDMA Rx Task Done */
			PDMA->TDSTS = MaskBit(port + I3C_PORT_MAX);
			slvRxOffset[port] = slvRxLen[port];

			/* receive data more than DMA buffer size -> overrun & drop */
			if (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_RXCOUNT_MASK) {
				I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port)
					| I3C_DATACTRL_FLUSHFB_MASK);
				LOG_WRN("Increase buffer size or limit transfer size !!!\r\n");
			}
		} else {
			/* PDMA Rx Task not finish */
			slvRxOffset[port] = slvRxLen[port] - (((PDMA->DSCT[I3C_PORT_MAX
				+ port].CTL & PDMA_DSCT_CTL_TXCNT_Msk) >> PDMA_DSCT_CTL_TXCNT_Pos)
				+ 1);
		}

		/* Process the Rcvd Data */
		if (slvRxOffset[port]) {
			/* Stop RX DMA */
			I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) &
				~I3C_DMACTRL_DMAFB_MASK);
			PDMA->CHCTL &= ~MaskBit(port + I3C_PORT_MAX);

			if (I3C_GET_REG_STATUS(port) & I3C_STATUS_CCC_MASK) {
				/* reserved for vendor CCC, drop rx data directly */
				if (slvRxBuf[port][0] == CCC_BROADCAST_SETAASA) {
					LOG_WRN("rcv setaasa...");
				}

				/* we can't support SETAASA because DYNADDR is RO. */
				slvRxOffset[port] = 0;
				I3C_SET_REG_STATUS(port, I3C_GET_REG_STATUS(port) |
					I3C_STATUS_CCC_MASK);
			} else {
				bRet = FALSE;

				/* To fill rx data to the requested mqueue */
				/* We must find callback from slave_data */
				const struct device *dev;
				struct i3c_npcm4xx_obj *obj;
				struct i3c_slave_payload *payload;
				int ret;

				dev = GetDevNodeFromPort(port);
				if ((dev == NULL) || !device_is_ready(dev))
					return;

				/* slave device */
				obj = DEV_DATA(dev);

				/* prepare m_queue to backup rx data */
				if (obj->slave_data.callbacks->write_requested != NULL) {
					payload = obj->slave_data.callbacks->write_requested(
						obj->slave_data.dev);
					payload->size = slvRxOffset[port];

					/*i3c_aspeed_rd_rx_fifo(obj, payload->buf, payload->size);*/
					bRet = TRUE;
					if (obj->config->priv_xfer_pec) {
						ret = pec_valid(dev, (uint8_t *)&slvRxBuf[port],
							slvRxOffset[port]);
					if (ret) {
						LOG_WRN("PEC error\n");
						bRet = FALSE;
						payload->size = 0;
					}
					}

					memcpy(payload->buf, slvRxBuf[port], payload->size);
				}

				if (obj->slave_data.callbacks->write_done != NULL) {
					obj->slave_data.callbacks->write_done(obj->slave_data.dev);
				}
			}
		}
	}

	/* Slave TX data has send ? */
	txDataLen = 0;
	if (I3C_GET_REG_DMACTRL(port) & I3C_DMACTRL_DMATB_MASK) {
		__u8 pdma_ch;

		pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);

		if (PDMA->TDSTS & MaskBit(PDMA_OFFSET + pdma_ch)) {
			txDataLen = 0;
		} else {
			txDataLen = ((PDMA->DSCT[port].CTL & PDMA_DSCT_CTL_TXCNT_Msk) >>
				PDMA_DSCT_CTL_TXCNT_Pos) + 1;
		}
	}

	/* Response data still in Tx FIFO */
	txDataLen += (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_TXCOUNT_MASK) >>
		I3C_DATACTRL_TXCOUNT_SHIFT;

	if (pDevice->txLen != 0) {
		if (txDataLen == 0) {
			/* call tx send complete hook */
			api_I3C_Slave_Finish_Response(pDevice);

			/* master read ibi data done */
			k_sem_give(&pDevice->ibi_complete);
		} else {
			/* do nothing, we can get correct tx len again */
		}
	}
}

I3C_ErrCode_Enum GetRegisterIndex(I3C_DEVICE_INFO_t *pDevice, uint16_t rx_cnt, uint8_t *pRx_buff,
	uint8_t *pIndexRet)
{
	uint8_t reg_count;
	uint8_t reg_chk_count;
	uint8_t i;
	__u16 cmd16;

	if (pIndexRet == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	*pIndexRet = 0;
	reg_count = pDevice->regCnt;
	reg_chk_count = 0;

	for (i = 0; i < reg_count; i++) {
		if (rx_cnt < GetCmdWidth(pDevice->pReg[i].attr.width))
			continue;
		reg_chk_count++;

		if (GetCmdWidth(pDevice->pReg[i].attr.width) == 1) {
			if (pDevice->pReg[i].cmd.cmd8 == pRx_buff[0]) {
				*pIndexRet = i;
				return I3C_ERR_OK;
			}

			continue;
		}

		if (GetCmdWidth(pDevice->pReg[i].attr.width) == 2) {
			if (pDevice->pReg[i].attr.endian == 0)
				cmd16 = (__u16)pRx_buff[0] | (__u16)pRx_buff[1] << 8;
			else
				cmd16 = (__u16)pRx_buff[0] << 8 | (__u16)pRx_buff[1];

			if (pDevice->pReg[i].cmd.cmd16 == cmd16) {
				*pIndexRet = i;
				return I3C_ERR_OK;
			}

			continue;
		}

		continue;
	}

	/* master send invalid "index" */
	if (reg_chk_count == reg_count)
		return I3C_ERR_DATA_ERROR;

	return I3C_ERR_PENDING;
}

static void i3c_npcm4xx_isr(const struct device *dev)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port = config->inst_id;
	uint32_t mconfig, sconfig;

	mconfig = sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCONFIG);
	if ((mconfig & I3C_MCONFIG_MSTENA_MASK) == I3C_MCONFIG_MSTENA_MASTER_ON) {
		I3C_Master_ISR(port);
		return;
	}

	sconfig = sys_read32(I3C_BASE_ADDR(port) + OFFSET_CONFIG);
	if ((sconfig & I3C_CONFIG_SLVENA_MASK) == I3C_CONFIG_SLVENA_SLAVE_ON) {
		I3C_Slave_ISR(port);
		return;
	}
}

/*
 * Usage:      Used to validate the register info from the input data
 *
 * port, data read from which internal device
 * rx_cnt, used to pass the length of read data
 * pRx_buff, used to pass the address of read data buffer
 * pRetInfo, used to return the update offset of register
 * pTx_buff, used to return the address of register buffer
 *
 * return:
 * 0         : complete or can't find "Index" now
 * 1-255     : valid "Index"
 * 0xFFFFFFFB: master send hdrcmd read but "Index" change
 * 0xFFFFFFFC: master send hdrcmd write
 * 0xFFFFFFFD: master send hdrcmd too fast and slave can't handle them
 * 0xFFFFFFFE: Invalid "Index"
 * 0xFFFFFFFF: Invalid Parameter
 */
uint32_t I3C_Slave_Register_Access(I3C_PORT_Enum port, uint16_t rx_cnt, uint8_t *pRx_buff,
	bool bHDR)
{
	I3C_DEVICE_INFO_t *pDevice;
	uint8_t cmd_id, hdrCmd1, hdrCmd2;
	I3C_ErrCode_Enum result;
	uint32_t tmp32;

	pDevice = api_I3C_Get_INODE(port);

	hdrCmd1 = 0;
	if (bHDR) {
		tmp32 = I3C_GET_REG_HDRCMD(port);
		hdrCmd1 = (uint8_t)(tmp32 & I3C_HDRCMD_CMD0_MASK);
		hdrCmd2 = hdrCmd1 & 0x7F;

		if (tmp32 & I3C_HDRCMD_OVFLW_MASK) {
			return 0xFFFFFFFD;
		}

		if (hdrCmd1 & 0x80) {
			if ((tmp32 & I3C_HDRCMD_NEWCMD_MASK) == 0) {
				LOG_INF("HDR Read Error !!!\r\n");
				return 0xFFFFFFFF;
			}

			if (hdrCmd2 != pDevice->cmdIndex) {
				LOG_INF("HDR Read Error: \"Index\" change !!!\r\n");
				return 0xFFFFFFFB;
			} else {
				return 0;
			}
		} else {
			if (tmp32 & I3C_HDRCMD_NEWCMD_MASK) {
				return 0xFFFFFFFC;
			}
		}

		result = GetRegisterIndex(pDevice, 1, (uint8_t *)&hdrCmd2, &cmd_id);
	} else {
		result = GetRegisterIndex(pDevice, rx_cnt, pRx_buff, &cmd_id);
	}

	if (result == I3C_ERR_PARAMETER_INVALID)
		return 0xFFFFFFFF;
	if (result == I3C_ERR_DATA_ERROR)
		return 0xFFFFFFFE;
	if (result == I3C_ERR_PENDING)
		return 0;

	pDevice->cmdIndex = cmd_id;

	if ((bHDR && rx_cnt) || (!bHDR && (rx_cnt >
		GetCmdWidth(pDevice->pReg[pDevice->cmdIndex].attr.width)))) {
		if ((pDevice->pReg[pDevice->cmdIndex].attr.write == 0) ||
			(bHDR && (hdrCmd1 & 0x80))) {
			pDevice->pRxBuf = NULL;
			pDevice->rxLen = 0;
			pDevice->rxOffset = 0;
			return 0;
		}

		pDevice->rxLen = pDevice->pReg[pDevice->cmdIndex].len;
		pDevice->rxOffset = 0;
		if (!bHDR) {
			pRx_buff += GetCmdWidth(pDevice->pReg[cmd_id].attr.width);
			rx_cnt -= GetCmdWidth(pDevice->pReg[cmd_id].attr.width);
		}

		while (rx_cnt) {
			if (pDevice->rxOffset >= pDevice->rxLen)
				break;

			pDevice->pReg[pDevice->cmdIndex].buf[pDevice->rxOffset] = *pRx_buff;
			pDevice->rxOffset++;
			pRx_buff++;
			rx_cnt--;
		}
	}

	if (pDevice->pReg[pDevice->cmdIndex].attr.read) {
		api_I3C_Slave_Prepare_Response(pDevice, pDevice->pReg[pDevice->cmdIndex].len,
			pDevice->pReg[pDevice->cmdIndex].buf);
	} else {
		api_I3C_Slave_Prepare_Response(pDevice, 0, NULL);
	}

	return 0;
}

static int i3c_init_work_queue(I3C_PORT_Enum port)
{
	switch (port) {
	case 0:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area0,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area0),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
	case 1:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area1,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area1),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#if (I3C_PORT_MAX == 6)
	case 2:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area2,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area2),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
	case 3:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area3,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area3),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
	case 4:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area4,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area4),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
	case 5:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area5,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area5),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
	default:
		return -ENXIO;
	}

	k_work_init(&work_stop[port], work_stop_fun);
	k_work_init(&work_next[port], work_next_fun);
	k_work_init(&work_retry[port], work_retry_fun);
	k_work_init(&work_send_ibi[port], work_send_ibi_fun);
	k_work_init(&work_rcv_ibi[port], work_rcv_ibi_fun);

	return 0;
}

static void sir_allowed_worker(struct k_work *work)
{
	struct i3c_npcm4xx_obj *obj = CONTAINER_OF(work, struct i3c_npcm4xx_obj, work);

	/* k_msleep(1000); */
	obj->sir_allowed_by_sw = 1;
}

static int i3c_npcm4xx_init(const struct device *dev)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	struct i3c_npcm4xx_obj *obj = DEV_DATA(dev);
	I3C_PORT_Enum port = config->inst_id;
	I3C_DEVICE_INFO_t *pDevice;
	int ret;

	LOG_INF("size_t=%d, uint32_t=%d\n", sizeof(size_t), sizeof(uint32_t));
	LOG_INF("Base=%x\n", (uint32_t) config->base);
	LOG_INF("slave=%d, secondary=%d\n", config->slave, config->secondary);
	LOG_INF("i2c_scl_hz=%d, i3c_scl_hz=%d\n", config->i2c_scl_hz, config->i3c_scl_hz);

	obj->dev = dev;
	obj->task_count = 0;
	obj->pTaskListHead = NULL;
	obj->pDevice = &gI3c_dev_node_internal[port];

	obj->config = config;
	obj->hw_dat_free_pos = GENMASK(DEVICE_COUNT_MAX - 1, 0);

	ret = i3c_init_work_queue(port);
	__ASSERT(ret == 0, "failed to init work queue for i3c driver !!!");

	/* update default setting */
	I3C_Port_Default_Setting(port);

	/* to customize device node for different projects */
	pDevice = &gI3c_dev_node_internal[port];

	/* Update device node by user setting */
	pDevice->disableTimeout = TRUE;
	pDevice->vendorID = I3C_GET_REG_VENDORID(port);
	pDevice->partNumber = (uint32_t)config->part_id << 16 |
		(uint32_t)port << 12 | /* instance id */
		(uint32_t)config->vendor_def_id; /* vendor def id*/

	pDevice->bcr = config->bcr;
	pDevice->dcr = config->dcr;
	pDevice->pid[0] = (uint8_t)(pDevice->vendorID >> 7);
	pDevice->pid[1] = (uint8_t)(pDevice->vendorID << 1);
	pDevice->pid[2] = (uint8_t)(config->part_id >> 8);
	pDevice->pid[3] = (uint8_t)config->part_id;
	pDevice->pid[4] = ((port & 0x0F) << 4) |
		((uint8_t)(config->vendor_def_id >> 8) & 0x0F);
	pDevice->pid[5] = (uint8_t)config->vendor_def_id;

	pDevice->staticAddr = config->assigned_addr;

	pDevice->max_rd_len = I3C_PAYLOAD_SIZE_MAX;
	pDevice->max_wr_len = I3C_PAYLOAD_SIZE_MAX;

	/* init mutex lock */
	k_mutex_init(&pDevice->lock);

	if (config->slave) {
		if (config->secondary)
			pDevice->mode = I3C_DEVICE_MODE_SECONDARY_MASTER;
		else
			pDevice->mode = I3C_DEVICE_MODE_SLAVE_ONLY;

		pDevice->callback = I3C_Slave_Callback;

		pDevice->stopSplitRead = FALSE;
		pDevice->capability.OFFLINE = FALSE;

		obj->sir_allowed_by_sw = 0;
		k_work_init(&obj->work, sir_allowed_worker);

		/* for loopback test without ibi behavior only */
		pDevice->pReg = I3C_REGs_PORT_SLAVE;
		pDevice->regCnt = sizeof(I3C_REGs_PORT_SLAVE) / sizeof(I3C_REG_ITEM_t);
	} else {
		pDevice->mode = I3C_DEVICE_MODE_CURRENT_MASTER;

		pDevice->bRunI3C = TRUE;
		pDevice->ackIBI = TRUE;
		pDevice->dynamicAddr = config->assigned_addr;
		pDevice->callback = I3C_Master_Callback;
	}

	api_I3C_connect_bus(port, config->busno);

	hal_I3C_Config_Device(pDevice);
	return 0;
}

#define I3C_NPCM4XX_INIT(n) static int i3c_npcm4xx_config_func_##n(const struct device *dev);\
	static const struct i3c_npcm4xx_config i3c_npcm4xx_config_##n = {\
		.inst_id = DT_INST_PROP_OR(n, instance_id, 0),\
		.assigned_addr = DT_INST_PROP_OR(n, assigned_address, 0),\
		.slave = DT_INST_PROP_OR(n, slave, 0),\
		.secondary = DT_INST_PROP_OR(n, secondary, 0),\
		.bcr = DT_INST_PROP_OR(n, bcr, 0),\
		.dcr = DT_INST_PROP_OR(n, dcr, 0),\
		.part_id = DT_INST_PROP_OR(n, part_id, 0),\
		.vendor_def_id = DT_INST_PROP_OR(n, vendor_def_id, 0),\
		.busno = DT_INST_PROP_OR(n, busno, I3C_BUS_COUNT_MAX),\
		.i2c_scl_hz = DT_INST_PROP_OR(n, i2c_scl_hz, 0),\
		.i3c_scl_hz = DT_INST_PROP_OR(n, i3c_scl_hz, 0),\
		.base = (struct i3c_reg *)DT_INST_REG_ADDR(n),\
	};\
	static struct i3c_npcm4xx_obj i3c_npcm4xx_obj##n;\
	DEVICE_DT_INST_DEFINE(n, &i3c_npcm4xx_config_func_##n, NULL, &i3c_npcm4xx_obj##n,\
			      &i3c_npcm4xx_config_##n, POST_KERNEL,\
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);\
	static int i3c_npcm4xx_config_func_##n(const struct device *dev)\
	{\
		int ret;\
		ret = i3c_npcm4xx_init(dev);\
		if (ret < 0) \
			return ret;\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), i3c_npcm4xx_isr,\
			DEVICE_DT_INST_GET(n), 0);\
		irq_enable(DT_INST_IRQN(n));\
		return 0;\
	}

DT_INST_FOREACH_STATUS_OKAY(I3C_NPCM4XX_INIT);
