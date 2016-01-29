/****************************************************************************
*																			*
*							Read CMP Message Types							*
*						Copyright Peter Gutmann 1999-2011					*
*																			*
****************************************************************************/

#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

#ifdef USE_CMP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if 0	/* 12/6/09 Due to a bug in the buffer-positioning the following code 
				   hasn't actually worked since 3.2.1 in 2005, since this 
				   hasn't caused any complaints we disable it for attack-
				   surface reduction */

/* Read a certificate encrypted with CMP's garbled reinvention of CMS 
   content:

	EncryptedCert ::= SEQUENCE {
		dummy1			[0]	... OPTIONAL,		-- Ignored
		cekAlg			[1]	AlgorithmIdentifier,-- CEK algorithm
		encCEK			[2]	BIT STRING,			-- Encrypted CEK
		dummy2			[3]	... OPTIONAL,		-- Ignored
		dummy3			[4] ... OPTIONAL,		-- Ignored
		encData			BIT STRING				-- Encrypted certificate
		} 

   This muddle is only applied for non-cryptlib sessions, if two cryptlib
   implementations are communicating then the certificate is wrapped using 
   CMS */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
static int readEncryptedDataInfo( INOUT STREAM *stream, 
								  OUT_BUFFER_ALLOC( *encDataLength ) \
										void **encDataPtrPtr, 
								  OUT_LENGTH_BOUNDED_Z( maxLength ) \
										int *encDataLength, 
								  IN_LENGTH_SHORT_MIN( 32 ) const int minLength,
								  IN_LENGTH_SHORT_MIN( 32 ) const int maxLength )
	{
	void *dataPtr;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( encDataPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( encDataLength, sizeof( int ) ) );

	REQUIRES( minLength >= 32 && minLength < maxLength && \
			  minLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*encDataPtrPtr = NULL;
	*encDataLength = 0;

	/* Read and remember the encrypted data */
	status = readBitStringHole( stream, &length, minLength, 
								CTAG_EV_ENCCEK );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_PKCSIZE || length > CRYPT_MAX_PKCSIZE )
		return( CRYPT_ERROR_BADDATA );
	status = sMemGetDataBlock( stream, &dataPtr, length );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, length );
	if( cryptStatusError( status ) )
		return( status );
	*encDataPtrPtr = dataPtr;
	*encDataLength = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int readEncryptedCert( INOUT STREAM *stream,
							  IN_HANDLE const CRYPT_CONTEXT iImportContext,
							  OUT_BUFFER( outDataMaxLength, *outDataLength ) \
									void *outData, 
							  IN_DATALENGTH_MIN( 16 ) const int outDataMaxLength,
							  OUT_DATALENGTH_Z int *outDataLength, 
							  INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CONTEXT iSessionKey;
	MECHANISM_WRAP_INFO mechanismInfo;
	QUERY_INFO queryInfo;
	void *encKeyPtr = DUMMY_INIT_PTR, *encCertPtr;
	int encKeyLength = DUMMY_INIT, encCertLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( isHandleRangeValid( iImportContext ) );

	/* Read the CEK algorithm identifier and encrypted CEK.  All of the
	   values are optional although there's no indication of why or what
	   you're supposed to do if they're not present (OTOH for others there's
	   no indication of what you're supposed to do when they're present
	   either) so we treat an absent required value as an error and ignore
	   the others */
	status = readSequence( stream, NULL );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( CTAG_EV_DUMMY1 ) )	/* Junk */
		status = readUniversal( stream );
	if( !cryptStatusError( status ) )			/* CEK algo */
		status = readContextAlgoID( stream, &iSessionKey, &queryInfo,
									CTAG_EV_CEKALGO );
	if( !cryptStatusError( status ) )			/* Enc.CEK */
		status = readEncryptedDataInfo( stream, &encKeyPtr, &encKeyLength, 
										MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo, 
				  "Invalid encrypted certificate CEK information" ) );
		}
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( CTAG_EV_DUMMY2 ) )
		status = readUniversal( stream );		/* Junk */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( CTAG_EV_DUMMY3 ) )
		status = readUniversal( stream );		/* Junk */
	if( !cryptStatusError( status ) )
		status = readEncryptedDataInfo( stream, &encCertPtr, &encCertLength,
										128, 8192 );
	if( !cryptStatusError( status ) &&			/* Enc.certificate */
		( queryInfo.cryptMode == CRYPT_MODE_ECB || \
		  queryInfo.cryptMode == CRYPT_MODE_CBC ) )
		{
		int blockSize;

		/* Make sure that the data length is valid.  Checking at this point
		   saves a lot of unnecessary processing and allows us to return a
		   more meaningful error code */
		status = krnlSendMessage( iSessionKey, IMESSAGE_GETATTRIBUTE, 
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) || \
			( queryInfo.size % blockSize ) != 0 )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKey, IMESSAGE_DECREFCOUNT );
		retExt( status,
				( status, errorInfo, 
				  "Invalid encrypted certificate data" ) );
		}

	/* Import the wrapped session key into the session key context */
	setMechanismWrapInfo( &mechanismInfo, encKeyPtr, encKeyLength,
						  NULL, 0, iSessionKey, iImportContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1 );
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iSessionKey, IMESSAGE_DECREFCOUNT );
		retExt( status,
				( status, errorInfo, 
				  "Couldn't decrypt encrypted certificate CEK" ) );
		}

	/* Decrypt the returned certificate and copy the result back to the
	   caller.  We don't worry about padding because the certificate-import
	   code knows when to stop based on the encoded certificate data */
	status = krnlSendMessage( iSessionKey, IMESSAGE_CTX_DECRYPT,
							  encCertPtr, encCertLength );
	krnlSendNotifier( iSessionKey, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo, 
				  "Couldn't decrypt returned encrypted certificate using "
				  "CEK" ) );
		}
	return( attributeCopyParams( outData, outDataMaxLength, outDataLength, 
								 encCertPtr, encCertLength ) );
	}
