/****************************************************************************
*																			*
*								CMS Signature Routines						*
*						Copyright Peter Gutmann 1993-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "mech.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "mechs/mech.h"
#endif /* Compiler-specific includes */

/* CMS version */

#define CMS_VERSION		1

/* The maximum size for the encoded CMS signed attributes */

#define ENCODED_ATTRIBUTE_SIZE	512

/* A structure to store CMS attribute information */

typedef struct {
	/* The format of the signature: Basic CMS or full S/MIME */
	CRYPT_FORMAT_TYPE formatType;

	/* Objects needed to create the attributes.  The time source is a device
	   associated with the signing key (usually the system device, but can
	   be a crypto device) used to obtain the signing time.  The TSP session
	   is an optional session that's used to timestamp the signature */
	BOOLEAN useDefaultAttributes;		/* Whether we provide default attrs.*/
	CRYPT_CERTIFICATE iCmsAttributes;	/* CMS attributes */
	CRYPT_CONTEXT iMessageHash;			/* Hash for MessageDigest */
	CRYPT_HANDLE iTimeSource;			/* Time source for signing time */
	CRYPT_SESSION iTspSession;			/* Optional TSP session */

	/* The encoded attributes.  The encodedAttributes pointer is null if 
	   there are no attributes present, or points to the buffer containing 
	   the encoded attributes */
	BYTE attributeBuffer[ ENCODED_ATTRIBUTE_SIZE + 8 ];
	BUFFER_OPT( maxEncodedAttributeSize, encodedAttributeSize ) \
	BYTE *encodedAttributes;
	int maxEncodedAttributeSize;

	/* Returned data: The size of the encoded attribute information in the
	   buffer */
	int encodedAttributeSize;
	} CMS_ATTRIBUTE_INFO;

#define initCmsAttributeInfo( attributeInfo, format, useDefault, cmsAttributes, messageHash, timeSource, tspSession ) \
		memset( attributeInfo, 0, sizeof( CMS_ATTRIBUTE_INFO ) ); \
		( attributeInfo )->formatType = format; \
		( attributeInfo )->useDefaultAttributes = useDefault; \
		( attributeInfo )->iCmsAttributes = cmsAttributes; \
		( attributeInfo )->iMessageHash = messageHash; \
		( attributeInfo )->iTimeSource = timeSource; \
		( attributeInfo )->iTspSession = tspSession; \
		( attributeInfo )->maxEncodedAttributeSize = ENCODED_ATTRIBUTE_SIZE;

#ifdef USE_INT_CMS

/****************************************************************************
*																			*
*								Utility Functions 							*
*																			*
****************************************************************************/

