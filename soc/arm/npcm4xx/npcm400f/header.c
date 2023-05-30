/*----------------------------------------------------------------------------*/
/* Copyright (c) 2019 by Nuvoton Electronics Corporation                      */
/* All rights reserved.							      */
/*----------------------------------------------------------------------------*/

#include <stddef.h>
#include <stdint.h>

typedef void (*ptr_rom_hook)(void);

/* Firmware header definition */
struct FIRMWARE_HEDAER_TYPE {
	uint8_t hImageTag[8]; /* "%FiMg32@" */
	uint8_t hSignature[512]; /* Include BB */
	uint32_t hOtpImgHdrOffset; /* Big Endian */

	uint16_t hFW_ID;
	uint8_t hReservedField0[2];

	/* YH@New */
	uint16_t hActiveFwAddr; /* Address byte 1 and 2 */
	uint16_t hRecoveryFwAddr; /* Address byte 1 and 2 */
	uint32_t hSystemEcFwOffset;

	union {
		uint8_t hDevMode;
		struct {
			uint8_t hSecureBoot : 1; /*  effect when oSecureBootEn = 0. */
			uint8_t hSecurityLvl : 1; /*  effect when oSecureBootEn = 0. */
			uint8_t hOTPRefToTable : 1; /*  effect when oSecureBootEn = 0. */
			uint8_t hHwTrimRefOTPTable : 1; /*  effect when hOTPRefToTable = 1, */
							/*  oSecureBootEn = 0. */
			uint8_t hNotUpdateOTPRegister : 1; /*  effect when oSecureBootEn = 0. */
			uint8_t hNotEraseOTPTable : 1;
			uint8_t hOTPRefToSrcTable : 1; /*  effect when oSecureBootEn = 0 */
							/*  and only source exist. */
			uint8_t hNotDoBackUp : 1; /*  effect in secure mode. */
		} hDevMode_Bits;
	};

	uint8_t hFlashLockReg0;
	uint8_t Reserved;
	uint8_t hEcFwRegionSize;

	uint32_t hUserFWEntryPoint; /* Little Endian */
	uint32_t hUserFWRamCodeFlashStart; /* Little Endian, */
	uint32_t hUserFWRamCodeFlashEnd; /* Little Endian, */
	uint32_t hUserFWRamCodeRamStart; /* Little Endian, */

	uint32_t hImageLen; /* Big Endian */

	union {
		uint8_t hSigPubKeyHashIdx;
		struct {
			uint8_t hSigPubKeyHashIdx : 1;
			uint8_t Reserved : 5;
			uint8_t hShaAlgoUsed : 1;
			uint8_t hSessPubKeyExist : 1;
		} hPubKeyStatus_Bits;
	};

	uint8_t hMajorVer; /* 0 ~ 55 */
	uint16_t hMinorVer; /* little Endian */

	uint8_t hRevokeKey; /* 0x58('X') is key 1, 0x59 is key2 */
	uint8_t hRevokeKeyInv;

	uint16_t bBBSize; /* Big Endian, */
	uint32_t hBBOffset; /* Big Endian, */
	uint8_t hBuildersPubKey[64]; /* Reserved now */
	uint32_t hBBWorkRAM; /* Big Endian, */

	uint8_t hSigPubKey[512]; /* rsa & ecdsa public key */
	uint8_t hRamCodeHash[32];

	uint8_t hOEMVersion[8];
	uint8_t hReleaseDate[3];
	uint8_t hProjectID[2];
	uint8_t hReservedField4[3];

	ptr_rom_hook hRomHook1Ptr; /* 6694 */
	ptr_rom_hook hRomHook2Ptr; /* 6694 */
	ptr_rom_hook hRomHook3Ptr; /* 6694 */
	ptr_rom_hook hRomHook4Ptr; /* 6694 */

	/* 6694 */
	struct {
		uint32_t hOffset;
		uint32_t hSize;
	} hFwSeg[4];

	uint8_t hhRamCodeAESTag[16];
	uint8_t hBootBlockAESTag[16];

	uint8_t hFwHash[4][64];

};

extern unsigned long _vector_table;
extern unsigned long __ramcode_flashstart__;
extern unsigned long __ramcode_flashend__;
extern unsigned long __ramcode_ramstart__;

extern unsigned long __hook_seg_start__;
extern unsigned long __hook_seg_end__;
extern unsigned long __mainfw_seg_start__;
extern unsigned long __mainfw_seg_end__;

__attribute__((section(".header"))) struct FIRMWARE_HEDAER_TYPE fw_header = {
	.hUserFWEntryPoint = (uint32_t)(&_vector_table),
	.hUserFWRamCodeFlashStart = (uint32_t)(&__ramcode_flashstart__),
	.hUserFWRamCodeFlashEnd = (uint32_t)(&__ramcode_flashend__),
	.hUserFWRamCodeRamStart = (uint32_t)(&__ramcode_ramstart__),
	.hRomHook1Ptr = 0, /* Hook 1 muse be flash code */
	.hRomHook2Ptr = 0,
	.hRomHook3Ptr = 0,
	.hRomHook4Ptr = 0,
	.hFwSeg[0].hOffset = 0x210, /* fixed, can't change */
	.hFwSeg[0].hSize = 0x500, /* fixed, can't change */
	.hFwSeg[1].hOffset = (uint32_t)(&__hook_seg_start__) - 0x80000,
	.hFwSeg[1].hSize = (uint32_t)(&__mainfw_seg_start__) - 0x80000,
	.hFwSeg[2].hOffset = (uint32_t)(&__mainfw_seg_start__) - 0x80000,
	.hFwSeg[2].hSize = (uint32_t)(&__mainfw_seg_end__) - 0x80000,
	.hFwSeg[3].hOffset = 0, /* Reserved for fan table and fill in BinGentool */
	.hFwSeg[3].hSize = 0, /* Reserved for fan table and fill in BinGentool */
};
