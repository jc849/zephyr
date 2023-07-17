/*
 * Copyright (c) 2021 Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT nuvoton_npcm4xx_kcs

#include <device.h>
#include <kernel.h>
#include <soc.h>
#include <errno.h>
#include <string.h>
#include <logging/log.h>
#include <drivers/clock_control.h>
#include <drivers/ipmi/kcs_nuvoton.h>
#include <soc.h>
#include "soc_espi.h"
LOG_MODULE_REGISTER(kcs_npcm4xx, CONFIG_LOG_DEFAULT_LEVEL);

/* LPC registers */
#define STR     0x00
#define ODR     0x02
#define IDR     0x04
#define HIPMCTL 0x0C
#define HIPMIC  0x0E
#define HIPMIE  0x10


#define EC_CFG_LDN_KCS1  0x11 /* KCS/PM Channel 1 */
#define EC_CFG_LDN_KCS2  0x12 /* KCS/PM Channel 2 */
#define EC_CFG_LDN_KCS3  0x17 /* KCS3/PM Channel 3 */
#define EC_CFG_LDN_KCS4  0x1E /* KCS4/PM Channel 4 */

/* Index of EC (2E/2F or 4E/4F) Configuration Register */
#define EC_CFG_IDX_LDN             0x07
#define EC_CFG_IDX_CTRL            0x30
#define EC_CFG_IDX_DATA_IO_ADDR_H  0x60
#define EC_CFG_IDX_DATA_IO_ADDR_L  0x61
#define EC_CFG_IDX_CMD_IO_ADDR_H   0x62
#define EC_CFG_IDX_CMD_IO_ADDR_L   0x63

/* misc. constant */
#define KCS_DUMMY_ZERO  0x0
#define KCS_BUF_SIZE    0x100

enum kcs_npcm4xx_chan {
	KCS_CH1 = 1,
	KCS_CH2,
	KCS_CH3,
	KCS_CH4,
};

struct kcs_npcm4xx_data {
	uint32_t idr;
	uint32_t odr;
	uint32_t str;

	uint8_t ibuf[KCS_BUF_SIZE];
	uint32_t ibuf_idx;
	uint32_t ibuf_avail;

	uint8_t obuf[KCS_BUF_SIZE];
	uint32_t obuf_idx;
	uint32_t obuf_data_sz;

	uint32_t phase;
	uint32_t error;
};

struct kcs_npcm4xx_config {
	uintptr_t base;
	uint32_t chan;
	uint32_t addr;
	void (*irq_config_func)(const struct device *dev);
};

/* IPMI 2.0 - Table 9-1, KCS Interface Status Register Bits */
#define KCS_STR_STATE_MASK      GENMASK(7, 6)
#define KCS_STR_STATE_SHIFT     6
#define KCS_STR_CMD_DAT         BIT(3)
#define KCS_STR_SMS_ATN         BIT(2)
#define KCS_STR_IBF             BIT(1)
#define KCS_STR_OBF             BIT(0)

/* IPMI 2.0 - Table 9-2, KCS Interface State Bits */
enum kcs_state {
	KCS_STATE_IDLE,
	KCS_STATE_READ,
	KCS_STATE_WRITE,
	KCS_STATE_ERROR,
	KCS_STATE_NUM
};

/* IPMI 2.0 - Table 9-3, KCS Interface Control Codes */
enum kcs_cmd_code {
	KCS_CMD_GET_STATUS_ABORT        = 0x60,
	KCS_CMD_WRITE_START             = 0x61,
	KCS_CMD_WRITE_END               = 0x62,
	KCS_CMD_READ_BYTE               = 0x68,
	KCS_CMD_NUM
};

/* IPMI 2.0 - Table 9-4, KCS Interface Status Codes */
enum kcs_error_code {
	KCS_NO_ERROR                    = 0x00,
	KCS_ABORTED_BY_COMMAND          = 0x01,
	KCS_ILLEGAL_CONTROL_CODE        = 0x02,
	KCS_LENGTH_ERROR                = 0x06,
	KCS_UNSPECIFIED_ERROR           = 0xff
};

/* IPMI 2.0 - Figure 9. KCS Phase in Transfer Flow Chart */
enum kcs_phase {
	KCS_PHASE_IDLE,

	KCS_PHASE_WRITE_START,
	KCS_PHASE_WRITE_DATA,
	KCS_PHASE_WRITE_END_CMD,
	KCS_PHASE_WRITE_DONE,

	KCS_PHASE_WAIT_READ,
	KCS_PHASE_READ,

	KCS_PHASE_ABORT_ERROR1,
	KCS_PHASE_ABORT_ERROR2,
	KCS_PHASE_ERROR,

	KCS_PHASE_NUM
};

