#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import os
import logging
import FunctionDefine as Fn
from shutil import copy
import Util
from NuMsCNG import *
#from pyMsCNG import *

def CNGDigest(Path, File, Hash):
	path_temp = os.getcwd()
	os.chdir(Path)
	ToBeDigest = Fn.getFileBin(File)
	if(Hash == 256):
		CNGHash = NuCNGDigest(list(ToBeDigest), Util.SHA256)
	elif(Hash == 512):
		CNGHash = NuCNGDigest(list(ToBeDigest), Util.SHA512)
	os.chdir(path_temp)
	return CNGHash

def CNG_SignFile(CertName, PadType, Hash, Salt, FileName):
	Path_Temp = os.getcwd()

	SHA = Util.SHA512 if (Hash == 512) else Util.SHA256
	KeyType = Util.Key_EC
	PadType = Util.RSASSA_PKCS1V15 if(PadType == 0) else Util.RSASSA_PSS
	#Salt = int(Salt, 0)
	SignData = ""

	try:
		# hash data by NuCNGDigest
		bdata = Fn.getFileBin(FileName)
		DigestData =  NuCNGDigest(list(bdata), SHA)
		# sign by NuCNGSign
		SignData = NuCNGSignHash(list(DigestData), CertName, SHA, KeyType, Util.Sign_NoPadding, 0)
		if(SignData is None):
			KeyType = Util.Key_RSA
			SignData = NuCNGSignHash(list(DigestData), CertName, SHA, KeyType, PadType, Salt)
			if(SignData is None):
				raise Util.CNGError('NuCNGSignHash failed')

		ba = bytearray(SignData)

		# self verify
		# result = NuCNGVerifyHash(list(DigestData), list(SignData), CertName, SHA, KeyType, PadType, Salt)
		# print('result = %x' % (result))

		# RSA Key
		if(KeyType == 1):
			for x in range(0, Util.Signature_len - len(ba)):
				ba.append(0x00)
			return ba
		# EC Key
		else:
			for x in range(0, Util.Signature_len - len(ba)):
				ba.append(0x00)
			return ba

	except BaseException as e:
		Fn.OutputString(0, 'CNG Sign File failed')
		Fn.OutputString(0, 'Error in %s, %s : %s' % (CNG_SignFile.__name__, 'CNG_SignFile', e))
		logging.error('Error in %s, %s : %s' % (CNG_SignFile.__name__, 'CNG_SignFile', e))
		Fn.ClearTmpFiles()
	finally:
		Fn.DeleteFile('Sign.sign')
		os.chdir(Path_Temp)

def CNG_SymEncrypt(FileName, FileIV, FileKey, outputname):
	os.chdir(Util.Path_Input)
	iv = Fn.getFileBin(FileIV)[0:16]
	key = Fn.getFileBin(FileKey)
	# Fn.DeleteFile(FileIV)
	# Fn.DeleteFile(FileKey)
	data = Fn.getFileBin(FileName)

	try:
		#print(siv)
		#print(skey)
		EncryptedData = NuCNGSymmetricEncrypt(list(data), list(iv), list(key))
		return Fn.GenBinFilefromBytes(EncryptedData, outputname)

	except BaseException as e:
		Fn.OutputString(0, 'CNG AES Encrypt failed')
		Fn.OutputString(0, 'Error in %s : %s' % (CNG_SymEncrypt.__name__, e))
		logging.error('Error in %s : %s' % (CNG_SymEncrypt.__name__, e))
		Fn.ClearTmpFiles()
	finally:
		pass

def CNG_ExportPrivKey(CertName):
	try:
		# Get EC Private Key
		PrivKey = NuCNGExportPrivKey(CertName, Util.Key_EC)
		if(len(PrivKey) == 0):
			raise Util.CNGError("PrivKey length is zero")
		if(PrivKey is None):
			raise Util.CNGError("Export Private Key failed")
		return PrivKey

	except BaseException as e:
		Fn.OutputString(1, 'CNG Export Private Key failed')
		Fn.OutputString(1, 'Error in %s : %s' % (CNG_ExportPrivKey.__name__, e))
		logging.error('Error in %s : %s' % (CNG_ExportPrivKey.__name__, e))
		Fn.ClearTmpFiles()

def CNG_ExportPubKey(CertName, totallen = 64, reFile = 0):
	Path_Temp = os.getcwd()
	os.chdir(Util.Path_Key)
	try:
		# Get Public Key
		PubKey = NuCNGExportPubKey(CertName, Util.Key_RSA)
		if(PubKey is None):
			PubKey = NuCNGExportPubKey(CertName, Util.Key_EC)
			if(PubKey is None):
				raise Util.CNGError("Export Public Key failed")

		# Gen File or return data
		if(reFile == 1):
			return Fn.GenBinFilefromBytes(PubKey, 'FileTemp3')
		else:
			ba = bytearray(PubKey)
			reserved = Fn.ReservedData((totallen - len(ba)) * 8)
			for i in range(0, len(reserved)):
				ba.append(reserved[i])
			return (bytes(ba))

		return PubKey
	except BaseException as e:
		Fn.OutputString(1, 'CNG Export PubKey failed')
		Fn.OutputString(1, 'Error in %s : %s' % (CNG_ExportPubKey.__name__, e))
		logging.error('Error in %s : %s' % (CNG_ExportPubKey.__name__, e))
		Fn.ClearTmpFiles()
	finally:
		os.chdir(Path_Temp)

def CNG_Derive(CertName):
	Path_Temp = os.getcwd()
	os.chdir(Util.Path_Key)
	try:
		SharedSecret, PubKey= NuCNGDeriveKey(CertName)
		return Fn.GenBinFilefromBytes(SharedSecret, '_SS'), Fn.GenBinFilefromBytes(PubKey, 'FileTemp3')

	except BaseException as e:
		Fn.OutputString(0, 'CNG Derive failed')
		Fn.OutputString(1, 'Error in %s : %s' % (CNG_Derive.__name__, e))
		logging.error('Error in %s : %s' % (CNG_Derive.__name__, e))
		Fn.ClearTmpFiles()
	finally:
		os.chdir(Path_Temp)

def CNG_IdentifyKeyType(CertName):
	KeyType = Util.KeyType.RSA
	KeyLonger = False
	Key3072_384 = False
	try:
		# Get Public Key
		PubKey = NuCNGExportPubKey(CertName, Util.Key_RSA)
		if(PubKey is None):
			PubKey = NuCNGExportPubKey(CertName, Util.Key_EC)
			KeyType = Util.KeyType.ECDSA
			if(PubKey is None):
				raise Util.CNGError("Identify Key Type failed")

		# RSA2048->256, RSA3072->384, RSA4096->512
		# P256->64, P384->96, P521->132
		# longer key
		if (len(PubKey) == 512 or len(PubKey) == 132):
			KeyLonger = True
		# USE RSA3072, EC-384
		if (len(PubKey) == 384 or len(PubKey) == 96):
			KeyLonger = True
			Key3072_384 = True
		return KeyType, KeyLonger, Key3072_384

	except BaseException as e:
		Fn.OutputString(1, 'Identify Key Type failed')
		Fn.OutputString(1, 'Error in %s : %s' % (CNG_ExportPubKey.__name__, e))
		logging.error('Error in %s : %s' % (CNG_ExportPubKey.__name__, e))
		Fn.ClearTmpFiles()