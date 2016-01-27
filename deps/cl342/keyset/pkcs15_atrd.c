/****************************************************************************
*																			*
*					cryptlib PKCS #15 Attribute Read Routines				*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/* A macro to check that we're OK to read more data beyond this point */

#define canContinue( stream, status, endPos ) \
		( cryptStatusOK( status ) && stell( stream ) < endPos )

/* OID information used to read a PKCS #15 keyset */

static const OID_INFO FAR_BSS dataObjectOIDinfo[] = {
	{ OID_CRYPTLIB_CONTENTTYPE, TRUE },
	{ WILDCARD_OID, FALSE },
	{ NULL, 0 }, { NULL, 0 }
	};
static const OID_INFO FAR_BSS cryptlibDataOIDinfo[] = {
	{ OID_CRYPTLIB_CONFIGDATA, CRYPT_IATTRIBUTE_CONFIGDATA },
	{ OID_CRYPTLIB_USERINDEX, CRYPT_IATTRIBUTE_USERINDEX },
	{ OID_CRYPTLIB_USERINFO, CRYPT_IATTRIBUTE_USERINFO },
	{ WILDCARD_OID, CRYPT_ATTRIBUTE_NONE },
	{ NULL, 0 }, { NULL, 0 }
	};

/* Permitted object subtypes.  PKCS #15 uses context-specific tagging to 
   identify the subtypes within an object type so we store a list of
   permitted tags for each object type */

typedef struct {
	PKCS15_OBJECT_TYPE type;	/* Object type */
	int subTypes[ 7 ];			/* Subtype tags */
	} ALLOWED_ATTRIBUTE_TYPES;

