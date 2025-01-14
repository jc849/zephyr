/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/sys_io.h>
#include <sys/crc.h>

#include <sys/util.h>
#include <sys/byteorder.h>
#include <kernel.h>
#include <init.h>
#include <irq.h>
#include <kernel/thread_stack.h>
#include <portability/cmsis_os2.h>

#include "pub_i3c.h"
#include "i3c_core.h"
#include "i3c_master.h"
#include "i3c_slave.h"
#include "hal_i3c.h"
#include "i3c_drv.h"

#include <stdlib.h>
#include <string.h>

#include "sig_id.h"

LOG_MODULE_REGISTER(npcm4xx_i3c_drv, CONFIG_I3C_LOG_LEVEL);

struct i3c_npcm4xx_obj *gObj[I3C_PORT_MAX];

#define NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE 1024
#define NPCM4XX_I3C_WORK_QUEUE_PRIORITY -2

/* current set 10ms * 5 * 3 => 0.15 seconds */
#define NPCM4XX_I3C_HJ_RETRY_MAX	3
#define NPCM4XX_I3C_HJ_CHECK_MAX	5
#define NPCM4XX_I3C_HJ_UDELAY		10000

#define I3C_NPCM4XX_CCC_TIMEOUT		K_MSEC(100)
#define I3C_NPCM4XX_XFER_TIMEOUT	K_MSEC(100)

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c0), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area0, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c1), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area1, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c2), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area2, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c3), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area3, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c4), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area4, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c5), okay)
K_THREAD_STACK_DEFINE(npcm4xx_i3c_stack_area5, NPCM4XX_I3C_WORK_QUEUE_STACK_SIZE);
#endif

#define ENTER_MASTER_ISR()      { \
	/* GPIO_Set_Data(GPIOC, 4, GPIO_DATA_LOW); */ \
	/* GPIOC5 goes L */ \
	/* RegClrBit(M8(0x40081000 + (0x0C * 0x2000L)), BIT(5)); */ }
#define EXIT_MASTER_ISR()       { \
	/* GPIO_Set_Data(GPIOC, 4, GPIO_DATA_HIGH); */ \
	/* GPIOC5 goes H */ \
	/* RegSetBit(M8(0x40081000 + (0x0C * 0x2000L)), BIT(5)); */ }

#define ENTER_SLAVE_ISR()   { \
	/* GPIO_Set_Data(GPIOC, 5, GPIO_DATA_LOW); */ \
	/* GPIOC4 goes L */ \
	/* RegClrBit(M8(0x40081000 + (0x0C * 0x2000L)), BIT(4)); */ }
#define EXIT_SLAVE_ISR()    { \
	/* GPIO_Set_Data(GPIOC, 5, GPIO_DATA_HIGH); */ \
	/* GPIOC4 goes H */ \
	/* RegSetBit(M8(0x40081000 + (0x0C * 0x2000L)), BIT(4)); */ }

struct k_work_q npcm4xx_i3c_work_q[I3C_PORT_MAX];

struct k_work work_stop[I3C_PORT_MAX];
struct k_work work_next[I3C_PORT_MAX];
struct k_work work_send_ibi[I3C_PORT_MAX];
struct k_work work_entdaa[I3C_PORT_MAX];

static uint32_t i3c_npcm4xx_master_send_done(void *pCallbackData, struct I3C_CallBackResult *CallBackResult);
static void i3c_npcm4xx_start_xfer(struct i3c_npcm4xx_obj *obj, struct i3c_npcm4xx_xfer *xfer);

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

	pDevice = Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	pTask = pDevice->pTaskListHead;
	if (pTask == NULL)
		return;

	I3C_Master_Stop_Request((uint32_t)pTask);
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

	pDevice = Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	pTask = pDevice->pTaskListHead;
	if (pTask == NULL)
		return;

	I3C_Master_Run_Next_Frame((uint32_t)pTask);
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

	pBus = Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	if (pBus->pCurrentTask != NULL)	{
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[i], item);
		return;
	}

	pDeviceSlv = I3C_Get_INODE(i);

	pTask = pDeviceSlv->pTaskListHead;
	if (pTask == NULL) {
		return;
	}

	pTask = pDeviceSlv->pTaskListHead;
	pBus->pCurrentTask = pTask; /* task with higher priority will insert in the front */

	pTaskInfo = pTask->pTaskInfo;
	I3C_Slave_Start_Request((uint32_t)pTaskInfo);
}

void work_entdaa_fun(struct k_work *item)
{
	struct i3c_npcm4xx_config *config;
	struct i3c_npcm4xx_obj *obj;
	struct i3c_npcm4xx_xfer *xfer;
	uint8_t i;
	uint16_t rxlen = 63;
	uint8_t RxBuf_expected[63];
	uint32_t Baudrate;
	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pDevice;
	ptrI3C_RetFunc pCallback;
	k_spinlock_key_t key;
	I3C_ErrCode_Enum result;

	for (i = 0; i < I3C_PORT_MAX; i++) {
		if (item == &work_entdaa[i])
			break;
	}

	if (i == I3C_PORT_MAX)
		return;

	pDevice = Get_Current_Master_From_Port(i);
	if (pDevice == NULL)
		return;

	pBus = Get_Bus_From_Port(i);
	if (pBus == NULL)
		return;

	obj = gObj[i];
	config = obj->config;
	Baudrate = config->i3c_scl_hz;
	pCallback = i3c_npcm4xx_master_send_done;

	xfer = (struct i3c_npcm4xx_xfer *)hal_I3C_MemAlloc(sizeof(struct i3c_npcm4xx_xfer));

	if (xfer) {
		xfer->ret = -ETIMEDOUT;
		xfer->ncmds = 1;
		xfer->abort = false;
		xfer->complete = false;
	} else {
		return;
	}

	k_mutex_lock(&pDevice->lock, K_FOREVER);

	result = I3C_Master_Insert_Task_ENTDAA(&rxlen, RxBuf_expected, Baudrate, TIMEOUT_TYPICAL,
			pCallback, (void *)xfer, i, I3C_TASK_POLICY_APPEND_LAST, IS_HIF);

	if (result != I3C_ERR_OK) {
		LOG_ERR("Workqueue create ENTDAA task failed");
		k_mutex_unlock(&pDevice->lock);
		hal_I3C_MemFree(xfer);
		return;
	}

	k_sem_init(&xfer->xfer_complete, 0, 1);

	i3c_npcm4xx_start_xfer(obj, xfer);

	/* wait done, xfer.ret will be changed in ISR */
	if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
		key = k_spin_lock(&xfer->lock);
		if (xfer->complete == true) {
			/* In this case, means context switch after callback function
			 * call spin_unlock() and timeout happened.
			 * Take semaphore again to make xfer_complete synchronization.
			 */
			LOG_WRN("sem timeout but task complete, get sem again to clear flag");
			k_spin_unlock(&xfer->lock, key);
			if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
				LOG_WRN("take sem again timeout");
			}
		} else { /* the memory will be free when driver call pCallback */
			LOG_ERR("ENTDAA timeout");
			xfer->abort = true;
			k_spin_unlock(&xfer->lock, key);
			k_mutex_unlock(&pDevice->lock);
			return;
		}
	}

	k_mutex_unlock(&pDevice->lock);

	hal_I3C_MemFree(xfer);
}

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

static void i3c_setup_dma_channel(uint8_t hw_id, int dma_channel)
{
	uint32_t pdma_offset = 0;
	uint32_t pdma_value = 0;

	if (dma_channel < 4)
		pdma_value = PDMA->REQSEL0_3;
	else if (dma_channel < 8)
		pdma_value = PDMA->REQSEL4_7;
	else if (dma_channel < 12)
		pdma_value = PDMA->REQSEL8_11;
	else if (dma_channel < 16)
		pdma_value = PDMA->REQSEL12_15;
	else
		return;

	pdma_offset = dma_channel % 4;
	pdma_value = pdma_value & ~(0xFF << (8 * pdma_offset));
	pdma_value = pdma_value | (hw_id << (8 * pdma_offset));

	if (dma_channel < 4)
		PDMA->REQSEL0_3 = pdma_value;
	else if (dma_channel < 8)
		PDMA->REQSEL4_7 = pdma_value;
	else if (dma_channel < 12)
		PDMA->REQSEL8_11 = pdma_value;
	else if (dma_channel < 16)
		PDMA->REQSEL12_15 = pdma_value;
	else
		return;
}

