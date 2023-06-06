#!/usr/bin/env python3
#
# Copyright (c) 2020 Nuvoton Technology Corporation
#
# SPDX-License-Identifier: Apache-2.0

import FunctionDefine as Fn
import Util
from shutil import copy
import logging
import os
#from Cryptodome.Cipher import AES


def OpenSSL_SignFile(hash, rsapad, FileName, PriKey1):
    os.chdir(Util.Path_Input)
    src = os.path.join(Util.Path_Key, PriKey1)
    copy(src, Util.Path_Input)
    blonger_key = 0
    b3072_384 = 0
    TargetFormat = 'PEM'
    PriKeyFormat1, PriKeyType1, LongKey = IdentifyPrivKey(PriKey1)
    if (PriKeyFormat1 == Util.KeyFormat.DER):
        TargetFormat = 'DER'
    sha = '-sha512' if (hash == 512) else '-sha256'
    if(PriKeyType1 == Util.KeyType.RSA):
        if(rsapad == 1):
            sigopt = '-sigopt rsa_padding_mode:pss'
        else:
            sigopt = ''
        try:
            # check length and long key
            cmd = 'openssl pkey -inform %s -in %s -text -noout' % (
                TargetFormat, PriKey1)
            s = os.popen(cmd).read()
            if (s.find('2048') != -1):
                pass
                # print('private key identify...RSA2048')
            elif (s.find('4096') != -1):
                blonger_key = 1
                # print('private key identify...RSA4096')
            elif (s.find('3072') != -1):
                blonger_key = 1
                b3072_384 = 1
                # print('private key identify...RSA3072')
            else:
                raise Util.KeyTypeError('RSA Key length not support')
            # RSA sign hash
            # cmd = 'openssl dgst -binary %s %s -> hash' % (sha, FileName)
            # os.system(cmd)
            #print('signed file HASH : %s' % os.popen(cmd).read())
            # openssl dgst -sha256 -sign rsa2048key_pri.pem -sigopt rsa_padding_mode:pss -out sign_pss src.bin
            # RSA sign
            cmd = 'openssl dgst %s -keyform %s -sign %s %s %s > %s' % (
                sha, TargetFormat, PriKey1, sigopt, FileName, 'Sign.sign')
            os.system(cmd)
            # verify RSA
            #cmd = 'openssl dgst %s -keyform %s -prverify %s -signature %s %s' % (sha, TargetFormat, PriKey1, 'Sign.sign', FileName)
            #s = os.popen(cmd).read()
            # if(s.find('Verified OK') != -1):
            #	print('signature verify...pass')
            # else:
            #	print('signature verify...failed')
            #	raise
            # get sign
            bdata = Fn.getFileBin('Sign.sign')
            ba = bytearray(bdata)
            if(not LongKey):
                for x in range(0, Util.Signature_len - len(ba)):
                    ba.append(0x00)
            # ba.reverse()
            return ba
        except BaseException as e:
            Fn.OutputString(1, 'Signing file failed')
            Fn.OutputString(1, 'Error in %s, %s : %s' %
                            (OpenSSL_SignFile.__name__, 'OpenSSL_SignFile', e))
            logging.error('Error in %s, %s : %s' %
                          (OpenSSL_SignFile.__name__, 'OpenSSL_SignFile', e))
            Fn.ClearTmpFiles()
        finally:
            Fn.DeleteFile('FileTemp')
            # Fn.DeleteFile('Sign.sign')
            Fn.DeleteFile(PriKey1)
    # ECDSA
    elif(PriKeyType1 == Util.KeyType.ECDSA):
        try:
            os.chdir(Util.Path_Input)
            # check length and long key
            cmd = 'openssl ec -inform %s -in %s -text -noout' % (
                TargetFormat, PriKey1)
            s = os.popen(cmd).read()
            if (s.find('256') != -1):
                pass
                # print('private key identify...P-256')
            elif (s.find('521') != -1):
                blonger_key = 1
                # print('private key identify...P-521')
            elif (s.find('384') != -1):
                blonger_key = 1
                b3072_384 = 1
                # print('private key identify...P-384')
            else:
                raise Util.KeyTypeError('EC Key curve not support')
            # ECDSA sign hash
            # cmd = 'openssl dgst %s %s' % (sha, FileName)
            # print('signed file HASH : %s' % os.popen(cmd).read())
            # ECDSA sign
            cmd = 'openssl dgst %s -keyform %s -sign %s %s > %s' % (
                sha, TargetFormat, PriKey1, FileName, 'Sign.sign')
            os.system(cmd)
            # verify EC
            #cmd = 'openssl dgst %s -keyform %s -prverify %s -signature %s %s' % (sha, TargetFormat, PriKey1, 'Sign.sign', FileName)
            #s = os.popen(cmd).read()
            # if(s.find('Verified OK') != -1):
            #	print('signature verify...pass')
            # else:
            #	print('signature verify...failed')
            #	raise
            # get sign
            bdata = Fn.getFileBin('Sign.sign')
            ba = bytearray(bdata)
            r, s = GetRS(ba, blonger_key, b3072_384)

            for i in range(0, len(s)):
                r.append(s[i])

            for x in range(0, Util.Signature_len - len(r)):
                r.append(0x00)

            # r1.reverse()
            return r

        except BaseException as e:
            Fn.OutputString(1, 'Signing file failed')
            Fn.OutputString(1, 'Error in %s, %s : %s' %
                            (OpenSSL_SignFile.__name__, 'ECDSA', e))
            logging.error('Error in %s, %s : %s' %
                          (OpenSSL_SignFile.__name__, 'ECDSA', e))
            Fn.ClearTmpFiles()
        finally:
            Fn.DeleteFile('FileTemp')
            # Fn.DeleteFile('Sign.sign')
            Fn.DeleteFile(PriKey1)