#endif /* 0 */

/* Process a request that's (supposedly) been authorised by an RA rather 
   than coming directly from a user */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static BOOLEAN processRARequest( INOUT CMP_PROTOCOL_INFO *protocolInfo,
								 IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
								 IN_ENUM_OPT( CMP_MESSAGE ) \
									const CMP_MESSAGE_TYPE messageType,
								 INOUT ERROR_INFO *errorInfo )
	{
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES_B( isHandleRangeValid( iCertRequest ) );
	REQUIRES_B( messageType >= CTAG_PB_IR && messageType < CTAG_PB_LAST );
				/* CTAG_PB_IR == 0 so this is the same as _NONE */

	/* If the user isn't an RA then this can't be an RA-authorised request */
	if( !protocolInfo->userIsRA )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo, 
				  "Request supposedly from an RA didn't come from an actual "
				  "RA user" ) );
		}

	/* An RA-authorised request can only be a CR.  They can't be an IR 
	   because they need to be signed, and they can't be a KUR or RR because 
	   we assume that users will be updating and revoking their own 
	   certificates, it doesn't make much sense to require an RA for this */
	if( messageType != CTAG_PB_CR )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo, 
				  "Request type %d supposedly from an RA is of the wrong "
				  "type, should be %d", messageType, CTAG_PB_CR ) );
		}

	/* It's an RA-authorised request, mark the request as such */
	return( krnlSendMessage( iCertRequest, IMESSAGE_SETATTRIBUTE, 
							 MESSAGE_VALUE_TRUE, 
							 CRYPT_IATTRIBUTE_REQFROMRA ) );
	}

/* Try and obtain more detailed information on why a certificate request
   wasn't compatible with stored PKI user information.  Note that the
   mapping table below is somewhat specific to the implementation of 
   copyPkiUserToCertReq() in certs/comp_pkiu.c, we hardcode a few common 
   cases here and use a generic error message for the rest */

#ifdef USE_ERRMSGS

typedef struct {
	const CRYPT_ATTRIBUTE_TYPE errorLocus;
	const CRYPT_ERRTYPE_TYPE errorType;
	const char *errorString;
	} EXT_ERROR_MAP_INFO;

