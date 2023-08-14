/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __I3C_H__
#define __I3C_H__

#include <errno.h>
#include <soc.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/i3c/i3c.h>
#include <portability/cmsis_os2.h>
#include <logging/log.h>

#include <drivers/i3c/NPCM4XX/pub_I3C.h>

#define DT_DRV_COMPAT nuvoton_npcm4xx_i3c

#define I3C_1MHz_VAL_CONST		    1000000UL

#define APB3_CLK                    24000000                    /** ARM Peripheral Bus 3    */

#define I3C_CLOCK_FREQUENCY         APB3_CLK
#define I3C_FCLOCK_FREQUENCY	    APB3_CLK
#define I3C_CLOCK_SLOW_FREQUENCY	APB3_CLK
#define I3C_CLOCK_SLOW_TC_FREQUENCY	APB3_CLK

/*
 * define for register
 */

/* Bus characteristics register information. */
#define BCR_MASTER_CAPABLE_MASK     0xC0
#define BCR_HDR_CAPABLE_MASK        0x20
#define BCR_BRIDGE_CAPABLE_MASK     0x10
#define BCR_OFFLINE_CAPABLE_MASK    0x08
#define BCR_MDB_EXIST_MASK          0x04
#define BCR_IBI_CAPABLE_MASK        0x02
#define BCR_MAX_SPEED_LIMIT_MASK    0x01

#define BCR_MASTER_CAPABLE          0x40
#define BCR_HDR_CAPABLE             0x20
#define BCR_BRIDGE_CAPABLE          0x10
#define BCR_OFFLINE_CAPABLE         0x08
#define BCR_MDB_EXIST               0x04
#define BCR_IBI_CAPABLE             0x02
#define BCR_MAX_SPEED_LIMIT         0x01

#define DEVICE_COUNT_MAX	32

struct i3c_ibi_status {
	uint32_t length;
	uint8_t id;
	uint8_t last;
	uint8_t error;
	uint8_t ibi_status;
};

struct i3c_npcm4xx_cmd {
	uint32_t cmd_lo;
	uint32_t cmd_hi;
	void *tx_buf;
	void *rx_buf;
	int tx_length;
	int rx_length;
	int ret;
};
struct i3c_npcm4xx_xfer {
	int ret;
	int ncmds;
	struct i3c_npcm4xx_cmd *cmds;
	struct k_sem sem;
};

struct i3c_npcm4xx_dev_priv {
	int pos;
	struct {
		int enable;
		struct i3c_ibi_callbacks *callbacks;
		struct i3c_dev_desc *context;
		struct i3c_ibi_payload *incomplete;
	} ibi;
};

struct i3c_npcm4xx_config {
	int inst_id;
	int assigned_addr;
	int bcr;
	int dcr;
	int busno;
	struct i3c_reg *base;
	bool slave;
	bool secondary;
	uint32_t i3c_scl_hz;
	uint32_t i2c_scl_hz;
	uint16_t pid_extra_info;
	int ibi_append_pec;
	int priv_xfer_pec;
	int sda_tx_hold_ns;
	int i3c_pp_scl_hi_period_ns;
	int i3c_pp_scl_lo_period_ns;
	int i3c_od_scl_hi_period_ns;
	int i3c_od_scl_lo_period_ns;
};

struct i3c_npcm4xx_obj {
	const struct device *dev;
	I3C_DEVICE_INFO_t *pDevice;
	volatile __u8 task_count;
	struct I3C_TRANSFER_TASK *pTaskListHead;

	struct i3c_npcm4xx_config *config;
	struct k_spinlock lock;
	struct i3c_npcm4xx_xfer *curr_xfer;
	struct k_work work;
	bool sir_allowed_by_sw;
	struct {
		uint32_t ibi_status_correct :1;
		uint32_t ibi_pec_force_enable :1;
		uint32_t reserved :30;
	} hw_feature;

	int (*ibi_status_parser)(uint32_t value, struct i3c_ibi_status *result);

	uint32_t hw_dat_free_pos;
	uint8_t dev_addr_tbl[DEVICE_COUNT_MAX];

	struct i3c_dev_desc *dev_descs[DEVICE_COUNT_MAX];

	/* slave mode data */
	struct i3c_slave_setup slave_data;
	osEventFlagsId_t ibi_event;
	osEventFlagsId_t data_event;
	uint16_t extra_val;
};

#define DEV_CFG(dev)			((struct i3c_npcm4xx_config *)(dev)->config)
#define DEV_DATA(dev)			((struct i3c_npcm4xx_obj *)(dev)->data)
#define DESC_PRIV(desc)			((struct i3c_npcm4xx_dev_priv *)(desc)->priv_data)

enum I3C_REG_OFFSET {
	OFFSET_MCONFIG = 0x0,
	OFFSET_CONFIG = 0x4,
	OFFSET_STATUS = 0x8,
	OFFSET_CTRL = 0xC,
	OFFSET_INTSET = 0x10,
	OFFSET_INTCLR = 0x14,
	OFFSET_INTMASKED = 0x18,
	OFFSET_ERRWARN = 0x1C,
	OFFSET_DMACTRL = 0x20,
	OFFSET_DATACTRL = 0x2C,
	OFFSET_WDATAB = 0x30,
	OFFSET_WDATABE = 0x34,
	OFFSET_WDATAH = 0x38,
	OFFSET_WDATAHE = 0x3C,
	OFFSET_RDATAB = 0x40,
	OFFSET_RDATAH = 0x48,
	OFFSET_WDATAB1 = 0x54,
	OFFSET_CAPABILITIES = 0x60,
	OFFSET_DYNADDR = 0x64,
	OFFSET_MAXLIMITS = 0x68,
	OFFSET_PARTNO = 0x6C,
	OFFSET_IDEXT = 0x70,
	OFFSET_VENDORID = 0x74,
	OFFSET_TCCLOCK = 0x78,
	OFFSET_MCTRL = 0x84,
	OFFSET_MSTATUS = 0x88,
	OFFSET_IBIRULES = 0x8C,
	OFFSET_MINTSET = 0x90,
	OFFSET_MINTCLR = 0x94,
	OFFSET_MINTMASKED = 0x98,
	OFFSET_MERRWARN = 0x9C,
	OFFSET_MDMACTRL = 0xA0,
	OFFSET_MDATACTRL = 0xAC,
	OFFSET_MWDATAB = 0xB0,
	OFFSET_MWDATABE = 0xB4,
	OFFSET_MWDATAH = 0xB8,
	OFFSET_MWDATAHE = 0xBC,
	OFFSET_MRDATAB = 0xC0,
	OFFSET_MRDATAH = 0xC8,
	OFFSET_MWDATAB1 = 0xCC,
	OFFSET_MWMSG_SDR = 0xD0,
	OFFSET_MRMSG_SDR = 0xD4,
	OFFSET_MWMSG_DDR = 0xD8,
	OFFSET_MRMSG_DDR = 0xDC,
	OFFSET_MDYNADDR = 0xE4,
	OFFSET_HDRCMD = 0x108,
	OFFSET_IBIEXT1 = 0x140,
	OFFSET_IBIEXT2 = 0x144,
	OFFSET_ID = 0x1FC,
};

/* MCONFIG */
#define I3C_MCONFIG_I2CBAUD_MASK (0xF0000000U)
#define I3C_MCONFIG_I2CBAUD_SHIFT (28U)
#define I3C_MCONFIG_I2CBAUD(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_I2CBAUD_SHIFT)) \
& I3C_MCONFIG_I2CBAUD_MASK)

#define I3C_MCONFIG_ODHPP_MASK (0x1000000U)
#define I3C_MCONFIG_ODHPP_SHIFT (24U)
#define I3C_MCONFIG_ODHPP(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_ODHPP_SHIFT)) \
& I3C_MCONFIG_ODHPP_MASK)

#define I3C_MCONFIG_ODBAUD_MASK (0xFF0000U)
#define I3C_MCONFIG_ODBAUD_SHIFT (16U)
#define I3C_MCONFIG_ODBAUD(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_ODBAUD_SHIFT)) \
& I3C_MCONFIG_ODBAUD_MASK)

#define I3C_MCONFIG_PPLOW_MASK (0xF000U)
#define I3C_MCONFIG_PPLOW_SHIFT (12U)
#define I3C_MCONFIG_PPLOW(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_PPLOW_SHIFT)) \
& I3C_MCONFIG_PPLOW_MASK)

#define I3C_MCONFIG_PPBAUD_MASK (0xF00U)
#define I3C_MCONFIG_PPBAUD_SHIFT (8U)
#define I3C_MCONFIG_PPBAUD(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_PPBAUD_SHIFT)) \
& I3C_MCONFIG_PPBAUD_MASK)

#define I3C_MCONFIG_ODSTOP_MASK (0x40U)
#define I3C_MCONFIG_ODSTOP_SHIFT (6U)
#define I3C_MCONFIG_ODSTOP(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_ODSTOP_SHIFT)) \
& I3C_MCONFIG_ODSTOP_MASK)

#define I3C_MCONFIG_HKEEP_MASK (0x30U)
#define I3C_MCONFIG_HKEEP_SHIFT (4U)
#define I3C_MCONFIG_HKEEP(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_HKEEP_SHIFT)) \
& I3C_MCONFIG_HKEEP_MASK)

#define I3C_MCONFIG_DISTO_MASK (0x8U)
#define I3C_MCONFIG_DISTO_SHIFT (3U)
#define I3C_MCONFIG_DISTO(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_DISTO_SHIFT)) \
& I3C_MCONFIG_DISTO_MASK)

#define I3C_MCONFIG_MSTENA_MASK (0x3U)
#define I3C_MCONFIG_MSTENA_SHIFT (0U)
#define I3C_MCONFIG_MSTENA(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCONFIG_MSTENA_SHIFT)) \
& I3C_MCONFIG_MSTENA_MASK)

enum I3C_MCONFIG_HKEEP_Enum {
	I3C_MCONFIG_HKEEP_NO = 0,
	I3C_MCONFIG_HKEEP_ONCHIP = 1,
	I3C_MCONFIG_HKEEP_EXTSDA = 2,
	I3C_MCONFIG_HKEEP_EXTBOTH = 3,
};

enum I3C_MCONFIG_DISTO_Enum {
	I3C_MCONFIG_DISTO_OFF = 0, I3C_MCONFIG_DISTO_ON = 1,
};

enum I3C_MCONFIG_MSTENA_Enum {
	I3C_MCONFIG_MSTENA_MASTER_OFF = 0,
	I3C_MCONFIG_MSTENA_MASTER_ON = 1,
	I3C_MCONFIG_MSTENA_MASTER_CAPABLE = 2,
};

/* MCTRL */
#define I3C_MCTRL_RDTERM_MASK (0xFF0000U)
#define I3C_MCTRL_RDTERM_SHIFT (16U)
#define I3C_MCTRL_RDTERM(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCTRL_RDTERM_SHIFT)) \
& I3C_MCTRL_RDTERM_MASK)

#define I3C_MCTRL_ADDR_MASK (0xFE00U)
#define I3C_MCTRL_ADDR_SHIFT (9U)
#define I3C_MCTRL_ADDR(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCTRL_ADDR_SHIFT)) \
& I3C_MCTRL_ADDR_MASK)

#define I3C_MCTRL_DIR_MASK (0x100U)
#define I3C_MCTRL_DIR_SHIFT (8U)
#define I3C_MCTRL_DIR(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCTRL_DIR_SHIFT)) \
& I3C_MCTRL_DIR_MASK)

#define I3C_MCTRL_IBIRESP_MASK                   (0xC0U)
#define I3C_MCTRL_IBIRESP_SHIFT                  (6U)
#define I3C_MCTRL_IBIRESP(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MCTRL_IBIRESP_SHIFT)) & I3C_MCTRL_IBIRESP_MASK)

#define I3C_MCTRL_TYPE_MASK                      (0x30U)
#define I3C_MCTRL_TYPE_SHIFT                     (4U)
#define I3C_MCTRL_TYPE(x)                        (((uint32_t)(((uint32_t)(x)) \
<< I3C_MCTRL_TYPE_SHIFT)) & I3C_MCTRL_TYPE_MASK)

#define I3C_MCTRL_REQUEST_MASK (0x7U)
#define I3C_MCTRL_REQUEST_SHIFT (0U)
#define I3C_MCTRL_REQUEST(x) (((uint32_t)(((uint32_t)(x)) << I3C_MCTRL_REQUEST_SHIFT)) \
& I3C_MCTRL_REQUEST_MASK)

#define I3C_MCTRL_RDTERM_MAX    (uint16_t)(I3C_MCTRL_RDTERM_MASK >> I3C_MCTRL_RDTERM_SHIFT)

enum I3C_MCTRL_DIR_Enum {
	I3C_MCTRL_DIR_WRITE = 0, I3C_MCTRL_DIR_READ = 1,
};

