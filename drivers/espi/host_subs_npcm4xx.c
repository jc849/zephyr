/*
 * Copyright (c) 2020 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_host_sub

/**
 * @file
 * @brief Nuvoton NPCM4XX host sub modules driver
 *
 * This file contains the drivers of NPCM4XX Host Sub-Modules that serve as an
 * interface between the Host and Core domains. Please refer the block diagram.
 *
 *                                        +------------+
 *                                        |   KCS/PM   |<--->|
 *                                  +<--->|   Channels |     |
 *                                  |     +------------+     |
 *                                  |     +------------+     |
 *                +------------+    |<--->|    Core    |<--->|
 *   eSPI_CLK --->|  eSPI Bus  |    |     |   to Host  |     |
 *   eSPI_RST --->| Controller |    |     +------------+     |
 * eSPI_IO3-0 <-->|            |<-->|     +------------+     |
 *    eSPI_CS --->| (eSPI mode)|    |     |   Shared   |     |
 * eSPI_ALERT <-->|            |    |<--->|   Memory   |<--->|
 *                +------------+    |     +------------+     |
 *                                  |     +------------+     |
 *                                  |<--->|    MSWC    |<--->|
 *                                  |     +------------+     |
 *                                  |                        |
 *                                  |                        |
 *                                  |                        |
 *                                  |                        |
 *                                  |                        |
 *                                HMIB                       | Core Bus
 *                     (Host Modules Internal Bus)           +------------
 *
 *
 * For most of them, the Host can configure these modules via eSPI(Peripheral
 * Channel)/LPC by accessing 'Configuration and Control register Set' which IO
 * base address is 0x2E as default. (The table below illustrates structure of
 * 'Configuration and Control Register Set') And the interrupts in core domain
 * help handling any events from host side.
 *
 *   Index |     Configuration and Control Register Set
 * --------|--------------------------------------------------+   Bank Select
 *    07h  |      Logical Device Number Register (LDN)        |---------+
 * --------|---------------------------------------------------         |
 *  20-2Fh |        SuperI/O Configuration Registers          |         |
 * ------------------------------------------------------------         |
 * --------|---------------------------------------------------_        |
 *    30h  |      Logical Device Control Register             | |_      |
 * --------|--------------------------------------------------- | |_    |
 *  60-63h |   I/O Space Configuration Registers              | | | |   |
 * --------|--------------------------------------------------- | | |   |
 *  70-71h |     Interrupt Configuration Registers            | | | |   |
 * --------|--------------------------------------------------- | | |<--+
 *  F0-FFh | Special Logical Device Configuration Registers   | | | |
 * --------|--------------------------------------------------- | | |
 *           |--------------------------------------------------- | |
 *             |--------------------------------------------------- |
 *               |---------------------------------------------------
 *
 *
 * This driver introduces four host sub-modules. It includes:
 *
 * 1. KCS/Power Management (PM) channels.
 *   ● KCS/PM channel registers
 *     — Command/Status register
 *     — Data register
 *       channel 1: legacy 62h, 66h; channel 2: legacy 68h, 6Ch
 *       (Zephyr setting: 200h, 204h);
 *       channel 3: legacy 6Ah, 6Eh; channel 4: legacy 6Bh, 6Fh;
 *   ● KCS/PM interrupt using:
 *     — Serial IRQ
 *     — SMI
 *     — EC_SCI
 *   ● Configured by four logical devices: KCS/PM1/2/3/4 (LDN 0x11/0x12/0x17/0x1E)
 *
 * 2. Shared Memory mechanism (SHM).
 *   This module allows sharing of the on-chip RAM by both Core and the Host.
 *   It also supports the following features:
 *   ● Four Core/Host communication windows for direct RAM access
 *   ● Eight Protection regions for each access window
 *   ● Host IRQ and SMI generation
 *   ● Port 80 debug support
 *   ● Configured by one logical device: SHM (LDN 0x0F)
 *
 * 3. Core Access to Host Modules (C2H).
 *   ● A interface to access module registers in host domain.
 *     It enables the Core to access the registers in host domain (i.e., Host
 *     Configuration, Serial Port, SHM, and MSWC), through HMIB.
 *
 * 4. Mobile System Wake-Up functions (MSWC).
 *   It detects and handles wake-up events from various sources in the Host
 *   modules and alerts the Core for better power consumption.
 *
 * INCLUDE FILES: soc_host.h
 *
 */

#include <assert.h>
#include <drivers/espi.h>
#include <drivers/gpio.h>
#include <drivers/clock_control.h>
#include <kernel.h>
#include <soc.h>
#include "espi_utils.h"
#include "soc_host.h"
#include "soc_espi.h"
#include "soc_miwu.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(host_sub_npcm4xx, LOG_LEVEL_ERR);

struct host_sub_npcm4xx_config {
	/* host module instances */
	struct mswc_reg *const inst_mswc;
	struct shm_reg *const inst_shm;
	struct c2h_reg *const inst_c2h;
	struct pmch_reg *const inst_pm_acpi;
	struct pmch_reg *const inst_pm_hcmd;
	struct pmch_reg *const inst_pmch3;
	struct pmch_reg *const inst_pmch4;
	/* clock configuration */
	const uint8_t clks_size;
	const struct npcm4xx_clk_cfg *clks_list;
	/* mapping table between host access signals and wake-up input */
	struct npcm4xx_wui host_acc_wui;
};

struct host_sub_npcm4xx_data {
	sys_slist_t *callbacks; /* pointer on the espi callback list */
	uint8_t plt_rst_asserted; /* current PLT_RST# status */
	uint8_t espi_rst_asserted; /* current ESPI_RST# status */
	const struct device *host_bus_dev; /* device for eSPI/LPC bus */
};

