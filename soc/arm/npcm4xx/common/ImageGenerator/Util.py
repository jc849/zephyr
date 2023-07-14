#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

from enum import Enum
import os


class Target(Enum):
    Config = 0
    FileName = 1
    FWImageHeader = 2
    OTPImageHeader = 3
    OTPbitmap = 4
    Description = 5
    Crypto = 6
    OpenSSL = 7
    CNG = 8


class KeyFormat(Enum):
    DER = 0
    PEM = 1


class KeyType(Enum):
    RSA = 0
    ECDSA = 1


class XmlError(BaseException):
    pass


class SettingError(BaseException):
    pass


class KeyTypeError(BaseException):
    pass


class LengthError(BaseException):
    pass


class CNGError(BaseException):
    pass


def SetPath(path=None):
    global Path_Current, Path_Input, Path_Key, Path_Output, Path_Xml
    if(path is not None):
        if (os.path.isabs(path)):
            Path_Current = path
        else:
            Path_Current = os.path.join(Path_Current, path)

        Path_Input = os.path.join(Path_Current, 'Input')
        Path_Xml = os.path.join(Path_Current, 'Xml')
        Path_Key = os.path.join(Path_Current, 'Key')
        Path_Output = os.path.join(Path_Current, 'Output')

    elif(path is None):
        Path_Input = os.path.join(Path_Current, 'Input')
        Path_Xml = os.path.join(Path_Current, 'Xml')
        Path_Key = os.path.join(Path_Current, 'Key')
        Path_Output = os.path.join(Path_Current, 'Output')


def SetCryptoFunc(func):
    global CryptoSelect
    CryptoSelect = func


# Crypto
# 0 for Openssl, 1 for MS CNG, 2 for PKCS#11
CryptoSelect = 0

# Signing padding type
Sign_NoPadding = 0
RSASSA_PKCS1V15 = 1
RSASSA_PSS = 2

# Key Type
Key_RSA = 1
Key_EC = 2

# HashType
SHA1 = 1
SHA256 = 2
SHA384 = 3
SHA512 = 4

# Key Length
RSA2048 = 1
RSA3072 = 2
RSA4096 = 3
EC256 = 4
EC384 = 5
EC521 = 6

# flag
internal = False
DumpOTP = False
MD = False 			# Manufacture Done

# version
version = 'NuveSIOBinGenTool_1.10.20.0701'

# Path
Path_Current = os.getcwd()
Path_Input = os.path.join(Path_Current, 'Input')
Path_Xml = os.path.join(Path_Current, 'Xml')
Path_Key = os.path.join(Path_Current, 'Key')
Path_Output = os.path.join(Path_Current, 'Output')
Path_Custom = None

# FW Image Header Reserved (bits)
hReservedField0 = 32  # 4 bytes = 32 bits
hReservedField1 = 6  # 6 bits
hReservedField2 = 24  # 3 bytes = 24 bits
hReservedField3 = 0  # 0 bytes = 0 bits
hReservedField4 = 280  # 35 bytes = 280 bits
hReservedField5 = 128  # 16 bytes = 128 bits

# OTP Image Header Reserved
hOtpImgRsvd = 176  # 22 bytes = 176 bits

# OTP Image Reserved
oReservedField1 = 3  # 3 bits
oReservedField2 = 1  # 1 bits
oReservedField3 = 2  # 2 bits
oReservedField4 = 80  # 10 bytes = 80 bits
oReservedField5 = 256  # 32 bytes = 256 bits

# Global File
sBBName = ''
sDataCodeName = ''
sRAMCodeName = ''
sCombineName = ''

sHookSegName = ''
sDataSegName = ''

sFlashCodeName = ''
sRAMCodeAlignName = ''
sCombineName_Encrypt = ''
sRAMCodeName_Encrypt = ''
sBBName_Encrypt = ''
sAESPubKeyName = ''

# Length
FW_Image_header_len = 1280
FW_Image_hash_table_len = 256
OTP_Image_header_len = 64
Signature_len = 512
OTP_Image = 1024
# 6694
OTP_Alignment = 256  # 4096
BB_Alignment = 256
BBSize_Alignment = 256
hImageTag = 8
hOtpImgHdrOffset = 4
FW_Header_nonsig = hImageTag + Signature_len + \
    hOtpImgHdrOffset + (hReservedField0 / 8)
