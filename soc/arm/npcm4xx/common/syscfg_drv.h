/**************************************************************************/
/* @file         syscfg_drv.h                                             */
/* @version      V1.00                                                    */
/* $Date:        2020/05/06 $                                            */
/* @brief        NCT6694D system configuration driver header file         */
/*                                                                        */
/* @note                                                                  */
/* Copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.       */
/**************************************************************************/

#ifndef __SYSCFG_DRV_H__
#define __SYSCFG_DRV_H__

/***** Definitions ********************************************************/
#define GPIO_RESET_BY_VSB                                                                          \
	RegSetBit(SCFG->DEVALT10,                                                                  \
		  SCFG_DEVALT10_GPIO_RST_SL_Msk) /* GPIO is reset by VSB Power-Up reset. */
#define GPIO_RESET_BY_CORE                                                                         \
	RegClrBit(SCFG->DEVALT10,                                                                  \
		  SCFG_DEVALT10_GPIO_RST_SL_Msk) /* GPIO is reset by Core domain reset. */

enum PIN_NAME_Enum {
	Pin_A1_RXD1,
	Pin_A1_CTSB,
	Pin_A1_PWM7,
	Pin_A1_GPIO00,

	Pin_A2_USB20_D_MINUS,

	Pin_A3_USB20_D_POSITIVE,

	Pin_A4_USBCLK,
	Pin_A4_GPIOA6,
	Pin_A4_TA6,

	Pin_A5_GPIO96,
	Pin_A5_SCL6A,
	Pin_A5_I3C1_SCL,

	Pin_A6_VIN14_THR14_TD0P,
	Pin_A6_GPIO92,

	pin_A7_CAN1_RX,
	pin_A7_GPIOF3,

	pin_A8_CAN0_RX,
	pin_A8_GPIOF1,

	Pin_A9_I3C2_SDA,
	Pin_A9_TA0,
	Pin_A9_GPIO83,

	Pin_A10_SPIP_DIO3,
	Pin_A10_GPIO77,
	Pin_A10_TRIST_STRAP,

	Pin_A11_WP_GPIO76,
	Pin_A11_SPIP_DIO2,
	Pin_A11_GPIO76,
	Pin_A11_TEST_STRAP,

	Pin_A12_GPIO73,
	Pin_A12_SPIP_CS,

	Pin_B1_EMAC_RXD0,
	Pin_B1_DRSB,
	Pin_B1_TA7,
	Pin_B1_GPIO01,

	Pin_B2_SCL3A,
	Pin_B2_GPIOA7,
	Pin_B2_CR_SIN,

	Pin_B3_SDA3A,
	Pin_B3_GPIOA2,
	Pin_B3_CR_SOUT,

	Pin_B4_GPIOA5,
	Pin_B4_PWM6,

	Pin_B5_GPIO97,
	Pin_B5_SDA6A,
	Pin_B5_PECI,
	Pin_B5_I3C1_SDA,

	Pin_B6_VIN1_THR1_TD3P,
	Pin_B6_DSADC_CLK,
	Pin_B6_GPIO93,

	pin_B7_CAN1_TX,
	pin_B7_GPIOF2,

	pin_B8_CAN0_TX,
	pin_B8_GPIOF0,

	Pin_B9_I3C2_SCL,
	Pin_B9_PWM0,
	Pin_B9_GPIO82,

	Pin_B10_PCHVSB, /* DSW_EN = 1 */
	Pin_B10_GPIO80, /* DSW_EN = 0 */

	Pin_B11_JEN_STRAP,
	Pin_B11_GPIO75,

	Pin_B12_SPIP_DIO1,
	Pin_B12_GPIO70,

	Pin_C1_EMAC_REF_CLK,
	Pin_C1_RTSB,
	Pin_C1_PWM8,
	Pin_C1_GPIO02,

	Pin_C2_EMAC_CRS_DV,
	Pin_C2_GPIOB0,
	Pin_C2_SMI,

	Pin_C3_EMAC_MDIO,
	Pin_C3_DCDB,
	Pin_C3_PWM9,
	Pin_C3_GPIO06,

	Pin_C4_GPIOA4,
	Pin_C4_TA5,

	Pin_C5_GPIOA0,
	Pin_C5_PWM4,

	Pin_C6_GPIO94,
	Pin_C6_VIN2_THR2_TD4P,