static const struct npcm4xx_clk_cfg host_dev_clk_cfg[] =
					NPCM4XX_DT_CLK_CFG_ITEMS_LIST(0);

struct host_sub_npcm4xx_config host_sub_cfg = {
	.inst_mswc = (struct mswc_reg *)DT_INST_REG_ADDR_BY_NAME(0, mswc),
	.inst_shm = (struct shm_reg *)DT_INST_REG_ADDR_BY_NAME(0, shm),
	.inst_c2h = (struct c2h_reg *)DT_INST_REG_ADDR_BY_NAME(0, c2h),
	.inst_pm_acpi = (struct pmch_reg *)DT_INST_REG_ADDR_BY_NAME(0, pm_acpi),
	.inst_pm_hcmd = (struct pmch_reg *)DT_INST_REG_ADDR_BY_NAME(0, pm_hcmd),
	.inst_pmch3 = (struct pmch_reg *)DT_INST_REG_ADDR_BY_NAME(0, pmch3),
	.inst_pmch4 = (struct pmch_reg *)DT_INST_REG_ADDR_BY_NAME(0, pmch4),
	.host_acc_wui = NPCM4XX_DT_WUI_ITEM_BY_NAME(0, host_acc_wui),
	.clks_size = ARRAY_SIZE(host_dev_clk_cfg),
	.clks_list = host_dev_clk_cfg,
};

struct host_sub_npcm4xx_data host_sub_data;

/* Application shouldn't touch these flags in KBC status register directly */
#define NPCM4XX_KBC_STS_MASK (BIT(NPCM4XX_HIKMST_IBF) | BIT(NPCM4XX_HIKMST_OBF) \
						| BIT(NPCM4XX_HIKMST_A2))

/* IO base address of EC Logical Device Configuration */
#define NPCM4XX_EC_CFG_IO_ADDR 0x2E

/* Timeout to wait for Core-to-Host transaction to be completed. */
#define NPCM4XX_C2H_TRANSACTION_TIMEOUT_US 200

/* Logical Device Number Assignments */
#define EC_CFG_LDN_SHM   0x0F
#define EC_CFG_LDN_ACPI  0x11 /* KCS/PM Channel 1 */
#define EC_CFG_LDN_HCMD  0x12 /* KCS/PM Channel 2 */
#define EC_CFG_LDN_PMCH3 0x17 /* KCS3/PM Channel 3 */
#define EC_CFG_LDN_PMCH4 0x1E /* KCS4/PM Channel 4 */

/* Index of EC (2E/2F or 4E/4F) Configuration Register */
#define EC_CFG_IDX_LDN             0x07
#define EC_CFG_IDX_CTRL            0x30
#define EC_CFG_IDX_DATA_IO_ADDR_H  0x60
#define EC_CFG_IDX_DATA_IO_ADDR_L  0x61
#define EC_CFG_IDX_CMD_IO_ADDR_H   0x62
#define EC_CFG_IDX_CMD_IO_ADDR_L   0x63

/* Index of Special Logical Device Configuration (Shared Memory Module) */
#define EC_CFG_IDX_SHM_CFG         0xF1
#define EC_CFG_IDX_SHM_WND1_ADDR_0 0xF4
#define EC_CFG_IDX_SHM_WND1_ADDR_1 0xF5
#define EC_CFG_IDX_SHM_WND1_ADDR_2 0xF6
#define EC_CFG_IDX_SHM_WND1_ADDR_3 0xF7
#define EC_CFG_IDX_SHM_WND2_ADDR_0 0xF8
#define EC_CFG_IDX_SHM_WND2_ADDR_1 0xF9
#define EC_CFG_IDX_SHM_WND2_ADDR_2 0xFA
#define EC_CFG_IDX_SHM_WND2_ADDR_3 0xFB

/* Host sub-device local inline functions */
static inline uint8_t host_shd_mem_wnd_size_sl(uint32_t size)
{
	/* The minimum supported shared memory region size is 8 bytes */
	if (size <= 8U) {
		size = 8U;
	}

	/* The maximum supported shared memory region size is 4K bytes */
	if (size >= 4096U) {
		size = 4096U;
	}

	/*
	 * If window size is not a power-of-two, it is rounded-up to the next
	 * power-of-two value, and return value corresponds to RWINx_SIZE field.
	 */
	return (32 - __builtin_clz(size - 1U)) & 0xff;
}

/* Host ACPI sub-device local functions */
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_IO)
static void host_acpi_process_input_data(uint8_t data)
{
	struct pmch_reg *const inst_acpi = host_sub_cfg.inst_pm_acpi;
	struct espi_event evt = {
		.evt_type = ESPI_BUS_PERIPHERAL_NOTIFICATION,
		.evt_details = ESPI_PERIPHERAL_HOST_IO,
		.evt_data = ESPI_PERIPHERAL_NODATA
	};

	LOG_DBG("%s: acpi data 0x%02x", __func__, data);

	/*
	 * The high byte contains information from the host, and the lower byte
	 * indicates if the host sent a command or data. 1 = Command.
	 */
	evt.evt_data = (data << NPCM4XX_ACPI_DATA_POS) |
		       (IS_BIT_SET(inst_acpi->HIPMST, NPCM4XX_HIPMST_CMD) <<
				       NPCM4XX_ACPI_TYPE_POS);
	espi_send_callbacks(host_sub_data.callbacks, host_sub_data.host_bus_dev,
							evt);
}

