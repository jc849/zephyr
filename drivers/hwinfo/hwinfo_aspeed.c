/*
 * Copyright (c) 2021 ASPEED Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <drivers/hwinfo.h>
#include <string.h>

/* SoC mapping Table */

#define ASPEED_REVISION_ID0	0x04
#define ASPEED_REVISION_ID1	0x14

#define SOC_ID(str, rev) { .name = str, .rev_id = rev, }

struct soc_id {
        const char *name;
        uint64_t rev_id;
};

static struct soc_id soc_map_table[] = {
        SOC_ID("AST1030-A0", 0x8000000080000000),
};

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	int i;
	uint32_t base = DT_REG_ADDR(DT_NODELABEL(syscon));
	uint64_t rev_id;

	rev_id = sys_read32(base + ASPEED_REVISION_ID0);
	rev_id = ((uint64_t)sys_read32(base + ASPEED_REVISION_ID1) << 32) |
		 rev_id;

	for (i = 0; i < ARRAY_SIZE(soc_map_table); i++) {
		if (rev_id == soc_map_table[i].rev_id)
			break;
	}

	if (i == ARRAY_SIZE(soc_map_table))
		printk("UnKnow-SOC: %llx\n", rev_id);
	else
		printk("SOC: %4s \n", soc_map_table[i].name);

	if (length > sizeof(rev_id)) {
		length = sizeof(rev_id);
	}

	memcpy(buffer, &rev_id, length);

	return length;
}
