/****************************************************************************
*																			*
*					cryptlib PKCS #12 Object-Read Routines					*
*						Copyright Peter Gutmann 1997-2010					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs12.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs12.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS12

/* OID information used to read a PKCS #12 keyset */

static const FAR_BSS OID_INFO keyCertBagOIDinfo[] = {
	{ OID_PKCS12_SHROUDEDKEYBAG, TRUE },
	{ OID_PKCS12_CERTBAG, FALSE },
	{ NULL, 0 }, { NULL, 0 }
	};

/* OID information used to read decrypted PKCS #12 objects */

static const FAR_BSS OID_INFO certBagOIDinfo[] = {
	{ OID_PKCS12_CERTBAG, 0 },
	{ NULL, 0 }, { NULL, 0 }
	};
static const FAR_BSS OID_INFO certOIDinfo[] = {
	{ OID_PKCS9_X509CERTIFICATE, 0 },
	{ NULL, 0 }, { NULL, 0 }
	};

/* Protection algorithms used for encrypted keys and certificates, and a 
   mapping from PKCS #12 to cryptlib equivalents.  Beyond these there are
   also 40- and 128-bit RC4 and 128-bit RC2, but nothing seems to use
   them.  40-bit RC2 is used by Windows to, uhh, "protect" public
   certificates so we have to support it in order to be able to read
   certificates (see the comment in keymgmt/pkcs12.c for details on how
   the 40-bit RC2 key is handled) */

enum { PKCS12_ALGO_NONE, PKCS12_ALGO_3DES_192, PKCS12_ALGO_3DES_128, 
	   PKCS12_ALGO_RC2_40 };

typedef struct {
	const CRYPT_ALGO_TYPE cryptAlgo;
	const int keySize;
	} PKCS12_ALGO_MAP;

static const PKCS12_ALGO_MAP algoMap3DES_192 = { CRYPT_ALGO_3DES, bitsToBytes( 192 ) };
static const PKCS12_ALGO_MAP algoMap3DES_128 = { CRYPT_ALGO_3DES, bitsToBytes( 128 ) };
static const PKCS12_ALGO_MAP algoMapRC2_40 = { CRYPT_ALGO_RC2, bitsToBytes( 40 ) };

static const FAR_BSS OID_INFO encryptionOIDinfo[] = {
	{ OID_PKCS12_PBEWITHSHAAND3KEYTRIPLEDESCBC, PKCS12_ALGO_3DES_192, 
	  &algoMap3DES_192 },
	{ OID_PKCS12_PBEWITHSHAAND2KEYTRIPLEDESCBC, PKCS12_ALGO_3DES_128,
	  &algoMap3DES_128 },
	{ OID_PKCS12_PBEWITHSHAAND40BITRC2CBC, PKCS12_ALGO_RC2_40,
	  &algoMapRC2_40 },
	{ NULL, 0 }, { NULL, 0 }
	};

/* PKCS #12 attributes.  This is a subset of the full range that can be 
   used, we skip any that we don't care about using a wildcard OID match */

enum { PKCS12_ATTRIBUTE_NONE, PKCS12_ATTRIBUTE_LABEL, PKCS12_ATTRIBUTE_ID };

