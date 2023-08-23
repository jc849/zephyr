/*----------------------------------------------------------------------------*/
/* Copyright (c) 2019 by Nuvoton Electronics Corporation                      */
/* All rights reserved.							      */
/*----------------------------------------------------------------------------*/

#include <stddef.h>
#include <stdint.h>
#include <header.h>

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
