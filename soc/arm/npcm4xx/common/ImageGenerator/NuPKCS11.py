#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os
import logging
import getpass
import binascii
from shutil import copy
import Util
import FunctionDefine as Fn
# from PyKCS11 import *
# from PyKCS11 import Session
from OpenSSL import OpenSSL_Digest

def PKCS11_SignFile(Hash, FileName, objID):
    Path_Temp = os.getcwd()

    try:
        obj_ID = ((int)(objID),)
        pkcs11 = PyKCS11Lib()
        pkcs11.load("C:\SoftHSM2\lib\softhsm2.dll")

        # get 1st slot, open session
        slot = pkcs11.getSlotList(tokenPresent=True)[0]
        session = pkcs11.openSession(slot, CKF_SERIAL_SESSION | CKF_RW_SESSION)
        passwd = getpass.getpass("Please input session password: ")
        session.login(passwd)

        # get privkey
        privKey = session.findObjects([(CKA_CLASS, CKO_PRIVATE_KEY), (CKA_ID, obj_ID)])[0]
        d = privKey.to_dict()
        KeyType = d['CKA_KEY_TYPE']

        # setting Mechanism
        # CKM_SHA256_RSA_PKCS
        # CKM_SHA512_RSA_PKCS
        if(KeyType == 'CKK_RSA'):
            mechanism = Mechanism(CKM_SHA512_RSA_PKCS, None)  if (Hash == 512) else Mechanism(CKM_SHA256_RSA_PKCS, None)
            # prepare data
            toSign = Fn.getFileBin(FileName)
            signature = session.sign(privKey, (bytes)(toSign), mechanism)
            #print("\nsignature: {}".format(binascii.hexlify(bytearray(signature))))
            ba = bytearray(signature)
            for x in range(0, Util.Signature_len - len(ba)):
                ba.append(0x00)
            return ba

        # CKM_ECDSA (Should digest manully first)
        elif(KeyType == 'CKK_EC'):
            mechanism = Mechanism(CKM_ECDSA, None)
            # prepare data
            toSign = OpenSSL_Digest(Util.Path_Input, FileName, Hash)

            signature = session.sign(privKey, (bytes)(toSign), mechanism)
            signature = (bytearray)(signature)
            #print("\nsignature: {}".format(binascii.hexlify((signature))))

            for x in range(0, Util.Signature_len - len(signature)):
                signature.append(0x00)

            return signature

        else:
            Fn.OutputString(0, 'Key Type not support')

        # close session
        session.logout()
        session.closeSession()
    except PyKCS11.PyKCS11Error as e:
        Fn.OutputString(1, e)

    finally:
        #DeleteFile('Sign.sign')
        os.chdir(Path_Temp)