static const EXT_ERROR_MAP_INFO extErrorMapTbl[] = {
	{ CRYPT_CERTINFO_SUBJECTNAME, CRYPT_ERRTYPE_ISSUERCONSTRAINT,
	  "Certificate request DN differs from PKI user DN" },
	{ CRYPT_CERTINFO_SUBJECTNAME, CRYPT_ERRTYPE_ATTR_ABSENT,
	  "Certificate request contains an empty DN" },
	{ CRYPT_CERTINFO_SUBJECTALTNAME, CRYPT_ERRTYPE_ISSUERCONSTRAINT,
	  "Certificate request subjectAltName conflicts with PKI user "
	  "subjectAltName" },
	{ CRYPT_CERTINFO_COMMONNAME, CRYPT_ERRTYPE_ATTR_ABSENT,
	  "Certificate request DN is missing a CommonName" },
	{ CRYPT_CERTINFO_COMMONNAME, CRYPT_ERRTYPE_ISSUERCONSTRAINT,
	  "Certificate request CommonName differs from PKI user CommonName" },
	{ CRYPT_ATTRIBUTE_NONE, CRYPT_ERRTYPE_NONE, NULL },
		{ CRYPT_ATTRIBUTE_NONE, CRYPT_ERRTYPE_NONE, NULL }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int reportExtendedCertErrorInfo( INOUT SESSION_INFO *sessionInfoPtr,
										IN_ERROR const int errorStatus )
	{
	const EXT_ERROR_MAP_INFO *extErrorInfoPtr = NULL;
	int errorType, errorLocus DUMMY_INIT, i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( cryptStatusError( errorStatus ) );

	/* Try and get further details on what went wrong */
	status = krnlSendMessage( sessionInfoPtr->iCertRequest, 
							  IMESSAGE_GETATTRIBUTE, &errorType,
							  CRYPT_ATTRIBUTE_ERRORTYPE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( sessionInfoPtr->iCertRequest, 
								  IMESSAGE_GETATTRIBUTE, &errorLocus,
								  CRYPT_ATTRIBUTE_ERRORLOCUS );
	if( cryptStatusError( status ) )
		{
		/* We couldn't get any further information, return a generic error
		   message */
		retExt( errorStatus,
				( errorStatus, SESSION_ERRINFO, 
				  "Information in certificate request can't be reconciled "
				  "with our information for the user (no further problem "
				  "details are available)" ) );
		}

	/* Provide a more detailed report on what went wrong.  We search the 
	   table by locus as the primary ID since these are more unique than the 
	   error type */
	for( i = 0; extErrorMapTbl[ i ].errorType != CRYPT_ERRTYPE_NONE && \
				i < FAILSAFE_ARRAYSIZE( extErrorMapTbl, EXT_ERROR_MAP_INFO );
		 i++ )
		{
		if( extErrorMapTbl[ i ].errorLocus == errorLocus && \
			extErrorMapTbl[ i ].errorType == errorType )
			{
			extErrorInfoPtr = &extErrorMapTbl[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( extErrorMapTbl, EXT_ERROR_MAP_INFO ) );
	if( extErrorInfoPtr == NULL )
		{
		/* It's no obviously recognisable problem, return a lower-level 
		   error message */
		retExt( errorStatus,
				( errorStatus, SESSION_ERRINFO, 
				  "Information in certificate request can't be reconciled "
				  "with our information for the user, error type %d, error "
				  "locus %d", errorType, errorLocus ) );
		}

	/* Return the actual error message.  Note the somewhat unusual means
	   of passing the string argument, some tools will warn of a problem
	   due to passing in a non-literal string if we pass the errorString
	   in directly (even though it is a literal string) so we have to use 
	   one level of indirection to bypass the warning */
	retExt( errorStatus,
			( errorStatus, SESSION_ERRINFO, 
			  "%s", extErrorInfoPtr->errorString ) );
	}
#else

#define reportExtendedCertErrorInfo( sessionInfoPtr, errorStatus ) \
		return( errorStatus );
#endif /* USE_ERRMSGS */

/****************************************************************************
*																			*
*								PKI Body Functions							*
*																			*
****************************************************************************/

/* Read request body:

	body			[n]	EXPLICIT SEQUENCE {	-- Processed by caller
						...					-- CRMF request
					} 

   The outer tag and SEQUENCE have already been processed by the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readRequestBody( INOUT STREAM *stream, 
							INOUT SESSION_INFO *sessionInfoPtr,
							INOUT CMP_PROTOCOL_INFO *protocolInfo,
							IN_ENUM_OPT( CMP_MESSAGE ) \
								const CMP_MESSAGE_TYPE messageType,
							IN_LENGTH_SHORT const int messageLength )
	{
	CMP_INFO *cmpInfo = sessionInfoPtr->sessionCMP;
	MESSAGE_DATA msgData;
	BYTE authCertID[ CRYPT_MAX_HASHSIZE + 8 ];
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( messageType >= CTAG_PB_IR && messageType < CTAG_PB_LAST );
			  /* CTAG_PB_IR == 0 so this is the same as _NONE */
	REQUIRES( messageLength > 0 && messageLength < MAX_INTLENGTH_SHORT );

	/* Import the CRMF request */
	status = importCertFromStream( stream,
								   &sessionInfoPtr->iCertRequest,
								   DEFAULTUSER_OBJECT_HANDLE,
								   ( messageType == CTAG_PB_P10CR ) ? \
									CRYPT_CERTTYPE_CERTREQUEST : \
								   ( messageType == CTAG_PB_RR ) ? \
									CRYPT_CERTTYPE_REQUEST_REVOCATION : \
									CRYPT_CERTTYPE_REQUEST_CERT,
								   messageLength, KEYMGMT_FLAG_NONE );
	if( cryptStatusError( status ) )
		{
		protocolInfo->pkiFailInfo = CMPFAILINFO_BADCERTTEMPLATE;
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid CRMF request" ) );
		}

	/* If it's a request type that can be self-signed (revocation requests 
	   are unsigned) and it's from an encryption-only key (that is, a key 
	   that's not capable of signing, indicated by the request not being 
	   self-signed) remember this so that we can peform special-case 
	   processing later on */
	if( messageType != CTAG_PB_RR )
		{
		status = krnlSendMessage( sessionInfoPtr->iCertRequest,
								  IMESSAGE_GETATTRIBUTE, &value,
								  CRYPT_CERTINFO_SELFSIGNED );
		if( cryptStatusError( status ) )
			return( status );
		if( !value )
			{
			/* If the request is for a signing key then having an unsigned 
			   request is an error */
			status = krnlSendMessage( sessionInfoPtr->iCertRequest,
									  IMESSAGE_GETATTRIBUTE, &value,
									  CRYPT_CERTINFO_KEYUSAGE );
			if( cryptStatusOK( status ) && \
				( value & ( KEYUSAGE_SIGN | KEYUSAGE_CA ) ) )
				{
				protocolInfo->pkiFailInfo = CMPFAILINFO_BADCERTTEMPLATE;
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
						  "CRMF request is for a signing key but the request "
						  "isn't signed" ) );
				}
			protocolInfo->cryptOnlyKey = TRUE;
			}
		}

	/* Record the identity of the PKI user (for a MAC'd request) or 
	   certificate (for a signed request) that authorised this request */
	setMessageData( &msgData, authCertID, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( protocolInfo->useMACreceive ? \
								cmpInfo->userInfo : \
								sessionInfoPtr->iAuthInContext,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( sessionInfoPtr->iCertRequest,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_AUTHCERTID );
	if( cryptStatusError( status ) )
		return( status );

	/* Revocation requests don't contain any information so there's nothing
	   further to check */
	if( messageType == CTAG_PB_RR )
		return( CRYPT_OK );

	/* Check whether this request is one that's been authorised by an RA
	   rather than coming directly from a user */
	status = krnlSendMessage( sessionInfoPtr->iCertRequest, 
							  IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_CERTINFO_KEYFEATURES );
	if( cryptStatusOK( status ) && ( value & KEYFEATURE_FLAG_RAISSUED ) )
		{
		status = processRARequest( protocolInfo, 
								   sessionInfoPtr->iCertRequest, 
								   messageType, SESSION_ERRINFO );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that the information in the request is consistent with the
	   user information template.  If it's an ir then the subject may not 
	   know their DN or may only know their CN, in which case they'll send 
	   an empty/CN-only subject DN in the hope that we can fill it in for 
	   them.  If it's not an ir then the user information acts as a filter 
	   to ensure that the request doesn't contain values that it shouldn't */
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_SETATTRIBUTE, &cmpInfo->userInfo,
							  CRYPT_IATTRIBUTE_PKIUSERINFO );
	if( cryptStatusError( status ) )
		{
		protocolInfo->pkiFailInfo = CMPFAILINFO_BADCERTTEMPLATE;

		/* See if we can get any additional information on what the problem 
		   was */
		return( reportExtendedCertErrorInfo( sessionInfoPtr, status ) );
		}
	return( CRYPT_OK );
	}

/* Read response body:

	body			[n] EXPLICIT SEQUENCE {	-- Processed by caller
		caPubs		[1] EXPLICIT SEQUENCE {...} OPTIONAL,-- Ignored
		response		SEQUENCE {
						SEQUENCE {
			certReqID	INTEGER (0),
			status		PKIStatusInfo,
			certKeyPair	SEQUENCE {			-- If status == 0 or 1
				cert[0]	EXPLICIT Certificate,
or				cmpEncCert					-- For encr-only key
					[1] EXPLICIT CMPEncryptedCert,
or				cmsEncCert					-- For encr-only key
					[2] EXPLICIT EncryptedCert,
						...					-- Ignored
					}
				}
			}
		}

   The outer tag and SEQUENCE have already been processed by the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readResponseBody( INOUT STREAM *stream, 
							 INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT CMP_PROTOCOL_INFO *protocolInfo,
							 STDC_UNUSED IN_ENUM_OPT( CMP_MESSAGE ) \
								const CMP_MESSAGE_TYPE messageType,
							 STDC_UNUSED IN_LENGTH_SHORT const int messageLength )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	void *bodyInfoPtr DUMMY_INIT_PTR;
	int bodyLength, tag, value, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Skip any noise before the payload if necessary */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 1 ) )
		{
		/* caPubs */
		status = readUniversal( stream );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a revocation response then the only returned data is the 
	   status value */
	if( protocolInfo->operation == CTAG_PB_RR )
		{
		readSequence( stream, NULL );		/* Inner wrapper */
		return( readPkiStatusInfo( stream, isServer( sessionInfoPtr ),
								   &sessionInfoPtr->errorInfo ) );
		}

	/* It's a certificate response, unwrap the body to find the certificate 
	   payload */
	readSequence( stream, NULL );			/* Inner wrapper */
	readSequence( stream, NULL );
	readUniversal( stream );				/* certReqId */
	status = readPkiStatusInfo( stream, isServer( sessionInfoPtr ),
								&sessionInfoPtr->errorInfo );
	if( cryptStatusError( status ) )
		return( status );
	readSequence( stream, NULL );			/* certKeyPair wrapper */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	tag = EXTRACT_CTAG( tag );
	status = readConstructed( stream, &bodyLength, tag );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( stream, &bodyInfoPtr, bodyLength );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( bodyInfoPtr != NULL );

	/* Process the returned certificate as required */
	switch( tag )
		{
		case CTAG_CK_CERT:
			/* Plaintext certificate, we're done */
			break;

#if 0	/* 12/6/09 See earlier comment */
		case CTAG_CK_ENCRYPTEDCERT:
			/* Certificate encrypted with CMP's garbled attempt at doing 
			   CMS, try and decrypt it */
			status = readEncryptedCert( stream, sessionInfoPtr->privateKey,
										SESSION_ERRINFO );
			break;
#endif /* 0 */

		case CTAG_CK_NEWENCRYPTEDCERT:
			{
			ERROR_INFO errorInfo;

			/* Certificate encrypted with CMS, unwrap it.  Note that this 
			   relies on the fact that cryptlib generates the 
			   subjectKeyIdentifier that's used to identify the decryption 
			   key by hashing the subjectPublicKeyInfo, this is needed 
			   because when the newly-issued certificate is received only 
			   the keyID is available (since the certificate hasn't been 
			   decrypted and read yet) while the returned certificate uses 
			   the sKID to identify the decryption key.  If the keyID and
			   sKID aren't the same then the envelope-unwrapping code will
			   report a CRYPT_ERROR_WRONGKEY */
			status = envelopeUnwrap( bodyInfoPtr, bodyLength,
									 bodyInfoPtr, bodyLength, &bodyLength,
									 sessionInfoPtr->privateKey, &errorInfo );
			if( cryptStatusError( status ) )
				{
				retExtErr( cryptArgError( status ) ? \
						   CRYPT_ERROR_FAILED : status,
						   ( cryptArgError( status ) ? \
							 CRYPT_ERROR_FAILED : status, SESSION_ERRINFO, 
							 &errorInfo, 
							 "Couldn't decrypt CMS enveloped certificate: " ) );
				}
			break;
			}

		default:
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Unknown returned certificate encapsulation type %d",
					  tag ) );
		}
	ENSURES( cryptStatusOK( status ) );
		/* All error paths have already been checked above */

	/* Import the certificate as a cryptlib object */
	setMessageCreateObjectIndirectInfo( &createInfo, bodyInfoPtr, bodyLength,
										CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT, &createInfo,
							  OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid returned certificate" ) );
		}
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;

	/* In order to acknowledge receipt of this message we have to return at a
	   later point a hash of the certificate carried in this message created 
	   using the hash algorithm used in the certificate signature.  This 
	   makes the CMP-level transport layer dependant on the certificate 
	   format it's carrying (so the code will repeatedly break every time a 
	   new certificate hash algorithm or certificate format is added), but 
	   that's what the standard requires */
	status = krnlSendMessage( sessionInfoPtr->iCertResponse,
							  IMESSAGE_GETATTRIBUTE, &value,
							  CRYPT_IATTRIBUTE_CERTHASHALGO );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't extract confirmation hash type from returned "
				  "certificate" ) );
		}
	if( ( value != CRYPT_ALGO_MD5 && value != CRYPT_ALGO_SHA1 && \
		  value != CRYPT_ALGO_SHA2 && value != CRYPT_ALGO_SHAng ) || \
		!algoAvailable( value ) )
		{
		/* Certificates can only provide fingerprints using a subset of
		   available hash algorithms */
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
				  "Can't confirm certificate issue using hash algorithm %d",
				  value ) );
		}
	protocolInfo->confHashAlgo = value;

	return( CRYPT_OK );
	}

