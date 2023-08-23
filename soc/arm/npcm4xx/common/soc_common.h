/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* use for copy sram vector table from rom to sram */
void npcm4xx_sram_vector_table_copy(void);

/* use for replace/restore VTOR */
uintptr_t npcm4xx_vector_table_save(void);
void npcm4xx_vector_table_restore(uintptr_t vtor);