static const ALLOWED_ATTRIBUTE_TYPES allowedTypesTbl[] = {
	{ PKCS15_OBJECT_PUBKEY,
	  { BER_SEQUENCE, MAKE_CTAG( CTAG_PK_ECC ), MAKE_CTAG( CTAG_PK_DH ), 
	    MAKE_CTAG( CTAG_PK_DSA ), 
		CRYPT_ERROR, CRYPT_ERROR } },
	{ PKCS15_OBJECT_PRIVKEY,
	  { BER_SEQUENCE, MAKE_CTAG( CTAG_PK_ECC ), MAKE_CTAG( CTAG_PK_DH ), 
	    MAKE_CTAG( CTAG_PK_DSA ), 
		CRYPT_ERROR, CRYPT_ERROR } },
	{ PKCS15_OBJECT_CERT,
	  { BER_SEQUENCE, CRYPT_ERROR, CRYPT_ERROR } },
	{ PKCS15_OBJECT_SECRETKEY,
	  { CRYPT_ERROR, CRYPT_ERROR } },
	{ PKCS15_OBJECT_DATA, 
	  { MAKE_CTAG( CTAG_DO_OIDDO ), CRYPT_ERROR, CRYPT_ERROR } },
	{ PKCS15_OBJECT_NONE, { CRYPT_ERROR, CRYPT_ERROR } },
		{ PKCS15_OBJECT_NONE, { CRYPT_ERROR, CRYPT_ERROR } }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Read a sequence of PKCS #15 key identifiers */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readKeyIdentifiers( INOUT STREAM *stream, 
							   INOUT PKCS15_INFO *pkcs15infoPtr,
							   IN_LENGTH const int endPos )
	{
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	
	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_INTLENGTH );

	for( status = CRYPT_OK, iterationCount = 0;
		 cryptStatusOK( status ) && stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_MED; iterationCount++ )
		{
		long value;
		int payloadLength;

		/* Read each identifier type and copy the useful ones into the PKCS
		   #15 information */
		readSequence( stream, &payloadLength );
		status = readShortInteger( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		switch( value )
			{
			case PKCS15_KEYID_ISSUERANDSERIALNUMBER:
				{
				HASHFUNCTION_ATOMIC hashFunctionAtomic;
				void *iAndSPtr = DUMMY_INIT_PTR;
				int iAndSLength, hashSize;

				/* If we've already got the iAndSID, use that version 
				   instead */
				if( pkcs15infoPtr->iAndSIDlength > 0 )
					{
					status = readUniversal( stream );
					continue;
					}

				/* Hash the full issuerAndSerialNumber to get an iAndSID */
				getHashAtomicParameters( CRYPT_ALGO_SHA1, 0,
										 &hashFunctionAtomic, &hashSize );
				status = getStreamObjectLength( stream, &iAndSLength );
				if( cryptStatusOK( status ) )
					status = sMemGetDataBlock( stream, &iAndSPtr, 
											   iAndSLength );
				if( cryptStatusOK( status ) )
					status = sSkip( stream, iAndSLength );
				if( cryptStatusError( status ) )
					return( status );
				hashFunctionAtomic( pkcs15infoPtr->iAndSID, KEYID_SIZE, 
									iAndSPtr, iAndSLength );
				pkcs15infoPtr->iAndSIDlength = hashSize;
				break;
				}

			case PKCS15_KEYID_SUBJECTKEYIDENTIFIER:
				status = readOctetString( stream, pkcs15infoPtr->keyID,
										  &pkcs15infoPtr->keyIDlength, 
										  8, CRYPT_MAX_HASHSIZE );
				break;

			case PKCS15_KEYID_ISSUERANDSERIALNUMBERHASH:
				/* If we've already got the iAndSID by hashing the
				   issuerAndSerialNumber, use that version instead */
				if( pkcs15infoPtr->iAndSIDlength > 0 )
					{
					status = readUniversal( stream );
					continue;
					}
				status = readOctetString( stream, pkcs15infoPtr->iAndSID,
										  &pkcs15infoPtr->iAndSIDlength, 
										  KEYID_SIZE, KEYID_SIZE );
				break;

			case PKCS15_KEYID_ISSUERNAMEHASH:
				status = readOctetString( stream, pkcs15infoPtr->issuerNameID,
										  &pkcs15infoPtr->issuerNameIDlength, 
										  KEYID_SIZE, KEYID_SIZE );
				break;

			case PKCS15_KEYID_SUBJECTNAMEHASH:
				status = readOctetString( stream, pkcs15infoPtr->subjectNameID,
										  &pkcs15infoPtr->subjectNameIDlength, 
										  KEYID_SIZE, KEYID_SIZE );
				break;

			case PKCS15_KEYID_PGP2:
				status = readOctetString( stream, pkcs15infoPtr->pgp2KeyID,
										  &pkcs15infoPtr->pgp2KeyIDlength, 
										  PGP_KEYID_SIZE, PGP_KEYID_SIZE );
				break;

			case PKCS15_KEYID_OPENPGP:
				status = readOctetString( stream, pkcs15infoPtr->openPGPKeyID,
										  &pkcs15infoPtr->openPGPKeyIDlength, 
										  PGP_KEYID_SIZE, PGP_KEYID_SIZE );
				break;

			default:
				status = readUniversal( stream );
			}
		}
	if( iterationCount >= FAILSAFE_ITERATIONS_MED )
		{
		/* This could be either an internal error or some seriously 
		   malformed data, since we can't tell without human intervention
		   we throw a debug exception but otherwise treat it as bad data */
		DEBUG_DIAG(( "Encountered more than %d key IDs", 
					 FAILSAFE_ITERATIONS_MED ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_BADDATA );
		}

	return( status );
	}

/****************************************************************************
*																			*
*							Read PKCS #15 Attributes						*
*																			*
****************************************************************************/

/* Read public/private key attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPubkeyAttributes( INOUT STREAM *stream, 
								 INOUT PKCS15_INFO *pkcs15infoPtr,
								 IN_LENGTH const int endPos, 
								 const BOOLEAN isPubKeyObject )
	{
	int usageFlags, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_INTLENGTH );

	status = readBitString( stream, &usageFlags );		/* Usage flags */
	if( canContinue( stream, status, endPos ) &&		/* Native flag */
		peekTag( stream ) == BER_BOOLEAN )
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&		/* Access flags */
		peekTag( stream ) == BER_BITSTRING )
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&		/* Key reference */
		peekTag( stream ) == BER_INTEGER )
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&		/* Start date */
		peekTag( stream ) == BER_TIME_GENERALIZED )
		status = readGeneralizedTime( stream, &pkcs15infoPtr->validFrom );
	if( canContinue( stream, status, endPos ) &&		/* End date */
		peekTag( stream ) == MAKE_CTAG( CTAG_KA_VALIDTO ) )
		status = readGeneralizedTimeTag( stream, &pkcs15infoPtr->validTo, 
										 CTAG_KA_VALIDTO );
	if( cryptStatusError( status ) )
		return( status );
	if( isPubKeyObject )
		pkcs15infoPtr->pubKeyUsage = usageFlags;
	else
		pkcs15infoPtr->privKeyUsage = usageFlags;

	return( CRYPT_OK );
	}