/* Read conf body:

	body		   [19]	EXPLICIT SEQUENCE {	-- Processed by caller
						SEQUENCE {
		certHash		OCTET STRING
		certReqID		INTEGER (0),
			}
		}

   The outer tag and SEQUENCE have already been processed by the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readConfBody( INOUT STREAM *stream, 
						 INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT CMP_PROTOCOL_INFO *protocolInfo,
						 IN_ENUM_OPT( CMP_MESSAGE ) \
							const CMP_MESSAGE_TYPE messageType,
						 IN_LENGTH_SHORT const int messageLength )
	{
	static const MAP_TABLE hashMapTable[] = {
		/* We're the server so we control the hash algorithm that'll be 
		   used, which means that it'll always be one of the following */
		{ CRYPT_ALGO_SHA1, MESSAGE_COMPARE_FINGERPRINT_SHA1 },
		{ CRYPT_ALGO_SHA2, MESSAGE_COMPARE_FINGERPRINT_SHA2 },
		{ CRYPT_ALGO_SHAng, MESSAGE_COMPARE_FINGERPRINT_SHAng },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	MESSAGE_DATA msgData;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int compareMessageValue, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( messageType >= CTAG_PB_IR && messageType < CTAG_PB_LAST );
			  /* CTAG_PB_IR == 0 so this is the same as _NONE */

	/* If there's no certStatus then the client has rejected the 
	   certificate.  This isn't an explicit error since it's a valid 
	   protocol outcome so we return an OK status but set the overall 
	   protocol status to a generic error value to indicate that we don't 
	   want to continue normally */
	if( messageLength <= 0 )
		{
		protocolInfo->status = CRYPT_ERROR;
		return( CRYPT_OK );
		}

	/* Read the client's returned confirmation information */
	readSequence( stream, NULL );
	status = readOctetString( stream, certHash, &length,
							  8, CRYPT_MAX_HASHSIZE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid certificate confirmation" ) );
		}

	/* Compare the certificate hash to the one sent by the client.  Since 
	   we're the server this is a cryptlib-issued certificate so we know 
	   that it'll always use one of the SHA family of hashes */
	status = mapValue( protocolInfo->confHashAlgo, &compareMessageValue,
					   hashMapTable, 
					   FAILSAFE_ARRAYSIZE( hashMapTable, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	setMessageData( &msgData, certHash, length );
	status = krnlSendMessage( sessionInfoPtr->iCertResponse, 
							  IMESSAGE_COMPARE, &msgData, 
							  compareMessageValue );
	if( cryptStatusError( status ) )
		{
		/* The user is confirming an unknown certificate, the best that we 
		   can do is return a generic certificate-mismatch error */
		protocolInfo->pkiFailInfo = CMPFAILINFO_BADCERTID;
		retExt( CRYPT_ERROR_NOTFOUND,
				( CRYPT_ERROR_NOTFOUND, SESSION_ERRINFO, 
				  "Returned certificate hash doesn't match issued "
				  "certificate" ) );
		}
	return( CRYPT_OK );
	}

/* Read genMsg body:

	body		   [21]	EXPLICIT SEQUENCE OF {	-- Processed by caller
						SEQUENCE {
		infoType		OBJECT IDENTIFIER,
		intoValue		ANY DEFINED BY infoType OPTIONAL
						}
					}

   The outer tag and SEQUENCE have already been processed by the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readGenMsgBody( INOUT STREAM *stream, 
						   INOUT SESSION_INFO *sessionInfoPtr,
						   STDC_UNUSED CMP_PROTOCOL_INFO *protocolInfo,
						   IN_ENUM_OPT( CMP_MESSAGE ) \
								const CMP_MESSAGE_TYPE messageType,
						   IN_LENGTH_SHORT const int messageLength )
	{
	const BOOLEAN isRequest = ( messageType == CTAG_PB_GENM ) ? TRUE : FALSE;
	int status;

	/* If it's a request GenMsg, check for a PKIBoot request.  Note that 
	   this assumes that the only GenMsg that we'll receive is a PKIBoot,
	   there are no known other users of this message type and even if 
	   someone did decide to use it, it's not clear what we're supposed to
	   to with the contents */
	if( isRequest )
		{
		readSequence( stream, NULL );
		status = readFixedOID( stream, OID_PKIBOOT, 
							   sizeofOID( OID_PKIBOOT ) );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_NOTAVAIL,
					( CRYPT_ERROR_NOTAVAIL, SESSION_ERRINFO, 
					  "Invalid genMsg type, expected PKIBoot request" ) );
			}
		return( CRYPT_OK );
		}

	/* It's a PKIBoot response with the InfoTypeAndValue handled as CMS
	   content (see the comment for writeGenMsgResponseBody() in 
	   cmp_wrmsg.c), import the certificate trust list.  Since this isn't a 
	   true certificate chain and isn't used as such, we import it as 
	   data-only certificates */
	status = importCertFromStream( stream, &sessionInfoPtr->iCertResponse,
								   DEFAULTUSER_OBJECT_HANDLE, 
								   CRYPT_CERTTYPE_CERTCHAIN, messageLength,
								   KEYMGMT_FLAG_DATAONLY_CERT );
	if( cryptStatusError( status ) )
		retExt( status, 
				( status, SESSION_ERRINFO, "Invalid PKIBoot response" ) );
	return( CRYPT_OK );
	}