static void kcs_set_state(const struct device *dev, enum kcs_state stat)
{
	uint32_t reg;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;
	struct kcs_npcm4xx_config *cfg = (struct kcs_npcm4xx_config *)dev->config;

	reg = sys_read8(cfg->base + kcs->str);
	reg &= ~(KCS_STR_STATE_MASK);
	reg |= (stat << KCS_STR_STATE_SHIFT) & KCS_STR_STATE_MASK;
	sys_write8(reg, cfg->base + kcs->str);
}

static void kcs_write_data(const struct device *dev, uint8_t data)
{
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;
	struct kcs_npcm4xx_config *cfg = (struct kcs_npcm4xx_config *)dev->config;

	sys_write8(data, cfg->base + kcs->odr);
}

static uint8_t kcs_read_data(const struct device *dev)
{
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;
	struct kcs_npcm4xx_config *cfg = (struct kcs_npcm4xx_config *)dev->config;

	return sys_read8(cfg->base + kcs->idr);
}

static void kcs_force_abort(const struct device *dev)
{
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	kcs_set_state(dev, KCS_STATE_ERROR);
	kcs_read_data(dev);
	kcs_write_data(dev, KCS_DUMMY_ZERO);
	kcs->ibuf_avail = 0;
	kcs->ibuf_idx = 0;
	kcs->phase = KCS_PHASE_ERROR;
}

static void kcs_handle_cmd(const struct device *dev)
{
	uint8_t cmd;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	kcs_set_state(dev, KCS_STATE_WRITE);
	kcs_write_data(dev, KCS_DUMMY_ZERO);

	cmd = kcs_read_data(dev);
	printk("cmd: 0x%x\r\n", cmd);
	switch (cmd) {
	case KCS_CMD_WRITE_START:
		kcs->phase = KCS_PHASE_WRITE_START;
		kcs->error = KCS_NO_ERROR;
		break;

	case KCS_CMD_WRITE_END:
		if (kcs->phase != KCS_PHASE_WRITE_DATA) {
			kcs_force_abort(dev);
			break;
		}
		kcs->phase = KCS_PHASE_WRITE_END_CMD;
		break;

	case KCS_CMD_GET_STATUS_ABORT:
		if (kcs->error == KCS_NO_ERROR) {
			kcs->error = KCS_ABORTED_BY_COMMAND;
		}

		kcs->phase = KCS_PHASE_ABORT_ERROR1;
		break;

	default:
		kcs_force_abort(dev);
		kcs->error = KCS_ILLEGAL_CONTROL_CODE;
		break;
	}
}

static void kcs_handle_data(const struct device *dev)
{
	uint8_t data;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	switch (kcs->phase) {
	case KCS_PHASE_WRITE_START:
		kcs->phase = KCS_PHASE_WRITE_DATA;
	/* fall through */
	case KCS_PHASE_WRITE_DATA:
		if (kcs->ibuf_idx < KCS_BUF_SIZE) {
			kcs_set_state(dev, KCS_STATE_WRITE);
			kcs_write_data(dev, KCS_DUMMY_ZERO);
			kcs->ibuf[kcs->ibuf_idx] = kcs_read_data(dev);
			printk("data: 0x%x\r\n", kcs->ibuf[kcs->ibuf_idx]);
			kcs->ibuf_idx++;
		} else {
			kcs_force_abort(dev);
			kcs->error = KCS_LENGTH_ERROR;
		}
		break;

	case KCS_PHASE_WRITE_END_CMD:
		if (kcs->ibuf_idx < KCS_BUF_SIZE) {
			kcs_set_state(dev, KCS_STATE_READ);
			kcs->ibuf[kcs->ibuf_idx] = kcs_read_data(dev);
			kcs->ibuf_idx++;
			kcs->ibuf_avail = 1;
			kcs->phase = KCS_PHASE_WRITE_DONE;
		} else {
			kcs_force_abort(dev);
			kcs->error = KCS_LENGTH_ERROR;
		}
		break;

	case KCS_PHASE_READ:
		if (kcs->obuf_idx == kcs->obuf_data_sz) {
			kcs_set_state(dev, KCS_STATE_IDLE);
		}

		data = kcs_read_data(dev);
		if (data != KCS_CMD_READ_BYTE) {
			kcs_set_state(dev, KCS_STATE_ERROR);
			kcs_write_data(dev, KCS_DUMMY_ZERO);
			break;
		}

		if (kcs->obuf_idx == kcs->obuf_data_sz) {
			kcs_write_data(dev, KCS_DUMMY_ZERO);
			kcs->phase = KCS_PHASE_IDLE;
			break;
		}

		kcs_write_data(dev, kcs->obuf[kcs->obuf_idx]);
		kcs->obuf_idx++;
		break;

	case KCS_PHASE_ABORT_ERROR1:
		kcs_set_state(dev, KCS_STATE_READ);
		kcs_read_data(dev);
		kcs_write_data(dev, kcs->error);
		kcs->phase = KCS_PHASE_ABORT_ERROR2;
		break;

	case KCS_PHASE_ABORT_ERROR2:
		kcs_set_state(dev, KCS_STATE_IDLE);
		kcs_read_data(dev);
		kcs_write_data(dev, KCS_DUMMY_ZERO);
		kcs->phase = KCS_PHASE_IDLE;
		break;

	default:
		kcs_force_abort(dev);
		break;
	}

}

