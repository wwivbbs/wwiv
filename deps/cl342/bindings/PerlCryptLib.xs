/*******************************************************************************

 Perl extesione interface (XS) to 'cryptlib' library (PerlCryptLib)

 Copyright (C) 2006-2010 Alvaro Livraghi. All Rights Reserved.
 Alvaro Livraghi, <perlcryptlib@gmail.com>

*******************************************************************************/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include <stdlib.h>

#include "ppport.h"

#include CRYPTLIB_H

#include "const-c.inc"

MODULE = PerlCryptLib		PACKAGE = PerlCryptLib		

INCLUDE: const-xs.inc

################################################################################
#                                                                              #
# Funzioni di utilita' generale                                                #
#                                                                              #
################################################################################

#
# Simula (per Perl) la seguente operazione su un generico buffer:
#
#      void * buffer = "Questo e' un buffer";
#      buffer += offset;
#
void * shift_buffer(buffer, length, offset)
	void * buffer;
	int length;
	const int offset;
	INIT:
		void * __buffer;
	CODE:
		__buffer = (void *)malloc(length);
		if ( __buffer != 0 ) {
			length -= offset;
			buffer += offset;
			memcpy(__buffer, buffer, length);
			sv_setpvn(ST(0), __buffer, length);
			RETVAL = newSVpvn(__buffer, length);
			free(__buffer);
		}
	OUTPUT:
		length




################################################################################
#                                                                              #
# Trascodifica funzioni di cryptlib                                            #
#                                                                              #
################################################################################


int cryptInit()


int cryptEnd()


int cryptLogin(user, name, password)
	int user;
	const char * name;
	const char * password;
	CODE:
		RETVAL = cryptLogin(&user, name, password);
	OUTPUT:
		RETVAL
		user


int cryptLogout(user)
	const int user;


int cryptCreateEnvelope(cryptEnvelope, cryptUser, formatType)
	int cryptEnvelope;
	const int cryptUser;
	const int formatType;
	CODE:
		RETVAL = cryptCreateEnvelope(&cryptEnvelope, cryptUser, formatType);
	OUTPUT:
		RETVAL
		cryptEnvelope


int cryptDestroyEnvelope(cryptEnvelope)
	const int cryptEnvelope;


int cryptSetAttribute(cryptEnvelope, attributeType, value)
	const int cryptEnvelope;
	const int attributeType;
	const int value;


int cryptSetAttributeString(cryptEnvelope, attributeType, value, valueLength)
	const int cryptEnvelope;
	const int attributeType;
	const void * value;
	const int valueLength;


int cryptGetAttribute(cryptObject, attributeType, value)
	const int cryptObject;
	const int attributeType;
	int value;
	CODE:
		RETVAL = cryptGetAttribute(cryptObject, attributeType, &value);
	OUTPUT:
		RETVAL
		value


int cryptGetAttributeString(cryptObject, attributeType, value, valueLength)
	const int cryptObject;
	const int attributeType;
	void * value = (SvIOK(ST(2)) ? (void *)SvIV(ST(2)) : (void *)SvPV_nolen(ST(2)));
	int valueLength;
	CODE:
		RETVAL = cryptGetAttributeString(cryptObject, attributeType, value, &valueLength);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(2), value, valueLength);
	OUTPUT:
		RETVAL
		valueLength


int cryptPushData(cryptEnvelope, buffer, length, bytesCopied)
	const int cryptEnvelope;
	const void * buffer = (SvIOK(ST(1)) ? (const void *)SvIV(ST(1)) : (const void *)SvPV_nolen(ST(1)));
	const int length;
	int bytesCopied;
	CODE:
		RETVAL = cryptPushData(cryptEnvelope, buffer, length, &bytesCopied);
	OUTPUT:
		RETVAL
		bytesCopied


int cryptFlushData(cryptEnvelope)
	const int cryptEnvelope;


