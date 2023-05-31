/**************************************************************************/ /**
 * @copyright (C) 2021 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/

#ifndef __REGS_H__
#define __REGS_H__

#ifdef __cplusplus
#define __I volatile /*!< Defines 'read only' permissions                 */
#else
#define __I volatile const /*!< Defines 'read only' permissions                 */
#endif
#define __O volatile /*!< Defines 'write only' permissions                */
#define __IO volatile /*!< Defines 'read / write' permissions              */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

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

/*==========================================================================*/
/* CORE UNIVERSAL ASYNCHRONOUS RECEIVER-TRANSMITTER (CR_UART)               */
/*==========================================================================*/
/**                                                                         */
/*    @addtogroup CR_UART CORE UNIVERSAL ASYNCHRONOUS                       */
/*    RECEIVER-TRANSMITTER(CR_UART)                                         */
/*    Memory Mapped Structure for CR_UART Controller                        */
/* @{                                                                       */

struct UART_T {
	__IO uint8_t TBUF; /*!< [0x00]    Transmit Data Buffer    */
	uint8_t RESERVED1[1];

	__I uint8_t RBUF;  /*!< [0x02]    Receive Data Buffer     */
	uint8_t RESERVED2[1];
	__IO uint8_t ICTRL; /*!< [0x04]    Interrupt Control       */
	uint8_t RESERVED3[1];
	__I uint8_t STAT; /*!< [0x06]    Status                  */
	uint8_t RESERVED4[1];
	__IO uint8_t FRS; /*!< [0x08]    Frame Select            */
	uint8_t RESERVED5[1];
	__IO uint8_t MDSL; /*!< [0x0A]    Mode Select             */
	uint8_t RESERVED6[1];
	__IO uint8_t BAUD; /*!< [0x0C]    Baud Rate Divisor       */
	uint8_t RESERVED7[1];
	__IO uint8_t PSR; /*!< [0x0E]    Baud Rate Prescaler     */
	uint8_t RESERVED8[7];
	__IO uint8_t FCTRL; /*!< [0x16]    FIFO Control Register   */
	uint8_t RESERVED9[1];
	__IO uint8_t TXFLV; /*!< [0x18]    TX_FIFP Current Level Register */
	uint8_t RESERVEDA[1];
	__IO uint8_t RXFLV; /*!< [0x1A]    RX_FIFP Current Level Register */
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

/*==============================================================================*/
/* System Configuration Core Registers                                          */
/*==============================================================================*/
/**                                                                             */
/*    @addtogroup SCFG SYSTEM CONFIGURATION CORE REGISTERS (SCFG)               */
/*    Memory Mapped Structure for SCFG Controller                               */
/*@{                                                                            */

struct SCFG_T {
	__IO uint8_t DEVCNT; /*!< [0x00]    Device Control                      */
	__I uint8_t STRPST; /*!< [0x01]    Straps Status                        */
	__IO uint8_t RSTCTL; /*!< [0x02]    Reset Control and Status            */
	__IO uint8_t DEV_CTL2; /*!< [0x03]    Device Control 2                  */
	__IO uint8_t DEV_CTL3; /*!< [0x04]    Device Control 3                  */
	uint8_t RESERVED1[1];
	__IO uint8_t DEV_CTL4; /*!< [0x06]    Device Control 4                  */
	uint8_t RESERVED2[4];
	__IO uint8_t DEVALT10; /*!< [0x0B]    Device Alternate Function 10      */
	__IO uint8_t DEVALT11; /*!< [0x0C]    Device Alternate Function 11      */
	__I uint8_t DEVALT12; /*!< [0x0D]    Device Alternate Function 12       */
	uint8_t RESERVED3[2];
	__IO uint8_t DEVALT0; /*!< [0x10]    Device Alternate Function 0        */
	__IO uint8_t DEVALT1; /*!< [0x11]    Device Alternate Function 1        */
	__IO uint8_t DEVALT2; /*!< [0x12]    Device Alternate Function 2        */
	__IO uint8_t DEVALT3; /*!< [0x13]    Device Alternate Function 3        */
	__IO uint8_t DEVALT4; /*!< [0x14]    Device Alternate Function 4        */
	__IO uint8_t DEVALT5; /*!< [0x15]    Device Alternate Function 5        */
	__IO uint8_t DEVALT6; /*!< [0x16]    Device Alternate Function 6        */
	__IO uint8_t DEVALT7; /*!< [0x17]    Device Alternate Function 7        */
	__IO uint8_t DEVALT8; /*!< [0x18]    Device Alternate Function 8        */
	__IO uint8_t DEVALT9; /*!< [0x19]    Device Alternate Function 9        */
	__IO uint8_t DEVALTA; /*!< [0x1A]    Device Alternate Function A        */
	__IO uint8_t DEVALTB; /*!< [0x1B]    Device Alternate Function B        */
	__IO uint8_t DEVALTC; /*!< [0x1C]    Device Alternate Function C        */
	uint8_t RESERVED6[1];
	__IO uint8_t DEVALTE; /*!< [0x1E]    Device Alternate Function E        */
	uint8_t RESERVED7[3];
	__IO uint8_t DBGCTRL; /*!< [0x22]    Device Alternate Function E        */
	uint8_t RESERVED8[1];
	__IO uint8_t DEVALTCX; /*!< [0x24]    Device Alternate Function CX      */
	uint8_t RESERVED9[3];
	__IO uint8_t DEVPU0; /*!< [0x28]    Device Pull-Up Enable 0              */
	__IO uint8_t DEVPD1; /*!< [0x29]    Device Pull-Down Enable 1            */
	uint8_t RESERVED10[1];
	__IO uint8_t LV_CTL1; /*!< [0x2B]    Low-Voltage Pins Control 1 Register */
	uint8_t RESERVED11[2];
	__IO uint8_t DEVALT2E; /*!< [0x2E]    Device Alternate Function 2E       */
	uint8_t RESERVED12[1];
	__IO uint8_t DEVALT30; /*!< [0x30]    Device Alternate Function 30       */
	__IO uint8_t DEVALT31; /*!< [0x31]    Device Alternate Function 31       */
	__IO uint8_t DEVALT32; /*!< [0x32]    Device Alternate Function 32       */
	__IO uint8_t DEVALT33; /*!< [0x33]    Device Alternate Function 33       */
	__IO uint8_t DEVALT34; /*!< [0x34]    Device Alternate Function 34       */
	__IO uint8_t M4DIS; /*!< [0x35]    Cortex-M4 function Disable            */
	uint8_t RESERVED13[22];
	__IO uint8_t EMCPHY_CTL; /*!< [0x4C]    EMC Management Interface Control */
	__IO uint8_t EMC_CTL; /*!< [0x4D]    EMC Control                         */
	uint8_t RESERVED14[2];
	__IO uint8_t DEVALT50; /*!< [0x50]    ACPI_HW_SW_ctl_0                   */
	__IO uint8_t DEVALT51; /*!< [0x51]    ACPI_HW_SW_ctl_1                   */
	__IO uint8_t DEVALT52; /*!< [0x52]    ACPI_HW_SW_ctl_2                   */
	__IO uint8_t DEVALT53; /*!< [0x53]    ACPI_HW_SW_val_0                   */
	__IO uint8_t DEVALT54; /*!< [0x54]    ACPI_HW_SW_val_1                   */
	__IO uint8_t DEVALT55; /*!< [0x55]    ACPI_HW_SW_val_2                   */
	uint8_t RESERVED15[3];
	__IO uint8_t DEVALT59; /*!< [0x59]    ACPI_HW_OD_SEL_0                   */
	__IO uint8_t DEVALT5A; /*!< [0x5A]    ACPI_HW_OD_SEL_1                   */
	__IO uint8_t DEVALT5B; /*!< [0x5B]    ACPI_HW_OD_SEL_2                   */
	__IO uint8_t DEVALT5C; /*!< [0x5C]    ACPI_GPIO_SEL_0                    */
	__IO uint8_t DEVALT5D; /*!< [0x5D]    ACPI_GPIO_SEL_1                    */
	__IO uint8_t DEVALT5E; /*!< [0x5E]    ACPI_GPIO_SEL_2                    */
	__IO uint8_t DEVALT5F; /*!< [0x5F]    ACPI_GPIO_SEL_3                    */
	uint8_t RESERVED16[2];
	__IO uint8_t DEVALT62; /*!< [0x62]    Device Alternate Function 62       */
	uint8_t RESERVED17[3];
	__IO uint8_t DEVALT66; /*!< [0x66]    Device Alternate Function 66       */
	__IO uint8_t DEVALT67; /*!< [0x67]    Device Alternate Function 67       */
	uint8_t RESERVED18[14];
	__IO uint8_t DBGFRZEN1; /*!< [0x76]    Debug Freeze Enable 1             */
	__IO uint8_t DBGFRZEN2; /*!< [0x77]    Debug Freeze Enable 2             */
	__IO uint8_t DBGFRZEN3; /*!< [0x78]    Debug Freeze Enable 3             */
	__IO uint8_t DBGFRZEN4; /*!< [0x79]    Debug Freeze Enable 4             */
};

/*=================================================================================*/
/* HIGH-FREQUENCY CLOCK GENERATOR (HFCG)                                           */
/*=================================================================================*/
/**                                                                                */
/*    @addtogroup HFCG HIGH-FREQUENCY CLOCK GENERATOR (HFCG)                       */
/*    Memory Mapped Structure for HFCG Controller                                  */
/*@{                                                                               */

struct HFCG_T {
	__IO uint8_t CTRL; /*!< [0x00]    HFCG Control                 */
	uint8_t RESERVED1[1];
	__IO uint8_t ML; /*!< [0x02]    HFCG M Low Byte Value        */
	uint8_t RESERVED2[1];
	__IO uint8_t MH; /*!< [0x04]    HFCG M High Byte Value       */
	uint8_t RESERVED3[1];
	__IO uint8_t N; /*!< [0x06]    HFCG N Value                 */
	uint8_t RESERVED4[1];
	__IO uint8_t P; /*!< [0x08]    HFCG Prescaler               */
	uint8_t RESERVED5[7];
	__IO uint8_t BCD; /*!< [0x10]    HFCG Bus Clock Dividers      */
	uint8_t RESERVED6[1];
	__IO uint8_t BCD1; /*!< [0x12]    HFCG Bus Clock Dividers 1    */
	uint8_t RESERVED7[1];
	__IO uint8_t BCD2; /*!< [0x14]    HFCG Bus Clock Dividers 2    */
};

/*--------------------------*/
/* HFCGN fields             */
/*--------------------------*/
#define HFCG_N_XF_RANGE_Pos (7)
#define HFCG_N_XF_RANGE_Msk (0x1 << HFCG_N_XF_RANGE_Pos)

#define HFCG_N_HFCGN5_0_Pos (0)
#define HFCG_N_HFCGN5_0_Msk (0x3F << HFCG_N_HFCGN5_0_Pos)

/*--------------------------*/
/* HFCBCD fields            */
/*--------------------------*/
#define HFCG_BCD_APB2DIV_Pos (4)
#define HFCG_BCD_APB2DIV_Msk (0xF << HFCG_BCD_APB2DIV_Pos)

#define HFCG_BCD_APB1DIV_Pos (0)
#define HFCG_BCD_APB1DIV_Msk (0xF << HFCG_BCD_APB1DIV_Pos)

/*--------------------------*/
/* UICTRL fields            */
/*--------------------------*/
#define UART_ICTRL_EEI_Pos (7)
#define UART_ICTRL_EEI_Msk (0x1 << UART_ICTRL_EEI_Pos)

#define UART_ICTRL_ERI_Pos (6)
#define UART_ICTRL_ERI_Msk (0x1 << UART_ICTRL_ERI_Pos)

#define UART_ICTRL_ETI_Pos (5)
#define UART_ICTRL_ETI_Msk (0x1 << UART_ICTRL_ETI_Pos)

#define UART_ICTRL_RBF_Pos (1)
#define UART_ICTRL_RBF_Msk (0x1 << UART_ICTRL_RBF_Pos)

#define UART_ICTRL_TBE_Pos (0)
#define UART_ICTRL_TBE_Msk (0x1 << UART_ICTRL_TBE_Pos)

/** @addtogroup NCT6694D_PERIPHERAL_MEM_MAP NCT6694D Peripheral Memory Base */
/*  Memory Mapped Structure for NCT6694D Peripheral                         */
/*  @{                                                                      */
/****************************************************************************/
/*                   Base Address of NCT6694D Modules                       */
/****************************************************************************/
#define SCFG_BASE_ADDR (0x400C3000)

#define ONCHIP_BASE ((uint32_t)0x00fff000)
#define CP_CONTROL_BASE (ONCHIP_BASE + 0x880UL)

#define CP_CONTROL (struct CP_CONTROL_T *)(CP_CONTROL_BASE)
#define GLBLEN_FIELD FIELD(8, 1)

#define GCR_BASE ((uint32_t)0xf0800000)
#define GCR (struct GCR_T *)(GCR_BASE)
#define CP1URXDSEL_FIELD FIELD(31, 1)
#define CP1UTXDSEL_FIELD FIELD(1, 1)

#define UART_BASE_ADDR (0x400C4000)
#define UART ((struct UART_T *)UART_BASE_ADDR)
#define LCR_MASK FIELD(0, 8)
#define IER_MASK FIELD(0, 8)
#define DLAB_FIELD FIELD(7, 1)
#define WLS_FIELD FIELD(0, 2)
#define NSB_FIELD FIELD(2, 1)
#define PBE_FIELD FIELD(3, 1)
#define THRE_FIELD FIELD(5, 1)

#define HFCG_BASE_ADDR (0x400B5000)
#define HFCG ((struct HFCG_T *)HFCG_BASE_ADDR)

#define SCFG ((struct SCFG_T *)SCFG_BASE_ADDR)

#define CP_TWD_BASE (ONCHIP_BASE + 0x800UL)
#define CP_TWD (struct CP_TWD_T *)(CP_TWD_BASE)

#define CLK_BASE ((uint32_t)0xf0801000)
#define CLK (struct CLK_T *)(CLK_BASE)
#define UARTDIV2_FIELD FIELD(11, 5)

#ifdef __cplusplus
}
#endif

#endif /* __REGS_H__ */
