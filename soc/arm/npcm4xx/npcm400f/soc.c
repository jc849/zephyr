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

uintptr_t scfg_base = DT_REG_ADDR_BY_IDX(DT_NODELABEL(scfg), 0);

void z_platform_init(void)
{
	struct scfg_reg *inst_scfg = (struct scfg_reg *)scfg_base;

	if (scfg_base) {
#if CONFIG_GPIO_NPCM4XX_RESET_SL_POWER_UP
		inst_scfg->DEVALT10 = NPCM4XX_DEVALT10_CRGPIO_SELECT_SL_POWER;
#else
		inst_scfg->DEVALT10 = NPCM4XX_DEVALT10_CRGPIO_SELECT_SL_CORE;
#endif
	}
}

static int soc_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	npcm4xx_sram_vector_table_copy();

	return 0;
}

SYS_INIT(soc_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