int cryptPopData(cryptEnvelope, buffer, length, bytesCopied)
	const int cryptEnvelope;
	void * buffer;
	const int length;
	int bytesCopied;
	CODE:
		RETVAL = cryptPopData(cryptEnvelope, buffer, length, &bytesCopied);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(1), buffer, bytesCopied);
	OUTPUT:
		RETVAL
		bytesCopied


int cryptCreateContext(cryptContext, cryptUser, cryptAlgo)
	int cryptContext;
	const int cryptUser;
	const int cryptAlgo;
	CODE:
		RETVAL = cryptCreateContext(&cryptContext, cryptUser, cryptAlgo);
	OUTPUT:
		RETVAL
		cryptContext


int cryptDestroyContext(cryptContext)
	const int cryptContext;


int cryptKeysetOpen(keyset, cryptUser, keysetType, name, options)
	int keyset;
	const int cryptUser;
	const int keysetType;
	const char * name;
	const int options;
	CODE:
		RETVAL = cryptKeysetOpen(&keyset, cryptUser, keysetType, name, options);
	OUTPUT:
		RETVAL
		keyset


int cryptKeysetClose(keyset)
	const int keyset


int cryptGenerateKey(cryptContext)
	const int cryptContext


int cryptExportKey(encryptedKey, encryptedKeyMaxLength, encryptedKeyLength, exportKey, sessionKeyContext)
	void * encryptedKey = (SvIOK(ST(0)) ? (void *)SvIV(ST(0)) : (void *)SvPV_nolen(ST(0)));
	const int encryptedKeyMaxLength;
	int encryptedKeyLength;
	const int exportKey;
	const int sessionKeyContext;
	CODE:
		RETVAL = cryptExportKey(encryptedKey, encryptedKeyMaxLength, &encryptedKeyLength, exportKey, sessionKeyContext);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(0), encryptedKey, encryptedKeyLength);
	OUTPUT:
		RETVAL
		encryptedKeyLength


int cryptCreateCert(cryptCert, cryptUser, certType)
	int cryptCert;
	const int cryptUser;
	const int certType;
	CODE:
		RETVAL = cryptCreateCert(&cryptCert, cryptUser, certType);
	OUTPUT:
		RETVAL
		cryptCert


int cryptSignCert(certificate, signContext)
	const int certificate;
	const int signContext;


int cryptImportCert(certObject, certObjectLength, cryptUser, certificate)
	const void * certObject;
	const int certObjectLength;
	const int cryptUser;
	int certificate;
	CODE:
		RETVAL = cryptImportCert(certObject, certObjectLength, cryptUser, &certificate);
	OUTPUT:
		RETVAL
		certificate


int cryptExportCert(certObject, certObjectMaxLength, certObjectLength, certFormatType, certificate)
	void * certObject = (SvIOK(ST(0)) ? (void *)SvIV(ST(0)) : (void *)SvPV_nolen(ST(0)));
	const int certObjectMaxLength;
	int certObjectLength;
	const int certFormatType;
	const int certificate;
	CODE:
		RETVAL = cryptExportCert(certObject, certObjectMaxLength, &certObjectLength, certFormatType, certificate);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(0), certObject, certObjectLength);
	OUTPUT:
		RETVAL
		certObjectLength


int cryptCheckCert(certRequest, cryptCA)
	const int certRequest;
	const int cryptCA;


int cryptDestroyCert(cryptCert)
	const int cryptCert;


int cryptImportKey(encryptedKey, encryptedKeyLength, importContext, sessionKeyContext)
	const void * encryptedKey;
	const int encryptedKeyLength;
	const int importContext;
	const int sessionKeyContext;


int cryptImportKeyEx(encryptedKey, encryptedKeyLength, importContext, sessionKeyContext, returnedContext)
	const void * encryptedKey;
	const int encryptedKeyLength;
	const int importContext;
	const int sessionKeyContext;
	int returnedContext;
	CODE:
		RETVAL = cryptImportKeyEx(encryptedKey, encryptedKeyLength, importContext, sessionKeyContext, &returnedContext);
	OUTPUT:
		RETVAL
		returnedContext


