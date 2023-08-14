/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EXPORTS_PUB_I3C_H_
#define EXPORTS_PUB_I3C_H_

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <common/reg/reg_def.h>
#include <common/reg/reg_access.h>

/* generic data type used in lib source for compatibility */
typedef uint8_t		__u8;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef int8_t		__s8;
typedef int16_t		__s16;
typedef int32_t		__s32;

/*typedef struct i3c_reg	I3C_T;*/
#define I3C_T struct i3c_reg

#define	FALSE	false
#define TRUE	true

#define LSByte(word)	((__u8)((word) & 0xFF))
#define LSHalf(word)	((__u16)((word) & 0xFFFF))
#define MSByte(word)	((__u8)((word) >> 24))
#define MSHalf(word)	((__u16)((word) >> 16))

#ifndef MaskBit
#define MaskBit(x)  ((__u32) 0x1 << (x))
#endif

#ifndef make32
#define make32(whi, wlo) ((__u32)(((__u16)(wlo)) | (((__u32)(whi)) << 16)))
#endif

#ifndef make16
#define make16(bhi, blo) ((__u16)(((__u8)(blo)) | (((__u16)(bhi)) << 8)))
#endif

#ifndef BIT
#define BIT(b)			(1 << (b))
#endif

#ifndef IS_BIT_SET
#define IS_BIT_SET(reg, bit)        (((reg >> bit) & (0x1)) != 0)
#endif


/*----------------------------- I3C --------------------------------*/
#define I3C_TRANSFER_SPEED_SDR_12p5MHZ  12500000
#define I3C_TRANSFER_SPEED_SDR_8MHZ     8000000
#define I3C_TRANSFER_SPEED_SDR_6MHZ     6000000
#define I3C_TRANSFER_SPEED_SDR_4MHZ     4000000
#define I3C_TRANSFER_SPEED_SDR_2MHZ     2000000
#define I3C_TRANSFER_SPEED_SDR_1MHZ     1000000

#define I3C_TRANSFER_SPEED_SDR_IBI      1000000

#define I3C_TRANSFER_SPEED_I2C_1MHZ     1000000
#define I3C_TRANSFER_SPEED_I2C_400KHZ   400000
#define I3C_TRANSFER_SPEED_I2C_100KHZ   100000

#define I3C_TRANSFER_SPEED_UNDEF  0

#define I3C_PAYLOAD_SIZE_MAX	69
#define IBI_PAYLOAD_SIZE_MAX	8

enum I3C_PORT {
	I3C1_IF,
	I3C2_IF,
	I3C3_IF,
	I3C4_IF,
	I3C5_IF,
	I3C6_IF,
#if (CHIP_ID_NPCM4XX == 5832)
	I3C_PORT_MAX = 2,
#else
	I3C_PORT_MAX = 6,
#endif
};

#define I3C_PORT_Enum enum I3C_PORT

#define I3C_BUS_COUNT_MAX    I3C_PORT_MAX

#define I2C_STATIC_ADDR_DEFAULT_7BIT 0xFFU
#define I3C_DYNAMIC_ADDR_DEFAULT_7BIT 0x00U

/*#define I3C_BROADCAST_ADDR	0x7E*/

enum I3C_DEVICE_TYPE {
	I3C_DEVICE_TYPE_PURE_I3C = 0U,
	I3C_DEVICE_TYPE_PURE_I2C = 1U,
	I3C_DEVICE_TYPE_MIXED = 2U,
};

#define I3C_DEVICE_TYPE_Enum enum I3C_DEVICE_TYPE

enum I3C_DEVICE_MODE {
	I3C_DEVICE_MODE_DISABLE            = 0U,
	I3C_DEVICE_MODE_CURRENT_MASTER     = 1U,
	I3C_DEVICE_MODE_SLAVE_ONLY         = 2U,
	I3C_DEVICE_MODE_SECONDARY_MASTER   = 3U,
};