static void host_acpi_init(void)
{
	struct pmch_reg *const inst_acpi = host_sub_cfg.inst_pm_acpi;

	/* Use SMI/SCI postive polarity by default */
	inst_acpi->HIPMCTL &= ~BIT(NPCM4XX_HIPMCTL_SCIPOL);
	inst_acpi->HIPMIC &= ~BIT(NPCM4XX_HIPMIC_SMIPOL);

	/* Set SMIB/SCIB bits to make sure SMI#/SCI# are driven to high */
	inst_acpi->HIPMIC |= BIT(NPCM4XX_HIPMIC_SMIB) | BIT(NPCM4XX_HIPMIC_SCIB);

	/*
	 * Allow SMI#/SCI# generated from PM module.
	 * On eSPI bus, we suggest set VW value of SCI#/SMI# directly.
	 */
	inst_acpi->HIPMIE |= BIT(NPCM4XX_HIPMIE_SCIE);
	inst_acpi->HIPMIE |= BIT(NPCM4XX_HIPMIE_SMIE);

	/*
	 * Init ACPI PM channel (Host IO) with:
	 * 1. Enable Input-Buffer Full (IBF) core interrupt.
	 * 2. BIT 7 must be 1.
	 */
	inst_acpi->HIPMCTL |= BIT(7) | BIT(NPCM4XX_HIPMCTL_IBFIE);
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_PMCH3)
static void host_pmch3_process_input_data(uint8_t data)
{
	struct pmch_reg *const inst_pmch3 = host_sub_cfg.inst_pmch3;
	struct espi_event evt = {
		.evt_type = ESPI_BUS_PERIPHERAL_NOTIFICATION,
		.evt_details = ESPI_PERIPHERAL_HOST_IO,
		.evt_data = ESPI_PERIPHERAL_NODATA
	};

	LOG_DBG("%s: pmch3 data 0x%02x", __func__, data);

	/*
	 * The high byte contains information from the host, and the lower byte
	 * indicates if the host sent a command or data. 1 = Command.
	 */
	evt.evt_data = (data << NPCM4XX_ACPI_DATA_POS) |
		       (IS_BIT_SET(inst_pmch3->HIPMST, NPCM4XX_HIPMST_CMD) <<
				       NPCM4XX_ACPI_TYPE_POS);
	espi_send_callbacks(host_sub_data.callbacks, host_sub_data.host_bus_dev,
							evt);
}

static void host_pmch3_init(void)
{
	struct pmch_reg *const inst_pmch3 = host_sub_cfg.inst_pmch3;

	/* Use SMI/SCI postive polarity by default */
	inst_pmch3->HIPMCTL &= ~BIT(NPCM4XX_HIPMCTL_SCIPOL);
	inst_pmch3->HIPMIC &= ~BIT(NPCM4XX_HIPMIC_SMIPOL);

	/* Set SMIB/SCIB bits to make sure SMI#/SCI# are driven to high */
	inst_pmch3->HIPMIC |= BIT(NPCM4XX_HIPMIC_SMIB) | BIT(NPCM4XX_HIPMIC_SCIB);

	/*
	 * Allow SMI#/SCI# generated from PM module.
	 * On eSPI bus, we suggest set VW value of SCI#/SMI# directly.
	 */
	inst_pmch3->HIPMIE |= BIT(NPCM4XX_HIPMIE_SCIE);
	inst_pmch3->HIPMIE |= BIT(NPCM4XX_HIPMIE_SMIE);

	/*
	 * Init KCS3 PM channel 3 (Host IO) with:
	 * 1. Enable Input-Buffer Full (IBF) core interrupt.
	 * 2. BIT 7 must be 1.
	 */
	inst_pmch3->HIPMCTL |= BIT(7) | BIT(NPCM4XX_HIPMCTL_IBFIE);
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_PMCH4)
static void host_pmch4_process_input_data(uint8_t data)
{
	struct pmch_reg *const inst_pmch4 = host_sub_cfg.inst_pmch4;
	struct espi_event evt = {
		.evt_type = ESPI_BUS_PERIPHERAL_NOTIFICATION,
		.evt_details = ESPI_PERIPHERAL_HOST_IO,
		.evt_data = ESPI_PERIPHERAL_NODATA
	};

	LOG_DBG("%s: pmch4 data 0x%02x", __func__, data);

	/*
	 * The high byte contains information from the host, and the lower byte
	 * indicates if the host sent a command or data. 1 = Command.
	 */
	evt.evt_data = (data << NPCM4XX_ACPI_DATA_POS) |
		       (IS_BIT_SET(inst_pmch4->HIPMST, NPCM4XX_HIPMST_CMD) <<
				       NPCM4XX_ACPI_TYPE_POS);
	espi_send_callbacks(host_sub_data.callbacks, host_sub_data.host_bus_dev,
							evt);
}

static void host_pmch4_init(void)
{
	struct pmch_reg *const inst_pmch4 = host_sub_cfg.inst_pmch4;

	/* Use SMI/SCI postive polarity by default */
	inst_pmch4->HIPMCTL &= ~BIT(NPCM4XX_HIPMCTL_SCIPOL);
	inst_pmch4->HIPMIC &= ~BIT(NPCM4XX_HIPMIC_SMIPOL);

	/* Set SMIB/SCIB bits to make sure SMI#/SCI# are driven to high */
	inst_pmch4->HIPMIC |= BIT(NPCM4XX_HIPMIC_SMIB) | BIT(NPCM4XX_HIPMIC_SCIB);

	/*
	 * Allow SMI#/SCI# generated from PM module.
	 * On eSPI bus, we suggest set VW value of SCI#/SMI# directly.
	 */
	inst_pmch4->HIPMIE |= BIT(NPCM4XX_HIPMIE_SCIE);
	inst_pmch4->HIPMIE |= BIT(NPCM4XX_HIPMIE_SMIE);

	/*
	 * Init KCS4 PM channel 4 (Host IO) with:
	 * 1. Enable Input-Buffer Full (IBF) core interrupt.
	 * 2. BIT 7 must be 1.
	 */
	inst_pmch4->HIPMCTL |= BIT(7) | BIT(NPCM4XX_HIPMCTL_IBFIE);
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD)
/* Host command argument and memory map buffers */
static uint8_t	shm_host_cmd[CONFIG_ESPI_NPCM4XX_PERIPHERAL_HOST_CMD_PARAM_SIZE]
	__aligned(8);
