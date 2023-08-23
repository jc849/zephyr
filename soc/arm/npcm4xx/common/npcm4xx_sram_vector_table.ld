/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
SECTION_PROLOGUE(.npcm4xx_sram_vector_table,,)
{
#if defined(CONFIG_CPU_CORTEX_M_HAS_VTOR)
/*
 * In an MCU with VTOR, the VTOR.TBLOFF is set to the start address of the
 * exc_vector_table (i.e. _vector_start) during initialization. Therefore,
 * exc_vector_table must respect the alignment requirements of VTOR.TBLOFF
 * described below.
 */

/* VTOR bits 0:6 are reserved (RES0). This requires that the base address
 * of the vector table is 32-word aligned.
 */
. = ALIGN( 1 << LOG2CEIL(4 * 32) );

/* When setting TBLOFF in VTOR we must align the offset to the number of
 * exception entries in the vector table. The minimum alignment of 32 words
 * is sufficient for the 16 ARM Core exceptions and up to 16 HW interrupts.
 * For more than 16 HW interrupts, we adjust the alignment by rounding up
 * to the next power of two; this restriction guarantees a functional VTOR
 * setting in any Cortex-M implementation (might not be required in every
 * Cortex-M processor).
 */
. = ALIGN( 1 << LOG2CEIL(4 * (16 + CONFIG_NUM_IRQS)) );
#endif

/* vector table from initial SP(0x00) to systick(0x3C) */
_npcm4xx_sram_vector_start = .;
KEEP(*(.npcm4xx_exc_sram_vector_table))
KEEP(*(".npcm4xx_exc_sram_vector_table.*"))
_npcm4xx_sram_vector_end = .;

/* irq vector table for NVIC interrupts */
_npcm4xx_sram_irq_vector_start = .;
. += (_vector_end - _vector_start) - (_npcm4xx_sram_vector_end - _npcm4xx_sram_vector_start);
_npcm4xx_sram_irq_vector_end = .;

/* implement vector function from reset(0x04) to systick(0x3C) */
_npcm4xx_sram_vector_func_start = .;
KEEP(*(.npcm4xx_sram_vector_func))
KEEP(*(".npcm4xx_sram_vector_func.*"))
_npcm4xx_sram_vector_func_end = .;

} GROUP_DATA_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

/* total sram vector table size = sram vector table + sram irq vector(empty now) +
 * sram vector function)
 */
_npcm4xx_sram_vector_table_size = _npcm4xx_sram_vector_func_end - _npcm4xx_sram_vector_start;

/* use for load code from rom to sram */
_npcm4xx_rom_vector_table_start = LOADADDR(.npcm4xx_sram_vector_table);
