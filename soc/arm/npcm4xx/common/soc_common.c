/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <arch/arm/aarch32/cortex_m/cmsis.h>
#include <linker/linker-defs.h>

#ifdef CONFIG_XIP
__ramfunc uintptr_t npcm4xx_vector_table_save(void)
{
	extern char _npcm4xx_sram_vector_start[];
	uintptr_t vtor = 0;

	vtor = SCB->VTOR;
	SCB->VTOR = (uintptr_t)(_npcm4xx_sram_vector_start) & SCB_VTOR_TBLOFF_Msk;
	__DSB();
	__ISB();
	return vtor;
}

__ramfunc void npcm4xx_vector_table_restore(uintptr_t vtor)
{
	SCB->VTOR = vtor & SCB_VTOR_TBLOFF_Msk;
	__DSB();
	__ISB();
}

void npcm4xx_sram_vector_table_copy(void)
{
        extern char _npcm4xx_sram_vector_start[];
        extern char _npcm4xx_rom_vector_table_start[];
        extern char _npcm4xx_sram_vector_table_size[];

        /* copy sram vector table from rom to sram */
        (void *)memcpy(&_npcm4xx_sram_vector_start, &_npcm4xx_rom_vector_table_start,
                        (uint32_t)_npcm4xx_sram_vector_table_size);
}
#else /* !CONFIG_XIP */
uintptr_t npcm4xx_vector_table_save(void)
{
	return 0;
}

void npcm4xx_vector_table_restore(uintptr_t vtor)
{
	return;
}

void npcm4xx_sram_vector_table_copy(void)
{
	return;
}
#endif
