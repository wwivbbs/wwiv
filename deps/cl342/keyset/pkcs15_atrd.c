/****************************************************************************
*																			*
*					cryptlib PKCS #15 Attribute Read Routines				*
*						Copyright Peter Gutmann 1996-2014					*
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
			  endPos < MAX_BUFFER_SIZE );

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
				void *iAndSPtr DUMMY_INIT_PTR;
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
					{
					status = sSkip( stream, iAndSLength, 
									MAX_INTLENGTH_SHORT );
					}
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
	int usageFlags, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_BUFFER_SIZE );

	status = readBitString( stream, &usageFlags );			/* Usage flags */
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_BOOLEAN )								/* Native flag */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_BITSTRING )								/* Access flags */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_INTEGER )								/* Key reference */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_TIME_GENERALIZED )						/* Start date */
		status = readGeneralizedTime( stream, &pkcs15infoPtr->validFrom );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG( CTAG_KA_VALIDTO ) )				/* End date */
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
	int tag, length, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_BUFFER_SIZE );

	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == BER_BOOLEAN )								/* Authority flag */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == BER_SEQUENCE )								/* Identifier */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG( CTAG_CA_DUMMY ) )					/* Thumbprint */
		status = readUniversal( stream );
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG( CTAG_CA_TRUSTED_USAGE ) )			/* Trusted usage */
		{
		readConstructed( stream, NULL, CTAG_CA_TRUSTED_USAGE );
		status = readBitString( stream, &pkcs15infoPtr->trustedUsage );
		}
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG( CTAG_CA_IDENTIFIERS ) )			/* Identifiers */
		{
		status = readConstructed( stream, &length, CTAG_CA_IDENTIFIERS );
		if( cryptStatusOK( status ) && length > 0 )
			{
			status = readKeyIdentifiers( stream, pkcs15infoPtr, 
										 stell( stream ) + length );
			}
		}
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG_PRIMITIVE( CTAG_CA_TRUSTED_IMPLICIT ) )
		{													/* Implicitly trusted */
		status = readBooleanTag( stream, &pkcs15infoPtr->implicitTrust,
								 CTAG_CA_TRUSTED_IMPLICIT );
		}
	if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
		tag == MAKE_CTAG( CTAG_CA_VALIDTO ) )				/* Validity */
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
		if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
			tag == BER_TIME_GENERALIZED )					/* Start date */
			status = readGeneralizedTime( stream, &pkcs15infoPtr->validFrom );
		if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
			tag == MAKE_CTAG_PRIMITIVE( CTAG_CA_VALIDTO ) )	/* End date */
			status = readGeneralizedTimeTag( stream, &pkcs15infoPtr->validTo,
											 CTAG_CA_VALIDTO );
		}

	return( cryptStatusError( status ) ? status : CRYPT_OK );
	}		/* checkStatusLimitsPeekTag() can return tag as status */