hImageHash = 32

# FanTables


class FanTableType(Enum):
    Single = 0
    Combine = 1


FanTables_Name = []
FanTables_Content = bytearray()
FanTableCnt = 0
FanTableSize = 1024
FanTableTag_Profile = 'CfgMMIORegProfile'
FanTanleTag_Item = 'MMIORegItem'

# TextColor
CRed = '\33[91m'
CYellow = '\33[93m'
ENDC = '\033[0m'

# For Customize xml
# ReleaseMode, SecurityMode
ReleaseMode = 0
SecurityMode = 0
RSAPKCPAD = 0
# hDevMode
hSecureBoot = 0
hSecurityLvl = 0
hOTPRefToTable = 0
hHwTrimRefOTPTable = 0
hNotUpdateOTPRegister = 0
hNotEraseOTPTable = 0
hNotDoBackup = 0
hOTPRefToSrcTable = 0
hNotDoBackup = 0

# oSigPubKeySts
oSigPubKeySts = [0, 0, 0, 0, 0, 0, 0, 0]
# oLongKeySel
oLongKeySel = 0

# [0], byte offset
#[1], mask
#[2], length(bit)
# [3], << shift
DictColDet = {}
OTPMagicString = b'\x25\x4F\x74\x50\x6D\x41\x70\x40'
FHDMagicString = b'\x25\x46\x69\x4d\x67\x33\x32\x40'

# FW image header
DictColDet['hSignature'] = [8, None, 4096, 0]
DictColDet['hOtpImgHdrOffset'] = [520, None, 32, 0]
DictColDet['hActiveECFwOffset'] = [528, None, 16, 0]
DictColDet['hRecoveryEcFwOffset'] = [530, None, 16, 0]
DictColDet['hSystemECFWOffset'] = [532, None, 32, 0]
DictColDet['hDevMode'] = [536, 0xC0, 6, 0]
DictColDet['hSecureBoot'] = [536, 0xFE, 1, 0]
DictColDet['hSecurityLvl'] = [536, 0xFD, 1, 1]
DictColDet['hOTPRefToTable'] = [536, 0xFB, 1, 2]
DictColDet['hHwTrimRefOTPTable'] = [536, 0xF7, 1, 3]
DictColDet['hNotUpdateOTPRegister'] = [536, 0xEF, 1, 4]
DictColDet['hNotEraseOTPTable'] = [536, 0xDF, 1, 5]
# 6694
DictColDet['hOTPRefToSrcTable'] = [536, 0xBF, 1, 6]
DictColDet['hNotDoBackup'] = [536, 0x7F, 1, 7]
DictColDet['hFlashLockReg0'] = [537, None, 8, 0]
# 6694
#DictColDet['BackupWhenUpdate'] = [539, 0xFE, 1, 0]
DictColDet['hEcFwRegionSize'] = [539, None, 8, 0]
DictColDet['hUserFWEntryPoint'] = [540, None, 32, 0]
DictColDet['hUserFWRamCodeFlashStart'] = [544, None, 32, 0]
DictColDet['hUserFWRamCodeFlashEnd'] = [548, None, 32, 0]
DictColDet['hUserFWRamCodeRamStart'] = [552, None, 32, 0]
DictColDet['hImageLen'] = [556, None, 32, 0]
DictColDet['hSigPubKeyHashIdx'] = [560, 0xFE, 1, 0]
DictColDet['hShaAlgoUsed'] = [560, 0xBF, 1, 6]
DictColDet['EcFwMajorVer'] = [561, None, 8, 0]
DictColDet['EcFwMinorVer'] = [562, None, 16, 0]
DictColDet['hRevokeKey'] = [564, None, 8, 0]
DictColDet['hRevokeKeyInv'] = [565, None, 8, 0]
DictColDet['hBBSize'] = [566, None, 16, 0]
DictColDet['hBBOffset'] = [568, None, 32, 0]
DictColDet['hBBWorkRAM'] = [636, None, 32, 0]
DictColDet['hSigPubKey'] = [640, None, 4096, 0]
DictColDet['OEMver'] = [1184, None, 64, 0]
# 6694
# DictColDet['hFwRbkValidLog'] = [1200, None, 256, 0]
# DictColDet['hImageHash'] = [1232, None, 256, 0]
# 6694
DictColDet['hHook1Ptr'] = [1200, None, 32, 0]
DictColDet['hHook2Ptr'] = [1204, None, 32, 0]
DictColDet['hHook3Ptr'] = [1208, None, 32, 0]
DictColDet['hHook4Ptr'] = [1212, None, 32, 0]

