#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os
from zlib import crc32
from shutil import rmtree
import Util


def OutputString(Func, message):
    if Func == 0:
        print('Warning : %s' % message, flush=True)
    elif Func == 1:
        print('Error : %s' % message, flush=True)


def CheckWorkDir(WorkDir):
    check = True
    if os.path.isdir(WorkDir):
        tmpPath = os.path.join(WorkDir, 'NuveSIOBinGen')
        if os.path.isdir(tmpPath):
            tmpXmlPath = os.path.join(tmpPath, 'Xml')
            tmpInputPath = os.path.join(tmpPath, 'Input')
            tmpKeyPath = os.path.join(tmpPath, 'Key')
            if not os.path.isdir(tmpXmlPath):
                OutputString(0, 'Xml folder not exist')
                check = False
            if not os.path.isdir(tmpInputPath):
                OutputString(0, 'Input folder not exist')
                check = False
            if not os.path.isdir(tmpKeyPath):
                OutputString(0, 'Key folder not exist')
                check = False
        else:
            OutputString(0, 'NuveSIOBinGen folder not exist')
            check = False
    else:
        OutputString(0, 'Work directory not exist')
        check = False
    if(check):
        Util.Path_Current = os.path.join(WorkDir, 'NuveSIOBinGen')
        Util.SetPath()
        return True
    else:
        return False


def CheckOutputDir():
    temp = os.getcwd()
    os.chdir(Util.Path_Current)
    if os.path.isdir(Util.Path_Output):
        rmtree(Util.Path_Output)
    os.mkdir('Output')
    os.chdir(temp)


def CheckFile(FileList, re=0):
    for File in FileList:
        if not os.path.exists(File):
            if (re == 1):
                return False
            OutputString(0, '%s not found' % File)
            ClearTmpFiles()
        return True


def GetAlign(length, align):
    re = length % align
    if(re != 0):
        add = align - re
        return add
    return 0


def RevokeKey(KeyIdx):
    List = []
    if(KeyIdx is None):
        List.append(0)
        List.append(0)
    else:
        iKeyIdx = int(KeyIdx, 0)
        if(iKeyIdx == 88):
            List.append(88)  # 0x58 = 88
            List.append(167)  # ~88 = B1010 0111 = A7h = 167
        elif(iKeyIdx == 89):
            List.append(89)  # 0x59 = 89
            List.append(166)  # ~89 = B1010 0110 = A6h = 166
    return(bytes(List))


def MajorVerLogged(num, length):
    temp = [0] * (length * 8 - 1)
    if((num < 1) or (num > length * 8)):
        OutputString(
            0, 'Major version out of range. Please check <oMajorVerLogged> and <oSysFWMajorVerLogged>')
    temp.insert(num - 1, 1)  # little endian
    return ConvertBitArray2Byte(temp, length)


def ReservedData(bitLength):
    List = []
    if(bitLength % 8 == 0):
        bitLength = (int)(bitLength / 8)
        for x in range(0, bitLength):
            List.append(0)
    return(bytes(List))


def ConvertInt2Bin(num, bitcount):
    bitarray = bitfield(int(num, 0))
    add = bitcount - len(bitarray)
    if(add != 0):
        for i in range(0, add):
            bitarray.insert(0, '0')
    return bitarray


def ConvertBitArray2Byte(datalist, offset=1):
    temp = []
    for i in range(0, len(datalist)):
        if type(datalist[i]) != list:
            temp.append(datalist[i])
        else:
            for j in range(0, len(datalist[i])):
                temp.append((int)(datalist[i][j]))
    c = 0
    for i in range(0, len(temp)):
        c = ((c << 1) | temp[i])
    return (c).to_bytes(offset, byteorder='big')


def Text2Bin(text, length):
    reserved = length - len(text)
    for x in range(0, reserved):
        text += '0'
    return text.encode('ascii')


def Bytes2Int(data, endian):
    if(endian == 'big'):
        return int.from_bytes(data, byteorder='big')
    if(endian == 'little'):
        return int.from_bytes(data, byteorder='little')


def Int2Bytes(data, len, endian):
    if(endian == 'big'):
        return (data).to_bytes(len, byteorder='big')
    if(endian == 'little'):
        return (data).to_bytes(len, byteorder='little')


