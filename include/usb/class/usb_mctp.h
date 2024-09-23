/*
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2018,2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief USB HID Class device API header
 */

#ifndef ZEPHYR_INCLUDE_USB_MCTP_CLASS_DEVICE_H_
#define ZEPHYR_INCLUDE_USB_MCTP_CLASS_DEVICE_H_

#include <usb/usb_ch9.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*mctp_cb_t)(const struct device *dev, int32_t len,
			uint8_t *data);

struct mctp_ops {
	mctp_cb_t read;
	mctp_cb_t write;
};

struct mctp_usb_hdr {
	uint16_t id;
	uint8_t	rsvd;
	uint8_t	len;
} __packed;

#define U8_MAX		((uint8_t)255)
#define MCTP_USB_XFER_SIZE	512
#define MCTP_USB_BTU		U8_MAX
#define MCTP_USB_MTU_MIN	MCTP_USB_BTU
#define MCTP_USB_MTU_MAX	(U8_MAX - sizeof(struct mctp_usb_hdr))
#define MCTP_USB_DMTF_ID	0x1ab4


void usb_mctp_register_device(const struct device *dev,
			     const struct mctp_ops *op);

int mctp_usb_ep_write(const struct device *dev, const uint8_t *data, uint32_t data_len,
		     uint32_t *bytes_ret);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_USB_HID_CLASS_DEVICE_H_ */