# 6694
DictColDet['hFwSeg1Offset'] = [1216, None, 32, 0]
DictColDet['hFwSeg1End'] = [1220, None, 32, 0]
DictColDet['hFwSeg2Offset'] = [1224, None, 32, 0]
DictColDet['hFwSeg2End'] = [1228, None, 32, 0]
DictColDet['hFwSeg3Offset'] = [1232, None, 32, 0]
DictColDet['hFwSeg3End'] = [1236, None, 32, 0]
DictColDet['hFwSeg4Offset'] = [1240, None, 32, 0]
DictColDet['hFwSeg4End'] = [1244, None, 32, 0]

DictColDet['hRamCodeAESTag'] = [1248, None, 16, 0]
DictColDet['hBootBlockAESTag'] = [1264, None, 16, 0]

DictColDet['hFwSeg1Hash'] = [1280, None, 512, 0]
DictColDet['hFwSeg2Hash'] = [1344, None, 512, 0]
DictColDet['hFwSeg3Hash'] = [1408, None, 512, 0]
DictColDet['hFwSeg4Hash'] = [1472, None, 512, 0]


# Fan Table
# DictColDet['FanTable'] = [1280, None, None, 0]

# OTP Image Header
DictColDet['hOtpImgChksum'] = [8, None, 256, 0]
# OTP bit map
DictColDet['oFlashConnection'] = [0, 0xFC, 2, 0]
DictColDet['oStrapMode1'] = [0, 0xFB, 1, 2]
DictColDet['oStrapMode2'] = [0, 0xF7, 1, 3]
# DictColDet['oNotTrySysIfSPI2'] = [0, 0xEF, 1, 4]
DictColDet['oNotTrySysIfFIUPvt'] = [0, 0xEF, 1, 4]
DictColDet['oNotTrySysIfFIUShd'] = [0, 0xDF, 1, 5]
DictColDet['oNotTryMafAndAMD'] = [0, 0xBF, 1, 6]
DictColDet['oNotTrySysIfSPI1'] = [0, 0x7F, 1, 7]
DictColDet['oNotTrySysIfFIUBkp'] = [1, 0xFE, 1, 0]
# DictColDet['oFIUPri4BMode'] = [1, 0xFE, 1, 0]
DictColDet['oFIUShr4BMode'] = [1, 0xFD, 1, 1]
# 6694
DictColDet['oSPIP4BMode'] = [1, 0xFB, 1, 2]
DictColDet['oSpiQuadPEn'] = [1, 0xF7, 1, 3]
DictColDet['oECPTRCheckCRC'] = [1, 0xEF, 1, 4]
# DictColDet['oSysAuthAlways'] = [1, 0xDF, 1, 5]
DictColDet['oMCPFlashSize'] = [1, 0x3F, 2, 6]
DictColDet['oMCPFLMode'] = [2, 0xFC, 2, 0]
DictColDet['oFIUShrFLMode'] = [2, 0xF3, 2, 2]
# 6694
DictColDet['oSPIPFLMode'] = [2, 0xCF, 2, 4]
# DictColDet['oSPIMFLMode'] = [2, 0x3F, 2, 6]

DictColDet['oFIU0ClkDiv'] = [3, 0xFC, 2, 0]
# 6694
DictColDet['oSPIMClkDiv'] = [3, 0xF3, 2, 2]
DictColDet['oSPIPClkDiv'] = [3, 0xCF, 2, 4]
DictColDet['oFwNotUse4KStep'] = [3, 0xBF, 1, 6]
DictColDet['oFwNotUse2NStep'] = [3, 0x7F, 1, 7]