enum I3C_MCTRL_REQUEST_Enum {
	I3C_MCTRL_REQUEST_NONE = 0,
	I3C_MCTRL_REQUEST_EMIT_START = 1,
	I3C_MCTRL_REQUEST_EMIT_STOP = 2,
	I3C_MCTRL_REQUEST_IBI_MANUAL = 3,
	I3C_MCTRL_REQUEST_DO_ENTDAA = 4,
	I3C_MCTRL_REQUEST_EXIT_DDR = 6,
	I3C_MCTRL_REQUEST_IBI_AUTO = 7,
};

/* MSTATUS */
#define I3C_MSTATUS_IBIADDR_MASK                 (0x7F000000U)
#define I3C_MSTATUS_IBIADDR_SHIFT                (24U)
#define I3C_MSTATUS_IBIADDR(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_IBIADDR_SHIFT)) & I3C_MSTATUS_IBIADDR_MASK)

#define I3C_MSTATUS_NOWMASTER_MASK               (0x80000U)
#define I3C_MSTATUS_NOWMASTER_SHIFT              (19U)
#define I3C_MSTATUS_NOWMASTER(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_NOWMASTER_SHIFT)) & I3C_MSTATUS_NOWMASTER_MASK)

#define I3C_MSTATUS_ERRWARN_MASK                 (0x8000U)
#define I3C_MSTATUS_ERRWARN_SHIFT                (15U)
#define I3C_MSTATUS_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_ERRWARN_SHIFT)) & I3C_MSTATUS_ERRWARN_MASK)

#define I3C_MSTATUS_IBIWON_MASK                  (0x2000U)
#define I3C_MSTATUS_IBIWON_SHIFT                 (13U)
#define I3C_MSTATUS_IBIWON(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_IBIWON_SHIFT)) & I3C_MSTATUS_IBIWON_MASK)

#define I3C_MSTATUS_TXNOTFULL_MASK               (0x1000U)
#define I3C_MSTATUS_TXNOTFULL_SHIFT              (12U)
#define I3C_MSTATUS_TXNOTFULL(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_TXNOTFULL_SHIFT)) & I3C_MSTATUS_TXNOTFULL_MASK)

#define I3C_MSTATUS_RXPEND_MASK                  (0x800U)
#define I3C_MSTATUS_RXPEND_SHIFT                 (11U)
#define I3C_MSTATUS_RXPEND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_RXPEND_SHIFT)) & I3C_MSTATUS_RXPEND_MASK)

#define I3C_MSTATUS_COMPLETE_MASK                (0x400U)
#define I3C_MSTATUS_COMPLETE_SHIFT               (10U)
#define I3C_MSTATUS_COMPLETE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_COMPLETE_SHIFT)) & I3C_MSTATUS_COMPLETE_MASK)

#define I3C_MSTATUS_MCTRLDONE_MASK               (0x200U)
#define I3C_MSTATUS_MCTRLDONE_SHIFT              (9U)
#define I3C_MSTATUS_MCTRLDONE(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_MCTRLDONE_SHIFT)) & I3C_MSTATUS_MCTRLDONE_MASK)

#define I3C_MSTATUS_SLVSTART_MASK                (0x100U)
#define I3C_MSTATUS_SLVSTART_SHIFT               (8U)
#define I3C_MSTATUS_SLVSTART(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_SLVSTART_SHIFT)) & I3C_MSTATUS_SLVSTART_MASK)

#define I3C_MSTATUS_IBITYPE_MASK                 (0xC0U)
#define I3C_MSTATUS_IBITYPE_SHIFT                (6U)
#define I3C_MSTATUS_IBITYPE(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_IBITYPE_SHIFT)) & I3C_MSTATUS_IBITYPE_MASK)

#define I3C_MSTATUS_NACKED_MASK                  (0x20U)
#define I3C_MSTATUS_NACKED_SHIFT                 (5U)
#define I3C_MSTATUS_NACKED(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_NACKED_SHIFT)) & I3C_MSTATUS_NACKED_MASK)

#define I3C_MSTATUS_BETWEEN_MASK                 (0x10U)
#define I3C_MSTATUS_BETWEEN_SHIFT                (4U)
#define I3C_MSTATUS_BETWEEN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_BETWEEN_SHIFT)) & I3C_MSTATUS_BETWEEN_MASK)

#define I3C_MSTATUS_STATE_MASK                   (0x7U)
#define I3C_MSTATUS_STATE_SHIFT                  (0U)
#define I3C_MSTATUS_STATE(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MSTATUS_STATE_SHIFT)) & I3C_MSTATUS_STATE_MASK)

enum I3C_MSTATUS_IBITYPE_Enum {
	I3C_MSTATUS_IBITYPE_None = 0,
	I3C_MSTATUS_IBITYPE_IBI = 1,
	I3C_MSTATUS_IBITYPE_MstReq = 2,
	I3C_MSTATUS_IBITYPE_HotJoin = 3,
};

enum I3C_MSTATUS_STATE_Enum {
	I3C_MSTATUS_STATE_IDLE = 0U,
	I3C_MSTATUS_STATE_SLVREQ = 1U,
	I3C_MSTATUS_STATE_MSGSDR = 2U,
	I3C_MSTATUS_STATE_NORMACT = 3U,
	I3C_MSTATUS_STATE_DDR = 4U,
	I3C_MSTATUS_STATE_DAA = 5U,
	I3C_MSTATUS_STATE_IBIACK = 6U,
	I3C_MSTATUS_STATE_IBIRCV = 7U,
};

/* IBIRULES */
#define I3C_IBIRULES_NOBYTE_MASK                (0x80000000U)
#define I3C_IBIRULES_NOBYTE_SHIFT               (31U)
#define I3C_IBIRULES_NOBYTE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_NOBYTE_SHIFT)) & I3C_IBIRULES_NOBYTE_MASK)

#define I3C_IBIRULES_MSB0_MASK                  (0x40000000U)
#define I3C_IBIRULES_MSB0_SHIFT                 (30U)
#define I3C_IBIRULES_MSB0(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_MSB0_SHIFT)) & I3C_IBIRULES_MSB0_MASK)

#define I3C_IBIRULES_ADDR4_MASK                 (0x3F000000U)
#define I3C_IBIRULES_ADDR4_SHIFT                (24U)
#define I3C_IBIRULES_ADDR4(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_ADDR4_SHIFT)) & I3C_IBIRULES_ADDR4_MASK)

#define I3C_IBIRULES_ADDR3_MASK                 (0xFC0000U)
#define I3C_IBIRULES_ADDR3_SHIFT                (18U)
#define I3C_IBIRULES_ADDR3(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_ADDR3_SHIFT)) & I3C_IBIRULES_ADDR3_MASK)

#define I3C_IBIRULES_ADDR2_MASK                 (0x3F000U)
#define I3C_IBIRULES_ADDR2_SHIFT                (12U)
#define I3C_IBIRULES_ADDR2(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_ADDR2_SHIFT)) & I3C_IBIRULES_ADDR2_MASK)

#define I3C_IBIRULES_ADDR1_MASK                 (0xFC0U)
#define I3C_IBIRULES_ADDR1_SHIFT                (6U)
#define I3C_IBIRULES_ADDR1(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_ADDR1_SHIFT)) & I3C_IBIRULES_ADDR1_MASK)

#define I3C_IBIRULES_ADDR0_MASK                 (0x3FU)
#define I3C_IBIRULES_ADDR0_SHIFT                (0U)
#define I3C_IBIRULES_ADDR0(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIRULES_ADDR0_SHIFT)) & I3C_IBIRULES_ADDR0_MASK)

/* MINTSET */
#define I3C_MINTSET_NOWMASTER_MASK               (0x80000U)
#define I3C_MINTSET_NOWMASTER_SHIFT              (19U)
#define I3C_MINTSET_NOWMASTER(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_NOWMASTER_SHIFT)) & I3C_MINTSET_NOWMASTER_MASK)

#define I3C_MINTSET_ERRWARN_MASK                 (0x8000U)
#define I3C_MINTSET_ERRWARN_SHIFT                (15U)
#define I3C_MINTSET_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_ERRWARN_SHIFT)) & I3C_MINTSET_ERRWARN_MASK)

#define I3C_MINTSET_IBIWON_MASK                  (0x2000U)
#define I3C_MINTSET_IBIWON_SHIFT                 (13U)
#define I3C_MINTSET_IBIWON(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_IBIWON_SHIFT)) & I3C_MINTSET_IBIWON_MASK)

#define I3C_MINTSET_TXNOTFULL_MASK               (0x1000U)
#define I3C_MINTSET_TXNOTFULL_SHIFT              (12U)
#define I3C_MINTSET_TXNOTFULL(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_TXNOTFULL_SHIFT)) & I3C_MINTSET_TXNOTFULL_MASK)

#define I3C_MINTSET_RXPEND_MASK                  (0x800U)
#define I3C_MINTSET_RXPEND_SHIFT                 (11U)
#define I3C_MINTSET_RXPEND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_RXPEND_SHIFT)) & I3C_MINTSET_RXPEND_MASK)

#define I3C_MINTSET_COMPLETE_MASK                (0x400U)
#define I3C_MINTSET_COMPLETE_SHIFT               (10U)
#define I3C_MINTSET_COMPLETE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_COMPLETE_SHIFT)) & I3C_MINTSET_COMPLETE_MASK)

#define I3C_MINTSET_MCTRLDONE_MASK               (0x200U)
#define I3C_MINTSET_MCTRLDONE_SHIFT              (9U)
#define I3C_MINTSET_MCTRLDONE(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_MCTRLDONE_SHIFT)) & I3C_MINTSET_MCTRLDONE_MASK)

#define I3C_MINTSET_SLVSTART_MASK                (0x100U)
#define I3C_MINTSET_SLVSTART_SHIFT               (8U)
#define I3C_MINTSET_SLVSTART(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTSET_SLVSTART_SHIFT)) & I3C_MINTSET_SLVSTART_MASK)

/* MINTCLR */
#define I3C_MINTCLR_NOWMASTER_MASK               (0x80000U)
#define I3C_MINTCLR_NOWMASTER_SHIFT              (19U)
#define I3C_MINTCLR_NOWMASTER(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_NOWMASTER_SHIFT)) & I3C_MINTCLR_NOWMASTER_MASK)

#define I3C_MINTCLR_ERRWARN_MASK                 (0x8000U)
#define I3C_MINTCLR_ERRWARN_SHIFT                (15U)
#define I3C_MINTCLR_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_ERRWARN_SHIFT)) & I3C_MINTCLR_ERRWARN_MASK)

#define I3C_MINTCLR_IBIWON_MASK                  (0x2000U)
#define I3C_MINTCLR_IBIWON_SHIFT                 (13U)
#define I3C_MINTCLR_IBIWON(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_IBIWON_SHIFT)) & I3C_MINTCLR_IBIWON_MASK)

#define I3C_MINTCLR_TXNOTFULL_MASK               (0x1000U)
#define I3C_MINTCLR_TXNOTFULL_SHIFT              (12U)
#define I3C_MINTCLR_TXNOTFULL(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_TXNOTFULL_SHIFT)) & I3C_MINTCLR_TXNOTFULL_MASK)

#define I3C_MINTCLR_RXPEND_MASK                  (0x800U)
#define I3C_MINTCLR_RXPEND_SHIFT                 (11U)
#define I3C_MINTCLR_RXPEND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_RXPEND_SHIFT)) & I3C_MINTCLR_RXPEND_MASK)

#define I3C_MINTCLR_COMPLETE_MASK                (0x400U)
#define I3C_MINTCLR_COMPLETE_SHIFT               (10U)
#define I3C_MINTCLR_COMPLETE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_COMPLETE_SHIFT)) & I3C_MINTCLR_COMPLETE_MASK)

#define I3C_MINTCLR_MCTRLDONE_MASK               (0x200U)
#define I3C_MINTCLR_MCTRLDONE_SHIFT              (9U)
#define I3C_MINTCLR_MCTRLDONE(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_MCTRLDONE_SHIFT)) & I3C_MINTCLR_MCTRLDONE_MASK)

#define I3C_MINTCLR_SLVSTART_MASK                (0x100U)
#define I3C_MINTCLR_SLVSTART_SHIFT               (8U)
#define I3C_MINTCLR_SLVSTART(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTCLR_SLVSTART_SHIFT)) & I3C_MINTCLR_SLVSTART_MASK)

/* MINTMASKED */
#define I3C_MINTMASKED_NOWMASTER_MASK            (0x80000U)
#define I3C_MINTMASKED_NOWMASTER_SHIFT           (19U)
#define I3C_MINTMASKED_NOWMASTER(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_NOWMASTER_SHIFT)) & I3C_MINTMASKED_NOWMASTER_MASK)

#define I3C_MINTMASKED_ERRWARN_MASK              (0x8000U)
#define I3C_MINTMASKED_ERRWARN_SHIFT             (15U)
#define I3C_MINTMASKED_ERRWARN(x)                (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_ERRWARN_SHIFT)) & I3C_MINTMASKED_ERRWARN_MASK)

#define I3C_MINTMASKED_IBIWON_MASK               (0x2000U)
#define I3C_MINTMASKED_IBIWON_SHIFT              (13U)
#define I3C_MINTMASKED_IBIWON(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_IBIWON_SHIFT)) & I3C_MINTMASKED_IBIWON_MASK)