#define I3C_DEVICE_MODE_Enum enum I3C_DEVICE_MODE

enum I3C_SLAVE_DEVICE_MODE {
	I3C_SLAVE_DEVICE_MODE_I2C,
	I3C_SLAVE_DEVICE_MODE_I3C,
};

#define I3C_SLAVE_DEVICE_MODE_Enum enum I3C_SLAVE_DEVICE_MODE

enum I3C_BUS_STATE {
	I3C_BUS_STATE_DEFAULT,
	I3C_BUS_STATE_WAIT_RESET_DONE,
	I3C_BUS_STATE_WAIT_CLEAR_DONE,
	I3C_BUS_STATE_INIT,
	I3C_BUS_STATE_IDLE,
};

#define I3C_BUS_STATE_Enum enum I3C_BUS_STATE

enum I3C_BUS_TYPE {
	I3C_BUS_TYPE_PURE_I3C = 0U,
	I3C_BUS_TYPE_PURE_I2C = 1U,
	I3C_BUS_TYPE_MIXED = 2U,
};

#define I3C_BUS_TYPE_Enum enum I3C_BUS_TYPE

enum I3C_VERSION {
	I3C_VER_1p0 = 0U,
	I3C_VER_1p1 = 1U,
};

#define I3C_VERSION_Enum enum I3C_VERSION

struct I3C_BAUDRATE {
	__u32 i3cI2c;
	__u32 i3cSdr;

	__u16 tHmax;
	__u16 tHmin;
	__u16 tLmax;
	__u16 tLmin;
	__u16 tTimeout;
	_Bool bRunI3C;
};

#define I3C_BAUDRATE_t struct I3C_BAUDRATE

struct I3C_DEVICE_INFO;
struct I3C_DEVICE_INFO_SHORT;

/*typedef */struct I3C_BUS_INFO {
	__u8 busno;

	I3C_BUS_TYPE_Enum busType;
	I3C_VERSION_Enum busVersion;
	I3C_BUS_STATE_Enum busState;
	struct I3C_TRANSFER_TASK *pCurrentTask;
	__u16 timer_ms;

	__u8 DevCount;
	struct I3C_DEVICE_INFO_SHORT *pDevList;
	struct I3C_DEVICE_INFO *pCurrentMaster;
} /* I3C_BUS_INFO_t*/;

#define I3C_BUS_INFO_t struct I3C_BUS_INFO

/*typedef */union I3C_DEVICE_ATTRIB_t {
	struct {
		__u16 reqSETAASA : 1;
		__u16 reqSETDASA : 1;
		__u16 reqRSTDAA : 1;
		__u16 reqSETHID : 1;
		__u16 reqHotJoin : 1;
		__u16 reqPostInit : 1;

		__u16 doneSETAASA : 1;
		__u16 doneSETDASA : 1;
		__u16 doneRSTDAA : 1;
		__u16 doneSETHID : 1;
		__u16 doneHotJoin : 1;
		__u16 donePostInit : 1;

		__u16 suppMST : 1;
		__u16 suppSLV : 1;
		__u16 defaultMST : 1;

		__u16 suppENTDAA : 1;
		__u16 present: 1;
		__u16 runI3C: 1;
	} b;
	__u16 U16;
} /* I3C_DEVICE_ATTRIB_t */;

#define I3C_DEVICE_ATTRIB_t union I3C_DEVICE_ATTRIB_t

/*typedef */struct I3C_DEVICE_INFO_SHORT {
	__u8 staticAddr;
	__u8 dynamicAddr;
	__u8 pid[6];
	__u8 bcr;
	__u8 dcr;

	I3C_DEVICE_ATTRIB_t attr;
	struct I3C_DEVICE_INFO_SHORT *pNextDev;
	void *pDeviceInfo;
} /* I3C_DEVICE_INFO_SHORT_t */;

#define I3C_DEVICE_INFO_SHORT_t struct I3C_DEVICE_INFO_SHORT

