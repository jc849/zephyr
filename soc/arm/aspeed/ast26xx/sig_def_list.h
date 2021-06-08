#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart0), okay)
SIG_DEFINE(TXD1,	C13,	SIG_DESC_SET(0x41C, 6))
SIG_DEFINE(RXD1,	D13,	SIG_DESC_SET(0x41C, 7))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
SIG_DEFINE(TXD2,	N26,	SIG_DESC_SET(0x41C, 14))
SIG_DEFINE(RXD2,	M26,	SIG_DESC_SET(0x41C, 15))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart2), okay)
SIG_DEFINE(TXD3,	C15,	SIG_DESC_SET(0x418, 28))
SIG_DEFINE(RXD3,	F15,	SIG_DESC_SET(0x418, 29))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart3), okay)
SIG_DEFINE(TXD4,	H25,	SIG_DESC_SET(0x410, 14))
SIG_DEFINE(RXD4,	J24,	SIG_DESC_SET(0x410, 15))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart4), okay)
SIG_DEFINE(TXD5,	C8)
SIG_DEFINE(RXD5,	D8)
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart5), okay)
SIG_DEFINE(TXD6,	E21, SIG_DESC_SET(0x414, 16))
SIG_DEFINE(RXD6,	B22, SIG_DESC_SET(0x414, 17))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart6), okay)
SIG_DEFINE(TXD7,	C21,	SIG_DESC_SET(0x414, 18))
SIG_DEFINE(RXD7,	A22,	SIG_DESC_SET(0x414, 19))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart9), okay)
SIG_DEFINE(TXD10,	R26,	SIG_DESC_SET(0x430, 20))
SIG_DEFINE(RXD10,	P24,	SIG_DESC_SET(0x430, 21))
#endif

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart10), okay)
SIG_DEFINE(TXD11,	P23,	SIG_DESC_SET(0x430, 22))
SIG_DEFINE(RXD11,	T24,	SIG_DESC_SET(0x430, 23))
#endif

#ifdef CONFIG_ARM_ICE
SIG_DEFINE(ARM_TRST,	F11,	SIG_DESC_SET(0x418, 0))
SIG_DEFINE(ARM_TDO,	C9,	SIG_DESC_SET(0x418, 1))
SIG_DEFINE(ARM_TCK,	B9,	SIG_DESC_SET(0x418, 2))
SIG_DEFINE(ARM_TMS,	A9,	SIG_DESC_SET(0x418, 3))
SIG_DEFINE(ARM_TDI,	D9,	SIG_DESC_SET(0x418, 4))
#endif

#ifdef CONFIG_DEVICE_USB
SIG_DEFINE(USB2BDDP, A6, SIG_DESC_SET(0x440, 28))
SIG_DEFINE(USB2BDDN, B6, SIG_DESC_CLEAR(0x440, 29))
#endif

#ifdef CONFIG_DEVICE_ADC
SIG_DEFINE(ADC0, AD20, SIG_DESC_CLEAR(0x430, 24))
SIG_DEFINE(ADC1, AC18, SIG_DESC_CLEAR(0x430, 25))
SIG_DEFINE(ADC2, AE19, SIG_DESC_CLEAR(0x430, 26))
SIG_DEFINE(ADC3, AD19, SIG_DESC_CLEAR(0x430, 27))
SIG_DEFINE(ADC4, AC19, SIG_DESC_CLEAR(0x430, 28))
SIG_DEFINE(ADC5, AB19, SIG_DESC_CLEAR(0x430, 29))
SIG_DEFINE(ADC6, AB18, SIG_DESC_CLEAR(0x430, 30))
SIG_DEFINE(ADC7, AE18, SIG_DESC_CLEAR(0x430, 31))
SIG_DEFINE(ADC8, AB16, SIG_DESC_CLEAR(0x434, 0))
SIG_DEFINE(ADC9, AA17, SIG_DESC_CLEAR(0x434, 1))
SIG_DEFINE(ADC10, AB17, SIG_DESC_CLEAR(0x434, 2))
SIG_DEFINE(ADC11, AE16, SIG_DESC_CLEAR(0x434, 3))
SIG_DEFINE(ADC12, AC16, SIG_DESC_CLEAR(0x434, 4))
SIG_DEFINE(ADC13, AA16, SIG_DESC_CLEAR(0x434, 5))
SIG_DEFINE(ADC14, AD16, SIG_DESC_CLEAR(0x434, 6))
SIG_DEFINE(ADC15, AC17, SIG_DESC_CLEAR(0x434, 7))

#endif /* end of "#if CONFIG_DEVICE_ADC" */

