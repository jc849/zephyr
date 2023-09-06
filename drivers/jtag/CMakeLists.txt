# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_JTAG_SHELL		jtag_shell.c)
zephyr_library_sources_ifdef(CONFIG_JTAG_NPCM4XX	jtag_npcm4xx.c)
