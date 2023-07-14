#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import logging
import Util
import xml.etree.ElementTree as ET
import FunctionDefine as Fn
from shutil import move
from shutil import copy
from shutil import rmtree
from OpenSSL import *
from XmlParser import *
from Crypto import *
from NuMsCNG import *
from NuPKCS11 import PKCS11_SignFile

# region Variable
Dict_Config = {}
Dict_OpenSSL = {}
Dict_CNG = {}
Dict_PKCS11 = {}
Dict_File = {}
Dict_FW_H = {}
Dict_OTP_H = {}
Dict_OTP = {}
bPriKey = 0
bPubKey = 0
bPubHash1 = 0
bPubHash2 = 0
bFWPubHash1 = 0
bFWPubHash2 = 0
bSessPrivKey = 0
FWLength = 0
BBLength = 0
BBEnLength = 0
BBImage_InFile = None
BBLength_InFile = 0
BBOffset_InFile = 0
BBWorkRAM_InFile = 0
BBAlign = 0
RAMCodeLength = 0
RAMCodeEnLength = 0
CombineLength = 0
CombineEnLength = 0

Seg4Align = 0

FlashCodeLength = 0
DataCodeLength = 0
hUserFWEntryPoint = 0
hUserFWRamCodeFlashStart = 0
hUserFWRamCodeFlashEnd = 0
hUserFWRamCodeRamStart = 0
HashTypeforECKey = 256
HashTypeforSystemKey = 256
Signature = True
GenOTP = 0
AESencrypt = 0
OtpAlign = 0
RAMCodePadding = 0
hReservedField4_InFile = 0
# hHook1Ptr = 0
# hHook2Ptr = 0
# hHook3Ptr = 0
# hHook4Ptr = 0
hSeg1Offset = 0
hSeg2Offset = 0
hSeg3Offset = 0
hSeg4Offset = 0
hSeg1Size = 0
hSeg2Size = 0
hSeg3Size = 0
hSeg4Size = 0

hHashTotal = []
hAESAdd = []

hSeg1_OEM = []

# endregion

# -----Function Field-----


def GenFWHeader():
    # os.chdir(Util.Path_Key)
    # if(bPriKey and (Util.CryptoSelect == 0 or Util.CryptoSelect == 2)):
    #     FileList = [Dict_OpenSSL['EcFwSigKey']]
    #     Fn.CheckFile(FileList)
    os.chdir(Util.Path_Input)
    Fn.CheckOutputDir()
    try:
        global hSeg1Size, hSeg2Size, hSeg3Size, hSeg4Size
        global hSeg1Offset, hSeg2Offset, hSeg3Offset, hSeg4Offset
        global FlashCodeLength, RAMCodeLength, DataCodeLength, BBAlign
        #global RAMCodeAlignLength

        hash_dat = []

        print("Generate FW Header...")
        ListFWHeaderCol = []
        ListFWHeaderCol_Sign = []

        # ListFWHeaderCol_Sign is for Signature Column Start
        # hActiveECFwOffset
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hActiveECFwOffset'], 0).to_bytes(2, byteorder='little')))

        # hRecoveryEcFwOffset
        ListFWHeaderCol_Sign.append(bytearray([0]*2))
            # (int(Dict_FW_H['hRecoveryEcFwOffset'], 0).to_bytes(2, byteorder='little')))

        # hSystemECFWOffset
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hSystemECFWOffset'], 0).to_bytes(4, byteorder='little')))

        # hDevMode
        temp = [0, 0, 1, 1, 0, 0, 0, 0]
        ListFWHeaderCol_Sign.append(Fn.ConvertBitArray2Byte(temp))

        # hFlashLockReg0
        ListFWHeaderCol_Sign.append(bytearray([0]))
        ListFWHeaderCol_Sign.append(bytearray([0]))

        # hBackUpReq
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hEcFwRegionSize'], 0).to_bytes(1, byteorder='big')))

        # hUserFWEntryPoint
        data = hUserFWEntryPoint
        # (int(Dict_FW_H['hUserFWEntryPoint'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hUserFWEntryPoint'] is not None) else hUserFWEntryPoint
        ListFWHeaderCol_Sign.append(data)

        # hUserFWRamCodeFlashStart
        data = hUserFWRamCodeFlashStart
        # (int(Dict_FW_H['hUserFWRamCodeFlashStart'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hUserFWRamCodeFlashStart'] is not None) else hUserFWRamCodeFlashStart
        ListFWHeaderCol_Sign.append(data)

        # hUserFWRamCodeFlashEnd
        data = hUserFWRamCodeFlashEnd
        # (int(Dict_FW_H['hUserFWRamCodeFlashEnd'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hUserFWRamCodeFlashEnd'] is not None) else hUserFWRamCodeFlashEnd
        ListFWHeaderCol_Sign.append(data)

        # hUserFWRamCodeRamStart
        data = hUserFWRamCodeRamStart
        # (int(Dict_FW_H['hUserFWRamCodeRamStart'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hUserFWRamCodeRamStart'] is not None) else hUserFWRamCodeRamStart
        ListFWHeaderCol_Sign.append(data)

        # hImageLen (not use)
        ListFWHeaderCol_Sign.append(
            (FWLength).to_bytes(4, byteorder='big'))

        # hSigPubKeyHashIdx
        # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #     _idx = Dict_OpenSSL['EcFwSigKey_Idx']
        # elif(Util.CryptoSelect == 1):
        #     _idx = Dict_CNG['SignCert_Idx']

        if(HashTypeforECKey == 256):
            temp = [0, 0, 0, 0, 0, 0, 0, 0]
        else:
            temp = [0, 1, 0, 0, 0, 0, 0, 0]

        ListFWHeaderCol_Sign.append(Fn.ConvertBitArray2Byte(temp))

        # hMajorVer
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hMajorVer'], 0).to_bytes(1, byteorder='little')))

        # hMinorVer
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hMinorVer'], 0).to_bytes(2, byteorder='little')))

        # hRevokeKey, hRevokeKeyInv
        #ListFWHeaderCol_Sign.append(Fn.RevokeKey(Dict_FW_H['hRevokeKey']))
        ListFWHeaderCol_Sign.append(bytearray([0]*2))

        # hBBSize
        # if Dict_File['BBImage'] is None:
        ListFWHeaderCol_Sign.append(
                (BBLength_InFile).to_bytes(2, byteorder='little'))
        # else:
        #     ListFWHeaderCol_Sign.append(
        #         (BBLength).to_bytes(2, byteorder='little'))

        # hBBOffset
        # if Dict_File['BBImage'] is None:
        data = (BBOffset_InFile).to_bytes(4, byteorder='little')
        # else:
        #     data = (0).to_bytes(4, byteorder='little') if (BBLength == 0) else (
        #         hSeg3Offset + FlashCodeLength + RAMCodeLength + DataCodeLength + BBAlign).to_bytes(4, byteorder='little')
        ListFWHeaderCol_Sign.append(data)

        # print('bb_image_offset',  '{:x}'.format(
        #     hSeg3Offset + FlashCodeLength + RAMCodeLength + DataCodeLength + BBAlign))

        # hBuildersPubKey
        # if(AESencrypt):
        #     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #         data = OpenSSL_GetPubKey(Util.sAESPubKeyName, totallen=64)
        #     elif(Util.CryptoSelect == 1):
        #         Path_Old = Fn.ForwardPath(Util.Path_Key)
        #         data = Fn.getFileBin(Util.sAESPubKeyName)
        #         Fn.ForwardPath(Path_Old)
        # else:
        data = Fn.ReservedData(64 * 8)
        ListFWHeaderCol_Sign.append(data)

        # hBBWorkRAM
        # if Dict_FW_H['hBBWorkRAM'] is None:
        ListFWHeaderCol_Sign.append(
                (BBWorkRAM_InFile.to_bytes(4, byteorder='little')))
        # else:
        #     ListFWHeaderCol_Sign.append(
        #         (int(Dict_FW_H['hBBWorkRAM'], 0).to_bytes(4, byteorder='little')))

        # hSigPubKey
        # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #     if((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0):
        #         data = OpenSSL_GetPubKey(Dict_OpenSSL['EcFwPubKey0'], totallen=512) if (
        #             bPubKey) else Fn.ReservedData(512 * 8)
        #     elif((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 1):
        #         data = OpenSSL_GetPubKey(Dict_OpenSSL['EcFwPubKey1'], totallen=512) if (
        #             bPubKey) else Fn.ReservedData(512 * 8)
        # elif(Util.CryptoSelect == 1):
        #     if((int)(Dict_CNG['SignCert_Idx']) == 0):
        #         data = CNG_ExportPubKey(Dict_CNG['EcFwCert0_Subject'], totallen=512) if (
        #             bPubKey) else Fn.ReservedData(512 * 8)
        #     elif((int)(Dict_CNG['SignCert_Idx']) == 1):
        #         data = CNG_ExportPubKey(Dict_CNG['EcFwCert1_Subject'], totallen=512) if (
        #             bPubKey) else Fn.ReservedData(512 * 8)
        data = Fn.ReservedData(512 * 8)
        ListFWHeaderCol_Sign.append(data)

        # hRamCodeHash
        hash_dat.clear()
        hash_dat.append(bytearray(Fn.getFileBin(Util.sRAMCodeName)))

        if(BBLength != 0) or (BBLength_InFile != 0):
            hash_dat.append(bytearray(Fn.getFileBin(Util.sBBName)))

        # check data
        data = CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
            hash_dat, 'RAMCodeBBCombine'), 256)

        ListFWHeaderCol_Sign.append(data)

        # hOEMversion
        ListFWHeaderCol_Sign.append(Fn.Text2Bin(Dict_FW_H['hOEMversion'], 8))

        # hReleaseDate
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hReleaseDate'], 0).to_bytes(3, byteorder='big')))

        # hProjectID
        ListFWHeaderCol_Sign.append(
            (int(Dict_FW_H['hProjectID'], 0).to_bytes(2, byteorder='big')))

        # hReservedField4
        ListFWHeaderCol_Sign.append(hReservedField4_InFile)

        # hHook1Ptr
        # data = (int(Dict_FW_H['hHook1Ptr'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hHook1Ptr'] is not None) else Fn.getFilePoint(Dict_File['FirmwareImage'], 1200, 4)
        ListFWHeaderCol_Sign.append(bytearray([0]*4))

        # # hHook2Ptr
        # data = (int(Dict_FW_H['hHook2Ptr'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hHook2Ptr'] is not None) else Fn.getFilePoint(Dict_File['FirmwareImage'], 1204, 4)
        ListFWHeaderCol_Sign.append(bytearray([0]*4))

        # hHook3Ptr
        # data = (int(Dict_FW_H['hHook3Ptr'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hHook3Ptr'] is not None) else Fn.getFilePoint(Dict_File['FirmwareImage'], 1208, 4)
        ListFWHeaderCol_Sign.append(bytearray([0]*4))

        # hHook4Ptr
        # data = (int(Dict_FW_H['hHook4Ptr'], 0).to_bytes(4, byteorder='little')) if (
        #     Dict_FW_H['hHook4Ptr'] is not None) else Fn.getFilePoint(Dict_File['FirmwareImage'], 1212, 4)
        ListFWHeaderCol_Sign.append(bytearray([0]*4))

        # hFwSeg1Offset / hFwSeg1Size (header)
        hSeg1Offset = Fn.getFilePoint(Dict_File['FirmwareImage'], 1216, 4)
        hSeg1Size = Fn.Bytes2Int(Fn.getFilePoint(Dict_File['FirmwareImage'], 1220, 4), 'little') - Fn.Bytes2Int(hSeg1Offset, 'little')
        ListFWHeaderCol_Sign.append(hSeg1Offset)
        ListFWHeaderCol_Sign.append(hSeg1Size.to_bytes(4, byteorder='little'))

        # hFwSeg2Offset / hFwSeg2Size (hook)
        hSeg2Offset = Fn.getFilePoint(Dict_File['FirmwareImage'], 1224, 4)
        ListFWHeaderCol_Sign.append(hSeg2Offset)
        ListFWHeaderCol_Sign.append(hSeg2Size.to_bytes(4, byteorder='little'))

        # hFwSeg3Offset / hFwSeg3Size (main f/w)
        ListFWHeaderCol_Sign.append(
            hSeg3Offset.to_bytes(4, byteorder='little'))
        ListFWHeaderCol_Sign.append(hSeg3Size.to_bytes(4, byteorder='little'))

        # hFwSeg4Offset / hFwSeg4Size
        ListFWHeaderCol_Sign.append(
            hSeg4Offset.to_bytes(4, byteorder='little'))
        ListFWHeaderCol_Sign.append(hSeg4Size.to_bytes(4, byteorder='little'))

        # AES Encryption
        # aes_type = 0 #int(Dict_OTP['oAESDecryptEn'], 10)
        # if(AESencrypt):
        #     # AES-GSM use header's hash (0-719) as aes_add
        #     if(aes_type == 2):
        #         hash_dat.clear()
        #         hash_dat.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
        #             ListFWHeaderCol_Sign, 'aes_add_src'), 256))
        #         Fn.GenBinFilefromList(hash_dat, 'aes_add')
        #         Fn.DeleteFile('aes_add_src')

        #     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #         Util.sRAMCodeName_Encrypt = OpenSSL_AES_Encrypt(
        #             aes_type, Util.sRAMCodeAlignName, '_MyIV', '_MyKey', 'aes_add', 'RAMCodeFile_Encrypt', len(Fn.getFileBin(Util.sRAMCodeAlignName)))
        #     elif(Util.CryptoSelect == 1):
        #         Util.sRAMCodeName_Encrypt = CNG_SymEncrypt(
        #             Util.sRAMCodeAlignName, '_MyIV', '_MyKey', 'RAMCodeFile_Encrypt')

        #     if(BBLength != 0):
        #         if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #             Util.sBBName_Encrypt = OpenSSL_AES_Encrypt(
        #                 aes_type, Util.sBBName, '_MyIV', '_MyKey', 'aes_add', 'BBImage_Encrypt', BBLength)
        #         elif(Util.CryptoSelect == 1):
        #             Util.sBBName_Encrypt = CNG_SymEncrypt(
        #                 Util.sBBName, '_MyIV', '_MyKey', 'BBImage_Encrypt')

        #     Fn.DeleteFile('_MyIV')
        #     Fn.DeleteFile('_MyKey')
        #     Fn.DeleteFile('aes_add')

         # Added AES tag to hRamCodeAESTag and hBootBlockAESTag
        # if(AESencrypt and (aes_type == 2)):
        #     ListFWHeaderCol_Sign.append(
        #         Fn.getFileBin('RAMCodeFile_Encrypt_tag')[0:16])
        #     Fn.DeleteFile('RAMCodeFile_Encrypt_tag')

        #     if(BBLength != 0):
        #         ListFWHeaderCol_Sign.append(
        #             Fn.getFileBin('BBImage_Encrypt_tag')[0:16])
        #         Fn.DeleteFile('BBImage_Encrypt_tag')
        #     else:
        #         ListFWHeaderCol_Sign.append(bytearray([0]*16))
        # else:
        ListFWHeaderCol_Sign.append(bytearray([0]*32))

        # Should ignore "hImageHash" here, get index for inserting later
        # hImageHashIndex = len(ListFWHeaderCol_Sign)

        # print('HashTypeforECKey', '{:d}'.format(HashTypeforECKey))

        # Seg 1 (Header)
        hHashTotal.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
            ListFWHeaderCol_Sign, 'seg_1'), HashTypeforECKey))
        if HashTypeforECKey == 256:
            # pad 32byte for sha256
            hHashTotal.append(bytearray([0]*32))

        # Seg 2 (Hook)
        if hSeg2Size > 0:
            hash_dat.clear()
            hash_dat.append(Fn.getFileBin(Util.sHookSegName))
            hHashTotal.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
                hash_dat, 'seg_2'), HashTypeforECKey))
            if HashTypeforECKey == 256:
                # pad 32byte for sha256
                hHashTotal.append(bytearray([0]*32))
        else:
            hHashTotal.append(bytearray([0]*64))

        # Seg 3 (FirmwareImage, flash + ram_code + data_code + BB)
        hash_dat.clear()
        hash_dat.append(
            bytearray(Fn.getFileBin(Util.sFlashCodeName)))
        if(AESencrypt):
            hash_dat.append(
                bytearray(Fn.getFileBin(Util.sRAMCodeName_Encrypt)))
        else:
            hash_dat.append(
                bytearray(Fn.getFileBin(Util.sRAMCodeName)))
        hash_dat.append(
            bytearray(Fn.getFileBin(Util.sDataCodeName)))

        if(BBLength != 0):
            hash_dat.append(Fn.ReservedData(BBAlign*8))
            if(AESencrypt):
                hash_dat.append(
                    bytearray(Fn.getFileBin(Util.sBBName_Encrypt)))
            else:
                hash_dat.append(bytearray(Fn.getFileBin(Util.sBBName)))

        # hImageHash
        hHashTotal.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
            hash_dat, 'seg_3'), HashTypeforECKey))
        if HashTypeforECKey == 256:
            # pad 32byte for sha256
            hHashTotal.append(bytearray([0]*32))

        # Seg 4 (Fantable)
        if hSeg4Size > 0:
            hash_dat.clear()
            hash_dat.append(Fn.getFileBin(Util.sDataSegName))
            hHashTotal.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
                hash_dat, 'seg_4'), HashTypeforECKey))
            if HashTypeforECKey == 256:
                # pad 32byte for sha256
                hHashTotal.append(bytearray([0]*32))
        else:
            hHashTotal.append(bytearray([0]*64))

        Fn.GenBinFilefromList(hHashTotal, 'HashTotal')

        # for debug
        # sign_hash = []
        # sign_hash.append(CryptoDigest(
        #     Util.Path_Input, Fn.GenBinFilefromList(hHashTotal, 'HashTotal'), HashTypeforECKey))
        # Fn.GenBinFilefromList(sign_hash, 'sign_hash')``

        # hImageTag
        ListFWHeaderCol.append(Dict_FW_H['hImageTag'].encode('ascii'))

        # gen sign
        # print('Signature', '{:x}'.format(Signature))
        # print('oRSAPKCPAD', '{:x}'.format((int)(Dict_OTP['oRSAPKCPAD'])))
        # print('HashTypeforECKey', '{:d}'.format(HashTypeforECKey))
        # if(Signature):
        #     if(int(Dict_Config["CryptoSelect"]) == 0):
        #         if(bPriKey != 1):
        #             Fn.OutputString(1, 'There is no private key.')
        #             raise Util.SettingError("<EcFwSigKey> setting error")
        #         ListFWHeaderCol.append(OpenSSL_SignFile(HashTypeforECKey, 0, Fn.GenBinFilefromList(
        #             hHashTotal, 'SignField'), Dict_OpenSSL['EcFwSigKey']))
        #     elif(int(Dict_Config["CryptoSelect"]) == 1):
        #         if((int)(Dict_CNG['SignCert_Idx']) == 0):
        #             ListFWHeaderCol.append(CNG_SignFile(
        #                 Dict_CNG["EcFwCert0_Subject"], 0, HashTypeforECKey, 0, Fn.GenBinFilefromList(hHashTotal, 'SignField')))
        #         elif((int)(Dict_CNG['SignCert_Idx']) == 1):
        #             ListFWHeaderCol.append(CNG_SignFile(
        #                 Dict_CNG["EcFwCert1_Subject"], 0, HashTypeforECKey, 0, Fn.GenBinFilefromList(hHashTotal, 'SignField')))
        #     else:
        #         ListFWHeaderCol.append(PKCS11_SignFile(HashTypeforECKey, Fn.GenBinFilefromList(
        #             hHashTotal, 'SignField'), Dict_PKCS11['KeyID']))
        # else:
            # ListFWHeaderCol.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
            #     hHashTotal, 'SignField'), HashTypeforECKey))
        ListFWHeaderCol.append(bytearray([0]*512))

        # hOtpImgHdrOffset
        # if(GenOTP == 1):
        #     if(AESencrypt):
        #         ListFWHeaderCol.append(
        #             (FWLength + OtpAlign).to_bytes(4, byteorder='big'))
        #     else:
        #         ListFWHeaderCol.append(
        #             (FWLength + OtpAlign).to_bytes(4, byteorder='big'))
        # else:
        ListFWHeaderCol.append(Fn.ReservedData(32))
        ListFWHeaderCol.append(Fn.ReservedData(Util.hReservedField0))

        # Gen Header list
        for i in range(0, len(ListFWHeaderCol_Sign)):
            ListFWHeaderCol.append(ListFWHeaderCol_Sign[i])

        # Check total size, 1280 bytes
        total = 0
        for i in range(0, len(ListFWHeaderCol)):
            total += len(ListFWHeaderCol[i])
        if(total != Util.FW_Image_header_len):
            raise Util.LengthError('FW header length error %s' % total)
        logging.info('GenFWHeader OK')

        Fn.GenBinFilefromList(ListFWHeaderCol, 'ListFWHeaderCol')

        return ListFWHeaderCol
    except BaseException as e:
        Fn.OutputString(1, 'Gen FW Header failed')
        Fn.OutputString(1, 'Error in %s : %s' % (GenFWHeader.__name__, e))
        logging.error('Error in %s : %s' % (GenFWHeader.__name__, e))
        Fn.ClearTmpFiles()