#ifdef CONFIG_DEVICE_PWM_TACH
SIG_DEFINE(PWM0, AD26, SIG_DESC_SET(0x41C, 16))
SIG_DEFINE(PWM1, AD22, SIG_DESC_SET(0x41C, 17))
SIG_DEFINE(PWM2, AD23, SIG_DESC_SET(0x41C, 18))
SIG_DEFINE(PWM3, AD24, SIG_DESC_SET(0x41C, 19))
SIG_DEFINE(PWM4, AD25, SIG_DESC_SET(0x41C, 20))
SIG_DEFINE(PWM5, AC22, SIG_DESC_SET(0x41C, 21))
SIG_DEFINE(PWM6, AC24, SIG_DESC_SET(0x41C, 22))
SIG_DEFINE(PWM7, AC23, SIG_DESC_SET(0x41C, 23))
SIG_DEFINE(PWM8_0, AB22, SIG_DESC_SET(0x41C, 24))
SIG_DEFINE(PWM8_1,  D22, SIG_DESC_CLEAR(0x414, 8),  SIG_DESC_SET(0x4B4, 8))
SIG_DEFINE(PWM9_0, W24, SIG_DESC_SET(0x41C, 25))
SIG_DEFINE(PWM9_1,  E22, SIG_DESC_CLEAR(0x414, 9),  SIG_DESC_SET(0x4B4, 9))
SIG_DEFINE(PWM10_0, AA23, SIG_DESC_SET(0x41C, 26))
SIG_DEFINE(PWM10_1, D23, SIG_DESC_CLEAR(0x414, 10), SIG_DESC_SET(0x4B4, 10))
SIG_DEFINE(PWM11_0, AA24, SIG_DESC_SET(0x41C, 27))
SIG_DEFINE(PWM11_1, C23, SIG_DESC_CLEAR(0x414, 11), SIG_DESC_SET(0x4B4, 11))
SIG_DEFINE(PWM12_0, W23, SIG_DESC_SET(0x41C, 28))
SIG_DEFINE(PWM12_1, C22, SIG_DESC_CLEAR(0x414, 12), SIG_DESC_SET(0x4B4, 12))
SIG_DEFINE(PWM13_0, AB23, SIG_DESC_SET(0x41C, 29))
SIG_DEFINE(PWM13_1, A25, SIG_DESC_CLEAR(0x414, 13), SIG_DESC_SET(0x4B4, 13))
SIG_DEFINE(PWM14_0, AB24, SIG_DESC_SET(0x41C, 30))
SIG_DEFINE(PWM14_1, A24, SIG_DESC_CLEAR(0x414, 14), SIG_DESC_SET(0x4B4, 14))
SIG_DEFINE(PWM15_0, Y23, SIG_DESC_SET(0x41C, 31))
SIG_DEFINE(PWM15_1, A23, SIG_DESC_CLEAR(0x414, 15), SIG_DESC_SET(0x4B4, 15))
SIG_DEFINE(TACH0, AA25, SIG_DESC_SET(0x430, 0))
SIG_DEFINE(TACH1, AB25, SIG_DESC_SET(0x430, 1))
SIG_DEFINE(TACH2, Y24, SIG_DESC_SET(0x430, 2))
SIG_DEFINE(TACH3, AB26, SIG_DESC_SET(0x430, 3))
SIG_DEFINE(TACH4, Y26, SIG_DESC_SET(0x430, 4))
SIG_DEFINE(TACH5, AC26, SIG_DESC_SET(0x430, 5))
SIG_DEFINE(TACH6, Y25, SIG_DESC_SET(0x430, 6))
SIG_DEFINE(TACH7, AA26, SIG_DESC_SET(0x430, 7))
SIG_DEFINE(TACH8, V25, SIG_DESC_SET(0x430, 8))
SIG_DEFINE(TACH9, U24, SIG_DESC_SET(0x430, 9))
SIG_DEFINE(TACH10, V24, SIG_DESC_SET(0x430, 10))
SIG_DEFINE(TACH11, V26, SIG_DESC_SET(0x430, 11))
SIG_DEFINE(TACH12, U25, SIG_DESC_SET(0x430, 12))
SIG_DEFINE(TACH13, T23, SIG_DESC_SET(0x430, 13))
SIG_DEFINE(TACH14, W26, SIG_DESC_SET(0x430, 14))
SIG_DEFINE(TACH15, U26, SIG_DESC_SET(0x430, 15))
#endif

#ifdef CONFIG_DEVICE_JTAG
SIG_DEFINE(MTRSTN, D17, SIG_DESC_SET(0x418, 0))
SIG_DEFINE(MTDI, A16, SIG_DESC_SET(0x418, 1))
SIG_DEFINE(MTCK, E17, SIG_DESC_SET(0x418, 2))
SIG_DEFINE(MTMS, D16, SIG_DESC_SET(0x418, 3))
SIG_DEFINE(MTDO, C16, SIG_DESC_SET(0x418, 4))
#endif