/* Read error body:

	body		   [23]	EXPLICIT SEQUENCE {	-- Processed by caller
		status			PKIFailureInfo,
		errorCode		INTEGER OPTIONAL,
		errorMsg		SEQUENCE ... OPTIONAL	-- Ignored
		}

   The outer tag and SEQUENCE have already been processed by the caller */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readErrorBody( INOUT STREAM *stream, 
						  INOUT SESSION_INFO *sessionInfoPtr,
						  STDC_UNUSED CMP_PROTOCOL_INFO *protocolInfo,
						  STDC_UNUSED const CMP_MESSAGE_TYPE messageType,
						  IN_LENGTH_SHORT const int messageLength )
	{
	ERROR_INFO *errorInfo = &sessionInfoPtr->errorInfo;
	const char *peerTypeString = isServer( sessionInfoPtr ) ? \
								 "Client" : "Server";
	const int endPos = stell( stream ) + messageLength;
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Read the outer wrapper and PKI status information.  In another one of
	   CMP's many wonderful features there are two places to communicate 
	   error information with no clear indication in the spec as to which one
	   is meant to be used.  To deal with this we stop at the first one that
	   contains an error status */
	status = readPkiStatusInfo( stream, isServer( sessionInfoPtr ),
								&sessionInfoPtr->errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* In addition to the PKI status information there can be a second lot
	   of error information which is exactly the same only different, so if 
	   we haven't got anything from the status information we check to see 
	   whether this layer can give us anything */
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_INTEGER )
		{
		long value;

		status = readShortInteger( stream, &value );
		if( cryptStatusOK( status ) && \
			( value < 0 || value > MAX_INTLENGTH_SHORT ) )
			status = CRYPT_ERROR_BADDATA;
		if( cryptStatusOK( status ) )
			{
			const int errorCode = ( int ) value;

			retExt( CRYPT_ERROR_FAILED,
					( CRYPT_ERROR_FAILED, errorInfo,
					  "%s returned nonspecific failure code %d",
					  peerTypeString, errorCode ) );
			}
		}
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_SEQUENCE )
		status = readUniversal( stream );

	/* Make sure that we always return an error code.  That is, if there's no
	   error in reading the error information then we still have to return an
	   error because what we've successfully read is a report of an error */
	retExt( cryptStatusError( status ) ? status : CRYPT_ERROR_FAILED,
			( cryptStatusError( status ) ? status : CRYPT_ERROR_FAILED, 
			  errorInfo, 
			  "%s returned nonspecific failure code", peerTypeString ) );
	}