def ByteArray2HexString(array):
    hexstr = ''.join('{:02x}'.format(x) for x in array)
    return hexstr


def bitfield(n):
    return [(digit) for digit in bin(n)[2:]]  # [2:] to chop off the "0b" part


def getFileBin(FileName, offset=0):
    with open(FileName, 'rb') as fin:
        fin.seek(offset)
        data = fin.read()
    return data


def getFilePoint(FileName, offset, length):
    with open(FileName, 'rb') as fin:
        fin.seek(offset)
        data = fin.read(length)
    return data


def getBinPoint(binary, offset, length):
    tmp = []
    for x in range(offset, offset + length):
        tmp.append(bytearray(binary)[x])
    return tmp


def setBinPoint(binary, offset, FileName, File=0):
    if(File == 0):
        with open(FileName, 'r+b') as fout:
            fout.seek(offset)
            fout.write(binary)
    elif(File == 1):
        with open(FileName, 'r+b') as fout:
            tempA = getFilePoint(FileName, 0, offset)
            tempB = getFilePoint(FileName, offset, os.path.getsize(FileName))
            fout.write(tempA)
            fout.write(binary)
            fout.write(tempB)


def GetCRC32(FileName):
    bdata = getFileBin(FileName)
    crc = crc32(bdata)
    return crc.to_bytes(8, byteorder='big')


def DeleteFile(FileName):
    if os.path.isfile(FileName):
        os.remove(FileName)


def GenFileCombine(FileList, outFileName):
    os.chdir(Util.Path_Input)
    data = []
    for i in range(0, len(FileList)):
        temp = getFileBin(FileList[i])
        data.append(temp)
    name = GenBinFilefromList(data, outFileName)
    return name


def GenFileAlign(FileName, align, outFileName):
    data = getFileBin(FileName)
    length = GetAlign(len(data), align)
    with open(outFileName, 'wb') as fout:
        fout.write(data)
        for i in range(0, length):
            fout.write(b'\x0F')
    return outFileName


def GenBinFilefromList(MyList, outFileName):
    with open(outFileName, 'wb') as fout:
        for i in range(0, len(MyList)):
            fout.write(MyList[i])
    return outFileName


def GenBinFilefromBytes(MyBytes, outFileName):
    with open(outFileName, 'wb') as fout:
        fout.write(MyBytes)
    return outFileName


def ForwardPath(Path):
    Path_Temp = os.getcwd()
    os.chdir(Path)
    return Path_Temp


def SearchIndex(File, Target, Start, End):
    return File.index(Target, Start, End)


def ClearTmpFiles():
    # Delete temp files
    os.chdir(Util.Path_Input)
    DeleteFile(Util.sBBName_Encrypt)
    DeleteFile(Util.sDataCodeName)
    DeleteFile(Util.sRAMCodeName)
    DeleteFile(Util.sCombineName)
    DeleteFile(Util.sFlashCodeName)
    DeleteFile(Util.sRAMCodeAlignName)
    DeleteFile(Util.sCombineName_Encrypt)
    DeleteFile(Util.sRAMCodeName_Encrypt)
    DeleteFile('SignTemp')
    DeleteFile('SignField')
    DeleteFile('Sign.sign')
    DeleteFile('oOtpRegion0Digest')
    # DeleteFile('OutputOTPImage')
    DeleteFile('tmpInput')
    DeleteFile('BBImage')
    DeleteFile('seg_1')
    DeleteFile('seg_2')
    DeleteFile('seg_3')
    DeleteFile('HashTotal')
    DeleteFile('HookSegFile')
    DeleteFile('ListFWHeaderCol')
    DeleteFile('RAMCode_BB')
    DeleteFile('RAMCodeBBCombine')

    # os.chdir(Util.Path_Key)
    # DeleteFile(Util.sAESPubKeyName)
    # DeleteFile('FileTemp2')
    # DeleteFile('FileTemp3')
    # DeleteFile('oOtpRegion0Digest')
    # DeleteFile('_SS')
    # os._exit(0)


def HelpFunc():
    print('Gen Image:\tNuveSIOBinGenTool.exe </X WorkingDirectory> XmlFile')
    print('Show Version:\tNuveSIOBinGenTool.exe  /v')