#define CMD_INVALID_SDR 0xFE
#define CMD_INVALID_DDRRd 0xFE
#define CMD_DEFAULT 0xFD
#define CMD_INVALID 0xFE
#define CMD_INVALID_DDRWr 0x7E

#define TIMEOUT_TYPICAL	10

#define NOT_HIF FALSE
#define IS_HIF TRUE

/*typedef */union cmd_t {
	__u8 cmd8;
	__u16 cmd16;
} /* cmd_t*/;

#define cmd_t union cmd_t

/*typedef */struct cmd_attrib {
	__u8 endian	: 1;	/* 0b: little, 1b: bigh endian, if width != 0 */
	__u8 width : 1;		/* 0b = 1, 1b = 2 */
	__u8 write : 1;
	__u8 read  : 1;
} /* cmd_attrib_t */;

#define cmd_attrib_t struct cmd_attrib

/*typedef */struct I3C_REG_ITEM {
	cmd_t cmd;

	union {
		__u8 rw;
		cmd_attrib_t attr;
	};

	__u16 len;
	__u8 *buf;
} /* I3C_REG_ITEM_t*/;

#define I3C_REG_ITEM_t struct I3C_REG_ITEM

enum I3C_ErrCode {
	I3C_ERR_OK = 0,
	I3C_ERR_PENDING = 1,				/* wait i3c engine to process it */
	I3C_ERR_OUT_OF_MEMORY = 2,          /* allocate memory fail */
	I3C_ERR_PARAMETER_INVALID = 4,      /* parameter invalid */
	I3C_ERR_TASK_INVALID = 8,			/* illegal task */
	I3C_ERR_MEMORY_RAN_OUT = 16,        /* allocate memory is insufficient */
	I3C_ERR_DATA_ERROR = 32,			/* crc error for SPD5118, MCTP */
	I3C_ERR_SW_TIMEOUT = 64,			/* timer timeout, or retry too many times */
	I3C_ERR_HW_NOT_SUPPORT,
	I3C_ERR_BUS_ERROR,
	I3C_ERR_MERRWARN,
	I3C_ERR_NACK,
	I3C_ERR_TERM,
	I3C_ERR_WRABT,
	I3C_ERR_ERRWARN,
	I3C_ERR_HW_TIMEOUT,
	I3C_ERR_BUS_BUSY,
	I3C_ERR_SLVSTART,
	I3C_ERR_IBI,
	I3C_ERR_MR,
	I3C_ERR_HJ,
	I3C_ERR_NACK_SLVSTART,
};

#define I3C_ErrCode_Enum enum I3C_ErrCode