int cryptAddPublicKey(keyset, certificate)
	const int keyset;
	const int certificate;


int cryptAddPrivateKey(keyset, cryptKey, password)
	const int keyset;
	const int cryptKey;
	const char * password;


int cryptGetPrivateKey(cryptHandle, cryptContext, keyIDtype, keyID, password)
	const int cryptHandle;
	int cryptContext;
	const int keyIDtype;
	const void * keyID = (SvIOK(ST(3)) ? (const void *)SvIV(ST(3)) : (const void *)SvPV_nolen(ST(3)));
	const char * password = (SvIOK(ST(4)) ? (const char *)SvIV(ST(4)) : (const char *)SvPV_nolen(ST(4)));
	CODE:
		RETVAL = cryptGetPrivateKey(cryptHandle, &cryptContext, keyIDtype, keyID, password);
	OUTPUT:
		RETVAL
		cryptContext


int cryptGetPublicKey(cryptObject, publicKey, keyIDtype, keyID)
	const int cryptObject;
	int publicKey;
	const int keyIDtype;
	const void * keyID = (SvIOK(ST(3)) ? (const void *)SvIV(ST(3)) : (const void *)SvPV_nolen(ST(3))); /* Consente di passare 0 come NULL */
	CODE:
		RETVAL = cryptGetPublicKey(cryptObject, &publicKey, keyIDtype, keyID);
	OUTPUT:
		RETVAL
		publicKey


int cryptAddCertExtension(certificate, oid, criticalFlag, extension, extensionLength)
	const int certificate;
	const char * oid;
	const int criticalFlag;
	const void * extension;
	const int extensionLength;


int cryptAddRandom(randomData, randomDataLength)
	const void * randomData = (SvIOK(ST(0)) ? (const void *)SvIV(ST(0)) : (const void *)SvPV_nolen(ST(0)));
	const int randomDataLength;


int cryptCAAddItem(keyset, certificate)
	const int keyset;
	const int certificate;


int cryptCADeleteItem(keyset, certType, keyIDtype, keyID)
	const int keyset;
	const int certType;
	const int keyIDtype;
	const void * keyID;


int cryptCACertManagement(cryptCert, action, keyset, caKey, certRequest)
	int cryptCert;
	const int action;
	const int keyset;
	const int caKey;
	const int certRequest;
	CODE:
		RETVAL = cryptCACertManagement(&cryptCert, action, keyset, caKey, certRequest);
	OUTPUT:
		RETVAL
		cryptCert


int cryptCAGetItem(keyset, certificate, certType, keyIDtype, keyID)
	const int keyset;
	int certificate;
	const int certType;
	const int keyIDtype;
	const void * keyID = (SvIOK(ST(4)) ? (const void *)SvIV(ST(4)) : (const void *)SvPV_nolen(ST(4)));
	CODE:
		RETVAL = cryptCAGetItem(keyset, &certificate, certType, keyIDtype, keyID);
	OUTPUT:
		RETVAL
		certificate


int cryptCheckSignature(signature, signatureLength, sigCheckKey, hashContext)
	const void * signature;
	const int signatureLength;
	const int sigCheckKey;
	const int hashContext;


int cryptCheckSignatureEx(signature, signatureLength, sigCheckKey, hashContext, extraData)
	const void * signature;
	const int signatureLength;
	const int sigCheckKey;
	const int hashContext;
	int extraData;
	CODE:
		RETVAL = cryptCheckSignatureEx(signature, signatureLength, sigCheckKey, hashContext, &extraData);
	OUTPUT:
		RETVAL
		extraData


int cryptCreateSession(cryptSession, cryptUser, sessionType)
	int cryptSession;
	const int cryptUser;
	const int sessionType;
	CODE:
		RETVAL = cryptCreateSession(&cryptSession, cryptUser, sessionType);
	OUTPUT:
		RETVAL
		cryptSession