#define I3C_MINTMASKED_TXNOTFULL_MASK            (0x1000U)
#define I3C_MINTMASKED_TXNOTFULL_SHIFT           (12U)
#define I3C_MINTMASKED_TXNOTFULL(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_TXNOTFULL_SHIFT)) & I3C_MINTMASKED_TXNOTFULL_MASK)

#define I3C_MINTMASKED_RXPEND_MASK               (0x800U)
#define I3C_MINTMASKED_RXPEND_SHIFT              (11U)
#define I3C_MINTMASKED_RXPEND(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_RXPEND_SHIFT)) & I3C_MINTMASKED_RXPEND_MASK)

#define I3C_MINTMASKED_COMPLETE_MASK             (0x400U)
#define I3C_MINTMASKED_COMPLETE_SHIFT            (10U)
#define I3C_MINTMASKED_COMPLETE(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_COMPLETE_SHIFT)) & I3C_MINTMASKED_COMPLETE_MASK)

#define I3C_MINTMASKED_MCTRLDONE_MASK            (0x200U)
#define I3C_MINTMASKED_MCTRLDONE_SHIFT           (9U)
#define I3C_MINTMASKED_MCTRLDONE(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_MCTRLDONE_SHIFT)) & I3C_MINTMASKED_MCTRLDONE_MASK)

#define I3C_MINTMASKED_SLVSTART_MASK             (0x100U)
#define I3C_MINTMASKED_SLVSTART_SHIFT            (8U)
#define I3C_MINTMASKED_SLVSTART(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_MINTMASKED_SLVSTART_SHIFT)) & I3C_MINTMASKED_SLVSTART_MASK)

/* MERRWARN */
#define I3C_MERRWARN_TIMEOUT_MASK                (0x100000U)
#define I3C_MERRWARN_TIMEOUT_SHIFT               (20U)
#define I3C_MERRWARN_TIMEOUT(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_TIMEOUT_SHIFT)) & I3C_MERRWARN_TIMEOUT_MASK)

#define I3C_MERRWARN_INVREQ_MASK                 (0x80000U)
#define I3C_MERRWARN_INVREQ_SHIFT                (19U)
#define I3C_MERRWARN_INVREQ(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_INVREQ_SHIFT)) & I3C_MERRWARN_INVREQ_MASK)

#define I3C_MERRWARN_MSGERR_MASK                 (0x40000U)
#define I3C_MERRWARN_MSGERR_SHIFT                (18U)
#define I3C_MERRWARN_MSGERR(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_MSGERR_SHIFT)) & I3C_MERRWARN_MSGERR_MASK)

#define I3C_MERRWARN_OWRITE_MASK                 (0x20000U)
#define I3C_MERRWARN_OWRITE_SHIFT                (17U)
#define I3C_MERRWARN_OWRITE(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_OWRITE_SHIFT)) & I3C_MERRWARN_OWRITE_MASK)

#define I3C_MERRWARN_OREAD_MASK                  (0x10000U)
#define I3C_MERRWARN_OREAD_SHIFT                 (16U)
#define I3C_MERRWARN_OREAD(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_OREAD_SHIFT)) & I3C_MERRWARN_OREAD_MASK)

#define I3C_MERRWARN_HCRC_MASK                   (0x400U)
#define I3C_MERRWARN_HCRC_SHIFT                  (10U)
#define I3C_MERRWARN_HCRC(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_HCRC_SHIFT)) & I3C_MERRWARN_HCRC_MASK)

#define I3C_MERRWARN_HPAR_MASK                   (0x200U)
#define I3C_MERRWARN_HPAR_SHIFT                  (9U)
#define I3C_MERRWARN_HPAR(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_HPAR_SHIFT)) & I3C_MERRWARN_HPAR_MASK)

#define I3C_MERRWARN_TERM_MASK                   (0x10U)
#define I3C_MERRWARN_TERM_SHIFT                  (4U)
#define I3C_MERRWARN_TERM(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_TERM_SHIFT)) & I3C_MERRWARN_TERM_MASK)

#define I3C_MERRWARN_WRABT_MASK                  (0x8U)
#define I3C_MERRWARN_WRABT_SHIFT                 (3U)
#define I3C_MERRWARN_WRABT(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_WRABT_SHIFT)) & I3C_MERRWARN_WRABT_MASK)

#define I3C_MERRWARN_NACK_MASK                   (0x4U)
#define I3C_MERRWARN_NACK_SHIFT                  (2U)
#define I3C_MERRWARN_NACK(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MERRWARN_NACK_SHIFT)) & I3C_MERRWARN_NACK_MASK)

enum I3C_MERRWARN_Enum {
	I3C_MERRWARN_TIMEOUT,
	I3C_MERRWARN_INVREQ,
	I3C_MERRWARN_MSGERR,
	I3C_MERRWARN_OWRITE,
	I3C_MERRWARN_OREAD,
	I3C_MERRWARN_HCRC,
	I3C_MERRWARN_HPAR,
	I3C_MERRWARN_TERM,
	I3C_MERRWARN_WRABT,
	I3C_MERRWARN_NACK,
};

/* MDMACTRL */
#define I3C_MDMACTRL_DMAWIDTH_MASK               (0x30U)
#define I3C_MDMACTRL_DMAWIDTH_SHIFT              (4U)
#define I3C_MDMACTRL_DMAWIDTH(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDMACTRL_DMAWIDTH_SHIFT)) & I3C_MDMACTRL_DMAWIDTH_MASK)
#define I3C_MDMACTRL_DMAWIDTH_BYTE           (0x01U)
#define I3C_MDMACTRL_DMAWIDTH_HALF_WORD      (0x02U)

#define I3C_MDMACTRL_DMATB_MASK                  (0xCU)
#define I3C_MDMACTRL_DMATB_SHIFT                 (2U)
#define I3C_MDMACTRL_DMATB(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDMACTRL_DMATB_SHIFT)) & I3C_MDMACTRL_DMATB_MASK)

#define I3C_MDMACTRL_DMAFB_MASK                  (0x3U)
#define I3C_MDMACTRL_DMAFB_SHIFT                 (0U)
#define I3C_MDMACTRL_DMAFB(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDMACTRL_DMAFB_SHIFT)) & I3C_MDMACTRL_DMAFB_MASK)

/* MDATACTRL */
#define I3C_MDATACTRL_RXEMPTY_MASK               (0x80000000U)
#define I3C_MDATACTRL_RXEMPTY_SHIFT              (31U)
#define I3C_MDATACTRL_RXEMPTY(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_RXEMPTY_SHIFT)) & I3C_MDATACTRL_RXEMPTY_MASK)

#define I3C_MDATACTRL_TXFULL_MASK                (0x40000000U)
#define I3C_MDATACTRL_TXFULL_SHIFT               (30U)
#define I3C_MDATACTRL_TXFULL(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_TXFULL_SHIFT)) & I3C_MDATACTRL_TXFULL_MASK)

#define I3C_MDATACTRL_RXCOUNT_MASK               (0x1F000000U)
#define I3C_MDATACTRL_RXCOUNT_SHIFT              (24U)
#define I3C_MDATACTRL_RXCOUNT(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_RXCOUNT_SHIFT)) & I3C_MDATACTRL_RXCOUNT_MASK)

#define I3C_MDATACTRL_TXCOUNT_MASK               (0x1F0000U)
#define I3C_MDATACTRL_TXCOUNT_SHIFT              (16U)
#define I3C_MDATACTRL_TXCOUNT(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_TXCOUNT_SHIFT)) & I3C_MDATACTRL_TXCOUNT_MASK)

#define I3C_MDATACTRL_RXTRIG_MASK                (0xC0U)
#define I3C_MDATACTRL_RXTRIG_SHIFT               (6U)
#define I3C_MDATACTRL_RXTRIG(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_RXTRIG_SHIFT)) & I3C_MDATACTRL_RXTRIG_MASK)

#define I3C_MDATACTRL_TXTRIG_MASK                (0x30U)
#define I3C_MDATACTRL_TXTRIG_SHIFT               (4U)
#define I3C_MDATACTRL_TXTRIG(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_TXTRIG_SHIFT)) & I3C_MDATACTRL_TXTRIG_MASK)

#define I3C_MDATACTRL_UNLOCK_MASK                (0x4U)
#define I3C_MDATACTRL_UNLOCK_SHIFT               (2U)
#define I3C_MDATACTRL_UNLOCK(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_UNLOCK_SHIFT)) & I3C_MDATACTRL_UNLOCK_MASK)

#define I3C_MDATACTRL_FLUSHFB_MASK               (0x2U)
#define I3C_MDATACTRL_FLUSHFB_SHIFT              (1U)
#define I3C_MDATACTRL_FLUSHFB(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_FLUSHFB_SHIFT)) & I3C_MDATACTRL_FLUSHFB_MASK)

#define I3C_MDATACTRL_FLUSHTB_MASK               (0x1U)
#define I3C_MDATACTRL_FLUSHTB_SHIFT              (0U)
#define I3C_MDATACTRL_FLUSHTB(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDATACTRL_FLUSHTB_SHIFT)) & I3C_MDATACTRL_FLUSHTB_MASK)

/* MWDATAB */
#define I3C_MWDATAB_END_A_MASK                (0x10000U)
#define I3C_MWDATAB_END_A_SHIFT               (16U)
#define I3C_MWDATAB_END_A(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAB_END_A_SHIFT)) & I3C_MWDATAB_END_A_MASK)

#define I3C_MWDATAB_END_B_MASK                     (0x100U)
#define I3C_MWDATAB_END_B_SHIFT                    (8U)
#define I3C_MWDATAB_END_B(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAB_END_B_SHIFT)) & I3C_MWDATAB_END_B_MASK)

#define I3C_MWDATAB_DATA_MASK                    (0xFFU)
#define I3C_MWDATAB_DATA_SHIFT                   (0U)
#define I3C_MWDATAB_DATA(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAB_DATA_SHIFT)) & I3C_MWDATAB_DATA_MASK)

/* MWDATABE */
#define I3C_MWDATABE_DATA_MASK                   (0xFFU)
#define I3C_MWDATABE_DATA_SHIFT                  (0U)
#define I3C_MWDATABE_DATA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATABE_DATA_SHIFT)) & I3C_MWDATABE_DATA_MASK)

/* MWDATAH */
#define I3C_MWDATAH_END_MASK                     (0x10000U)
#define I3C_MWDATAH_END_SHIFT                    (16U)
#define I3C_MWDATAH_END(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAH_END_SHIFT)) & I3C_MWDATAH_END_MASK)

#define I3C_MWDATAH_DATA1_MASK                   (0xFF00U)
#define I3C_MWDATAH_DATA1_SHIFT                  (8U)
#define I3C_MWDATAH_DATA1(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAH_DATA1_SHIFT)) & I3C_MWDATAH_DATA1_MASK)

#define I3C_MWDATAH_DATA0_MASK                   (0xFFU)
#define I3C_MWDATAH_DATA0_SHIFT                  (0U)
#define I3C_MWDATAH_DATA0(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAH_DATA0_SHIFT)) & I3C_MWDATAH_DATA0_MASK)

/* MWDATAHE */
#define I3C_MWDATAHE_DATA1_MASK                  (0xFF00U)
#define I3C_MWDATAHE_DATA1_SHIFT                 (8U)
#define I3C_MWDATAHE_DATA1(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAHE_DATA1_SHIFT)) & I3C_MWDATAHE_DATA1_MASK)

#define I3C_MWDATAHE_DATA0_MASK                  (0xFFU)
#define I3C_MWDATAHE_DATA0_SHIFT                 (0U)
#define I3C_MWDATAHE_DATA0(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAHE_DATA0_SHIFT)) & I3C_MWDATAHE_DATA0_MASK)

/* MRDATAB */
#define I3C_MRDATAB_DATA_MASK                   (0xFFU)
#define I3C_MRDATAB_DATA_SHIFT                  (0U)
#define I3C_MRDATAB_DATA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_MRDATAB_DATA_SHIFT)) & I3C_MRDATAB_DATA_MASK)

/* MRDATAH */
#define I3C_MRDATAH_DATA1_MASK                     (0xFF00U)
#define I3C_MRDATAH_DATA1_SHIFT                    (8U)
#define I3C_MRDATAH_DATA1(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_MRDATAH_DATA1_SHIFT)) & I3C_MRDATAH_DATA1_MASK)

#define I3C_MRDATAH_DATA0_MASK                     (0xFFU)
#define I3C_MRDATAH_DATA0_SHIFT                    (0U)
#define I3C_MRDATAH_DATA0(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_MRDATAH_DATA0_SHIFT)) & I3C_MRDATAH_DATA0_MASK)

/* MWDATAB1 */
#define I3C_MWDATAB1_DATA_MASK                  (0xFFU)
#define I3C_MWDATAB1_DATA_SHIFT                 (0U)
#define I3C_MWDATAB1_DATA(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWDATAB1_DATA_SHIFT)) & I3C_MWDATAB1_DATA_MASK)