/****************************************************************************
*																			*
*						Read Function Access Information					*
*																			*
****************************************************************************/

typedef struct {
	const CMP_MESSAGE_TYPE type;
	const READMESSAGE_FUNCTION function;
	} MESSAGEREAD_INFO;
static const MESSAGEREAD_INFO FAR_BSS messageReadTable[] = {
	{ CTAG_PB_IR, readRequestBody },
	{ CTAG_PB_CR, readRequestBody },
	{ CTAG_PB_P10CR, readRequestBody },
	{ CTAG_PB_KUR, readRequestBody },
	{ CTAG_PB_RR, readRequestBody },
	{ CTAG_PB_IP, readResponseBody },
	{ CTAG_PB_CP, readResponseBody },
	{ CTAG_PB_KUP, readResponseBody },
	{ CTAG_PB_RP, readResponseBody },
	{ CTAG_PB_CERTCONF, readConfBody },
	{ CTAG_PB_PKICONF, readConfBody },
	{ CTAG_PB_GENM, readGenMsgBody },
	{ CTAG_PB_GENP, readGenMsgBody },
	{ CTAG_PB_ERROR, readErrorBody },
	{ CTAG_PB_LAST, NULL }, { CTAG_PB_LAST, NULL }
	};

CHECK_RETVAL_PTR \
READMESSAGE_FUNCTION getMessageReadFunction( IN_ENUM_OPT( CMP_MESSAGE ) \
									const CMP_MESSAGE_TYPE messageType )
	{
	int i;

	REQUIRES_N( messageType >= CTAG_PB_IR && messageType < CTAG_PB_LAST );
				/* CTAG_PB_IR == 0 so this is the same as _NONE */

	for( i = 0; 
		 messageReadTable[ i ].type != CTAG_PB_LAST && \
			i < FAILSAFE_ARRAYSIZE( messageReadTable, MESSAGEREAD_INFO ); 
		 i++ )
		{
		if( messageReadTable[ i ].type == messageType )
			return( messageReadTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( messageReadTable, MESSAGEREAD_INFO ) );

	return( NULL );
	}
#endif /* USE_CMP */
