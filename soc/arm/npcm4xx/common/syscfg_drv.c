/**************************************************************************/
/* @file        syscfg_drv.c                                              */
/* @version     V1.00                                                     */
/* $Date:       2022/05/20 $                                              */
/* @brief       NCT6694D system configuration driver source file          */
/*                                                                        */
/* @note                                                                  */
/* Copyright (C) 2022 Nuvoton Technology Corp. All rights reserved.       */
/**************************************************************************/

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <logging/log.h>
#include "NPCM4XX.h"
#include "reg_access.h"
#include "syscfg_drv.h"

/*------------------------------------------------------------------------*/
/**
 * @brief                           Pin Selection
 * @param [in]      pin             Pin information
 * @return                          none
 */
/*------------------------------------------------------------------------*/
void PinSelect(enum PIN_NAME_Enum pin)
{
	switch (pin) {
	case Pin_A1_RXD1:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_A1_CTSB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT4);
		break;
	case Pin_A1_PWM7:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT4);
		RegSetBit(SCFG->DEVALT5, BIT1);
		break;
	case Pin_A1_GPIO00:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT4);
		RegClrBit(SCFG->DEVALT5, BIT1);
		break;

	case Pin_A4_GPIOA6:
		RegClrBit(SCFG->DEVALT3, BIT6);
		break;
	case Pin_A4_TA6:
		RegSetBit(SCFG->DEVALT3, BIT6);
		break;

	case Pin_A5_GPIO96:
		RegClrBit(SCFG->DEVALT7, BIT2);
		RegSetBit(SCFG->DEVALT7, BIT3);
		break;
	case Pin_A5_SCL6A:
		RegClrBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT3);
		break;
	case Pin_A5_I3C1_SCL:
		RegSetBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT3);
		break;

	case Pin_A6_VIN14_THR14_TD0P:
		RegSetBit(SCFG->DEVALT2, BIT0);
		break;
	case Pin_A6_GPIO92:
		RegClrBit(SCFG->DEVALT2, BIT0);
		RegClrBit(SCFG->DEVALT2, BIT3);
		break;

	case pin_A7_CAN1_RX:
		RegSetBit(SCFG->DEVALT34, BIT7);
		break;
	case pin_A7_GPIOF3:
		RegClrBit(SCFG->DEVALT34, BIT7);
		break;

	case pin_A8_CAN0_RX:
		RegSetBit(SCFG->DEVALT34, BIT6);
		break;
	case pin_A8_GPIOF1:
		RegClrBit(SCFG->DEVALT34, BIT6);
		break;

	case Pin_A9_I3C2_SDA:
		RegSetBit(SCFG->DEVALTA, BIT4);
		break;
	case Pin_A9_TA0:
		RegClrBit(SCFG->DEVALTA, BIT4);
		RegSetBit(SCFG->DEVALT3, BIT0);
		break;
	case Pin_A9_GPIO83:
		RegClrBit(SCFG->DEVALTA, BIT4);
		RegClrBit(SCFG->DEVALT3, BIT0);
		break;

	case Pin_A10_SPIP_DIO3:
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_A10_GPIO77:
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_A11_WP_GPIO76:
		RegSetBit(SCFG->DEV_CTL3, BIT2);
		break;
	case Pin_A11_SPIP_DIO2:
		RegClrBit(SCFG->DEV_CTL3, BIT2);
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_A11_GPIO76:
		RegClrBit(SCFG->DEV_CTL3, BIT2);
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_A12_SPIP_CS:
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_A12_GPIO73:
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_B1_EMAC_RXD0:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_B1_DRSB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT3);
		break;
	case Pin_B1_TA7:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegSetBit(SCFG->DEVALT3, BIT7);
		break;
	case Pin_B1_GPIO01:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegClrBit(SCFG->DEVALT3, BIT7);
		break;

	case Pin_B2_CR_SIN:
		RegSetBit(SCFG->DEVALT31, BIT3);
		break;
	case Pin_B2_SCL3A:
		RegClrBit(SCFG->DEVALT31, BIT3);
		RegSetBit(SCFG->DEVALTA, BIT1);
		break;
	case Pin_B2_GPIOA7:
		RegClrBit(SCFG->DEVALT31, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT1);
		break;

	case Pin_B3_CR_SOUT:
		RegSetBit(SCFG->DEVALT31, BIT2);
		break;
	case Pin_B3_SDA3A:
		RegClrBit(SCFG->DEVALT31, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT1);
		break;
	case Pin_B3_GPIOA2:
		RegClrBit(SCFG->DEVALT31, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT1);
		break;

	case Pin_B4_GPIOA5:
		RegClrBit(SCFG->DEVALT5, BIT0);
		break;
	case Pin_B4_PWM6:
		RegSetBit(SCFG->DEVALT5, BIT0);
		break;

	case Pin_B5_GPIO97:
		RegClrBit(SCFG->DEVALT7, BIT2);
		RegSetBit(SCFG->DEVALT7, BIT4);
		RegClrBit(SCFG->DEVALT5, BIT4);
		break;
	case Pin_B5_SDA6A:
		RegClrBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT4);
		RegClrBit(SCFG->DEVALT5, BIT4);
		break;
	case Pin_B5_PECI:
		RegClrBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT4);
		RegSetBit(SCFG->DEVALT5, BIT4);
		break;
	case Pin_B5_I3C1_SDA:
		RegSetBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT4);
		RegClrBit(SCFG->DEVALT5, BIT4);
		break;

	case Pin_B6_VIN1_THR1_TD3P:
		RegSetBit(SCFG->DEVALT1, BIT1);
		break;
	case Pin_B6_DSADC_CLK:
		RegClrBit(SCFG->DEVALT1, BIT1);
		RegSetBit(SCFG->DEVALT2, BIT3);
		break;
	case Pin_B6_GPIO93:
		RegClrBit(SCFG->DEVALT1, BIT1);
		RegClrBit(SCFG->DEVALT2, BIT3);
		break;

	case pin_B7_CAN1_TX:
		RegSetBit(SCFG->DEVALT34, BIT7);
		break;
	case pin_B7_GPIOF2:
		RegClrBit(SCFG->DEVALT34, BIT7);
		break;

	case pin_B8_CAN0_TX:
		RegSetBit(SCFG->DEVALT34, BIT6);
		break;
	case pin_B8_GPIOF0:
		RegClrBit(SCFG->DEVALT34, BIT6);
		break;

	case Pin_B9_I3C2_SCL:
		RegSetBit(SCFG->DEVALTA, BIT4);
		break;
	case Pin_B9_PWM0:
		RegClrBit(SCFG->DEVALTA, BIT4);
		RegSetBit(SCFG->DEVALT4, BIT2);
		break;
	case Pin_B9_GPIO82:
		RegClrBit(SCFG->DEVALTA, BIT4);
		RegClrBit(SCFG->DEVALT4, BIT2);
		break;

	case Pin_B10_PCHVSB:
		RegClrBit(SCFG->DEVALTE, BIT0);
		break;
	case Pin_B10_GPIO80:
		RegClrBit(SCFG->DEVALTE, BIT0);
		break;

	case Pin_B12_SPIP_DIO1:
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_B12_GPIO70:
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_C1_EMAC_REF_CLK:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_C1_RTSB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT0);
		break;
	case Pin_C1_PWM8:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT0);
		RegSetBit(SCFG->DEVALT5, BIT2);
		break;
	case Pin_C1_GPIO02:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT0);
		RegClrBit(SCFG->DEVALT5, BIT2);
		break;

	case Pin_C2_EMAC_CRS_DV:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_C2_SMI:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT5, BIT6);
		break;
	case Pin_C2_GPIOB0:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT5, BIT6);
		break;

	case Pin_C3_EMAC_MDIO:
		RegSetBit(SCFG->EMC_CTL, BIT1);
		break;
	case Pin_C3_DCDB:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegSetBit(SCFG->DEVALT9, BIT3);
		break;
	case Pin_C3_PWM9:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegSetBit(SCFG->DEVALT5, BIT3);
		break;
	case Pin_C3_GPIO06:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegClrBit(SCFG->DEVALT5, BIT3);
		break;

	case Pin_C4_GPIOA4:
		RegClrBit(SCFG->DEVALT3, BIT5);
		break;
	case Pin_C4_TA5:
		RegSetBit(SCFG->DEVALT3, BIT5);
		break;

	case Pin_C5_GPIOA0:
		RegClrBit(SCFG->DEVALT4, BIT6);
		break;
	case Pin_C5_PWM4:
		RegSetBit(SCFG->DEVALT4, BIT6);
		break;

	case Pin_C6_GPIO94:
		RegClrBit(SCFG->DEVALT1, BIT2);
		break;
	case Pin_C6_VIN2_THR2_TD4P:
		RegSetBit(SCFG->DEVALT1, BIT2);
		break;

	case Pin_C7_VIN15_THR15_TD1P:
		RegSetBit(SCFG->DEVALT2, BIT1);
		break;
	case Pin_C7_GPIO91:
		RegClrBit(SCFG->DEVALT2, BIT1);
		RegClrBit(SCFG->DEVALT2, BIT3);
		break;

	case Pin_C8_GPIO86:
		RegClrBit(SCFG->DEVALT1, BIT7);
		break;
	case Pin_C8_VIN7:
		RegSetBit(SCFG->DEVALT1, BIT7);
		break;

	case Pin_C9_SKTOCC:
		RegClrBit(SCFG->DEVALT5D, BIT4);
		break;
	case Pin_C9_GPIOE6:
		RegSetBit(SCFG->DEVALT5D, BIT4);
		break;

	case Pin_C10_GPIO81:
		RegClrBit(SCFG->DEVALT3, BIT3);
		break;
	case Pin_C10_TA3:
		RegSetBit(SCFG->DEVALT3, BIT3);
		break;

	case Pin_C12_SPIP_DIO0:
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_C12_GPIO67:
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_D1_EMAC_TXD1:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_D1_DTRB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT3);
		break;
	case Pin_D1_TA8:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegSetBit(SCFG->DEVALT4, BIT0);
		break;
	case Pin_D1_GPIO03:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT3);
		RegClrBit(SCFG->DEVALT4, BIT0);
		break;

	case Pin_D2_EMAC_TX_EN:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_D2_SOUTB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT2);
		break;
	case Pin_D2_CR_SOUT:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT6);
		break;
	case Pin_D2_TA1:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT6);
		RegSetBit(SCFG->DEVALT3, BIT1);
		break;
	case Pin_D2_GPIO05:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT6);
		RegClrBit(SCFG->DEVALT3, BIT1);
		break;

	case Pin_D3_EMAC_MDC:
		RegSetBit(SCFG->EMC_CTL, BIT1);
		break;
	case Pin_D3_RIB:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegSetBit(SCFG->DEVALT9, BIT5);
		break;
	case Pin_D3_TA9:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegClrBit(SCFG->DEVALT9, BIT5);
		RegSetBit(SCFG->DEVALT4, BIT1);
		break;
	case Pin_D3_GPIO07:
		RegClrBit(SCFG->EMC_CTL, BIT1);
		RegClrBit(SCFG->DEVALT9, BIT5);
		RegClrBit(SCFG->DEVALT4, BIT1);
		break;

	case Pin_D4_GPIOA3:
		RegClrBit(SCFG->DEVALT4, BIT7);
		break;
	case Pin_D4_PWM5:
		RegSetBit(SCFG->DEVALT4, BIT7);
		break;

	case Pin_D5_GPIOA1:
		RegClrBit(SCFG->DEVALT3, BIT4);
		break;
	case Pin_D5_TA4:
		RegSetBit(SCFG->DEVALT3, BIT4);
		break;

	case Pin_D6_ATX_5VSB:
		RegSetBit(SCFG->DEVALT1, BIT4);
		break;
	case Pin_D6_VIN3:
		RegClrBit(SCFG->DEVALT1, BIT4);
		RegSetBit(SCFG->DEVALT1, BIT3);
		break;
	case Pin_D6_GPIO95:
		RegClrBit(SCFG->DEVALT1, BIT4);
		RegClrBit(SCFG->DEVALT1, BIT3);
		break;

	case Pin_D7_VIN16_THR16_TD2P:
		RegSetBit(SCFG->DEVALT2, BIT2);
		break;
	case Pin_D7_DSMOUT:
		RegClrBit(SCFG->DEVALT2, BIT2);
		RegSetBit(SCFG->DEVALT2, BIT3);
		break;
	case Pin_D7_GPIO90:
		RegClrBit(SCFG->DEVALT2, BIT2);
		RegClrBit(SCFG->DEVALT2, BIT3);
		break;

	case Pin_D8_RSMRST:
		RegClrBit(SCFG->DEVALT5E, BIT1);
		break;
	case Pin_D8_GPIOE5:
		RegSetBit(SCFG->DEVALT5E, BIT1);
		break;

	case Pin_D9_CASEOPEN:
		RegClrBit(SCFG->DEVALT5D, BIT5);
		break;
	case Pin_D9_GPIOE4:
		RegSetBit(SCFG->DEVALT5D, BIT5);
		break;

	case Pin_D10_GPIOD7:
		RegClrBit(SCFG->DEVALT66, BIT7);
		break;
	case Pin_D10_CTSF:
		RegSetBit(SCFG->DEVALT66, BIT7);
		break;

	case Pin_D11_GPIOD5:
		RegClrBit(SCFG->DEVALT66, BIT5);
		break;
	case Pin_D11_SOUTF:
		RegSetBit(SCFG->DEVALT66, BIT5);
		break;

	case Pin_D12_SPIP_CLK:
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;
	case Pin_D12_GPIO66:
		RegClrBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_E1_EMAC_TXD0:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_E1_SINB:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegSetBit(SCFG->DEVALT9, BIT1);
		break;
	case Pin_E1_CR_SIN:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT1);
		RegSetBit(SCFG->DEVALTB, BIT1);
		break;
	case Pin_E1_PWM1:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT1);
		RegClrBit(SCFG->DEVALTB, BIT1);
		RegSetBit(SCFG->DEVALT4, BIT3);
		break;
	case Pin_E1_GPIO04:
		RegClrBit(SCFG->EMC_CTL, BIT0);
		RegClrBit(SCFG->DEVALT9, BIT1);
		RegClrBit(SCFG->DEVALTB, BIT1);
		RegClrBit(SCFG->DEVALT4, BIT3);
		break;

	case Pin_E2_LPCPD:
		RegSetBit(SCFG->DEVALT0, BIT6);
		break;
	case Pin_E2_LDRQ:
		RegClrBit(SCFG->DEVALT0, BIT6);
		RegSetBit(SCFG->DEVALTB, BIT7);
		break;
	case Pin_E2_GPIOB2:
		RegClrBit(SCFG->DEVALT0, BIT6);
		RegClrBit(SCFG->DEVALTB, BIT7);
		break;

	case Pin_E9_FW_RDY:
		RegClrBit(SCFG->DEVALT7, BIT0);
		break;
	case Pin_E9_GPIO72:
		RegSetBit(SCFG->DEVALT7, BIT0);
		break;

	case Pin_E10_GPIOD6:
		RegClrBit(SCFG->DEVALT66, BIT6);
		break;
	case Pin_E10_RTFS:
		RegSetBit(SCFG->DEVALT66, BIT6);
		break;

	case Pin_E11_GPIOD4:
		RegClrBit(SCFG->DEVALT66, BIT4);
		break;
	case Pin_E11_SINF:
		RegSetBit(SCFG->DEVALT66, BIT4);
		break;

	case Pin_F1_GPIOB1:
		RegClrBit(SCFG->DEVALT0, (BIT5 | BIT0));
		break;
	case Pin_F1_CLKRUN:
		RegSetBit(SCFG->DEVALT0, BIT5);
		break;
	case Pin_F1_ESPI_CS2:
		RegSetBit(SCFG->DEVALT0, BIT0);
		break;

	case Pin_F2_GPIOB3:
		RegClrBit(SCFG->DEVALT0, BIT7);
		break;
	case Pin_F2_SERIRQ:
		RegSetBit(SCFG->DEVALT0, BIT7);
		break;

	case Pin_F3_DSRA:
		RegSetBit(SCFG->DEVALTB, BIT4);
		break;
	case Pin_F3_GPIO11:
		RegClrBit(SCFG->DEVALTB, BIT4);
		break;

	case Pin_F4_CTSA:
		RegSetBit(SCFG->DEVALTB, BIT5);
		break;
	case Pin_F4_GPIO10:
		RegClrBit(SCFG->DEVALTB, BIT5);
		break;

	case Pin_F7_GPIO87:
		RegClrBit(SCFG->DEVALT1, BIT5);
		break;
	case Pin_F7_VIN5:
		RegSetBit(SCFG->DEVALT1, BIT5);
		break;

	case Pin_F9_SLP_SUS:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		break;
	case Pin_F9_GPIO71:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		RegSetBit(SCFG->DEVALT5C, BIT5);
		break;
	case Pin_F9_VSB3VSW:
		RegSetBit(SCFG->DEVALT5C, BIT4);
		RegSetBit(SCFG->DEVALT5C, BIT5);
		break;

	case Pin_F10_GPIOD3:
		RegClrBit(SCFG->DEVALT66, BIT3);
		break;
	case Pin_F10_CTSE:
		RegSetBit(SCFG->DEVALT66, BIT3);
		break;

	case Pin_F11_GPIOD1:
		RegClrBit(SCFG->DEVALT66, BIT1);
		break;
	case Pin_F11_SOUTE:
		RegSetBit(SCFG->DEVALT66, BIT1);
		break;

	case Pin_G3_DTRA:
		RegSetBit(SCFG->DEVALTB, BIT4);
		break;
	case Pin_G3_GPIO13:
		RegClrBit(SCFG->DEVALTB, BIT4);
		break;

	case Pin_G4_GPIO12:
		RegClrBit(SCFG->DEVALTB, BIT0);
		break;
	case Pin_G4_RTSA:
		RegSetBit(SCFG->DEVALTB, BIT0);
		break;

	case Pin_G6_PD4:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_G6_LED_C:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_G6_BEEP:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegSetBit(SCFG->DEVALT6, BIT1);
		break;
	case Pin_G6_GPIO27:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALT6, BIT1);
		break;

	case Pin_G9_SLP_S5:
		RegClrBit(SCFG->DEVALT5F, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT6);
		RegClrBit(SCFG->DEVALT5F, BIT5);
		break;
	case Pin_G9_VW_S5:
		RegClrBit(SCFG->DEVALT5F, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT5);
		RegClrBit(SCFG->DEVALT6, BIT6);
		RegSetBit(SCFG->DEVALT5F, BIT5);
		break;
	case Pin_G9_VW_S5_OUT:
		RegClrBit(SCFG->DEVALT5F, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT5);
		RegSetBit(SCFG->DEVALT6, BIT6);
		break;
	case Pin_G9_VW_S5_GPIO65:
		RegSetBit(SCFG->DEVALT5F, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT5);
		break;

	case Pin_G9_VW_S4:
		RegClrBit(SCFG->DEVALT5F, BIT1);
		RegSetBit(SCFG->DEVALT6, BIT5);
		RegClrBit(SCFG->DEVALT6, BIT6);
		RegSetBit(SCFG->DEVALT5F, BIT5);
		break;
	case Pin_G9_VW_S4_OUT:
		RegClrBit(SCFG->DEVALT5F, BIT1);
		RegSetBit(SCFG->DEVALT6, BIT5);
		RegSetBit(SCFG->DEVALT6, BIT6);
		break;
	case Pin_G9_VW_S4_GPIO65:
		RegSetBit(SCFG->DEVALT5F, BIT1);
		RegSetBit(SCFG->DEVALT6, BIT5);
		break;

	case Pin_G10_GPIOD2:
		RegClrBit(SCFG->DEVALT66, BIT2);
		break;
	case Pin_G10_RTSE:
		RegSetBit(SCFG->DEVALT66, BIT2);
		break;

	case Pin_G11_GPIOD0:
		RegClrBit(SCFG->DEVALT66, BIT0);
		break;
	case Pin_G11_SINE:
		RegSetBit(SCFG->DEVALT66, BIT0);
		break;

	case Pin_G12_FLM_DIO3:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_G12_GPIO56:
		RegClrBit(SCFG->DEVALTC, BIT2);
		break;
	case Pin_G12_SHD_DIO3:
		RegSetBit(SCFG->DEVALTC, BIT2);
		break;

	case Pin_H2_GPIO16:
		RegClrBit(SCFG->DEVALTB, BIT4);
		break;
	case Pin_H2_DCDA:
		RegSetBit(SCFG->DEVALTB, BIT4);
		break;

	case Pin_H3_SOUTA:
		RegSetBit(SCFG->DEVALTB, BIT3);
		break;
	case Pin_H3_CR_SOUT:
		RegClrBit(SCFG->DEVALTB, BIT3);
		RegSetBit(SCFG->DEVALTA, BIT7);
		break;
	case Pin_H3_SOUTA_P80:
		RegClrBit(SCFG->DEVALTB, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT7);
		RegSetBit(SCFG->DEVALTC, BIT7);
		break;
	case Pin_H3_GPIO15:
		RegClrBit(SCFG->DEVALTB, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT7);
		RegClrBit(SCFG->DEVALTC, BIT7);
		break;

	case Pin_H4_SINA:
		RegSetBit(SCFG->DEVALTB, BIT2);
		break;
	case Pin_H4_CR_SIN:
		RegClrBit(SCFG->DEVALTB, BIT2);
		RegSetBit(SCFG->DEVALTC, BIT6);
		break;
	case Pin_H4_GPIO14:
		RegClrBit(SCFG->DEVALTB, BIT2);
		RegClrBit(SCFG->DEVALTC, BIT6);
		break;

	case Pin_H9_PWROK0:
		RegClrBit(SCFG->DEVALT5E, BIT3);
		break;
	case Pin_H9_GPIOE2:
		RegSetBit(SCFG->DEVALT5E, BIT3);
		break;

	case pin_H10_KBRST:
		RegSetBit(SCFG->DEVALTE, BIT4);
		break;
	case pin_H10_GPIO57:
		RegClrBit(SCFG->DEVALTE, BIT4);
		break;

	case Pin_H11_3VSBSW:
		RegSetBit(SCFG->DEVALT5C, BIT4);
		break;
	case Pin_H11_RESETCONI:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		RegSetBit(SCFG->DEVALT5F, BIT3);
		break;
	case Pin_H11_DPWROKI:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		RegClrBit(SCFG->DEVALT5F, BIT3);
		RegSetBit(SCFG->DEVALT9, BIT6);
		break;
	case Pin_H11_PWM3:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		RegClrBit(SCFG->DEVALT5F, BIT3);
		RegClrBit(SCFG->DEVALT9, BIT6);
		RegSetBit(SCFG->DEVALT4, BIT5);
		break;
	case Pin_H11_GPIO64:
		RegClrBit(SCFG->DEVALT5C, BIT4);
		RegClrBit(SCFG->DEVALT5F, BIT3);
		RegClrBit(SCFG->DEVALT9, BIT6);
		RegClrBit(SCFG->DEVALT4, BIT5);
		break;

	case Pin_H12_FLM_DIO2:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_H12_GPIO55:
		RegClrBit(SCFG->DEV_CTL3, BIT1);
		RegClrBit(SCFG->DEVALTC, BIT2);
		break;
	case Pin_H12_SHD_DIO2:
		RegClrBit(SCFG->DEV_CTL3, BIT1);
		RegSetBit(SCFG->DEVALTC, BIT2);
		break;
	case Pin_H12_WP_GPIO55:
		RegSetBit(SCFG->DEV_CTL3, BIT1);
		break;

	case Pin_J2_GPIOC3:
		RegClrBit(SCFG->DEVALT62, BIT3);
		break;
	case Pin_J2_CTSC:
		RegSetBit(SCFG->DEVALT62, BIT3);
		break;

	case Pin_J3_GPIOC7:
		RegClrBit(SCFG->DEVALT62, BIT7);
		break;
	case Pin_J3_CTSD:
		RegSetBit(SCFG->DEVALT62, BIT7);
		break;

	case Pin_J4_CLKOUT_LFCLK:
		RegSetBit(SCFG->DEVCNT, BIT0);
		RegSetBit(SCFG->DEVALT7, BIT6);
		break;
	case Pin_J4_CLKOUT_CLK:
		RegClrBit(SCFG->DEVCNT, BIT0);
		RegSetBit(SCFG->DEVALT7, BIT6);
		break;
	case Pin_J4_RIA:
		RegClrBit(SCFG->DEVALT7, BIT6);
		RegSetBit(SCFG->DEVALTB, BIT6);
		break;
	case Pin_J4_GPIO17:
		RegClrBit(SCFG->DEVALT7, BIT6);
		RegClrBit(SCFG->DEVALTB, BIT6);
		break;

	case Pin_J5_BUSY:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_J5_P2_DGL:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_J5_GPIO22:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;

	case Pin_J6_PD3:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_J6_LED_D:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_J6_GPIO30:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;

	case Pin_J7_PD1:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_J7_LED_F:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_J7_GPIO32:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;

	case Pin_J8_PSIN:
		RegClrBit(SCFG->DEVALT5C, BIT0);
		break;
	case Pin_J8_GPIO51:
		RegSetBit(SCFG->DEVALT5C, BIT0);
		break;

	case Pin_J9_DEEP_S5_0:
		RegClrBit(SCFG->DEVALT5E, BIT2);
		break;
	case Pin_J9_VSB3VSW:
		RegSetBit(SCFG->DEVALT5E, BIT2);
		RegClrBit(SCFG->DEVALT5E, BIT5);
		break;
	case Pin_J9_GPIOE0:
		RegSetBit(SCFG->DEVALT5E, BIT2);
		RegSetBit(SCFG->DEVALT5E, BIT5);
		break;

	case Pin_J10_RSTOUT0:
		RegClrBit(SCFG->DEVALT5D, BIT0);
		break;
	case Pin_J10_GPIO62:
		RegSetBit(SCFG->DEVALT5D, BIT0);
		break;

	case Pin_J11_ATXPGD:
		RegClrBit(SCFG->DEVALT5F, BIT2);
		break;
	case Pin_J11_GPIO63:
		RegSetBit(SCFG->DEVALT5F, BIT2);
		break;

	case Pin_J12_FLM_SCLK:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_J12_GPIO47:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegClrBit(SCFG->DEVALTC, BIT3);
		break;
	case Pin_J12_SHD_SCLK:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALTC, BIT3);
		break;

	case Pin_K2_GPIOC2:
		RegClrBit(SCFG->DEVALT62, BIT2);
		break;
	case Pin_K2_RTSC:
		RegSetBit(SCFG->DEVALT62, BIT2);
		break;

	case Pin_K3_GPIOC6:
		RegClrBit(SCFG->DEVALT62, BIT6);
		break;
	case Pin_K3_RTSD:
		RegSetBit(SCFG->DEVALT62, BIT6);
		break;

	case Pin_K4_PVT_CS:
		RegSetBit(SCFG->DEVALT0, BIT1);
		break;
	case Pin_K4_ACK:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT0, BIT1);
		break;
	case Pin_K4_GPIO23:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT0, BIT1);
		break;

	case Pin_K5_PD7:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_K5_P2_DGH:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_K5_GPIO24:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;

	case Pin_K6_PD2:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_K6_LED_E:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_K6_GPIO31:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;

	case Pin_K7_PD0:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_K7_LED_G:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_K7_GPIO33:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALTC, BIT3);
		break;

	case Pin_K8_PSOUT:
		RegClrBit(SCFG->DEVALT5C, BIT1);
		break;
	case Pin_K8_GPIO50:
		RegSetBit(SCFG->DEVALT5C, BIT1);
		break;

	case Pin_K9_DPWROK:
		RegClrBit(SCFG->DEVALT5E, BIT0);
		break;
	case Pin_K9_GPIOE1:
		RegSetBit(SCFG->DEVALT5E, BIT0);
		break;

	case Pin_K10_RSTOUT2:
		RegClrBit(SCFG->DEVALT5D, BIT2);
		break;
	case Pin_K10_SCL1A:
		RegSetBit(SCFG->DEVALT5D, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT0);
		break;
	case Pin_K10_GPIO60:
		RegSetBit(SCFG->DEVALT5D, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT0);
		break;

	case Pin_K11_RSTOUT1:
		RegClrBit(SCFG->DEVALT5D, BIT1);
		break;
	case Pin_K11_SDA1A:
		RegSetBit(SCFG->DEVALT5D, BIT1);
		RegSetBit(SCFG->DEVALTA, BIT0);
		break;
	case Pin_K11_GPIO61:
		RegSetBit(SCFG->DEVALT5D, BIT1);
		RegClrBit(SCFG->DEVALTA, BIT0);
		break;

	case Pin_K12_FLM_DIO1:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_K12_GPIO46:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegClrBit(SCFG->DEVALTC, BIT3);
		break;
	case Pin_K12_SHD_DIO1:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALTC, BIT3);
		break;

	case Pin_L2_GPIOC1:
		RegClrBit(SCFG->DEVALT62, BIT1);
		break;
	case Pin_L2_SOUTC:
		RegSetBit(SCFG->DEVALT62, BIT1);
		break;

	case Pin_L3_GPIOC5:
		RegClrBit(SCFG->DEVALT62, BIT5);
		break;
	case Pin_L3_SOUTD:
		RegSetBit(SCFG->DEVALT62, BIT5);
		break;

	case Pin_L4_PE:
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT0, BIT2);
		break;
	case Pin_L4_BACK_CS:
		RegSetBit(SCFG->DEVALT0, BIT2);
		break;
	case Pin_L4_CIRRX:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT7, BIT1);
		RegClrBit(SCFG->DEVALT0, BIT2);
		break;
	case Pin_L4_GPIO21:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT1);
		RegClrBit(SCFG->DEVALT0, BIT2);
		break;

	case Pin_L5_PD6:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_L5_LED_A:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_L5_SDA1B:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegSetBit(SCFG->DEVALTA, BIT5);
		break;
	case Pin_L5_GPIO25:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT5);
		break;

	case Pin_L6_CR_SOUT:
		RegSetBit(SCFG->DEVALT31, BIT0);
		break;
	case Pin_L6_SLIN:
		RegClrBit(SCFG->DEVALT31, BIT0);
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_L6_SCL5A:
		RegClrBit(SCFG->DEVALT31, BIT0);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT3);
		break;
	case Pin_L6_GPIO34:
		RegClrBit(SCFG->DEVALT31, BIT0);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT3);
		break;

	case Pin_L7_ERR:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_L7_VSBY_32KHZ_IN:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT7, BIT5);
		break;
	case Pin_L7_PWM2:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT5);
		RegSetBit(SCFG->DEVALT4, BIT4);
		break;
	case Pin_L7_SCL4A:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT5);
		RegClrBit(SCFG->DEVALT4, BIT4);
		RegSetBit(SCFG->DEVALTA, BIT2);
		break;
	case Pin_L7_GPIO36:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT7, BIT5);
		RegClrBit(SCFG->DEVALT4, BIT4);
		RegClrBit(SCFG->DEVALTA, BIT2);
		break;

	case Pin_L8_STB:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_L8_GRN_LED:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALTE, BIT6);
		break;
	case Pin_L8_GPIOB6:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALTE, BIT6);
		break;

	case Pin_L9_PSON:
		RegClrBit(SCFG->DEVALT5C, BIT2);
		break;
	case Pin_L9_GPIO52:
		RegSetBit(SCFG->DEVALT5C, BIT2);
		break;

	case Pin_L10_SLP_S3:
		RegClrBit(SCFG->DEVALT5F, BIT0);
		RegClrBit(SCFG->DEVALT6, BIT6);
		RegClrBit(SCFG->DEVALT5F, BIT5);
		break;
	case Pin_L10_VW_S3:
		RegClrBit(SCFG->DEVALT5F, BIT0);
		RegClrBit(SCFG->DEVALT6, BIT6);
		RegSetBit(SCFG->DEVALT5F, BIT5);
		break;
	case Pin_L10_VW_S3_OUT:
		RegClrBit(SCFG->DEVALT5F, BIT0);
		RegSetBit(SCFG->DEVALT6, BIT6);
		break;
	case Pin_L10_VW_S3_GPIO53:
		RegSetBit(SCFG->DEVALT5F, BIT0);
		break;

	case Pin_L11_FLM_CSI:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_L11_ECSCI:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALT5C, BIT3);
		RegSetBit(SCFG->DEVALT5, BIT5);
		break;
	case Pin_L11_PME:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegClrBit(SCFG->DEVALT5C, BIT3);
		break;
	case Pin_L11_GPIO54:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALT5C, BIT3);
		RegClrBit(SCFG->DEVALT5, BIT5);
		break;

	case Pin_L12_FLM_DIO0:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_L12_GPIO45:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegClrBit(SCFG->DEVALTC, BIT3);
		break;
	case Pin_L12_SHD_DIO0:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALTC, BIT3);
		break;

	case Pin_M2_GPIOC0:
		RegClrBit(SCFG->DEVALT62, BIT0);
		break;
	case Pin_M2_SINC:
		RegSetBit(SCFG->DEVALT62, BIT0);
		break;

	case Pin_M3_GPIOC4:
		RegClrBit(SCFG->DEVALT62, BIT4);
		break;
	case Pin_M3_SIND:
		RegSetBit(SCFG->DEVALT62, BIT4);
		break;

	case Pin_M4_14MCLKIN:
		RegClrBit(SCFG->DEVALT6, BIT7);
		break;
	case Pin_M4_SLCT:
		RegClrBit(SCFG->DEVALT6, BIT7);
		RegSetBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALTE, BIT7);
		break;
	case Pin_M4_YLW_LED:
		RegClrBit(SCFG->DEVALT6, BIT7);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALTE, BIT7);
		break;
	case Pin_M4_GPIO20:
		RegClrBit(SCFG->DEVALT6, BIT7);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALTE, BIT7);
		break;

	case Pin_M5_PD5:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_M5_LED_B:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_M5_SCL1B:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegSetBit(SCFG->DEVALTA, BIT5);
		break;
	case Pin_M5_GPIO26:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT5);
		break;

	case Pin_M6_CR_SIN:
		RegSetBit(SCFG->DEVALT31, BIT1);
		break;
	case Pin_M6_INIT:
		RegClrBit(SCFG->DEVALT31, BIT1);
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_M6_P1_DGL:
		RegClrBit(SCFG->DEVALT31, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_M6_SDA5A:
		RegClrBit(SCFG->DEVALT31, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegSetBit(SCFG->DEVALTA, BIT3);
		break;
	case Pin_M6_GPIO35:
		RegClrBit(SCFG->DEVALT31, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALTA, BIT3);
		break;

	case Pin_M7_AFD:
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;
	case Pin_M7_P1_DGH:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		break;
	case Pin_M7_TA2:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegSetBit(SCFG->DEVALT3, BIT2);
		break;
	case Pin_M7_SDA4A:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALT3, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT2);
		break;
	case Pin_M7_GPIO37:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegClrBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALT3, BIT2);
		RegClrBit(SCFG->DEVALTA, BIT2);
		break;

	case Pin_M12_FLM_CSIO:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;
	case Pin_M12_GPIO44:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegClrBit(SCFG->DEVALTC, BIT3);
		break;
	case Pin_M12_SHD_CS:
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALTC, BIT3);
		break;

		/* Combo */
	case Pin_I2C1A:
		RegSetBit(SCFG->DEVALT5D, (BIT2 | BIT1));
		RegSetBit(SCFG->DEVALTA, BIT0);
		break;
	case Pin_I2C1B:
		RegClrBit(SCFG->DEVALT6, (BIT3 | BIT2));
		RegSetBit(SCFG->DEVALTA, BIT5);
		break;
	case Pin_I2C3A:
		RegClrBit(SCFG->DEVALT31, (BIT3 | BIT2));
		RegSetBit(SCFG->DEVALTA, BIT1);
		break;
	case Pin_I2C4A:
		RegClrBit(SCFG->DEVALT6, (BIT3 | BIT2));
		RegClrBit(SCFG->DEVALT7, BIT5);
		RegClrBit(SCFG->DEVALT4, BIT4);
		RegClrBit(SCFG->DEV_CTL3, BIT2);
		RegSetBit(SCFG->DEVALTA, BIT2);
		break;
	case Pin_I2C5A:
		RegClrBit(SCFG->DEVALT31, (BIT1 | BIT0));
		RegClrBit(SCFG->DEVALT6, (BIT3 | BIT2));
		RegSetBit(SCFG->DEVALTA, BIT3);
		break;
	case Pin_I2C6A:
	case Pin_TSI:
		RegClrBit(SCFG->DEVALT7, (BIT3 | BIT4 | BIT2));
		RegClrBit(SCFG->DEVALT5, BIT4);
		break;
	case Pin_I3C1:
		RegSetBit(SCFG->DEVALT7, BIT2);
		RegClrBit(SCFG->DEVALT7, (BIT4 | BIT3));
		RegClrBit(SCFG->DEVALT5, BIT4);
		break;
	case Pin_I3C2:
		RegSetBit(SCFG->DEVALTA, BIT4);
		break;

	case Pin_COMA:
		RegClrBit(SCFG->DEVALT7, BIT6);
		RegSetBit(SCFG->DEVALTB, (BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT0));
		break;
	case Pin_COMB:
		RegClrBit(SCFG->EMC_CTL, (BIT1 | BIT0));
		RegSetBit(SCFG->DEVALT9, (BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0));
		break;
	case Pin_COMC:
		RegSetBit(SCFG->DEVALT62, (BIT3 | BIT2 | BIT1 | BIT0));
		break;
	case Pin_COMD:
		RegSetBit(SCFG->DEVALT62, (BIT7 | BIT6 | BIT5 | BIT4));
		break;
	case Pin_COME:
		RegSetBit(SCFG->DEVALT66, (BIT3 | BIT2 | BIT1 | BIT0));
		break;
	case Pin_COMF:
		RegSetBit(SCFG->DEVALT66, (BIT7 | BIT6 | BIT5 | BIT4));
		break;

	case Pin_PRT:
		RegClrBit(SCFG->DEVALT6, (BIT7 | BIT3));
		RegClrBit(SCFG->DEVALTE, BIT7);
		RegClrBit(SCFG->DEVALT0, (BIT2 | BIT1));
		RegClrBit(SCFG->DEVALT31, (BIT1 | BIT0));
		RegSetBit(SCFG->DEVALT6, BIT2);
		break;

	case Pin_SPIP:
		RegClrBit(SCFG->DEV_CTL3, BIT2);
		RegSetBit(SCFG->DEVALTC, BIT1);
		break;

	case Pin_SHD_FIU:
		RegClrBit(SCFG->DEVCNT, BIT6);
		RegClrBit(SCFG->DEVALT5, BIT7);
		RegSetBit(SCFG->DEVALTC, (BIT3 | BIT2));
		RegClrBit(SCFG->DEV_CTL3, BIT1);
		break;
	case Pin_PVT_FIU:
		RegSetBit(SCFG->DEVALTC, (BIT3 | BIT2));
		RegClrBit(SCFG->DEV_CTL3, BIT1);
		RegSetBit(SCFG->DEVALT0, BIT1);
		break;
	case Pin_BACK_FIU:
		RegSetBit(SCFG->DEVALTC, (BIT3 | BIT2));
		RegClrBit(SCFG->DEV_CTL3, BIT1);
		RegSetBit(SCFG->DEVALT0, BIT2);
		break;

	case Pin_FLM:
		RegSetBit(SCFG->DEVALT5, BIT7);
		break;

	case Pin_7SEG:
		RegClrBit(SCFG->DEVALT6, BIT2);
		RegSetBit(SCFG->DEVALT6, BIT3);
		RegClrBit(SCFG->DEVALT31, BIT1);
		RegClrBit(SCFG->DEVALT6, BIT2);
		break;

	case Pin_USBSW_PCH:
		RegClrBit(SCFG->DEVALT0, BIT3);
		break;
	case Pin_USBSW_eSIO:
		RegSetBit(SCFG->DEVALT0, BIT3);
		break;

	case Pin_RMII:
		RegSetBit(SCFG->EMC_CTL, BIT0);
		break;
	case Pin_MDIO:
		RegSetBit(SCFG->EMC_CTL, BIT1);
		break;

	case Pin_CAN0:
		RegSetBit(SCFG->DEVALT34, BIT6);
		break;
	case Pin_CAN1:
		RegSetBit(SCFG->DEVALT34, BIT7);
		break;

	default:
		break;
	}
}