/* Write CMS signer information:

	SignerInfo ::= SEQUENCE {
		version					INTEGER (1),
		issuerAndSerialNumber	IssuerAndSerialNumber,
		digestAlgorithm			AlgorithmIdentifier,
		signedAttrs		  [ 0 ]	IMPLICIT SET OF Attribute OPTIONAL,
		signatureAlgorithm		AlgorithmIdentifier,
		signature				OCTET STRING,
		unsignedAttrs	  [ 1 ]	IMPLICIT SET OF Attribute OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 7 ) ) \
static int writeCmsSignerInfo( INOUT STREAM *stream,
							   IN_HANDLE const CRYPT_CERTIFICATE certificate,
							   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							   IN_RANGE( 0, CRYPT_MAX_HASHSIZE ) \
									const int hashAlgoParam,
							   IN_BUFFER_OPT( attributeSize ) \
									const void *attributes, 
							   IN_DATALENGTH_Z const int attributeSize,
							   IN_BUFFER( signatureSize ) const void *signature, 
							   IN_LENGTH_SHORT const int signatureSize,
							   IN_HANDLE_OPT const CRYPT_HANDLE unsignedAttrObject )
	{
	MESSAGE_DATA msgData;
	DYNBUF iAndSDB;
	const int sizeofHashAlgoID = sizeofAlgoIDex( hashAlgo, hashAlgoParam, 0 );
	int timeStampSize DUMMY_INIT, unsignedAttributeSize = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( attributes == NULL && attributeSize == 0 ) || \
			isReadPtr( attributes, attributeSize ) );
	assert( isReadPtr( signature, signatureSize ) );

	REQUIRES( isHandleRangeValid( certificate ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashAlgoParam >= 0 && hashAlgoParam <= CRYPT_MAX_HASHSIZE );
	REQUIRES( ( attributes == NULL && attributeSize == 0 ) || \
			  ( attributes != NULL && \
				attributeSize > 0 && attributeSize < MAX_BUFFER_SIZE ) );
	REQUIRES( signatureSize > MIN_CRYPT_OBJECTSIZE && \
			  signatureSize < MAX_INTLENGTH_SHORT );
	REQUIRES( unsignedAttrObject == CRYPT_UNUSED || \
			  isHandleRangeValid( unsignedAttrObject ) );

	if( cryptStatusError( sizeofHashAlgoID ) )
		return( sizeofHashAlgoID );

	/* Get the signerInfo information */
	if( unsignedAttrObject != CRYPT_UNUSED )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( unsignedAttrObject, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_ENC_TIMESTAMP );
		if( cryptStatusError( status ) )
			return( status );
		timeStampSize = msgData.length;
		unsignedAttributeSize = ( int ) \
						sizeofObject( sizeofOID( OID_TSP_TSTOKEN ) + \
									  sizeofObject( timeStampSize ) );
		}
	status = dynCreate( &iAndSDB, certificate,
						CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the outer SEQUENCE wrapper and version number */
	writeSequence( stream, sizeofShortInteger( CMS_VERSION ) + \
						   dynLength( iAndSDB ) + sizeofHashAlgoID + \
						   attributeSize + signatureSize + \
						   ( ( unsignedAttributeSize ) ? \
							 ( int ) sizeofObject( unsignedAttributeSize ) : 0 ) );
	writeShortInteger( stream, CMS_VERSION, DEFAULT_TAG );

	/* Write the issuerAndSerialNumber, digest algorithm identifier,
	   attributes (if there are any) and signature */
	swrite( stream, dynData( iAndSDB ), dynLength( iAndSDB ) );
	writeAlgoIDex( stream, hashAlgo, hashAlgoParam, 0 );
	if( attributeSize > 0 )
		swrite( stream, attributes, attributeSize );
	status = swrite( stream, signature, signatureSize );
	dynDestroy( &iAndSDB );
	if( cryptStatusError( status ) || unsignedAttributeSize <= 0 )
		return( status );

	/* Write the unsigned attributes.  Note that the only unsigned attribute
	   in use at this time is a (not-quite) countersignature containing a
	   timestamp, so the following code always assumes that the attribute is
	   a timestamp.  First we write the [1] IMPLICT SET OF attribute
	   wrapper */
	writeConstructed( stream, unsignedAttributeSize, 1 );
	writeSequence( stream, sizeofOID( OID_TSP_TSTOKEN ) + \
						   sizeofObject( timeStampSize ) );
	writeOID( stream, OID_TSP_TSTOKEN );
	status = writeSet( stream, timeStampSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Then we copy the timestamp data directly into the stream */
	return( exportAttributeToStream( stream, unsignedAttrObject,
									 CRYPT_IATTRIBUTE_ENC_TIMESTAMP ) );
	}

/* Create a CMS countersignature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createCmsCountersignature( IN_BUFFER( dataSignatureSize ) \
										const void *dataSignature,
									  IN_LENGTH_SHORT const int dataSignatureSize,
									  IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
									  IN_RANGE( 0, CRYPT_MAX_HASHSIZE ) \
											const int hashAlgoParam,
									  IN_HANDLE const CRYPT_SESSION iTspSession )
	{
	CRYPT_CONTEXT iHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	int length, status;

	assert( isReadPtr( dataSignature, dataSignatureSize ) );

	REQUIRES( dataSignatureSize > MIN_CRYPT_OBJECTSIZE && \
			  dataSignatureSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashAlgoParam >= 0 && hashAlgoParam <= CRYPT_MAX_HASHSIZE );
	REQUIRES( isHandleRangeValid( iTspSession ) );

	/* Hash the signature data to create the hash value to countersign.
	   The CMS spec requires that the signature is calculated on the
	   contents octets (in other words the V of the TLV) of the signature,
	   so we have to skip the signature algorithm and OCTET STRING wrapper */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	if( isHashMacExtAlgo( hashAlgo ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &hashAlgoParam, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}
	iHashContext = createInfo.cryptHandle;
#if 1	/* Standard CMS countersignature */
	sMemConnect( &stream, dataSignature, dataSignatureSize );
	readUniversal( &stream );
	status = readOctetStringHole( &stream, &length, 16, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		{
		void *dataPtr;

		status = sMemGetDataBlock( &stream, &dataPtr, length );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
									  dataPtr, length );
			}
		}
	sMemDisconnect( &stream );
#else	/* Broken TSP not-quite-countersignature */
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
					 ( MESSAGE_CAST ) dataSignature, dataSignatureSize );