static void i3c_setup_dma_configure(I3C_PORT_Enum port, int tx_dma_channel, int rx_dma_channel)
{
	switch (port) {
	case I3C1_IF:
		i3c_setup_dma_channel(PDMA_I3C1_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C1_RX, rx_dma_channel);
		break;
	case I3C2_IF:
		i3c_setup_dma_channel(PDMA_I3C2_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C2_RX, rx_dma_channel);
		break;
	case I3C3_IF:
		i3c_setup_dma_channel(PDMA_I3C3_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C3_RX, rx_dma_channel);
		break;
	case I3C4_IF:
		i3c_setup_dma_channel(PDMA_I3C4_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C4_RX, rx_dma_channel);
		break;
	case I3C5_IF:
		i3c_setup_dma_channel(PDMA_I3C5_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C5_RX, rx_dma_channel);
		break;
	case I3C6_IF:
		i3c_setup_dma_channel(PDMA_I3C6_TX, tx_dma_channel);
		i3c_setup_dma_channel(PDMA_I3C6_RX, rx_dma_channel);
		break;
	default:
		break;
	}
}

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
	.attr.width = 0, .attr.read = true, .attr.write = true }, {
	.cmd.cmd8 = CMDIdx_ID, .len = CMD_BUF_LEN_ID, .buf = I3C_REGs_BUF_CMD_ID,
	.attr.width = 0, .attr.read = true, .attr.write = false },
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
	I3C_PORT_Enum port = I3C_Get_IPORT(pDevice);
	I3C_ErrCode_Enum result = I3C_ERR_OK;
	struct i3c_npcm4xx_obj *obj;
	uint32_t mconfig;
	uint32_t sconfig;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	if ((pDevice->capability.OFFLINE == false) && (pDevice->bcr & 0x08))
		return I3C_ERR_PARAMETER_INVALID;

	result = I3C_ERR_OK;

	/* SKEW = 0, HKEEP = 3 */
	mconfig = ((I3C_GET_REG_MCONFIG(port) & 0xF1FFFF7B) |
		I3C_MCONFIG_HKEEP(I3C_MCONFIG_HKEEP_EXTBOTH));
	sconfig = I3C_GET_REG_CONFIG(port) & 0xFE7F071F;

	if (pDevice->mode == I3C_DEVICE_MODE_DISABLE) {
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

		PDMA->CHCTL |= BIT(PDMA_OFFSET + port)
			| BIT(PDMA_OFFSET + I3C_PORT_MAX + port);

		if (pDevice->dma_tx_channel >= PDMA_CH_MAX)
			return I3C_ERR_PARAMETER_INVALID;

		if (pDevice->dma_rx_channel >= PDMA_CH_MAX)
			return I3C_ERR_PARAMETER_INVALID;

		i3c_setup_dma_configure(port, pDevice->dma_tx_channel, pDevice->dma_rx_channel);

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
		sconfig |= I3C_CONFIG_BAMATCH(I3C_CLOCK_SLOW_FREQUENCY / I3C_1MHz_VAL_CONST);
		/* sconfig |= I3C_CONFIG_BAMATCH(0x7F); */

		/* if support ASYNC-0, update TCCLOCK */
		I3C_SET_REG_TCCLOCK(port, I3C_TCCLOCK_FREQ(2 * (I3C_CLOCK_FREQUENCY /
			I3C_1MHz_VAL_CONST)) | I3C_TCCLOCK_ACCURACY(30));
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
	if (pDevice->stopSplitRead == false) {
	}

	/* assign static address */
	sconfig &= ~I3C_CONFIG_SADDR_MASK;
	if (pDevice->staticAddr != 0x00)
		sconfig |= I3C_CONFIG_SADDR(pDevice->staticAddr);

	sconfig &= ~I3C_CONFIG_S0IGNORE_MASK;

	obj = gObj[port];

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

	I3C_SET_REG_INTSET(port, I3C_INTSET_CHANDLED_MASK | I3C_INTSET_DDRMATCHED_MASK |
		I3C_INTSET_ERRWARN_MASK | I3C_INTSET_CCC_MASK | I3C_INTSET_DACHG_MASK |
		I3C_INTSET_STOP_MASK | I3C_INTSET_START_MASK | I3C_INTSET_EVENT_MASK);

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

	if ((pDevice->mode == I3C_DEVICE_MODE_SLAVE_ONLY)
		|| (pDevice->mode == I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		I3C_Prepare_To_Read_Command((uint32_t) port);
	}

	return result;
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
	uint32_t PPBAUD;
	uint32_t PPLOW;
	uint32_t ODBAUD;
	uint32_t I2CBAUD;
	uint32_t ODHPP = 1;
	I3C_DEVICE_INFO_t *pMasterDevice;

	uint32_t mconfig;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;

	if (pTaskInfo == NULL)
		return;

	mconfig = I3C_GET_REG_MCONFIG(pTaskInfo->Port);

	PPBAUD = (mconfig & I3C_MCONFIG_PPBAUD_MASK) >> I3C_MCONFIG_PPBAUD_SHIFT;
	PPLOW = (mconfig & I3C_MCONFIG_PPLOW_MASK) >> I3C_MCONFIG_PPLOW_SHIFT;
	ODBAUD = (mconfig & I3C_MCONFIG_ODBAUD_MASK) >> I3C_MCONFIG_ODBAUD_SHIFT;
	I2CBAUD = (mconfig & I3C_MCONFIG_I2CBAUD_MASK) >> I3C_MCONFIG_I2CBAUD_SHIFT;

	mconfig &= ~(I3C_MCONFIG_I2CBAUD_MASK | I3C_MCONFIG_ODHPP_MASK | I3C_MCONFIG_ODBAUD_MASK
		| I3C_MCONFIG_PPLOW_MASK | I3C_MCONFIG_PPBAUD_MASK);

	pMasterDevice = I3C_Get_INODE(pTaskInfo->Port);

	pTask = pTaskInfo->pTask;
	if (pTask->frame_idx != 0)
		return;

	pFrame = &(pTask->pFrameList[pTask->frame_idx]);

	// update by frame setting
	if ((pTask->frame_idx == 0) && ((pFrame->flag & I3C_TRANSFER_REPEAT_START) == 0) &&
			(pFrame->type != I3C_TRANSFER_TYPE_I2C) && (pFrame->address == 0x7E)) {
		ODHPP = 0;
	}

	if (pFrame->type == I3C_TRANSFER_TYPE_I2C) {
		switch(pFrame->baudrate) {
			case I3C_TRANSFER_SPEED_I2C_1MHZ:
				if (APB3_CLK == 96000000) { // 926 - 922 KHz
					PPBAUD = 3;
					PPLOW = 0;
					ODBAUD = 12;
					I2CBAUD = 0;
				} else if (APB3_CLK == 48000000) { // 878 - 880 KHz
					PPBAUD = 2;
					PPLOW = 0;
					ODBAUD = 8;
					I2CBAUD = 0;
				}
				break;
			case I3C_TRANSFER_SPEED_I2C_400KHZ:
				if (APB3_CLK == 96000000) { // 371 - 369 KHz
					PPBAUD = 9;
					PPLOW = 0;
					ODBAUD = 12;
					I2CBAUD = 0;
				} else if (APB3_CLK == 48000000) { // 340 - 338 KHz
					PPBAUD = 9;
					PPLOW = 0;
					ODBAUD = 6;
					I2CBAUD = 0;
				}
				break;
			default:
				if (APB3_CLK == 96000000) { // 98 - 96 KHz
					PPBAUD = 15;
					PPLOW = 0;
					ODBAUD = 30;
					I2CBAUD = 0;
				} else if (APB3_CLK == 48000000) { // 98 - 97 KHz
					PPBAUD = 2;
					PPLOW = 0;
					ODBAUD = 80;
					I2CBAUD = 0;
				}
				break;
		}
	}
	else
	{
		switch(pFrame->baudrate) {
			case I3C_TRANSFER_SPEED_SDR_12p5MHZ:
				// I3C PP=12.5MHz, OD Freq = 1MHz if ODHPP = 0
				if (APB3_CLK == 96000000) {
					PPBAUD = 2;
					PPLOW = 2;
					ODBAUD = 15;
				} else if (APB3_CLK == 48000000) {
					PPBAUD = 0;
					PPLOW = 2;
					ODBAUD = 23;
				}
				break;
			case I3C_TRANSFER_SPEED_SDR_8MHZ:
				// I3C PP=8MHz, OD Freq = 1MHz if ODHPP = 0
				if (APB3_CLK == 96000000) {
					PPBAUD = 2;
					PPLOW = 6;
					ODBAUD = 15;
				} else if (APB3_CLK == 48000000) {
					PPBAUD = 0;
					PPLOW = 4;
					ODBAUD = 23;
				}
				break;
			case I3C_TRANSFER_SPEED_SDR_6MHZ:
				// I3C PP=6MHz, OD Freq = 1MHz if ODHPP = 0
				if (APB3_CLK == 96000000) {
					PPBAUD = 2;
					PPLOW = 10;
					ODBAUD = 15;
				} else if (APB3_CLK == 48000000) {
					PPBAUD = 1;
					PPLOW = 4;
					ODBAUD = 11;
				}
				break;
			case I3C_TRANSFER_SPEED_SDR_4MHZ:
				// I3C PP=4MHz, OD Freq = 1MHz if ODHPP = 0
				if (APB3_CLK == 96000000) {
					PPBAUD = 5;
					PPLOW = 12;
					ODBAUD = 7;
                                } else if (APB3_CLK == 48000000) {
					PPBAUD = 0;
					PPLOW = 10;
					ODBAUD = 23;
                                }
				break;
			case I3C_TRANSFER_SPEED_SDR_2MHZ:
				// I3C PP=2MHz, OD Freq = 1MHz if ODHPP = 0
				if (APB3_CLK == 96000000) {
					PPBAUD = 15;
					PPLOW = 15;
					ODBAUD = 2;
				} else if (APB3_CLK == 48000000) {
					PPBAUD = 4;
					PPLOW = 14;
					ODBAUD = 4;
				}
				break;
			default:
				if (APB3_CLK == 96000000) {
					PPBAUD = 15;
					PPLOW = 15;
					ODBAUD = 5;
				} else if (APB3_CLK == 48000000) {
					PPBAUD = 15;
					PPLOW = 15;
					ODBAUD = 1;
				}
				break;
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

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_disable_interface(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_enable_interrupt(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_disable_interrupt(I3C_PORT_Enum port)
{
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	return I3C_ERR_OK;
}

uint32_t hal_I3C_get_MAXRD(I3C_PORT_Enum port)
{
	uint32_t regVal;

	if (port >= I3C_PORT_MAX)
		return 0;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal = (regVal & I3C_MAXLIMITS_MAXRD_MASK) >> I3C_MAXLIMITS_MAXRD_SHIFT;
	return regVal;
}

void hal_I3C_set_MAXRD(I3C_PORT_Enum port, uint32_t val)
{
	uint32_t regVal;

	if (port >= I3C_PORT_MAX)
		return;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal &= I3C_MAXLIMITS_MAXWR_MASK;
	val = ((val << I3C_MAXLIMITS_MAXRD_SHIFT) & I3C_MAXLIMITS_MAXRD_MASK);
	regVal = regVal | val;
	I3C_SET_REG_MAXLIMITS(port, regVal);
}

uint32_t hal_I3C_get_MAXWR(I3C_PORT_Enum port)
{
	uint32_t regVal;

	if (port >= I3C_PORT_MAX)
		return 0;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal = (regVal & I3C_MAXLIMITS_MAXWR_MASK) >> I3C_MAXLIMITS_MAXWR_SHIFT;
	return regVal;
}

void hal_I3C_set_MAXWR(I3C_PORT_Enum port, uint32_t val)
{
	uint32_t regVal;

	if (port >= I3C_PORT_MAX)
		return;

	regVal = I3C_GET_REG_MAXLIMITS(port);
	regVal &= I3C_MAXLIMITS_MAXRD_MASK;
	val = ((val << I3C_MAXLIMITS_MAXWR_SHIFT) & I3C_MAXLIMITS_MAXWR_MASK);
	regVal = regVal | val;
	I3C_SET_REG_MAXLIMITS(port, regVal);
}

uint32_t hal_I3C_get_mstatus(I3C_PORT_Enum port)
{
	return I3C_GET_REG_MSTATUS(port);
}

void hal_I3C_set_mstatus(I3C_PORT_Enum port, uint32_t val)
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

uint8_t hal_I3C_get_ibiAddr(I3C_PORT_Enum port)
{
	uint32_t mstatus = I3C_GET_REG_MSTATUS(port);
	uint8_t ibi_addr = (uint8_t)(
		(mstatus & I3C_MSTATUS_IBIADDR_MASK) >> I3C_MSTATUS_IBIADDR_SHIFT);
	return ibi_addr;
}

bool hal_I3C_Is_Master_Idle(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_IDLE) ?
		true : false;
}

bool hal_I3C_Is_Slave_Idle(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return false;
	if ((I3C_GET_REG_STATUS(port) & I3C_STATUS_STNOTSTOP_MASK) == 0)
		return true;
	return false;
}

bool hal_I3C_Is_Master_DAA(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DAA) ?
		true : false;
}

bool hal_I3C_Is_Slave_DAA(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return false;
	if (I3C_GET_REG_STATUS(port) & I3C_STATUS_STDAA_MASK)
		return true;
	return false;
}

bool hal_I3C_Is_Master_SLVSTART(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_SLVREQ) ?
		true : false;
}

bool hal_I3C_Is_Slave_SLVSTART(I3C_PORT_Enum port)
{
	if ((I3C_GET_REG_CONFIG(port) & I3C_CONFIG_SLVENA_MASK) == 0)
		return false;
	if (I3C_GET_REG_STATUS(port) & I3C_STATUS_EVENT_MASK)
		return true;
	return false;
}

bool hal_I3C_Is_Master_NORMAL(I3C_PORT_Enum port)
{
	return ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_NORMACT) ?
		true : false;
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
	I3C_DEVICE_INFO_t *pDevice;

	/* invalid channel, 6694 only support 14 channels, 0 - 13
	 * I3C PDMA channel,
	 * 0: I3C1_TX, 1: I3C2_TX, 2: I3C3_TX, 3: I3C4_TX, 4: I3C5_TX, 5: I3C6_TX,
	 * 6: I3C1_RX, 7: I3C2_RX, 8: I3C3_RX, 9: I3C4_RX, 10: I3C5_RX, 11: I3C6_RX,
	 */

	pDevice = I3C_Get_INODE(port);

	if (port >= I3C_PORT_MAX)
		return 0xFF;

	if (direction == I3C_TRANSFER_DIR_WRITE)
		return pDevice->dma_tx_channel;
	else
		return pDevice->dma_rx_channel;
}

void hal_I3C_Disable_Master_RX_DMA(I3C_PORT_Enum port)
{
	uint8_t pdma_ch;
	uint32_t key;

	key = irq_lock();

	I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMAFB_MASK);

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);
	PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
	irq_unlock(key);
}

void hal_I3C_Disable_Master_DMA(I3C_PORT_Enum port)
{
	uint8_t pdma_ch;
	uint32_t key;

	key = irq_lock();
	I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~(I3C_MDMACTRL_DMAFB_MASK |
				I3C_MDMACTRL_DMATB_MASK));
	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);
	PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);
	PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
	irq_unlock(key);
}

void hal_I3C_Reset_Master_TX(I3C_PORT_Enum port)
{
	uint32_t key;

	key = irq_lock();

	I3C_SET_REG_MDATACTRL(port, I3C_GET_REG_MDATACTRL(port) | I3C_MDATACTRL_FLUSHTB_MASK);

	irq_unlock(key);
}

bool hal_I3C_TxFifoNotFull(I3C_PORT_Enum port)
{
	return (I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_TXNOTFULL_MASK) ? true : false;
}

void hal_I3C_WriteByte(I3C_PORT_Enum port, uint8_t val)
{
	I3C_SET_REG_MWDATABE(port, val);
}