/* Host command sub-device local functions */
static void host_hcmd_process_input_data(uint8_t data)
{
	struct espi_event evt = { ESPI_BUS_PERIPHERAL_NOTIFICATION,
		ESPI_PERIPHERAL_EC_HOST_CMD, ESPI_PERIPHERAL_NODATA
	};

	evt.evt_data = data;
	LOG_DBG("%s: host cmd data 0x%02x", __func__, evt.evt_data);
	espi_send_callbacks(host_sub_data.callbacks, host_sub_data.host_bus_dev,
							evt);
}

static void host_hcmd_init(void)
{
	struct pmch_reg *const inst_hcmd = host_sub_cfg.inst_pm_hcmd;
	struct shm_reg *const inst_shm = host_sub_cfg.inst_shm;
	uint32_t win_size = CONFIG_ESPI_NPCM4XX_PERIPHERAL_HOST_CMD_PARAM_SIZE;

	/* Don't stall SHM transactions */
	inst_shm->SHM_CTL &= ~0x40;
	/* Disable Window 1 protection */
	inst_shm->WIN1_WR_PROT = 0;
	inst_shm->WIN1_RD_PROT = 0;

	/* Configure Win1 size for ec host command. */
	SET_FIELD(inst_shm->WIN_SIZE, NPCM4XX_WIN_SIZE_RWIN1_SIZE_FIELD,
		host_shd_mem_wnd_size_sl(win_size));
	inst_shm->WIN_BASE1 = (uint32_t)shm_host_cmd;

	/*
	 * Clear processing flag before enabling host's interrupts in case
	 * it's set by the other command during sysjump.
	 */
	inst_hcmd->HIPMST &= ~BIT(NPCM4XX_HIPMST_F0);

	/*
	 * Init Host Command PM channel (Host IO) with:
	 * 1. Enable Input-Buffer Full (IBF) core interrupt.
	 * 2. BIT 7 must be 1.
	 */
	inst_hcmd->HIPMCTL |= BIT(7) | BIT(NPCM4XX_HIPMCTL_IBFIE);
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION)
/* Host command argument and memory map buffers */
static uint8_t shm_acpi_mmap[CONFIG_ESPI_NPCM4XX_PERIPHERAL_ACPI_SHD_MEM_SIZE]
	__aligned(8);
static void host_shared_mem_region_init(void)
{
	struct shm_reg *const inst_shm = host_sub_cfg.inst_shm;
	uint32_t win_size = CONFIG_ESPI_NPCM4XX_PERIPHERAL_ACPI_SHD_MEM_SIZE;

	/* Don't stall SHM transactions */
	inst_shm->SHM_CTL &= ~0x40;
	/* Disable Window 2 protection */
	inst_shm->WIN2_WR_PROT = 0;
	inst_shm->WIN2_RD_PROT = 0;

	/* Configure Win2 size for ACPI shared mem region. */
	SET_FIELD(inst_shm->WIN_SIZE, NPCM4XX_WIN_SIZE_RWIN2_SIZE_FIELD,
		host_shd_mem_wnd_size_sl(win_size));
	inst_shm->WIN_BASE2 = (uint32_t)shm_acpi_mmap;
	/* Enable write protect of Share memory window 2 */
	inst_shm->WIN2_WR_PROT = 0xFF;

	/*
	 * TODO: Initialize shm_acpi_mmap buffer for host command flags. We
	 * might use EACPI_GET_SHARED_MEMORY in espi_api_lpc_read_request()
	 * instead of setting host command flags here directly.
	 */
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_HOST_IO) || \
				defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) || \
				defined(CONFIG_ESPI_PERIPHERAL_PMCH3) || \
				defined(CONFIG_ESPI_PERIPHERAL_PMCH4)