DictColDet['oFwSearchStep64KOnly'] = [3, 0xBF, 1, 6]
DictColDet['oFwSearchStep4KOnly'] = [3, 0x7F, 1, 7]
DictColDet['oSecureBoot'] = [4, 0xFE, 1, 0]
DictColDet['oSecurityLvl'] = [4, 0xFD, 1, 1]
DictColDet['oHaltIfMafRollbk'] = [4, 0xFB, 1, 2]
DictColDet['oHaltIfActiveRollbk'] = [4, 0xF7, 1, 3]
DictColDet['oHaltIfOnlyMafValid'] = [4, 0xEF, 1, 4]
DictColDet['oTryBootIfAllCrashed'] = [4, 0xDF, 1, 5]
DictColDet['oUnmapRomBfXferCtl'] = [4, 0x7F, 1, 7]
DictColDet['oDisableDBGAtRst'] = [5, 0xFE, 1, 0]
DictColDet['oAESKeyLock'] = [5, 0xFD, 1, 1]
DictColDet['oHWCfgFieldLock'] = [5, 0xFB, 1, 2]
DictColDet['oOtpRgn2Lock'] = [5, 0xF7, 1, 3]
DictColDet['oOtpRgn3Lock'] = [5, 0xEF, 1, 4]
DictColDet['oOtpRgn4Lock'] = [5, 0xDF, 1, 5]
DictColDet['oOtpRgn5Lock'] = [5, 0xBF, 1, 6]
DictColDet['oOtpRgn6Lock'] = [5, 0x7F, 1, 7]
DictColDet['oSecEvnLogLoc'] = [6, None, 16, 0]
# 6694
DictColDet['oSigPubKeySts'] = [8, None, 8, 0]
DictColDet['oRSAPubKeySts'] = [8, 0xFC, 2, 0]
DictColDet['oECPubKeySts'] = [8, 0xF3, 2, 2]
DictColDet['oRevokeKeySts'] = [8, 0xCF, 2, 4]
DictColDet['oLongKeyUsed'] = [8, 0xBF, 1, 6]
DictColDet['oSHA512Used'] = [8, 0x7F, 1, 7]
# 6694
DictColDet['oLngKeySel'] = [16, 0xFE, 1, 0]
DictColDet['oRSAPKCPAD'] = [16, 0xFD, 1, 1]
DictColDet['oAESDecryptEn'] = [16, 0xCF, 2, 4]

DictColDet['oRetryLimitEn'] = [17, 0xFC, 1, 2]
DictColDet['oSkipCtyptoSelfTest'] = [17, 0xF7, 1, 3]
DictColDet['oOlnyLogCriticalEvent'] = [17, 0xEF, 1, 4]
# DictColDet['oSysAuthAlways'] = [17, 0xDF, 1, 5]
DictColDet['oOTPRegionRdLock'] = [17, 0xBF, 1, 6]
DictColDet['oClrRamExitRom'] = [17, 0x7F, 1, 7]

DictColDet['oVSpiExistNoTimeOut'] = [18, 0xFE, 1, 0]
DictColDet['oNoNeedWaitPltRdy'] = [18, 0xFD, 1, 1]
DictColDet['oTryBootNotCtrlFWRdy'] = [18, 0xFB, 1, 2]
DictColDet['oNotUpdateToPrvFw'] = [18, 0xF7, 1, 3]
DictColDet['oSysPfrWP0En'] = [19, 0xFE, 1, 0]
DictColDet['oSysPfrWP1En'] = [19, 0xFD, 1, 1]
DictColDet['oSysPfrWP2En'] = [19, 0xFB, 1, 2]
DictColDet['oOTPDatValid0'] = [21, None, 8, 0]
DictColDet['oOTPDatValid1'] = [22, None, 8, 0]