void kcs_npcm4xx_isr(const struct device *dev)
{
	uint32_t stat;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;
	struct kcs_npcm4xx_config *cfg = (struct kcs_npcm4xx_config *)dev->config;

	stat = sys_read8(cfg->base + kcs->str);
	printk("stat:0x%x\r\n", stat);
	if (stat & KCS_STR_IBF) {
		if (stat & KCS_STR_CMD_DAT) {
			kcs_handle_cmd(dev);
		} else {
			kcs_handle_data(dev);
		}
	}
}

static int kcs_aspeed_config_ioregs(const struct device *dev)
{
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	kcs->idr = IDR;
	kcs->odr = ODR;
	kcs->str = STR;

	return 0;
}

int kcs_nuvoton_read(const struct device *dev,
		    uint8_t *buf, uint32_t buf_sz)
{
	int ret;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	if (kcs == NULL || buf == NULL) {
		return -EINVAL;
	}

	if (buf_sz < kcs->ibuf_idx) {
		return -ENOSPC;
	}

	if (!kcs->ibuf_avail) {
		return -ENODATA;
	}

	if (kcs->phase != KCS_PHASE_WRITE_DONE) {
		kcs_force_abort(dev);
		return -EPERM;
	}

	memcpy(buf, kcs->ibuf, kcs->ibuf_idx);
	ret = kcs->ibuf_idx;

	kcs->phase = KCS_PHASE_WAIT_READ;
	kcs->ibuf_avail = 0;
	kcs->ibuf_idx = 0;

	return ret;
}

int kcs_nuvoton_write(const struct device *dev,
		     uint8_t *buf, uint32_t buf_sz)
{
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;

	/* a minimum response size is 3: netfn + cmd + cmplt_code */
	if (buf_sz < 3 || buf_sz > KCS_BUF_SIZE) {
		return -EINVAL;
	}

	if (kcs->phase != KCS_PHASE_WAIT_READ) {
		return -EPERM;
	}

	kcs->phase = KCS_PHASE_READ;
	kcs->obuf_idx = 1;
	kcs->obuf_data_sz = buf_sz;
	memcpy(kcs->obuf, buf, buf_sz);
	kcs_write_data(dev, kcs->obuf[0]);

	return buf_sz;
}

static int kcs_npcm4xx_init(const struct device *dev)
{
	int rc;
	struct kcs_npcm4xx_data *kcs = (struct kcs_npcm4xx_data *)dev->data;
	struct kcs_npcm4xx_config *cfg = (struct kcs_npcm4xx_config *)dev->config;

	kcs->ibuf_idx = 0;
	kcs->ibuf_avail = 0;

	kcs->obuf_idx = 0;
	kcs->obuf_data_sz = 0;

	kcs->phase = KCS_PHASE_IDLE;

	rc = kcs_aspeed_config_ioregs(dev);
	if (rc) {
		LOG_ERR("failed to config IO regs, rc=%d\n", rc);
		return rc;
	}

	LOG_INF("KCS%d: addr=0x%x, idr=0x%x, odr=0x%x, str=0x%x\n",
		cfg->chan, cfg->addr,
		kcs->idr, kcs->odr, kcs->str);

	return 0;
}

#define KCS_NPCM4XX_INIT(n)						      \
	static struct kcs_npcm4xx_data kcs_npcm4xx_data_##n;		      \
									      \
	static const struct kcs_npcm4xx_config kcs_npcm4xx_config_##n = {     \
		.base = DT_INST_REG_ADDR(n),				      \
		.chan = DT_INST_PROP(n, chan),				      \
		.addr = DT_INST_PROP(n, addr),				      \
	};								      \
									      \
	DEVICE_DT_INST_DEFINE(n,					      \
			      kcs_npcm4xx_init,				      \
			      NULL,					      \
			      &kcs_npcm4xx_data_##n,			      \
			      &kcs_npcm4xx_config_##n,			      \
			      POST_KERNEL,				      \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE,	      \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(KCS_NPCM4XX_INIT)