#ifdef CONFIG_DEVICE_I3C
SIG_DEFINE(I3C1SCL, AF23, SIG_DESC_SET(0x438, 16))
SIG_DEFINE(I3C1SDA, AE24, SIG_DESC_SET(0x438, 17))
SIG_DEFINE(I3C2SCL, AF22, SIG_DESC_SET(0x438, 18))
SIG_DEFINE(I3C2SDA, AE22, SIG_DESC_SET(0x438, 19))
SIG_DEFINE(I3C3SCL, AF25, SIG_DESC_SET(0x438, 20))
SIG_DEFINE(I3C3SDA, AE26, SIG_DESC_SET(0x438, 21))
SIG_DEFINE(I3C4SCL, AE25, SIG_DESC_SET(0x438, 22))
SIG_DEFINE(I3C4SDA, AF24, SIG_DESC_SET(0x438, 23))
SIG_DEFINE(HVI3C5SCL, C19, SIG_DESC_SET(0x418, 12))
SIG_DEFINE(HVI3C5SDA, A19, SIG_DESC_SET(0x418, 13))
SIG_DEFINE(HVI3C6SCL, C20, SIG_DESC_SET(0x418, 14))
SIG_DEFINE(HVI3C6SDA, D19, SIG_DESC_SET(0x418, 15))
#endif

#ifdef CONFIG_DEVICE_I2C
SIG_DEFINE(SCL1, B20, SIG_DESC_SET(0x4B8, 8), SIG_DESC_CLEAR(0x418, 8))
SIG_DEFINE(SDA1, A20, SIG_DESC_SET(0x4B8, 9), SIG_DESC_CLEAR(0x418, 9))
SIG_DEFINE(SCL2, E19, SIG_DESC_SET(0x4B8, 10), SIG_DESC_CLEAR(0x418, 10))
SIG_DEFINE(SDA2, D20, SIG_DESC_SET(0x4B8, 11), SIG_DESC_CLEAR(0x418, 11))
SIG_DEFINE(SCL3, C19, SIG_DESC_SET(0x4B8, 12), SIG_DESC_CLEAR(0x418, 12))
SIG_DEFINE(SDA3, A19, SIG_DESC_SET(0x4B8, 13), SIG_DESC_CLEAR(0x418, 13))
SIG_DEFINE(SCL4, C20, SIG_DESC_SET(0x4B8, 14), SIG_DESC_CLEAR(0x418, 14))
SIG_DEFINE(SDA4, D19, SIG_DESC_SET(0x4B8, 15), SIG_DESC_CLEAR(0x418, 15))
SIG_DEFINE(SCL5, A11, SIG_DESC_SET(0x418, 16))
SIG_DEFINE(SDA5, C11, SIG_DESC_SET(0x418, 17))
SIG_DEFINE(SCL6, D12, SIG_DESC_SET(0x418, 18))
SIG_DEFINE(SDA6, E13, SIG_DESC_SET(0x418, 19))
SIG_DEFINE(SCL7, D11, SIG_DESC_SET(0x418, 20))
SIG_DEFINE(SDA7, E11, SIG_DESC_SET(0x418, 21))
SIG_DEFINE(SCL8, F13, SIG_DESC_SET(0x418, 22))
SIG_DEFINE(SDA8, E12, SIG_DESC_SET(0x418, 23))
SIG_DEFINE(SCL9, D15, SIG_DESC_SET(0x418, 24))
SIG_DEFINE(SDA9, A14, SIG_DESC_SET(0x418, 25))
SIG_DEFINE(SCL10, E15, SIG_DESC_SET(0x418, 26))
SIG_DEFINE(SDA10, A13, SIG_DESC_SET(0x418, 27))
SIG_DEFINE(SCL11, M24, SIG_DESC_SET(0x4B0, 0), SIG_DESC_CLEAR(0x410, 0))
SIG_DEFINE(SDA11, M25, SIG_DESC_SET(0x4B0, 1), SIG_DESC_CLEAR(0x410, 1))
SIG_DEFINE(SCL12, L26, SIG_DESC_SET(0x4B0, 2), SIG_DESC_CLEAR(0x410, 2))
SIG_DEFINE(SDA12, K24, SIG_DESC_SET(0x4B0, 3), SIG_DESC_CLEAR(0x410, 3))
SIG_DEFINE(SCL13, K26, SIG_DESC_SET(0x4B0, 4), SIG_DESC_CLEAR(0x410, 4))
SIG_DEFINE(SDA13, L24, SIG_DESC_SET(0x4B0, 5), SIG_DESC_CLEAR(0x410, 5))
SIG_DEFINE(SCL14, L23, SIG_DESC_SET(0x4B0, 6), SIG_DESC_CLEAR(0x410, 6))
SIG_DEFINE(SDA14, K25, SIG_DESC_SET(0x4B0, 7), SIG_DESC_CLEAR(0x410, 7))
SIG_DEFINE(SCL15, D18, SIG_DESC_SET(0x4B4, 28), SIG_DESC_CLEAR(0x414, 28))
SIG_DEFINE(SDA15, B17, SIG_DESC_SET(0x4B4, 29), SIG_DESC_CLEAR(0x414, 29))
SIG_DEFINE(SCL16, C17, SIG_DESC_SET(0x4B4, 30), SIG_DESC_CLEAR(0x414, 30))
SIG_DEFINE(SDA16, E18, SIG_DESC_SET(0x4B4, 31), SIG_DESC_CLEAR(0x414, 31))
#endif