/* MWMSG_SDR_CONTROL */
#define I3C_MWMSG_SDR_CONTROL_LEN_MASK           (0xF800U)
#define I3C_MWMSG_SDR_CONTROL_LEN_SHIFT          (11U)
#define I3C_MWMSG_SDR_CONTROL_LEN(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_CONTROL_LEN_SHIFT)) & I3C_MWMSG_SDR_CONTROL_LEN_MASK)

#define I3C_MWMSG_SDR_CONTROL_I2C_MASK           (0x400U)
#define I3C_MWMSG_SDR_CONTROL_I2C_SHIFT          (10U)
#define I3C_MWMSG_SDR_CONTROL_I2C(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_CONTROL_I2C_SHIFT)) & I3C_MWMSG_SDR_CONTROL_I2C_MASK)

#define I3C_MWMSG_SDR_CONTROL_END_MASK           (0x100U)
#define I3C_MWMSG_SDR_CONTROL_END_SHIFT          (8U)
#define I3C_MWMSG_SDR_CONTROL_END(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_CONTROL_END_SHIFT)) & I3C_MWMSG_SDR_CONTROL_END_MASK)

#define I3C_MWMSG_SDR_CONTROL_ADDR_MASK          (0xFEU)
#define I3C_MWMSG_SDR_CONTROL_ADDR_SHIFT         (1U)
#define I3C_MWMSG_SDR_CONTROL_ADDR(x)            (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_CONTROL_ADDR_SHIFT)) & I3C_MWMSG_SDR_CONTROL_ADDR_MASK)

#define I3C_MWMSG_SDR_CONTROL_DIR_MASK           (0x1U)
#define I3C_MWMSG_SDR_CONTROL_DIR_SHIFT          (0U)
#define I3C_MWMSG_SDR_CONTROL_DIR(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_CONTROL_DIR_SHIFT)) & I3C_MWMSG_SDR_CONTROL_DIR_MASK)

/* MWMSG_SDR_DATA */
#define I3C_MWMSG_SDR_DATA_DATA16B_MASK          (0xFFFFU)
#define I3C_MWMSG_SDR_DATA_DATA16B_SHIFT         (0U)
#define I3C_MWMSG_SDR_DATA_DATA16B(x)            (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_SDR_DATA_DATA16B_SHIFT)) & I3C_MWMSG_SDR_DATA_DATA16B_MASK)

/* MRMSG_SDR */
#define I3C_MRMSG_SDR_DATA_MASK                  (0xFFFFU)
#define I3C_MRMSG_SDR_DATA_SHIFT                 (0U)
#define I3C_MRMSG_SDR_DATA(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MRMSG_SDR_DATA_SHIFT)) & I3C_MRMSG_SDR_DATA_MASK)

/* MWMSG_DDR_CONTROL */
#define I3C_MWMSG_DDR_CONTROL_END_MASK           (0x4000U)
#define I3C_MWMSG_DDR_CONTROL_END_SHIFT          (14U)
#define I3C_MWMSG_DDR_CONTROL_END(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_CONTROL_END_SHIFT)) & I3C_MWMSG_DDR_CONTROL_END_MASK)

#define I3C_MWMSG_DDR_CONTROL_LEN_MASK           (0x3FFU)
#define I3C_MWMSG_DDR_CONTROL_LEN_SHIFT          (0U)
#define I3C_MWMSG_DDR_CONTROL_LEN(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_CONTROL_LEN_SHIFT)) & I3C_MWMSG_DDR_CONTROL_LEN_MASK)

#define I3C_MWMSG_DDR_CONTROL_ADDR_MASK           (0xFE00U)
#define I3C_MWMSG_DDR_CONTROL_ADDR_SHIFT          (9U)
#define I3C_MWMSG_DDR_CONTROL_ADDR(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_CONTROL_ADDR_SHIFT)) & I3C_MWMSG_DDR_CONTROL_ADDR_MASK)

#define I3C_MWMSG_DDR_CONTROL_DIR_MASK           (0x80U)
#define I3C_MWMSG_DDR_CONTROL_DIR_SHIFT          (7U)
#define I3C_MWMSG_DDR_CONTROL_DIR(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_CONTROL_DIR_SHIFT)) & I3C_MWMSG_DDR_CONTROL_DIR_MASK)

#define I3C_MWMSG_DDR_CONTROL_CMD_MASK           (0x7FU)
#define I3C_MWMSG_DDR_CONTROL_CMD_SHIFT          (0U)
#define I3C_MWMSG_DDR_CONTROL_CMD(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_CONTROL_CMD_SHIFT)) & I3C_MWMSG_DDR_CONTROL_CMD_MASK)

/* MWMSG_DDR_DATA */
#define I3C_MWMSG_DDR_DATA_DATA16B_MASK          (0xFFFFU)
#define I3C_MWMSG_DDR_DATA_DATA16B_SHIFT         (0U)
#define I3C_MWMSG_DDR_DATA_DATA16B(x)            (((uint32_t)(((uint32_t)(x)) \
<< I3C_MWMSG_DDR_DATA_DATA16B_SHIFT)) & I3C_MWMSG_DDR_DATA_DATA16B_MASK)

/* MRMSG_DDR */
#define I3C_MRMSG_DDR_DATA_MASK                  (0xFFFFU)
#define I3C_MRMSG_DDR_DATA_SHIFT                 (0U)
#define I3C_MRMSG_DDR_DATA(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MRMSG_DDR_DATA_SHIFT)) & I3C_MRMSG_DDR_DATA_MASK)

/* MDYNADDR */
#define I3C_MDYNADDR_DADDR_MASK                  (0xFEU)
#define I3C_MDYNADDR_DADDR_SHIFT                 (1U)
#define I3C_MDYNADDR_DADDR(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDYNADDR_DADDR_SHIFT)) & I3C_MDYNADDR_DADDR_MASK)

#define I3C_MDYNADDR_DAVALID_MASK                (0x1U)
#define I3C_MDYNADDR_DAVALID_SHIFT               (0U)
#define I3C_MDYNADDR_DAVALID(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MDYNADDR_DAVALID_SHIFT)) & I3C_MDYNADDR_DAVALID_MASK)

/* HDRCMD */
#define I3C_HDRCMD_NEWCMD_MASK                (0x80000000U)
#define I3C_HDRCMD_NEWCMD_SHIFT               (31U)
#define I3C_HDRCMD_NEWCMD(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_HDRCMD_NEWCMD_SHIFT)) & I3C_HDRCMD_NEWCMD_MASK)

#define I3C_HDRCMD_OVFLW_MASK                (0x40000000U)
#define I3C_HDRCMD_OVFLW_SHIFT               (30U)
#define I3C_HDRCMD_OVFLW(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_HDRCMD_OVFLW_SHIFT)) & I3C_HDRCMD_OVFLW_MASK)

#define I3C_HDRCMD_CMD0_MASK                (0xFFU)
#define I3C_HDRCMD_CMD0_SHIFT               (0U)
#define I3C_HDRCMD_CMD0(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_HDRCMD_CMD0_SHIFT)) & I3C_HDRCMD_CMD0_MASK)

/* IBIEXT1 */
#define I3C_IBIEXT1_EXT3_MASK                (0xFF000000U)
#define I3C_IBIEXT1_EXT3_SHIFT               (24U)
#define I3C_IBIEXT1_EXT3(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT1_EXT3_SHIFT)) & I3C_IBIEXT1_EXT3_MASK)

#define I3C_IBIEXT1_EXT2_MASK                (0xFF0000U)
#define I3C_IBIEXT1_EXT2_SHIFT               (16U)
#define I3C_IBIEXT1_EXT2(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT1_EXT2_SHIFT)) & I3C_IBIEXT1_EXT2_MASK)

#define I3C_IBIEXT1_EXT1_MASK                (0xFF00U)
#define I3C_IBIEXT1_EXT1_SHIFT               (8U)
#define I3C_IBIEXT1_EXT1(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT1_EXT1_SHIFT)) & I3C_IBIEXT1_EXT1_MASK)

#define I3C_IBIEXT1_MAX_MASK                (0x70U)
#define I3C_IBIEXT1_MAX_SHIFT               (4U)
#define I3C_IBIEXT1_MAX(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT1_MAX_SHIFT)) & I3C_IBIEXT1_MAX_MASK)

#define I3C_IBIEXT1_CNT_MASK                (0x7U)
#define I3C_IBIEXT1_CNT_SHIFT               (0U)
#define I3C_IBIEXT1_CNT(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT1_CNT_SHIFT)) & I3C_IBIEXT1_CNT_MASK)

#define I3C_IBIEXT2_EXT7_MASK                (0xFF000000U)
#define I3C_IBIEXT2_EXT7_SHIFT               (24U)
#define I3C_IBIEXT2_EXT7(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT2_EXT7_SHIFT)) & I3C_IBIEXT2_EXT7_MASK)

#define I3C_IBIEXT2_EXT6_MASK                (0xFF0000U)
#define I3C_IBIEXT2_EXT6_SHIFT               (16U)
#define I3C_IBIEXT2_EXT6(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT2_EXT6_SHIFT)) & I3C_IBIEXT2_EXT6_MASK)

#define I3C_IBIEXT2_EXT5_MASK                (0xFF00U)
#define I3C_IBIEXT2_EXT5_SHIFT               (8U)
#define I3C_IBIEXT2_EXT5(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT2_EXT5_SHIFT)) & I3C_IBIEXT2_EXT5_MASK)

#define I3C_IBIEXT2_EXT4_MASK                (0xFFU)
#define I3C_IBIEXT2_EXT4_SHIFT               (0U)
#define I3C_IBIEXT2_EXT4(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_IBIEXT2_EXT4_SHIFT)) & I3C_IBIEXT2_EXT4_MASK)

/* SID */
#define I3C_SID_ID_MASK                          (0xFFFFFFFFU)
#define I3C_SID_ID_SHIFT                         (0U)
#define I3C_SID_ID(x)                            (((uint32_t)(((uint32_t)(x)) \
<< I3C_SID_ID_SHIFT)) & I3C_SID_ID_MASK)

/* CONFIG */
#define I3C_CONFIG_SADDR_MASK                   (0xFE000000U)
#define I3C_CONFIG_SADDR_SHIFT                  (25U)
#define I3C_CONFIG_SADDR(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_SADDR_SHIFT)) & I3C_CONFIG_SADDR_MASK)

#define I3C_CONFIG_BAMATCH_MASK                 (0x7F0000U)
#define I3C_CONFIG_BAMATCH_SHIFT                (16U)
#define I3C_CONFIG_BAMATCH(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_BAMATCH_SHIFT)) & I3C_CONFIG_BAMATCH_MASK)

#define I3C_CONFIG_HDRCMD_MASK                 (0x400U)
#define I3C_CONFIG_HDRCMD_SHIFT                (10U)
#define I3C_CONFIG_HDRCMD(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_HDRCMD_SHIFT)) & I3C_CONFIG_HDRCMD_MASK)

#define I3C_CONFIG_OFFLINE_MASK                 (0x200U)
#define I3C_CONFIG_OFFLINE_SHIFT                (9U)
#define I3C_CONFIG_OFFLINE(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_OFFLINE_SHIFT)) & I3C_CONFIG_OFFLINE_MASK)

#define I3C_CONFIG_IDRAND_MASK                  (0x100U)
#define I3C_CONFIG_IDRAND_SHIFT                 (8U)
#define I3C_CONFIG_IDRAND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_IDRAND_SHIFT)) & I3C_CONFIG_IDRAND_MASK)

#define I3C_CONFIG_DDROK_MASK                   (0x10U)
#define I3C_CONFIG_DDROK_SHIFT                  (4U)
#define I3C_CONFIG_DDROK(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_DDROK_SHIFT)) & I3C_CONFIG_DDROK_MASK)

#define I3C_CONFIG_S0IGNORE_MASK                (0x8U)
#define I3C_CONFIG_S0IGNORE_SHIFT               (3U)
#define I3C_CONFIG_S0IGNORE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_S0IGNORE_SHIFT)) & I3C_CONFIG_S0IGNORE_MASK)

#define I3C_CONFIG_MATCHSS_MASK                 (0x4U)
#define I3C_CONFIG_MATCHSS_SHIFT                (2U)
#define I3C_CONFIG_MATCHSS(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_MATCHSS_SHIFT)) & I3C_CONFIG_MATCHSS_MASK)

#define I3C_CONFIG_NACK_MASK                    (0x2U)
#define I3C_CONFIG_NACK_SHIFT                   (1U)
#define I3C_CONFIG_NACK(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_NACK_SHIFT)) & I3C_CONFIG_NACK_MASK)

#define I3C_CONFIG_SLVENA_MASK                  (0x1U)
#define I3C_CONFIG_SLVENA_SHIFT                 (0U)
#define I3C_CONFIG_SLVENA(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_CONFIG_SLVENA_SHIFT)) & I3C_CONFIG_SLVENA_MASK)

enum I3C_CONFIG_SLVENA_Enum {
	I3C_CONFIG_SLVENA_SLAVE_OFF = 0, I3C_CONFIG_SLVENA_SLAVE_ON = 1,
};

