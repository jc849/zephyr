#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import xml.etree.ElementTree as ET
import FunctionDefine as Fn
import logging
import math
import os
import Util


def ReadXml(FileName):
    # Makefile Usage
    tmpFile = os.path.join(Util.Path_Current, FileName)
    # print(tmpFile)
    FileList = [tmpFile]
    if (not Fn.CheckFile(FileList, re=1)):
        tmpFile = os.path.join(Util.Path_Xml, FileName)
        # print(tmpFile)
        FileList = [tmpFile]
        Fn.CheckFile(FileList)
    try:
        tree = ET.ElementTree(file=tmpFile)
        logging.info('Load xml file OK')
        return tree
    except BaseException as e:
        Fn.OutputString(0, 'Read xml file failed')
        logging.error('Error in %s : %s' % (ReadXml.__name__, e))
        Fn.ClearTmpFiles()


def ParseXmlwithTarget(Node, Target):
    try:
        Dict_Variable = {}
        t = 0
        for elem in Node.iter():
            Dict_Variable[elem.tag] = elem.text
            if (t == 0):
                Dict_Variable.pop(elem.tag)
                t = 1
            if (elem.tag == 'Table'):
                Util.FanTables_Name.append(elem.text)

        CheckXml(Dict_Variable, Target)
        logging.info('ParseXml OK, %s' % Target)
        return Dict_Variable

    except BaseException as e:
        Fn.OutputString(0, 'Error when Parse Xml')
        Fn.OutputString(0, 'Error in %s : %s' %
                        (ParseXmlwithTarget.__name__, e))
        logging.error('Error in %s : %s' % (ParseXmlwithTarget.__name__, e))
        Fn.ClearTmpFiles()


def ParseFanTable():
    for table in Util.FanTables_Name:
        print('Parse Fan Table')
        if(table is not None):
            table = os.path.join(Util.Path_Xml, table)
            xmltree = ReadXml(table)
            TableType = Util.FanTableType.Single

            for elem in xmltree.iter(tag=Util.FanTableTag_Profile):
                if (elem.get('Type') == 'Combine'):
                    TableType = Util.FanTableType.Combine

            for elem in xmltree.iter(tag=Util.FanTanleTag_Item):
                value = '0x' + elem.get('Value')
                value = int(value, 16)
                Util.FanTables_Content.append(value)
    else:
        print('table is not None')


