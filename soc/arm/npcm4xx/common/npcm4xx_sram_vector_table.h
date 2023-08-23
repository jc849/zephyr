/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Definitions for the ram(sram) vector table
 *
 *
 * Definitions for the ram(sram) vector table.
 *
 * System exception handler names all have the same format:
 *
 *   __<exception name with underscores>
 *
 * No other symbol has the same format, so they are easy to spot.
 */

#ifndef ZEPHYR_SOC_ARM_NPCM4XX_COMMON_SRAM_VECTOR_TABLE_H_
#define ZEPHYR_SOC_ARM_NPCM4XX_COMMON_SRAM_VECTOR_TABLE_H_

#ifdef _ASMLANGUAGE

#include <toolchain.h>
#include <linker/sections.h>
#include <sys/util.h>

GDATA(_npcm4xx_sram_vector_table)

GTEXT(z_arm_npcm4xx_reset)
GTEXT(z_arm_npcm4xx_nmi)
GTEXT(z_arm_npcm4xx_hard_fault)
GTEXT(z_arm_ncpm4xx_mpu_fault)
GTEXT(z_arm_npcm4xx_bus_fault)
GTEXT(z_arm_npcm4xx_usage_fault)
GTEXT(z_arm_npcm4xx_svc)
GTEXT(z_arm_npcm4xx_debug_monitor)
GTEXT(z_arm_npcm4xx_pendsv)
GTEXT(z_arm_npcm4xx_sys_clock_isr)
GTEXT(z_arm_npcm4xx_exc_spurious)

#else /* _ASMLANGUAGE */

#ifdef __cplusplus
extern "C" {
#endif

extern void *_npcm4xx_sram_vector_table[];

#ifdef __cplusplus
}
#endif

#endif /* _ASMLANGUAGE */

#endif /* ZEPHYR_SOC_ARM_NPCM4XX_COMMON_SRAM_VECTOR_TABLE_H_ */