enum I3C_CCC {
	CCC_BROADCAST_ENEC      = 0x00, /* Enable Events Command */
	CCC_BROADCAST_DISEC     = 0x01, /* Disable Events Command */
	CCC_BROADCAST_ENTAS0    = 0x02, /* Enter Activity State 0, 1 us */
	CCC_BROADCAST_ENTAS1    = 0x03, /* Enter Activity State 1, 100 us */
	CCC_BROADCAST_ENTAS2    = 0x04, /* Enter Activity State 2, 2 ms */
	CCC_BROADCAST_ENTAS3    = 0x05, /* Enter Activity State 3, 50 ms */
	CCC_BROADCAST_RSTDAA    = 0x06, /* Reset Dynamic Address Assignment */
	CCC_BROADCAST_ENTDAA    = 0x07, /* Enter Dynamic Address Assignment */
	CCC_BROADCAST_DEFSLVS   = 0x08,
	CCC_BROADCAST_SETMWL    = 0x09,
	CCC_BROADCAST_SETMRL    = 0x0A,
	CCC_BROADCAST_ENTTM     = 0x0B,
	CCC_BROADCAST_SETBUSCON = 0x0C,
/*    CCC_BROADCAST_rsvd      = 0x0D - 0x11, */
	CCC_BROADCAST_ENDXFER   = 0x12,
/*    CCC_BROADCAST_rsvd      = 0x13 - 0x1F, */
	CCC_BROADCAST_ENTHDR0   = 0x20,
	CCC_BROADCAST_ENTHDR1   = 0x21,
	CCC_BROADCAST_ENTHDR2   = 0x22,
	CCC_BROADCAST_ENTHDR3   = 0x23,
	CCC_BROADCAST_ENTHDR4   = 0x24,
	CCC_BROADCAST_ENTHDR5   = 0x25,
	CCC_BROADCAST_ENTHDR6   = 0x26,
	CCC_BROADCAST_ENTHDR7   = 0x27,
	CCC_BROADCAST_SETXTIME  = 0x28,
	CCC_BROADCAST_SETAASA   = 0x29, /* Set All Addresses to Static Addresses */
	CCC_BROADCAST_RSTACT    = 0x2A,
	CCC_BROADCAST_DEFGRPA   = 0x2B,
	CCC_BROADCAST_RSTGRPA   = 0x2C,
	CCC_BROADCAST_MLANE     = 0x2D,
/*    CCC_BROADCAST_rsvd      = 0x2E - 0x48, */
/*    CCC_BROADCAST_rsvd      = 0x49 - 0x4C, */
/*    CCC_BROADCAST_rsvd      = 0x4D - 0x57, */
/*    CCC_BROADCAST_rsvd      = 0x58 - 0x5B, */
/*    CCC_BROADCAST_rsvd      = 0x5C - 0x60, */
/*    CCC_BROADCAST_rsvd      = 0x61 - 0x7F, */
	CCC_BROADCAST_SETHID    = 0x61, /* JEDEC DDR5 */
	CCC_BROADCAST_DEVCTRL   = 0x62, /* JEDEC DDR5 */
	CCC_DIRECT_ENEC         = 0x80, /* Enable Events Command, Direct Set */
	CCC_DIRECT_DISEC        = 0x81, /* Disable Events Command, Direct Set */
	CCC_DIRECT_ENTAS0       = 0x82, /* Enter Activity State 0, 1 us, Direct Set */
	CCC_DIRECT_ENTAS1       = 0x83, /* Enter Activity State 1, 100 us, Direct Set */
	CCC_DIRECT_ENTAS2       = 0x84, /* Enter Activity State 2, 2 ms, Direct Set */
	CCC_DIRECT_ENTAS3       = 0x85, /* Enter Activity State 3, 50 ms, Direct Set */
	CCC_DIRECT_RSTDAA       = 0x86, /* Reset Dynamic Address Assignment, Direct Set */
	CCC_DIRECT_SETDASA      = 0x87, /* Set Dynamic Address from Static Address, Direct Set */
	CCC_DIRECT_SETNEWDA     = 0x88, /* Set New Dynamic Address, Direct Set */
	CCC_DIRECT_SETMWL       = 0x89, /* Set Max Write Length, Direct Set */
	CCC_DIRECT_SETMRL       = 0x8A, /* Set Max Read Length, Direct Set */
	CCC_DIRECT_GETMWL       = 0x8B, /* Get Max Write Length, Direct Get */
	CCC_DIRECT_GETMRL       = 0x8C, /* Get Max Read Length, Direct Get */
	CCC_DIRECT_GETPID       = 0x8D, /* Get Provisioned ID, Direct Get */
	CCC_DIRECT_GETBCR       = 0x8E, /* Get Bus Characteristics Register, Direct Get */
	CCC_DIRECT_GETDCR       = 0x8F, /* Get Device Characteristics Register, Direct Get */
	CCC_DIRECT_GETSTATUS    = 0x90, /* Get Device Status, Direct Get */
	CCC_DIRECT_GETACCMST    = 0x91,
	CCC_DIRECT_GETACCCR     = 0x91, /* Get Accept Controller Role, Direct Get */
	CCC_DIRECT_ENDXFER      = 0x92, /* Data Transfer Ending Procedure Control, Direct Set */
	CCC_DIRECT_SETBRGTGT    = 0x93, /* Set Bridge Targets, Direct Set */
	CCC_DIRECT_GETMXDS      = 0x94, /* Get Max Data Speed, Direct Get */
	CCC_DIRECT_GETCAPS      = 0x95, /* GetOptional Feature Capabilities, Direct Get */
	CCC_DIRECT_SETROUTE     = 0x96, /* Set Route, Direct Set */
/*    CCC_DIRECT_D2DXFER      = 0x97, // Device to Device Tuneling Control, not include in Basic*/
	CCC_DIRECT_SETXTIME     = 0x98, /* Set Exchange Timing Information, Direct Set */
	CCC_DIRECT_GETXTIME     = 0x99, /* Get Exchange Timing Information, Direct Get */
	CCC_DIRECT_RSTACT       = 0x9A, /* Target Reset Action, Direct Set */
	CCC_DIRECT_SETGRPA      = 0x9B, /* Set Group Address */
	CCC_DIRECT_RSTGRPA      = 0x9C, /* Reset Group Address */
	CCC_DIRECT_MLANE        = 0x9D,
/*    CCC_DIRECT_rsvd         = 0x9E - 0xBF, */
/*    CCC_DIRECT_rsvd         = 0xC0 - 0xC3, */
/*    CCC_DIRECT_rsvd         = 0xC4 - 0xD6, */
/*    CCC_DIRECT_rsvd         = 0xD7 - 0xDA, */
/*    CCC_DIRECT_rsvd         = 0xDB - 0xDF, */
/*    CCC_DIRECT_rsvd         = 0xE0 - 0xFE, */
/*    CCC_DIRECT_rsvd         = 0xFF, */
};