def OpenSSL_Digest(path, FileName, SHA):
    path_temp = os.getcwd()
    os.chdir(path)
    if(SHA == 256):
        cmd = 'openssl dgst -sha256 %s' % (FileName)
    elif(SHA == 512):
        cmd = 'openssl dgst -sha512 %s' % (FileName)
    s = os.popen(cmd).read()  # SHA256(openssl.exe)= b107...
    pre, suf = s.split(' ')
    suf = suf[0:-1]
    ba = bytearray.fromhex(suf)
    os.chdir(path_temp)
    return ba


def OpenSSL_GetPubKey(PubKey1, totallen=64, reFile=0):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        PubKeyFormat1, PubKeyType1, LongKey, key3072_384 = IdentifyPubKey(
            PubKey1)
        if(PubKeyType1 == Util.KeyType.ECDSA):
            XYpair = []
            if(PubKeyFormat1 == Util.KeyFormat.PEM):
                cmd = 'openssl pkey -pubin -inform PEM -outform DER -in %s -out %s' % (
                    PubKey1, 'ECDSAkey_pub_temp-1.der')
                os.system(cmd)
                PubKey1 = 'ECDSAkey_pub_temp-1.der'
            bdata1 = Fn.getFileBin(PubKey1)

            s = 26 if (LongKey) else 27
            if(LongKey and key3072_384):
                s = 24
            for i in range(s, len(bdata1)):
                XYpair.append(bdata1[i])  # 64 bytes or 132 bytes(Long Key)

            if(reFile == 1):
                return Fn.GenBinFilefromBytes(bytes(XYpair), 'FileTemp2')
            else:
                reserved = Fn.ReservedData((totallen - len(XYpair)) * 8)
                for i in range(0, len(reserved)):
                    XYpair.append(reserved[i])
                return (bytes(XYpair))
        elif(PubKeyType1 == Util.KeyType.RSA):
            modulus = []
            if(PubKeyFormat1 == Util.KeyFormat.PEM):
                cmd = 'openssl pkey -pubin -inform PEM -outform DER -in %s -out %s' % (
                    PubKey1, 'RSAkey_pub_temp.der')
                os.system(cmd)
                PubKey1 = 'RSAkey_pub_temp.der'
            bdata = Fn.getFileBin(PubKey1)
            for i in range(33, len(bdata) - 5):
                modulus.append(bdata[i])  # 256 bytes or 512 bytes(Long Key)
            if(reFile == 1):
                return Fn.GenBinFilefromBytes(bytes(modulus), 'FileTemp3')
            else:
                reserved = Fn.ReservedData((totallen - len(modulus)) * 8)
                for i in range(0, len(reserved)):
                    modulus.append(reserved[i])
                return (bytes(modulus))
        else:
            raise Util.KeyTypeError(
                'Key Type Unknown, should be RSA or ECDSA.')

    except BaseException as e:
        Fn.OutputString(1, 'Error in %s : %s' %
                        (OpenSSL_GetPubKey.__name__, e))
        logging.error('Error in %s : %s' % (OpenSSL_GetPubKey.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        Fn.DeleteFile('FileTemp')
        # Fn.DeleteFile('FileTemp2')
        # Fn.DeleteFile('FileTemp3')
        Fn.DeleteFile('ECDSAkey_pub_temp-1.der')
        Fn.DeleteFile('RSAkey_pub_temp.der')
        os.chdir(Path_Temp)


def OpenSSL_GetPrivKey(PrivKey):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        PrivKeyFormat, PrivKeyType, LongKey = IdentifyPrivKey(PrivKey)
        if(PrivKeyType == Util.KeyType.ECDSA):
            priv = []
            if(PrivKeyFormat == Util.KeyFormat.PEM):
                cmd = 'openssl pkey -inform PEM -outform DER -in %s -out %s' % (
                    PrivKey, 'ECDSAkey_pri_temp-1.der')
                os.system(cmd)
                PrivKey = 'ECDSAkey_pri_temp-1.der'
            bdata = Fn.getFileBin(PrivKey)
            for i in range(7, 7 + 32):
                priv.append(bdata[i])  # 32 bytes
            return (bytes(priv))
        else:
            raise Util.KeyTypeError(
                'Key Type Unknown, should be ECDH NIST P-256.')

    except BaseException as e:
        Fn.OutputString(0, 'Getting Private key failed')
        Fn.OutputString(1, 'Error in %s : %s' %
                        (OpenSSL_GetPrivKey.__name__, e))
        logging.error('Error in %s : %s' % (OpenSSL_GetPrivKey.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        Fn.DeleteFile('ECDSAkey_pri_temp-1.der')
        os.chdir(Path_Temp)


def OpenSSL_ExportPubKey(PrivKey, keytype, keyname):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        if(keytype == Util.Key_RSA):
            cmd = 'openssl pkey -pubout -in %s -out %s' % (PrivKey, keyname)
            os.system(cmd)
            return keyname
        if(keytype == Util.EC256):
            cmd = 'openssl pkey -pubout -in %s -out %s' % (PrivKey, keyname)
            os.system(cmd)
            return keyname
    except BaseException as e:
        Fn.OutputString(0, 'Export PubKey failed')
        logging.error('Error in %s : %s' % (OpenSSL_ExportPubKey.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        os.chdir(Path_Temp)


def OpenSSL_GenKey(keytype):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        if(keytype == Util.EC256):
            cmd = 'openssl ecparam -genkey -name prime256v1 -noout -out ECDH256_AES_pri.pem'
            os.system(cmd)
            cmd = 'openssl pkey -pubout -in ECDH256_AES_pri.pem -out ECDH256_AES_pub.pem'
            os.system(cmd)
            return 'ECDH256_AES_pub.pem'
    except BaseException as e:
        Fn.OutputString(0, 'Gen PrivKey failed')
        logging.error('Error in %s : %s' % (OpenSSL_GenKey.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        os.chdir(Path_Temp)


def OpenSSL_GenAESKey(privkey, pubkey, delete=0):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        PubKeyFormat, PubKeyType, LongKey, key3072_384 = IdentifyPubKey(pubkey)
        if(PubKeyFormat == Util.KeyFormat.DER):
            Format = 'DER'
        else:
            Format = 'PEM'
        cmd = 'openssl pkeyutl -derive -inkey %s -peerform %s -peerkey %s -out _SS' % (
            privkey, Format, pubkey)
        os.system(cmd)
        return '_SS'

    except BaseException as e:
        Fn.OutputString(0, 'Gen AES key failed')
        logging.error('Error in %s : %s' % (OpenSSL_GenAESKey.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        if(delete):
            Fn.DeleteFile(privkey)
        os.chdir(Path_Temp)


def OpenSSL_AES_Encrypt(type, FileName, FileIV, FileKey, FileAdd, outputname, length):
    os.chdir(Util.Path_Input)
    iv = Fn.getFileBin(FileIV)[0:16]
    key = Fn.getFileBin(FileKey)
    # Fn.DeleteFile(FileIV)
    # Fn.DeleteFile(FileKey)
    siv = ""
    skey = ""

    for i in range(0, len(iv)):
        if(hex(iv[i])[3:4] == ''):
            siv = siv + '0' + hex(iv[i])[2:3]
        else:
            siv = siv + hex(iv[i])[2:3] + hex(iv[i])[3:4]
    for i in range(0, len(key)):
        if(hex(key[i])[3:4] == ''):
            skey = skey + '0' + hex(key[i])[2:3]
        else:
            skey = skey + hex(key[i])[2:3] + hex(key[i])[3:4]
    try:
        # print(siv)
        # print(skey)
        if(type == 1):
            # AES-ECB
            if(length % 16 == 0):
                cmd = 'openssl aes-256-cbc -nopad -iv %s -K %s -in %s -out %s' % (
                    siv, skey, FileName, outputname)
            else:
                cmd = 'openssl aes-256-cbc -iv %s -K %s -in %s -out %s' % (
                    siv, skey, FileName, outputname)
            os.system(cmd)

            # use Cryptodome lib
            # src_dat = Fn.getFileBin(FileName)
            # cryptor = AES.new(key, AES.MODE_CBC, iv)
            # enc_dat = cryptor.encrypt(src_dat)
            # enc_dat = bytearray(enc_dat)
            # Fn.GenBinFilefromBytes(enc_dat, 'enc_dat')

            return outputname

        elif(type == 2):
            # AES-GCM
            # use Cryptodome lib
            # enc_final = []
            # nonce = bytes(12)
            iv_gcm = Fn.getFileBin(FileIV)[0:32]
            #add = iv_gcm
            add = Fn.getFileBin(FileAdd)[0:32]
            src_dat = Fn.getFileBin(FileName)
            cryptor = AES.new(key, AES.MODE_GCM, iv_gcm)
            cryptor.update(add)   # no needs update
            enc_dat, tag = cryptor.encrypt_and_digest(src_dat)

            Fn.GenBinFilefromBytes(tag, "".join((outputname, "_tag")))

            # no needs to check tag
            # enc_final.append(enc_dat)
            # enc_final.append(tag)
            # enc_dat = bytearray(enc_dat)
            # Fn.GenBinFilefromList(enc_final, outputname)

            #Fn.GenBinFilefromBytes(tag, 'gcm_tag')
            #Fn.GenBinFilefromBytes(enc_dat, 'gcm_lib')
            Fn.GenBinFilefromBytes(enc_dat, outputname)

            # # use libressl
            # cmd = 'libressl enc -aes-256-gcm -iv %s -K %s -in %s -out %s' % (
            #     siv, skey, FileName, outputname)
            # os.system(cmd)
            # Fn.GenBinFilefromBytes(enc_dat, outputname)

        return outputname

    except BaseException as e:
        Fn.OutputString(0, 'Gen AES Encrypt failed')
        logging.error('Error in %s : %s' % (OpenSSL_AES_Encrypt.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        pass


def OpenSSL_GetAESKey(AESKey):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    bdata = Fn.getFileBin(AESKey)
    os.chdir(Path_Temp)
    return (bytes(bdata))


def CheckKeyPair(privkey, pubkey):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    try:
        keyname = 'FileTemp'
        keyname2 = 'FileTemp2'
        PrivFormat, PrivType, PrivLongkey = IdentifyPrivKey(privkey)
        PubFormat, PubType, PubLongkey, key3072_384 = IdentifyPubKey(pubkey)
        if(PrivType != PubType):
            raise Util.KeyTypeError('Key algorithm not match')

        # Transform format to Binary for key pair check
        if(PrivFormat == Util.KeyFormat.PEM):
            cmd = 'openssl pkey -pubout -in %s -out %s' % (privkey, keyname2)
            os.system(cmd)
            cmd = 'openssl pkey -pubin -inform PEM -outform DER -in %s -out %s' % (
                keyname2, keyname)
        else:
            cmd = 'openssl pkey -inform DER -pubout -in %s -out %s' % (
                privkey, keyname)
        os.system(cmd)

        hashPubKeyA = Fn.ByteArray2HexString(
            OpenSSL_Digest(Util.Path_Key, keyname, 256))

        if(PubFormat == Util.KeyFormat.PEM):
            cmd = 'openssl pkey -pubin -inform PEM -outform DER -in %s -out %s' % (
                pubkey, keyname)
        else:
            keyname = pubkey
            #cmd = 'openssl pkey -pubin -inform DER -outform PEM -in %s -out %s' % (pubkey, keyname)

        os.system(cmd)

        hashPubKeyB = Fn.ByteArray2HexString(
            OpenSSL_Digest(Util.Path_Key, keyname, 256))

        if(hashPubKeyA != hashPubKeyB):
            raise Util.KeyTypeError('Check key pair failed')

    except BaseException as e:
        Fn.OutputString(1, 'Error in %s : %s' % (CheckKeyPair.__name__, e))
        logging.error('Error in %s : %s' % (CheckKeyPair.__name__, e))
        Fn.ClearTmpFiles()
    finally:
        Fn.DeleteFile('FileTemp')
        Fn.DeleteFile('FileTemp2')
        os.chdir(Path_Temp)


def IdentifyPrivKey(Key):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    MyFormat = Util.KeyFormat.PEM
    MyType = Util.KeyType.RSA
    LongKey = False
    bdata = Fn.getFileBin(Key)
    for i in range(0, 5):
        if (bdata[i] != 45):  # '-' = 2Dh
            MyFormat = Util.KeyFormat.DER
    if(MyFormat == Util.KeyFormat.DER):
        cmd = 'openssl pkey -inform DER -outform PEM -in %s -out %s' % (
            Key, 'FileTemp')
        os.system(cmd)
        cmd = 'openssl pkey -in %s -text -noout' % 'FileTemp'
        data = os.popen(cmd).read()
    elif(MyFormat == Util.KeyFormat.PEM):
        cmd = 'openssl pkey -in %s -text -noout' % Key
        data = os.popen(cmd).read()
    if(data.find('ASN1 OID') != -1):
        MyType = Util.KeyType.ECDSA

    if(MyType == Util.KeyType.ECDSA):
        if(data.find('521') != -1):
            LongKey = True
    elif(MyType == Util.KeyType.RSA):
        if(data.find('4096') != -1):
            LongKey = True
    os.chdir(Path_Temp)
    return MyFormat, MyType, LongKey


def IdentifyPubKey(Key):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    MyFormat = Util.KeyFormat.PEM
    MyType = Util.KeyType.RSA
    LongKey = False
    key3072_384 = False
    bdata = Fn.getFileBin(Key)
    for i in range(0, 5):
        if (bdata[i] != 45):  # '-' = 2Dh
            MyFormat = Util.KeyFormat.DER
    if(MyFormat == Util.KeyFormat.DER):
        cmd = 'openssl pkey -inform DER -pubin -in %s -text -noout' % Key
        data = os.popen(cmd).read()
    elif(MyFormat == Util.KeyFormat.PEM):
        cmd = 'openssl pkey -pubin -in %s -text -noout' % Key
        data = os.popen(cmd).read()
    if(data.find('ASN1 OID') != -1):
        MyType = Util.KeyType.ECDSA
    if(MyType == Util.KeyType.ECDSA):
        if(data.find('521') != -1):
            LongKey = True
        elif(data.find('384') != -1):
            LongKey = True
            key3072_384 = True
    elif(MyType == Util.KeyType.RSA):
        if(data.find('4096') != -1):
            LongKey = True
        if(data.find('3072') != -1):
            LongKey = True
            key3072_384 = True
    os.chdir(Path_Temp)
    return MyFormat, MyType, LongKey, key3072_384


def IdentifyPubKey_2(Key):
    Path_Temp = os.getcwd()
    os.chdir(Util.Path_Key)
    MyFormat = Util.KeyFormat.PEM
    MyType = Util.KeyType.RSA
    LongKey = False
    key3072_384 = False
    bdata = Fn.getFileBin(Key)
    for i in range(0, 5):
        if (bdata[i] != 45):  # '-' = 2Dh
            MyFormat = Util.KeyFormat.DER
    if(MyFormat == Util.KeyFormat.DER):
        cmd = 'openssl pkey -inform DER -pubin -in %s -text -noout' % Key
        data = os.popen(cmd).read()
    elif(MyFormat == Util.KeyFormat.PEM):
        cmd = 'openssl pkey -pubin -in %s -text -noout' % Key
        data = os.popen(cmd).read()
    if(data.find('ASN1 OID') != -1):
        MyType = Util.KeyType.ECDSA
    if(MyType == Util.KeyType.ECDSA):
        if(data.find('521') != -1):
            LongKey = True
        elif(data.find('384') != -1):
            LongKey = True
            key3072_384 = True
    elif(MyType == Util.KeyType.RSA):
        if(data.find('4096') != -1):
            LongKey = True
        if(data.find('3072') != -1):
            LongKey = True
            key3072_384 = True
    os.chdir(Path_Temp)
    return MyType, LongKey, key3072_384


def GetRS(ba, LongKey, b3072_384):
    if(not LongKey):
        length1 = ba[3]
        if(length1 == 32):
            r = ba[4: 4 + 32]
        elif(length1 == 33):
            r = ba[5: 5 + 32]

        length2 = ba[4 + length1 + 1]
        if(length2 == 32):
            s = ba[4 + length1 + 2:]
        elif(length2 == 33):
            s = ba[4 + length1 + 2 + 1:]
    if(LongKey and b3072_384):
        length1 = ba[3]
        if(length1 == 48):
            r = ba[4: 4 + 48]
        elif(length1 == 49):
            r = ba[5: 5 + 48]

        length2 = ba[4 + length1 + 1]
        if(length2 == 48):
            s = ba[4 + length1 + 2:]
        elif(length2 == 49):
            s = ba[4 + length1 + 2 + 1:]
    if(LongKey and not b3072_384):
        length1 = ba[4]
        if(length1 == 65):
            r = bytearray(b'\x00' + ba[5: 5 + 65])		# fill to 66 bytes
        elif(length1 == 66):
            r = ba[5: 6 + 65]

        length2 = ba[5 + length1 + 1]
        if(length2 == 65):
            s = bytearray(b'\x00' + ba[5 + length1 + 2:])  # fill to 66 bytes
        elif(length2 == 66):
            s = ba[5 + length1 + 2:]

        if(len(r) + len(s) != 132):  # 66*2
            Fn.OutputString(0, 'rs pair length error')
    return r, s
