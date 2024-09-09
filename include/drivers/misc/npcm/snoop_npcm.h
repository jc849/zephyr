/*
 * Copyright (c) 2024 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MISC_SNOOP_NPCM_H_
#define ZEPHYR_INCLUDE_DRIVERS_MISC_SNOOP_NPCM_H_

/*
 * callback to handle snoop RX data
 * @snoop: pointer to the byte snooped by channel 0, NULL if no data
 * @len: data len
 */
typedef void snoop_npcm_rx_callback_t(const uint8_t *snoop, int len);

int snoop_npcm_register_rx_callback(const struct device *dev, snoop_npcm_rx_callback_t cb);

#endif
