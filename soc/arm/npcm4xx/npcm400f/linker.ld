/* linker.ld - Linker command/script file */

/*
 * Copyright (c) 2021 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <arch/arm/aarch32/cortex_m/scripts/linker.ld>

#ifdef CONFIG_OPENAMP_RSC_TABLE

#define RTRAM_ADDR  (0x36106000)
#define RTRAM_SIZE  (1K)

MEMORY
    {
    RTRAM (wx) : ORIGIN = RTRAM_ADDR, LENGTH = RTRAM_SIZE
    }

#endif

SECTIONS
    {

#include <linker/rel-sections.ld>
#ifdef CONFIG_OPENAMP_RSC_TABLE

	SECTION_PROLOGUE(.resource_table,, SUBALIGN(4))
	{
		KEEP(*(.resource_table*))
  } > RTRAM AT > ROMABLE_REGION

#endif
}
