# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(${ZEPHYR_BASE}/drivers)
zephyr_include_directories(${ZEPHYR_BASE}/soc/arm/common/cortex_m)

zephyr_sources(
  soc.c
  gdma.c
)

zephyr_sources_ifdef(
  CONFIG_ARM_MPU
  mpu_regions.c
)