def CheckXml(Dict_Variable, Target):
    if(Target == Util.Target.OTPbitmap):
        CheckZeroOrOne(Dict_Variable, 'oStrapMode1')
        CheckZeroOrOne(Dict_Variable, 'oStrapMode2')
        # CheckZeroOrOne(Dict_Variable, 'oNotTrySysIfSPI2')
        CheckZeroOrOne(Dict_Variable, 'oNotTrySysIfFIUPvt')
        CheckZeroOrOne(Dict_Variable, 'oNotTrySysIfFIUShd')
        CheckZeroOrOne(Dict_Variable, 'oNotTryMafAndAMD')
        CheckZeroOrOne(Dict_Variable, 'oNotTrySysIfSPI1')
        CheckZeroOrOne(Dict_Variable, 'oNotTrySysIfFIUBkp')
        # CheckZeroOrOne(Dict_Variable, 'oFIUPri4BMode')
        CheckZeroOrOne(Dict_Variable, 'oFIUShr4BMode')
        # 6694
        CheckZeroOrOne(Dict_Variable, 'oSPIP4BMode')
        CheckZeroOrOne(Dict_Variable, 'oSpiQuadPEn')
        CheckZeroOrOne(Dict_Variable, 'oECPTRCheckCRC')
        # CheckZeroOrOne(Dict_Variable, 'oSysAuthAlways')
        CheckZeroOrOne(Dict_Variable, 'oSecureBoot')
        CheckZeroOrOne(Dict_Variable, 'oSecurityLvl')
        CheckZeroOrOne(Dict_Variable, 'oHaltIfMafRollbk')
        CheckZeroOrOne(Dict_Variable, 'oHaltIfActiveRollbk')
        CheckZeroOrOne(Dict_Variable, 'oHaltIfOnlyMafValid')
        CheckZeroOrOne(Dict_Variable, 'oTryBootIfAllCrashed')
        CheckZeroOrOne(Dict_Variable, 'oUnmapRomBfXferCtl')
        CheckZeroOrOne(Dict_Variable, 'oDisableDBGAtRst')
        CheckZeroOrOne(Dict_Variable, 'oAESKeyLock')
        CheckZeroOrOne(Dict_Variable, 'oHWCfgFieldLock')
        CheckZeroOrOne(Dict_Variable, 'oOtpRgn2Lock')
        CheckZeroOrOne(Dict_Variable, 'oOtpRgn3Lock')
        CheckZeroOrOne(Dict_Variable, 'oOtpRgn4Lock')
        CheckZeroOrOne(Dict_Variable, 'oOtpRgn5Lock')
        CheckZeroOrOne(Dict_Variable, 'oOtpRgn6Lock')
        CheckZeroOrOne(Dict_Variable, 'oRSAPKCPAD')
        CheckZeroOrOne(Dict_Variable, 'oRetryLimitEn')
        CheckZeroOrOne(Dict_Variable, 'oSkipCtyptoSelfTest')
        CheckZeroOrOne(Dict_Variable, 'oOlnyLogCriticalEvent')
        CheckZeroOrOne(Dict_Variable, 'oLongKeySel')
        CheckZeroOrOne(Dict_Variable, 'oOTPRegionRdLock')
        CheckZeroOrOne(Dict_Variable, 'oClrRamExitRom')
        CheckZeroOrOne(Dict_Variable, 'oVSpiExistNoTimeOut')
        CheckZeroOrOne(Dict_Variable, 'oNoWaitVSpiExist')
        CheckZeroOrOne(Dict_Variable, 'oTryBootNotCtrlFWRdy')
        CheckZeroOrOne(Dict_Variable, 'oNotUpdateToPrvFw')
        CheckZeroOrOne(Dict_Variable, 'oPartID')
        CheckZeroOrOne(Dict_Variable, 'oAESNotSupport')
        CheckZeroOrOne(Dict_Variable, 'oMCUClkNNotDiv2')
        CheckZeroOrOne(Dict_Variable, 'oMCPRdEdge')
        CheckZeroOrOne(Dict_Variable, 'oMCPSel')
        CheckZeroOrOne(Dict_Variable, 'oANAD2High')
        CheckZeroOrOne(Dict_Variable, 'oValANAD')
        CheckZeroOrOne(Dict_Variable, 'oValDevId')
        CheckZeroOrOne(Dict_Variable, 'oValRSMRST_L')
        CheckZeroOrOne(Dict_Variable, 'oValRSMRST_Sys')
        CheckZeroOrOne(Dict_Variable, 'oDivMinHigh')
        CheckZeroOrOne(Dict_Variable, 'oValDivMin')
        CheckZeroOrOne(Dict_Variable, 'oDivMaxHigh')
        CheckZeroOrOne(Dict_Variable, 'oValDivMax')
        CheckZeroOrOne(Dict_Variable, 'oFrcDivHigh')
        CheckZeroOrOne(Dict_Variable, 'oValFrcDiv')
        CheckZeroOrOne(Dict_Variable, 'oValFR_CLK')
        # 6694
        CheckZeroOrOne(Dict_Variable, 'oRefOTPClk')
        CheckZeroOrOne(Dict_Variable, 'oXFRANGE')

        CheckRange(Dict_Variable, 'oFlashConnection', 2)
        CheckRange(Dict_Variable, 'oMCPFlashSize', 2)
        CheckRange(Dict_Variable, 'oMCPFLMode', 2)
        CheckRange(Dict_Variable, 'oFIUShrFLMode', 2)
        # 6694
        CheckRange(Dict_Variable, 'oSPIPFLMode', 2)
        # CheckRange(Dict_Variable, 'oSPIMFLMode', 2)

        CheckRange(Dict_Variable, 'oFIUClkDiv', 2)
        # 6694
        CheckRange(Dict_Variable, 'oSPIPClkDiv', 2)
        CheckRange(Dict_Variable, 'oSPIMClkDiv', 2)

        CheckRange(Dict_Variable, 'oSecEvnLogLoc', 16)

        # 6694
        # CheckRange(Dict_Variable, 'oSigPubKeySts', 8)
        CheckRange(Dict_Variable, 'oRSAPubKeySts', 2)
        CheckRange(Dict_Variable, 'oECPubKeySts', 2)
        CheckRange(Dict_Variable, 'oRevokeKeySts', 2)
        CheckRange(Dict_Variable, 'oLongKeyUsed', 1)
        CheckRange(Dict_Variable, 'oSHA512Used', 1)
        CheckRange(Dict_Variable, 'oAESDecryptEn', 2)

        CheckRange(Dict_Variable, 'oSysPfrWP0En', 8)
        CheckRange(Dict_Variable, 'oSysPfrWP1En', 8)
        CheckRange(Dict_Variable, 'oSysPfrWP1En', 8)
        CheckRange(Dict_Variable, 'oOTPDatValid0', 8)
        CheckRange(Dict_Variable, 'oOTPDatValid1', 8)
        # 6694
        CheckRange(Dict_Variable, 'oLed1Sel', 4)
        CheckRange(Dict_Variable, 'oLed1Pole', 1)
        CheckRange(Dict_Variable, 'oLed2Sel', 4)
        CheckRange(Dict_Variable, 'oLed2Pole', 1)
        CheckRange(Dict_Variable, 'oLedActRbkBlkDef', 4)
        CheckRange(Dict_Variable, 'oLedSysRbkBlkDef', 4)
        CheckRange(Dict_Variable, 'oLedFwCpyBlkDef', 4)
        CheckRange(Dict_Variable, 'oLedSysOnlyBlkDef', 4)
        CheckRange(Dict_Variable, 'oLedAllCrashBlkDef', 4)
        CheckRange(Dict_Variable, 'oLedCryptoTestFailBlkDef', 4)
        # CheckRange(Dict_Variable, 'oRomMlbxLoc', 16)

        CheckRange(Dict_Variable, 'oMCPRdDly', 3)

        CheckRange(Dict_Variable, 'oANAD2Low', 8)
        CheckRange(Dict_Variable, 'oDevId', 5)
        CheckRange(Dict_Variable, 'oRSMRST_L', 8)
        CheckRange(Dict_Variable, 'oRSMRST_Sys', 8)
        CheckRange(Dict_Variable, 'oDivMinLow', 8)
        CheckRange(Dict_Variable, 'oDivMaxLow', 8)
        CheckRange(Dict_Variable, 'oFrcDivLow', 8)
        CheckRange(Dict_Variable, 'oFR_CLK', 4)
        CheckRange(Dict_Variable, 'oOTPWriteTime', 8)
        CheckRange(Dict_Variable, 'oDnxRsmrstWidth', 8)
        CheckRange(Dict_Variable, 'oDnxDPOkWidth', 8)
        CheckRange(Dict_Variable, 'oECTestMode0', 16)

        CheckRange(Dict_Variable, 'oVSpiExistWaitCnter', 8)
        CheckRange(Dict_Variable, 'oPwmLedAdj', 8)
        CheckRange(Dict_Variable, 'oFPRED', 4)
        CheckRange(Dict_Variable, 'oAHB6DIV', 2)
        CheckRange(Dict_Variable, 'oAPB1DIV', 4)
        CheckRange(Dict_Variable, 'oAPB2DIV', 4)
        CheckRange(Dict_Variable, 'oAPB3DIV', 4)

        CheckRange(Dict_Variable, 'oChipTesterID', 32)
        CheckRange(Dict_Variable, 'oSysFwSigPubKeySts', 8)
        CheckRange(Dict_Variable, 'oUserDataField', 160)
        CheckRange(Dict_Variable, 'oUserDataField1', 128)
        CheckRange(Dict_Variable, 'oUserDataField2', 128)
        CheckRange(Dict_Variable, 'oUserDataField3', 128)
        CheckRange(Dict_Variable, 'oUserDataField4', 128)


def CheckZeroOrOne(Dict, s):
    if(Util.internal):
        if(int(Dict[s], 0) != 0) and (int(Dict[s], 0) != 1):
            Fn.OutputString(1, '<%s> value error, please check xml.' % s)
            Fn.ClearTmpFiles()
    else:
        if s in Dict:
            if(int(Dict[s], 0) != 0) and (int(Dict[s], 0) != 1):
                Fn.OutputString(1, '<%s> value error, please check xml.' % s)
                Fn.ClearTmpFiles()
        else:
            pass


def CheckRange(Dict, s, bits):
    if(Util.internal):
        value = int(Dict[s], 0)
        if(value > math.pow(2, bits) - 1):
            Fn.OutputString(1, '<%s> value error, please check xml.' % s)
            Fn.ClearTmpFiles()
    else:
        if s in Dict:
            value = int(Dict[s], 0)
            if(value > math.pow(2, bits) - 1):
                Fn.OutputString(1, '<%s> value error, please check xml.' % s)
                Fn.ClearTmpFiles()
        else:
            pass
