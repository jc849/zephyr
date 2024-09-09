/*
 * Copyright (c) 2024 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nuvoton_npcm4xx_snoop

#include <device.h>
#include <kernel.h>
#include <soc.h>
#include <errno.h>
#include <string.h>
#include <logging/log.h>
#include <drivers/clock_control.h>
#include <drivers/misc/npcm/snoop_npcm.h>

LOG_MODULE_REGISTER(snoop_npcm, CONFIG_LOG_DEFAULT_LEVEL);

#define DP80BUF1 0x3c
#define DP80BUF	0x40
#define DP80BUF_BYTE_LEN_MSK 0x7
#define DP80BUF_BYTE_OFFSET 12
#define DP80BUF_RNG BIT(11)
#define DP80BUF_OFFS_MSK 0x7
#define DP80BUF_OFFS_OFFSET 8
#define DP80BUF_DP80BUF_MSK 0xFF
#define DP80BUF_DP80BUF_OFFST 0

#define DP80STS	0x42
#define DP80STS_FOR BIT(7)
#define DP80STS_FNE BIT(6)
#define DP80STS_FWR BIT(5)
#define DP80STS_FLV BIT(4)

#define DP80IE 0x43
#define DP80IE_FORIEN BIT(7)
#define DP80IE_FNEIEN BIT(6)
#define DP80IE_FWRIEN BIT(5)
#define DP80IE_FLVIEN BIT(4)
#define DP80IE_ILV_MSK 0x0F
#define DP80IE_ILV_OFFST 0

#define DP80CTL 0x44
#define DP80CTL_RFIFO BIT(4)
#define DP80CTL_RAA BIT(3)
#define DP80CTL_ADV BIT(2)
#define DP80CTL_SYNCEN BIT(1)
#define DP80CTL_DP80EN BIT(0)

#define SNOOP_FIFO_DEPTH	16

static uint8_t snoop_ptr[SNOOP_FIFO_DEPTH];

struct snoop_npcm_data {
	snoop_npcm_rx_callback_t *rx_cb;
};

struct snoop_npcm_config {
	uintptr_t base;
};

int snoop_npcm_register_rx_callback(const struct device *dev, snoop_npcm_rx_callback_t cb)
{
	struct snoop_npcm_data *data = (struct snoop_npcm_data *)dev->data;

	if (data->rx_cb) {
		LOG_ERR("Snoop RX callback is registered\n");
		return -EBUSY;
	}

	data->rx_cb = cb;

	return 0;
}

static void snoop_npcm_isr(const struct device *dev)
{
	uint8_t sts, len;
	struct snoop_npcm_config *cfg = (struct snoop_npcm_config *)dev->config;
	struct snoop_npcm_data *data = (struct snoop_npcm_data *)dev->data;

	sts = sys_read8(cfg->base + DP80STS);

    if (sts & DP80STS_FOR) {
        LOG_WRN("P80 FIFO was overrun\r\n");
        sys_write8(DP80STS_FOR, cfg->base + DP80STS);
    }

    if (sts & DP80STS_FWR) {
        LOG_DBG("P80 FIFO was written\r\n");
        sys_write8(DP80STS_FWR, cfg->base + DP80STS);
    }

	len = (sys_read16(cfg->base + DP80BUF) >> DP80BUF_BYTE_OFFSET) & DP80BUF_BYTE_LEN_MSK;

    switch (len) {
        case 0: /* 1-byte */
            snoop_ptr[0] = sys_read8(cfg->base + DP80BUF);
			len = 1;
            break;
        case 1: /* 2-bytes */
			snoop_ptr[0] = sys_read32(cfg->base + DP80BUF1) & 0xFF;
			snoop_ptr[1] = (sys_read32(cfg->base + DP80BUF1) >> 8) & 0xFF;
			len = 2;
            break;
        case 2: /* 4-bytes */
			snoop_ptr[0] = sys_read32(cfg->base + DP80BUF1) & 0xFF;
			snoop_ptr[1] = (sys_read32(cfg->base + DP80BUF1) >> 8) & 0xFF;
			snoop_ptr[2] = (sys_read32(cfg->base + DP80BUF1) >> 16) & 0xFF;
			snoop_ptr[3] = (sys_read32(cfg->base + DP80BUF1) >> 24) & 0xFF;
			len = 4;
            break;
    }

	if (data->rx_cb)
		data->rx_cb(snoop_ptr, len);
}

static void snoop_npcm_enable(const struct device *dev)
{
	struct snoop_npcm_config *cfg = (struct snoop_npcm_config *)dev->config;

	sys_write8(DP80CTL_RFIFO, cfg->base + DP80CTL);
	sys_write8(DP80CTL_DP80EN | DP80CTL_RAA, cfg->base + DP80CTL);
	sys_write8(DP80IE_FWRIEN | DP80IE_FNEIEN, cfg->base + DP80IE);
}

static int snoop_npcm_init(const struct device *dev)
{
	IRQ_CONNECT(DT_INST_IRQN(0),
		    DT_INST_IRQ(0, priority),
		    snoop_npcm_isr,
		    DEVICE_DT_INST_GET(0), 0);

	irq_enable(DT_INST_IRQN(0));

	snoop_npcm_enable(dev);

	return 0;
}

static struct snoop_npcm_data snoop_npcm_data;

static struct snoop_npcm_config snoop_npcm_config = {
	.base = DT_REG_ADDR_BY_NAME(DT_NODELABEL(shm), shm),
};

DEVICE_DT_INST_DEFINE(0, snoop_npcm_init, NULL,
		      &snoop_npcm_data, &snoop_npcm_config,
		      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		      NULL);