bool hal_I3C_DMA_Write(I3C_PORT_Enum port, I3C_DEVICE_MODE_Enum mode, uint8_t *txBuf, uint16_t txLen)
{
	uint8_t pdma_ch;
	uint32_t Temp;
	uint32_t key;

	if (txLen == 0)
		return true;
	if (port >= I3C_PORT_MAX)
		return false;
	if ((mode != I3C_DEVICE_MODE_CURRENT_MASTER) && (mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		(mode != I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		return false;
	}

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_WRITE);

	key = irq_lock();

	if (mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		if (txLen == 1) {
			/* case 1: Write Data Len = 1 and Tx FIFO not full */
			if (I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_TXNOTFULL_MASK) {
				I3C_SET_REG_MWDATABE(port, *txBuf);
				irq_unlock(key);
				return true;
			}

			/* case 2: Write Data Len = 1, use Basic Mode */
			PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
			I3C_SET_REG_MDMACTRL(port,
				I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMATB_MASK);

			/* can't flush Tx FIFO data for DDR */
			/* 2-2. Start DMA */
			PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
			PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

			PDMA_TxBuf_END[pdma_ch] = txBuf[0];

			PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = (((uint32_t) 0)
				<< PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_INC | PDMA_DAR_FIX |
				PDMA_WIDTH_32 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
				(uint32_t)&PDMA_TxBuf_END[pdma_ch];
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA =
				(uint32_t)&(((struct i3c_reg *) I3C_BASE_ADDR(port))->MWDATABE);
		} else {
			/* case 3: Write Data Buffer > 1, use Scatter Gather Mode */
			PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
			I3C_SET_REG_MDMACTRL(port,
				I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMATB_MASK);

			/* can't flush Tx FIFO data for DDR */
			PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
			PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

			PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = PDMA_OP_SCATTER;
			PDMA->DSCT[PDMA_OFFSET + pdma_ch].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2];

			I3C_DSCT[pdma_ch * 2].CTL = (((uint32_t) txLen - 2)
				<< PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_INC | PDMA_DAR_FIX |
				PDMA_WIDTH_8 | PDMA_OP_SCATTER | PDMA_REQ_SINGLE;
			I3C_DSCT[pdma_ch * 2].ENDSA = (uint32_t)&txBuf[0];

			I3C_DSCT[pdma_ch * 2].ENDDA =
				(uint32_t)&(((struct i3c_reg *)I3C_BASE_ADDR(port))->MWDATAB1);
			I3C_DSCT[pdma_ch * 2].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2 + 1];

			PDMA_TxBuf_END[pdma_ch] = txBuf[txLen - 1];
			I3C_DSCT[pdma_ch * 2 + 1].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos) |
				PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC |
				PDMA_REQ_SINGLE;
			I3C_DSCT[pdma_ch * 2 + 1].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
			I3C_DSCT[pdma_ch * 2 + 1].ENDDA =
				(uint32_t)&(((struct i3c_reg *) I3C_BASE_ADDR(port))->MWDATABE);
		}

		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) | I3C_MDMACTRL_DMATB(2));
		irq_unlock(key);
		return true;
	}

	/* Prepare for Master read slave's register, or Slave send IBI data */
	if (txLen == 1) {
		if (I3C_GET_REG_STATUS(port) & I3C_STATUS_TXNOTFULL_MASK) {
			I3C_SET_REG_WDATABE(port, *txBuf);
			irq_unlock(key);
			return true;
		}

		PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
		I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);

		Temp = I3C_GET_REG_DATACTRL(port);
		if (Temp & I3C_DATACTRL_TXCOUNT_MASK) {
			LOG_WRN("[SWrite] txcount = %d", (Temp & I3C_DATACTRL_TXCOUNT_MASK) >> I3C_DATACTRL_TXCOUNT_SHIFT);
			I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);
		}

		PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

		PDMA_TxBuf_END[pdma_ch] = txBuf[0];

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos)
				| PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC
				| PDMA_REQ_SINGLE;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA =
			(uint32_t) &(((struct i3c_reg *)I3C_BASE_ADDR(port))->WDATABE);
	} else {
		PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);
		I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);

		Temp = I3C_GET_REG_DATACTRL(port);
		if (Temp & I3C_DATACTRL_TXCOUNT_MASK) {
			LOG_WRN("[SWrite] txcount = %d", (Temp & I3C_DATACTRL_TXCOUNT_MASK) >> I3C_DATACTRL_TXCOUNT_SHIFT);
			I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);
		}

		PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL = PDMA_OP_SCATTER;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2];

		I3C_DSCT[pdma_ch * 2].CTL = (((uint32_t) txLen - 2) << PDMA_DSCT_CTL_TXCNT_Pos) |
			PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_8 | PDMA_OP_SCATTER |
			PDMA_REQ_SINGLE;
		I3C_DSCT[pdma_ch * 2].ENDSA = (uint32_t)&txBuf[0];
		I3C_DSCT[pdma_ch * 2].ENDDA =
			(uint32_t)&(((struct i3c_reg *)I3C_BASE_ADDR(port))->WDATAB1);
		I3C_DSCT[pdma_ch * 2].NEXT = (uint32_t)&I3C_DSCT[pdma_ch * 2 + 1];

		PDMA_TxBuf_END[pdma_ch] = txBuf[txLen - 1];

		I3C_DSCT[pdma_ch * 2 + 1].CTL = (((uint32_t)0) << PDMA_DSCT_CTL_TXCNT_Pos) |
			PDMA_SAR_INC | PDMA_DAR_FIX | PDMA_WIDTH_32 | PDMA_OP_BASIC |
			PDMA_REQ_SINGLE;
		I3C_DSCT[pdma_ch * 2 + 1].ENDSA = (uint32_t)&PDMA_TxBuf_END[pdma_ch];
		I3C_DSCT[pdma_ch * 2 + 1].ENDDA =
			(uint32_t)&(((struct i3c_reg *)I3C_BASE_ADDR(port))->WDATABE);
	}

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) | I3C_DMACTRL_DMATB(2));
	irq_unlock(key);
	return true;
}

bool hal_I3C_DMA_Read(I3C_PORT_Enum port, I3C_DEVICE_MODE_Enum mode, uint8_t *rxBuf, uint16_t rxLen)
{
	uint8_t pdma_ch;
	uint32_t Temp;
	uint32_t key;

	if (rxLen == 0)
		return true;
	if (port >= I3C_PORT_MAX)
		return false;
	if ((mode != I3C_DEVICE_MODE_CURRENT_MASTER) && (mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		(mode != I3C_DEVICE_MODE_SECONDARY_MASTER)) {
		return false;
	}

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);

	key = irq_lock();

	PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);

	if (mode == I3C_DEVICE_MODE_CURRENT_MASTER) {
		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) & ~I3C_MDMACTRL_DMAFB_MASK);

		Temp = I3C_GET_REG_MDATACTRL(port);

		if (Temp & I3C_MDATACTRL_RXCOUNT_MASK) {
			LOG_WRN("[MRead] rxcount = %d", (Temp & I3C_MDATACTRL_RXCOUNT_MASK) >> I3C_MDATACTRL_RXCOUNT_SHIFT);
			/* I3C_SET_REG_MDATACTRL(port, I3C_GET_REG_MDATACTRL(port) | I3C_MDATACTRL_FLUSHFB_MASK); */
		}

		if (Temp & I3C_MDATACTRL_TXCOUNT_MASK) {
			LOG_DBG("[MRead] txcount = %d", (Temp & I3C_MDATACTRL_TXCOUNT_MASK) >> I3C_MDATACTRL_TXCOUNT_SHIFT);
		}

		PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
		PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

		PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL =
			(((uint32_t)rxLen - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_FIX |
				PDMA_DAR_INC | PDMA_WIDTH_8 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
			(uint32_t)&(((struct i3c_reg *)I3C_BASE_ADDR(port))->MRDATAB);
		PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA = (uint32_t)&rxBuf[0];

		I3C_SET_REG_MDMACTRL(port, I3C_GET_REG_MDMACTRL(port) | I3C_MDMACTRL_DMAFB(2));
		irq_unlock(key);
		return true;
	}

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMAFB_MASK);

	/* Try not flush Rx fifo, to prevent data loss */
	Temp = I3C_GET_REG_DATACTRL(port);

	if (Temp & I3C_DATACTRL_RXCOUNT_MASK) {
		LOG_WRN("[SRead] rxcount = %d", (Temp & I3C_DATACTRL_RXCOUNT_MASK) >> I3C_DATACTRL_RXCOUNT_SHIFT);
		/* I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHFB_MASK); */
	}

	if (Temp & I3C_DATACTRL_TXCOUNT_MASK) {
		LOG_DBG("[SRead] txcount = %d", (Temp & I3C_DATACTRL_TXCOUNT_MASK) >> I3C_DATACTRL_TXCOUNT_SHIFT);
	}

	PDMA->CHCTL |= BIT(PDMA_OFFSET + pdma_ch);
	PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);

	PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL =
		(((uint32_t)rxLen - 1) << PDMA_DSCT_CTL_TXCNT_Pos) | PDMA_SAR_FIX | PDMA_DAR_INC |
			PDMA_WIDTH_8 | PDMA_OP_BASIC | PDMA_REQ_SINGLE;
	PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDSA =
		(uint32_t)&(((struct i3c_reg *)I3C_BASE_ADDR(port))->RDATAB);
	PDMA->DSCT[PDMA_OFFSET + pdma_ch].ENDDA = (uint32_t)&rxBuf[0];

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) | I3C_DMACTRL_DMAFB(2));
	irq_unlock(key);
	return true;
}

uint8_t hal_i3c_get_dynamic_address(I3C_PORT_Enum port)
{
	uint32_t temp;

	temp = I3C_GET_REG_DYNADDR(port);

	if ((temp & 0x01) == 0)
		return I3C_DYNAMIC_ADDR_DEFAULT_7BIT;
	return (uint8_t)(temp >> 1);
}

bool hal_i3c_get_capability_support_master(I3C_PORT_Enum port)
{
	return true;
}

bool hal_i3c_get_capability_support_slave(I3C_PORT_Enum port)
{
	return true;
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
	return true;
}

bool hal_i3c_get_capability_support_V10(I3C_PORT_Enum port)
{
	return true;
}

bool hal_i3c_get_capability_support_V11(I3C_PORT_Enum port)
{
	return false;
}

uint8_t hal_I3C_get_event_support_status(I3C_PORT_Enum port)
{
	uint32_t status;

	status = I3C_GET_REG_STATUS(port);
	return (uint8_t)(status >> 24);
}

uint32_t hal_I3C_get_status(I3C_PORT_Enum port)
{
	return I3C_GET_REG_STATUS(port);
}

bool hal_I3C_run_ASYN0(I3C_PORT_Enum port)
{
	uint32_t status;

	status = I3C_GET_REG_STATUS(port);
	if ((status & I3C_STATUS_TIMECTRL_MASK) == I3C_STATUS_TIMECTRL(0x01))
		return true;
	return false;
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
	uint32_t key;

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
	pDevice = Get_Current_Master_From_Port(pTaskInfo->Port);

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
				rdterm = (pFrame->access_len - pFrame->access_idx);
				if (rdterm <= I3C_MCTRL_RDTERM_MAX)
					mctrl |= I3C_MCTRL_RDTERM(rdterm);
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
				Setup_Master_Read_DMA(pDevice);
			else {
				LOG_WRN("Configure Read DMA but already Enabled");
			}
		}

		mctrl |= I3C_MCTRL_REQUEST(I3C_MCTRL_REQUEST_EMIT_START);
	}

	/* lock irq, to avoid SLVSTART trigger faster than MCTRLDONE interrupt */
	key = irq_lock();

	I3C_SetXferRate(pTaskInfo);

	I3C_SET_REG_MCTRL(pTaskInfo->Port, mctrl);

	while ((I3C_GET_REG_MSTATUS(pTaskInfo->Port) & I3C_MSTATUS_MCTRLDONE_MASK) == 0);

	irq_unlock(key);

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Stop(I3C_PORT_Enum port)
{
	uint32_t mctrl;
	uint32_t mstatus;
	uint32_t mconfig;
	uint32_t key;

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

	key = irq_lock();

	I3C_SET_REG_MINTCLR(port, I3C_MINTCLR_MCTRLDONE_MASK);

	I3C_SET_REG_MCTRL(port, mctrl);

	/* We disable mctrldone interrupt, wait mctrl done after emit stop */
	while ((I3C_GET_REG_MSTATUS(port) & I3C_MSTATUS_MCTRLDONE_MASK) == 0);

	irq_unlock(key);

	return I3C_ERR_OK;
}

static void i3c_npcm4xx_reset(I3C_PORT_Enum port)
{
	struct i3c_npcm4xx_obj *obj;
	struct i3c_npcm4xx_config *config;
	struct pmc_reg *pmc_base;
	uint8_t sw_rst;

	obj = gObj[port];
	config = obj->config;

	/* disable i3c interrupt */
	irq_disable(config->irq);

	pmc_base = (struct pmc_reg *)config->pmc_base;

	/* save reset value */
	sw_rst = pmc_base->SW_RST1;

	/* trigger sw reset */
	pmc_base->SW_RST1 = (sw_rst | BIT(port));

	/* after reset done, partno become zero */
	while(I3C_GET_REG_PARTNO(port));

	/* restore reset value */
	pmc_base->SW_RST1 = sw_rst;

	/* re-init device configuration */
	hal_I3C_Config_Device(I3C_Get_INODE(port));

	/* enable irq interrupt */
	irq_enable(config->irq);
}

