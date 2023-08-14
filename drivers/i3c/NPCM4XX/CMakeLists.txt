# Copyright (c) 2023 Nuvoton Technology Corporation.
# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_I3C_NPCM4XX	        i3c_core.c)
zephyr_sources_ifdef(CONFIG_I3C_NPCM4XX	        i3c_master.c)
zephyr_sources_ifdef(CONFIG_I3C_NPCM4XX	        i3c_slave.c)
zephyr_sources_ifdef(CONFIG_I3C_NPCM4XX	        api_i3c.c)