/**************************************************************************/ /**
 * SPDX-License-Identifier: Apache-2.0
 * @copyright (C) 2021 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#ifndef __REGS_H__
#define __REGS_H__

#ifdef __cplusplus
extern "C" {
#endif

struct CP_CONTROL_T {
	volatile uint16_t B2CPST0;
	volatile uint16_t B2CPST1;
	volatile uint16_t CP2BNT0;
	volatile uint16_t CP2BNT1;
	volatile uint16_t CPSTATR;
	volatile uint16_t CPCFGR;
	volatile uint16_t LKREG1;
	volatile uint16_t LKREG2;
	volatile uint16_t CP2TIP_INT;
	volatile uint16_t _reserved;
	volatile uint16_t SRAMWINC;
	volatile uint16_t SPI01WINC;
	volatile uint16_t SPI3WINC;
	volatile uint16_t SPIXWINC;
	volatile uint16_t MISCWINC;
};

struct GCR_T {
	volatile uint8_t _res1[0x273];
	volatile uint32_t MFSEL6;
};

struct UART_T {
	volatile uint8_t DAT; /* /DLAB:0 RBR/THR /DLAB:1 DLL */
	volatile uint8_t _res1[3];
	volatile uint8_t IER; /* /DLAB:0 IER /DLAB:1 DLM */
	volatile uint8_t _res2[3];
	volatile uint8_t IIR; /* /IIR/FCR */
	volatile uint8_t _res3[3];
	volatile uint8_t LCR;
	volatile uint8_t _res4[3];
	volatile uint8_t MCR;
	volatile uint8_t _res5[3];
	volatile uint8_t LSR;
	volatile uint8_t _res6[3];
	volatile uint8_t MSR;
	volatile uint8_t _res7[3];
	volatile uint8_t TOR;
};

struct CP_TWD_T {
	volatile uint8_t TWCFG;
	volatile uint8_t _res1;
	volatile uint8_t TWCP;
	volatile uint8_t _res2;
	volatile uint16_t TWDT0;
	volatile uint8_t T0CSR;
	volatile uint8_t _res3;
	volatile uint8_t WDCNT;
	volatile uint8_t _res4;
	volatile uint8_t WDSDM;
};

struct CLK_T {
	volatile uint8_t _res1[0x57];
	volatile uint32_t CLKDIV3;
};

#define ONCHIP_BASE ((uint32_t)0x00fff000)
#define CP_CONTROL_BASE (ONCHIP_BASE + 0x880UL)

#define CP_CONTROL (struct CP_CONTROL_T *)(CP_CONTROL_BASE)
#define GLBLEN_FIELD FIELD(8, 1)

#define GCR_BASE ((uint32_t)0xf0800000)
#define GCR (struct GCR_T *)(GCR_BASE)
#define CP1URXDSEL_FIELD FIELD(31, 1)
#define CP1UTXDSEL_FIELD FIELD(1, 1)

#define CP_UART_BASE (ONCHIP_BASE + 0x900UL)
#define CP_UART (struct UART_T *)(CP_UART_BASE)
#define LCR_MASK FIELD(0, 8)
#define IER_MASK FIELD(0, 8)
#define DLAB_FIELD FIELD(7, 1)
#define WLS_FIELD FIELD(0, 2)
#define NSB_FIELD FIELD(2, 1)
#define PBE_FIELD FIELD(3, 1)
#define THRE_FIELD FIELD(5, 1)

#define CP_TWD_BASE (ONCHIP_BASE + 0x800UL)
#define CP_TWD (struct CP_TWD_T *)(CP_TWD_BASE)

#define CLK_BASE ((uint32_t)0xf0801000)
#define CLK (struct CLK_T *)(CLK_BASE)
#define UARTDIV2_FIELD FIELD(11, 5)

#ifdef __cplusplus
}
#endif

#endif /* __REGS_H__ */