#endif /* 1 */
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iTspSession, IMESSAGE_SETATTRIBUTE,
								  &iHashContext,
								  CRYPT_SESSINFO_TSP_MSGIMPRINT );
		}
	krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the result to the TSA for countersigning */
	return( krnlSendMessage( iTspSession, IMESSAGE_SETATTRIBUTE,
							 MESSAGE_VALUE_TRUE, CRYPT_SESSINFO_ACTIVE ) );
	}

/* Add sMimeCapabilities to a CMS attribute object */

static void addSmimeCapabilities( IN_HANDLE const CRYPT_CERTIFICATE iCmsAttributes )
	{
	typedef struct { 
		CRYPT_ALGO_TYPE algorithm;
		CRYPT_ALGO_TYPE secondaryAlgorithm;
		CRYPT_ATTRIBUTE_TYPE smimeCapability;
		} SMIMECAP_INFO;
	static const SMIMECAP_INFO smimeCapInfo[] = {
		{ CRYPT_ALGO_3DES, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_3DES },
		{ CRYPT_ALGO_AES, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_AES },
#ifdef USE_SHAng
		{ CRYPT_ALGO_SHAng, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_SHAng },
#endif /* USE_SHAng */
		{ CRYPT_ALGO_SHA2, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_SHA2 },
		{ CRYPT_ALGO_SHA1, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_SHA1 },
#ifdef USE_SHAng
		{ CRYPT_ALGO_HMAC_SHAng, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng },
#endif /* USE_SHAng */
		{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2 },
		{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1 },
		{ CRYPT_IALGO_GENERIC_SECRET, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256 },
		{ CRYPT_IALGO_GENERIC_SECRET, CRYPT_ALGO_NONE, 
		  CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128 },
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHAng,
		  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng },
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2,
		  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2 },
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE,	/* 'None' since always avail. */
		  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1 },
		{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE,	/* 'None' since always avail. */
		  CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1 },
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHAng,
		  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng },
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2,
		  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2 },
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_NONE,	/* 'None' since always avail. */
		  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1 },
		{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ATTRIBUTE_NONE },
		{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, CRYPT_ATTRIBUTE_NONE },
		};
	CRYPT_ALGO_TYPE prevAlgo = CRYPT_ALGO_NONE;
	int value, i, status;

	REQUIRES_V( isHandleRangeValid( iCmsAttributes ) );

	/* If there are already sMIMECapabilities present don't try and add 
	   anything further */
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE, 
							  &value, CRYPT_CERTINFO_CMS_SMIMECAPABILITIES );
	if( cryptStatusOK( status ) )
		return;

	/* Add an sMIMECapability for each supported algorithm or algorithm 
	   combination.  Since these are no-value attributes it's not worth 
	   aborting the signature generation if the attempt to add them fails 
	   so we don't bother checking the return value */
	for( i = 0; smimeCapInfo[ i ].algorithm != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( smimeCapInfo, SMIMECAP_INFO );
		 i++ )
		{
		const CRYPT_ALGO_TYPE algorithm = smimeCapInfo[ i ].algorithm;

		/* Make sure that the primary algorithm is available */
		if( prevAlgo != algorithm && !algoAvailable( algorithm ) )
			{
			prevAlgo = algorithm;
			continue;
			}
		prevAlgo = algorithm;

		/* If there's a secondary algorithm, make sure that it's available */
		if( smimeCapInfo[ i ].secondaryAlgorithm != CRYPT_ALGO_NONE && \
			!algoAvailable( smimeCapInfo[ i ].secondaryAlgorithm ) )
			continue;

		/* List this algorithm or algorithm combination as an available 
		   capability */
		( void ) krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE, 
								  MESSAGE_VALUE_UNUSED, 
								  smimeCapInfo[ i ].smimeCapability );
		}
	ENSURES_V( i < FAILSAFE_ARRAYSIZE( smimeCapInfo, SMIMECAP_INFO ) );

	/* Add any futher non-algorithm-related sMIMECapabilities */
	( void ) krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_UNUSED,
							  CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE );
	}