/* Read an object's attributes */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readClassAttributes( INOUT STREAM *stream, 
								INOUT PKCS15_INFO *pkcs15infoPtr,
								IN_ENUM( PKCS15_OBJECT ) \
									const PKCS15_OBJECT_TYPE type )
	{
	BOOLEAN isCryptlibObject = FALSE;
	int tag, length, endPos, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );

	/* Read the attribute wrapper */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Process per-object-type attributes */
	switch( type )
		{
		case PKCS15_OBJECT_PUBKEY:
		case PKCS15_OBJECT_PRIVKEY:
			/* It's a public/private-key object, read the ID and assorted 
			   flags */
			if( length < sizeofObject( MIN_NAME_LENGTH ) )
				return( CRYPT_ERROR_BADDATA );
			status = readOctetString( stream, pkcs15infoPtr->iD,
									  &pkcs15infoPtr->iDlength, 
									  MIN_NAME_LENGTH, CRYPT_MAX_HASHSIZE );
			if( cryptStatusOK( status ) && stell( stream ) < endPos )
				{
				status = readPubkeyAttributes( stream, pkcs15infoPtr, 
											endPos,
											( type == PKCS15_OBJECT_PUBKEY ) ? \
											TRUE : FALSE );
				}
			break;

		case PKCS15_OBJECT_CERT:
			/* It's a certificate object, read the ID and assorted flags */
			if( length < sizeofObject( MIN_NAME_LENGTH ) )
				return( CRYPT_ERROR_BADDATA );
			status = readOctetString( stream, pkcs15infoPtr->iD,
									  &pkcs15infoPtr->iDlength, 
									  MIN_NAME_LENGTH, CRYPT_MAX_HASHSIZE );
			if( cryptStatusOK( status ) && stell( stream ) < endPos )
				{
				status = readCertAttributes( stream, pkcs15infoPtr, 
											 endPos );
				}
			break;

		case PKCS15_OBJECT_SECRETKEY:
			/* It's a secret-key object, there are no attributes of interest 
			   present */
			break;

		case PKCS15_OBJECT_DATA:
			/* If it's a data object then all of the attributes are 
			   optional.  If it's specifically a cryptlib data object then 
			   it'll be identified via the cryptlib OID */
			if( length <= 0 )
				return( CRYPT_OK );
			if( checkStatusPeekTag( stream, status, tag ) && \
				tag == BER_STRING_UTF8 )
				{
				/* Skip application name */
				status = readUniversal( stream );
				}
			if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
				tag == BER_OBJECT_IDENTIFIER )
				{
				int value;

				status = readOID( stream, dataObjectOIDinfo, 
								  FAILSAFE_ARRAYSIZE( dataObjectOIDinfo, \
													  OID_INFO ), &value );
				if( cryptStatusOK( status ) && value == TRUE )
					isCryptlibObject = TRUE;
				}
			break;

		case PKCS15_OBJECT_UNRECOGNISED:
			/* It's an unrecognised object type, we don't know what to do 
			   with any attributes that may be present */
			break;
		
		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Skip any additional attribute information that may be present */
	if( stell( stream ) < endPos )
		{
		status = sseek( stream, endPos );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( isCryptlibObject ? OK_SPECIAL : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readSubclassAttributes( INOUT STREAM *stream, 
								   INOUT PKCS15_INFO *pkcs15infoPtr,
								   IN_ENUM( PKCS15_OBJECT ) \
										const PKCS15_OBJECT_TYPE type )
	{
	int tag, length, endPos, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );

	/* Read the attribute wrapper */
	readConstructed( stream, NULL, CTAG_OB_SUBCLASSATTR );
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	switch( type )
		{
		case PKCS15_OBJECT_PRIVKEY:
			if( checkStatusPeekTag( stream, status, tag ) && \
				tag == BER_SEQUENCE )						/* Name */
				status = readUniversal( stream );
			if( checkStatusLimitsPeekTag( stream, status, tag, endPos ) && \
				tag == MAKE_CTAG( CTAG_PK_IDENTIFIERS ) )	/* KeyIDs */
				{
				status = readConstructed( stream, &length, 
										  CTAG_PK_IDENTIFIERS );
				if( cryptStatusOK( status ) && length > 0 )
					{
					status = readKeyIdentifiers( stream, pkcs15infoPtr, 
												 stell( stream ) + length );
					}
				}
			break;

		case PKCS15_OBJECT_PUBKEY:
		case PKCS15_OBJECT_CERT:
		case PKCS15_OBJECT_SECRETKEY:
		case PKCS15_OBJECT_DATA:
			/* These object types don't have subclass attributes */
			status = CRYPT_ERROR_BADDATA;
			break;

		case PKCS15_OBJECT_UNRECOGNISED:
			/* It's an unrecognised object type, we don't know what to do 
			   with any attributes that may be present */
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Skip any additional attribute information that may be present */
	if( stell( stream ) < endPos )
		{
		status = sseek( stream, endPos );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readTypeAttributes( INOUT STREAM *stream, 
							   INOUT PKCS15_INFO *pkcs15infoPtr,
							   IN_ENUM( PKCS15_OBJECT ) \
									const PKCS15_OBJECT_TYPE type,
							   const BOOLEAN unrecognisedAttribute,
							   const BOOLEAN isCryptlibData )
	{
	int tag, length, endPos, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );

	/* Read the attribute wrapper */	
	readConstructed( stream, NULL, CTAG_OB_TYPEATTR );
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	if( unrecognisedAttribute )
		{
		/* It's a non-recognised object subtype, skip it */
		return( ( stell( stream ) < endPos ) ? \
				sseek( stream, endPos ) : CRYPT_OK );
		}

	/* Parameterised types have special tagging requirements when using 
	   context-specific tags and the declaration is a "Tag Type" (for 
	   example for the "direct" choice for the ObjectValue type) and the 
	   "Type" in the "Tag Type" is a "DummyReference".  In this case the 
	   context tag is encoded as an EXPLICIT rather than IMPLCIIT tag (see 
	   section F.2 of PKCS #15 v1.2 and newer).  The only case where this 
	   occurs is for the ObjectValue.direct option.
		   
	   This is complicated by the fact that versions of PKCS #15 before v1.2 
	   erroneously stated that all context-specific tags in parameterised 
	   types should use EXPLICIT tagging, however no (known) implementation 
	   ever did this.
		   
	   What this double error means is that existing implementations get 
	   things almost right, the exception being ObjectValue.direct, which 
	   does require an EXPLICIT tag.  To deal with this, we check for the 
	   presence of the optional tag and skip it if it's present */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( CTAG_OV_DIRECT ) )
		{
		status = readConstructed( stream, &length, CTAG_OV_DIRECT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the payload data, which just consists of remembering where the 
	   payload starts */
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
			{
			int value;

			/* If it's not cryptlib data, we can't do much with it */
			if( !isCryptlibData )
				break;

			/* It's a cryptlib data object, extract the contents */
			status = readOID( stream, cryptlibDataOIDinfo, 
							  FAILSAFE_ARRAYSIZE( cryptlibDataOIDinfo, \
												  OID_INFO ), 
							  &value );
			if( cryptStatusError( status ) )
				return( status );
			if( value == CRYPT_IATTRIBUTE_CONFIGDATA || \
				value == CRYPT_IATTRIBUTE_USERINDEX )
				{
				/* The configuration data and user index are SEQUENCEs of 
				   objects */
				status = readSequence( stream, NULL );
				if( cryptStatusError( status ) )
					return( status );
				}
			if( value == CRYPT_ATTRIBUTE_NONE )
				{
				/* It's a non-recognised cryptlib data subtype, skip it */
				break;
				}
			pkcs15infoPtr->dataOffset = stell( stream );
			pkcs15infoPtr->dataType = value;
			break;
			}

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Skip the data payload */
	if( stell( stream ) < endPos )
		{
		status = sseek( stream, endPos );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readObjectAttributes( INOUT STREAM *stream, 
						  INOUT PKCS15_INFO *pkcs15infoPtr,
						  IN_ENUM( PKCS15_OBJECT ) \
								const PKCS15_OBJECT_TYPE type, 
						  INOUT ERROR_INFO *errorInfo )
	{
	const ALLOWED_ATTRIBUTE_TYPES *allowedTypeInfo;
	BOOLEAN skipDataRead = TRUE, isCryptlibData = FALSE;
	int tag, length, outerLength, lastTag = CRYPT_ERROR, i, status;

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
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
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
		lastTag = allowedTypeInfo->subTypes[ i ];
		}
	ENSURES( i < FAILSAFE_ITERATIONS_SMALL );

	/* If this is an unrecognised subtype, make sure that it's at least 
	   vaguely valid.  Tags are in the range { BER_SEQUENCE, [0]...[n] }, 
	   we interpret "vaguely valid" to mean within a short distance of the 
	   last tag [n] that we recognise as valid */
	if( skipDataRead && ( tag != BER_SEQUENCE ) && \
		( tag < MAKE_CTAG( 0 ) || tag > lastTag + 10 ) )
		return( CRYPT_ERROR_BADDATA );

	/* Process the PKCS15CommonObjectAttributes */
	status = readSequence( stream, &length );
	if( cryptStatusOK( status ) && length > 0 )
		{
		const int endPos = stell( stream ) + length;

		/* Read the label if it's present and skip anything else */
		if( checkStatusPeekTag( stream, status, tag ) && \
			tag == BER_STRING_UTF8 )
			{
			status = readCharacterString( stream,
						pkcs15infoPtr->label, CRYPT_MAX_TEXTSIZE, 
						&pkcs15infoPtr->labelLength, BER_STRING_UTF8 );
			}
		if( !cryptStatusError( status ) && stell( stream ) < endPos )
			status = sseek( stream, endPos );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 common object attributes" ) );
		}

	/* Process the class attributes */
	status = readClassAttributes( stream, pkcs15infoPtr, type );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 %s attributes",
				  ( type == PKCS15_OBJECT_PUBKEY ) ? "public key" : \
				  ( type == PKCS15_OBJECT_PRIVKEY ) ? "private key" : \
				  ( type == PKCS15_OBJECT_CERT ) ? "certificate" : \
				  ( type == PKCS15_OBJECT_DATA ) ? "data object" : \
												   "class" ) );
		}
	if( status == OK_SPECIAL )
		isCryptlibData = TRUE;

	/* We have to have at least an ID present for any standard object 
	   types */
	ENSURES( ( type == PKCS15_OBJECT_SECRETKEY || \
			   type == PKCS15_OBJECT_DATA || \
			   type == PKCS15_OBJECT_UNRECOGNISED ) || \
			 pkcs15infoPtr->iDlength > 0 );

	/* If there's no keyID present then we use the iD as the keyID */
	if( pkcs15infoPtr->keyIDlength <= 0 && pkcs15infoPtr->iDlength > 0 )
		{
		memcpy( pkcs15infoPtr->keyID, pkcs15infoPtr->iD, 
				pkcs15infoPtr->iDlength );
		pkcs15infoPtr->keyIDlength = pkcs15infoPtr->iDlength;
		}

	/* Skip the subclass attributes if present */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( CTAG_OB_SUBCLASSATTR ) )
		{
		status = readSubclassAttributes( stream, pkcs15infoPtr, type );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Invalid PKCS #15 %s attributes",
					  ( type == PKCS15_OBJECT_PRIVKEY ) ? "private key" : \
														  "subclass" ) );
			}
		}

	/* Process the type attributes */
	status = readTypeAttributes( stream, pkcs15infoPtr, type, 
								 skipDataRead, isCryptlibData );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PKCS #15 %s payload data",
				  ( type == PKCS15_OBJECT_PUBKEY ) ? "public key" : \
				  ( type == PKCS15_OBJECT_PRIVKEY ) ? "private key" : \
				  ( type == PKCS15_OBJECT_CERT ) ? "certificate" : \
				  ( type == PKCS15_OBJECT_DATA ) ? "data object" : \
												   "class" ) );
		}

	return( skipDataRead ? OK_SPECIAL : CRYPT_OK );
	}
#endif /* USE_PKCS15 */