I3C_ErrCode_Enum hal_I3C_Stop_SlaveEvent(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	uint32_t ctrl;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	port = pTaskInfo->Port;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
			((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	ctrl = I3C_GET_REG_CTRL(port);

	/* clean up ibi data, extdata and event mask */
	ctrl &= ~(I3C_CTRL_IBIDATA_MASK | I3C_CTRL_EXTDATA_MASK | I3C_CTRL_EVENT_MASK);

	I3C_SET_REG_CTRL(port, ctrl);

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

	pDevice = I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	pTask = pTaskInfo->pTask;
	pFrame = &pTask->pFrameList[pTask->frame_idx];

	if (I3C_GET_REG_CTRL(port) & I3C_CTRL_EVENT_MASK) {
		LOG_WRN("Generarte IBI but CTRL in Progress: 0x%x\n",
				I3C_GET_REG_CTRL(port));
		hal_I3C_Stop_SlaveEvent(pTaskInfo);
	}

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
			Setup_Slave_IBI_DMA(pDevice);
		} else {
			ctrl |= I3C_CTRL_EXTDATA(1);

			/* MAX = data len, CNT = 0 -> use TX FIFO */
			I3C_SET_REG_IBIEXT1(port,
				I3C_IBIEXT1_MAX(pFrame->access_len - pFrame->access_idx));
			Setup_Slave_IBI_DMA(pDevice);
		}
	}

	ctrl |= I3C_CTRL_EVENT(I3C_CTRL_EVENT_IBI);

	I3C_SET_REG_CTRL(port, ctrl);

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Start_Master_Request(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	uint32_t ctrl;
	uint32_t key;

	if (pTaskInfo == NULL)
		return I3C_ERR_PARAMETER_INVALID;

	port = pTaskInfo->Port;
	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	if (I3C_GET_REG_CTRL(port) & I3C_CTRL_EVENT_MASK) {
		LOG_WRN("Generarte Master REQ but CTRL in Progress: 0x%x\n",
				I3C_GET_REG_CTRL(port));
		hal_I3C_Stop_SlaveEvent(pTaskInfo);
	}

	key = irq_lock();

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & I3C_DMACTRL_DMATB_MASK);
	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

	irq_unlock(key);

	pDevice->txLen = 0;
	pDevice->txOffset = 0;
	pDevice->rxLen = 0;
	pDevice->rxOffset = 0;

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl |= I3C_CTRL_EVENT(I3C_CTRL_EVENT_MstReq);

	I3C_SET_REG_CTRL(port, ctrl);
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Start_HotJoin(I3C_TASK_INFO_t *pTaskInfo)
{
	I3C_PORT_Enum port;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_ErrCode_Enum ret = I3C_ERR_OK;
	struct i3c_npcm4xx_obj *obj = NULL;
	uint32_t ctrl;
	uint8_t retry;
	uint8_t check_count;
	uint32_t key;

	if (pTaskInfo == NULL) {
		LOG_ERR("HotJoin: pTaskInfo NULL\n");
		ret = I3C_ERR_PARAMETER_INVALID;
		goto hj_exit;
	}

	port = pTaskInfo->Port;
	if (port >= I3C_PORT_MAX) {
		LOG_ERR("HotJoin: Port not valid = 0x%x\n", port);
		ret = I3C_ERR_PARAMETER_INVALID;
		goto hj_exit;
	}

	obj = gObj[port];

	pTask = pTaskInfo->pTask;

	/* check dynamic address already present or not */
	if (I3C_Update_Dynamic_Address(port)) {
		LOG_WRN("HotJoin: DA present before generate Hot-Join\n");
		if (pTask) {
			I3C_Slave_End_Request((uint32_t)pTask);
		}

		ret = I3C_ERR_OK;
		goto hj_exit;
	}

	/* check daa is under progress or not */
	if (I3C_GET_REG_STATUS(port) & I3C_STATUS_STDAA_MASK) {
		LOG_WRN("HotJoin: STDAA work in progress.\n");
		if (pTask) {
			I3C_Slave_End_Request((uint32_t)pTask);
		}

		ret = I3C_ERR_OK;
		goto hj_exit;
	}

	pDevice = I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER))) {
		LOG_ERR("HotJoin: Only support slave or secondary master\n");
		ret = I3C_ERR_PARAMETER_INVALID;
		goto hj_exit;
	}

	if (I3C_GET_REG_CTRL(port) & I3C_CTRL_EVENT_MASK) {
		LOG_WRN("Generarte HJ but CTRL in Progress: 0x%x\n",
				I3C_GET_REG_CTRL(port));
		hal_I3C_Stop_SlaveEvent(pTaskInfo);
	}

	key = irq_lock();

	I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) & ~I3C_DMACTRL_DMATB_MASK);
	I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port) | I3C_DATACTRL_FLUSHTB_MASK);

	irq_unlock(key);

	pDevice->txLen = 0;
	pDevice->txOffset = 0;
	pDevice->rxLen = 0;
	pDevice->rxOffset = 0;

	ctrl = I3C_GET_REG_CTRL(port);
	ctrl &= ~(I3C_CTRL_IBIDATA_MASK | I3C_CTRL_EXTDATA_MASK | I3C_CTRL_EVENT_MASK);
	ctrl |= I3C_CTRL_EVENT(I3C_CTRL_EVENT_HotJoin);

	/* initial retry and check count */
        retry = 0x0;
        check_count = 0x0;

hj_retry:
	/* wait util bus idle */
	while(I3C_GET_REG_STATUS(port) & I3C_STATUS_STNOTSTOP_MASK);

	/* if daa already done, exit hot-join progress */
	if (I3C_Update_Dynamic_Address(port)) {
		LOG_WRN("HotJoin: Bus BUS idle and DA present\n");
		if (pTask) {
			I3C_Slave_End_Request((uint32_t)pTask);
		}

		ret = I3C_ERR_OK;
		goto hj_exit;
	}

	/* HotJoin only could generate when bus idle, otherwise hw may stuck. */
	I3C_SET_REG_CTRL(port, ctrl);
	I3C_SET_REG_CONFIG(port, I3C_GET_REG_CONFIG(port) | I3C_CONFIG_SLVENA_MASK);

	do {
		k_busy_wait(NPCM4XX_I3C_HJ_UDELAY);

		if ((I3C_GET_REG_CTRL(port) & I3C_CTRL_EVENT_MASK) ==
				I3C_CTRL_EVENT_None) {
			LOG_WRN("HotJoin: Send successful\n");
			ret = I3C_ERR_OK;
			goto hj_exit;
		} else {
			check_count++;
		}

		if (check_count >= NPCM4XX_I3C_HJ_CHECK_MAX)
			break;

	} while(true);

	if (I3C_Update_Dynamic_Address(port)) {
		LOG_WRN("HotJoin: Try to generate event but DA=0x%x present\n",
				I3C_Update_Dynamic_Address(port));

		/* Disable generate event */
		I3C_SET_REG_CTRL(port, I3C_CTRL_EVENT_None);

		if (pTask) {
			I3C_Slave_End_Request((uint32_t)pTask);
		}

		ret = I3C_ERR_OK;
		goto hj_exit;
	}

	/* reset i3c hw since there is no dynamic address present */
	i3c_npcm4xx_reset(port);

	/* add retry count */
	retry++;

	if (retry >= NPCM4XX_I3C_HJ_RETRY_MAX) {
		LOG_ERR("HotJoin: Send event failed\n");
		if (pTask) {
			I3C_Slave_End_Request((uint32_t)pTask);
		}

		ret = I3C_ERR_BUS_ERROR;
		goto hj_exit;
	} else {
		/* re-initial check count */
		check_count = 0x0;
		/* retry again */
		goto hj_retry;
	}

hj_exit:
	if (obj) {
		obj->config->hj_req = I3C_HOT_JOIN_STATE_None;
	}

	return ret;
}

I3C_ErrCode_Enum hal_I3C_Slave_TX_Free(I3C_PORT_Enum port)
{
	I3C_DEVICE_INFO_t *pDevice;
	uint16_t txDataLen;

	if (port >= I3C_PORT_MAX)
		return I3C_ERR_PARAMETER_INVALID;

	pDevice = I3C_Get_INODE(port);
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

	pDevice = I3C_Get_INODE(port);
	if ((pDevice->mode != I3C_DEVICE_MODE_SLAVE_ONLY) &&
		((pDevice->mode != I3C_DEVICE_MODE_SECONDARY_MASTER)))
		return I3C_ERR_PARAMETER_INVALID;

	/* Slave TX data has send ? DMA will move response data to FIFO automatically */
	/* Response data still in Tx FIFO ? */
	txDataLen = (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_TXCOUNT_MASK) >>
		I3C_DATACTRL_TXCOUNT_SHIFT;

	*txLen = txDataLen;
	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Stop_Slave_TX(I3C_DEVICE_INFO_t *pDevice)
{
	uint32_t dmactrl;
	uint32_t datactrl;
	uint32_t key;

	key = irq_lock();

	dmactrl = I3C_GET_REG_DMACTRL(pDevice->port);
	datactrl = I3C_GET_REG_DATACTRL(pDevice->port);
	dmactrl &= ~I3C_DMACTRL_DMATB_MASK;
	I3C_SET_REG_DMACTRL(pDevice->port, dmactrl);

	PDMA->TDSTS = BIT(PDMA_OFFSET + pDevice->port);
	PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pDevice->port);

	datactrl |= I3C_DATACTRL_FLUSHTB_MASK;
	I3C_SET_REG_DATACTRL(pDevice->port, datactrl);

	irq_unlock(key);

	return I3C_ERR_OK;
}

I3C_ErrCode_Enum hal_I3C_Set_Pending(I3C_PORT_Enum port, uint8_t mask)
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

void *hal_I3C_MemAlloc(uint32_t Size)
{
	return k_malloc(Size);
}