	Pin_C7_VIN15_THR15_TD1P,
	Pin_C7_GPIO91,

	Pin_C8_GPIO86,
	Pin_C8_VIN7,

	Pin_C9_SKTOCC,
	Pin_C9_GPIOE6,

	Pin_C10_GPIO81,
	Pin_C10_TA3,

	Pin_C11_GPIO67,
	Pin_C11_DSW_EN_STRAP,

	Pin_C12_SPIP_DIO0,
	Pin_C12_GPIO67,

	Pin_D1_EMAC_TXD1,
	Pin_D1_DTRB,
	Pin_D1_TA8,
	Pin_D1_GPIO03,

	Pin_D2_EMAC_TX_EN,
	Pin_D2_SOUTB,
	Pin_D2_CR_SOUT,
	Pin_D2_TA1,
	Pin_D2_GPIO05,

	Pin_D3_EMAC_MDC,
	Pin_D3_RIB,
	Pin_D3_TA9,
	Pin_D3_GPIO07,

	Pin_D4_GPIOA3,
	Pin_D4_PWM5,

	Pin_D5_GPIOA1,
	Pin_D5_TA4,

	Pin_D6_ATX_5VSB,
	Pin_D6_VIN3,
	Pin_D6_GPIO95,

	Pin_D7_VIN16_THR16_TD2P,
	Pin_D7_DSMOUT,
	Pin_D7_GPIO90,

	Pin_D8_RSMRST,
	Pin_D8_GPIOE5,

	Pin_D9_CASEOPEN,
	Pin_D9_GPIOE4,

	Pin_D10_GPIOD7,
	Pin_D10_CTSF,

	Pin_D11_GPIOD5,
	Pin_D11_SOUTF,

	Pin_D12_SPIP_CLK,
	Pin_D12_GPIO66,

	Pin_E1_EMAC_TXD0,
	Pin_E1_SINB,
	Pin_E1_CR_SIN,
	Pin_E1_PWM1,
	Pin_E1_GPIO04,

	Pin_E2_LPCPD,
	Pin_E2_LDRQ,
	Pin_E2_GPIOB2,

	pin_E3_ESPI_EN_STRAP,
	pin_E3_GPIOB4,

	pin_E4_MODE_STRAP2,
	Pin_E4_GPIOB5,

	Pin_E5_3VCC,

	Pin_E6_AVSS,

	Pin_E7_VREF,

	Pin_E8_VBAT,

	Pin_E9_FW_RDY,
	Pin_E9_GPIO72,

	Pin_E10_GPIOD6,
	Pin_E10_RTFS,

	Pin_E11_GPIOD4,
	Pin_E11_SINF,

	Pin_E12_RTC_XOUT,

	Pin_F1_GPIOB1,
	Pin_F1_CLKRUN,
	Pin_F1_ESPI_CS2,
	pin_F1_SHD_CS,

	Pin_F2_GPIOB3,
	Pin_F2_SERIRQ,

	Pin_F3_DSRA,
	Pin_F3_GPIO11,

	Pin_F4_CTSA,
	Pin_F4_GPIO10,

	Pin_F5_VSS,

	Pin_F6_VTT,

	Pin_F7_GPIO87,
	Pin_F7_VIN5,

	Pin_F8_VSS,

	Pin_F9_SLP_SUS,
	Pin_F9_GPIO71,
	Pin_F9_VSB3VSW,

	Pin_F10_GPIOD3,
	Pin_F10_CTSE,

	Pin_F11_GPIOD1,
	Pin_F11_SOUTE,

	Pin_F12_RTX_XIN,

	Pin_G1_ESPI_CLK,
	Pin_G1_LCLK,
	Pin_G1_SHD_SCLK,

	Pin_G2_GPIOB7,
	Pin_G2_PLTRST,

	Pin_G3_DTRA,
	Pin_G3_GPIO13,

	Pin_G4_2E4E_SEL_STRAP,
	Pin_G4_GPIO12,
	Pin_G4_RTSA,

	Pin_G5_VHIF,

	Pin_G6_PD4,
	Pin_G6_LED_C,
	Pin_G6_BEEP,
	Pin_G6_GPIO27,

	Pin_G7_AVSB,

	Pin_G8_3VSB,