#ifdef CONFIG_DEVICE_GPIO
SIG_DEFINE(GPIOP7, Y23, SIG_DESC_CLEAR(0x41C, 31), SIG_DESC_CLEAR(0x4BC, 31),
	   SIG_DESC_CLEAR(0x69C, 31))
SIG_DEFINE(GPIT0, AD20, SIG_DESC_SET(0x430, 24))
SIG_DEFINE(GPIT1, AC18, SIG_DESC_SET(0x430, 25))
SIG_DEFINE(GPIT2, AE19, SIG_DESC_SET(0x430, 26))
SIG_DEFINE(GPIT3, AD19, SIG_DESC_SET(0x430, 27))
SIG_DEFINE(GPIT4, AC19, SIG_DESC_SET(0x430, 28))
SIG_DEFINE(GPIT5, AB19, SIG_DESC_SET(0x430, 29))
SIG_DEFINE(GPIT6, AB18, SIG_DESC_SET(0x430, 30))
SIG_DEFINE(GPIT7, AE18, SIG_DESC_SET(0x430, 31))
SIG_DEFINE(GPIU0, AB16, SIG_DESC_SET(0x434, 0), SIG_DESC_SET(0x694, 16))
SIG_DEFINE(GPIU1, AA17, SIG_DESC_SET(0x434, 1), SIG_DESC_SET(0x694, 17))
SIG_DEFINE(GPIU2, AB17, SIG_DESC_SET(0x434, 2), SIG_DESC_SET(0x694, 18))
SIG_DEFINE(GPIU3, AE16, SIG_DESC_SET(0x434, 3), SIG_DESC_SET(0x694, 19))
SIG_DEFINE(GPIU4, AC16, SIG_DESC_SET(0x434, 4), SIG_DESC_SET(0x694, 20))
SIG_DEFINE(GPIU5, AA16, SIG_DESC_SET(0x434, 5), SIG_DESC_SET(0x694, 21))
SIG_DEFINE(GPIU6, AD16, SIG_DESC_SET(0x434, 6), SIG_DESC_SET(0x694, 22))
SIG_DEFINE(GPIU7, AC17, SIG_DESC_SET(0x434, 7), SIG_DESC_SET(0x694, 23))
#endif

#ifdef CONFIG_DEVICE_SGPIOM
SIG_DEFINE(SGPM1CLK, A18, SIG_DESC_SET(0x414, 24))
SIG_DEFINE(SGPM1LD, B18, SIG_DESC_SET(0x414, 25))
SIG_DEFINE(SGPM1O, C18, SIG_DESC_SET(0x414, 26))
SIG_DEFINE(SGPM1I, A17, SIG_DESC_SET(0x414, 27))
SIG_DEFINE(SGPM2CLK, K26, SIG_DESC_SET(0x6D0, 4), SIG_DESC_CLEAR(0x410, 4),
		   SIG_DESC_CLEAR(0x4B0, 4), SIG_DESC_CLEAR(0x690, 4))
SIG_DEFINE(SGPM2LD, L24, SIG_DESC_SET(0x6D0, 5), SIG_DESC_CLEAR(0x410, 5),
		   SIG_DESC_CLEAR(0x4B0, 5), SIG_DESC_CLEAR(0x690, 5))
SIG_DEFINE(SGPM2O, L23, SIG_DESC_SET(0x6D0, 6), SIG_DESC_CLEAR(0x410, 6),
		   SIG_DESC_CLEAR(0x4B0, 6), SIG_DESC_CLEAR(0x690, 6))
SIG_DEFINE(SGPM2I, K25, SIG_DESC_SET(0x6D0, 7), SIG_DESC_CLEAR(0x410, 7),
		   SIG_DESC_CLEAR(0x4B0, 7), SIG_DESC_CLEAR(0x690, 7))
#endif