#define I3C_CCC_Enum enum I3C_CCC

#define ENEC_MASK_ENINT (0x01)
#define ENEC_MASK_ENMR  (0x02)
#define ENEC_MASK_ENHJ  (0x08)

#define DISEC_MASK_ENINT (0x01)
#define DISEC_MASK_ENMR  (0x02)
#define DISEC_MASK_ENHJ  (0x08)

enum I3C_TRANSFER_PROTOCOL {
	I3C_TRANSFER_PROTOCOL_I2C_WRITE         = 0x00,
	I3C_TRANSFER_PROTOCOL_I2C_READ          = 0x01,
	I3C_TRANSFER_PROTOCOL_I2C_WRITEnREAD    = 0x02,

	I3C_TRANSFER_PROTOCOL_I3C_WRITE         = 0x10,
	I3C_TRANSFER_PROTOCOL_I3C_READ          = 0x11,
	I3C_TRANSFER_PROTOCOL_I3C_WRITEnREAD    = 0x12,
	I3C_TRANSFER_PROTOCOL_I3C_W7E           = 0x13,
	I3C_TRANSFER_PROTOCOL_I3C_R7E           = 0x14,
	I3C_TRANSFER_PROTOCOL_I3C_W7EnREAD      = 0x15,

	I3C_TRANSFER_PROTOCOL_DDR_WRITE         = 0x20,
	I3C_TRANSFER_PROTOCOL_DDR_READ          = 0x21,

	I3C_TRANSFER_PROTOCOL_CCCb              = 0x40U,	/* Broadcast CCC*/
	I3C_TRANSFER_PROTOCOL_CCCw              = 0x41U,	/* Direct Write*/
	I3C_TRANSFER_PROTOCOL_CCCr              = 0x42U,	/* Direct Read*/
	I3C_TRANSFER_PROTOCOL_ENTDAA            = 0x43U,	/* ENTDAA*/

	I3C_TRANSFER_PROTOCOL_EVENT             = 0x80U,	/* for IBI/Hot-Join/Master Request*/
	I3C_TRANSFER_PROTOCOL_IBI               = 0x81U,	/* Slave's task for IBI*/
	I3C_TRANSFER_PROTOCOL_MASTER_REQUEST    = 0x82U,	/* Slave's task for Master Request*/
	I3C_TRANSFER_PROTOCOL_HOT_JOIN          = 0x84U,	/* Slave's task for Hot-Join*/
};