DictColDet['oLed1Sel'] = [23, 0xF0, 4, 0]
DictColDet['oLed1Pole'] = [23, 0xEF, 1, 4]
DictColDet['oLed2Sel'] = [24, 0xF0, 4, 0]
DictColDet['oLed2Pole'] = [24, 0xEF, 1, 4]
DictColDet['oLedActRbkBlkDef'] = [25, 0xF0, 4, 0]
DictColDet['oLedSysRbkBlkDef'] = [25, 0x0F, 4, 4]
DictColDet['oLedFwCpyBlkDef'] = [26, 0xF0, 4, 0]
DictColDet['oLedSysOnlyBlkDef'] = [26, 0x0F, 4, 4]
DictColDet['oLedAllCrashBlkDef'] = [27, 0xF0, 4, 0]
DictColDet['oLedCryptoTestFailBlkDef'] = [27, 0x0F, 4, 4]

# DictColDet['oRomMlbxLoc'] = [30, None, 16, 0]
DictColDet['oPartID'] = [320, 0xFE, 1, 0]
DictColDet['oAESNotSupport'] = [320, 0xFD, 1, 1]
DictColDet['oMCUClkNNotDiv2'] = [320, 0xFB, 1, 2]
DictColDet['oMCPPackMode'] = [320, 0x1F, 3, 5]
DictColDet['oANAD2Low'] = [321, None, 8, 0]
DictColDet['oANAD2High'] = [322, 0xFE, 1, 0]
DictColDet['oValANAD'] = [322, 0x7F, 1, 7]
DictColDet['oDevId'] = [323, 0xE0, 5, 0]
DictColDet['oValDevId'] = [323, 0x7F, 1, 7]
DictColDet['oRSMRST_L'] = [324, None, 8, 0]
DictColDet['oValRSMRST_L'] = [325, 0x7F, 1, 7]
DictColDet['oRSMRST_Sys'] = [326, None, 8, 0]
DictColDet['oValRSMRST_Sys'] = [327, 0x7F, 1, 7]
DictColDet['oDivMinLow'] = [328, None, 8, 0]
DictColDet['oDivMinHigh'] = [329, 0xFE, 1, 0]
DictColDet['oValDivMin'] = [329, 0x7F, 1, 7]
DictColDet['oDivMaxLow'] = [330, None, 8, 0]
DictColDet['oDivMaxHigh'] = [331, 0xFE, 1, 0]
DictColDet['oValDivMax'] = [331, 0x7F, 1, 7]
DictColDet['oFrcDivLow'] = [332, None, 8, 0]
DictColDet['oFrcDivHigh'] = [333, 0xFE, 1, 0]
DictColDet['oValFrcDiv'] = [333, 0x7F, 1, 7]
DictColDet['oFR_CLK'] = [334, 0xF0, 4, 0]
DictColDet['oValFR_CLK'] = [334, 0x7F, 1, 7]
DictColDet['oOTPWriteTime'] = [335, None, 8, 0]
# DictColDet['oDnxRsmrstWidth'] = [336, None, 8, 0]
# DictColDet['oDnxDPOkWidth'] = [337, None, 8, 0]
DictColDet['oECTestMode0'] = [338, None, 16, 0]
# 6694
DictColDet['oChipTesterID'] = [340, None, 32, 0]
DictColDet['oSysFwSigPubKeySts'] = [363, None, 8, 0]
DictColDet['oUserDataField'] = [492, None, 160, 0]

DictColDet['oUserDataField1'] = [512, None, 1024, 0]
DictColDet['oUserDataField2'] = [640, None, 1024, 0]
DictColDet['oUserDataField3'] = [768, None, 1024, 0]
DictColDet['oUserDataField4'] = [896, None, 1024, 0]

DictColDet['EcFwPubKey0'] = [32, None, 512, 0]
DictColDet['EcFwPubKey1'] = [96, None, 512, 0]
DictColDet['oOtpRegion0Digest'] = [160, None, 256, 0]
DictColDet['oSessPrivKey'] = [192, None, 256, 0]
DictColDet['oAESKey'] = [288, None, 256, 0]
DictColDet['SySFwPubKey0'] = [364, None, 512, 0]
DictColDet['SySFwPubKey1'] = [428, None, 512, 0]