void hal_I3C_MemFree(void *pv)
{
	k_free(pv);
}

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
	uint8_t pec_v;
	uint8_t address;
	uint8_t addr_rnw;
	int ret;

	if (len == 0 || ptr == NULL)
		return -EINVAL;

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
	pDevice = I3C_Get_INODE(port);

	ret = I3C_Slave_Prepare_Response(pDevice, nbytes, bytes);
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
	pDevice = I3C_Get_INODE(port);
	pBus = Get_Bus_From_Port(port);

	while (pDevice->pTaskListHead != NULL) {
		key = k_spin_lock(&obj->lock);
		if (pBus->pCurrentTask != NULL) {
			k_spin_unlock(&obj->lock, key);
			k_yield();
		} else {
			pTask = pDevice->pTaskListHead;
			if (pTask) {
				pBus->pCurrentTask = pTask;
			} else {
				k_spin_unlock(&obj->lock, key);
				break;
			}

			pTaskInfo = pTask->pTaskInfo;

			I3C_Master_Start_Request((uint32_t)pTaskInfo);

			k_spin_unlock(&obj->lock, key);
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
	uint8_t convert_pid[8] = {0};
	I3C_BUS_INFO_t *pBus = NULL;
	I3C_DEVICE_INFO_t *pDevice = NULL;
	I3C_DEVICE_INFO_SHORT_t *pDevInfo = NULL;
	I3C_DEVICE_ATTRIB_t attr;

	obj = DEV_DATA(dev);
	config = obj->config;
	port = config->inst_id;
	pDevice = I3C_Get_INODE(port);

	if (pDevice == NULL) {
		return -ENODEV;
	}

	pBus = pDevice->pOwner;

	if (pBus == NULL) {
		return -ENODEV;
	}

	/* Use for ENTDAA */
	attr.b.present = 1;

	/* up layer send u64 little endian format, covert to big endian
	 * and suppose pid only need 6 bytes
	 */
	sys_put_be64(slave->info.pid, convert_pid);

	pDevInfo = NewDevInfo(pBus, slave, attr, slave->info.static_addr, slave->info.assigned_dynamic_addr,
			(uint8_t *)&convert_pid[2], slave->info.bcr, slave->info.dcr);

	if (pDevInfo == NULL) {
		return -ENOMEM;
	}

	if (slave->info.i2c_mode) {
		slave->info.dynamic_addr = slave->info.static_addr;
	} else if (slave->info.assigned_dynamic_addr) {
		slave->info.dynamic_addr = slave->info.assigned_dynamic_addr;
	}

	pos = i3c_npcm4xx_get_pos(obj, slave->info.dynamic_addr);
	if (pos >= 0) {
		LOG_WRN("addr %x has been registered at %d, update information\n",
			slave->info.dynamic_addr, pos);

		/* assign dev as slave's bus controller */
		if (slave->bus == NULL) {
			slave->bus = dev;
		}

		/* assign priv data
		 * TODO: Add reference count for pDevInfo and free priv_data
		 */
		if (slave->priv_data == NULL) {
			slave->priv_data = obj->dev_descs[pos]->priv_data;
		}

		return 0;
	} else {
		/* find a free position from master's hw_dat_free_pos */
		for (i = 0; i < DEVICE_COUNT_MAX; i++) {
			if (obj->hw_dat_free_pos & BIT(i))
				break;
		}

		/* can't attach if no free space */
		if (i == DEVICE_COUNT_MAX) {
			RemoveDevInfo(pBus, pDevInfo);
			return -ENOMEM;
		}

		/* assign dev as slave's bus controller */
		slave->bus = dev;

		/* allocate private data of the device */
		priv = (struct i3c_npcm4xx_dev_priv *)hal_I3C_MemAlloc(sizeof(struct i3c_npcm4xx_dev_priv));
		if (priv == NULL) {
			RemoveDevInfo(pBus, pDevInfo);
			return -ENOMEM;
		}

		priv->pos = -1;
		priv->ibi.enable = false;
		priv->ibi.callbacks = NULL;
		priv->ibi.context = slave;
		priv->ibi.incomplete = NULL;
		slave->priv_data = priv;

		priv->pos = i;

		obj->dev_addr_tbl[i] = slave->info.dynamic_addr;
		obj->dev_descs[i] = slave;
		obj->hw_dat_free_pos &= ~BIT(i);
	}

	return 0;
}

int i3c_npcm4xx_master_detach_device(const struct device *dev, struct i3c_dev_desc *slave)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(dev);
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(slave);
	struct i3c_npcm4xx_config *config = NULL;
	I3C_DEVICE_INFO_SHORT_t *pDevInfo;
	I3C_DEVICE_INFO_t *pDevice = NULL;
	I3C_BUS_INFO_t *pBus = NULL;
	I3C_PORT_Enum port;
	int pos;
	uint8_t convert_pid[8] = {0};

	config = obj->config;
	port = config->inst_id;
	pDevice = I3C_Get_INODE(port);
	pBus = pDevice->pOwner;

	/* Remove by dynamic address */
	if (slave->info.assigned_dynamic_addr != 0) {
		pDevInfo = GetDevInfoByDynamicAddr(pBus, slave->info.assigned_dynamic_addr);
	} else {
		/* up layer send u64 little endian format, covert to big endian
		 * and suppose pid only need 6 bytes
		 */
		sys_put_be64(slave->info.pid, convert_pid);

		if (IsValidPID((uint8_t *)&convert_pid[2])) {
			pDevInfo = GetDevInfoByPID(pBus, (uint8_t *)&convert_pid[2]);
		} else {
			LOG_ERR("Invalid PID");
			return -EINVAL;
		}
	}

	if (pDevInfo) {
		RemoveDevInfo(pBus, pDevInfo);
	} else {
		return -ENODEV;
	}

	pos = priv->pos;
	if (pos < 0) {
		return pos;
	}

	obj->hw_dat_free_pos |= BIT(pos);
	obj->dev_addr_tbl[pos] = 0;

	hal_I3C_MemFree(slave->priv_data);
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

int i3c_npcm4xx_slave_set_static_addr(const struct device *dev, uint8_t static_addr)
{
        struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port = config->inst_id;
	I3C_DEVICE_INFO_t *pDevice;
	uint32_t sconfig;

	pDevice = &gI3c_dev_node_internal[port];
	pDevice->staticAddr = static_addr;

	sconfig = I3C_GET_REG_CONFIG(port);
	SET_FIELD(sconfig, NPCM4XX_I3C_CONFIG_SADDR, static_addr);
	I3C_SET_REG_CONFIG(port, sconfig);

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
	I3C_TASK_INFO_t *pTaskInfo;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_PORT_Enum port;
	uint32_t event_en;
	int ret;
	uint8_t *xfer_buf;
	I3C_DEVICE_INFO_t *pDevice;
	int iRet = 0;

	__ASSERT_NO_MSG(data);
	__ASSERT_NO_MSG(data->buf);
	__ASSERT_NO_MSG(data->size);

	config = DEV_CFG(dev);
	port = config->inst_id;
	pDevice = I3C_Get_INODE(port);

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
			I3C_Slave_Update_Pending(pDevice, 0x01);
			k_mutex_unlock(&pDevice->lock);
			return 0;
		}

		/* init ibi complete sem */
		k_sem_init(&pDevice->ibi_complete, 0, 1);

		/* osEventFlagsClear(obj->ibi_event, ~osFlagsError); */

		uint16_t txlen;
		uint16_t rxlen = 0;
		uint8_t TxBuf[2];
		I3C_TRANSFER_PROTOCOL_Enum protocol = I3C_TRANSFER_PROTOCOL_IBI;
		uint32_t timeout = TIMEOUT_TYPICAL;

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
		pTaskInfo = I3C_Slave_Create_Task(protocol, txlen, &txlen, &rxlen, TxBuf, NULL,
			timeout, NULL, port, NOT_HIF);
		k_work_submit_to_queue(&npcm4xx_i3c_work_q[port], &work_send_ibi[port]);

		/* wait ibi master read complete done */
		iRet = k_sem_take(&pDevice->ibi_complete, K_MSEC(100));

		if (iRet != 0) {
			LOG_ERR("wait master read timeout %d", iRet);
			pTask = pTaskInfo->pTask;
			/* cancel slave event */
			hal_I3C_Stop_SlaveEvent(pTaskInfo);
			/* remove ibi task from queue */
			I3C_Slave_End_Request((uint32_t)pTask);
			/* stop TX and DMA */
			hal_I3C_Stop_Slave_TX(pDevice);
			/* release memory resource */
			I3C_Slave_Finish_Response(pDevice);
			k_mutex_unlock(&pDevice->lock);
			return iRet;
		}
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

int i3c_npcm4xx_slave_hj_req(const struct device *dev)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port;
	uint8_t addr;

	port = config->inst_id;

	addr = I3C_Update_Dynamic_Address((uint32_t) port);

	if (addr) {
		LOG_ERR("Dynamic address present = 0x%x\n", addr);
		return -1;
	}

	if (config->hj_req == I3C_HOT_JOIN_STATE_None) {
		if (config->rst_reason == NPCM4XX_RESET_REASON_VCC_POWERUP) {
			LOG_WRN("Auto Hot-Join\n");
			config->hj_req = I3C_HOT_JOIN_STATE_Request;
			/* change reset reason to sw-rst, direct hot-join next time */
			config->rst_reason = NPCM4XX_RESET_REASON_DEBUGGER_RST;
		} else {
			LOG_WRN("Direct Hot-Join\n");
			I3C_Slave_Insert_Task_HotJoin(port);
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[port], &work_send_ibi[port]);
			config->hj_req = I3C_HOT_JOIN_STATE_Queue;
		}
	} else {
		LOG_WRN("Hot-Join request progress, state = %d\n", config->hj_req);
	}

	return 0;
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

int i3c_npcm4xx_set_pid_extra_info(const struct device *dev, uint16_t extra_info)
{
	struct i3c_npcm4xx_config *config = DEV_CFG(dev);
	I3C_PORT_Enum port = config->inst_id;
	I3C_DEVICE_INFO_t *pDevice;
	uint32_t partno;

	partno = I3C_GET_REG_PARTNO(port);

	SET_FIELD(partno, NPCM4XX_I3C_PARTNO_VENDOR_DEF, extra_info);
	I3C_SET_REG_PARTNO(port, partno);

	pDevice = &gI3c_dev_node_internal[port];
	pDevice->partNumber = partno;

	pDevice->pid[4] = ((port & 0x0F) << 4) |
		((uint8_t)(extra_info >> 8) & 0x0F);
	pDevice->pid[5] = (uint8_t)extra_info;

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

	pDevice = I3C_Get_INODE(port);
	pTask = pDevice->pTaskListHead;
	payload->size = *pTask->pRdLen;
	memcpy(payload->buf, pTask->pRdBuf, payload->size);

	/* notify, ibi task has been processed */
	priv->ibi.callbacks->write_done(priv->ibi.context);
}

static uint32_t i3c_npcm4xx_master_send_done(void *pCallbackData, struct I3C_CallBackResult *CallBackResult)
{
	struct i3c_npcm4xx_xfer *xfer;
	k_spinlock_key_t key;

	if (CallBackResult == NULL) {
		LOG_ERR("CallBackResult NULL!");
		return -EINVAL;
	}

	xfer = (struct i3c_npcm4xx_xfer *) pCallbackData;

	key = k_spin_lock(&xfer->lock);

	if (xfer->abort == true) {
		k_spin_unlock(&xfer->lock, key);
		LOG_ERR("command abort, free buffer");
		hal_I3C_MemFree(xfer);
		return 0;
	}

	xfer->rx_len = CallBackResult->rx_len;
	xfer->tx_len = CallBackResult->tx_len;
	xfer->ret = CallBackResult->result;
	xfer->complete = true;

	/* context switch may happened after spin_unlock() */
	k_spin_unlock(&xfer->lock, key);

	k_sem_give(&xfer->xfer_complete);

	return 0;
}

int i3c_npcm4xx_master_send_ccc(const struct device *dev, struct i3c_ccc_cmd *ccc)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(dev);
	struct i3c_npcm4xx_config *config;
	struct i3c_npcm4xx_xfer *xfer;
	int pos = 0, ret = 0;
	k_spinlock_key_t key;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_ErrCode_Enum result;

	/* To construct task */
	uint8_t CCC;
	uint32_t Baudrate = 0;
	uint32_t Timeout = TIMEOUT_TYPICAL;
	ptrI3C_RetFunc pCallback = i3c_npcm4xx_master_send_done;
	uint8_t PortId;
	I3C_TASK_POLICY_Enum Policy = I3C_TASK_POLICY_APPEND_LAST;
	bool bHIF = NOT_HIF;

	uint16_t TxLen = 0;
	uint8_t TxBuf[I3C_PAYLOAD_SIZE_MAX] = { 0 };
	uint16_t RxLen;
	uint8_t *RxBuf = NULL;

	if (ccc->id & I3C_CCC_DIRECT) {
		pos = i3c_npcm4xx_get_pos(obj, ccc->addr);
		if (pos < 0) {
			return pos;
		}
	}

	config = obj->config;

	CCC = ccc->id;
	PortId = (uint8_t)(config->inst_id);
	Baudrate = config->i3c_scl_hz;
	Policy = I3C_TASK_POLICY_APPEND_LAST;
	Timeout = TIMEOUT_TYPICAL;
	bHIF = IS_HIF;
	pDevice = obj->pDevice;

	xfer = (struct i3c_npcm4xx_xfer *)hal_I3C_MemAlloc(sizeof(struct i3c_npcm4xx_xfer));

	if (xfer) {
		xfer->ncmds = 1;
		xfer->abort = false;
		xfer->complete = false;
		xfer->ret = -ETIMEDOUT;
	} else {
		return -ENOMEM;
	}

	k_mutex_lock(&pDevice->lock, K_FOREVER);

	if (ccc->id & I3C_CCC_DIRECT) {
		if ((CCC == CCC_DIRECT_ENEC) || (CCC == CCC_DIRECT_DISEC)) {
			if (ccc->payload.length != 1) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = 2;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], ccc->payload.data, 1);
			result = I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_RSTDAA) {
			if (ccc->payload.length != 0) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = 1;
			TxBuf[0] = ccc->addr;
			result = I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETMWL) {
			if (ccc->payload.length != 2) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = 3;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], ccc->payload.data, 2);
			result = I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETMRL) {
			if ((ccc->payload.length != 2) && (ccc->payload.length != 3)) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = ccc->payload.length + 1;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], ccc->payload.data, ccc->payload.length);
			result = I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_SETDASA) {
			if (ccc->payload.length != 1) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = 2;
			TxBuf[0] = ccc->addr;
			memcpy(&TxBuf[1], ccc->payload.data, ccc->payload.length);
			result = I3C_Master_Insert_Task_CCCw(CCC, 1, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETMWL) {
			TxLen = 1;
			RxLen = 2;
			TxBuf[0] = ccc->addr;
			RxBuf = ccc->payload.data;
			result = I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, &RxLen, TxBuf, RxBuf,
					Baudrate, Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETMRL) {
			TxLen = 1;
			RxLen = 3;
			TxBuf[0] = ccc->addr;
			RxBuf = ccc->payload.data;
			result = I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, &RxLen, TxBuf, RxBuf,
					Baudrate, Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETPID) {
			TxLen = 1;
			RxLen = 6;
			TxBuf[0] = ccc->addr;
			RxBuf = ccc->payload.data;
			result = I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, &RxLen, TxBuf, RxBuf,
					Baudrate, Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if ((CCC == CCC_DIRECT_GETBCR) || (CCC == CCC_DIRECT_GETDCR)
			|| (CCC == CCC_DIRECT_GETACCMST)) {
			TxLen = 1;
			RxLen = 1;
			TxBuf[0] = ccc->addr;
			RxBuf = ccc->payload.data;
			result = I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, &RxLen, TxBuf, RxBuf,
					Baudrate,Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else if (CCC == CCC_DIRECT_GETSTATUS) {
			TxLen = 1;
			RxLen = 2;
			TxBuf[0] = ccc->addr;
			RxBuf = ccc->payload.data;
			result = I3C_Master_Insert_Task_CCCr(CCC, 1, TxLen, &RxLen, TxBuf, RxBuf,
					Baudrate, Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		} else {
			k_mutex_unlock(&pDevice->lock);
			return -EINVAL;
		}
	} else {
		if ((CCC == CCC_BROADCAST_ENEC) || (CCC == CCC_BROADCAST_DISEC)) {
			if (ccc->payload.length != 1) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = ccc->payload.length;
			memcpy(TxBuf, ccc->payload.data, TxLen);
		} else if ((CCC == CCC_BROADCAST_RSTDAA) ||
			(CCC == CCC_BROADCAST_SETAASA)) {
			TxLen = 0;
		} else if (CCC == CCC_BROADCAST_SETMWL) {
			if (ccc->payload.length != 2) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = ccc->payload.length;
			memcpy(TxBuf, ccc->payload.data, TxLen);
		} else if (CCC == CCC_BROADCAST_SETMRL) {
			if ((ccc->payload.length != 1) || (ccc->payload.length != 3)) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}
			if (ccc->payload.data == NULL) {
				k_mutex_unlock(&pDevice->lock);
				return -EINVAL;
			}

			TxLen = ccc->payload.length;
			memcpy(TxBuf, ccc->payload.data, TxLen);
		} else if (CCC == CCC_BROADCAST_SETHID) {
			TxLen = 1;
			TxBuf[0] = 0x00;
		} else if (CCC == CCC_BROADCAST_DEVCTRL) {
			TxLen = 3;

			TxBuf[0] = 0xE0;
			TxBuf[1] = 0x00;
			TxBuf[2] = 0x00;
		} else {
			k_mutex_unlock(&pDevice->lock);
			return -EINVAL;
		}

		result = I3C_Master_Insert_Task_CCCb(CCC, TxLen, TxBuf, Baudrate, Timeout,
				pCallback, (void *)xfer, PortId, Policy, bHIF);
	}

	if (result != I3C_ERR_OK) {
		LOG_ERR("Create CCC task failed");
		k_mutex_unlock(&pDevice->lock);
		hal_I3C_MemFree(xfer);
		return -EINVAL;
	}

	k_sem_init(&xfer->xfer_complete, 0, 1);

	i3c_npcm4xx_start_xfer(obj, xfer);

	/* wait done, xfer.ret will be changed in ISR */
	if(k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
		key = k_spin_lock(&xfer->lock);
		if (xfer->complete == true) {
			/* In this case, means context switch after callback function
			 * call spin_unlock() and timeout happened.
			 * Take semaphore again to make xfer_complete synchronization.
			 */
			LOG_WRN("sem timeout but task complete, get sem again to clear flag");
			k_spin_unlock(&xfer->lock, key);
			if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
				LOG_WRN("take sem again timeout");
			}
		} else { /* the memory will be free when driver call pCallback */
			xfer->abort = true;
			ret = xfer->ret;
			k_spin_unlock(&xfer->lock, key);
			k_mutex_unlock(&pDevice->lock);
			LOG_ERR("CCC command timeout abort");
			return ret;
		}
	}

	ret = xfer->ret;

	k_mutex_unlock(&pDevice->lock);

	hal_I3C_MemFree(xfer);

	return ret;
}