/****************************************************************************
*																			*
*							Create CMS Attributes 							*
*																			*
****************************************************************************/

/* Finalise processing of and hash the CMS attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int hashCmsAttributes( INOUT CMS_ATTRIBUTE_INFO *cmsAttributeInfo,
							  IN_HANDLE const CRYPT_CONTEXT iAttributeHash,
							  const BOOLEAN lengthCheckOnly )
	{
	MESSAGE_DATA msgData;
	BYTE temp, hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isWritePtr( cmsAttributeInfo, sizeof( CMS_ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( cmsAttributeInfo->encodedAttributes, \
						cmsAttributeInfo->maxEncodedAttributeSize ) );

	REQUIRES( isHandleRangeValid( cmsAttributeInfo->iCmsAttributes ) );
	REQUIRES( isHandleRangeValid( cmsAttributeInfo->iMessageHash ) );
	REQUIRES( isHandleRangeValid( iAttributeHash ) );

	/* Extract the message hash information and add it as a messageDigest
	   attribute, replacing any existing value if necessary (we don't bother
	   checking the return value because the attribute may or may not be 
	   present, and a failure to delete it will be detected immediately
	   afterwards when we try and set it).  If we're doing a call just to 
	   get the length of the exported data we use a dummy hash value since 
	   the hashing may not have completed yet */
	( void ) krnlSendMessage( cmsAttributeInfo->iCmsAttributes, 
							  IMESSAGE_DELETEATTRIBUTE, NULL,
							  CRYPT_CERTINFO_CMS_MESSAGEDIGEST );
	setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
	if( lengthCheckOnly )
		{
		memset( hash, 0, CRYPT_MAX_HASHSIZE );	/* Keep mem.checkers happy */
		status = krnlSendMessage( cmsAttributeInfo->iMessageHash, 
								  IMESSAGE_GETATTRIBUTE, &msgData.length, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		}
	else
		{
		status = krnlSendMessage( cmsAttributeInfo->iMessageHash, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CTXINFO_HASHVALUE );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( cmsAttributeInfo->iCmsAttributes, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_CMS_MESSAGEDIGEST );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If we're creating the attributes for a real signature rather than 
	   just as part of a size check and there's a reliable time source
	   present, use the time from that instead of the built-in system time.
	   Although this seems like a trivial thing it's likely that the 
	   presence of a high-assurance time source means that the accuracy of
	   timekeeping is considered critical so we fail the signature 
	   generation if we can't set the time from the reliable source */
	if( !lengthCheckOnly )
		{
		const time_t currentTime = \
				getReliableTime( cmsAttributeInfo->iTimeSource );

		if( currentTime > MIN_TIME_VALUE )
			{
			setMessageData( &msgData, ( MESSAGE_CAST ) &currentTime,
							sizeof( time_t ) );
			( void ) krnlSendMessage( cmsAttributeInfo->iCmsAttributes, 
									  IMESSAGE_DELETEATTRIBUTE, NULL,
									  CRYPT_CERTINFO_CMS_SIGNINGTIME );
			status = krnlSendMessage( cmsAttributeInfo->iCmsAttributes, 
									  IMESSAGE_SETATTRIBUTE_S, &msgData, 
									  CRYPT_CERTINFO_CMS_SIGNINGTIME );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Export the attributes into an encoded signedAttributes data block */
	if( lengthCheckOnly )
		{ setMessageData( &msgData, NULL, 0 ); }
	else
		{ 
		setMessageData( &msgData, cmsAttributeInfo->encodedAttributes,
						cmsAttributeInfo->maxEncodedAttributeSize );
		}
	status = krnlSendMessage( cmsAttributeInfo->iCmsAttributes, 
							  IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_ICERTFORMAT_DATA );
	if( cryptStatusError( status ) )
		return( status );
	cmsAttributeInfo->encodedAttributeSize = msgData.length;

	/* If it's a length check, just generate a dummy hash value and exit */
	if( lengthCheckOnly )
		return( krnlSendMessage( iAttributeHash, IMESSAGE_CTX_HASH, "", 0 ) );

	/* Replace the IMPLICIT [ 0 ] tag at the start with a SET OF tag to 
	   allow the attributes to be hashed, hash them into the attribute hash 
	   context, and replace the original tag */
	temp = cmsAttributeInfo->encodedAttributes[ 0 ];
	cmsAttributeInfo->encodedAttributes[ 0 ] = BER_SET;
	status = krnlSendMessage( iAttributeHash, IMESSAGE_CTX_HASH,
							  cmsAttributeInfo->encodedAttributes,
							  cmsAttributeInfo->encodedAttributeSize );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iAttributeHash, IMESSAGE_CTX_HASH, "", 0 );
	cmsAttributeInfo->encodedAttributes[ 0 ] = temp;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createCmsAttributes( INOUT CMS_ATTRIBUTE_INFO *cmsAttributeInfo,
								OUT_HANDLE_OPT CRYPT_CONTEXT *iCmsHashContext,
								IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
								IN_RANGE( 0, CRYPT_MAX_HASHSIZE ) \
									const int hashAlgoParam,
								const BOOLEAN lengthCheckOnly )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BOOLEAN createdHashContext = FALSE;
	int status;

	assert( isWritePtr( cmsAttributeInfo, sizeof( CMS_ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( cmsAttributeInfo->attributeBuffer, \
						cmsAttributeInfo->maxEncodedAttributeSize ) );
	assert( isWritePtr( iCmsHashContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( cmsAttributeInfo->formatType == CRYPT_FORMAT_CMS || \
			  cmsAttributeInfo->formatType == CRYPT_FORMAT_SMIME );
	REQUIRES( ( cmsAttributeInfo->iCmsAttributes == CRYPT_UNUSED && \
				cmsAttributeInfo->useDefaultAttributes == FALSE ) || \
			  ( cmsAttributeInfo->iCmsAttributes == CRYPT_UNUSED && \
				cmsAttributeInfo->useDefaultAttributes == TRUE ) || \
			  ( isHandleRangeValid( cmsAttributeInfo->iCmsAttributes ) && \
			    cmsAttributeInfo->useDefaultAttributes == FALSE ) );
	REQUIRES( isHandleRangeValid( cmsAttributeInfo->iMessageHash ) );
	REQUIRES( isHandleRangeValid( cmsAttributeInfo->iTimeSource ) );
	REQUIRES( ( cmsAttributeInfo->iTspSession == CRYPT_UNUSED ) || \
			  isHandleRangeValid( cmsAttributeInfo->iTspSession ) );
	REQUIRES( cmsAttributeInfo->encodedAttributes == NULL && \
			  cmsAttributeInfo->encodedAttributeSize == 0 );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashAlgoParam >= 0 && hashAlgoParam <= CRYPT_MAX_HASHSIZE );

	/* Clear return value */
	*iCmsHashContext = CRYPT_ERROR;

	/* Set up the attribute buffer */
	cmsAttributeInfo->encodedAttributes = cmsAttributeInfo->attributeBuffer;

	/* If the user hasn't supplied the attributes, generate them ourselves */
	if( cmsAttributeInfo->useDefaultAttributes )
		{
		setMessageCreateObjectInfo( &createInfo,
									CRYPT_CERTTYPE_CMS_ATTRIBUTES );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		cmsAttributeInfo->iCmsAttributes = createInfo.cryptHandle;
		}
	ENSURES( isHandleRangeValid( cmsAttributeInfo->iCmsAttributes ) );

	/* If it's an S/MIME (vs.pure CMS) signature add the sMIMECapabilities 
	   to further bloat things up.  Since these are no-value attributes 
	   it's not worth aborting the signature generation if the attempt to 
	   add them fails so we don't bother checking a return value */
	if( cmsAttributeInfo->formatType == CRYPT_FORMAT_SMIME )
		( void ) addSmimeCapabilities( cmsAttributeInfo->iCmsAttributes );

	/* Generate the attributes and hash them into the CMS hash context */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusOK( status ) && isHashMacExtAlgo( hashAlgo ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &hashAlgoParam, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		}
	if( cryptStatusOK( status ) )
		{
		createdHashContext = TRUE;
		status = hashCmsAttributes( cmsAttributeInfo, createInfo.cryptHandle, 
									lengthCheckOnly );
		}
	if( cmsAttributeInfo->useDefaultAttributes )
		{
		krnlSendNotifier( cmsAttributeInfo->iCmsAttributes, 
						  IMESSAGE_DECREFCOUNT );
		cmsAttributeInfo->iCmsAttributes = CRYPT_UNUSED;
		}
	if( cryptStatusError( status ) )
		{
		if( createdHashContext )
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Return the hash of the attributes to the caller */
	*iCmsHashContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Create/Check a CMS Signature 					*
*																			*
****************************************************************************/

/* Create a CMS signature.  The use of authenticated attributes is a three-
   way choice:

	useDefaultAuthAttr = FALSE,		No attributes.
	iAuthAttr = CRYPT_UNUSED

	useDefaultAuthAttr = TRUE,		We supply default attributes.
	iAuthAttr = CRYPT_UNUSED

	useDefaultAuthAttr = FALSE,		Caller has supplied attributes
	iAuthAttr = validhandle */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int createSignatureCMS( OUT_BUFFER_OPT( sigMaxLength, *signatureLength ) \
							void *signature, 
						IN_DATALENGTH_Z const int sigMaxLength, 
						OUT_DATALENGTH_Z int *signatureLength,
						IN_HANDLE const CRYPT_CONTEXT signContext,
						IN_HANDLE const CRYPT_CONTEXT iHashContext,
						const BOOLEAN useDefaultAuthAttr,
						IN_HANDLE_OPT const CRYPT_CERTIFICATE iAuthAttr,
						IN_HANDLE_OPT const CRYPT_SESSION iTspSession,
						IN_ENUM( CRYPT_FORMAT ) \
						const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_CONTEXT iCmsHashContext = iHashContext;
	CRYPT_CERTIFICATE iSigningCert;
	STREAM stream;
	CMS_ATTRIBUTE_INFO cmsAttributeInfo;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 128 + 8 ];
	BYTE *bufPtr = ( signature == NULL ) ? NULL : buffer;
	const int bufSize = ( signature == NULL ) ? 0 : CRYPT_MAX_PKCSIZE + 128;
	int hashAlgo, hashAlgoParam = 0;
	int dataSignatureSize, length DUMMY_INIT, status;

	assert( ( signature == NULL && sigMaxLength == 0 ) || \
			isReadPtr( signature, sigMaxLength ) );
	assert( isWritePtr( signatureLength, sizeof( int ) ) );

	REQUIRES( ( signature == NULL && sigMaxLength == 0 ) || \
			  ( signature != NULL && \
			    sigMaxLength > MIN_CRYPT_OBJECTSIZE && \
				sigMaxLength < MAX_BUFFER_SIZE ) );
	REQUIRES( isHandleRangeValid( signContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( ( iAuthAttr == CRYPT_UNUSED && \
				useDefaultAuthAttr == FALSE ) || \
			  ( iAuthAttr == CRYPT_UNUSED && \
				useDefaultAuthAttr == TRUE ) || \
			  ( isHandleRangeValid( iAuthAttr ) && \
			    useDefaultAuthAttr == FALSE ) );
	REQUIRES( ( iTspSession == CRYPT_UNUSED ) || \
			  isHandleRangeValid( iTspSession ) );
	REQUIRES( formatType == CRYPT_FORMAT_CMS || \
			  formatType == CRYPT_FORMAT_SMIME );

	/* Clear return value */
	*signatureLength = 0;

	initCmsAttributeInfo( &cmsAttributeInfo, formatType, 
						  useDefaultAuthAttr, iAuthAttr, iHashContext, 
						  signContext, iTspSession );

	/* Get the message hash algo and signing certificate */
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && isHashMacExtAlgo( hashAlgo ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashAlgoParam, CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );
	status = krnlSendMessage( signContext, IMESSAGE_GETDEPENDENT,
							  &iSigningCert, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM1 : status );

	/* If we're using signed attributes, set them up to be added to the
	   signature info */
	if( useDefaultAuthAttr || iAuthAttr != CRYPT_UNUSED )
		{
		status = createCmsAttributes( &cmsAttributeInfo, &iCmsHashContext, 
									  hashAlgo, hashAlgoParam,
									  ( signature == NULL ) ? TRUE : FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Create the signature */
	status = createSignature( bufPtr, bufSize, &dataSignatureSize, 
							  signContext, iCmsHashContext, CRYPT_UNUSED, 
							  SIGNATURE_CMS );
	if( iCmsHashContext != iHashContext )
		krnlSendNotifier( iCmsHashContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're countersigning the signature (typically done via a
	   timestamp), create the countersignature */
	if( iTspSession != CRYPT_UNUSED && signature != NULL )
		{
		status = createCmsCountersignature( buffer, dataSignatureSize,
											hashAlgo, hashAlgoParam,
											iTspSession );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the signerInfo record */
	sMemOpenOpt( &stream, signature, ( signature == NULL ) ? 0 : sigMaxLength );
	status = writeCmsSignerInfo( &stream, iSigningCert, hashAlgo, hashAlgoParam,
								 cmsAttributeInfo.encodedAttributes, 
								 cmsAttributeInfo.encodedAttributeSize,
								 buffer, dataSignatureSize,
								 ( signature == NULL ) ? CRYPT_UNUSED : iTspSession );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	if( iTspSession != CRYPT_UNUSED && signature == NULL )
		{
		/* If we're countersigning the signature with a timestamp and doing 
		   a length check only then we run into problems because we can't 
		   report how big the final size will be without contacting the TSA 
		   for a timestamp.  In theory we could report some hopefully-OK
		   large-enough size, but at the moment we take advantage of the 
		   fact that we're only going to be called from the enveloping code 
		   because a timestamp only makes sense as a countersignature on CMS 
		   data.  It's somewhat ugly because it asumes internal knowledge of 
		   the envelope abstraction but there isn't really any clean way to 
		   handle this because we can't tell in advance how much data the 
		   TSA will send us.

		   In theory the code doesn't even need to return an estimate 
		   because, without knowing the exact length of the 
		   countersignature, the enveloping code has to fall back to using
		   indefinite-length encoding (see envelope/cms_envpre.c), so the
		   length value that we return here is ignored */
		length += MIN_BUFFER_SIZE;
		}
	*signatureLength = length;

	return( CRYPT_OK );
	}

/* Check a CMS signature.  The reason why there are apparently two 
   signature-checking objects present in the function arguments is that
   sigCheckContext is the raw public-key context while iSigCheckKey is the 
   overall signature-checking object, which may include attached 
   certificates and other information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkSignatureCMS( IN_BUFFER( signatureLength ) const void *signature, 
					   IN_DATALENGTH const int signatureLength,
					   IN_HANDLE const CRYPT_CONTEXT sigCheckContext,
					   IN_HANDLE const CRYPT_CONTEXT iHashContext,
					   OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iExtraData,
					   IN_HANDLE const CRYPT_HANDLE iSigCheckKey )
	{
	CRYPT_CERTIFICATE iLocalExtraData;
	CRYPT_CONTEXT iCmsHashContext = iHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	QUERY_INFO queryInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	static const BYTE setTag[] = { BER_SET };
	BYTE hashValue[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashAlgo, hashAlgoParam = 0, status;	/* int vs.enum */

	assert( isReadPtr( signature, signatureLength ) );
	assert( ( iExtraData == NULL ) || \
			isWritePtr( iExtraData, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( signatureLength > 40 && signatureLength < MAX_BUFFER_SIZE );
	REQUIRES( isHandleRangeValid( sigCheckContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( isHandleRangeValid( iSigCheckKey ) );

	if( iExtraData != NULL )
		*iExtraData = CRYPT_ERROR;

	/* Get the message hash algo */
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && isHashMacExtAlgo( hashAlgo ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashAlgoParam, CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );

	/* Unpack the SignerInfo record and make sure that the supplied key is
	   the correct one for the sig.check and the supplied hash context
	   matches the algorithm used in the signature */
	sMemConnect( &stream, signature, signatureLength );
	status = queryAsn1Object( &stream, &queryInfo );
	if( cryptStatusOK( status ) && \
		( queryInfo.formatType != CRYPT_FORMAT_CMS && \
		  queryInfo.formatType != CRYPT_FORMAT_SMIME ) )
		status = CRYPT_ERROR_BADDATA;
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES( rangeCheck( queryInfo.iAndSStart, queryInfo.iAndSLength,
						  queryInfo.size ) );
	setMessageData( &msgData, \
					( BYTE * ) signature + queryInfo.iAndSStart, \
					queryInfo.iAndSLength );
	status = krnlSendMessage( iSigCheckKey, IMESSAGE_COMPARE, &msgData,
							  MESSAGE_COMPARE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( status ) )
		{
		/* A failed comparison is reported as a generic CRYPT_ERROR,
		   convert it into a wrong-key error if necessary */
		return( ( status == CRYPT_ERROR ) ? \
				CRYPT_ERROR_WRONGKEY : status );
		}
	if( queryInfo.hashAlgo != hashAlgo || \
		queryInfo.hashAlgoParam != hashAlgoParam )
		return( CRYPT_ARGERROR_NUM2 );

	/* If there are no signed attributes present, just check the signature 
	   and exit */
	if( queryInfo.attributeStart <= 0 )
		{
		return( checkSignature( signature, signatureLength, sigCheckContext,
								iCmsHashContext, CRYPT_UNUSED, 
								SIGNATURE_CMS ) );
		}

	/* There are signedAttributes present, hash the data, substituting a SET 
	   OF tag for the IMPLICIT [ 0 ] tag at the start */
	REQUIRES( rangeCheck( queryInfo.attributeStart, 
						  queryInfo.attributeLength, queryInfo.size ) );
	setMessageCreateObjectInfo( &createInfo, queryInfo.hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	if( isHashMacExtAlgo( queryInfo.hashAlgo ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  &queryInfo.hashAlgoParam, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}
	iCmsHashContext = createInfo.cryptHandle;
	status = krnlSendMessage( iCmsHashContext, IMESSAGE_CTX_HASH,
							  ( BYTE * ) setTag, sizeof( BYTE ) );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCmsHashContext, IMESSAGE_CTX_HASH,
						( BYTE * ) signature + queryInfo.attributeStart + 1,
						queryInfo.attributeLength - 1 );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCmsHashContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Check the signature */
	status = checkSignature( signature, signatureLength, sigCheckContext,
							 iCmsHashContext, CRYPT_UNUSED, SIGNATURE_CMS );
	krnlSendNotifier( iCmsHashContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Import the attributes and make sure that the data hash value given in
	   the signed attributes matches the user-supplied hash */
	REQUIRES( rangeCheck( queryInfo.attributeStart, 
						  queryInfo.attributeLength, queryInfo.size ) );
	setMessageCreateObjectIndirectInfo( &createInfo,
						( BYTE * ) signature + queryInfo.attributeStart,
						queryInfo.attributeLength,
						CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	iLocalExtraData = createInfo.cryptHandle;
	setMessageData( &msgData, hashValue, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iLocalExtraData, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_CMS_MESSAGEDIGEST );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iHashContext, IMESSAGE_COMPARE, &msgData,
								  MESSAGE_COMPARE_HASH );
		if( cryptStatusError( status ) )
			status = CRYPT_ERROR_SIGNATURE;
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalExtraData, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* If the user wants to look at the authenticated attributes, make them
	   externally visible, otherwise delete them */
	if( iExtraData != NULL )
		*iExtraData = iLocalExtraData;
	else
		krnlSendNotifier( iLocalExtraData, IMESSAGE_DECREFCOUNT );

	return( CRYPT_OK );
	}
#endif /* USE_INT_CMS */
