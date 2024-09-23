/*
 * USB mctp function
 *
 * Copyright (c) 2018 Phytec Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>

#include <sys/byteorder.h>
#include <usb/usb_device.h>
#include <usb_descriptor.h>
#include <usb/class/usb_mctp.h>

#define LOG_LEVEL CONFIG_USB_DEVICE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(usb_mctp);

#define USB_CLASS_MCTP 0x14

#define MCTP_IN_EP_IDX		0
#define MCTP_OUT_EP_IDX		1

static uint8_t mctp_buf[MCTP_USB_XFER_SIZE];

struct mctp_device_info {
	const struct mctp_ops *ops;
	struct usb_dev_data common;
};

/* usb.rst config structure start */
struct usb_mctp_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

#define INITIALIZER_IF							\
	{								\
		.bLength = sizeof(struct usb_if_descriptor),		\
		.bDescriptorType = USB_DESC_INTERFACE,			\
		.bInterfaceNumber = 0,					\
		.bAlternateSetting = 0,					\
		.bNumEndpoints = 2,					\
		.bInterfaceClass = USB_CLASS_MCTP,				\
		.bInterfaceSubClass = 0,				\
		.bInterfaceProtocol = 0x1,				\
		.iInterface = 0,					\
	}

#define INITIALIZER_IF_EP(addr, attr, mps)				\
	{								\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = addr,				\
		.bmAttributes = attr,					\
		.wMaxPacketSize = sys_cpu_to_le16(mps),			\
		.bInterval = 0,		\
	}

#define DEFINE_MCTP_DESCR(x, _)						\
	USBD_CLASS_DESCR_DEFINE(primary, x)				\
	struct usb_mctp_config mctp_cfg_##x = {				\
	/* Interface descriptor */					\
	.if0 = INITIALIZER_IF,						\
	.if0_out_ep =						\
		INITIALIZER_IF_EP(AUTO_EP_IN,				\
				  USB_DC_EP_BULK,			\
				  MCTP_USB_XFER_SIZE),		\
	.if0_in_ep =						\
		INITIALIZER_IF_EP(AUTO_EP_OUT,				\
				  USB_DC_EP_BULK,			\
				  MCTP_USB_XFER_SIZE),		\
	};

static sys_slist_t usb_mctp_devlist;

void usb_mctp_register_device(const struct device *dev, const struct mctp_ops *ops)
{
	struct mctp_device_info *dev_data = dev->data;

	dev_data->ops = ops;
	dev_data->common.dev = dev;

	sys_slist_append(&usb_mctp_devlist, &dev_data->common.node);

	LOG_DBG("Added dev_data %p dev %p to devlist %p", dev_data, dev,
		&usb_mctp_devlist);
}

static void mctp_out_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status)
{
	struct mctp_device_info *dev_data;
	struct usb_dev_data *common;
	uint32_t bytes_to_read;

	common = usb_get_dev_data_by_ep(&usb_mctp_devlist, ep);
	if (common == NULL) {
		LOG_WRN("Device data not found for endpoint %u", ep);
		return;
	}

	dev_data = CONTAINER_OF(common, struct mctp_device_info, common);

	if (ep_status != USB_DC_EP_DATA_OUT || dev_data->ops == NULL ||
	    dev_data->ops->read == NULL) {
		return;
	}

	usb_read(ep, NULL, 0, &bytes_to_read);
	LOG_DBG("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
	usb_read(ep, mctp_buf, bytes_to_read, NULL);

	dev_data->ops->read(common->dev, bytes_to_read, mctp_buf);
}

int mctp_usb_ep_write(const struct device *dev, const uint8_t *data, uint32_t data_len,
		     uint32_t *bytes_ret)
{
	const struct usb_cfg_data *cfg = dev->config;
	int ret;

	/* transfer data to host */
	ret = usb_transfer_sync(cfg->endpoint[MCTP_IN_EP_IDX].ep_addr,
				(uint8_t *)data, data_len, USB_TRANS_WRITE);
	if (ret != data_len) {
		LOG_ERR("Transfer failure");
		return -EINVAL;
	}

	*bytes_ret = ret;

	return 0;
}

#define INITIALIZER_EP_DATA(cb, addr)			\
	{						\
		.ep_cb = cb,				\
		.ep_addr = addr,			\
	}

#define DEFINE_MCTP_EP(x, _)						\
	static struct usb_ep_cfg_data mctp_ep_data_##x[] = {		\
		INITIALIZER_EP_DATA(usb_transfer_ep_callback, AUTO_EP_IN),		\
		INITIALIZER_EP_DATA(mctp_out_cb, AUTO_EP_OUT),		\
	};


/* usb.rst endpoint configuration end */

static void mctp_status_cb(struct usb_cfg_data *cfg,
			       enum usb_dc_status_code status,
			       const uint8_t *param)
{
	ARG_UNUSED(cfg);
	ARG_UNUSED(status);
	ARG_UNUSED(param);

	LOG_DBG("status_cb status %d", status);
}

static void mctp_interface_config(struct usb_desc_header *head,
				      uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *if_desc = (struct usb_if_descriptor *)head;
	struct usb_mctp_config *desc =
		CONTAINER_OF(if_desc, struct usb_mctp_config, if0);


	desc->if0.bInterfaceNumber = bInterfaceNumber;
}


#define DEFINE_MCTP_CFG_DATA(x, _)					\
	USBD_CFG_DATA_DEFINE(primary, mctp)				\
	struct usb_cfg_data mctp_config_##x = {				\
		.usb_device_description = NULL,				\
		.interface_config = mctp_interface_config,		\
		.interface_descriptor = &mctp_cfg_##x.if0,		\
		.cb_usb_status = mctp_status_cb,				\
		.interface = {						\
			.class_handler = NULL,		\
			.custom_handler = NULL,	\
		},							\
		.num_endpoints = ARRAY_SIZE(mctp_ep_data_##x),		\
		.endpoint = mctp_ep_data_##x,				\
	};

static int usb_mctp_device_init(const struct device *dev)
{
	LOG_INF("Init MCTP USB Device: dev %p (%s)", dev, dev->name);

	return 0;
}

#define DEFINE_MCTP_DEV_DATA(x, _)					\
	struct mctp_device_info usb_mctp_dev_data_##x;

#define DEFINE_MCTP_DEVICE(x, _)						\
	DEVICE_DEFINE(usb_mctp_device_##x,				\
			    CONFIG_USB_MCTP_DEVICE_NAME "_" #x,		\
			    &usb_mctp_device_init,			\
			    NULL,					\
			    &usb_mctp_dev_data_##x,			\
			    &mctp_config_##x, POST_KERNEL,		\
			    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,	\
			    NULL);

UTIL_LISTIFY(CONFIG_USB_MCTP_DEVICE_COUNT, DEFINE_MCTP_DESCR, _)
UTIL_LISTIFY(CONFIG_USB_MCTP_DEVICE_COUNT, DEFINE_MCTP_EP, _)
UTIL_LISTIFY(CONFIG_USB_MCTP_DEVICE_COUNT, DEFINE_MCTP_CFG_DATA, _)
UTIL_LISTIFY(CONFIG_USB_MCTP_DEVICE_COUNT, DEFINE_MCTP_DEV_DATA, _)
UTIL_LISTIFY(CONFIG_USB_MCTP_DEVICE_COUNT, DEFINE_MCTP_DEVICE, _)