/* i3cdev -> target device */
/* xfers -> send out data to slave or read in data from slave */
int i3c_npcm4xx_master_priv_xfer(struct i3c_dev_desc *i3cdev, struct i3c_priv_xfer *xfers,
	int nxfers)
{
	struct i3c_npcm4xx_obj *obj = DEV_DATA(i3cdev->bus);
	struct i3c_npcm4xx_dev_priv *priv = DESC_PRIV(i3cdev);
	struct i3c_npcm4xx_xfer *xfer;
	int pos;
	int i, ret;
	k_spinlock_key_t key;
	I3C_DEVICE_INFO_t *pDevice;

	I3C_TASK_INFO_t *pTaskInfo;
	bool bWnR;
	I3C_TRANSFER_PROTOCOL_Enum Protocol;
	uint8_t Addr;
	uint16_t TxLen = 0;
	uint16_t RxLen = 0;
	uint8_t *TxBuf = NULL;
	uint8_t *RxBuf = NULL;
	uint32_t Baudrate;
	uint32_t Timeout = TIMEOUT_TYPICAL;
	ptrI3C_RetFunc pCallback = i3c_npcm4xx_master_send_done;
	uint8_t PortId = obj->config->inst_id;
	I3C_TASK_POLICY_Enum Policy = I3C_TASK_POLICY_APPEND_LAST;
	bool bHIF = IS_HIF;

	I3C_BUS_INFO_t *pBus;
	I3C_DEVICE_INFO_t *pMaster;
	struct i3c_dev_desc *pSlaveDev;

	if (!nxfers) {
		return 0;
	}

	pBus = Get_Bus_From_Port(PortId);
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
	pSlaveDev = obj->dev_descs[pos];
	pDevice = obj->pDevice;

	if (pSlaveDev == NULL)
		return DEVICE_COUNT_MAX;

	xfer = (struct i3c_npcm4xx_xfer *)hal_I3C_MemAlloc(sizeof(struct i3c_npcm4xx_xfer));

	if (xfer) {
		xfer->abort = false;
		xfer->complete = false;
		xfer->ncmds = nxfers;
		xfer->ret = -ETIMEDOUT;
	} else {
		return -ENOMEM;
	}

	k_mutex_lock(&pDevice->lock, K_FOREVER);

	for (i = 0; i < nxfers; i++) {
		bWnR = (((i + 1) < nxfers) && (xfers[i].rnw == 0) && (xfers[i + 1].rnw == 1));

		Baudrate = (pSlaveDev->info.i2c_mode) ? obj->config->i2c_scl_hz :
			obj->config->i3c_scl_hz;

		/* Try to slow down for the fly line */
		/* Baudrate = (pSlaveDev->attr.b.runI3C) ? I3C_TRANSFER_SPEED_SDR_6MHZ : */
		/*	I3C_TRANSFER_SPEED_I2C_100KHZ;	*/

		if (!bWnR) {
			if (xfers[i].rnw) {
				Protocol = (pSlaveDev->info.i2c_mode) ?
					I3C_TRANSFER_PROTOCOL_I2C_READ :
					I3C_TRANSFER_PROTOCOL_I3C_READ;
				RxBuf = xfers[i].data.in;
				RxLen = xfers[i].len;
			} else {
				Protocol = (pSlaveDev->info.i2c_mode) ?
					I3C_TRANSFER_PROTOCOL_I2C_WRITE :
					I3C_TRANSFER_PROTOCOL_I3C_WRITE;
				TxBuf = xfers[i].data.out;
				TxLen = xfers[i].len;
			}
		} else {
			Protocol = (pSlaveDev->info.i2c_mode) ?
				I3C_TRANSFER_PROTOCOL_I2C_WRITEnREAD :
				I3C_TRANSFER_PROTOCOL_I3C_WRITEnREAD;
			TxBuf = xfers[i].data.out;
			TxLen = xfers[i].len;
			RxBuf = xfers[i + 1].data.in;
			RxLen = xfers[i + 1].len;
		}

		pTaskInfo = I3C_Master_Create_Task(Protocol, Addr, 0, &TxLen, &RxLen, TxBuf, RxBuf,
			Baudrate, Timeout, pCallback, (void *)xfer, PortId, Policy, bHIF);
		if (pTaskInfo != NULL) {

			k_sem_init(&xfer->xfer_complete, 0, 1);

			i3c_npcm4xx_start_xfer(obj, xfer);

			/* wait done, xfer.ret will be changed in ISR */
			if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_XFER_TIMEOUT) != 0) {
				key = k_spin_lock(&xfer->lock);
				if (xfer->complete == true) {
					/* In this case, means context switch after callback function
					 * call spin_unlock() and timeout happened.
					 * Take semaphore again to make xfer_complete synchronization.
					 */
					LOG_WRN("sem timeout but task complete, get sem again to clear flag");
					k_spin_unlock(&xfer->lock, key);
					if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_XFER_TIMEOUT) != 0) {
						LOG_WRN("take sem again timeout");
					}
				} else { /* the memory will be free when driver call pCallback */
					xfer->abort = true;
					ret = xfer->ret;
					k_spin_unlock(&xfer->lock, key);
					k_mutex_unlock(&pDevice->lock);
					LOG_ERR("Xfer command timeout abort");
					return ret;
				}
			}

			/* report actual read length */
			if (bWnR) {
				i++;
			}

			if (xfers[i].rnw == 1)
				xfers[i].len = xfer->rx_len;
		} else {
			LOG_ERR("Create Master Xfer Node failed");
			xfer->ret = -ENOMEM;
		}
	}

	ret = xfer->ret;

	k_mutex_unlock(&pDevice->lock);

	hal_I3C_MemFree(xfer);

	return ret;
}

int i3c_npcm4xx_master_send_entdaa(struct i3c_dev_desc *i3cdev)
{
	struct i3c_npcm4xx_config *config;
	I3C_PORT_Enum port;
	int ret = 0;

	uint16_t rxlen = 63;
	uint8_t RxBuf_expected[63];
	uint32_t Baudrate = 0;
	k_spinlock_key_t key;
	I3C_DEVICE_INFO_t *pDevice;
	I3C_ErrCode_Enum result;

	config = DEV_CFG(i3cdev->bus);
	port = config->inst_id;

	struct i3c_npcm4xx_obj *obj = DEV_DATA(i3cdev->bus);
	struct i3c_npcm4xx_xfer *xfer;
	ptrI3C_RetFunc pCallback = i3c_npcm4xx_master_send_done;

	pDevice = obj->pDevice;

	Baudrate = config->i3c_scl_hz;

	xfer = (struct i3c_npcm4xx_xfer *)hal_I3C_MemAlloc(sizeof(struct i3c_npcm4xx_xfer));

	if (xfer) {
		xfer->ret = -ETIMEDOUT;
		xfer->ncmds = 1;
		xfer->abort = false;
		xfer->complete = false;
	} else {
		return -ENOMEM;
	}

	k_mutex_lock(&pDevice->lock, K_FOREVER);

	result = I3C_Master_Insert_Task_ENTDAA(&rxlen, RxBuf_expected, Baudrate, TIMEOUT_TYPICAL,
			pCallback, (void *)xfer, port, I3C_TASK_POLICY_APPEND_LAST, IS_HIF);

	if (result != I3C_ERR_OK) {
		LOG_ERR("Create ENTDAA task failed");
		k_mutex_unlock(&pDevice->lock);
		hal_I3C_MemFree(xfer);
		return -EINVAL;
	}

	k_sem_init(&xfer->xfer_complete, 0, 1);

	i3c_npcm4xx_start_xfer(obj, xfer);

	/* wait done, xfer.ret will be changed in ISR */
	if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
		key = k_spin_lock(&xfer->lock);
		if (xfer->complete == true) {
			/* In this case, means context switch after callback function
			 * call spin_unlock() and timeout happened.
			 * Take semaphore again to make xfer_complete synchronization.
			 */
			LOG_WRN("sem timeout but task complete, get sem again to clear flag");
			k_spin_unlock(&xfer->lock, key);
			if (k_sem_take(&xfer->xfer_complete, I3C_NPCM4XX_CCC_TIMEOUT) != 0) {
				LOG_WRN("take sem again timeout");
			}
		} else { /* the memory will be free when driver call pCallback */
			xfer->abort = true;
			ret = xfer->ret;
			k_spin_unlock(&xfer->lock, key);
			k_mutex_unlock(&pDevice->lock);
			return ret;
		}
	} else {
		if (xfer->ret == 0) {
			i3cdev->info.i2c_mode = 0;
		}
	}

	ret = xfer->ret;

	k_mutex_unlock(&pDevice->lock);

	hal_I3C_MemFree(xfer);

	return ret;
}

