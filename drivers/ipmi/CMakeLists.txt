# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_IPMI_KCS_NPCM4XX	kcs_npcm4xx.c)