/* Host pm (host io) sub-module isr function for all channels such as ACPI. */
static void host_pmch_ibf_isr(void *arg)
{
	ARG_UNUSED(arg);
	struct pmch_reg *const inst_acpi = host_sub_cfg.inst_pm_acpi;
	struct pmch_reg *const inst_hcmd = host_sub_cfg.inst_pm_hcmd;
	struct pmch_reg *const inst_pmch3 = host_sub_cfg.inst_pmch3;
	struct pmch_reg *const inst_pmch4 = host_sub_cfg.inst_pmch4;
	uint8_t in_data;

	/* Host put data on input buffer of ACPI channel */
	if (IS_BIT_SET(inst_acpi->HIPMST, NPCM4XX_HIPMST_IBF)) {
		/* Set processing flag before reading command byte */
		inst_acpi->HIPMST |= BIT(NPCM4XX_HIPMST_F0);
		/* Read out input data and clear IBF pending bit */
		in_data = inst_acpi->HIPMDI;
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_IO)
		host_acpi_process_input_data(in_data);
#endif
	}

	/* Host put data on input buffer of HOSTCMD channel */
	if (IS_BIT_SET(inst_hcmd->HIPMST, NPCM4XX_HIPMST_IBF)) {
		/* Set processing flag before reading command byte */
		inst_hcmd->HIPMST |= BIT(NPCM4XX_HIPMST_F0);
		/* Read out input data and clear IBF pending bit */
		in_data = inst_hcmd->HIPMDI;
#if defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD)
		host_hcmd_process_input_data(in_data);
#endif
	}

	/* Host put data on input buffer of KCS3/PMCH3 channel */
	if (IS_BIT_SET(inst_pmch3->HIPMST, NPCM4XX_HIPMST_IBF)) {
		/* Set processing flag before reading command byte */
		inst_pmch3->HIPMST |= BIT(NPCM4XX_HIPMST_F0);
		/* Read out input data and clear IBF pending bit */
		in_data = inst_pmch3->HIPMDI;
#if defined(CONFIG_ESPI_PERIPHERAL_PMCH3)
		host_pmch3_process_input_data(in_data);
#endif
	}

	/* Host put data on input buffer of KCS4/PMCH4 channel */
	if (IS_BIT_SET(inst_pmch4->HIPMST, NPCM4XX_HIPMST_IBF)) {
		/* Set processing flag before reading command byte */
		inst_pmch4->HIPMST |= BIT(NPCM4XX_HIPMST_F0);
		/* Read out input data and clear IBF pending bit */
		in_data = inst_pmch4->HIPMDI;
#if defined(CONFIG_ESPI_PERIPHERAL_PMCH4)
		host_pmch4_process_input_data(in_data);
#endif
	}
}
#endif

/* Host port80 sub-device local functions */
#if defined(CONFIG_ESPI_PERIPHERAL_DEBUG_PORT_80)
static void host_port80_isr(void *arg)
{
	ARG_UNUSED(arg);
	struct shm_reg *const inst_shm = host_sub_cfg.inst_shm;
	struct espi_event evt = { ESPI_BUS_PERIPHERAL_NOTIFICATION,
		(ESPI_PERIPHERAL_INDEX_0 << 16) | ESPI_PERIPHERAL_DEBUG_PORT80,
		ESPI_PERIPHERAL_NODATA
	};
	uint8_t status = inst_shm->DP80STS;

	LOG_DBG("%s: p80 status 0x%02X", __func__, status);

	/* Read out port80 data continuously if FIFO is not empty */
	while (IS_BIT_SET(inst_shm->DP80STS, NPCM4XX_DP80STS_FNE)) {
		LOG_DBG("p80: %04x", inst_shm->DP80BUF);
		evt.evt_data = inst_shm->DP80BUF;
		espi_send_callbacks(host_sub_data.callbacks,
				host_sub_data.host_bus_dev, evt);
	}

	/* If FIFO is overflow, show error msg */
	if (IS_BIT_SET(status, NPCM4XX_DP80STS_FOR)) {
		inst_shm->DP80STS |= BIT(NPCM4XX_DP80STS_FOR);
		LOG_ERR("Port80 FIFO Overflow!");
	}

	/* Clear all pending bit indicates that FIFO was written by host */
	inst_shm->DP80STS |= BIT(NPCM4XX_DP80STS_FWR);
}

static void host_port80_init(void)
{
	struct shm_reg *const inst_shm = host_sub_cfg.inst_shm;

	/*
	 * Init PORT80 which includes:
	 * Enables a Core interrupt on every Host write to the FIFO,
	 * SYNC mode (It must be 1 in eSPI mode), Read Auto Advance mode, and
	 * Port80 module itself.
	 */
	inst_shm->DP80CTL = BIT(NPCM4XX_DP80CTL_RAA)
			| BIT(NPCM4XX_DP80CTL_DP80EN) | BIT(NPCM4XX_DP80CTL_SYNCEN);
}
#endif

#if defined(CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE)
static void host_cus_opcode_enable_interrupts(void)
{
	/* Enable host PM channel (Host IO) sub-device interrupt */
	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_HOST_IO) ||
	    IS_ENABLED(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) ||
	    IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH3) ||
	    IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH4)) {
		irq_enable(DT_INST_IRQ_BY_NAME(0, pmch_ibf, irq));
	}

	/* Enable host Port80 sub-device interrupt installation */
	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_DEBUG_PORT_80)) {
		irq_enable(DT_INST_IRQ_BY_NAME(0, p80_fifo, irq));
	}

	/* Enable host interface interrupts if its interface is eSPI */
	if (IS_ENABLED(CONFIG_ESPI)) {
		npcm4xx_espi_enable_interrupts(host_sub_data.host_bus_dev);
	}
}

static void host_cus_opcode_disable_interrupts(void)
{
	/* Disable host PM channel (Host IO) sub-device interrupt */
	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_HOST_IO) ||
		IS_ENABLED(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) ||
		IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH3) ||
		IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH4)) {
		irq_disable(DT_INST_IRQ_BY_NAME(0, pmch_ibf, irq));
	}

	/* Disable host Port80 sub-device interrupt installation */
	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_DEBUG_PORT_80)) {
		irq_disable(DT_INST_IRQ_BY_NAME(0, p80_fifo, irq));
	}

	/* Disable host interface interrupts if its interface is eSPI */
	if (IS_ENABLED(CONFIG_ESPI)) {
		npcm4xx_espi_disable_interrupts(host_sub_data.host_bus_dev);
	}
}
#endif /* CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE */

