/*
 * Copyright (c) 2023 Nuvoton Technology Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Populated vector table in RAM(SRAM)
 *
 * Vector table at the beginning of the image for starting system. The reset
 * vector is the system entry point, ie. the first instruction executed.
 *
 * The table is populated with all the system exception handlers. The NMI vector
 * must be populated with a valid handler since it can happen at any time. The
 * rest should not be triggered until the kernel is ready to handle them.
 */

#include <toolchain.h>
#include <linker/sections.h>
#include "npcm4xx_sram_vector_table.h"

_ASM_FILE_PROLOGUE

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_reset,z_arm_npcm4xx_reset)
    wfi
    b z_arm_npcm4xx_reset

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_nmi,z_arm_npcm4xx_nmi)
    wfi
    b z_arm_npcm4xx_nmi

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_fault,z_arm_npcm4xx_hard_fault)
    wfi
    b z_arm_npcm4xx_hard_fault

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_fault,z_arm_npcm4xx_mpu_fault)
    wfi
    b z_arm_npcm4xx_mpu_fault

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_fault,z_arm_npcm4xx_bus_fault)
    wfi

    b z_arm_npcm4xx_bus_fault

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_fault,z_arm_npcm4xx_usage_fault)
    wfi
    b z_arm_npcm4xx_usage_fault

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_svc,z_arm_npcm4xx_svc)
    wfi
    b z_arm_npcm4xx_svc

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_debug_monitor,z_arm_npcm4xx_debug_monitor)
    wfi
    b z_arm_npcm4xx_debug_monitor

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_pendsv,z_arm_npcm4xx_pendsv)
    wfi
    b z_arm_npcm4xx_pendsv

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_sys_clock_isr,z_arm_npcm4xx_sys_clock_isr)
    wfi
    b z_arm_npcm4xx_sys_clock_isr

SECTION_SUBSEC_FUNC(npcm4xx_sram_vector_func,__npcm4xx_fault,z_arm_npcm4xx_exc_spurious)
    wfi
    b z_arm_npcm4xx_exc_spurious

SECTION_SUBSEC_FUNC(npcm4xx_exc_sram_vector_table,_npcm4xx_sram_vector_table_section,_npcm4xx_sram_vector_table)

    /*
     * setting the _very_ early boot on the main stack allows to use memset
     * on the interrupt stack when CONFIG_INIT_STACKS is enabled before
     * switching to the interrupt stack for the rest of the early boot
     */
    .word z_main_stack + CONFIG_MAIN_STACK_SIZE

    .word z_arm_npcm4xx_reset
    .word z_arm_npcm4xx_nmi

    .word z_arm_npcm4xx_hard_fault
    .word z_arm_npcm4xx_mpu_fault
    .word z_arm_npcm4xx_bus_fault
    .word z_arm_npcm4xx_usage_fault
    .word 0
    .word 0
    .word 0
    .word 0
    .word z_arm_npcm4xx_svc
    .word z_arm_npcm4xx_debug_monitor
    .word 0
#if defined(CONFIG_MULTITHREADING)
    .word z_arm_npcm4xx_pendsv
#else
    .word 0
#endif
#if defined(CONFIG_CPU_CORTEX_M_HAS_SYSTICK)
#if defined(CONFIG_SYS_CLOCK_EXISTS)
    /* Install sys_clock_isr even if CORTEX_M_SYSTICK is not set
     * (e.g. to support out-of-tree SysTick-based timer drivers).
     */
    .word z_arm_npcm4xx_sys_clock_isr
#else
    .word z_arm_npcm4xx_exc_spurious
#endif /* CONFIG_SYS_CLOCK_EXISTS */
#else
    .word 0
#endif /* CONFIG_CPU_CORTEX_M_HAS_SYSTICK */
