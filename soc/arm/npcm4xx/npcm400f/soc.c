/*
 * Copyright (c) 2021 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(soc, CONFIG_SOC_LOG_LEVEL);

void z_platform_init(void)
{
	uintptr_t scfg_base = SCFG_BASE_ADDR;
	struct scfg_reg *inst_scfg = (struct scfg_reg *)scfg_base;

#ifdef CONFIG_GPIO_NPCM4XX_RESET_SL_POWER_UP
	inst_scfg->DEVALT10 = NPCM4XX_DEVALT10_CRGPIO_SELECT_SL_POWER;
#else
	inst_scfg->DEVALT10 = NPCM4XX_DEVALT10_CRGPIO_SELECT_SL_CORE;
#endif
}

static int soc_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	npcm4xx_sram_vector_table_copy();

	return 0;
}

SYS_INIT(soc_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