/* STATUS */
#define I3C_STATUS_TIMECTRL_MASK                (0xC0000000U)
#define I3C_STATUS_TIMECTRL_SHIFT               (30U)
#define I3C_STATUS_TIMECTRL(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_TIMECTRL_SHIFT)) & I3C_STATUS_TIMECTRL_MASK)

#define I3C_STATUS_ACTSTATE_MASK                (0x30000000U)
#define I3C_STATUS_ACTSTATE_SHIFT               (28U)
#define I3C_STATUS_ACTSTATE(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_ACTSTATE_SHIFT)) & I3C_STATUS_ACTSTATE_MASK)

#define I3C_STATUS_HJDIS_MASK                   (0x8000000U)
#define I3C_STATUS_HJDIS_SHIFT                  (27U)
#define I3C_STATUS_HJDIS(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_HJDIS_SHIFT)) & I3C_STATUS_HJDIS_MASK)

#define I3C_STATUS_MRDIS_MASK                   (0x2000000U)
#define I3C_STATUS_MRDIS_SHIFT                  (25U)
#define I3C_STATUS_MRDIS(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_MRDIS_SHIFT)) & I3C_STATUS_MRDIS_MASK)

#define I3C_STATUS_IBIDIS_MASK                  (0x1000000U)
#define I3C_STATUS_IBIDIS_SHIFT                 (24U)
#define I3C_STATUS_IBIDIS(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_IBIDIS_SHIFT)) & I3C_STATUS_IBIDIS_MASK)

#define I3C_STATUS_EVDET_MASK                   (0x300000U)
#define I3C_STATUS_EVDET_SHIFT                  (20U)
#define I3C_STATUS_EVDET(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_EVDET_SHIFT)) & I3C_STATUS_EVDET_MASK)

#define I3C_STATUS_EVENT_MASK                   (0x40000U)
#define I3C_STATUS_EVENT_SHIFT                  (18U)
#define I3C_STATUS_EVENT(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_EVENT_SHIFT)) & I3C_STATUS_EVENT_MASK)

#define I3C_STATUS_CHANDLED_MASK                (0x20000U)
#define I3C_STATUS_CHANDLED_SHIFT               (17U)
#define I3C_STATUS_CHANDLED(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_CHANDLED_SHIFT)) & I3C_STATUS_CHANDLED_MASK)

#define I3C_STATUS_DDRMATCH_MASK                (0x10000U)
#define I3C_STATUS_DDRMATCH_SHIFT               (16U)
#define I3C_STATUS_DDRMATCH(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_DDRMATCH_SHIFT)) & I3C_STATUS_DDRMATCH_MASK)

#define I3C_STATUS_ERRWARN_MASK                 (0x8000U)
#define I3C_STATUS_ERRWARN_SHIFT                (15U)
#define I3C_STATUS_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_ERRWARN_SHIFT)) & I3C_STATUS_ERRWARN_MASK)

#define I3C_STATUS_CCC_MASK                     (0x4000U)
#define I3C_STATUS_CCC_SHIFT                    (14U)
#define I3C_STATUS_CCC(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_CCC_SHIFT)) & I3C_STATUS_CCC_MASK)

#define I3C_STATUS_DACHG_MASK                   (0x2000U)
#define I3C_STATUS_DACHG_SHIFT                  (13U)
#define I3C_STATUS_DACHG(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_DACHG_SHIFT)) & I3C_STATUS_DACHG_MASK)

#define I3C_STATUS_TXNOTFULL_MASK               (0x1000U)
#define I3C_STATUS_TXNOTFULL_SHIFT              (12U)
#define I3C_STATUS_TXNOTFULL(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_TXNOTFULL_SHIFT)) & I3C_STATUS_TXNOTFULL_MASK)

#define I3C_STATUS_RXPEND_MASK                 (0x800U)
#define I3C_STATUS_RXPEND_SHIFT                (11U)
#define I3C_STATUS_RXPEND(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_RXPEND_SHIFT)) & I3C_STATUS_RXPEND_MASK)

#define I3C_STATUS_STOP_MASK                    (0x400U)
#define I3C_STATUS_STOP_SHIFT                   (10U)
#define I3C_STATUS_STOP(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STOP_SHIFT)) & I3C_STATUS_STOP_MASK)

#define I3C_STATUS_MATCHED_MASK                 (0x200U)
#define I3C_STATUS_MATCHED_SHIFT                (9U)
#define I3C_STATUS_MATCHED(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_MATCHED_SHIFT)) & I3C_STATUS_MATCHED_MASK)

#define I3C_STATUS_START_MASK                   (0x100U)
#define I3C_STATUS_START_SHIFT                  (8U)
#define I3C_STATUS_START(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_START_SHIFT)) & I3C_STATUS_START_MASK)

#define I3C_STATUS_STHDR_MASK                   (0x40U)
#define I3C_STATUS_STHDR_SHIFT                  (6U)
#define I3C_STATUS_STHDR(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STHDR_SHIFT)) & I3C_STATUS_STHDR_MASK)

#define I3C_STATUS_STDAA_MASK                   (0x20U)
#define I3C_STATUS_STDAA_SHIFT                  (5U)
#define I3C_STATUS_STDAA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STDAA_SHIFT)) & I3C_STATUS_STDAA_MASK)

#define I3C_STATUS_STREQWR_MASK                 (0x10U)
#define I3C_STATUS_STREQWR_SHIFT                (4U)
#define I3C_STATUS_STREQWR(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STREQWR_SHIFT)) & I3C_STATUS_STREQWR_MASK)

#define I3C_STATUS_STREQRD_MASK                 (0x8U)
#define I3C_STATUS_STREQRD_SHIFT                (3U)
#define I3C_STATUS_STREQRD(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STREQRD_SHIFT)) & I3C_STATUS_STREQRD_MASK)

#define I3C_STATUS_STCCCH_MASK                  (0x4U)
#define I3C_STATUS_STCCCH_SHIFT                 (2U)
#define I3C_STATUS_STCCCH(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STCCCH_SHIFT)) & I3C_STATUS_STCCCH_MASK)

#define I3C_STATUS_STMSG_MASK                   (0x2U)
#define I3C_STATUS_STMSG_SHIFT                  (1U)
#define I3C_STATUS_STMSG(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STMSG_SHIFT)) & I3C_STATUS_STMSG_MASK)

#define I3C_STATUS_STNOTSTOP_MASK               (0x1U)
#define I3C_STATUS_STNOTSTOP_SHIFT              (0U)
#define I3C_STATUS_STNOTSTOP(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_STATUS_STNOTSTOP_SHIFT)) & I3C_STATUS_STNOTSTOP_MASK)

/* CTRL */
#define I3C_CTRL_VENDINFO_MASK                  (0xFF000000U)
#define I3C_CTRL_VENDINFO_SHIFT                 (24U)
#define I3C_CTRL_VENDINFO(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_VENDINFO_SHIFT)) & I3C_CTRL_VENDINFO_MASK)

#define I3C_CTRL_ACTSTATE_MASK                  (0x300000U)
#define I3C_CTRL_ACTSTATE_SHIFT                 (20U)
#define I3C_CTRL_ACTSTATE(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_ACTSTATE_SHIFT)) & I3C_CTRL_ACTSTATE_MASK)

#define I3C_CTRL_PENDINT_MASK                   (0xF0000U)
#define I3C_CTRL_PENDINT_SHIFT                  (16U)
#define I3C_CTRL_PENDINT(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_PENDINT_SHIFT)) & I3C_CTRL_PENDINT_MASK)

#define I3C_CTRL_IBIDATA_MASK                   (0xFF00U)
#define I3C_CTRL_IBIDATA_SHIFT                  (8U)
#define I3C_CTRL_IBIDATA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_IBIDATA_SHIFT)) & I3C_CTRL_IBIDATA_MASK)

#define I3C_CTRL_EXTDATA_MASK                   (0x8U)
#define I3C_CTRL_EXTDATA_SHIFT                  (3U)
#define I3C_CTRL_EXTDATA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_EXTDATA_SHIFT)) & I3C_CTRL_EXTDATA_MASK)

#define I3C_CTRL_EVENT_MASK                     (0x3U)
#define I3C_CTRL_EVENT_SHIFT                    (0U)
#define I3C_CTRL_EVENT(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_CTRL_EVENT_SHIFT)) & I3C_CTRL_EVENT_MASK)

/* INTSET */
#define I3C_INTSET_EVENT_MASK                   (0x40000U)
#define I3C_INTSET_EVENT_SHIFT                  (18U)
#define I3C_INTSET_EVENT(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_EVENT_SHIFT)) & I3C_INTSET_EVENT_MASK)

#define I3C_INTSET_CHANDLED_MASK                (0x20000U)
#define I3C_INTSET_CHANDLED_SHIFT               (17U)
#define I3C_INTSET_CHANDLED(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_CHANDLED_SHIFT)) & I3C_INTSET_CHANDLED_MASK)

#define I3C_INTSET_DDRMATCHED_MASK              (0x10000U)
#define I3C_INTSET_DDRMATCHED_SHIFT             (16U)
#define I3C_INTSET_DDRMATCHED(x)                (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_DDRMATCHED_SHIFT)) & I3C_INTSET_DDRMATCHED_MASK)

#define I3C_INTSET_ERRWARN_MASK                 (0x8000U)
#define I3C_INTSET_ERRWARN_SHIFT                (15U)
#define I3C_INTSET_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_ERRWARN_SHIFT)) & I3C_INTSET_ERRWARN_MASK)

#define I3C_INTSET_CCC_MASK                     (0x4000U)
#define I3C_INTSET_CCC_SHIFT                    (14U)
#define I3C_INTSET_CCC(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_CCC_SHIFT)) & I3C_INTSET_CCC_MASK)

#define I3C_INTSET_DACHG_MASK                   (0x2000U)
#define I3C_INTSET_DACHG_SHIFT                  (13U)
#define I3C_INTSET_DACHG(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_DACHG_SHIFT)) & I3C_INTSET_DACHG_MASK)

#define I3C_INTSET_TXNOTFULL_MASK                  (0x1000U)
#define I3C_INTSET_TXNOTFULL_SHIFT                 (12U)
#define I3C_INTSET_TXNOTFULL(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_TXNOTFULL_SHIFT)) & I3C_INTSET_TXNOTFULL_MASK)

#define I3C_INTSET_RXPEND_MASK                  (0x800U)
#define I3C_INTSET_RXPEND_SHIFT                 (11U)
#define I3C_INTSET_RXPEND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_RXPEND_SHIFT)) & I3C_INTSET_RXPEND_MASK)

#define I3C_INTSET_STOP_MASK                    (0x400U)
#define I3C_INTSET_STOP_SHIFT                   (10U)
#define I3C_INTSET_STOP(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_STOP_SHIFT)) & I3C_INTSET_STOP_MASK)

#define I3C_INTSET_MATCHED_MASK                 (0x200U)
#define I3C_INTSET_MATCHED_SHIFT                (9U)
#define I3C_INTSET_MATCHED(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_MATCHED_SHIFT)) & I3C_INTSET_MATCHED_MASK)

#define I3C_INTSET_START_MASK                   (0x100U)
#define I3C_INTSET_START_SHIFT                  (8U)
#define I3C_INTSET_START(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTSET_START_SHIFT)) & I3C_INTSET_START_MASK)

/* INTCLR */
#define I3C_INTCLR_EVENT_MASK                   (0x40000U)
#define I3C_INTCLR_EVENT_SHIFT                  (18U)
#define I3C_INTCLR_EVENT(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_EVENT_SHIFT)) & I3C_INTCLR_EVENT_MASK)

#define I3C_INTCLR_CHANDLED_MASK                (0x20000U)
#define I3C_INTCLR_CHANDLED_SHIFT               (17U)
#define I3C_INTCLR_CHANDLED(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_CHANDLED_SHIFT)) & I3C_INTCLR_CHANDLED_MASK)

#define I3C_INTCLR_DDRMATCHED_MASK              (0x10000U)
#define I3C_INTCLR_DDRMATCHED_SHIFT             (16U)
#define I3C_INTCLR_DDRMATCHED(x)                (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_DDRMATCHED_SHIFT)) & I3C_INTCLR_DDRMATCHED_MASK)

#define I3C_INTCLR_ERRWARN_MASK                 (0x8000U)
#define I3C_INTCLR_ERRWARN_SHIFT                (15U)
#define I3C_INTCLR_ERRWARN(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_ERRWARN_SHIFT)) & I3C_INTCLR_ERRWARN_MASK)

#define I3C_INTCLR_CCC_MASK                     (0x4000U)
#define I3C_INTCLR_CCC_SHIFT                    (14U)
#define I3C_INTCLR_CCC(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_CCC_SHIFT)) & I3C_INTCLR_CCC_MASK)

#define I3C_INTCLR_DACHG_MASK                   (0x2000U)
#define I3C_INTCLR_DACHG_SHIFT                  (13U)
#define I3C_INTCLR_DACHG(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_DACHG_SHIFT)) & I3C_INTCLR_DACHG_MASK)

