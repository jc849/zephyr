# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

zephyr_sources(
  header.c
  soc_common.c
)

zephyr_sources_ifdef(CONFIG_XIP
  npcm4xx_sram_vector_table.S
)

zephyr_linker_sources_ifdef(CONFIG_XIP
  RAM_SECTIONS
  SORT_KEY 0
  npcm4xx_sram_vector_table.ld
)