#if defined(CONFIG_ESPI_PERIPHERAL_UART)
/* Host UART sub-device local functions */
void host_uart_init(void)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;

	/* Make sure unlock host access of serial port */
	inst_c2h->LKSIOHA &= ~BIT(NPCM4XX_LKSIOHA_LKSPHA);
	/* Clear 'Host lock violation occurred' bit of serial port initially */
	inst_c2h->SIOLV |= BIT(NPCM4XX_SIOLV_SPLV);
}
#endif

/* host core-to-host interface local functions */
static void host_c2h_wait_write_done(void)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;
	uint32_t elapsed_cycles;
	uint32_t start_cycles = k_cycle_get_32();
	uint32_t max_wait_cycles =
			k_us_to_cyc_ceil32(NPCM4XX_C2H_TRANSACTION_TIMEOUT_US);

	while (IS_BIT_SET(inst_c2h->SIBCTRL, NPCM4XX_SIBCTRL_CSWR)) {
		elapsed_cycles = k_cycle_get_32() - start_cycles;
		if (elapsed_cycles > max_wait_cycles) {
			LOG_ERR("c2h write transaction expired!");
			break;
		}
	}
}

static void host_c2h_wait_read_done(void)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;
	uint32_t elapsed_cycles;
	uint32_t start_cycles = k_cycle_get_32();
	uint32_t max_wait_cycles =
			k_us_to_cyc_ceil32(NPCM4XX_C2H_TRANSACTION_TIMEOUT_US);

	while (IS_BIT_SET(inst_c2h->SIBCTRL, NPCM4XX_SIBCTRL_CSRD)) {
		elapsed_cycles = k_cycle_get_32() - start_cycles;
		if (elapsed_cycles > max_wait_cycles) {
			LOG_ERR("c2h read transaction expired!");
			break;
		}
	}
}

void host_c2h_write_io_cfg_reg(uint8_t reg_index, uint8_t reg_data)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;

	/* Disable interrupts */
	int key = irq_lock();

	/* Lock host access EC configuration registers (0x4E/0x4F) */
	inst_c2h->LKSIOHA |= BIT(NPCM4XX_LKSIOHA_LKCFG);
	/* Enable Core-to-Host access CFG module */
	inst_c2h->CRSMAE |= BIT(NPCM4XX_CRSMAE_CFGAE);

	/* Verify core-to-host modules is not in progress */
	host_c2h_wait_read_done();
	host_c2h_wait_write_done();

	/*
	 * Specifying the in-direct IO address which A0 = 0 indicates the index
	 * register is accessed. Then write index address directly and it starts
	 * a write transaction to host sub-module on LPC/eSPI bus.
	 */
	inst_c2h->IHIOA = 0;
	inst_c2h->IHD = reg_index;
	host_c2h_wait_write_done();

	/*
	 * Specifying the in-direct IO address which A0 = 1 indicates the data
	 * register is accessed. Then write data directly and it starts a write
	 * transaction to host sub-module on LPC/eSPI bus.
	 */
	inst_c2h->IHIOA = 1;
	inst_c2h->IHD = reg_data;
	host_c2h_wait_write_done();

	/* Disable Core-to-Host access CFG module */
	inst_c2h->CRSMAE &= ~BIT(NPCM4XX_CRSMAE_CFGAE);
	/* Unlock host access EC configuration registers (0x4E/0x4F) */
	inst_c2h->LKSIOHA &= ~BIT(NPCM4XX_LKSIOHA_LKCFG);

	/* Enable interrupts */
	irq_unlock(key);
}

uint8_t host_c2h_read_io_cfg_reg(uint8_t reg_index)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;
	uint8_t data_val;

	/* Disable interrupts */
	int key = irq_lock();

	/* Lock host access EC configuration registers (0x4E/0x4F) */
	inst_c2h->LKSIOHA |= BIT(NPCM4XX_LKSIOHA_LKCFG);
	/* Enable Core-to-Host access CFG module */
	inst_c2h->CRSMAE |= BIT(NPCM4XX_CRSMAE_CFGAE);

	/* Verify core-to-host modules is not in progress */
	host_c2h_wait_read_done();
	host_c2h_wait_write_done();

	/*
	 * Specifying the in-direct IO address which A0 = 0 indicates the index
	 * register is accessed. Then write index address directly and it starts
	 * a write transaction to host sub-module on LPC/eSPI bus.
	 */
	inst_c2h->IHIOA = 0;
	inst_c2h->IHD = reg_index;
	host_c2h_wait_write_done();

	/*
	 * Specifying the in-direct IO address which A0 = 1 indicates the data
	 * register is accessed. Then write CSRD bit in SIBCTRL to issue a read
	 * transaction to host sub-module on LPC/eSPI bus. Once it was done,
	 * read data out from IHD.
	 */
	inst_c2h->IHIOA = 1;
	inst_c2h->SIBCTRL |= BIT(NPCM4XX_SIBCTRL_CSRD);
	host_c2h_wait_read_done();
	data_val = inst_c2h->IHD;

	/* Disable Core-to-Host access CFG module */
	inst_c2h->CRSMAE &= ~BIT(NPCM4XX_CRSMAE_CFGAE);
	/* Unlock host access EC configuration registers (0x4E/0x4F) */
	inst_c2h->LKSIOHA &= ~BIT(NPCM4XX_LKSIOHA_LKCFG);

	/* Enable interrupts */
	irq_unlock(key);

	return data_val;
}