	Pin_G9_SLP_S5,
	Pin_G9_VW_S5,
	Pin_G9_VW_S5_OUT,
	Pin_G9_VW_S5_GPIO65,

	Pin_G9_VW_S4,
	Pin_G9_VW_S4_OUT,
	Pin_G9_VW_S4_GPIO65,

	Pin_G10_GPIOD2,
	Pin_G10_RTSE,

	Pin_G11_GPIOD0,
	Pin_G11_SINE,

	Pin_G12_FLM_DIO3,
	Pin_G12_GPIO56,
	Pin_G12_SHD_DIO3,

	Pin_H1_eSPI_IO2,
	Pin_H1_LAD3,
	Pin_H1_SHD_DIO3,

	Pin_H2_DCDA,
	Pin_H2_GPIO16,

	Pin_H3_SOUTA,
	Pin_H3_CR_SOUT,
	Pin_H3_SOUTA_P80,
	Pin_H3_GPIO15,

	Pin_H4_GPIO14,
	Pin_H4_SINA,
	Pin_H4_CR_SIN,

	Pin_H5_GPIOE3,
	Pin_H5_MODE_STRAP1,

	Pin_H6_3VSB,

	Pin_H7_VCORE,

	Pin_H8_VSPI,

	Pin_H9_PWROK0,
	Pin_H9_GPIOE2,

	pin_H10_KBRST,
	pin_H10_GPIO57,

	Pin_H11_3VSBSW,
	Pin_H11_RESETCONI,
	Pin_H11_DPWROKI,
	Pin_H11_PWM3,
	Pin_H11_GPIO64,

	Pin_H12_FLM_DIO2,
	Pin_H12_GPIO55,
	Pin_H12_SHD_DIO2,
	Pin_H12_WP_GPIO55,

	Pin_J1_eSPI_IO2,
	Pin_J1_LAD2,
	Pin_J1_SHD_DIO2,

	Pin_J2_GPIOC3,
	Pin_J2_CTSC,

	Pin_J3_GPIOC7,
	Pin_J3_CTSD,

	Pin_J4_CLKOUT_LFCLK,
	Pin_J4_CLKOUT_CLK,
	Pin_J4_RIA,
	Pin_J4_GPIO17,

	Pin_J5_BUSY,
	Pin_J5_P2_DGL,
	Pin_J5_GPIO22,

	Pin_J6_PD3,
	Pin_J6_LED_D,
	Pin_J6_GPIO30,

	Pin_J7_PD1,
	Pin_J7_LED_F,
	Pin_J7_GPIO32,

	Pin_J8_PSIN,
	Pin_J8_GPIO51,

	Pin_J9_DEEP_S5_0,
	Pin_J9_VSB3VSW,
	Pin_J9_GPIOE0,

	Pin_J10_RSTOUT0,
	Pin_J10_GPIO62,

	Pin_J11_ATXPGD,
	Pin_J11_GPIO63,

	Pin_J12_FLM_SCLK,
	Pin_J12_GPIO47,
	Pin_J12_SHD_SCLK,

	Pin_K1_eSPI_IO1,
	Pin_K1_LDA1,
	Pin_K1_SHD_DIO1,

	Pin_K2_GPIOC2,
	Pin_K2_RTSC,

	Pin_K3_GPIOC6,
	Pin_K3_RTSD,

	Pin_K4_PVT_CS,
	Pin_K4_ACK,
	Pin_K4_GPIO23,

	Pin_K5_PD7,
	Pin_K5_P2_DGH,
	Pin_K5_GPIO24,

	Pin_K6_PD2,
	Pin_K6_LED_E,
	Pin_K6_GPIO31,

	Pin_K7_PD0,
	Pin_K7_LED_G,
	Pin_K7_GPIO33,

	Pin_K8_PSOUT,
	Pin_K8_GPIO50,

	Pin_K9_DPWROK,
	Pin_K9_GPIOE1,

	Pin_K10_RSTOUT2,
	Pin_K10_SCL1A,
	Pin_K10_GPIO60,

	Pin_K11_RSTOUT1,
	Pin_K11_SDA1A,
	Pin_K11_GPIO61,

	Pin_K12_FLM_DIO1,
	Pin_K12_GPIO46,
	Pin_K12_SHD_DIO1,