/* Read certificate attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCertAttributes( INOUT STREAM *stream, 
							   INOUT PKCS15_INFO *pkcs15infoPtr,
							   IN_LENGTH const int endPos )
	{
	int length, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_INTLENGTH );

	if( peekTag( stream ) == BER_BOOLEAN )			/* Authority flag */
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&	/* Identifier */
		peekTag( stream ) == BER_SEQUENCE )
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&	/* Thumbprint */
		peekTag( stream ) == MAKE_CTAG( CTAG_CA_DUMMY ) )
		status = readUniversal( stream );
	if( canContinue( stream, status, endPos ) &&	/* Trusted usage */
		peekTag( stream ) == MAKE_CTAG( CTAG_CA_TRUSTED_USAGE ) )
		{
		readConstructed( stream, NULL, CTAG_CA_TRUSTED_USAGE );
		status = readBitString( stream, &pkcs15infoPtr->trustedUsage );
		}
	if( canContinue( stream, status, endPos ) &&	/* Identifiers */
		peekTag( stream ) == MAKE_CTAG( CTAG_CA_IDENTIFIERS ) )
		{
		status = readConstructed( stream, &length, CTAG_CA_IDENTIFIERS );
		if( cryptStatusOK( status ) )
			status = readKeyIdentifiers( stream, pkcs15infoPtr, 
										 stell( stream ) + length );
		}
	if( canContinue( stream, status, endPos ) &&	/* Implicitly trusted */
		peekTag( stream ) == MAKE_CTAG_PRIMITIVE( CTAG_CA_TRUSTED_IMPLICIT ) )
		status = readBooleanTag( stream, &pkcs15infoPtr->implicitTrust,
								 CTAG_CA_TRUSTED_IMPLICIT );
	if( canContinue( stream, status, endPos ) &&	/* Validity */
		peekTag( stream ) == MAKE_CTAG( CTAG_CA_VALIDTO ) )
		{
		/* Due to miscommunication between PKCS #15 and 7816-15 there are 
		   two ways to encode the validity information for certificates, one 
		   based on the format used elsewhere in PKCS #15 (for PKCS #15) and 
		   the other based on the format used in certificates (for 7816-15).  
		   Luckily they can be distinguished by the tagging type */
		readConstructed( stream, NULL, CTAG_CA_VALIDTO );
		readUTCTime( stream, &pkcs15infoPtr->validFrom );
		status = readUTCTime( stream, &pkcs15infoPtr->validTo );
		}
	else
		{
		if( canContinue( stream, status, endPos ) &&	/* Start date */
			peekTag( stream ) == BER_TIME_GENERALIZED )
			status = readGeneralizedTime( stream, &pkcs15infoPtr->validFrom );
		if( canContinue( stream, status, endPos ) &&	/* End date */
			peekTag( stream ) == MAKE_CTAG_PRIMITIVE( CTAG_CA_VALIDTO ) )
			status = readGeneralizedTimeTag( stream, &pkcs15infoPtr->validTo,
											 CTAG_CA_VALIDTO );
		}

	return( status );
	}

