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
void (*npcm4xx_spi_nor_do_fw_update)(int type) = NULL;
uint8_t (*npcm4xx_spi_nor_set_update_fw_address)(uint32_t fw_img_start,
		uint32_t fw_img_size, struct npcm4xx_fw_write_bitmap *bitmap) = NULL;

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

__ramfunc uint8_t npcm4xx_set_update_fw_spi_nor_address(uint32_t fw_img_start,
                                                uint32_t fw_img_size,
                                                struct npcm4xx_fw_write_bitmap *bitmap)
{
	if (npcm4xx_spi_nor_set_update_fw_address)
		return npcm4xx_spi_nor_set_update_fw_address(fw_img_start, fw_img_size, bitmap);

	return -1;
}

__ramfunc void sys_arch_reboot(int type)
{
	uintptr_t vtor = 0;

	vtor = npcm4xx_vector_table_save();

	if (npcm4xx_spi_nor_do_fw_update)
		npcm4xx_spi_nor_do_fw_update(type);

	npcm4xx_vector_table_restore(vtor);

	NVIC_SystemReset();

	/* pending here */
	for (;;);
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

uint8_t npcm4xx_set_update_fw_spi_nor_address(uint32_t fw_img_start,
						uint32_t fw_img_size,
						struct npcm4xx_fw_write_bitmap *bitmap)
{
	return 0;
}
#endif
