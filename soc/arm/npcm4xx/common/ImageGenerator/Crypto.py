#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import logging
import Util
from OpenSSL import *
from NuMsCNG import *
#from Cryptodome.Cipher import AES
#from NuPKCS11 import *


def CryptoDigest(Path, File, Hash):
    CryptoMethod = GetMethod()
    if(CryptoMethod == 0):
        return OpenSSL_Digest(Path, File, Hash)
    else:
        return CNGDigest(Path, File, Hash)


def CryptoSign():
    pass


def CryptoGetPubKey(Target, totallen=64, reFile=0):
    pass


def CryptoGetPrivKey():
    pass


def CryptoReadKey():
    pass


def CryptoGenKey(KeyType):
    pass


def CryptoGenAESKey():
    pass


def CryptoAESEncrypt():
    pass


def GetMethod():
    return Util.CryptoSelect