#define I3C_INTCLR_TXNOTFULL_MASK                  (0x1000U)
#define I3C_INTCLR_TXNOTFULL_SHIFT                 (12U)
#define I3C_INTCLR_TXNOTFULL(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_TXNOTFULL_SHIFT)) & I3C_INTCLR_TXNOTFULL_MASK)

#define I3C_INTCLR_RXPEND_MASK                  (0x800U)
#define I3C_INTCLR_RXPEND_SHIFT                 (11U)
#define I3C_INTCLR_RXPEND(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_RXPEND_SHIFT)) & I3C_INTCLR_RXPEND_MASK)

#define I3C_INTCLR_STOP_MASK                    (0x400U)
#define I3C_INTCLR_STOP_SHIFT                   (10U)
#define I3C_INTCLR_STOP(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_STOP_SHIFT)) & I3C_INTCLR_STOP_MASK)

#define I3C_INTCLR_MATCHED_MASK                 (0x200U)
#define I3C_INTCLR_MATCHED_SHIFT                (9U)
#define I3C_INTCLR_MATCHED(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_MATCHED_SHIFT)) & I3C_INTCLR_MATCHED_MASK)

#define I3C_INTCLR_START_MASK                   (0x100U)
#define I3C_INTCLR_START_SHIFT                  (8U)
#define I3C_INTCLR_START(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTCLR_START_SHIFT)) & I3C_INTCLR_START_MASK)

/* INTMASKED */
#define I3C_INTMASKED_EVENT_MASK                (0x40000U)
#define I3C_INTMASKED_EVENT_SHIFT               (18U)
#define I3C_INTMASKED_EVENT(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_EVENT_SHIFT)) & I3C_INTMASKED_EVENT_MASK)

#define I3C_INTMASKED_CHANDLED_MASK             (0x20000U)
#define I3C_INTMASKED_CHANDLED_SHIFT            (17U)
#define I3C_INTMASKED_CHANDLED(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_CHANDLED_SHIFT)) & I3C_INTMASKED_CHANDLED_MASK)

#define I3C_INTMASKED_DDRMATCHED_MASK           (0x10000U)
#define I3C_INTMASKED_DDRMATCHED_SHIFT          (16U)
#define I3C_INTMASKED_DDRMATCHED(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_DDRMATCHED_SHIFT)) & I3C_INTMASKED_DDRMATCHED_MASK)

#define I3C_INTMASKED_ERRWARN_MASK              (0x8000U)
#define I3C_INTMASKED_ERRWARN_SHIFT             (15U)
#define I3C_INTMASKED_ERRWARN(x)                (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_ERRWARN_SHIFT)) & I3C_INTMASKED_ERRWARN_MASK)

#define I3C_INTMASKED_CCC_MASK                  (0x4000U)
#define I3C_INTMASKED_CCC_SHIFT                 (14U)
#define I3C_INTMASKED_CCC(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_CCC_SHIFT)) & I3C_INTMASKED_CCC_MASK)

#define I3C_INTMASKED_DACHG_MASK                (0x2000U)
#define I3C_INTMASKED_DACHG_SHIFT               (13U)
#define I3C_INTMASKED_DACHG(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_DACHG_SHIFT)) & I3C_INTMASKED_DACHG_MASK)

#define I3C_INTMASKED_TXNOTFULL_MASK               (0x1000U)
#define I3C_INTMASKED_TXNOTFULL_SHIFT              (12U)
#define I3C_INTMASKED_TXNOTFULL(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_TXNOTFULL_SHIFT)) & I3C_INTMASKED_TXNOTFULL_MASK)

#define I3C_INTMASKED_RXPEND_MASK               (0x800U)
#define I3C_INTMASKED_RXPEND_SHIFT              (11U)
#define I3C_INTMASKED_RXPEND(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_RXPEND_SHIFT)) & I3C_INTMASKED_RXPEND_MASK)

#define I3C_INTMASKED_STOP_MASK                 (0x400U)
#define I3C_INTMASKED_STOP_SHIFT                (10U)
#define I3C_INTMASKED_STOP(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_STOP_SHIFT)) & I3C_INTMASKED_STOP_MASK)

#define I3C_INTMASKED_MATCHED_MASK              (0x200U)
#define I3C_INTMASKED_MATCHED_SHIFT             (9U)
#define I3C_INTMASKED_MATCHED(x)                (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_MATCHED_SHIFT)) & I3C_INTMASKED_MATCHED_MASK)

#define I3C_INTMASKED_START_MASK                (0x100U)
#define I3C_INTMASKED_START_SHIFT               (8U)
#define I3C_INTMASKED_START(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_INTMASKED_START_SHIFT)) & I3C_INTMASKED_START_MASK)

/* ERRWARN */
#define I3C_ERRWARN_OWRITE_MASK                 (0x20000U)
#define I3C_ERRWARN_OWRITE_SHIFT                (17U)
#define I3C_ERRWARN_OWRITE(x)                   (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_OWRITE_SHIFT)) & I3C_ERRWARN_OWRITE_MASK)

#define I3C_ERRWARN_OREAD_MASK                  (0x10000U)
#define I3C_ERRWARN_OREAD_SHIFT                 (16U)
#define I3C_ERRWARN_OREAD(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_OREAD_SHIFT)) & I3C_ERRWARN_OREAD_MASK)

#define I3C_ERRWARN_S0S1_MASK                   (0x800U)
#define I3C_ERRWARN_S0S1_SHIFT                  (11U)
#define I3C_ERRWARN_S0S1(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_S0S1_SHIFT)) & I3C_ERRWARN_S0S1_MASK)

#define I3C_ERRWARN_HCRC_MASK                   (0x400U)
#define I3C_ERRWARN_HCRC_SHIFT                  (10U)
#define I3C_ERRWARN_HCRC(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_HCRC_SHIFT)) & I3C_ERRWARN_HCRC_MASK)

#define I3C_ERRWARN_HPAR_MASK                   (0x200U)
#define I3C_ERRWARN_HPAR_SHIFT                  (9U)
#define I3C_ERRWARN_HPAR(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_HPAR_SHIFT)) & I3C_ERRWARN_HPAR_MASK)

#define I3C_ERRWARN_SPAR_MASK                   (0x100U)
#define I3C_ERRWARN_SPAR_SHIFT                  (8U)
#define I3C_ERRWARN_SPAR(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_SPAR_SHIFT)) & I3C_ERRWARN_SPAR_MASK)

#define I3C_ERRWARN_INVSTART_MASK               (0x10U)
#define I3C_ERRWARN_INVSTART_SHIFT              (4U)
#define I3C_ERRWARN_INVSTART(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_INVSTART_SHIFT)) & I3C_ERRWARN_INVSTART_MASK)

#define I3C_ERRWARN_TERM_MASK                   (0x8U)
#define I3C_ERRWARN_TERM_SHIFT                  (3U)
#define I3C_ERRWARN_TERM(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_TERM_SHIFT)) & I3C_ERRWARN_TERM_MASK)

#define I3C_ERRWARN_URUNNACK_MASK               (0x4U)
#define I3C_ERRWARN_URUNNACK_SHIFT              (2U)
#define I3C_ERRWARN_URUNNACK(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_URUNNACK_SHIFT)) & I3C_ERRWARN_URUNNACK_MASK)

#define I3C_ERRWARN_URUN_MASK                   (0x2U)
#define I3C_ERRWARN_URUN_SHIFT                  (1U)
#define I3C_ERRWARN_URUN(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_URUN_SHIFT)) & I3C_ERRWARN_URUN_MASK)

#define I3C_ERRWARN_ORUN_MASK                   (0x1U)
#define I3C_ERRWARN_ORUN_SHIFT                  (0U)
#define I3C_ERRWARN_ORUN(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_ERRWARN_ORUN_SHIFT)) & I3C_ERRWARN_ORUN_MASK)

enum I3C_ERRWARN_Enum {
	I3C_ERRWARN_OWRITE,
	I3C_ERRWARN_OREAD,
	I3C_ERRWARN_S0S1,
	I3C_ERRWARN_HCRC,
	I3C_ERRWARN_HPAR,
	I3C_ERRWARN_SPAR,
	I3C_ERRWARN_INVSTART,
	I3C_ERRWARN_TERM,
	I3C_ERRWARN_URUNNACK,
	I3C_ERRWARN_URUN,
	I3C_ERRWARN_ORUN,
};

/* DMACTRL */
#define I3C_DMACTRL_DMAWIDTH_MASK               (0x30U)
#define I3C_DMACTRL_DMAWIDTH_SHIFT              (4U)
#define I3C_DMACTRL_DMAWIDTH(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DMACTRL_DMAWIDTH_SHIFT)) & I3C_DMACTRL_DMAWIDTH_MASK)
#define I3C_DMACTRL_DMAWIDTH_BYTE           (0x01U)
#define I3C_DMACTRL_DMAWIDTH_HALF_WORD      (0x02U)

#define I3C_DMACTRL_DMATB_MASK                  (0xCU)
#define I3C_DMACTRL_DMATB_SHIFT                 (2U)
#define I3C_DMACTRL_DMATB(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_DMACTRL_DMATB_SHIFT)) & I3C_DMACTRL_DMATB_MASK)

#define I3C_DMACTRL_DMAFB_MASK                  (0x3U)
#define I3C_DMACTRL_DMAFB_SHIFT                 (0U)
#define I3C_DMACTRL_DMAFB(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_DMACTRL_DMAFB_SHIFT)) & I3C_DMACTRL_DMAFB_MASK)

/* DATACTRL */
#define I3C_DATACTRL_RXEMPTY_MASK               (0x80000000U)
#define I3C_DATACTRL_RXEMPTY_SHIFT              (31U)
#define I3C_DATACTRL_RXEMPTY(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_RXEMPTY_SHIFT)) & I3C_DATACTRL_RXEMPTY_MASK)

#define I3C_DATACTRL_TXFULL_MASK                (0x40000000U)
#define I3C_DATACTRL_TXFULL_SHIFT               (30U)
#define I3C_DATACTRL_TXFULL(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_TXFULL_SHIFT)) & I3C_DATACTRL_TXFULL_MASK)

#define I3C_DATACTRL_RXCOUNT_MASK               (0x1F000000U)
#define I3C_DATACTRL_RXCOUNT_SHIFT              (24U)
#define I3C_DATACTRL_RXCOUNT(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_RXCOUNT_SHIFT)) & I3C_DATACTRL_RXCOUNT_MASK)

#define I3C_DATACTRL_TXCOUNT_MASK               (0x1F0000U)
#define I3C_DATACTRL_TXCOUNT_SHIFT              (16U)
#define I3C_DATACTRL_TXCOUNT(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_TXCOUNT_SHIFT)) & I3C_DATACTRL_TXCOUNT_MASK)

#define I3C_DATACTRL_RXTRIG_MASK                (0xC0U)
#define I3C_DATACTRL_RXTRIG_SHIFT               (6U)
#define I3C_DATACTRL_RXTRIG(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_RXTRIG_SHIFT)) & I3C_DATACTRL_RXTRIG_MASK)

#define I3C_DATACTRL_TXTRIG_MASK                (0x30U)
#define I3C_DATACTRL_TXTRIG_SHIFT               (4U)
#define I3C_DATACTRL_TXTRIG(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_TXTRIG_SHIFT)) & I3C_DATACTRL_TXTRIG_MASK)

#define I3C_DATACTRL_UNLOCK_MASK                (0x8U)
#define I3C_DATACTRL_UNLOCK_SHIFT               (3U)
#define I3C_DATACTRL_UNLOCK(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_UNLOCK_SHIFT)) & I3C_DATACTRL_UNLOCK_MASK)

#define I3C_DATACTRL_FLUSHFB_MASK               (0x2U)
#define I3C_DATACTRL_FLUSHFB_SHIFT              (1U)
#define I3C_DATACTRL_FLUSHFB(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_FLUSHFB_SHIFT)) & I3C_DATACTRL_FLUSHFB_MASK)

#define I3C_DATACTRL_FLUSHTB_MASK               (0x1U)
#define I3C_DATACTRL_FLUSHTB_SHIFT              (0U)
#define I3C_DATACTRL_FLUSHTB(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_DATACTRL_FLUSHTB_SHIFT)) & I3C_DATACTRL_FLUSHTB_MASK)

/* WDATAB */
#define I3C_WDATAB_END_A_MASK                (0x10000U)
#define I3C_WDATAB_END_A_SHIFT               (16U)
#define I3C_WDATAB_END_A(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAB_END_A_SHIFT)) & I3C_WDATAB_END_A_MASK)

#define I3C_WDATAB_END_B_MASK                     (0x100U)
#define I3C_WDATAB_END_B_SHIFT                    (8U)
#define I3C_WDATAB_END_B(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAB_END_B_SHIFT)) & I3C_WDATAB_END_B_MASK)

#define I3C_WDATAB_DATA_MASK                    (0xFFU)
#define I3C_WDATAB_DATA_SHIFT                   (0U)
#define I3C_WDATAB_DATA(x)                      (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAB_DATA_SHIFT)) & I3C_WDATAB_DATA_MASK)