int cryptCreateSignature(signature, signatureMaxLength, signatureLength, signContext, hashContext)
	void * signature = (SvIOK(ST(0)) ? (void *)SvIV(ST(0)) : (void *)SvPV_nolen(ST(0)));
	const int signatureMaxLength;
	int signatureLength;
	const int signContext;
	const int hashContext;
	CODE:
		RETVAL = cryptCreateSignature(signature, signatureMaxLength, &signatureLength, signContext, hashContext);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(0), signature, signatureLength);
	OUTPUT:
		RETVAL
		signatureLength


int cryptCreateSignatureEx(signature, signatureMaxLength, signatureLength, formatType, signContext, hashContext, extraData)
	void * signature = (SvIOK(ST(0)) ? (void *)SvIV(ST(0)) : (void *)SvPV_nolen(ST(0)));
	const int signatureMaxLength;
	int signatureLength;
	const int formatType;
	const int signContext;
	const int hashContext;
	const int extraData;
	CODE:
		RETVAL = cryptCreateSignatureEx(signature, signatureMaxLength, &signatureLength, formatType, signContext, hashContext, extraData);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(0), signature, signatureLength);
	OUTPUT:
		RETVAL
		signatureLength


int cryptDecrypt(cryptContext, buffer, length)
	const int cryptContext;
	void * buffer;
	const int length;
	CODE:
		RETVAL = cryptDecrypt(cryptContext, buffer, length);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(1), buffer, length);
	OUTPUT:
		RETVAL


int cryptDeleteAttribute(cryptObject, attributeType)
	const int cryptObject;
	const int attributeType;


int cryptDeleteCertExtension(certificate, oid)
	const int certificate;
	const char * oid;


int cryptDeleteKey(cryptObject, keyIDtype, keyID)
	const int cryptObject;
	const int keyIDtype;
	const void * keyID;


int cryptDestroyObject(cryptObject)
	const int cryptObject;


int cryptDestroySession(cryptSession)
	const int cryptSession;


int cryptDeviceClose(device)
	const int device;


int cryptDeviceCreateContext(cryptDevice, cryptContext, cryptAlgo)
	const int cryptDevice;
	int cryptContext;
	const int cryptAlgo;
	CODE:
		RETVAL = cryptDeviceCreateContext(cryptDevice, &cryptContext, cryptAlgo);
	OUTPUT:
		RETVAL
		cryptContext


int cryptDeviceOpen(device, cryptUser, deviceType, name)
	int device;
	const int cryptUser;
	const int deviceType;
	const char * name = (SvIOK(ST(0)) ? (const char *)SvIV(ST(0)) : (const char *)SvPV_nolen(ST(0)));
	CODE:
		RETVAL = cryptDeviceOpen(&device, cryptUser, deviceType, name);
	OUTPUT:
		RETVAL
		device


int cryptDeviceQueryCapability(cryptDevice, cryptAlgo, cryptQueryInfo)
	const int cryptDevice;
	const int cryptAlgo;
	HV * cryptQueryInfo;
	INIT:
		CRYPT_QUERY_INFO dummy;
	CODE:
		RETVAL = cryptDeviceQueryCapability(cryptDevice, cryptAlgo, &dummy);
		if ( RETVAL == CRYPT_OK ) {
			hv_store(cryptQueryInfo, "algoName",    8, newSVpv(dummy.algoName, strlen(dummy.algoName)), 0);
			hv_store(cryptQueryInfo, "blockSize",   9, newSVnv(dummy.blockSize), 0);
			hv_store(cryptQueryInfo, "minKeySize", 10, newSVnv(dummy.minKeySize), 0);
			hv_store(cryptQueryInfo, "keySize",     7, newSVnv(dummy.keySize), 0);
			hv_store(cryptQueryInfo, "maxKeySize", 10, newSVnv(dummy.maxKeySize), 0);
		}
	OUTPUT:
		RETVAL


int cryptEncrypt(cryptContext, buffer, length)
	const int cryptContext;
	void * buffer;
	const int length;
	CODE:
		RETVAL = cryptEncrypt(cryptContext, buffer, length);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(1), buffer, length);
	OUTPUT:
		RETVAL