	Pin_L1_eSPI_IO0,
	Pin_L1_LAD0,
	Pin_L1_SHD_DIO0,

	Pin_L2_GPIOC1,
	Pin_L2_SOUTC,

	Pin_L3_GPIOC5,
	Pin_L3_SOUTD,

	Pin_L4_PE,
	Pin_L4_BACK_CS,
	Pin_L4_CIRRX,
	Pin_L4_GPIO21,

	Pin_L5_PD6,
	Pin_L5_LED_A,
	Pin_L5_SDA1B,
	Pin_L5_GPIO25,

	Pin_L6_CR_SOUT,
	Pin_L6_SLIN,
	Pin_L6_SCL5A,
	Pin_L6_GPIO34,

	Pin_L7_ERR,
	Pin_L7_VSBY_32KHZ_IN,
	Pin_L7_PWM2,
	Pin_L7_SCL4A,
	Pin_L7_GPIO36,

	Pin_L8_STB,
	Pin_L8_GRN_LED,
	Pin_L8_GPIOB6,

	Pin_L9_PSON,
	Pin_L9_GPIO52,

	Pin_L10_SLP_S3,
	Pin_L10_VW_S3,
	Pin_L10_VW_S3_OUT,
	Pin_L10_VW_S3_GPIO53,

	Pin_L11_FLM_CSI,
	Pin_L11_ECSCI,
	Pin_L11_PME,
	Pin_L11_GPIO54,

	Pin_L12_FLM_DIO0,
	Pin_L12_GPIO45,
	Pin_L12_SHD_DIO0,

	Pin_M1_eSPI_CS,
	Pin_M1_LFRAME,

	Pin_M2_GPIOC0,
	Pin_M2_SINC,

	Pin_M3_GPIOC4,
	Pin_M3_SIND,

	Pin_M4_14MCLKIN,
	Pin_M4_SLCT,
	Pin_M4_YLW_LED,
	Pin_M4_GPIO20,

	Pin_M5_PD5,
	Pin_M5_LED_B,
	Pin_M5_SCL1B,
	Pin_M5_GPIO26,

	Pin_M6_CR_SIN,
	Pin_M6_INIT,
	Pin_M6_P1_DGL,
	Pin_M6_SDA5A,
	Pin_M6_GPIO35,

	Pin_M7_AFD,
	Pin_M7_P1_DGH,
	Pin_M7_TA2,
	Pin_M7_SDA4A,
	Pin_M7_GPIO37,

	Pin_M8_JTAG_TDI,
	Pin_M8_DB_MOSI,
	Pin_M8_GPIO40,
	Pin_M8_MCLK,

	Pin_M9_JTAG_TCK,
	Pin_M9_DB_SCK,
	Pin_M9_GPIO41,
	Pin_M9_MDAT,

	Pin_M10_JTAG_TDO,
	Pin_M10_DB_MISO,
	Pin_M10_GPIO42,
	Pin_M10_KCLK,

	Pin_M11_JTAG_TMS,
	Pin_M11_DB_SCE,
	Pin_M11_GPIO43,
	Pin_M11_KDAT,

	Pin_M12_FLM_CSIO,
	Pin_M12_GPIO44,
	Pin_M12_SHD_CS,

	/* Combo */
	Pin_I2C1A,
	Pin_I2C1B,
	Pin_I2C3A,
	Pin_I2C4A,
	Pin_I2C5A,
	Pin_I2C6A,
	Pin_TSI,

	Pin_I3C1,
	Pin_I3C2,

	Pin_COMA,
	Pin_COMB,
	Pin_COMC,
	Pin_COMD,
	Pin_COME,
	Pin_COMF,

	Pin_PRT,

	Pin_SPIP,
	Pin_SHD_FIU,
	Pin_PVT_FIU,
	Pin_BACK_FIU,
	Pin_FLM,

	Pin_7SEG,

	Pin_USBSW_PCH,
	Pin_USBSW_eSIO,

	Pin_RMII,
	Pin_MDIO,

	Pin_CAN0,
	Pin_CAN1,
};

/***** Structure *********************************************************************************/

/***** API function ******************************************************************************/
/**
 * @brief                           Pin Selection
 * @param [in]      pin             Pin information
 * @return                          none
 */
void PinSelect(enum PIN_NAME_Enum pin);

#endif /* __SYSCFG_DRV_H__ */

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