static const FAR_BSS OID_INFO attributeOIDinfo[] = {
	{ OID_PKCS9_FRIENDLYNAME, PKCS12_ATTRIBUTE_LABEL },
	{ OID_PKCS9_LOCALKEYID, PKCS12_ATTRIBUTE_ID },
	{ WILDCARD_OID, PKCS12_ATTRIBUTE_NONE },
	{ NULL, 0 }, { NULL, 0 }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Read protection algorithm information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readProtAlgoInfo( INOUT STREAM *stream, 
							 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
							 OUT_INT_SHORT_Z int *keySize )
	{
	const OID_INFO *oidInfoPtr;
	const PKCS12_ALGO_MAP *algoMapInfoPtr;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( keySize, sizeof( int ) ) );

	/* Clear return values */
	*cryptAlgo = CRYPT_ALGO_NONE;
	*keySize = 0;

	/* Read the wrapper and the protection algorithm OID and extract the
	   protection information parameters for it */
	readSequence( stream, NULL );
	status = readOIDEx( stream, encryptionOIDinfo, 
						FAILSAFE_ARRAYSIZE( encryptionOIDinfo, OID_INFO ), 
						&oidInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	algoMapInfoPtr = oidInfoPtr->extraInfo;
	*cryptAlgo = algoMapInfoPtr->cryptAlgo;
	*keySize = algoMapInfoPtr->keySize;

	return( CRYPT_OK );
	}

/* Read key-derivation information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int readKeyDerivationInfo( INOUT STREAM *stream, 
								  OUT_BUFFER( saltMaxLen, *saltLen ) void *salt,
								  IN_LENGTH_SHORT_MIN( 16 ) const int saltMaxLen,
								  OUT_LENGTH_BOUNDED_Z( saltMaxLen ) int *saltLen,
								  OUT_INT_SHORT_Z int *iterations )
	{
	long intValue;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( salt, saltMaxLen ) );
	assert( isWritePtr( saltLen, sizeof( int ) ) );
	assert( isWritePtr( iterations, sizeof( int ) ) );

	REQUIRES( saltMaxLen >= 16 && saltMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( salt, 0, min( 16, saltMaxLen ) );
	*saltLen = *iterations = 0;

	/* Read the wrapper and salt value */
	readSequence( stream, NULL );
	status = readOctetString( stream, salt, saltLen, 1, saltMaxLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the iteration count and make sure that it's within a sensible
	   range */
	status = readShortInteger( stream, &intValue );
	if( cryptStatusError( status ) )
		return( status );
	if( intValue < 1 || intValue > MAX_KEYSETUP_ITERATIONS )
		return( CRYPT_ERROR_BADDATA );
	*iterations = ( int ) intValue;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Read PKCS #12 Object Information					*
*																			*
****************************************************************************/

/* Read an object's attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readObjectAttributes( INOUT STREAM *stream, 
								 INOUT PKCS12_INFO *pkcs12info )
	{
	int endPos, length, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12info, sizeof( PKCS12_INFO ) ) );

	/* Determine how big the collection of attributes is */
	status = readSet( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the collection of attributes */
	for( iterationCount = 0;
		 stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		BYTE stringBuffer[ ( CRYPT_MAX_TEXTSIZE * 2 ) + 8 ];
		int attributeType, stringLength, srcIndex, destIndex;

		/* Read the outer wrapper and determine the attribute type based on
		   the OID */
		readSequence( stream, NULL );
		status = readOID( stream, attributeOIDinfo, 
						  FAILSAFE_ARRAYSIZE( attributeOIDinfo, OID_INFO ), 
						  &attributeType );
		if( cryptStatusError( status ) )
			return( status );

		/* Read the wrapper around the attribute payload */
		status = readSet( stream, &length );
		if( cryptStatusError( status ) )
			return( status );

		switch( attributeType )
			{
			case PKCS12_ATTRIBUTE_NONE:
				/* It's a don't-care attribute, skip it */
				if( length > 0 )
					status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
				break;

			case PKCS12_ATTRIBUTE_LABEL:
				/* Read the label, translating it from Unicode.  We assume
				   that it's just widechar ASCII/latin-1 (which always seems
				   to be the case), which avoids OS-specific i18n 
				   headaches */
				status = readCharacterString( stream, stringBuffer, 
									CRYPT_MAX_TEXTSIZE * 2, &stringLength,
									BER_STRING_BMP );
				if( cryptStatusError( status ) )
					break;
				for( srcIndex = destIndex = 0; srcIndex < stringLength;
					 srcIndex +=2, destIndex++ )
					{
					pkcs12info->label[ destIndex ] = \
								stringBuffer[ srcIndex + 1 ];
					}
				pkcs12info->labelLength = destIndex;
				break;

			case PKCS12_ATTRIBUTE_ID:
				/* It's a binary-blob ID value, usually a 32-bit little-
				   endian integer, remember it in case it's needed later 
				   (this is the sole vaguely-useful ID that PKCS #12
				   provides, and can sometimes be used to match
				   certificates to their corresponding private keys) */
				status = readOctetString( stream, pkcs12info->id, 
										  &pkcs12info->idLength, 
										  1, CRYPT_MAX_HASHSIZE );
				break;

			default:
				retIntError();
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/* Read object information.  The standard unencrypted object is always a
   certificate, the encrypted object can be a certificate as well, or a 
   private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readObjectInfo( INOUT STREAM *stream, 
						   OUT PKCS12_OBJECT_INFO *pkcs12objectInfo,
						   INOUT ERROR_INFO *errorInfo )
	{
	long length;
	int payloadOffset DUMMY_INIT;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12objectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	memset( pkcs12objectInfo, 0, sizeof( PKCS12_OBJECT_INFO ) );

	/* Read the inner portion of the redundantly-nested object types and
	   remember the payload details within it */
	status = readCMSheader( stream, certOIDinfo, 
							FAILSAFE_ARRAYSIZE( certOIDinfo, OID_INFO ), 
							&length, READCMS_FLAG_INNERHEADER | \
									 READCMS_FLAG_DEFINITELENGTH );
	if( cryptStatusOK( status ) && \
		( length < MIN_OBJECT_SIZE || length > MAX_INTLENGTH_SHORT ) )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusOK( status ) )
		{
		payloadOffset = stell( stream );
		status = sSkip( stream, length, SSKIP_MAX );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid certificate payload data" ) );
		}
	pkcs12objectInfo->payloadOffset = payloadOffset;
	pkcs12objectInfo->payloadSize = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readEncryptedObjectInfo( INOUT STREAM *stream, 
									OUT PKCS12_OBJECT_INFO *pkcs12objectInfo,
									const BOOLEAN isEncryptedCert,
									INOUT ERROR_INFO *errorInfo )
	{
#ifdef USE_ERRMSGS
	const char *objectName = isEncryptedCert ? "encrypted certificate" : \
											   "encrypted private key";
#endif /* USE_ERRMSGS */
	int payloadOffset DUMMY_INIT, payloadLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12objectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	memset( pkcs12objectInfo, 0, sizeof( PKCS12_OBJECT_INFO ) );

	/* Read the encryption algorithm information */
	status = readProtAlgoInfo( stream, &pkcs12objectInfo->cryptAlgo,
							   &pkcs12objectInfo->keySize );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid %s protection algorithm", objectName ) );
		}
	
	/* Read the key-derivation parameters */
	status = readKeyDerivationInfo( stream, pkcs12objectInfo->salt,
									CRYPT_MAX_HASHSIZE, 
									&pkcs12objectInfo->saltSize,
									&pkcs12objectInfo->iterations );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid %s protection parameters", objectName ) );
		}

	/* Read the start of the encrypted content.  This has a variety of 
	   encapsulations depending on how its hidden inside the PKCS #12 
	   object so we read it as a generic object.  readGenericHole()
	   disallows indefinite-length encodings so we know that the returned 
	   payload length will have a definite value */
	status = readGenericHole( stream, &payloadLength, MIN_OBJECT_SIZE, 
							  DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		{
		payloadOffset = stell( stream );
		status = sSkip( stream, payloadLength, SSKIP_MAX );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid %s payload data", objectName ) );
		}
	pkcs12objectInfo->payloadOffset = payloadOffset;
	pkcs12objectInfo->payloadSize = payloadLength;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read PKCS #12 Keys								*
*																			*
****************************************************************************/

/* Read a single object in a keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int pkcs12ReadObject( INOUT STREAM *stream, 
					  OUT PKCS12_INFO *pkcs12info, 
					  const BOOLEAN isEncryptedCert,
					  INOUT ERROR_INFO *errorInfo )
	{
	PKCS12_OBJECT_INFO localPkcs12ObjectInfo, *pkcs12ObjectInfoPtr;
	STREAM objectStream;
	BOOLEAN isPrivateKey = FALSE;
	void *objectData;
	int objectLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12info, sizeof( PKCS12_INFO ) ) );
	
	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	memset( pkcs12info, 0, sizeof( PKCS12_INFO ) );

	/* Read the current object's data */
	status = readRawObjectAlloc( stream, &objectData, &objectLength,
								 MIN_OBJECT_SIZE, MAX_INTLENGTH_SHORT - 1 );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't read PKCS #12 object data" ) );
		}
	ANALYSER_HINT( objectData != NULL );

	/* Read the object information from the in-memory object data.  First we 
	   have to find out what it is that we're dealing with, caused by yet 
	   more PKCS #12 braindamage in which the same object types can be 
	   encapsulated in different ways in different locations.  The nesting 
	   is:

			Data
				SEQUENCE OF
					ShroundedKeyBag | CertBag	<- Current position
			
			EncryptedData
				Data (= Certificate)			<- Current position

	   with the current level being either the ShroundedKeyBag/CertBag or
	   Data.  If we're expecting Data (denoted by the isEncryptedCert flag
	   being set, the PKCS #12 braindamage leads to counterintuitive 
	   control-flag naming) then we read it as is, if we're expecting some
	   other content-type then we have to analyse it to see what we've got */
	sMemConnect( &objectStream, objectData, objectLength );
	if( isEncryptedCert )
		{
		/* We're reading a public certificate held within CMS EncryptedData, 
		   skip the encapsulation to get to the encryption information */
		readSequence( &objectStream, NULL );
		status = readFixedOID( &objectStream, OID_CMS_DATA, 
							   sizeofOID( OID_CMS_DATA ) );
		}
	else
		{
		int isEncryptedPrivateKey;

		/* We're reading either a private key held within a ShroudedKeyBag 
		   or a certificate within a CertBag, see what we've got.  As usual
		   with PKCS #12 there are complications, in this case because 
		   certificates are stored within a redundantly nested 
		   X509Certificate object within a CertBag object, so we have to 
		   read the outer CMS header with the READCMS_FLAG_WRAPPERONLY flag 
		   set to avoid reading the start of the inner header, which is then 
		   read by the second readCMSheader() call.  Since this skips the
		   normal read of the inner header, we have to explicitly read it if
		   it's not a CertBag */
		status = isEncryptedPrivateKey = 
			readCMSheader( &objectStream, keyCertBagOIDinfo, 
						   FAILSAFE_ARRAYSIZE( keyCertBagOIDinfo, OID_INFO ),
						   NULL, READCMS_FLAG_WRAPPERONLY );
		if( !cryptStatusError( status ) && isEncryptedPrivateKey )
			{
			isPrivateKey = TRUE;
			status = readSequence( &objectStream, NULL );
			}
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &objectStream );
		clFree( "readObject", objectData );
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #12 object header" ) );
		}

	/* Read the object data, either as an encrypted object if it's a private 
	   key or an encrypted certificate, or as plain data if it's a standard 
	   certificate */
	if( isEncryptedCert || isPrivateKey )
		{
		status = readEncryptedObjectInfo( &objectStream, 
										  &localPkcs12ObjectInfo, 
										  isEncryptedCert, errorInfo );
		}
	else
		{
		status = readObjectInfo( &objectStream, &localPkcs12ObjectInfo, 
								 errorInfo );
		}
	if( cryptStatusOK( status ) && stell( &objectStream ) < objectLength )
		{
		/* There are object attributes present, read these as well.  Note 
		   that these apply to the overall set of objects, so we read them
		   into the general information rather than the per-object 
		   information */
		status = readObjectAttributes( &objectStream, pkcs12info );
		}
	sMemDisconnect( &objectStream );
	if( cryptStatusError( status ) )
		{
		clFree( "readObject", objectData );
		retExt( status, 
				( status, errorInfo, "Invalid %s information",
				  isPrivateKey ? "private key" : "certificate" ) );
		}

	/* Remember the encoded object data */
	if( isEncryptedCert )
		pkcs12info->flags = PKCS12_FLAG_ENCCERT;
	else
		{
		if( isPrivateKey )
			pkcs12info->flags = PKCS12_FLAG_PRIVKEY;
		else
			pkcs12info->flags = PKCS12_FLAG_CERT;
		}
	pkcs12ObjectInfoPtr = isPrivateKey ? &pkcs12info->keyInfo : \
										 &pkcs12info->certInfo;
	memcpy( pkcs12ObjectInfoPtr, &localPkcs12ObjectInfo, 
			sizeof( PKCS12_OBJECT_INFO ) );
	pkcs12ObjectInfoPtr->data = objectData;
	pkcs12ObjectInfoPtr->dataSize = objectLength;
	ENSURES( rangeCheck( pkcs12ObjectInfoPtr->payloadOffset, 
						 pkcs12ObjectInfoPtr->payloadSize,
						 pkcs12ObjectInfoPtr->dataSize ) );

	return( CRYPT_OK );
	}
#endif /* USE_PKCS12 */