#define I3C_TRANSFER_PROTOCOL_Enum enum I3C_TRANSFER_PROTOCOL

#define I2C_TRANSFER_PROTOCOL(x) (((x & 0xF0) == 0x00) ? TRUE : FALSE)
#define I3C_TRANSFER_PROTOCOL(x) (((x & 0xF0) == 0x10) ? TRUE : FALSE)
#define DDR_TRANSFER_PROTOCOL(x) (((x & 0xF0) == 0x20) ? TRUE : FALSE)
#define CCC_TRANSFER_PROTOCOL(x) (((x & 0xF0) == 0x40) ? TRUE : FALSE)
	#define CCC_BROADCAST(CCC) (((CCC >= 0) && (CCC <= 0x7F)) ? TRUE : FALSE)
	#define CCC_DIRECT(CCC) (((CCC >= 0x80) && (CCC <= 0xFE)) ? TRUE : FALSE)
#define EVENT_TRANSFER_PROTOCOL(x) (((x & 0xF0) == 0x80) ? TRUE : FALSE)
#define SLAVE_TRANSFER_PROTOCOL(x) (((x == 0x81) || (x == 0x82) || (x == 0x84)) ? TRUE : FALSE)

#define WRITE_TRANSFER_PROTOCOL(x)      (((x & 0xCF) == 0x00) ? TRUE : FALSE)
#define READ_TRANSFER_PROTOCOL(x)       (((x & 0xCF) == 0x01) ? TRUE : FALSE)
#define WRITEnREAD_TRANSFER_PROTOCOL(x) (((x & 0xCF) == 0x02) ? TRUE : FALSE)
#define W7E_TRANSFER_PROTOCOL(x)        (((x & 0xCF) == 0x03) ? TRUE : FALSE)
#define R7E_TRANSFER_PROTOCOL(x)        (((x & 0xCF) == 0x04) ? TRUE : FALSE)
#define W7EnREAD_TRANSFER_PROTOCOL(x)   (((x & 0xCF) == 0x05) ? TRUE : FALSE)

enum I3C_TASK_POLICY {
	I3C_TASK_POLICY_INSERT_FIRST,
	I3C_TASK_POLICY_APPEND_LAST,
};

#define I3C_TASK_POLICY_Enum enum I3C_TASK_POLICY

enum I3C_TRANSFER_FLAG {
	I3C_TRANSFER_NORMAL             = 0x00U,
	I3C_TRANSFER_NO_STOP            = 0x01U,
	I3C_TRANSFER_REPEAT_START       = 0x02U,
	I3C_TRANSFER_NO_START           = 0x04U,
	I3C_TRANSFER_RETRY_ENABLE		= 0x08U,
	I3C_TRANSFER_RETRY_WITHOUT_STOP = 0x10U,
	I3C_TRANSFER_WORD_WIDTH         = 0x20U,
	I3C_TRANSFER_MESSAGE_MODE       = 0x40U,
	I3C_TRANSFER_NAK				= 0x100U,
};

#define I3C_TRANSFER_FLAG_Enum enum I3C_TRANSFER_FLAG

enum I3C_TRANSFER_DIR {
	I3C_TRANSFER_DIR_WRITE = 0U,
	I3C_TRANSFER_DIR_READ  = 1U,
};

#define I3C_TRANSFER_DIR_Enum enum I3C_TRANSFER_DIR

enum I3C_TRANSFER_TYPE {
	I3C_TRANSFER_TYPE_SDR    = 0U,
	I3C_TRANSFER_TYPE_I2C    = 1U,
	I3C_TRANSFER_TYPE_DDR    = 2U,
};

#define I3C_TRANSFER_TYPE_Enum enum I3C_TRANSFER_TYPE