int cryptExportKeyEx(encryptedKey, encryptedKeyMaxLength, encryptedKeyLength, formatType, exportKey, sessionKeyContext)
	void * encryptedKey = (SvIOK(ST(0)) ? (void *)SvIV(ST(0)) : (void *)SvPV_nolen(ST(0)));
	const int encryptedKeyMaxLength;
	int encryptedKeyLength;
	const int formatType;
	const int exportKey;
	const int sessionKeyContext;
	CODE:
		RETVAL = cryptExportKeyEx(encryptedKey, encryptedKeyMaxLength, &encryptedKeyLength, formatType, exportKey, sessionKeyContext);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(0), encryptedKey, encryptedKeyLength);
	OUTPUT:
		RETVAL
		encryptedKeyLength


int cryptGetCertExtension(certificate, oid, criticalFlag, extension, extensionMaxLength, extensionLength)
	const int certificate;
	const char * oid;
	int criticalFlag;
	void * extension;
	const int extensionMaxLength;
	int extensionLength;
	CODE:
		RETVAL = cryptGetCertExtension(certificate, oid, &criticalFlag, extension, extensionMaxLength, &extensionLength);
		if ( RETVAL == CRYPT_OK ) sv_setpvn(ST(3), extension, extensionLength);
	OUTPUT:
		RETVAL
		criticalFlag
		extensionLength


int cryptQueryCapability(cryptAlgo, cryptQueryInfo)
	const int cryptAlgo;
	HV * cryptQueryInfo;
	INIT:
		CRYPT_QUERY_INFO dummy;
	CODE:
		RETVAL = cryptQueryCapability(cryptAlgo, &dummy);
		if ( RETVAL == CRYPT_OK ) {
			hv_store(cryptQueryInfo, "algoName",    8, newSVpv(dummy.algoName, strlen(dummy.algoName)), 0);
			hv_store(cryptQueryInfo, "blockSize",   9, newSVnv(dummy.blockSize), 0);
			hv_store(cryptQueryInfo, "minKeySize", 10, newSVnv(dummy.minKeySize), 0);
			hv_store(cryptQueryInfo, "keySize",     7, newSVnv(dummy.keySize), 0);
			hv_store(cryptQueryInfo, "maxKeySize", 10, newSVnv(dummy.maxKeySize), 0);
		}
	OUTPUT:
		RETVAL


int cryptQueryObject(objectData, objectDataLength, cryptObjectInfo)
	const void * objectData;
	const int objectDataLength;
	HV * cryptObjectInfo;
	INIT:
		CRYPT_OBJECT_INFO dummy;
	CODE:
		RETVAL = cryptQueryObject(objectData, objectDataLength, &dummy);
		if ( RETVAL == CRYPT_OK ) {
			hv_store(cryptObjectInfo, "objectType", 10, newSVnv(dummy.objectType), 0);
			hv_store(cryptObjectInfo, "cryptAlgo",   9, newSVnv(dummy.cryptAlgo), 0);
			hv_store(cryptObjectInfo, "cryptMode",   9, newSVnv(dummy.cryptMode), 0);
			hv_store(cryptObjectInfo, "hashAlgo",    8, newSVnv(dummy.hashAlgo), 0);
			hv_store(cryptObjectInfo, "salt",        4, newSVpv(dummy.salt, dummy.saltSize), 0);
			hv_store(cryptObjectInfo, "saltSize",    8, newSVnv(dummy.saltSize), 0);
		}
	OUTPUT:
		RETVAL

# Add deprecated functions when CRYPTLIB_VERSION prior 3.4.0
#if CRYPTLIB_VERSION < 3400

int cryptAsyncCancel(cryptObject)
	const int cryptObject;


int cryptAsyncQuery(cryptObject)
	const int cryptObject;


int cryptGenerateKeyAsync(cryptContext)
	const int cryptContext;

#endif