/* Platform specific host sub modules functions */
int npcm4xx_host_periph_read_request(enum lpc_peripheral_opcode op,
								uint32_t *data)
{
	if (op >= EACPI_START_OPCODE && op <= EACPI_MAX_OPCODE) {
		struct pmch_reg *const inst_acpi = host_sub_cfg.inst_pm_acpi;

		/* Make sure pm channel for apci is on */
		if (!IS_BIT_SET(inst_acpi->HIPMCTL, NPCM4XX_HIPMCTL_IBFIE)) {
			return -ENOTSUP;
		}

		switch (op) {
		case EACPI_OBF_HAS_CHAR:
			/* EC has written data back to host. OBF is
			 * automatically cleared after host reads
			 * the data
			 */
			*data = IS_BIT_SET(inst_acpi->HIPMST, NPCM4XX_HIPMST_OBF);
			break;
		case EACPI_IBF_HAS_CHAR:
			*data = IS_BIT_SET(inst_acpi->HIPMST, NPCM4XX_HIPMST_IBF);
			break;
		case EACPI_READ_STS:
			*data = inst_acpi->HIPMST;
			break;
#if defined(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION)
		case EACPI_GET_SHARED_MEMORY:
			*data = (uint32_t)shm_acpi_mmap;
			break;
#endif /* CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION */
		default:
			return -EINVAL;
		}
	}
#if defined(CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE)
	else if (op >= ECUSTOM_START_OPCODE && op <= ECUSTOM_MAX_OPCODE) {
		/* Other customized op codes */
		switch (op) {
		case ECUSTOM_HOST_CMD_GET_PARAM_MEMORY:
			*data = (uint32_t)shm_host_cmd;
			break;
		default:
			return -EINVAL;
		}
	}
#endif /* CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE */
	else {
		return -ENOTSUP;
	}

	return 0;
}

int npcm4xx_host_periph_write_request(enum lpc_peripheral_opcode op,
							const uint32_t *data)
{
	volatile uint32_t __attribute__((unused)) dummy;

	if (op >= EACPI_START_OPCODE && op <= EACPI_MAX_OPCODE) {
		struct pmch_reg *const inst_acpi = host_sub_cfg.inst_pm_acpi;

		/* Make sure pm channel for apci is on */
		if (!IS_BIT_SET(inst_acpi->HIPMCTL, NPCM4XX_HIPMCTL_IBFIE)) {
			return -ENOTSUP;
		}

		switch (op) {
		case EACPI_WRITE_CHAR:
			inst_acpi->HIPMDO = (*data & 0xff);
			break;
		case EACPI_WRITE_STS:
			inst_acpi->HIPMST = (*data & 0xff);
			break;
		default:
			return -EINVAL;
		}
	}
#if defined(CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE)
	else if (op >= ECUSTOM_START_OPCODE && op <= ECUSTOM_MAX_OPCODE) {
		/* Other customized op codes */
		struct pmch_reg *const inst_hcmd = host_sub_cfg.inst_pm_hcmd;

		switch (op) {
		case ECUSTOM_HOST_SUBS_INTERRUPT_EN:
			if (*data != 0) {
				host_cus_opcode_enable_interrupts();
			} else {
				host_cus_opcode_disable_interrupts();
			}
			break;
		case ECUSTOM_HOST_CMD_SEND_RESULT:
			/*
			 * Write result to the data byte.  This sets the TOH
			 * status bit.
			 */
			inst_hcmd->HIPMDO = (*data & 0xff);
			/* Clear processing flag */
			inst_hcmd->HIPMST &= ~BIT(NPCM4XX_HIPMST_F0);
			break;
		default:
			return -EINVAL;
		}
	}
#endif /* CONFIG_ESPI_PERIPHERAL_CUSTOM_OPCODE */
	else {
		return -ENOTSUP;
	}

	return 0;
}

void npcm4xx_host_init_subs_host_domain(void)
{
	struct c2h_reg *const inst_c2h = host_sub_cfg.inst_c2h;

	/* Enable Core-to-Host access module */
	inst_c2h->SIBCTRL |= BIT(NPCM4XX_SIBCTRL_CSAE);

	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_HOST_IO)) {
		/*
		 * Select ACPI bank which LDN are 0x11 (PM Channel 1) and enable
		 * module by setting bit 0 in its Control (index is 0x30) reg.
		 */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_LDN, EC_CFG_LDN_ACPI);
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CTRL, 0x01);
	}

	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) ||
	    IS_ENABLED(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION)) {
		/* Select 'Host Command' bank which LDN are 0x12 (PM chan 2) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_LDN, EC_CFG_LDN_HCMD);
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_CMD_DATA_PORT_NUM)
		/* Configure IO address of Data port (default: 0x200) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_DATA_IO_ADDR_H,
		 (CONFIG_ESPI_PERIPHERAL_HOST_CMD_DATA_PORT_NUM >> 8) & 0xff);
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_DATA_IO_ADDR_L,
		 CONFIG_ESPI_PERIPHERAL_HOST_CMD_DATA_PORT_NUM & 0xff);
		/* Configure IO address of CMD port (default: 0x204) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CMD_IO_ADDR_H,
		 ((CONFIG_ESPI_PERIPHERAL_HOST_CMD_DATA_PORT_NUM + 4) >> 8)
		 & 0xff);
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CMD_IO_ADDR_L,
		 (CONFIG_ESPI_PERIPHERAL_HOST_CMD_DATA_PORT_NUM + 4) & 0xff);
#endif
		/* Enable 'Host Command' io port (PM Channel 2) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CTRL, 0x01);

		/* Select 'Shared Memory' bank which LDN are 0x0F */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_LDN, EC_CFG_LDN_SHM);
		/* WIN 1 & 2 mapping to IO space */
		host_c2h_write_io_cfg_reg(0xF1,
				host_c2h_read_io_cfg_reg(0xF1) | 0x30);
		/* WIN1 as Host Command on the IO address (default: 0x0800) */
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_CMD_PARAM_PORT_NUM)
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_SHM_WND1_ADDR_1,
		(CONFIG_ESPI_PERIPHERAL_HOST_CMD_PARAM_PORT_NUM >> 8) & 0xff);
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_SHM_WND1_ADDR_0,
		CONFIG_ESPI_PERIPHERAL_HOST_CMD_PARAM_PORT_NUM & 0xff);