def GenOTPHeader():
    os.chdir(Util.Path_Input)
    FileList = ['OutputOTPImage']
    Fn.CheckFile(FileList)
    try:
        print("Generate OTP Header...")
        ListOTPHeaderCol = []
        ListOTPHeaderCol.append(Dict_OTP_H['hOtpImgTag'].encode('ascii'))
        ListOTPHeaderCol.append(CryptoDigest(
            Util.Path_Input, 'OutputOTPImage', 256))
        ListOTPHeaderCol.append((Util.OTP_Image).to_bytes(2, byteorder='big'))
        ListOTPHeaderCol.append(Fn.ReservedData(Util.hOtpImgRsvd))
        total = 0
        for i in range(0, len(ListOTPHeaderCol)):
            total += len(ListOTPHeaderCol[i])
        if(total != Util.OTP_Image_header_len):  # 64 bytes
            raise Util.LengthError('OTP header length error %s' % total)
        logging.info('GenOTPHeader OK')
        return ListOTPHeaderCol
    except BaseException as e:
        Fn.OutputString(1, 'Gen OTP Header failed')
        Fn.OutputString(1, 'Error in %s : %s' % (GenOTPHeader.__name__, e))
        logging.error('Error in %s : %s' % (GenOTPHeader.__name__, e))
        Fn.ClearTmpFiles()