/* WDATABE */
#define I3C_WDATABE_DATA_MASK                   (0xFFU)
#define I3C_WDATABE_DATA_SHIFT                  (0U)
#define I3C_WDATABE_DATA(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATABE_DATA_SHIFT)) & I3C_WDATABE_DATA_MASK)

/* WDATAH */
#define I3C_WDATAH_END_MASK                     (0x10000U)
#define I3C_WDATAH_END_SHIFT                    (16U)
#define I3C_WDATAH_END(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAH_END_SHIFT)) & I3C_WDATAH_END_MASK)

#define I3C_WDATAH_DATA1_MASK                   (0xFF00U)
#define I3C_WDATAH_DATA1_SHIFT                  (8U)
#define I3C_WDATAH_DATA1(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAH_DATA1_SHIFT)) & I3C_WDATAH_DATA1_MASK)

#define I3C_WDATAH_DATA0_MASK                   (0xFFU)
#define I3C_WDATAH_DATA0_SHIFT                  (0U)
#define I3C_WDATAH_DATA0(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAH_DATA0_SHIFT)) & I3C_WDATAH_DATA0_MASK)

/* WDATAHE */
#define I3C_WDATAHE_DATA1_MASK                  (0xFF00U)
#define I3C_WDATAHE_DATA1_SHIFT                 (8U)
#define I3C_WDATAHE_DATA1(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAHE_DATA1_SHIFT)) & I3C_WDATAHE_DATA1_MASK)

#define I3C_WDATAHE_DATA0_MASK                  (0xFFU)
#define I3C_WDATAHE_DATA0_SHIFT                 (0U)
#define I3C_WDATAHE_DATA0(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAHE_DATA0_SHIFT)) & I3C_WDATAHE_DATA0_MASK)

/* RDATAB */
#define I3C_RDATAB_DATA0_MASK                   (0xFFU)
#define I3C_RDATAB_DATA0_SHIFT                  (0U)
#define I3C_RDATAB_DATA0(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_RDATAB_DATA0_SHIFT)) & I3C_RDATAB_DATA0_MASK)

/* RDATAH */
#define I3C_RDATAH_DATA1_MASK                     (0xFF00U)
#define I3C_RDATAH_DATA1_SHIFT                    (8U)
#define I3C_RDATAH_DATA1(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_RDATAH_DATA1_SHIFT)) & I3C_RDATAH_DATA1_MASK)

#define I3C_RDATAH_DATA0_MASK                     (0xFFU)
#define I3C_RDATAH_DATA0_SHIFT                    (0U)
#define I3C_RDATAH_DATA0(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_RDATAH_DATA0_SHIFT)) & I3C_RDATAH_DATA0_MASK)

/* WDATAB1 */
#define I3C_WDATAB1_DATA_MASK                     (0xFFU)
#define I3C_WDATAB1_DATA_SHIFT                    (0U)
#define I3C_WDATAB1_DATA(x)                       (((uint32_t)(((uint32_t)(x)) \
<< I3C_WDATAB1_DATA_SHIFT)) & I3C_WDATAB1_DATA_MASK)

/* CAPABILITIES */
#define I3C_CAPABILITIES_DMA_MASK               (0x80000000U)
#define I3C_CAPABILITIES_DMA_SHIFT              (31U)
#define I3C_CAPABILITIES_DMA(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_DMA_SHIFT)) & I3C_CAPABILITIES_DMA_MASK)

#define I3C_CAPABILITIES_INT_MASK               (0x40000000U)
#define I3C_CAPABILITIES_INT_SHIFT              (30U)
#define I3C_CAPABILITIES_INT(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_INT_SHIFT)) & I3C_CAPABILITIES_INT_MASK)

#define I3C_CAPABILITIES_FIFORX_SHIFT           (28U)
#define I3C_CAPABILITIES_FIFORX_MASK            (0x30000000U)
#define I3C_CAPABILITIES_FIFORX(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_FIFORX_SHIFT)) & I3C_CAPABILITIES_FIFORX_MASK)

#define I3C_CAPABILITIES_FIFOTX_MASK            (0xC000000U)
#define I3C_CAPABILITIES_FIFOTX_SHIFT           (26U)
#define I3C_CAPABILITIES_FIFOTX(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_FIFOTX_SHIFT)) & I3C_CAPABILITIES_FIFOTX_MASK)

#define I3C_CAPABILITIES_EXTFIFO_MASK           (0x3800000U)
#define I3C_CAPABILITIES_EXTFIFO_SHIFT          (23U)
#define I3C_CAPABILITIES_EXTFIFO(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_EXTFIFO_SHIFT)) & I3C_CAPABILITIES_EXTFIFO_MASK)

#define I3C_CAPABILITIES_TIMECTRL_MASK          (0x200000U)
#define I3C_CAPABILITIES_TIMECTRL_SHIFT         (21U)
#define I3C_CAPABILITIES_TIMECTRL(x)            (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_TIMECTRL_SHIFT)) & I3C_CAPABILITIES_TIMECTRL_MASK)

#define I3C_CAPABILITIES_IBI_MR_HJ_MASK         (0x1F0000U)
#define I3C_CAPABILITIES_IBI_MR_HJ_SHIFT        (16U)
#define I3C_CAPABILITIES_IBI_MR_HJ(x)           (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_IBI_MR_HJ_SHIFT)) & I3C_CAPABILITIES_IBI_MR_HJ_MASK)

#define I3C_CAPABILITIES_BAMATCH_SUPP_MASK          (0x100000U)
#define I3C_CAPABILITIES_BAMATCH_SUPP_SHIFT         (20U)

#define I3C_CAPABILITIES_HOTJOIN_SUPP_MASK          (0x080000U)
#define I3C_CAPABILITIES_HOTJOIN_SUPP_SHIFT         (19U)

#define I3C_CAPABILITIES_MASTER_REQUEST_SUPP_MASK   (0x040000U)
#define I3C_CAPABILITIES_MASTER_REQUEST_SUPP_SHIFT  (18U)

#define I3C_CAPABILITIES_IBI_SUPP_MASK              (0x020000U)
#define I3C_CAPABILITIES_IBI_SUPP_SHIFT             (17U)

#define I3C_CAPABILITIES_IBI_PAYLOAD_MASK           (0x010000U)
#define I3C_CAPABILITIES_IBI_PAYLOAD_SHIFT          (16U)

#define I3C_CAPABILITIES_CCCHANDLE_MASK         (0xF000U)
#define I3C_CAPABILITIES_CCCHANDLE_SHIFT        (12U)
#define I3C_CAPABILITIES_CCCHANDLE(x)           (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_CCCHANDLE_SHIFT)) & I3C_CAPABILITIES_CCCHANDLE_MASK)

#define I3C_CAPABILITIES_SADDR_MASK             (0xC00U)
#define I3C_CAPABILITIES_SADDR_SHIFT            (10U)
#define I3C_CAPABILITIES_SADDR(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_SADDR_SHIFT)) & I3C_CAPABILITIES_SADDR_MASK)

#define I3C_CAPABILITIES_MASTER_MASK            (0x200U)
#define I3C_CAPABILITIES_MASTER_SHIFT           (9U)
#define I3C_CAPABILITIES_MASTER(x)              (((uint32_t)(((uint32_t)(x)) \
<< I3C_SCAPABILITIES_MASTER_SHIFT)) & I3C_SCAPABILITIES_MASTER_MASK)

#define I3C_CAPABILITIES_HDRSUPP_MASK           (0x1C0U)
#define I3C_CAPABILITIES_HDRSUPP_SHIFT          (6U)
#define I3C_CAPABILITIES_HDRSUPP(x)             (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_HDRSUPP_SHIFT)) & I3C_CAPABILITIES_HDRSUPP_MASK)

#define I3C_CAPABILITIES_IDREG_MASK             (0x3CU)
#define I3C_CAPABILITIES_IDREG_SHIFT            (2U)
#define I3C_CAPABILITIES_IDREG(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_IDREG_SHIFT)) & I3C_CAPABILITIES_IDREG_MASK)

#define I3C_CAPABILITIES_IDENA_MASK             (0x3U)
#define I3C_CAPABILITIES_IDENA_SHIFT            (0U)
#define I3C_CAPABILITIES_IDENA(x)               (((uint32_t)(((uint32_t)(x)) \
<< I3C_CAPABILITIES_IDENA_SHIFT)) & I3C_CAPABILITIES_IDENA_MASK)

/* DYNADDR */
#define I3C_DYNADDR_DADDR_MASK                  (0xFEU)
#define I3C_DYNADDR_DADDR_SHIFT                 (1U)
#define I3C_DYNADDR_DADDR(x)                    (((uint32_t)(((uint32_t)(x)) \
<< I3C_DYNADDR_DADDR_SHIFT)) & I3C_DYNADDR_DADDR_MASK)

#define I3C_DYNADDR_DAVALID_MASK                (0x1U)
#define I3C_DYNADDR_DAVALID_SHIFT               (0U)
#define I3C_DYNADDR_DAVALID(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_DYNADDR_DAVALID_SHIFT)) & I3C_DYNADDR_DAVALID_MASK)

/* MAXLIMITS */
#define I3C_MAXLIMITS_MAXWR_MASK                (0xFFF0000U)
#define I3C_MAXLIMITS_MAXWR_SHIFT               (16U)
#define I3C_MAXLIMITS_MAXWR(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MAXLIMITS_MAXWR_SHIFT)) & I3C_MAXLIMITS_MAXWR_MASK)

#define I3C_MAXLIMITS_MAXRD_MASK                (0xFFFU)
#define I3C_MAXLIMITS_MAXRD_SHIFT               (0U)
#define I3C_MAXLIMITS_MAXRD(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_MAXLIMITS_MAXRD_SHIFT)) & I3C_MAXLIMITS_MAXRD_MASK)

/* PARTNO */
#define I3C_PARTNO_PARTNO_MASK                (0xFFFFFFFFU)
#define I3C_PARTNO_PARTNO_SHIFT               (0U)
#define I3C_PARTNO_PARTNO(x)                  (((uint32_t)(((uint32_t)(x)) \
<< I3C_PARTNO_PARTNO_SHIFT)) & I3C_PARTNO_PARTNO_MASK)

/* IDEXT */
#define I3C_IDEXT_BCR_MASK                      (0xFF0000U)
#define I3C_IDEXT_BCR_SHIFT                     (16U)
#define I3C_IDEXT_BCR(x)                        (((uint32_t)(((uint32_t)(x)) \
<< I3C_IDEXT_BCR_SHIFT)) & I3C_IDEXT_BCR_MASK)

#define I3C_IDEXT_DCR_MASK                      (0xFF00U)
#define I3C_IDEXT_DCR_SHIFT                     (8U)
#define I3C_IDEXT_DCR(x)                        (((uint32_t)(((uint32_t)(x)) \
<< I3C_IDEXT_DCR_SHIFT)) & I3C_IDEXT_DCR_MASK)

/* VENDORID */
#define I3C_VENDORID_VID_MASK                   (0x7FFFU)
#define I3C_VENDORID_VID_SHIFT                  (0U)
#define I3C_VENDORID_VID(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_VENDORID_VID_SHIFT)) & I3C_VENDORID_VID_MASK)

/* TCCLOCK */
#define I3C_TCCLOCK_FREQ_MASK                   (0xFF00U)
#define I3C_TCCLOCK_FREQ_SHIFT                  (8U)
#define I3C_TCCLOCK_FREQ(x)                     (((uint32_t)(((uint32_t)(x)) \
<< I3C_TCCLOCK_FREQ_SHIFT)) & I3C_TCCLOCK_FREQ_MASK)

#define I3C_TCCLOCK_ACCURACY_MASK               (0xFFU)
#define I3C_TCCLOCK_ACCURACY_SHIFT              (0U)
#define I3C_TCCLOCK_ACCURACY(x)                 (((uint32_t)(((uint32_t)(x)) \
<< I3C_TCCLOCK_ACCURACY_SHIFT)) & I3C_TCCLOCK_ACCURACY_MASK)


#define PDMA_DSCT_CTL_TXCNT_Pos                     (16)
#define PDMA_DSCT_CTL_TXCNT_Msk                     (0x3ffful << PDMA_DSCT_CTL_TXCNT_Pos)

#define PDMA_DSCT_CTL_TXWIDTH_Pos                   (12)
#define PDMA_DSCT_CTL_TXWIDTH_Msk                   (0x3ul << PDMA_DSCT_CTL_TXWIDTH_Pos)
#define PDMA_WIDTH_8        0x00000000UL
#define PDMA_WIDTH_16       0x00001000UL
#define PDMA_WIDTH_32       0x00002000UL

#define PDMA_DSCT_CTL_DAINC_Pos                     (10)
#define PDMA_DSCT_CTL_DAINC_Msk                     (0x3ul << PDMA_DSCT_CTL_DAINC_Pos)
#define PDMA_DAR_INC        0x00000000UL
#define PDMA_DAR_FIX        0x00000C00UL

#define PDMA_DSCT_CTL_SAINC_Pos                     (8)
#define PDMA_DSCT_CTL_SAINC_Msk                     (0x3ul << PDMA_DSCT_CTL_SAINC_Pos)
#define PDMA_SAR_INC        0x00000000UL
#define PDMA_SAR_FIX        0x00000300UL