void I3C_Slave_Handle_DMA(uint32_t Parm);

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

	pDevice = Get_Current_Master_From_Port(I3C_IF);
	pBus = Get_Bus_From_Port(I3C_IF);

	mintmask = I3C_GET_REG_MINTMASKED(I3C_IF);
	if (mintmask == 0) {
		EXIT_MASTER_ISR();
		return;
	}

	if (mintmask & I3C_MINTMASKED_ERRWARN_MASK) {
		merrwarn = I3C_GET_REG_MERRWARN(I3C_IF);
		LOG_WRN("Master MERRWARN 0x%x", merrwarn);
		I3C_SET_REG_MERRWARN(I3C_IF, merrwarn);
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_MCTRLDONE_MASK);

		pTask = pDevice->pTaskListHead;
		switch (merrwarn) {
		case I3C_MERRWARN_NACK_MASK:
			mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
			if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_DAA) {
				pTask->pTaskInfo->result = I3C_ERR_NACK;
			} else {
				mstatus = I3C_GET_REG_MSTATUS(I3C_IF);
				I3C_SET_REG_MSTATUS(I3C_IF, mstatus | I3C_MSTATUS_COMPLETE_MASK);

				k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_stop[I3C_IF]);
				EXIT_MASTER_ISR();
				return;
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

		/* Clear up SLVSTART interrupt */
		if (mstatus & I3C_MSTATUS_SLVSTART_MASK) {
			I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);
		}

		if ((mstatus & I3C_MSTATUS_STATE_MASK) == I3C_MSTATUS_STATE_IBIACK) {
			I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_IBIWON_MASK |
			I3C_MSTATUS_COMPLETE_MASK | I3C_MSTATUS_MCTRLDONE_MASK);

			if (pDevice->ackIBI == false) {
				I3C_Master_Insert_DISEC_After_IbiNack((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			}

			/* To prevent ERRWARN.SPAR, we must ack/nak IBI ASAP */
			if (ibi_type == I3C_MSTATUS_IBITYPE_IBI) {
				I3C_Master_IBIACK((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			} else if (ibi_type == I3C_MSTATUS_IBITYPE_MstReq) {
				I3C_Master_Insert_GETACCMST_After_IbiAckMR((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			} else if (ibi_type == I3C_MSTATUS_IBITYPE_HotJoin) {
				I3C_Master_Insert_ENTDAA_After_IbiAckHJ((uint32_t) pTask);
				EXIT_MASTER_ISR();
				return;
			}

			I3C_Master_Insert_DISEC_After_IbiNack((uint32_t) pTask);
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

		/* Since we wait util MCTRLDONE flag as true before enable
		 * interrupt. Double check we handle IBIWON case before.
		 */
		if (mstatus & I3C_MINTMASKED_IBIWON_MASK) {
			LOG_ERR("IBIWON when MCTRLDONE case");
		}

		/* Clear SLVSTART flag */
		if (mstatus & I3C_MSTATUS_SLVSTART_MASK) {
			I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);
		}

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
				/* 7E+ Wr + RW */
				k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
					&work_next[I3C_IF]);
			} else {
				if (pFrame->type == I3C_TRANSFER_TYPE_DDR)
					I3C_SET_REG_MWDATAB1(I3C_IF, pFrame->hdrcmd);
				Setup_Master_Write_DMA(pDevice);
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
			if (PDMA->TDSTS & BIT(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX)) {
				PDMA->TDSTS = BIT(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX);
				*pTask->pRdLen = pFrame->access_len;
			} else if (I3C_GET_REG_MDMACTRL(I3C_IF) & I3C_MDMACTRL_DMAFB_MASK) {
				*pTask->pRdLen = pFrame->access_len -
					((uint16_t)((PDMA->DSCT[PDMA_OFFSET + I3C_IF +
						I3C_PORT_MAX].CTL & PDMA_DSCT_CTL_TXCNT_Msk) >>
					PDMA_DSCT_CTL_TXCNT_Pos) + 1);
			}
		}

		if (PDMA->TDSTS & BIT(PDMA_OFFSET + I3C_IF)) {
			PDMA->TDSTS = BIT(PDMA_OFFSET + I3C_IF);
		} else if (I3C_GET_REG_MDMACTRL(I3C_IF) & I3C_MDMACTRL_DMATB_MASK) {
			*pTask->pWrLen = pFrame->access_len -
				((uint16_t)((PDMA->DSCT[PDMA_OFFSET + I3C_IF].CTL &
					PDMA_DSCT_CTL_TXCNT_Msk) >>
					PDMA_DSCT_CTL_TXCNT_Pos) + 1);
		}

		I3C_SET_REG_MDMACTRL(I3C_IF, I3C_GET_REG_MDMACTRL(I3C_IF) &
			~I3C_MDMACTRL_DMAFB_MASK);
		PDMA->CHCTL &= ~BIT(PDMA_OFFSET + I3C_IF + I3C_PORT_MAX);

		I3C_SET_REG_MDMACTRL(I3C_IF, I3C_GET_REG_MDMACTRL(I3C_IF) &
			~I3C_MDMACTRL_DMATB_MASK);
		I3C_SET_REG_MDATACTRL(I3C_IF, I3C_GET_REG_MDATACTRL(I3C_IF) |
			I3C_MDATACTRL_FLUSHTB_MASK);
		PDMA->CHCTL &= ~BIT(PDMA_OFFSET + I3C_IF);

		/* Error has been caught, but complete also assert */
		if (pTaskInfo->result == I3C_ERR_WRABT) {
			k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF], &work_stop[I3C_IF]);
			EXIT_MASTER_ISR();
			return;
		}

		if (pTaskInfo->result == I3C_ERR_IBI) {
			struct i3c_npcm4xx_obj *obj;

			obj = gObj[I3C_IF];
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

		if (pTask->protocol == I3C_TRANSFER_PROTOCOL_ENTDAA) {
			if (pTaskInfo->result == I3C_ERR_NACK) {
			}

			*pTask->pRdLen = pTask->frame_idx * 9;

			pTaskInfo->result = I3C_ERR_OK;
			I3C_Master_End_Request((uint32_t) pTask);
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
				I3C_Master_End_Request((uint32_t) pTask);

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

		pDevice = I3C_Get_INODE(I3C_IF);
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
		I3C_Master_End_Request((uint32_t) pTask);
	}

	/* SLVSTART might asserted right after the current task has complete (STOP).
	 * So, we should do complete before SLVSTART
	 */
	if (I3C_GET_REG_MINTMASKED(I3C_IF) & I3C_MINTMASKED_SLVSTART_MASK) {
		I3C_SET_REG_MSTATUS(I3C_IF, I3C_MSTATUS_SLVSTART_MASK);

		if (pBus->pCurrentTask != NULL) {
			LOG_ERR("New slave request but pCurrentTask not NULL");
		}

		I3C_Master_New_Request((uint32_t)I3C_IF);
	}

	EXIT_MASTER_ISR();
}

void I3C_Slave_ISR(uint8_t I3C_IF)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_TASK_INFO_t *pTaskInfo = NULL;
	I3C_TRANSFER_TASK_t *pTask;
	I3C_TRANSFER_FRAME_t *pFrame;
	struct i3c_npcm4xx_obj *obj;
	uint32_t intmasked;
	uint32_t status;
	uint32_t ctrl;
	uint8_t evdet;
	uint32_t errwarn;
	bool bMATCHSS;
	uint8_t addr;
	uint32_t sconfig;

	ENTER_SLAVE_ISR();

	intmasked = I3C_GET_REG_INTMASKED(I3C_IF);
	if (intmasked == 0) {
		EXIT_SLAVE_ISR();
		return;
	}

	status = I3C_GET_REG_STATUS(I3C_IF);
	bMATCHSS = (I3C_GET_REG_CONFIG(I3C_IF) & I3C_CONFIG_MATCHSS_MASK) ? true : false;

	pDevice = I3C_Get_INODE(I3C_IF);

	obj = gObj[I3C_IF];

	if (bMATCHSS == false) {
		if (obj->config->hj_req == I3C_HOT_JOIN_STATE_Request) {
			if (intmasked & I3C_INTMASKED_STOP_MASK) {
				I3C_Slave_Insert_Task_HotJoin(I3C_IF);
				k_work_submit_to_queue(&npcm4xx_i3c_work_q[I3C_IF],
						&work_send_ibi[I3C_IF]);
				obj->config->hj_req = I3C_HOT_JOIN_STATE_Queue;
			}
		}

		if (status & I3C_STATUS_MATCHED_MASK) {
			LOG_WRN("Status matched before set MATCHSS, set bMATCHSS as true\n");
			bMATCHSS = true;
		}
	} else {
		if (obj->config->hj_req != I3C_HOT_JOIN_STATE_None) {
			LOG_WRN("MATCHSS set. DA=0x%x\n",
					I3C_Update_Dynamic_Address((uint32_t) I3C_IF));
			obj->config->hj_req = I3C_HOT_JOIN_STATE_None;
		}
	}

	/* Speedup to clear MATCHED flag in interrupt to avoid SDA driven from stop to start too quickly */
	if (intmasked & I3C_INTMASKED_STOP_MASK) {
		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_STOP_MASK);

		/* Clear address matched for SDR */
		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_MATCHED_MASK) {
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_MATCHED_MASK);
		}

		/* when bMATCHSS enabled, HW only set start/stop flag if MATCHED(address) as true */
		if (bMATCHSS) {
			/* Received stop interrupt from HW but currently bus status is start or stop,
			 * the status means "RE-ENTRY".
			 */
			if (hal_I3C_Is_Slave_Idle(I3C_IF) != true) {
				/* Received stop interrupt but current status is start, clear it. */
				if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_START_MASK) {
					LOG_DBG("[RE-ENTRY] FORCE CLEAR START in STOP INT, intmasked=0x%x status=0x%x",
							intmasked, I3C_GET_REG_STATUS(I3C_IF));
					I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_START_MASK);
				}

				/* Received stop interrupt but current status is stop, clear it. CPU running too slow? */
				if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_STOP_MASK) {
					LOG_ERR("[RE-ENTRY] RECV SECOND STOP in STOP INT, intmasked=0x%x status=0x%x",
							intmasked, I3C_GET_REG_STATUS(I3C_IF));
					I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_STOP_MASK);
				}
			}
		}
	}

	/* Target send IBI, Hot-Join or Master Request done */
	if (intmasked & I3C_INTMASKED_EVENT_MASK) {
		evdet = (uint8_t)((status & I3C_STATUS_EVDET_MASK) >> I3C_STATUS_EVDET_SHIFT);
		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_EVENT_MASK);
		pTask = pDevice->pTaskListHead;

		if (pTask) {
			pTaskInfo = pTask->pTaskInfo;
			if (pTaskInfo == NULL) {
				LOG_ERR("INTEVENT but pTaskInfo NULL");
			}
		} else {
			LOG_ERR("INTEVENT but pTask null");
		}

		if (pTaskInfo) {
			/* Target request IBI, Hot-Join and Master Request ACKED */
			if (evdet == 0x03) {
				/* Ack Hot-Join --> status = 0x341000 */
				pTaskInfo->result = I3C_ERR_OK;
				I3C_Slave_End_Request((uint32_t)pTask);
			}

			/* Target request IBI, Hot-Join and Master Request NACKED */
			if (evdet == 0x02) {
				pFrame = &pTask->pFrameList[pTask->frame_idx];
				if ((pFrame->flag & I3C_TRANSFER_RETRY_ENABLE) &&
					(pFrame->retry_count >= 1)) {
					pFrame->retry_count--;
					I3C_Slave_Start_Request((uint32_t)pTaskInfo);
				}
			} else {
				I3C_SET_REG_CTRL(I3C_IF, I3C_MCTRL_REQUEST_NONE);
				pTaskInfo->result = I3C_ERR_NACK;
				I3C_Slave_End_Request((uint32_t)pTask);
			}
		}

		intmasked &= ~I3C_INTMASKED_EVENT_MASK;
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	}

	if (intmasked & I3C_INTMASKED_DACHG_MASK) {
		/* LOG_INF("dynamic address changed\n"); */
		addr = I3C_Update_Dynamic_Address((uint32_t) I3C_IF);
		sconfig = I3C_GET_REG_CONFIG(I3C_IF);
		if (addr) {
			obj->sir_allowed_by_sw = 1;
			LOG_WRN("dyn addr = %X\n", addr);
			if (!(sconfig & I3C_CONFIG_MATCHSS_MASK)) {
				sconfig |= I3C_CONFIG_MATCHSS_MASK;
				I3C_SET_REG_CONFIG(I3C_IF, sconfig);
			}
		} else {
			obj->sir_allowed_by_sw = 0;
			LOG_WRN("reset dyn addr\n");
			if (sconfig & I3C_CONFIG_MATCHSS_MASK) {
				sconfig &= ~I3C_CONFIG_MATCHSS_MASK;
				I3C_SET_REG_CONFIG(I3C_IF, sconfig);
			}
		}

		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_DACHG_MASK);

		intmasked &= ~I3C_INTMASKED_DACHG_MASK;
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	}

	if (intmasked & I3C_INTMASKED_START_MASK) {
		if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_START_MASK) {
			I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_START_MASK);
		}

		intmasked &= ~I3C_INTMASKED_START_MASK;
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	}

	if (intmasked & I3C_INTMASKED_CCC_MASK) {
		/* must handle vendor CCC here, STOP will not be triggerred when MATCHSS == 1 */
		I3C_Slave_Handle_DMA((uint32_t) pDevice);

		intmasked = I3C_GET_REG_INTMASKED(I3C_IF);
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	}

	if (bMATCHSS && (intmasked & I3C_INTMASKED_CHANDLED_MASK)) {
		/* CCC Command, auto */
		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_CHANDLED_MASK);

		/* must handle nack SLVSTART here if MATCHSS = 1 */
		/* If master send disec after SLVSTART, the slave task should be removed.
		 * If master doesn't send disec, HW will retry automatically.
		 * So, the task should not be removed
		 */

		ctrl = I3C_GET_REG_CTRL(I3C_IF);
		if (ctrl & I3C_CTRL_EVENT_MASK) {
			evdet = (uint8_t)((status &
				I3C_STATUS_EVDET_MASK) >> I3C_STATUS_EVDET_SHIFT);

			if (evdet == 0x02) {
				/* IBI ?, NACK */
				/* Master Request ?, NACK */
				/* Hot-Join, NACK SLVSTART and follow DISEC with RESTART */
				pTask = pDevice->pTaskListHead;

				if (pTask == NULL) {
					LOG_WRN("Invalid Task !\n");
					EXIT_SLAVE_ISR();
					return;
				}

				pTaskInfo = pTask->pTaskInfo;
				pTaskInfo->result = I3C_ERR_NACK_SLVSTART;

				if ((ctrl & I3C_CTRL_EVENT_MASK) == I3C_CTRL_EVENT_IBI) {
					if (status & I3C_STATUS_IBIDIS_MASK) {
						pTaskInfo->result = I3C_ERR_OK;
					}
				} else if ((ctrl & I3C_CTRL_EVENT_MASK) == I3C_CTRL_EVENT_MstReq) {
					if (status & I3C_STATUS_MRDIS_MASK) {
						pTaskInfo->result = I3C_ERR_OK;
					}
				} else if ((ctrl & I3C_CTRL_EVENT_MASK) == I3C_CTRL_EVENT_HotJoin) {
					if (status & I3C_STATUS_HJDIS_MASK) {
						pTaskInfo->result = I3C_ERR_OK;
					}
				}

				I3C_Slave_End_Request((uint32_t)pTask);
			}
		}

		intmasked &= ~I3C_INTMASKED_CHANDLED_MASK;
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	} else if (intmasked & I3C_INTMASKED_CHANDLED_MASK){
		I3C_SET_REG_STATUS(I3C_IF, I3C_STATUS_CHANDLED_MASK);

		intmasked &= ~I3C_INTMASKED_CHANDLED_MASK;
		if (!intmasked) {
			EXIT_SLAVE_ISR();
			return;
		}
	}

	if (intmasked & I3C_INTMASKED_ERRWARN_MASK) {
		errwarn = I3C_GET_REG_ERRWARN(I3C_IF);

		LOG_WRN("Slave ERRWARN:0x%x\n", errwarn);

		if (errwarn & I3C_ERRWARN_SPAR_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_SPAR_MASK);
		}

		if (errwarn & I3C_ERRWARN_URUNNACK_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_URUNNACK_MASK);
		}

		if (errwarn & I3C_ERRWARN_INVSTART_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_INVSTART_MASK);
		}

		if (errwarn & I3C_ERRWARN_OWRITE_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_OWRITE_MASK);
		}
		if (errwarn & I3C_ERRWARN_OREAD_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_OREAD_MASK);
		}

		if (errwarn & I3C_ERRWARN_HCRC_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_HCRC_MASK);
		}
		if (errwarn & I3C_ERRWARN_HPAR_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_HPAR_MASK);
		}
		if (errwarn & I3C_ERRWARN_ORUN_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_ORUN_MASK);
		}
		if (errwarn & I3C_ERRWARN_TERM_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_TERM_MASK);
		}
		if (errwarn & I3C_ERRWARN_S0S1_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_S0S1_MASK);
		}
		if (errwarn & I3C_ERRWARN_URUN_MASK) {
			I3C_SET_REG_ERRWARN(I3C_IF, I3C_ERRWARN_URUN_MASK);
		}
	}

	if (intmasked & I3C_INTMASKED_STOP_MASK) {
		if (bMATCHSS) {
			if (hal_I3C_Is_Slave_Idle(I3C_IF) != true) {
				if (I3C_GET_REG_STATUS(I3C_IF) & I3C_STATUS_MATCHED_MASK) {
					LOG_WRN("[RE-ENTRY] MATCHED AGAIN, DATA MAY LOST");
				}
			}

			/* handle target data */
			I3C_Slave_Handle_DMA((uint32_t) pDevice);
		}
	}

	EXIT_SLAVE_ISR();
}