def GenOTPImage():
    # os.chdir(Util.Path_Key)
    # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
    #     FileList = [Dict_OpenSSL['EcFwPubKey0']]
    #     if(bPubHash1):
    #         Fn.CheckFile(FileList)
    #     FileList = [Dict_OpenSSL['EcFwPubKey1']]
    #     if(bPubHash2):
    #         Fn.CheckFile(FileList)
    #     FileList = [Dict_OpenSSL['SySFwPubKey0']]
    #     if(bFWPubHash1):
    #         Fn.CheckFile(FileList)
    #     FileList = [Dict_OpenSSL['SySFwPubKey1']]
    #     if(bFWPubHash2):
    #         Fn.CheckFile(FileList)
    #     FileList = [Dict_OpenSSL['SessPrivKey']]
    #     if(bSessPrivKey):
    #         Fn.CheckFile(FileList)

    try:
        print("Generate OTP Image...")
        ListOTPImageCol = []
        # 0
        temp = [int(Dict_OTP['oNotTrySysIfSPI1'], 0), int(Dict_OTP['oNotTryMafAndAMD'], 0), int(Dict_OTP['oNotTrySysIfFIUShd'], 0), int(Dict_OTP['oNotTrySysIfFIUPvt'], 0), int(
            Dict_OTP['oStrapMode2'], 0), int(Dict_OTP['oStrapMode1'], 0), Fn.ConvertInt2Bin(Dict_OTP['oFlashConnection'], 2)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 1
        temp = [Fn.ConvertInt2Bin(Dict_OTP['oMCPFlashSize'], 2), 0, Fn.ConvertInt2Bin(Dict_OTP['oECPTRCheckCRC'], 1),
                Fn.ConvertInt2Bin(Dict_OTP['oSpiQuadPEn'], 1), Fn.ConvertInt2Bin(
            Dict_OTP['oSPIP4BMode'], 1),
            Fn.ConvertInt2Bin(Dict_OTP['oFIUShr4BMode'], 1), int(Dict_OTP['oNotTrySysIfFIUBkp'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 2
        temp = [0, 0, Fn.ConvertInt2Bin(Dict_OTP['oSPIPFLMode'], 2), Fn.ConvertInt2Bin(
            Dict_OTP['oFIUShrFLMode'], 2), Fn.ConvertInt2Bin(Dict_OTP['oMCPFLMode'], 2)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 3
        temp = [Fn.ConvertInt2Bin(Dict_OTP['oFwNotUse2NStep'], 1), Fn.ConvertInt2Bin(Dict_OTP['oFwNotUse4KStep'], 1), Fn.ConvertInt2Bin(
            Dict_OTP['oSPIPClkDiv'], 2), Fn.ConvertInt2Bin(Dict_OTP['oSPIMClkDiv'], 2), Fn.ConvertInt2Bin(Dict_OTP['oFIUClkDiv'], 2)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 4
        temp = [int(Dict_OTP['oUnmapRomBfXferCtl'], 0), 0, int(Dict_OTP['oTryBootIfAllCrashed'], 0), int(Dict_OTP['oHaltIfOnlyMafValid'], 0), int(
            Dict_OTP['oHaltIfActiveRollbk'], 0), int(Dict_OTP['oHaltIfMafRollbk'], 0), int(Dict_OTP['oSecurityLvl'], 0), int(Dict_OTP['oSecureBoot'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 5
        temp = [int(Dict_OTP['oOtpRgn6Lock'], 0), int(Dict_OTP['oOtpRgn5Lock'], 0), int(Dict_OTP['oOtpRgn4Lock'], 0), int(Dict_OTP['oOtpRgn3Lock'], 0), int(Dict_OTP['oOtpRgn2Lock'], 0), int(
            Dict_OTP['oHWCfgFieldLock'], 0), int(Dict_OTP['oAESKeyLock'], 0), int(Dict_OTP['oDisableDBGAtRst'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 6-7
        ListOTPImageCol.append(
            int(Dict_OTP['oSecEvnLogLoc'], 0).to_bytes(2, byteorder='little'))

        # 8
        temp = [int(Dict_OTP['oSHA512Used'], 0), int(Dict_OTP['oLongKeyUsed'], 0),
                Fn.ConvertInt2Bin(Dict_OTP['oRevokeKeySts'], 2), Fn.ConvertInt2Bin(Dict_OTP['oECPubKeySts'], 2), Fn.ConvertInt2Bin(Dict_OTP['oRSAPubKeySts'], 2)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 9-15
        ListOTPImageCol.append(Fn.ReservedData(7 * 8))

        # 16
        temp = [0, 0, Fn.ConvertInt2Bin(Dict_OTP['oAESDecryptEn'], 2), 0, 0, int(
            Dict_OTP['oRSAPKCPAD'], 0), int(Dict_OTP['oLongKeySel'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 17
        temp = [int(Dict_OTP['oClrRamExitRom'], 0), int(Dict_OTP['oOTPRegionRdLock'], 0), 0, int(
            Dict_OTP['oOlnyLogCriticalEvent'], 0), int(Dict_OTP['oSkipCtyptoSelfTest'], 0), int(Dict_OTP['oRetryLimitEn'], 0), 0, 0]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 18
        temp = [0, 0, 0, 0,  int(Dict_OTP['oNotUpdateToPrvFw'], 0), int(Dict_OTP['oTryBootNotCtrlFWRdy'], 0), int(
            Dict_OTP['oNoWaitVSpiExist'], 0), int(Dict_OTP['oVSpiExistNoTimeOut'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 19
        temp = [0, 0, 0, 0, 0,  0, int(Dict_OTP['oSysPfrWP1En'], 0), int(
            Dict_OTP['oSysPfrWP0En'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 20 - 22
        ListOTPImageCol.append(Fn.ReservedData(1 * 8))
        ListOTPImageCol.append(
            int(Dict_OTP['oOTPDatValid0'], 0).to_bytes(1, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oOTPDatValid1'], 0).to_bytes(1, byteorder='big'))

        # 23 - 27
        temp = [0, 0, 0, Fn.ConvertInt2Bin(
            Dict_OTP['oLed1Pole'], 1), Fn.ConvertInt2Bin(Dict_OTP['oLed1Sel'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        temp = [0, 0, 0, Fn.ConvertInt2Bin(
            Dict_OTP['oLed2Pole'], 1), Fn.ConvertInt2Bin(Dict_OTP['oLed2Sel'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        temp = [Fn.ConvertInt2Bin(Dict_OTP['oLedSysRbkBlkDef'], 4), Fn.ConvertInt2Bin(
            Dict_OTP['oLedActRbkBlkDef'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        temp = [Fn.ConvertInt2Bin(Dict_OTP['oLedSysOnlyBlkDef'], 4), Fn.ConvertInt2Bin(
            Dict_OTP['oLedFwCpyBlkDef'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        temp = [Fn.ConvertInt2Bin(Dict_OTP['oLedCryptoTestFailBlkDef'], 4), Fn.ConvertInt2Bin(
            Dict_OTP['oLedAllCrashBlkDef'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        # 28 - 31
        ListOTPImageCol.append(Fn.ReservedData(4 * 8))

        # 32 - 95
        if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
            data = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
                Dict_OpenSSL['EcFwPubKey0'], reFile=1), HashTypeforECKey) if (bPubHash1) else Fn.ReservedData(64 * 8)
        elif(Util.CryptoSelect == 1):
            data = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
                Dict_CNG['EcFwCert0_Subject'], reFile=1), HashTypeforECKey) if (bPubHash1) else Fn.ReservedData(64 * 8)
        ListOTPImageCol.append(data)
        ListOTPImageCol.append(Fn.ReservedData(
            32 * 8)) if ((bPubHash1) and (HashTypeforECKey == 256)) else 0

        # 96 - 159
        if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
            data = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
                Dict_OpenSSL['EcFwPubKey1'], reFile=1), HashTypeforECKey) if (bPubHash2) else Fn.ReservedData(64 * 8)
        elif(Util.CryptoSelect == 1):
            data = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
                Dict_CNG['EcFwCert1_Subject'], reFile=1), HashTypeforECKey) if (bPubHash2) else Fn.ReservedData(64 * 8)
        ListOTPImageCol.append(data)
        ListOTPImageCol.append(Fn.ReservedData(
            32 * 8)) if ((bPubHash2) and (HashTypeforECKey == 256)) else 0

        # 160 - 191
        ListOTPImageCol.append(Fn.ReservedData(32 * 8))

        # 192 - 223
        if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
            data = OpenSSL_GetPrivKey(Dict_OpenSSL['SessPrivKey']) if (
                bSessPrivKey) else Fn.ReservedData(32 * 8)
        elif(Util.CryptoSelect == 1):
            data = CNG_ExportPrivKey(Dict_CNG['AESCert_Subject']) if (
                bSessPrivKey) else Fn.ReservedData(32 * 8)
        ListOTPImageCol.append(data)

        # 224 - 287
        ListOTPImageCol.append(Fn.ReservedData(64 * 8))

        # 288 - 319
        if Dict_OTP['oAESKey'] is None:
            ListOTPImageCol.append(Fn.ReservedData(32 * 8))
        else:
            ListOTPImageCol.append(OpenSSL_GetAESKey(Dict_OTP['oAESKey']))

        # 320 - 351
        temp = [int(Dict_OTP['oMCPSel'], 0), int(Dict_OTP['oMCPRdEdge'], 0), Fn.ConvertInt2Bin(Dict_OTP['oMCPRdDly'], 3), int(Dict_OTP['oMCUClkNNotDiv2'], 0), int(
            Dict_OTP['oAESNotSupport'], 0), int(Dict_OTP['oPartID'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oANAD2Low'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValANAD'], 0), 0, 0, 0, 0,
                0, 0, int(Dict_OTP['oANAD2High'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        temp = [int(Dict_OTP['oValDevId'], 0), 0, 0,
                Fn.ConvertInt2Bin(Dict_OTP['oDevId'], 5)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oRSMRST_L'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValRSMRST_L'], 0), 0, 0, 0, 0, 0, 0, 0]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oRSMRST_Sys'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValRSMRST_Sys'], 0), 0, 0, 0, 0, 0, 0, 0]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oDivMinLow'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValDivMin'], 0), 0, 0, 0, 0,
                0, 0, int(Dict_OTP['oDivMinHigh'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oDivMaxLow'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValDivMax'], 0), 0, 0, 0, 0,
                0, 0, int(Dict_OTP['oDivMaxHigh'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oFrcDivLow'], 0).to_bytes(1, byteorder='big'))

        temp = [int(Dict_OTP['oValFrcDiv'], 0), 0, 0, 0, 0,
                0, 0, int(Dict_OTP['oFrcDivHigh'], 0)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        temp = [int(Dict_OTP['oValFR_CLK'], 0), 0, 0, 0,
                Fn.ConvertInt2Bin(Dict_OTP['oFR_CLK'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))
        ListOTPImageCol.append(
            int(Dict_OTP['oOTPWriteTime'], 0).to_bytes(1, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oDnxRsmrstWidth'], 0).to_bytes(1, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oDnxDPOkWidth'], 0).to_bytes(1, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oECTestMode0'], 0).to_bytes(2, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oChipTesterID'], 0).to_bytes(4, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oVSpiExistWaitCnter'], 0).to_bytes(1, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oPwmLedAdj'], 0).to_bytes(1, byteorder='big'))

        temp = [Fn.ConvertInt2Bin(Dict_OTP['oFPRED'], 4),
                0, 0, Fn.ConvertInt2Bin(Dict_OTP['oAHB6DIV'], 2)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        temp = [Fn.ConvertInt2Bin(Dict_OTP['oAPB2DIV'], 4), Fn.ConvertInt2Bin(
            Dict_OTP['oAPB1DIV'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        temp = [int(Dict_OTP['oRefOTPClk']), 0, 0, int(
            Dict_OTP['oXFRANGE'], 0), Fn.ConvertInt2Bin(Dict_OTP['oAPB3DIV'], 4)]
        ListOTPImageCol.append(Fn.ConvertBitArray2Byte(temp))

        ListOTPImageCol.append(Fn.ReservedData(3 * 8))

        # 352 - 362
        ListOTPImageCol.append(Fn.ReservedData(11 * 8))

        # 363
        ListOTPImageCol.append(
            int(Dict_OTP['oSysFwSigPubKeySts'], 0).to_bytes(1, byteorder='big'))
        if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
            data = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
                Dict_OpenSSL['SySFwPubKey0'], reFile=1), HashTypeforSystemKey) if (bFWPubHash1) else Fn.ReservedData(64 * 8)
        elif(Util.CryptoSelect == 1):
            data = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
                Dict_CNG['SySFwCert0_Subject'], reFile=1), HashTypeforSystemKey) if (bFWPubHash1) else Fn.ReservedData(64 * 8)
        ListOTPImageCol.append(data)
        ListOTPImageCol.append(Fn.ReservedData(
            32 * 8)) if ((bFWPubHash1) and (HashTypeforSystemKey == 256)) else 0
        if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
            data = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
                Dict_OpenSSL['SySFwPubKey1'], reFile=1), HashTypeforSystemKey) if (bFWPubHash2) else Fn.ReservedData(64 * 8)
        elif(Util.CryptoSelect == 1):
            data = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
                Dict_CNG['SySFwCert1_Subject'], reFile=1), HashTypeforSystemKey) if (bFWPubHash2) else Fn.ReservedData(64 * 8)
        ListOTPImageCol.append(data)
        ListOTPImageCol.append(Fn.ReservedData(
            32 * 8)) if ((bFWPubHash2) and (HashTypeforSystemKey == 256)) else 0

        # 364 - 511
        ListOTPImageCol.append(
            int(Dict_OTP['oUserDataField'], 0).to_bytes(20, byteorder='big'))

        # 512 - 1023
        ListOTPImageCol.append(
            int(Dict_OTP['oUserDataField1'], 0).to_bytes(128, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oUserDataField2'], 0).to_bytes(128, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oUserDataField3'], 0).to_bytes(128, byteorder='big'))
        ListOTPImageCol.append(
            int(Dict_OTP['oUserDataField4'], 0).to_bytes(128, byteorder='big'))

        total = 0
        for i in range(0, len(ListOTPImageCol)):
            total += len(ListOTPImageCol[i])
        if(total != Util.OTP_Image):  # 1024 bytes
            logging.info('OTP length error %s' % total)
            raise Util.LengthError("OTP length = %s" % total)
        os.chdir(Util.Path_Input)
        with open('OutputOTPImage', 'wb') as fout:
            for i in range(0, len(ListOTPImageCol)):
                fout.write(ListOTPImageCol[i])
        logging.info('GenOTPImage OK')
    except BaseException as e:
        Fn.OutputString(1, 'Error in %s : %s' % (GenOTPImage.__name__, e))
        logging.error('Error in %s : %s' % (GenOTPImage.__name__, e))
        Fn.ClearTmpFiles()


def GenHeader():
    global OtpAlign, FWLength, BBLength, BBEnLength, BBAlign, BBImage_InFile, BBLength_InFile, BBOffset_InFile, BBWorkRAM_InFile, RAMCodeLength
    global FlashCodeLength, DataCodeLength
    # 6694
    global hUserFWEntryPoint, hUserFWRamCodeFlashStart, hUserFWRamCodeFlashEnd, hUserFWRamCodeRamStart, hReservedField4_InFile
    global hSeg1Size, hSeg2Size, hSeg3Size, hSeg4Size
    global hSeg1Offset, hSeg2Offset, hSeg3Offset, hSeg4Offset
    global Seg4Align

    #, #RAMCodeEnLength, #RAMCodePadding, #CombineLength#, CombineEnLength
    #global hHook1Ptr, hHook2Ptr, hHook3Ptr, hHook4Ptr
    #global RAMCodeEnLength, # CombineEnLength #, RAMCodeAlignLength

    List_OTPHeader = []
    List_FlashCode = []
    List_RAMCode = []
    List_DataCode = []
    List_HookSegCode = []
    List_DataSegCode = []
    List_BootBlock = []

    os.chdir(Util.Path_Input)
    FWLength = len(Fn.getFileBin(Dict_File['FirmwareImage']))

    # print('FWLength', '{:x}'.format(FWLength))

    # get value from input file
    hUserFWEntryPoint = Fn.getFilePoint(Dict_File['FirmwareImage'], 540, 4)
    hUserFWRamCodeFlashStart = Fn.getFilePoint(
        Dict_File['FirmwareImage'], 544, 4)
    hUserFWRamCodeFlashEnd = Fn.getFilePoint(
        Dict_File['FirmwareImage'], 548, 4)
    hUserFWRamCodeRamStart = Fn.getFilePoint(
        Dict_File['FirmwareImage'], 552, 4)

    # reserved 1197 - 1199
    hReservedField4_InFile = Fn.getFilePoint(
        Dict_File['FirmwareImage'], 1197, 3)

    # Get hook ptr from header
    # hHook1Ptr = Fn.getFilePoint(Dict_File['FirmwareImage'], 1200, 4)
    # hHook2Ptr = Fn.getFilePoint(Dict_File['FirmwareImage'], 1204, 4)
    # hHook3Ptr = Fn.getFilePoint(Dict_File['FirmwareImage'], 1208, 4)
    # hHook4Ptr = Fn.getFilePoint(Dict_File['FirmwareImage'], 1212, 4)

    # Get bootblock info from header
    BBAlign = 0
    BBLength = 0
    # if Dict_File['BBImage'] is not None:
    #     BBAlign = Fn.GetAlign(FWLength, Util.BB_Alignment)
    #     List_BootBlock.clear()
    #     List_BootBlock.append(Fn.getFileBin(Dict_File['BBImage']))
    #     BBLength = len(List_BootBlock[0])
    #     # Pad to 256 alinged for AES
    #     List_BootBlock.append(
    #         bytearray([0]*(Fn.GetAlign(BBLength, Util.BBSize_Alignment))))
    #     Util.sBBName = Fn.GenBinFilefromList(List_BootBlock, 'RAMCode_BB')
    #     BBLength += Fn.GetAlign(BBLength, Util.BBSize_Alignment)
    # elif Dict_File['BBImage'] is None:
    #     BBAlign = 0
    #     BBLength = 0
    #     BBLength_InFile = Fn.Bytes2Int(Fn.getFilePoint(
    #         Dict_File['FirmwareImage'], 566, 2), 'little')
    #     BBOffset_InFile = Fn.Bytes2Int(Fn.getFilePoint(
    #         Dict_File['FirmwareImage'], 568, 4), 'little')
    #     BBImage_InFile = Fn.getFilePoint(
    #         Dict_File['FirmwareImage'], BBOffset_InFile, BBLength_InFile)
    #     if BBLength_InFile != 0:
    #         Fn.GenBinFilefromBytes(BBImage_InFile, 'BBImage')
    #         Util.sBBName = Fn.GenBinFilefromBytes(BBImage_InFile, 'RAMCode_BB')
    # if Dict_FW_H['hBBWorkRAM'] is None:
    #     if Dict_File['BBImage'] is None:
    #         BBWorkRAM_InFile = Fn.getFilePoint(Dict_File['FirmwareImage'], 636, 4)
    #         BBWorkRAM_InFile = Fn.Bytes2Int(BBWorkRAM_InFile, 'little')
    #     else :
    #         # get bbworkram from bb offset 32
    #         BBWorkRAM_InFile = Fn.getFilePoint(Dict_File['BBImage'], 32, 4)
    #         BBWorkRAM_InFile = Fn.Bytes2Int(BBWorkRAM_InFile, 'little')

    # print('BBLength_InFile', '{:x}'.format(BBLength_InFile))
    # print('BBOffset_InFile', '{:x}'.format(BBOffset_InFile))
    # print('BBWorkRAM_InFile', '{:x}'.format(BBWorkRAM_InFile))

    # ActiveFwOffset
    mcp_size = 0 #int(Dict_OTP['oMCPFlashSize'], 10)
    if(mcp_size == 0):
        # auto mode, default 8K Bytes (2M/256)
        ActiveFwOffset = int(Dict_FW_H['hActiveECFwOffset'], 0) * 8192
    else:
        ActiveFwOffset = int(
            Dict_FW_H['hActiveECFwOffset'], 0) * (2048 << mcp_size)

    # RAMCodeStart
    # if Dict_FW_H['hUserFWRamCodeFlashStart'] is not None:
    #     # ref to xml
    #     RAMCodeStart = int(
    #         Dict_FW_H['hUserFWRamCodeFlashStart'], 0) - ActiveEcFwOffset
    # else:
    # ref to f/w header
    RAMCodeStart = Fn.Bytes2Int(
        hUserFWRamCodeFlashStart[0:4], 'little') - ActiveFwOffset

    # RAMCodeEnd
    # if Dict_FW_H['hUserFWRamCodeFlashEnd'] is not None:
    #     # ref to xml
    #     RAMCodeEnd = int(Dict_FW_H['hUserFWRamCodeFlashEnd'], 0) - ActiveEcFwOffset
    # else:
    # ref to f/w header
    RAMCodeEnd = Fn.Bytes2Int(
        hUserFWRamCodeFlashEnd[0:4], 'little') - ActiveFwOffset

    if(RAMCodeEnd & 0xF0000000):
        # f/w is executed in FIU
        RAMCodeStart &= 0x00FFFFFF
        RAMCodeEnd &= 0x00FFFFFF
    else:
        # f/w is executed in MCP
        RAMCodeStart -= 0x080000
        RAMCodeEnd -= 0x080000

    # ===================================================================== #
    #  Seg 1 (Header) & Seg 2 (Hook)
    #    _________________________________________________________________
    #   |  Sign  | Seg1 (Header) |  Hash Table | Seg2 (Hook) | Flash code |
    #   ^        ^               ^             ^             ^
    #  [00h]    [210h]          [500h]        [600h]        [hSeg3Offset]
    # ===================================================================== #
    hSeg2Offset = Fn.Bytes2Int(Fn.getFilePoint(
        Dict_File['FirmwareImage'], 1224, 4), 'little')
    hSeg2Size = Fn.Bytes2Int(Fn.getFilePoint(
        Dict_File['FirmwareImage'], 1228, 4), 'little') - hSeg2Offset
    List_HookSegCode.append(Fn.getFilePoint(
        Dict_File['FirmwareImage'], hSeg2Offset, hSeg2Size))
    Util.sHookSegName = Fn.GenBinFilefromList(
        List_HookSegCode, 'HookSegFile')

    # print('Hook/hSeg2Offset', '{:x}'.format(hSeg2Offset))
    # print('Hook/hSeg2Size', '{:x}'.format(hSeg2Size))

    # ===================================================================== #
    #  Seg 3 (Main firmware)
    #    _________________________________________________________________
    #   |   Flash code   |    RAM code    |   Data code   |   BB   | Seg4 |
    #   ^                ^                ^               ^
    #  [hSeg3Offset]    [RAMCodeStart]   [RAMCodeEnd]    [hSeg4Offset]
    # ===================================================================== #
    hSeg3Offset = Fn.Bytes2Int(Fn.getFilePoint(
        Dict_File['FirmwareImage'], 1232, 4), 'little')

    # print('hSeg3Offset', '{:x}'.format(hSeg3Offset))
    # print('RAMCodeStart', '{:x}'.format(RAMCodeStart))

    # Flash code file (FirmwareImage start - ram code start)
    List_FlashCode.append(Fn.getFilePoint(
        Dict_File['FirmwareImage'], hSeg3Offset, RAMCodeStart - hSeg3Offset))
    Util.sFlashCodeName = Fn.GenBinFilefromList(
        List_FlashCode, 'FlashCodeFile')
    FlashCodeLength = len(Fn.getFileBin(Util.sFlashCodeName))
    hSeg3Size = FlashCodeLength

    # Data code file (ram code end - FirmwareImage end)
    List_DataCode.append(Fn.getFileBin(Dict_File['FirmwareImage'], RAMCodeEnd))
    Util.sDataCodeName = Fn.GenBinFilefromList(List_DataCode, 'DataCodeFile')
    DataCodeLength = len(Fn.getFileBin(Util.sDataCodeName))
    hSeg3Size += DataCodeLength

    # RAM code file (ram code start - ram code end)
    #List_RAMCode.append(Fn.getFilePoint(
    #    Dict_File['FirmwareImage'], RAMCodeStart, FWLength - RAMCodeStart - DataCodeLength))
    List_RAMCode.append(Fn.getFilePoint(
        Dict_File['FirmwareImage'], RAMCodeStart, RAMCodeEnd - RAMCodeStart))
    Util.sRAMCodeName = Fn.GenBinFilefromList(List_RAMCode, 'RAMCodeFile')
    RAMCodeLength = len(Fn.getFileBin(Util.sRAMCodeName))
    Util.sRAMCodeAlignName = Util.sRAMCodeName
    # RAMCodeAlignLength = len(Fn.getFileBin(Util.sRAMCodeAlignName))
    hSeg3Size += (RAMCodeLength + BBLength + BBAlign)

    if(BBLength != 0):
        FWLength += BBLength + BBAlign

    # print('FlashCodeLength', '{:x}'.format(FlashCodeLength))
    # print('RAMCodeLength', '{:x}'.format(RAMCodeLength))
    # print('DataCodeLength', '{:x}'.format(DataCodeLength))

    # print('BBAlign', '{:x}'.format(BBAlign))
    # print('BBLength', '{:x}'.format(BBLength))

    # print('hSeg3Offset', '{:x}'.format(hSeg3Offset))
    # print('hSeg3Size', '{:x}'.format(hSeg3Size))

    # ===================================================================== #
    #  Seg 4 (Data)
    #    _______________________________
    #   |   Data code   |   OTP table   |
    #   ^                ^
    #  [hSeg4Offset]    [OTP Offset]
    # ===================================================================== #

    if(Util.FanTableCnt > 0):
        Seg4Align = Fn.GetAlign(hSeg3Offset + hSeg3Size, 256)
        hSeg4Offset = hSeg3Offset + hSeg3Size + Seg4Align
        List_DataSegCode.append(Util.FanTables_Content)
        Util.sDataSegName = Fn.GenBinFilefromList(
            List_DataSegCode, 'DataSegFile')
        hSeg4Size = len(Fn.getFileBin(Util.sDataSegName))
    else:
        hSeg4Offset = 0
        hSeg4Size = 0
        Seg4Align = 0

    FWLength += (hSeg4Size + Seg4Align)
    # print('hSeg4Offset', '{:x}'.format(hSeg4Offset))
    # print('hSeg4OSize', '{:x}'.format(hSeg4Size))
    # print('Seg4Align', '{:x}'.format(Seg4Align))

    if((Util.FW_Image_header_len + Util.FW_Image_hash_table_len + hSeg2Size + hSeg3Size + hSeg4Size + Seg4Align) != FWLength):
        logging.info('Check Size Failed')
        logging.info('FWLength %s' % FWLength)
        logging.info('hSeg2Size %s' % hSeg2Size)
        logging.info('hSeg3Size %s' % hSeg3Size)
        logging.info('hSeg4Size %s' % hSeg4Size)
        raise Util.LengthError('Check Size failed')

    # if(AESencrypt):
    #     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
    #         if(Dict_OpenSSL['BuildersPrivKey'] is not None):
    #             Util.sAESPubKeyName = OpenSSL_ExportPubKey(
    #                 Dict_OpenSSL['BuildersPrivKey'], Util.EC256, 'ECDH256_AES_pub.pem')

    #             # Gen AES key use BuildersPrivKey + SessPubKey
    #             AESName = OpenSSL_GenAESKey(
    #                 Dict_OpenSSL['BuildersPrivKey'], Dict_OpenSSL['SessPubKey'], 0)  # Shared Secret
    #         else:
    #             # Gen EC P-256 key pair
    #             Util.sAESPubKeyName = OpenSSL_GenKey(Util.EC256)

    #             # Gen AES key
    #             AESName = OpenSSL_GenAESKey(
    #                 'ECDH256_AES_pri.pem', Dict_OpenSSL['SessPubKey'], 1)  # Shared Secret

    #     elif(Util.CryptoSelect == 1):
    #         # Gen EC P-256 key pair and export public key
    #         # Gen AES key
    #         AESName, Util.sAESPubKeyName = CNG_Derive(
    #             Dict_CNG['AESCert_Subject'])  # Shared Secret

    #     MyIV = CryptoDigest(Util.Path_Key, AESName, 256)
    #     MyKey = CryptoDigest(
    #         Util.Path_Input, Fn.GenBinFilefromBytes(MyIV, '_MyIV'), 256)
    #     Fn.GenBinFilefromBytes(MyKey, '_MyKey')

    # OtpAlign = Fn.GetAlign(FWLength, Util.OTP_Alignment)

    # print('FWLength', '{:x}'.format(FWLength))
    # print('OtpAlign', '{:x}'.format(OtpAlign))

    # if(GenOTP):
    #     List_OTPHeader = GenOTPHeader()
    List_FWHeader = GenFWHeader()
    List_Header = [List_FWHeader, List_OTPHeader]
    logging.info('GenHeader OK')
    return List_Header


def GenPacket(List_Header):
    try:
        global Seg4Align
        # global hHashTotal

        print("Generate Package...")
        os.chdir(Util.Path_Input)
        hHashTotal = Fn.getFileBin('HashTotal')

        # seg 2
        if hSeg2Size > 0:
            Hook_Seg_Data = Fn.getFileBin(Util.sHookSegName)

        # seg 3
        Flash_Code_Data = Fn.getFileBin(Util.sFlashCodeName)
        if(AESencrypt):
            RAM_Code_Data = Fn.getFileBin(Util.sRAMCodeName_Encrypt)
        else:
            RAM_Code_Data = Fn.getFileBin(Util.sRAMCodeName)
        Data_Code_Data = Fn.getFileBin(Util.sDataCodeName)

        if(BBLength != 0):
            if(AESencrypt):
                BB_Data = Fn.getFileBin(Util.sBBName_Encrypt)
            else:
                BB_Data = Fn.getFileBin(Util.sBBName)

        # seg 4
        if hSeg4Size > 0:
            Data_Seg_Data = Fn.getFileBin(Util.sDataSegName)

        # write into file
        with open(Dict_File['OutputFWName'], 'wb') as fout:
            for i in range(0, len(List_Header[0])):
                # write F/W Header
                fout.write(List_Header[0][i])

            # write HashTotal
            fout.write(hHashTotal)

            # seg2 (hook)
            if hSeg2Size > 0:
                fout.write(Hook_Seg_Data)   # write Flash Code

            # seg3 (main firmware)
            fout.write(Flash_Code_Data)     # write Flash Code
            fout.write(RAM_Code_Data)       # write RAM Code
            fout.write(Data_Code_Data)      # write Data Code

            if(BBLength != 0):
                fout.write(bytearray([0]*BBAlign))  # pad for align
                fout.write(BB_Data)                 # write BB Data

            # seg4 (fan table)
            if hSeg4Size > 0:
                if(Seg4Align > 0):
                    fout.write(bytearray([0]*Seg4Align))    # write Flash Code
                fout.write(Data_Seg_Data)                   # write Flash Code

            if(GenOTP):
                fout.write(bytearray([0]*OtpAlign))     # pad for align
                for i in range(0, len(List_Header[1])):
                    fout.write(List_Header[1][i])       # write OTP Header

                Util.OTP_Image_data = Fn.getFileBin('OutputOTPImage')
                fout.write(Util.OTP_Image_data)         # write OTP

        move(Dict_File['OutputFWName'], Util.Path_Output)
        logging.info('GenPacket OK')

    except BaseException as e:
        Fn.OutputString(0, 'GenPacket failed')
        logging.error('Error in %s : %s' % (GenPacket.__name__, e))
        return


def DumpOTP(File):
    ColNum0 = 0
    OTPcnt = 0

    # Get OTP table
    try:
        print("Dump OTP Table...")
        tmp = [File]
        Fn.CheckFile(tmp)
        _ = Fn.getFileBin(File)
        _index = 0
        while _index < len(_):
            _index = Fn.SearchIndex(_, Util.OTPMagicString, _index+1, -1)
            if (_index & 0xFF) == 0:
                break
        OtpAlign = _index + Util.OTP_Image_header_len
        OTPTable = Fn.getFilePoint(File, OtpAlign, Util.OTP_Image)

        if len(OTPTable) != Util.OTP_Image:
            raise Util.LengthError('OTP table length not match.')

        print(Util.CRed + 'OTP Table : ' + Util.ENDC + ' ')

        # First row
        print('  ' + ' ', end=" ")
        for row in range(0, 16):
            print(Util.CYellow + f'{row:02x}' + Util.ENDC, end=" ")
        print('')
        # Other rows
        while(True):
            # First column
            print(Util.CYellow + f'{ColNum0:02x}' + Util.ENDC + ' ', end=" ")
            # Other columns
            for i in range(0, 16):
                print(f'{OTPTable[OTPcnt]:02x}'.upper(), end=" ")
                OTPcnt += 1
            print('')

            ColNum0 += 1
            if OTPcnt == Util.OTP_Image:
                break

    except ValueError:
        Fn.OutputString(0, 'There is no OTP table could be dumped.')
    except BaseException as e:
        Fn.OutputString(1, 'Dump OTP failed')
        logging.error('Error in %s : %s' % (GenFWHeader_Cus.__name__, e))
        Fn.ClearTmpFiles()


# def GenFWHeader_Cus():
#     os.chdir(Util.Path_Key)
#     if(bPriKey and (Util.CryptoSelect == 0 or Util.CryptoSelect == 2)):
#         FileList = [Dict_OpenSSL['EcFwSigKey']]
#         Fn.CheckFile(FileList)
#     os.chdir(Util.Path_Input)
#     Fn.CheckOutputDir()
#     try:
#         print("Generate FW Header...")
#         _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])

#         # parser header of 'BaseEcFwImg'
#         for key, value in Dict_Config.items():
#             if key in Util.DictColDet:
#                 if(not Dict_Config[key].isalnum()):  # neither number nor alphabet
#                     continue
#                 elif(key == 'OEMver'):
#                     continue
#                 byte_offset = Util.DictColDet[key][0]
#                 mask = Util.DictColDet[key][1]
#                 length = Util.DictColDet[key][2]
#                 shift = Util.DictColDet[key][3]

#                 if(length < 8):
#                     # for bit level field
#                     old_val = Fn.getBinPoint(_, byte_offset, 1)
#                     tmp = old_val[0] & mask
#                     new_val = tmp | (int(Dict_Config[key], 0) << shift)
#                     Fn.setBinPoint(new_val.to_bytes(
#                         1, byteorder='big'), byte_offset, Dict_Config['BaseEcFwImg'])
#                 else:
#                     new_val = Fn.Int2Bytes(
#                         int(Dict_Config[key], 0), (int)(length / 8), 'little')
#                     Fn.setBinPoint(new_val, byte_offset,
#                                    Dict_Config['BaseEcFwImg'])

#                 _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])

#         # ---- update new value from _Model.xml ----
#         # hRevokeKey, hRevokeKeyInv
#         if(Dict_Config['EcFwKeyRevocation'] == '1'):
#             new_val = Fn.RevokeKey('0x58')
#         elif(Dict_Config['EcFwKeyRevocation'] == '2'):
#             new_val = Fn.RevokeKey('0x59')
#         else:
#             new_val = Fn.RevokeKey(None)

#         byte_offset = Util.DictColDet['hRevokeKey'][0]
#         Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # hSigPubKey
#         if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
#             if((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0):
#                 new_val = OpenSSL_GetPubKey(Dict_OpenSSL['EcFwPubKey0'], totallen=512) if (
#                     bPubKey) else Fn.ReservedData(512 * 8)
#             elif((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 1):
#                 new_val = OpenSSL_GetPubKey(Dict_OpenSSL['EcFwPubKey1'], totallen=512) if (
#                     bPubKey) else Fn.ReservedData(512 * 8)
#         elif(Util.CryptoSelect == 1):
#             if((int)(Dict_CNG['SignCert_Idx']) == 0):
#                 new_val = CNG_ExportPubKey(Dict_CNG['EcFwCert0_Subject'], totallen=512) if (
#                     bPubKey) else Fn.ReservedData(512 * 8)
#             elif((int)(Dict_CNG['SignCert_Idx']) == 1):
#                 new_val = CNG_ExportPubKey(Dict_CNG['EcFwCert1_Subject'], totallen=512) if (
#                     bPubKey) else Fn.ReservedData(512 * 8)
#         byte_offset = Util.DictColDet['hSigPubKey'][0]
#         Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # hOEMversion
#         # new_val = Fn.Text2Bin(Dict_Config['OEMver'], 8)
#         # byte_offset = Util.DictColDet['OEMver'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # hImageLen
#         # if(AESencrypt):
#         #     byte_offset = Util.DictColDet['hImageLen'][0]
#         #     old_val = Fn.Bytes2Int(Fn.getBinPoint(_, byte_offset, 4), 'big')
#         #     diff = (RAMCodeEnLength - RAMCodeLength) + (BBEnLength - BBLength)
#         #     new_val = Fn.Int2Bytes(old_val + diff, 4, 'big')
#         #     Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # hFwSeg1Hash
#         # get 'hShaAlgoUsed'
#         # HashTypeforECKey = Fn.Bytes2Int(Fn.getBinPoint(_, 560, 1), 'little')
#         # if HashTypeforECKey & 0x40:
#         #     HashTypeforECKey = 512
#         # else:
#         #     HashTypeforECKey = 256

#         # print('HashTypeforECKey', '{:d}'.format(HashTypeforECKey))

#         hSeg1_OEM.clear()
#         hSeg1_OEM.append(bytearray(Fn.getBinPoint(Fn.getFileBin(Dict_Config['BaseEcFwImg']), 528, (1280-528))))

#         # new_val = Fn.getFileBin(Fn.GenBinFilefromList(hSeg1Hash, 'seg_1_OEM'))
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # update OEM HashTotal
#         hHashTotal.clear()
#         hHashTotal.append(CryptoDigest(Util.Path_Input, Fn.GenBinFilefromList(
#             hSeg1_OEM, 'seg_1_OEM'), HashTypeforECKey))

#         if HashTypeforECKey == 256:
#             # pad 32byte for sha256
#             hHashTotal.append(bytearray([0]*32))

#         hHashTotal.append(bytearray(Fn.getBinPoint(_, Util.DictColDet['hFwSeg2Hash'][0], (256-64))));
#         SignFieldName = Fn.GenBinFilefromList(hHashTotal, 'SignField_OEM')

#         new_val = Fn.getFileBin(SignFieldName)
#         Fn.setBinPoint(new_val, Util.DictColDet['hFwSeg1Hash'][0], Dict_Config['BaseEcFwImg'])

#         # _range = Util.FW_Image_header_len - \
#         #     (int)(Util.FW_Header_nonsig) - Util.hImageHash - \
#         #     (int)(Util.hReservedField5 / 8)
#         # data_FWHeader = Fn.getFilePoint(
#         #     Dict_Config['BaseEcFwImg'], (int)(Util.FW_Header_nonsig), _range)
#         # _range = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], Util.DictColDet['hImageLen'][0], (int)(
#         #     Util.DictColDet['hImageLen'][2] / 8))
#         # image_length = Fn.Bytes2Int(_range, 'big')
#         # data_FWImg = Fn.getFilePoint(
#         #     Dict_Config['BaseEcFwImg'], Util.FW_Image_header_len, image_length)
#         # data = bytearray(data_FWHeader) + bytearray(data_FWImg)
#         # new_val = CryptoDigest(
#         #     Util.Path_Input, Fn.GenBinFilefromBytes(data, 'SignTemp'), 256)
#         # byte_offset = Util.DictColDet['hImageHash'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])



#         # Signature
#         # _range = Util.FW_Image_header_len - \
#         #     (int)(Util.FW_Header_nonsig) + image_length
#         # data = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], (int)(
#         #     Util.FW_Header_nonsig), _range)

#         if(Signature):
#             if(int(Dict_Config["CryptoSelect"]) == 0):
#                 if(bPriKey != 1):
#                     Fn.OutputString(1, 'There is no private key.')
#                     raise Util.SettingError("<EcFwSigKey> setting error")
#                 new_val = OpenSSL_SignFile(HashTypeforECKey, (int)(Util.RSAPKCPAD), SignFieldName, Dict_OpenSSL['EcFwSigKey'])
#             elif(int(Dict_Config["CryptoSelect"]) == 1):
#                 if((int)(Dict_CNG['SignCert_Idx']) == 0):
#                     new_val = CNG_SignFile(Dict_CNG["EcFwCert0_Subject"], (int)(Util.RSAPKCPAD),
#                                            HashTypeforECKey, 0, SignFieldName)
#                 elif((int)(Dict_CNG['SignCert_Idx']) == 1):
#                     new_val = CNG_SignFile(Dict_CNG["EcFwCert1_Subject"], (int)(Util.RSAPKCPAD),
#                                            HashTypeforECKey, 0, SignFieldName)
#         else:
#             new_val = Fn.ReservedData(Util.Signature_len * 8)
#             # Digest function is disable now
#             # new_val = CryptoDigest(Util.Path_Input, Fn.GenBinFilefromBytes(data, 'SignField'), HashTypeforECKey)
#             # new_val += bytearray(Fn.ReservedData((512 - (HashTypeforECKey/8)) * 8))

#         Fn.setBinPoint(new_val, Util.DictColDet['hSignature'][0], Dict_Config['BaseEcFwImg'])

#     except BaseException as e:
#         Fn.OutputString(1, 'Gen FW Header failed')
#         Fn.OutputString(1, 'Error in %s : %s' % (GenFWHeader_Cus.__name__, e))
#         logging.error('Error in %s : %s' % (GenFWHeader_Cus.__name__, e))
#         Fn.ClearTmpFiles()


# def GenOTPHeader_Cus():
#     try:
#         _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])
#         _index = 0
#         while _index < len(_):
#             _index = Fn.SearchIndex(_, Util.OTPMagicString, _index+1, -1)
#             if (_index & 0xFF) == 0:
#                 break
#         OtpAlign = _index + Util.OTP_Image_header_len
#         OtpImgChksumOffset = _index + Util.DictColDet['hOtpImgChksum'][0]
#         print("Generate OTP Header...")

#         _ = Fn.getFilePoint(
#             Dict_Config['BaseEcFwImg'], OtpAlign, Util.OTP_Image)
#         MyHash = CryptoDigest(
#             Util.Path_Input, Fn.GenBinFilefromBytes(_, 'OutputOTPImage'), 256)
#         Fn.setBinPoint(MyHash, OtpImgChksumOffset, Dict_Config['BaseEcFwImg'])

#     except ValueError:
#         logging.error('Warning in %s : %s' %
#                       (GenOTPHeader_Cus.__name__, 'OTP header not exist.'))
#         Fn.OutputString(1, 'Gen OTP Header failed')

#     except BaseException as e:
#         Fn.OutputString(1, 'Gen OTP Header failed')
#         Fn.OutputString(1, 'Error in %s : %s' % (GenOTPHeader_Cus.__name__, e))
#         logging.error('Error in %s : %s' % (GenOTPHeader_Cus.__name__, e))
#         Fn.ClearTmpFiles()


# def GenOTPImage_Cus():

#     os.chdir(Util.Path_Key)
#     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
#         FileList = [Dict_OpenSSL['EcFwPubKey0']]
#         if(bPubHash1):
#             Fn.CheckFile(FileList)

#         FileList = [Dict_OpenSSL['EcFwPubKey1']]
#         if(bPubHash2):
#             Fn.CheckFile(FileList)

#         if(bFWPubHash1):
#             FileList = [Dict_OpenSSL['SySFwPubKey0']]
#             Fn.CheckFile(FileList)

#         if(bFWPubHash2):
#             FileList = [Dict_OpenSSL['SySFwPubKey1']]
#             Fn.CheckFile(FileList)

#         # if(bSessPrivKey):
#         #     FileList = [Dict_OpenSSL['SessPrivKey']]
#         #     Fn.CheckFile(FileList)

#     try:
#         os.chdir(Util.Path_Input)
#         _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])
#         _index = 0
#         while _index < len(_):
#             _index = Fn.SearchIndex(_, Util.OTPMagicString, _index+1, -1)
#             if (_index & 0xFF) == 0:
#                 break

#         OtpAlign = _index + Util.OTP_Image_header_len
#         print("Generate OTP Image...")

#         # parse all OTP column
#         for key, value in Dict_OTP.items():
#             if key in Util.DictColDet:
#                 if(not Dict_OTP[key].isalnum()):  # neither number nor alphabet
#                     continue
#                 byte_offset = OtpAlign + Util.DictColDet[key][0]
#                 mask = Util.DictColDet[key][1]
#                 length = Util.DictColDet[key][2]
#                 shift = Util.DictColDet[key][3]

#                 if(length < 8):
#                     old_val = Fn.getBinPoint(_, byte_offset, 1)
#                     tmp = old_val[0] & mask
#                     new_val = tmp | (int(Dict_OTP[key], 0) << shift)
#                     Fn.setBinPoint(new_val.to_bytes(
#                         1, byteorder='big'), byte_offset, Dict_Config['BaseEcFwImg'])
#                 else:
#                     new_val = int(Dict_OTP[key], 0).to_bytes(
#                         (int)(length / 8), byteorder='big')
#                     Fn.setBinPoint(new_val, byte_offset,
#                                    Dict_Config['BaseEcFwImg'])

#                 _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])

#         # Get HashTypeforECKey
#         # HashTypeforECKey = Fn.Bytes2Int(Fn.getBinPoint(_, 560, 1), 'little')
#         # if HashTypeforECKey & 0x40:
#         #     HashTypeforECKey = 512
#         # else:
#         #     HashTypeforECKey = 256

#         # EcFwPubKey0
#         if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
#             new_val = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
#                 Dict_OpenSSL['EcFwPubKey0'], reFile=1), HashTypeforECKey) if (bPubHash1) else Fn.ReservedData(64 * 8)
#         elif(Util.CryptoSelect == 1):
#             new_val = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
#                 Dict_CNG['EcFwCert0_Subject'], reFile=1), HashTypeforECKey) if (bPubHash1) else Fn.ReservedData(64 * 8)
#         if (HashTypeforECKey == 256):
#             new_val += bytearray(Fn.ReservedData(32 * 8))

#         byte_offset = OtpAlign + Util.DictColDet['EcFwPubKey0'][0]
#         Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # EcFwPubKey1
#         if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
#             new_val = CryptoDigest(Util.Path_Key, OpenSSL_GetPubKey(
#                 Dict_OpenSSL['EcFwPubKey1'], reFile=1), HashTypeforECKey) if (bPubHash2) else Fn.ReservedData(64 * 8)
#         elif(Util.CryptoSelect == 1):
#             new_val = CryptoDigest(Util.Path_Key, CNG_ExportPubKey(
#                 Dict_CNG['EcFwCert1_Subject'], reFile=1), HashTypeforECKey) if (bPubHash2) else Fn.ReservedData(64 * 8)
#         if (HashTypeforECKey == 256):
#             new_val += bytearray(Fn.ReservedData(32 * 8))
#         byte_offset = OtpAlign + Util.DictColDet['EcFwPubKey1'][0]
#         Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])

#         # oOtpRegion0Digest
#         # data = _[OtpAlign: OtpAlign + 160]
#         # new_val = CryptoDigest(Util.Path_Input, Fn.GenBinFilefromBytes(
#         #     data, 'oOtpRegion0Digest'), 256)
#         # byte_offset = OtpAlign + Util.DictColDet['oOtpRegion0Digest'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # oAESKey
#         # if (('oAESKey' in Dict_OTP) and (Dict_OTP['oAESKey'] is not None)):
#         #     new_val = OpenSSL_GetAESKey(Dict_OTP['oAESKey'])
#         #     byte_offset = OtpAlign + Util.DictColDet['oAESKey'][0]
#         #     Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # SySFwPubKey0
#         # new_val = Fn.ReservedData(64 * 8)
#         # byte_offset = OtpAlign + Util.DictColDet['SySFwPubKey0'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # SySFwPubKey1
#         # new_val = Fn.ReservedData(64 * 8)
#         # byte_offset = OtpAlign + Util.DictColDet['SySFwPubKey1'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#         # SessPrivKey
#         # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
#         #     new_val = OpenSSL_GetPrivKey(Dict_OpenSSL['SessPrivKey']) if (
#         #         bSessPrivKey) else Fn.ReservedData(32 * 8)
#         # elif(Util.CryptoSelect == 1):
#         #     new_val = CNG_ExportPrivKey(Dict_CNG['AESCert_Subject']) if (
#         #         bSessPrivKey) else Fn.ReservedData(32 * 8)
#         # byte_offset = OtpAlign + Util.DictColDet['oSessPrivKey'][0]
#         # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

#     except ValueError:
#         logging.error('Warning in %s : %s' %
#                       (GenOTPImage_Cus.__name__, 'OTP field not exist.'))
#         Fn.OutputString(1, 'Gen OTP Image failed')
#         # Fn.OutputString(0, 'Error in %s : %s' % (GenOTPImage_Cus.__name__, 'OTP image not found.'))
#         # Fn.ClearTmpFiles()

#     except BaseException as e:
#         Fn.OutputString(1, 'Error in %s : %s' % (GenOTPImage_Cus.__name__, e))
#         logging.error('Error in %s : %s' % (GenOTPImage_Cus.__name__, e))
#         Fn.ClearTmpFiles()


# def GenHeader_Cus():
#     global BBLength, BBEnLength, RAMCodeLength, RAMCodeEnLength, CombineEnLength #, RAMCodeAlignLength

    # FanTable
    # if(Util.FanTableCnt != 0):
    #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], 538, 1)
    #     FanTableCntLimit = Fn.Bytes2Int(_, 'big')
    #     # Check Size
    #     if(Util.FanTableCnt > FanTableCntLimit):
    #         Fn.OutputString(1, '%s Fan Tables at most.' % FanTableCntLimit)
    #         Fn.ClearTmpFiles()
    #     # Gen Empty Fan Table
    #     else:
    #         for i in range(0, FanTableCntLimit - Util.FanTableCnt):
    #             for j in range(0, Util.FanTableSize):
    #                 Util.FanTables_Content.append(0)

    #     Fn.OutputString(1, 'Add to gen fantable')

    # new_val = Util.FanTables_Content
    # byte_offset = Util.FW_Image_header_len
    # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

    # if(AESencrypt):
    #     # Gen IV, Key
    #     # ECDH: For app SessPub + BuilderPri, for f/w SessPri + BuilderPub
    #     # IV: Digest of ECHD value
    #     # key: Digest of IV

    #     # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
    #     #     # gen BuildersPriKey
    #     #     Util.sAESPubKeyName = OpenSSL_GenKey(Util.EC256)
    #     #     # gen ECHD share secret
    #     #     AESName = OpenSSL_GenAESKey(
    #     #         'ECDH256_AES_pri.pem', Dict_OpenSSL['SessPubKey'], 1)
    #     # elif(Util.CryptoSelect == 1):
    #     #     AESName, Util.sAESPubKeyName = CNG_Derive(
    #     #         Dict_CNG['AESCert_Subject'])
    #     # MyIV = CryptoDigest(Util.Path_Key, AESName, 256)
    #     # MyKey = CryptoDigest(
    #     #     Util.Path_Input, Fn.GenBinFilefromBytes(MyIV, '_MyIV'), 256)
    #     # Fn.GenBinFilefromBytes(MyKey, '_MyKey')

    #     # Check BB
    #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], Util.DictColDet['hBBSize'][0], (int)(
    #         Util.DictColDet['hBBSize'][2] / 8))
    #     BBLength = Fn.Bytes2Int(_, 'little')
    #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], Util.DictColDet['hBBOffset'][0], (int)(
    #         Util.DictColDet['hBBOffset'][2] / 8))
    #     BBOffset = Fn.Bytes2Int(_, 'little')

    #     # Gen RAMCodeFile
    #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], Util.DictColDet['hUserFWRamCodeFlashStart'][0], (int)(
    #         Util.DictColDet['hUserFWRamCodeFlashStart'][2] / 8))
    #     _ = _[::-1]
    #     RAMCodeStart = Fn.Bytes2Int(_[1:4], 'little')
    #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], Util.DictColDet['hUserFWRamCodeFlashEnd'][0], (int)(
    #         Util.DictColDet['hUserFWRamCodeFlashEnd'][2] / 8))
    #     _ = _[::-1]
    #     RAMCodeEnd = Fn.Bytes2Int(_[1:4], 'little')
    #     _ = Fn.getFilePoint(
    #         Dict_Config['BaseEcFwImg'], RAMCodeStart, RAMCodeEnd - RAMCodeStart)
    #     Util.sRAMCodeName = Fn.GenBinFilefromBytes(_, 'RAMCodeFile')
    #     RAMCodeLength = len(Fn.getFileBin(Util.sRAMCodeName))
    #     Util.sRAMCodeAlignName = Fn.GenFileAlign(
    #         Util.sRAMCodeName, 16, 'RAMCodeFile_Align')
    #     # RAMCodeAlignLength = len(Fn.getFileBin(Util.sRAMCodeAlignName))

        # # Gen Encrypted File
        # if(BBLength == 0):
        #     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #         Util.sRAMCodeName_Encrypt = OpenSSL_AES_Encrypt(
        #             1, Util.sRAMCodeAlignName, '_MyIV', '_MyKey', hAESAdd, 'RAMCodeFile_Encrypt', len(Fn.getFileBin(Util.sRAMCodeAlignName)))
        #     elif(Util.CryptoSelect == 1):
        #         Util.sRAMCodeName_Encrypt = CNG_SymEncrypt(
        #             Util.sRAMCodeAlignName, '_MyIV', '_MyKey', 'RAMCodeFile_Encrypt')
        #     RAMCodeEnLength = len(Fn.getFileBin(Util.sRAMCodeName_Encrypt))
        # elif(BBLength != 0):
        #     _ = Fn.getFilePoint(Dict_Config['BaseEcFwImg'], BBOffset, BBLength)
        #     sBBName = Fn.GenBinFilefromBytes(_, 'BB')
        #     FileList = [Util.sRAMCodeName, sBBName]
        #     Util.sCombineName = Fn.GenFileCombine(FileList, 'RAMCode_BB')
        #     CombineLength = len(Fn.getFileBin(Util.sCombineName))

        #     if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
        #         Util.sCombineName_Encrypt = OpenSSL_AES_Encrypt(
        #             1, Util.sCombineName, '_MyIV', '_MyKey', hAESAdd, 'RAMCode_BB_Encrypt', CombineLength)
        #     elif(Util.CryptoSelect == 1):
        #         Util.sCombineName_Encrypt = CNG_SymEncrypt(
        #             Util.sCombineName, '_MyIV', '_MyKey', 'RAMCode_BB_Encrypt')
        #     CombineEnLength = len(Fn.getFileBin(Util.sCombineName_Encrypt))
        #     _ = Fn.getFilePoint(Util.sCombineName_Encrypt, 0, RAMCodeLength)
        #     Util.sRAMCodeName_Encrypt = Fn.GenBinFilefromBytes(
        #         _, 'RAMCodeFile_Encrypt')
        #     RAMCodeEnLength = len(Fn.getFileBin(Util.sRAMCodeName_Encrypt))
        #     _ = Fn.getFileBin(Util.sCombineName_Encrypt, RAMCodeLength)
        #     Util.sBBName_Encrypt = Fn.GenBinFilefromBytes(_, 'BBImage_Encrypt')
        #     BBEnLength = len(Fn.getFileBin(Util.sBBName_Encrypt))
        #     Fn.DeleteFile('BB')

        # # Replace
        # new_val = Fn.getFileBin(Util.sRAMCodeName_Encrypt)
        # byte_offset = RAMCodeStart
        # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])
        # new_val = Fn.getFileBin(Util.sBBName_Encrypt)
        # byte_offset = BBOffset
        # Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

    # if(GenOTP):
    #     GenOTPHeader_Cus()

    # GenFWHeader_Cus()
    # logging.info('GenHeader OK')


# def GenPacket_Cus():
#     print("Generate Package...")
#     copy(Dict_Config['BaseEcFwImg'], Util.Path_Output)
#     os.rename(os.path.join(Util.Path_Output, Dict_Config['BaseEcFwImg']), os.path.join(
#         Util.Path_Output, Dict_Config['OutputEcFwImg']))
#     print('\nPlease get your image file in Output Folder.\n')


def ParseXml(XmlTree):
    global Dict_Config, Dict_OpenSSL, Dict_CNG, Dict_PKCS11, Dict_File, Dict_FW_H, Dict_OTP_H, Dict_OTP, GenOTP, AESencrypt,\
        bPriKey, bPubKey, bPubHash1, bPubHash2, bFWPubHash1, bFWPubHash2, bSessPrivKey,\
        HashTypeforECKey, HashTypeforSystemKey

    # Config Field
    Node = XmlTree.find('Config')
    Dict_Config = ParseXmlwithTarget(Node, Util.Target.Config)
    # Setting Path
    if ('ToolPath' in Dict_Config):
        if(Dict_Config['ToolPath'] is not None):
            Util.SetPath(Dict_Config['ToolPath'])
        else:
            Util.SetPath(os.path.dirname(os.path.abspath(__file__)))

    # Custom Config Field
    # if not Util.internal:
    #     os.chdir(Util.Path_Input)
    #     copy(Dict_Config['BaseEcFwImg'], os.path.join(
    #         Util.Path_Input, 'tmpInput'))
    #     Dict_Config['BaseEcFwImg'] = 'tmpInput'
    #     # os.chdir(Util.Path_Current)

    #     # ReleaseMode, SecurityMode
    #     Util.ReleaseMode = (int)(Dict_Config['ReleaseMode'])
    #     Util.SecurityMode = (int)(Dict_Config['SecurityMode'])
    #     Util.RSAPKCPAD = (int)(Dict_Config['RSAPKCPAD'])
    #     if(Util.ReleaseMode == 0):
    #         Util.hOTPRefToTable = 1
    #         Util.hNotUpdateOTPRegister = 1
    #         Util.hNotEraseOTPTable = 1
    #         print(Util.CYellow + '''\nRelease Mode : %s, OTP table won't be programmed into OTP HW.''' %
    #               Util.ReleaseMode + Util.ENDC)
    #     elif(Util.ReleaseMode == 1):
    #         Util.hOTPRefToTable = 1
    #         Util.hNotUpdateOTPRegister = 1
    #         Util.hNotEraseOTPTable = 1
    #         print(Util.CYellow + '''\nRelease Mode : %s, OTP table will be programmed into OTP HW.''' %
    #               Util.ReleaseMode + Util.ENDC)
    #     if(Util.SecurityMode == 0):
    #         Util.hSecureBoot = 0
    #         Util.hSecurityLvl = 0
    #         print(Util.CYellow + '''Security Mode : %s, No SecureBoot\n''' %
    #               Util.SecurityMode + Util.ENDC)
    #     elif(Util.SecurityMode == 1):
    #         Util.hSecureBoot = 1
    #         Util.hSecurityLvl = 0
    #         print(Util.CYellow + '''Security Mode : %s, Standard SecureBoot\n''' %
    #               Util.SecurityMode + Util.ENDC)
    #     elif(Util.SecurityMode == 2):
    #         Util.hSecureBoot = 1
    #         Util.hSecurityLvl = 1
    #         print(Util.CYellow + '''Security Mode : %s, NIST SP 800-193 compliant EC FW PFR\n''' %
    #               Util.SecurityMode + Util.ENDC)

    #     # keep hNotDoBackup / hOTPRefToSrcTable
    #     hDevMode_cus = Fn.Bytes2Int(Fn.getFilePoint(Dict_Config['BaseEcFwImg'], 536, 1), 'little')
    #     if(hDevMode_cus & 0x08):
    #         Util.hNotDoBackup = 1
    #     else :
    #         Util.hNotDoBackup = 0

    #     if(hDevMode_cus & 0x04):
    #         Util.hOTPRefToSrcTable = 1
    #     else :
    #         Util.hOTPRefToSrcTable = 0

    #     data = [Util.hNotDoBackup, Util.hOTPRefToSrcTable, Util.hNotEraseOTPTable, Util.hNotUpdateOTPRegister,
    #             Util.hHwTrimRefOTPTable, Util.hOTPRefToTable, Util.hSecurityLvl, Util.hSecureBoot]
    #     Dict_Config['hDevMode'] = '0x' + Fn.ConvertBitArray2Byte(data).hex()

    #     HashTypeforECKey = Fn.Bytes2Int(Fn.getFilePoint(Dict_Config['BaseEcFwImg'], 560, 1), 'little')
    #     if HashTypeforECKey & 0x40:
    #         HashTypeforECKey = 512
    #     else:
    #         HashTypeforECKey = 256

    #     os.chdir(Util.Path_Current)

    # OpenSSL Field
    # Node = XmlTree.find('Crypto')
    # Node = Node.find('OpenSSL')
    # Dict_OpenSSL = ParseXmlwithTarget(Node, Util.Target.OpenSSL)

    # # CNG Field
    # Node = XmlTree.find('Crypto')
    # Node = Node.find('CNG')
    # Dict_CNG = ParseXmlwithTarget(Node, Util.Target.CNG)

    # # PKCS11 Field
    # Node = XmlTree.find('Crypto')
    # Node = Node.find('PKCS11')
    # if(Node is not None):
    #     Dict_PKCS11 = ParseXmlwithTarget(Node, Util.Target.OpenSSL)

    # File Field
    Node = XmlTree.find('FileName')
    if(Node is not None):
        Dict_File = ParseXmlwithTarget(Node, Util.Target.FileName)

    # FW Header Field
    Node = XmlTree.find('FWImageHeader')
    if(Node is not None):
        Dict_FW_H = ParseXmlwithTarget(Node, Util.Target.FWImageHeader)

    # # OTP Header Field
    # Node = XmlTree.find('OTPImageHeader')
    # if(Node is not None):
    #     Dict_OTP_H = ParseXmlwithTarget(Node, Util.Target.OTPImageHeader)

    # Setting Global variable
    if 'AESencrypt' in Dict_Config:
        AESencrypt = 0 #(int)(Dict_Config['AESencrypt'])
    Util.CryptoSelect = 0 #(int)(Dict_Config['CryptoSelect'])

    # if(Util.CryptoSelect == 0 or Util.CryptoSelect == 2):
    #     KeyType = [0, 0]
    #     KeyLonger = [False, False]
    #     Key3072_384 = [False, False]
    #     if Dict_OpenSSL['EcFwSigKey'] is not None:
    #         bPriKey = 1
    #         if Dict_OpenSSL['EcFwPubKey0'] is not None:
    #             bPubHash1 = 1
    #             KeyType[0], KeyLonger[0], Key3072_384[0] = IdentifyPubKey_2(
    #                 Dict_OpenSSL['EcFwPubKey0'])
    #         if Dict_OpenSSL['EcFwPubKey1'] is not None:
    #             bPubHash2 = 1
    #             KeyType[1], KeyLonger[1], Key3072_384[1] = IdentifyPubKey_2(
    #                 Dict_OpenSSL['EcFwPubKey1'])
    #         if(((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0) and bPubHash1) or \
    #                 (((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 1) and bPubHash2):
    #             bPubKey = 1
    #         if(bPriKey and bPubKey):
    #             if((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0):
    #                 CheckKeyPair(
    #                     Dict_OpenSSL['EcFwSigKey'], Dict_OpenSSL['EcFwPubKey0'])
    #             else:
    #                 CheckKeyPair(
    #                     Dict_OpenSSL['EcFwSigKey'], Dict_OpenSSL['EcFwPubKey1'])

    #     if Util.internal:
    #         if Dict_OpenSSL['SessPrivKey'] is not None:
    #             bSessPrivKey = 1

    #     elif not Util.internal:
    #         # oSigPubKeySts
    #         if bPubHash1:
    #             if KeyType[0] == Util.KeyType.RSA:
    #                 Util.oSigPubKeySts[7] = 1   # Bit0
    #             else:
    #                 Util.oSigPubKeySts[5] = 1   # Bit2

    #         if bPubHash2:
    #             if KeyType[1] == Util.KeyType.RSA:
    #                 Util.oSigPubKeySts[6] = 1   # Bit1
    #             else:
    #                 Util.oSigPubKeySts[4] = 1   # Bit3

    #         # if ((int)(Dict_Config['EcFwKeyRevocation'])) == 1:
    #         # 	Util.oSigPubKeySts[3] = 1	    # Bit4
    #         # elif ((int)(Dict_Config['EcFwKeyRevocation'])) == 2:
    #         # 	Util.oSigPubKeySts[2] = 1	    # Bit5

    #         if (((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0) and KeyLonger[0]) or (((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 1) and KeyLonger[1]):
    #             Util.oSigPubKeySts[1] = 1       # Bit6

    #         # if ((int)(Dict_Config['HashType']) == 0):
    #         #     HashTypeforECKey = 256
    #         # elif ((int)(Dict_Config['HashType']) == 1):
    #         #     Util.oSigPubKeySts[0] = 1       # Bit7
    #         #     HashTypeforECKey = 512

    #         # oLongKeySel
    #         if (((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 0) and Key3072_384[0]) or (((int)(Dict_OpenSSL['EcFwSigKey_Idx']) == 1) and Key3072_384[1]):
    #             Util.oLongKeySel = 1

    #         # if (('SySFwPubKey0' in Dict_OpenSSL) and (Dict_OpenSSL['SySFwPubKey0'] is not None)):
    #         #     bFWPubHash1 = 1
    #         # if (('SySFwPubKey1' in Dict_OpenSSL) and (Dict_OpenSSL['SySFwPubKey1'] is not None)):
    #         #     bFWPubHash2 = 1
    #         # if (('SessPrivKey' in Dict_OpenSSL) and (Dict_OpenSSL['SessPrivKey'] is not None)):
    #         #     bSessPrivKey = 1

    # elif(Util.CryptoSelect == 1):
    #     KeyType = [0, 0]
    #     KeyLonger = [False, False]
    #     Key3072_384 = [False, False]

    #     if ((((int)(Dict_CNG['SignCert_Idx']) == 0) and (Dict_CNG['EcFwCert0_Subject'] is not None)) or
    #             (((int)(Dict_CNG['SignCert_Idx']) == 1)) and (Dict_CNG['EcFwCert1_Subject'] is not None)):
    #         bPriKey = 1
    #         bPubKey = 1
    #     if Dict_CNG['EcFwCert0_Subject'] is not None:
    #         bPubHash1 = 1
    #         KeyType[0], KeyLonger[0], Key3072_384[0] = CNG_IdentifyKeyType(
    #             Dict_CNG['EcFwCert0_Subject'])
    #     if Dict_CNG['EcFwCert1_Subject'] is not None:
    #         bPubHash2 = 1
    #         KeyType[1], KeyLonger[1], Key3072_384[1] = CNG_IdentifyKeyType(
    #             Dict_CNG['EcFwCert1_Subject'])

    #     if Util.internal:
    #         if Dict_CNG['AESCert_Subject'] is not None:
    #             bSessPrivKey = 1

    #     elif not Util.internal:
    #         # oSigPubKeySts
    #         if bPubHash1:
    #             if KeyType[0] == Util.KeyType.RSA:
    #                 Util.oSigPubKeySts[7] = 1  # Bit0
    #             else:
    #                 Util.oSigPubKeySts[5] = 1  # Bit2
    #         if bPubHash2:
    #             if KeyType[1] == Util.KeyType.RSA:
    #                 Util.oSigPubKeySts[6] = 1  # Bit1
    #             else:
    #                 Util.oSigPubKeySts[4] = 1  # Bit3

    #         # if ((int)(Dict_Config['EcFwKeyRevocation'])) == 1:
    #         # 	Util.oSigPubKeySts[3] = 1	# Bit4
    #         # elif ((int)(Dict_Config['EcFwKeyRevocation'])) == 2:
    #         # 	Util.oSigPubKeySts[2] = 1	# Bit5

    #         if (((int)(Dict_CNG['SignCert_Idx']) == 0) and KeyLonger[0]) or (((int)(Dict_CNG['SignCert_Idx']) == 1) and KeyLonger[1]):
    #             Util.oSigPubKeySts[1] = 1   # Bit6

    #         if (HashTypeforECKey == 256):
    #             Util.oSigPubKeySts[0] = 0   # Bit7
    #         else:
    #             Util.oSigPubKeySts[0] = 1   # Bit7

    #         # oLongKeySel
    #         if (((int)(Dict_CNG['SignCert_Idx']) == 0) and Key3072_384[0]) or (((int)(Dict_CNG['SignCert_Idx']) == 1) and Key3072_384[1]):
    #             Util.oLongKeySel = 1

    #         # if (('SySFwCert0_Subject' in Dict_CNG) and (Dict_CNG['SySFwCert0_Subject'] is not None)):
    #         #     bFWPubHash1 = 1
    #         # if (('SySFwCert1_Subject' in Dict_CNG) and (Dict_CNG['SySFwCert1_Subject'] is not None)):
    #         #     bFWPubHash2 = 1
    #         # if (('AESCert_Subject' in Dict_CNG) and (Dict_CNG['AESCert_Subject'] is not None)):
    #         #     bSessPrivKey = 1

    # if(bPubKey == 0):
    #     Fn.OutputString(
    #         1, "Need an EcFwPubKey or EcFwCert. Please check Crypto section.")
    #     Fn.ClearTmpFiles()

    # YH
    # if(Util.CryptoSelect == 0):
    #     Dict_Config['hSigPubKeyHashIdx'] = Dict_OpenSSL['EcFwSigKey_Idx']
    # elif(Util.CryptoSelect == 1):
    #     Dict_Config['hSigPubKeyHashIdx'] = Dict_CNG['SignCert_Idx']

    if(Util.internal):
        GenOTP = 0 #(int)(Dict_Config['GenOTP'])
    # elif(not Util.internal):  # Custom XML setting
        # if(Util.CryptoSelect == 0):
        #     Dict_Config['hSigPubKeyHashIdx'] = Dict_OpenSSL['EcFwSigKey_Idx']
        # elif(Util.CryptoSelect == 1):
        #     Dict_Config['hSigPubKeyHashIdx'] = Dict_CNG['SignCert_Idx']

        # if(((int)(Util.SecurityMode) != 0) and (bPriKey == 0)):
        #     Fn.OutputString(
        #         1, "Need an EcFwSigKey or EcFwCert<n>_Subject to calculate signature in Standard Secureboot Mode and PFR Mode.")
        #     Fn.ClearTmpFiles()

        # # if(AESencrypt and not bSessPrivKey):
        # #     Fn.OutputString(
        # #         1, "Need a <SessPrivKey> or <AESCert_Subject> to decrypt RAM Code in ROM.")
        # #     Fn.ClearTmpFiles()

    # GenOTP = True

    # Parse Fan Table
    # ParseFanTable()
    # if(len(Util.FanTables_Content) % Util.FanTableSize != 0):
    #     Fn.OutputString(1, "Fan Table Size not correct")
    #     Fn.ClearTmpFiles()
    # Util.FanTableCnt = (int)(len(Util.FanTables_Content) / Util.FanTableSize)
    # print('FanTableCnt = ', '{:x}'.format(Util.FanTableCnt))

    # OTP Field
    # if(GenOTP):
    #     Node = XmlTree.find('OTPbitmap')
    #     if((Node is not None) and Util.internal):
    #         Dict_OTP = ParseXmlwithTarget(Node, Util.Target.OTPbitmap)
    #         # if 'oSigPubKeySts' in Dict_OTP:
    #         #     ECPubKeySts = Fn.ConvertInt2Bin(Dict_OTP['oSigPubKeySts'], 8)
    #         #     HashTypeforECKey = 512 if(
    #         #         int(Dict_OTP['oSHA512Used'], 0) == 1) else 256

    #         if 'oSysFwSigPubKeySts' in Dict_OTP:
    #             SysPubKeySts = Fn.ConvertInt2Bin(
    #                 Dict_OTP['oSysFwSigPubKeySts'], 8)
    #             HashTypeforSystemKey = 512 if(
    #                 int(SysPubKeySts[3], 0) == 1) else 256
    #     elif(not Util.internal):
    #         if Util.SecurityMode == 0:
    #             Dict_OTP['oSecureBoot'] = str(0)
    #         else:
    #             if Util.MD:
    #                 Dict_OTP['oSecureBoot'] = str(1)
    #             elif not Util.MD:
    #                 Dict_OTP['oSecureBoot'] = str(0)
    #         Dict_OTP['oSecurityLvl'] = str(Util.hSecurityLvl)
    #         Dict_OTP['oSigPubKeySts'] = str(Fn.Bytes2Int(
    #             Fn.ConvertBitArray2Byte(Util.oSigPubKeySts), 'big'))
    #         Dict_OTP['oLongKeySel'] = str(Util.oLongKeySel)
    #         Dict_OTP['oRSAPKCPAD'] = str(Util.RSAPKCPAD)

    #     if Util.internal:
    #         HashTypeforECKey = 512 if(int(Dict_OTP['oSHA512Used'], 0) == 1) else 256

    Util.SetCryptoFunc(0)


def GenImage():
    try:
        if(Util.internal):
            GenOTPImage()
        # else:
        #     GenOTPImage_Cus()
        logging.info('GenImage OK')
    except BaseException as e:
        Fn.OutputString(1, 'GenImage failed')
        Fn.OutputString(1, 'Error in %s : %s' % (GenImage.__name__, e))
        logging.error('Error in %s : %s' % (GenImage.__name__, e))
        return


def RemoveOTP():
    try:
        os.chdir(Util.Path_Input)
        _ = Fn.getFileBin(Dict_Config['BaseEcFwImg'])
        _index = 0
        while _index < len(_):
            _index = Fn.SearchIndex(_, Util.OTPMagicString, _index+1, -1)
            if (_index & 0xFF) == 0:
                break

        OtpAlign = _index

        # get OTP align
        byte_offset = Util.DictColDet['hImageLen'][0]
        hImageLen = Fn.Bytes2Int(Fn.getBinPoint(_, byte_offset, 4), 'big')
        extra = Fn.GetAlign(
            hImageLen + Util.FW_Image_header_len, Util.OTP_Alignment)

        # Remove OTP image
        new_val = Fn.getFilePoint(
            Dict_Config['BaseEcFwImg'], 0, OtpAlign - extra)
        Fn.GenBinFilefromBytes(new_val, Dict_Config['BaseEcFwImg'])

        # set hOtpImgHdrOffset to zero
        new_val = Fn.ReservedData(32)
        byte_offset = Util.DictColDet['hOtpImgHdrOffset'][0]
        Fn.setBinPoint(new_val, byte_offset, Dict_Config['BaseEcFwImg'])

    # The input doesn't have OTP image, no need to remove
    except ValueError:
        pass


# region Main
# 0 NOTEST, 10 DEBUG, 20 INFO, 30 WARNING, 40 ERROR, 50 CRITICAL
logging.basicConfig(level=20, format='%(asctime)s - %(levelname)s : %(message)s',
                    filemode='w', filename='Log.txt')
os.system('color')
argv = sys.argv
logging.info('User input : %s' % argv)

XmlFile = ''
DumpFile = ''
Util.internal = False
Signature = True
# XmlFile = os.path.join(Util.Path_Xml, argv[-1])
XmlFile = argv[-1]

# Get argv
for i in range(0, len(argv)):
    tmp = argv[i].lower()
    if tmp == '/d':
        Signature = False
    elif tmp == '/g':
        Util.internal = True
    elif tmp == '/x':
        Util.Path_Custom = argv[i + 1]
        if Fn.CheckWorkDir(Util.Path_Custom):
            XmlFile = os.path.join(Util.Path_Xml, argv[i + 2])
        else:
            Fn.ClearTmpFiles()
    elif tmp == '/md':
        Util.MD = True
    elif tmp == '/otp':
        DumpFile = argv[i+1]
        DumpOTP(DumpFile)
        Fn.ClearTmpFiles()
    elif tmp == '/v':
        print(Util.version)
        Fn.ClearTmpFiles()
    elif tmp == '/h':
        Fn.HelpFunc()
        Fn.ClearTmpFiles()

# Check xml name and internal or not
if((XmlFile.find('_NTC_') >= 0) and (not Util.internal)):
    Fn.OutputString(0, 'The xml file is used in NTC, please check command.')
    Fn.ClearTmpFiles()
elif((XmlFile.find('_NTC_') < 0) and (Util.internal)):
    Fn.OutputString(
        0, 'The xml file is not used in NTC, please check command.')
    Fn.ClearTmpFiles()

if Util.internal:
    ParseXml(ReadXml(XmlFile))
    # if(GenOTP):
    #     GenImage()
    GenPacket(GenHeader())

# elif not Util.internal:
#     ParseXml(ReadXml(XmlFile))
#     if(GenOTP):
#         GenImage()
#     elif(not GenOTP):
#         RemoveOTP()
#     GenHeader_Cus()
#     GenPacket_Cus()

Fn.ClearTmpFiles()

# endregion