#endif
		/* Set WIN2 as MEMMAP on the configured IO address */
#if defined(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION_PORT_NUM)
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_SHM_WND2_ADDR_1,
		(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION_PORT_NUM >> 8) & 0xff);
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_SHM_WND2_ADDR_0,
		CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION_PORT_NUM & 0xff);
#endif
	/* Enable SHM direct memory access */
	host_c2h_write_io_cfg_reg(EC_CFG_IDX_CTRL, 0x01);
	}

	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH3)) {
		/* Select 'Host Command' bank which LDN are 0x17 (KCS3/PM chan 3) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_LDN, EC_CFG_LDN_PMCH3);
		/* Enable 'Host Command' io port (KCS3/PM Channel 3) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CTRL, 0x01);
	}

	if (IS_ENABLED(CONFIG_ESPI_PERIPHERAL_PMCH4)) {
		/* Select 'Host Command' bank which LDN are 0x1E (KCS4/PM chan 4) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_LDN, EC_CFG_LDN_PMCH4);
		/* Enable 'Host Command' io port (KCS4/PM Channel 4) */
		host_c2h_write_io_cfg_reg(EC_CFG_IDX_CTRL, 0x01);
	}

	LOG_DBG("Hos sub-modules configurations are done!");
}

void npcm4xx_host_enable_access_interrupt(void)
{
	npcm4xx_miwu_irq_get_and_clear_pending(&host_sub_cfg.host_acc_wui);

	npcm4xx_miwu_irq_enable(&host_sub_cfg.host_acc_wui);
}

void npcm4xx_host_disable_access_interrupt(void)
{
	npcm4xx_miwu_irq_disable(&host_sub_cfg.host_acc_wui);
}

int npcm4xx_host_init_subs_core_domain(const struct device *host_bus_dev,
							sys_slist_t *callbacks)
{
	struct shm_reg *const inst_shm = host_sub_cfg.inst_shm;
	const struct device *const clk_dev =
					device_get_binding(NPCM4XX_CLK_CTRL_NAME);
	int i;
	uint8_t shm_sts;

	host_sub_data.callbacks = callbacks;
	host_sub_data.host_bus_dev = host_bus_dev;

	/* Turn on all host necessary sub-module clocks first */
	for (i = 0; i < host_sub_cfg.clks_size; i++) {
		int ret;

		ret = clock_control_on(clk_dev, (clock_control_subsys_t *)
				&host_sub_cfg.clks_list[i]);
		if (ret < 0)
			return ret;
	}

	/*
	 * Set HOSTWAIT bit and avoid the other settings, then host can freely
	 * communicate with slave (EC).
	 */
	inst_shm->SMC_CTL &= BIT(NPCM4XX_SMC_CTL_HOSTWAIT);
	/* Clear shared memory status */
	shm_sts = inst_shm->SMC_STS;
	inst_shm->SMC_STS = shm_sts;

	/* host sub-module initialization in core domain */
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_IO)
	host_acpi_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD)
	host_hcmd_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_PMCH3)
	host_pmch3_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_PMCH4)
	host_pmch4_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_ACPI_SHM_REGION)
	host_shared_mem_region_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_DEBUG_PORT_80)
	host_port80_init();
#endif
#if defined(CONFIG_ESPI_PERIPHERAL_UART)
	host_uart_init();
#endif

	/* Host PM channel (Host IO) sub-device interrupt installation */
#if defined(CONFIG_ESPI_PERIPHERAL_HOST_IO) || \
				defined(CONFIG_ESPI_PERIPHERAL_EC_HOST_CMD) || \
				defined(CONFIG_ESPI_PERIPHERAL_PMCH3) || \
				defined(CONFIG_ESPI_PERIPHERAL_PMCH4)
	IRQ_CONNECT(DT_INST_IRQ_BY_NAME(0, pmch_ibf, irq),
		    DT_INST_IRQ_BY_NAME(0, pmch_ibf, priority),
		    host_pmch_ibf_isr,
		    NULL, 0);
	irq_enable(DT_INST_IRQ_BY_NAME(0, pmch_ibf, irq));
#endif

	/* Host Port80 sub-device interrupt installation */
#if defined(CONFIG_ESPI_PERIPHERAL_DEBUG_PORT_80)
	IRQ_CONNECT(DT_INST_IRQ_BY_NAME(0, p80_fifo, irq),
		    DT_INST_IRQ_BY_NAME(0, p80_fifo, priority),
		    host_port80_isr,
		    NULL, 0);
	irq_enable(DT_INST_IRQ_BY_NAME(0, p80_fifo, irq));
#endif

	if (IS_ENABLED(CONFIG_PM)) {
		/*
		 * Configure the host access wake-up event triggered from a host
		 * transaction on eSPI/LPC bus. Do not enable it here. Or plenty
		 * of interrupts will jam the system in S0.
		 */
		npcm4xx_miwu_interrupt_configure(&host_sub_cfg.host_acc_wui,
				NPCM4XX_MIWU_MODE_EDGE, NPCM4XX_MIWU_TRIG_HIGH);
	}

	return 0;
}