/* Read an object's attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readObjectAttributes( INOUT STREAM *stream, 
						  INOUT PKCS15_INFO *pkcs15infoPtr,
						  IN_ENUM( PKCS15_OBJECT ) const PKCS15_OBJECT_TYPE type, 
						  INOUT ERROR_INFO *errorInfo )
	{
	const ALLOWED_ATTRIBUTE_TYPES *allowedTypeInfo;
	BOOLEAN skipDataRead = TRUE, isCryptlibData = FALSE;
	int length, outerLength, endPos, value, tag, i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );
	REQUIRES( errorInfo != NULL );

	/* Clear the return value */
	memset( pkcs15infoPtr, 0, sizeof( PKCS15_INFO ) );

	/* Find the allowed-subtype information for this object type */
	for( i = 0; \
		 allowedTypesTbl[ i ].type != type && \
			allowedTypesTbl[ i ].type != PKCS15_OBJECT_NONE && \
			i < FAILSAFE_ARRAYSIZE( allowedTypesTbl, ALLOWED_ATTRIBUTE_TYPES ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( allowedTypesTbl, ALLOWED_ATTRIBUTE_TYPES ) );
	allowedTypeInfo = &allowedTypesTbl[ i ];

	/* Make sure that this is a subtype that we can handle */
	tag = peekTag( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	status = readGenericHole( stream, &outerLength, MIN_OBJECT_SIZE, 
							  DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	for( i = 0; allowedTypeInfo->subTypes[ i ] != CRYPT_ERROR && \
				i < FAILSAFE_ITERATIONS_SMALL; i++ )
		{
		/* If this is a recognised subtype, process the attribute data */
		if( allowedTypeInfo->subTypes[ i ] == tag )
			{
			skipDataRead = FALSE;
			break;
			}
		}
	ENSURES( i < FAILSAFE_ITERATIONS_SMALL );

	/* Process the PKCS15CommonObjectAttributes */
	status = readSequence( stream, &length );
#if 0	/* 12/8/10 Only ever present in one pre-release product, now fixed */
	if( cryptStatusOK( status ) && outerLength == sizeofObject( length ) )
		{
		/* Due to a disagreement over IMPLICIT vs. EXPLICIT tagging for the 
		   parameterised types used in PKCS #15 based on an error that was 
		   in the specification up until version 1.2 (see section F.2 of the 
		   specification) some implementations have an extra layer of 
		   encapsulation after the type tag.  If the inner SEQUENCE fits 
		   exactly into the outer wrapper then the tagging used was EXPLICIT 
		   and we need to dig down another level */
		status = readSequence( stream, &length );
		}
#endif /* 0 */
	if( cryptStatusOK( status ) && length > 0 )
		{
		endPos = stell( stream ) + length;

		/* Read the label if it's present and skip anything else */
		if( peekTag( stream ) == BER_STRING_UTF8 )
			{
			status = readCharacterString( stream,
						pkcs15infoPtr->label, CRYPT_MAX_TEXTSIZE, 
						&pkcs15infoPtr->labelLength, BER_STRING_UTF8 );
			}
		if( canContinue( stream, status, endPos ) )
			status = sseek( stream, endPos );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 common object attributes" ) );
		}

	/* Process the PKCS15CommonXXXAttributes */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	switch( type )
		{
		case PKCS15_OBJECT_PUBKEY:
		case PKCS15_OBJECT_PRIVKEY:
			/* It's a public/private-key object, read the ID and assorted 
			   flags */
			status = readOctetString( stream, pkcs15infoPtr->iD,
									  &pkcs15infoPtr->iDlength, 
									  1, CRYPT_MAX_HASHSIZE );
			if( cryptStatusOK( status ) && \
				canContinue( stream, status, endPos ) )
				{
				status = readPubkeyAttributes( stream, pkcs15infoPtr, 
											endPos,
											( type == PKCS15_OBJECT_PUBKEY ) ? \
											TRUE : FALSE );
				}
			if( cryptStatusError( status ) )
				{
				retExt( status, 
						( status, errorInfo, 
						  "Invalid PKCS #15 public/private-key "
						  "attributes" ) );
				}
			break;

		case PKCS15_OBJECT_CERT:
			/* It's a certificate object, read the ID and assorted flags */
			status = readOctetString( stream, pkcs15infoPtr->iD,
									  &pkcs15infoPtr->iDlength, 
									  1, CRYPT_MAX_HASHSIZE );
			if( cryptStatusOK( status ) && \
				canContinue( stream, status, endPos ) )
				{
				status = readCertAttributes( stream, pkcs15infoPtr, 
											 endPos );
				}
			if( cryptStatusError( status ) )
				{
				retExt( status, 
						( status, errorInfo, 
						  "Invalid PKCS #15 certificate attributes" ) );
				}
			break;

		case PKCS15_OBJECT_SECRETKEY:
			/* It's a secret-key object, there are no common attributes of interest 
			   present */
			break;

		case PKCS15_OBJECT_DATA:
			/* If it's a data object then all of the attributes are 
			   optional.  If it's specifically a cryptlib data object then 
			   it'll be identified via the cryptlib OID */
			if( length <= 0 )
				break;
			if( peekTag( stream ) == BER_STRING_UTF8 )
				status = readUniversal( stream );	/* Skip application name */
			if( canContinue( stream, status, endPos ) && \
				peekTag( stream ) == BER_OBJECT_IDENTIFIER )
				{
				status = readOID( stream, dataObjectOIDinfo, 
								  FAILSAFE_ARRAYSIZE( dataObjectOIDinfo, \
													  OID_INFO ), &value );
				if( cryptStatusOK( status ) && value == TRUE )
					isCryptlibData = TRUE;
				}
			break;
		
		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 common type attributes" ) );
		}

	/* Skip any additional attribute information that may be present */
	if( stell( stream ) < endPos )
		{
		status = sseek( stream, endPos );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* For now we use the iD as the keyID, this may be overridden later if
	   there's a real keyID present */
	if( pkcs15infoPtr->iDlength > 0 )
		{
		memcpy( pkcs15infoPtr->keyID, pkcs15infoPtr->iD, 
				pkcs15infoPtr->iDlength );
		pkcs15infoPtr->keyIDlength = pkcs15infoPtr->iDlength;
		}

	/* Skip the subclass attributes if present */
	if( peekTag( stream ) == MAKE_CTAG( CTAG_OB_SUBCLASSATTR ) )
		{
		status = readUniversal( stream );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Invalid PKCS #15 subclass attributes" ) );
			}
		}

	/* Process the type attributes, which just consists of remembering where
	   the payload starts */
	readConstructed( stream, NULL, CTAG_OB_TYPEATTR );
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	if( skipDataRead )
		{
		/* It's a non-recognised object subtype, skip it */
		return( ( stell( stream ) < endPos ) ? \
				sseek( stream, endPos ) : CRYPT_OK );
		}
	if( peekTag( stream ) == MAKE_CTAG( CTAG_OV_DIRECT ) )
		{
		/* Parameterised types have special tagging requirements when using 
		   context-specific tags and the declaration is a "Tag Type" (for 
		   example for the "direct" choice for the ObjectValue type) and the 
		   "Type" in the "Tag Type" is a "DummyReference".  In this case the 
		   context tag is encoded as an EXPLICIT rather than IMPLCIIT tag 
		   (see section F.2 of PKCS #15 v1.2 and newer).  The only case where
		   this occurs is for the ObjectValue.direct option.
		   
		   This is complicated by the fact that versions of PKCS #15 before 
		   v1.2 erroneously stated that all context-specific tags in 
		   parameterised types should use EXPLICIT tagging, however no 
		   (known) implementation ever did this.
		   
		   What this double error means is that existing implementations get 
		   things almost right, the exception being ObjectValue.direct, which
		   does require an EXPLICIT tag.  To deal with this, we check for the
		   presence of the optional tag and skip it if it's present */
		status = readConstructed( stream, &length, CTAG_OV_DIRECT );
		if( cryptStatusError( status ) )
			return( status );
		}
	switch( type )
		{
		case PKCS15_OBJECT_PUBKEY:
			pkcs15infoPtr->pubKeyOffset = stell( stream );
			break;

		case PKCS15_OBJECT_PRIVKEY:
			pkcs15infoPtr->privKeyOffset = stell( stream );
			break;

		case PKCS15_OBJECT_CERT:
			pkcs15infoPtr->certOffset = stell( stream );
			break;

		case PKCS15_OBJECT_SECRETKEY:
			pkcs15infoPtr->secretKeyOffset = stell( stream );
			break;

		case PKCS15_OBJECT_DATA:
			/* If it's not cryptlib data, we can't do much with it */
			if( !isCryptlibData )
				break;

			/* It's a cryptlib data object, extract the contents */
			status = readOID( stream, cryptlibDataOIDinfo, 
							  FAILSAFE_ARRAYSIZE( cryptlibDataOIDinfo, \
												  OID_INFO ), 
							  &value );
			if( cryptStatusOK( status ) && \
				( value == CRYPT_IATTRIBUTE_CONFIGDATA || \
				  value == CRYPT_IATTRIBUTE_USERINDEX ) )
				{
				/* The configuration data and user index are SEQUENCEs of 
				   objects */
				status = readSequence( stream, NULL );
				}
			if( cryptStatusError( status ) )
				break;
			if( value == CRYPT_ATTRIBUTE_NONE )
				{
				/* It's a non-recognised cryptlib data subtype, skip it */
				break;
				}
			pkcs15infoPtr->dataOffset = stell( stream );
			pkcs15infoPtr->dataType = value;
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 type attributes" ) );
		}

	/* Skip the object data and any additional attribute information that 
	   may be present */
	if( stell( stream ) < endPos )
		return( sseek( stream, endPos ) );

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