void I3C_Slave_Handle_DMA(uint32_t Parm)
{
	I3C_DEVICE_INFO_t *pDevice;
	I3C_PORT_Enum port;
	uint16_t txDataLen;
	bool bRet;
	uint8_t idx;
	uint8_t pdma_ch;

	pDevice = (I3C_DEVICE_INFO_t *)Parm;
	if (pDevice == NULL)
		return;

	port = pDevice->port;

	pdma_ch = Get_PDMA_Channel(port, I3C_TRANSFER_DIR_READ);

	/* Slave Rcv data ?
	 * 1. Rx DMA is started, and TXCNT change
	 * 2. DDR matched
	 * 3. vendor CCC, not implement yet
	 */
	if ((I3C_GET_REG_STATUS(port) & I3C_STATUS_DDRMATCH_MASK) ||
		((I3C_GET_REG_DMACTRL(port) & I3C_DMACTRL_DMAFB_MASK))) {
		/* Update receive data length */
		idx = slvRxId[port];

		if (PDMA->TDSTS & BIT(PDMA_OFFSET + pdma_ch)) {
			/* PDMA Rx Task Done */
			PDMA->TDSTS = BIT(PDMA_OFFSET + pdma_ch);
			slvRxOffset[port + (I3C_PORT_MAX * idx)] = slvRxLen[port];

			/* receive data more than DMA buffer size -> overrun & drop */
			if (I3C_GET_REG_DATACTRL(port) & I3C_DATACTRL_RXCOUNT_MASK) {
				I3C_SET_REG_DATACTRL(port, I3C_GET_REG_DATACTRL(port)
					| I3C_DATACTRL_FLUSHFB_MASK);
				LOG_DBG("Increase buffer size or limit transfer size !!!\r\n");
			}
		} else if (I3C_GET_REG_STATUS(port) & I3C_STATUS_STREQWR_MASK) {
			// bypass until all data has been received
			LOG_DBG("Send message too quick to response !!!\r\n");
			slvRxOffset[port + (I3C_PORT_MAX * idx)] = 0;
		} else {
			/* PDMA Rx Task not finish */
			slvRxOffset[port + (I3C_PORT_MAX * idx)] = slvRxLen[port]
			- (((PDMA->DSCT[PDMA_OFFSET + pdma_ch].CTL & PDMA_DSCT_CTL_TXCNT_Msk)
			>> PDMA_DSCT_CTL_TXCNT_Pos) + 1);
		}

		/* Process the Rcvd Data */
		if (slvRxOffset[port + (I3C_PORT_MAX * idx)]) {
			/* Stop RX DMA */
			I3C_SET_REG_DMACTRL(port, I3C_GET_REG_DMACTRL(port) &
				~I3C_DMACTRL_DMAFB_MASK);
			PDMA->CHCTL &= ~BIT(PDMA_OFFSET + pdma_ch);

			// Start Rx DMA here to prevent data loss
			I3C_Prepare_To_Read_Command((uint32_t)port);

			if (I3C_GET_REG_STATUS(port) & I3C_STATUS_CCC_MASK) {
				/* reserved for vendor CCC, drop rx data directly */
				if (slvRxBuf[port + (I3C_PORT_MAX * idx)][0] == CCC_BROADCAST_SETAASA) {
					LOG_WRN("rcv setaasa...");
				} else {
					LOG_WRN("rcv %X", slvRxBuf[port + (I3C_PORT_MAX * idx)][0]);
				}

				/* we can't support SETAASA because DYNADDR is RO. */
				slvRxOffset[port + (I3C_PORT_MAX * idx)] = 0;
				I3C_SET_REG_STATUS(port, I3C_GET_REG_STATUS(port) |
					I3C_STATUS_CCC_MASK);
			} else {
				bRet = false;

				/* To fill rx data to the requested mqueue */
				/* We must find callback from slave_data */
				struct i3c_npcm4xx_obj *obj = NULL;
				struct i3c_slave_payload *payload = NULL;
				int ret = 0;

				/* slave device */
				obj = gObj[port];

				/* prepare m_queue to backup rx data */
				if (obj->slave_data.callbacks->write_requested != NULL) {
					payload = obj->slave_data.callbacks->write_requested(
						obj->slave_data.dev);
					payload->size = slvRxOffset[port + (I3C_PORT_MAX * idx)];

					/*i3c_aspeed_rd_rx_fifo(obj, payload->buf, payload->size);*/
					bRet = true;
					if (obj->config->priv_xfer_pec) {
						ret = pec_valid(obj->dev, (uint8_t *)&slvRxBuf[port + (I3C_PORT_MAX * idx)],
							slvRxOffset[port + (I3C_PORT_MAX * idx)]);
						if (ret) {
							LOG_WRN("PEC error\n");
							bRet = false;
							payload->size = 0;
						}
					}

					memcpy(payload->buf, slvRxBuf[port + (I3C_PORT_MAX * idx)], payload->size);
				}

				if ((obj->slave_data.callbacks->write_done != NULL) && (payload->size != 0)) {
					obj->slave_data.callbacks->write_done(obj->slave_data.dev);
				}
			}
		}
	}

	/* Slave TX data has send ? */
	if ((pDevice->txLen != 0) && (pDevice->txOffset != 0)) {
		txDataLen = 0;

		hal_I3C_Slave_Query_TxLen(port, &txDataLen);
		if (txDataLen == 0) {
			/* call tx send complete hook */
			I3C_Slave_Finish_Response(pDevice);

			/* master read ibi data done */
			k_sem_give(&pDevice->ibi_complete);
		}
	}
}

I3C_ErrCode_Enum GetRegisterIndex(I3C_DEVICE_INFO_t *pDevice, uint16_t rx_cnt, uint8_t *pRx_buff,
	uint8_t *pIndexRet)
{
	uint8_t reg_count;
	uint8_t reg_chk_count;
	uint8_t i;
	uint16_t cmd16;

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
				cmd16 = (uint16_t)pRx_buff[0] | (uint16_t)pRx_buff[1] << 8;
			else
				cmd16 = (uint16_t)pRx_buff[0] << 8 | (uint16_t)pRx_buff[1];

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
	k_spinlock_key_t key;
	struct i3c_npcm4xx_obj *obj;

	mconfig = sys_read32(I3C_BASE_ADDR(port) + OFFSET_MCONFIG);
	if ((mconfig & I3C_MCONFIG_MSTENA_MASK) == I3C_MCONFIG_MSTENA_MASTER_ON) {
		I3C_Master_ISR(port);
		return;
	}

	sconfig = sys_read32(I3C_BASE_ADDR(port) + OFFSET_CONFIG);
	if ((sconfig & I3C_CONFIG_SLVENA_MASK) == I3C_CONFIG_SLVENA_SLAVE_ON) {
		obj = DEV_DATA(dev);
		key = k_spin_lock(&obj->lock);
		I3C_Slave_ISR(port);
		k_spin_unlock(&obj->lock, key);

		return;
	}
}

static int i3c_init_work_queue(I3C_PORT_Enum port)
{
	switch (port) {
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c0), okay)
	case 0:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area0,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area0),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c1), okay)
	case 1:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area1,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area1),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c2), okay)
	case 2:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area2,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area2),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c3), okay)
	case 3:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area3,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area3),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c4), okay)
	case 4:
		k_work_queue_start(&npcm4xx_i3c_work_q[port], npcm4xx_i3c_stack_area4,
			K_THREAD_STACK_SIZEOF(npcm4xx_i3c_stack_area4),
			NPCM4XX_I3C_WORK_QUEUE_PRIORITY, NULL);
		break;
#endif
#if DT_NODE_HAS_STATUS(DT_NODELABEL(i3c5), okay)
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
	k_work_init(&work_send_ibi[port], work_send_ibi_fun);
	k_work_init(&work_entdaa[port], work_entdaa_fun);

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

	gObj[port] = obj;
	ret = i3c_init_work_queue(port);
	__ASSERT(ret == 0, "failed to init work queue for i3c driver !!!");

	/* update default setting */
	I3C_Port_Default_Setting(port);

	/* to customize device node for different projects */
	pDevice = &gI3c_dev_node_internal[port];

	/* Update device node by user setting */
	pDevice->disableTimeout = true;
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

	pDevice->dma_tx_channel = config->dma_tx_channel;
	pDevice->dma_rx_channel = config->dma_rx_channel;

	/* init mutex lock */
	k_mutex_init(&pDevice->lock);

	if (config->slave) {
		if (config->secondary)
			pDevice->mode = I3C_DEVICE_MODE_SECONDARY_MASTER;
		else
			pDevice->mode = I3C_DEVICE_MODE_SLAVE_ONLY;

		pDevice->stopSplitRead = false;
		pDevice->capability.OFFLINE = false;

		obj->sir_allowed_by_sw = 0;
		k_work_init(&obj->work, sir_allowed_worker);

		/* for loopback test without ibi behavior only */
		pDevice->pReg = I3C_REGs_PORT_SLAVE;
		pDevice->regCnt = sizeof(I3C_REGs_PORT_SLAVE) / sizeof(I3C_REG_ITEM_t);
	} else {
		pDevice->mode = I3C_DEVICE_MODE_CURRENT_MASTER;

		pDevice->bRunI3C = true;
		pDevice->ackIBI = true;
		pDevice->dynamicAddr = config->assigned_addr;
	}

	I3C_connect_bus(port, config->busno);

	hal_I3C_Config_Device(pDevice);

	/* set hj req as false */
	config->hj_req = I3C_HOT_JOIN_STATE_None;

	config->rst_reason = npcm4xx_get_reset_reason();

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
		.dma_tx_channel = DT_INST_PROP_OR(n, dma_tx_channel, 0xff),\
		.dma_rx_channel = DT_INST_PROP_OR(n, dma_rx_channel, 0xff),\
		.busno = DT_INST_PROP_OR(n, busno, I3C_BUS_COUNT_MAX),\
		.i2c_scl_hz = DT_INST_PROP_OR(n, i2c_scl_hz, 0),\
		.i3c_scl_hz = DT_INST_PROP_OR(n, i3c_scl_hz, 0),\
		.base = (struct i3c_reg *)DT_INST_REG_ADDR_BY_NAME(n, i3c),\
		.pmc_base = DT_INST_REG_ADDR_BY_NAME(n, pmc),\
		.irq = DT_INST_IRQN(n),\
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
