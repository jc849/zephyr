/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* one bit per 4K, total 32 * 8 * 4K = 1024K MAX */
#define BITMAP_ARRAY_PER_SIZE	0x20
#define BITMAP_LIST_SIZE	0x8
struct npcm4xx_fw_write_bitmap
{
	uint32_t bitmap_lists[BITMAP_LIST_SIZE];
};

/* use for set new firwmare image spi nor address and partial write bitmaps */
uint8_t npcm4xx_set_update_fw_spi_nor_address(uint32_t fw_img_start,
	uint32_t fw_img_size, struct npcm4xx_fw_write_bitmap *bitmap);

/* use for total firmware update when system reboot */
extern void (*npcm4xx_spi_nor_do_fw_update)(int type);

/* use for npcm4xx driver to install npcm4xx_set_update_fw_spi_nor_address implement */
extern uint8_t (*npcm4xx_spi_nor_set_update_fw_address)(uint32_t fw_img_start,
		uint32_t fw_img_size, struct npcm4xx_fw_write_bitmap *bitmap);

/* use for copy sram vector table from rom to sram */
void npcm4xx_sram_vector_table_copy(void);

/* use for replace/restore VTOR */
uintptr_t npcm4xx_vector_table_save(void);
void npcm4xx_vector_table_restore(uintptr_t vtor);