#define PDMA_DSCT_CTL_TBINTDIS_Pos                  (7)
#define PDMA_DSCT_CTL_TBINTDIS_Msk                  (0x1ul << PDMA_DSCT_CTL_TBINTDIS_Pos)

#define PDMA_DSCT_CTL_BURSIZE_Pos                   (4)
#define PDMA_DSCT_CTL_BURSIZE_Msk                   (0x7ul << PDMA_DSCT_CTL_BURSIZE_Pos)
#define PDMA_BURST_128      0x00000000UL
#define PDMA_BURST_64       0x00000010UL
#define PDMA_BURST_32       0x00000020UL
#define PDMA_BURST_16       0x00000030UL
#define PDMA_BURST_8        0x00000040UL
#define PDMA_BURST_4        0x00000050UL
#define PDMA_BURST_2        0x00000060UL
#define PDMA_BURST_1        0x00000070UL

#define PDMA_DSCT_CTL_TXTYPE_Pos                    (2)
#define PDMA_DSCT_CTL_TXTYPE_Msk                    (0x1ul << PDMA_DSCT_CTL_TXTYPE_Pos)
#define PDMA_REQ_SINGLE     0x00000004UL
#define PDMA_REQ_BURST      0x00000000UL

#define PDMA_DSCT_CTL_OPMODE_Pos                    (0)
#define PDMA_DSCT_CTL_OPMODE_Msk                    (0x3ul << PDMA_DSCT_CTL_OPMODE_Pos)
#define PDMA_OP_STOP        0x00000000UL
#define PDMA_OP_BASIC       0x00000001UL
#define PDMA_OP_SCATTER     0x00000002UL

#define PDMA_DSCT_ENDSA_ENDSA_Pos                   (0)
#define PDMA_DSCT_ENDSA_ENDSA_Msk                   (0xfffffffful << PDMA_DSCT_ENDSA_ENDSA_Pos)

#define PDMA_DSCT_ENDDA_ENDDA_Pos                   (0)
#define PDMA_DSCT_ENDDA_ENDDA_Msk                   (0xfffffffful << PDMA_DSCT_ENDDA_ENDDA_Pos)

#define PDMA_DSCT_NEXT_NEXT_Pos                     (2)
#define PDMA_DSCT_NEXT_NEXT_Msk                     (0x3ffful << PDMA_DSCT_NEXT_NEXT_Pos)

#define PDMA_CHCTL_CHEN_Pos                         (0)
#define PDMA_CHCTL_CHEN_Msk                         (0xfffful << PDMA_CHCTL_CHEN_Pos)

#define PDMA_STOP_STOP_Pos                          (0)
#define PDMA_STOP_STOP_Msk                          (0xfffful << PDMA_STOP_STOP_Pos)

#define PDMA_SWREQ_SWREQ_Pos                        (0)
#define PDMA_SWREQ_SWREQ_Msk                        (0xffful << PDMA_SWREQ_SWREQ_Pos)

#define PDMA_TRGSTS_REQSTS_Pos                      (0)
#define PDMA_TRGSTS_REQSTS_Msk                      (0xfffful << PDMA_TRGSTS_REQSTS_Pos)

#define PDMA_PRISET_FPRISET_Pos                     (0)
#define PDMA_PRISET_FPRISET_Msk                     (0xfffful << PDMA_PRISET_FPRISET_Pos)

#define PDMA_PRICLR_FPRICLR_Pos                     (0)
#define PDMA_PRICLR_FPRICLR_Msk                     (0xfffful << PDMA_PRICLR_FPRICLR_Pos)

#define PDMA_INTEN_INTEN_Pos                        (0)
#define PDMA_INTEN_INTEN_Msk                        (0xfffful << PDMA_INTEN_INTEN_Pos)

#define PDMA_INTSTS_REQTOFX_Pos                     (8)
#define PDMA_INTSTS_REQTOFX_Msk                     (0xfffful << PDMA_INTSTS_REQTOFX_Pos)

#define PDMA_INTSTS_TEIF_Pos                        (2)
#define PDMA_INTSTS_TEIF_Msk                        (0x1ul << PDMA_INTSTS_TEIF_Pos)

#define PDMA_INTSTS_TDIF_Pos                        (1)
#define PDMA_INTSTS_TDIF_Msk                        (0x1ul << PDMA_INTSTS_TDIF_Pos)

#define PDMA_INTSTS_ABTIF_Pos                       (0)
#define PDMA_INTSTS_ABTIF_Msk                       (0x1ul << PDMA_INTSTS_ABTIF_Pos)

#define PDMA_ABTSTS_ABTIF_Pos                       (0)
#define PDMA_ABTSTS_ABTIF_Msk                       (0xfffful << PDMA_ABTSTS_ABTIF_Pos)

#define PDMA_TDSTS_TDIF_Pos                         (0)
#define PDMA_TDSTS_TDIF_Msk                         (0xfffful << PDMA_TDSTS_TDIF_Pos)

#define PDMA_SCATSTS_TEMPTYF_Pos                    (0)
#define PDMA_SCATSTS_TEMPTYF_Msk                    (0xfffful << PDMA_SCATSTS_TEMPTYF_Pos)

#define PDMA_TACTSTS_TXACTF_Pos                     (0)
#define PDMA_TACTSTS_TXACTF_Msk                     (0xfffful << PDMA_TACTSTS_TXACTF_Pos)

#define PDMA_SCATBA_SCATBA_Pos                      (16)
#define PDMA_SCATBA_SCATBA_Msk                      (0xfffful << PDMA_SCATBA_SCATBA_Pos)

#define PDMA_TOC0_1_TOC1_Pos                        (16)
#define PDMA_TOC0_1_TOC1_Msk                        (0xfffful << PDMA_TOC0_1_TOC1_Pos)

#define PDMA_TOC0_1_TOC0_Pos                        (0)
#define PDMA_TOC0_1_TOC0_Msk                        (0xfffful << PDMA_TOC0_1_TOC0_Pos)

#define PDMA_TOC2_3_TOC3_Pos                        (16)
#define PDMA_TOC2_3_TOC3_Msk                        (0xfffful << PDMA_TOC2_3_TOC3_Pos)

#define PDMA_TOC2_3_TOC2_Pos                        (0)
#define PDMA_TOC2_3_TOC2_Msk                        (0xfffful << PDMA_TOC2_3_TOC2_Pos)

#define PDMA_TOC4_5_TOC5_Pos                        (16)
#define PDMA_TOC4_5_TOC5_Msk                        (0xfffful << PDMA_TOC4_5_TOC5_Pos)

#define PDMA_TOC4_5_TOC4_Pos                        (0)
#define PDMA_TOC4_5_TOC4_Msk                        (0xfffful << PDMA_TOC4_5_TOC4_Pos)

#define PDMA_TOC6_7_TOC7_Pos                        (16)
#define PDMA_TOC6_7_TOC7_Msk                        (0xfffful << PDMA_TOC6_7_TOC7_Pos)

#define PDMA_TOC6_7_TOC6_Pos                        (0)
#define PDMA_TOC6_7_TOC6_Msk                        (0xfffful << PDMA_TOC6_7_TOC6_Pos)

#define PDMA_TOC8_9_TOC9_Pos                        (16)
#define PDMA_TOC8_9_TOC9_Msk                        (0xfffful << PDMA_TOC8_9_TOC9_Pos)

#define PDMA_TOC8_9_TOC8_Pos                        (0)
#define PDMA_TOC8_9_TOC8_Msk                        (0xfffful << PDMA_TOC8_9_TOC8_Pos)

#define PDMA_TOC10_11_TOC11_Pos                     (16)
#define PDMA_TOC10_11_TOC11_Msk                     (0xfffful << PDMA_TOC10_11_TOC11_Pos)

#define PDMA_TOC10_11_TOC10_Pos                     (0)
#define PDMA_TOC10_11_TOC10_Msk                     (0xfffful << PDMA_TOC10_11_TOC10_Pos)

#define PDMA_TOC12_13_TOC13_Pos                     (16)
#define PDMA_TOC12_13_TOC13_Msk                     (0xfffful << PDMA_TOC12_13_TOC13_Pos)

#define PDMA_TOC12_13_TOC12_Pos                     (0)
#define PDMA_TOC12_13_TOC12_Msk                     (0xfffful << PDMA_TOC12_13_TOC12_Pos)

#define PDMA_TOC14_15_TOC15_Pos                     (16)
#define PDMA_TOC14_15_TOC15_Msk                     (0xfffful << PDMA_TOC14_15_TOC15_Pos)

#define PDMA_TOC14_15_TOC14_Pos                     (0)
#define PDMA_TOC14_15_TOC14_Msk                     (0xfffful << PDMA_TOC14_15_TOC14_Pos)

#define PDMA_REQSEL0_3_REQSRC3_Pos                  (24)
#define PDMA_REQSEL0_3_REQSRC3_Msk                  (0x1ful << PDMA_REQSEL0_3_REQSRC3_Pos)

#define PDMA_REQSEL0_3_REQSRC2_Pos                  (16)
#define PDMA_REQSEL0_3_REQSRC2_Msk                  (0x1ful << PDMA_REQSEL0_3_REQSRC2_Pos)

#define PDMA_REQSEL0_3_REQSRC1_Pos                  (8)
#define PDMA_REQSEL0_3_REQSRC1_Msk                  (0x1ful << PDMA_REQSEL0_3_REQSRC1_Pos)

#define PDMA_REQSEL0_3_REQSRC0_Pos                  (0)
#define PDMA_REQSEL0_3_REQSRC0_Msk                  (0x1ful << PDMA_REQSEL0_3_REQSRC0_Pos)

#define PDMA_REQSEL4_7_REQSRC7_Pos                  (24)
#define PDMA_REQSEL4_7_REQSRC7_Msk                  (0x1ful << PDMA_REQSEL4_7_REQSRC7_Pos)

#define PDMA_REQSEL4_7_REQSRC6_Pos                  (16)
#define PDMA_REQSEL4_7_REQSRC6_Msk                  (0x1ful << PDMA_REQSEL4_7_REQSRC6_Pos)

#define PDMA_REQSEL4_7_REQSRC5_Pos                  (8)
#define PDMA_REQSEL4_7_REQSRC5_Msk                  (0x1ful << PDMA_REQSEL4_7_REQSRC5_Pos)

#define PDMA_REQSEL4_7_REQSRC4_Pos                  (0)
#define PDMA_REQSEL4_7_REQSRC4_Msk                  (0x1ful << PDMA_REQSEL4_7_REQSRC4_Pos)

#define PDMA_REQSEL8_11_REQSRC11_Pos                (24)
#define PDMA_REQSEL8_11_REQSRC11_Msk                (0x1ful << PDMA_REQSEL8_11_REQSRC11_Pos)

#define PDMA_REQSEL8_11_REQSRC10_Pos                (16)
#define PDMA_REQSEL8_11_REQSRC10_Msk                (0x1ful << PDMA_REQSEL8_11_REQSRC10_Pos)

#define PDMA_REQSEL8_11_REQSRC9_Pos                 (8)
#define PDMA_REQSEL8_11_REQSRC9_Msk                 (0x1ful << PDMA_REQSEL8_11_REQSRC9_Pos)

#define PDMA_REQSEL8_11_REQSRC8_Pos                 (0)
#define PDMA_REQSEL8_11_REQSRC8_Msk                 (0x1ful << PDMA_REQSEL8_11_REQSRC8_Pos)

#define PDMA_REQSEL12_15_REQSRC15_Pos               (24)
#define PDMA_REQSEL12_15_REQSRC15_Msk               (0x1ful << PDMA_REQSEL12_15_REQSRC15_Pos)

#define PDMA_REQSEL12_15_REQSRC14_Pos               (16)
#define PDMA_REQSEL12_15_REQSRC14_Msk               (0x1ful << PDMA_REQSEL12_15_REQSRC14_Pos)

#define PDMA_REQSEL12_15_REQSRC13_Pos               (8)
#define PDMA_REQSEL12_15_REQSRC13_Msk               (0x1ful << PDMA_REQSEL12_15_REQSRC13_Pos)

#define PDMA_REQSEL12_15_REQSRC12_Pos               (0)
#define PDMA_REQSEL12_15_REQSRC12_Msk               (0x1ful << PDMA_REQSEL12_15_REQSRC12_Pos)

/* PDMA Channel Select */
#define PDMA_I3C1_RX        5
#define PDMA_I3C1_TX        6
#define PDMA_I3C2_RX        7
#define PDMA_I3C2_TX        8
#define PDMA_I3C3_RX        9
#define PDMA_I3C3_TX        10
#define PDMA_I3C4_RX        11
#define PDMA_I3C4_TX        12
#define PDMA_I3C5_RX        13
#define PDMA_I3C5_TX        14
#define PDMA_I3C6_RX        15
#define PDMA_I3C6_TX        16

#define PDMA_OFFSET 0
#define PDMA_CH_MAX    16

#ifdef __cplusplus
extern "C"
	{
#endif

#ifdef __cplusplus
	}
#endif

#endif /* _I3C_H */