enum I3C_IBITYPE {
	I3C_IBITYPE_None    = 0,
	I3C_IBITYPE_IBI     = 1,
	I3C_IBITYPE_MstReq  = 2,
	I3C_IBITYPE_HotJoin = 3,
};

#define I3C_IBITYPE_Enum enum I3C_IBITYPE

typedef __u32 (*ptrI3C_RetFunc)(__u32 TaskInfo, __u32 ErrDetail);

struct I3C_CAPABILITY_INFO {
	__u8	MASTER : 1;
	__u8	SLAVE : 1;

	__u8	INT : 1;
	__u8	DMA : 1;
	__u8	OFFLINE : 1;

	__u8	HDR_DDR : 1;
	__u8	HDR_TSP : 1;
	__u8	HDR_TSL : 1;
	__u8	IBI : 1;
	__u8	HOT_JOIN : 1;
	__u8	MASTER_REQUEST : 1;

	/* Time Control */
	__u8	SYNC : 1;
	__u8	ASYNC0 : 1;
	__u8	ASYNC1 : 1;
	__u8	ASYNC2 : 1;
	__u8	ASYNC3 : 1;

	/* Version */
	__u8 I3C_VER_1p0 : 1;
	__u8 I3C_VER_1p1 : 1;

	__u8 FIFO_TX_LEN;
	__u8 FIFO_RX_LEN;
};

#define I3C_CAPABILITY_INFO_t struct I3C_CAPABILITY_INFO

struct I3C_DEVICE_INFO {
	struct I3C_CAPABILITY_INFO capability;
	I3C_PORT_Enum port;
	struct I3C_BUS_INFO *pOwner;    /* bus */
	struct I3C_DEVICE_INFO_SHORT *pDevInfo;

	I3C_DEVICE_MODE_Enum mode;  /* off, master on, slave only, master capable */
	_Bool bRunI3C;              /* current slave device working mode, 0:i2c, 1:i3c */

	/* common */
	__u8 dynamicAddr;
	__u8 pid[6];
	__u8 dcr;                /* Device characteristics register information.*/
	__u8 bcr;                /* Bus characteristics register information.*/
	__u16 vendorID;          /* Device vendor ID (manufacture ID).*/
	__u32 partNumber;        /* Device part number info */

	/* master config */
	_Bool enableOpenDrainHigh;
	_Bool enableOpenDrainStop;
	_Bool disableTimeout;
	_Bool ackIBI; /* For master, used to define master should ack or nack the slave request */
				  /* For slave, used to set correct protocol (with 7E)*/
	struct I3C_BAUDRATE baudrate;

	__u8 *pRxBuf;
	__u8 *pTxBuf;

	__u16 rxOffset;
	__u16 txOffset;

	__u16 rxLen;
	__u16 txLen;

	/* slave config */
	__u8 staticAddr; /* 7-bit */
	_Bool stopSplitRead; /* 1b: master can read un-read data after STOP */
						 /* 0b: slave should reset command, RX DMA/FIFO */
	__u8 cmdIndex;
	I3C_REG_ITEM_t *pReg;

	_Bool bAbort;
	volatile __u8 task_count;
	struct I3C_TRANSFER_TASK *pTaskListHead;
	ptrI3C_RetFunc callback;
};

#define I3C_DEVICE_INFO_t struct I3C_DEVICE_INFO

/*
 * initMode
 * Used to configure how to init slave device on the bus
 */
#define INITMODE_I2C        0x0000U
#define INITMODE_I3C        0x0001U
#define INITMODE_BUSRESET   0x0002U
#define INITMODE_DEVCTRL    0x0004U
#define INITMODE_SETHID     0x0008U
#define INITMODE_SETAASA    0x0010U
#define INITMODE_SETDASA    0x0020U
#define INITMODE_ENTDAA		0x0040U
#define INITMODE_POWERON    0x0080U
#define INITMODE_POST_INIT  0x0100U
#define INITMODE_RSTDAA     0x0200U

