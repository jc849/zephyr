/*
 * Copyright (c) 2021 Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _IPMI_KCS_NPCM4XX_H
#define _IPMI_KCS_NPCM4XX_H

int kcs_nuvoton_read(const struct device *dev,
		    uint8_t *buf, uint32_t buf_sz);
int kcs_nuvoton_write(const struct device *dev,
		     uint8_t *buf, uint32_t buf_sz);

#endif