struct I3C_TASK_INFO {
	struct I3C_TRANSFER_TASK *pTask;
	I3C_ErrCode_Enum result;
	ptrI3C_RetFunc callback;

	__u8 MasterRequest : 1;
	__u8 bHIF : 1;
	I3C_PORT_Enum Port;

	__u32 u32SwTimeout;

	void *pParentTaskInfo;

	/* used for Sensor module */
	__u8 idx;
	__u8 fmt;
};

#define I3C_TASK_INFO_t struct I3C_TASK_INFO

struct I3C_TRANSFER_TASK {
	struct I3C_TASK_INFO *pTaskInfo;

	I3C_TRANSFER_PROTOCOL_Enum protocol;
	__u32 baudrate;
	__u8 address;

	__u8 *pWrBuf;
	__u16 *pWrLen;

	__u8 *pRdBuf;
	__u16 *pRdLen;

	struct I3C_TRANSFER_FRAME *pFrameList;
	__u8 frame_count;
	__u8 frame_idx;

	struct I3C_TRANSFER_TASK *pNextTask;
};

#define I3C_TRANSFER_TASK_t struct I3C_TRANSFER_TASK

struct I3C_TRANSFER_FRAME {
	struct I3C_TRANSFER_TASK *pOwner;

	I3C_TRANSFER_FLAG_Enum flag;
	I3C_TRANSFER_TYPE_Enum type;
	__u32 baudrate;

	__u8 address;
	I3C_TRANSFER_DIR_Enum direction;
	__u8 hdrcmd;

	__u16 access_len;
	__u16 access_idx;
	__u8 *access_buf;
	__u8 retry_count;

	struct I3C_TRANSFER_FRAME *pNextFrame;
};

#define I3C_TRANSFER_FRAME_t struct I3C_TRANSFER_FRAME

/* LSM6DSO */
enum I3C_LSM6DSO_STATE {
	I3C_LSM6DSO_STATE_DEFAULT,
	I3C_LSM6DSO_STATE_INIT,
	I3C_LSM6DSO_STATE_IDLE,
	I3C_LSM6DSO_STATE_BUSY,
	I3C_LSM6DSO_STATE_TASK_CANNOT_COMPLETE,
	I3C_LSM6DSO_STATE_DEVICE_NOT_PRESENT,
};

#define I3C_LSM6DSO_STATE_Enum enum I3C_LSM6DSO_STATE

enum LSM6DSO_POST_INIT_STATE {
	LSM6DSO_POST_INIT_STATE_Default,

	LSM6DSO_POST_INIT_STATE_1,
	LSM6DSO_POST_INIT_STATE_1_Wait,
	LSM6DSO_POST_INIT_STATE_1_Done,

	LSM6DSO_POST_INIT_STATE_2,
	LSM6DSO_POST_INIT_STATE_2_Wait,
	LSM6DSO_POST_INIT_STATE_2_Done,

	LSM6DSO_POST_INIT_STATE_3,
	LSM6DSO_POST_INIT_STATE_3_Wait,
	LSM6DSO_POST_INIT_STATE_3_Done,

	LSM6DSO_POST_INIT_STATE_END,
};

#define LSM6DSO_POST_INIT_STATE_Enum enum LSM6DSO_POST_INIT_STATE

struct LSM6DSO_DEVICE_INFO {
	I3C_DEVICE_INFO_t i3c_device;
	__u32 baudrate;
	I3C_LSM6DSO_STATE_Enum state;
	LSM6DSO_POST_INIT_STATE_Enum post_init_state;
	__u16 initMode;
	__u16 rxLen;
	__u8 rxBuf[16];
	__u8 temp_val;
};

#define LSM6DSO_DEVICE_INFO_t struct LSM6DSO_DEVICE_INFO

/* PENDINT definition */
#define I3C_PENDING_NONE	0x00
#define I3C_PENDING_MCTP	0x01

#endif /* EXPORTS_PUB_I3C_H_ */
