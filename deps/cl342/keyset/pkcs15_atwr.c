/****************************************************************************
*																			*
*					cryptlib PKCS #15 Attribute Write Routines				*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Get assorted ID information from a context or certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getKeyIDs( INOUT PKCS15_INFO *pkcs15infoPtr,
					  IN_HANDLE const CRYPT_HANDLE iCryptContext )
	{
	MESSAGE_DATA msgData;
	BYTE sKIDbuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Get various pieces of information from the object.  The information
	   may already have been set up earlier on so we only set it if this is
	   a newly-added key.  We use a guard for the existence of both a label
	   and an ID since there may be a pre-set user ID (which isn't the same
	   as the key ID) present for implicitly created keys in user keysets */
	if( pkcs15infoPtr->labelLength <= 0 )
		{
		setMessageData( &msgData, pkcs15infoPtr->label, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusError( status ) )
			return( status );
		pkcs15infoPtr->labelLength = msgData.length;
		setMessageData( &msgData, pkcs15infoPtr->keyID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		if( cryptStatusError( status ) )
			return( status );
		pkcs15infoPtr->keyIDlength = msgData.length;
		}
	if( pkcs15infoPtr->iDlength <= 0 && pkcs15infoPtr->keyIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->iD, pkcs15infoPtr->keyID, 
				pkcs15infoPtr->keyIDlength );
		pkcs15infoPtr->iDlength = pkcs15infoPtr->keyIDlength;
		}
	if( pkcs15infoPtr->pgp2KeyIDlength <= 0 )
		{
		setMessageData( &msgData, pkcs15infoPtr->pgp2KeyID, PGP_KEYID_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID_PGP2 );
		if( cryptStatusOK( status ) )
			{
			/* Not present for all key types so an error isn't fatal */
			pkcs15infoPtr->pgp2KeyIDlength = msgData.length;
			}
		}
	if( pkcs15infoPtr->openPGPKeyIDlength <= 0 )
		{
		setMessageData( &msgData, pkcs15infoPtr->openPGPKeyID, PGP_KEYID_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
		if( cryptStatusOK( status ) )
			{
			/* Not present for all key types so an error isn't fatal */
			pkcs15infoPtr->openPGPKeyIDlength = msgData.length;
			}
		}

	/* The subjectKeyIdentifier, if present, may not be the same as the 
	   keyID if the certificate that it's in has come from a CA that does 
	   strange things with the sKID so we try and read this value and if 
	   it's present override the implicit sKID (== keyID) value with the 
	   actual sKID */
	setMessageData( &msgData, sKIDbuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
	if( cryptStatusOK( status ) )
		{
		memcpy( pkcs15infoPtr->keyID, sKIDbuffer, msgData.length );
		pkcs15infoPtr->keyIDlength = msgData.length;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int getCertIDs( INOUT PKCS15_INFO *pkcs15infoPtr, 
					   IN_HANDLE const CRYPT_HANDLE iCryptCert, 
					   OUT BOOLEAN *isCA, 
					   OUT BOOLEAN *trustedImplicit, 
					   OUT int *trustedUsage )
	{
	int status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( isCA, sizeof( BOOLEAN ) ) );
	assert( isWritePtr( trustedImplicit, sizeof( BOOLEAN ) ) );
	assert( isWritePtr( trustedUsage, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* Clear return values */
	*isCA = *trustedImplicit = FALSE;
	*trustedUsage = CRYPT_UNUSED;

	/* Get various pieces of status information from the certificate */
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE, isCA,
							  CRYPT_CERTINFO_CA );
	if( status == CRYPT_ERROR_NOTFOUND )
		{
		*isCA = FALSE;
		status = CRYPT_OK;
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE,
								  trustedUsage, CRYPT_CERTINFO_TRUSTED_USAGE );
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			/* If there's no trusted usage defined, don't store a trust
			   setting */
			*trustedUsage = CRYPT_UNUSED;
			status = CRYPT_OK;
			}
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE,
							&trustedImplicit, CRYPT_CERTINFO_TRUSTED_IMPLICIT );
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			/* If it's not implicitly trusted, don't store a trust setting */
			*trustedImplicit = FALSE;
			status = CRYPT_OK;
			}
		}
	if( cryptStatusOK( status ) )
		status = getValidityInfo( pkcs15infoPtr, iCryptCert );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're adding a standalone certificate then the iD and keyID won't 
	   have been set up yet so we need to set these up as well.  Since the 
	   certificate could be a data-only one we create the iD ourselves from 
	   the encoded public key components rather than trying to read an 
	   associated context's keyID attribute.  For similar reasons we 
	   specifically don't try and read the PGP ID information since for a 
	   certificate chain it'll come from the context of the leaf certificate 
	   rather than the current certificate (in any case they're not 
	   necessary since none of the certificates in the chain will be PGP 
	   keys) */
	if( pkcs15infoPtr->iDlength <= 0 )
		{
		status = getCertID( iCryptCert, CRYPT_IATTRIBUTE_SPKI,
							pkcs15infoPtr->iD, KEYID_SIZE, 
							&pkcs15infoPtr->iDlength );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( pkcs15infoPtr->keyIDlength <= 0 )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, pkcs15infoPtr->keyID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
		if( cryptStatusOK( status ) )
			pkcs15infoPtr->keyIDlength = msgData.length;
		else
			{
			memcpy( pkcs15infoPtr->keyID, pkcs15infoPtr->iD, 
					pkcs15infoPtr->iDlength );
			pkcs15infoPtr->keyIDlength = pkcs15infoPtr->iDlength;
			}
		}

	/* Get the various other IDs for the certificate */
	status = getCertID( iCryptCert, CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER,
						pkcs15infoPtr->iAndSID, KEYID_SIZE,
						&pkcs15infoPtr->iAndSIDlength );
	if( cryptStatusOK( status ) )
		status = getCertID( iCryptCert, CRYPT_IATTRIBUTE_SUBJECT,
							pkcs15infoPtr->subjectNameID, KEYID_SIZE,
							&pkcs15infoPtr->subjectNameIDlength );
	if( cryptStatusOK( status ) )
		status = getCertID( iCryptCert, CRYPT_IATTRIBUTE_ISSUER,
							pkcs15infoPtr->issuerNameID, KEYID_SIZE,
							&pkcs15infoPtr->issuerNameIDlength );
	return( status );
	}

/* Get the PKCS #15 key usage flags for a context */

CHECK_RETVAL \
static int getKeyUsageFlags( IN_HANDLE const CRYPT_HANDLE iCryptContext,
							 IN_FLAGS( PKCS15_USAGE ) const int privKeyUsage )
	{
	MESSAGE_DATA msgData;
	int keyUsage = PKSC15_USAGE_FLAG_NONE, value, status;

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( privKeyUsage >= PKSC15_USAGE_FLAG_NONE && \
			  privKeyUsage < PKCS15_USAGE_FLAG_MAX );

	/* There's one special-case situation in which there won't be any usage
	   information available and that's when we've been passed a dummy 
	   context that's used to contain key metadata for a crypto device.  If 
	   this is the case, we allow any usage that the algorithm allows, it's
	   up to the device (which we don't have any control over) to set more 
	   specific restrictions */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_DEVICESTORAGEID );
	if( cryptStatusOK( status ) )
		{
		int pkcAlgo;
		
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &pkcAlgo, CRYPT_CTXINFO_ALGO );
		if( cryptStatusError( status ) )
			return( status );
		switch( pkcAlgo )
			{
			case CRYPT_ALGO_DH:
				return( PKCS15_USAGE_DERIVE );

			case CRYPT_ALGO_RSA:
				return( PKCS15_USAGE_ENCRYPT | PKCS15_USAGE_DECRYPT | \
						PKCS15_USAGE_SIGN | PKCS15_USAGE_VERIFY );

			case CRYPT_ALGO_DSA:
				return( PKCS15_USAGE_SIGN | PKCS15_USAGE_VERIFY );

			case CRYPT_ALGO_ELGAMAL:
				return( PKCS15_USAGE_ENCRYPT | PKCS15_USAGE_DECRYPT );
			}
		retIntError();
		}

	/* Obtaining the usage flags gets a bit complicated because they're a 
	   mixture of parts of X.509 and PKCS #11 flags (and the X.509 -> PKCS 
	   #15 mapping isn't perfect, see for example key agreement) so we have 
	   to build them up from bits and pieces pulled in from all over the 
	   place */
	if( cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_ENCRYPT ) ) )
		keyUsage = PKCS15_USAGE_ENCRYPT;
	if( cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_DECRYPT ) ) )
		keyUsage |= PKCS15_USAGE_DECRYPT;
	if( cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_SIGN ) ) )
		keyUsage |= PKCS15_USAGE_SIGN;
	if( cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_SIGCHECK ) ) )
		keyUsage |= PKCS15_USAGE_VERIFY;
	if( cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_KA_EXPORT ) ) || \
		cryptStatusOK( krnlSendMessage( iCryptContext, IMESSAGE_CHECK,
										NULL, MESSAGE_CHECK_PKC_KA_IMPORT ) ) )
		keyUsage |= PKCS15_USAGE_DERIVE;	/* I don't think so Tim */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, &value,
							  CRYPT_CERTINFO_KEYUSAGE );
	if( cryptStatusOK( status ) && \
		( value & CRYPT_KEYUSAGE_NONREPUDIATION ) )
		{
		/* This may be a raw key or a certificate with no keyUsage present 
		   so a failure to read the usage attribute isn't a problem */
		keyUsage |= PKCS15_USAGE_NONREPUDIATION;
		}

	/* If the key ends up being unusable, tell the caller */
	if( keyUsage <= PKSC15_USAGE_FLAG_NONE )
		return( 0 );

	/* If this is a public-key object which is updating a private-key one
	   then the only key usages that we'll have found are public-key ones.  
	   To ensure that we don't disable use of the private-key object we copy 
	   across private-key usages where corresponding public-key ones are 
	   enabled.  This is used, for example, when updating an unrestricted-
	   usage raw private key with a restricted-usage public key, e.g. from a
	   certificate */
	if( cryptStatusError( krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
										   MESSAGE_CHECK_PKC_PRIVATE ) ) )
		{
		if( keyUsage & PKCS15_USAGE_ENCRYPT )
			keyUsage |= privKeyUsage & PKCS15_USAGE_DECRYPT;
		if( keyUsage & PKCS15_USAGE_VERIFY )
			keyUsage |= privKeyUsage & PKCS15_USAGE_SIGN;
		}

	return( keyUsage );
	}

/****************************************************************************
*																			*
*							Write PKCS #15 Attributes						*
*																			*
****************************************************************************/

/* Write PKCS #15 identifier values */

CHECK_RETVAL_RANGE( 0, MAX_INTLENGTH_SHORT ) STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofObjectIDs( const PKCS15_INFO *pkcs15infoPtr )
	{
	int identifierSize;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	identifierSize = ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_SUBJECTKEYIDENTIFIER ) + \
				sizeofObject( pkcs15infoPtr->keyIDlength ) );
	if( pkcs15infoPtr->iAndSIDlength > 0 )
		identifierSize += ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_ISSUERANDSERIALNUMBERHASH ) + \
				sizeofObject( pkcs15infoPtr->iAndSIDlength ) );
	if( pkcs15infoPtr->issuerNameIDlength > 0 )
		identifierSize += ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_ISSUERNAMEHASH ) + \
				sizeofObject( pkcs15infoPtr->issuerNameIDlength ) );
	if( pkcs15infoPtr->subjectNameIDlength > 0 )
		identifierSize += ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_SUBJECTNAMEHASH ) + \
				sizeofObject( pkcs15infoPtr->subjectNameIDlength ) );
	if( pkcs15infoPtr->pgp2KeyIDlength > 0 )
		identifierSize += ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_PGP2 ) + \
				sizeofObject( pkcs15infoPtr->pgp2KeyIDlength ) );
	if( pkcs15infoPtr->openPGPKeyIDlength > 0 )
		identifierSize += ( int ) \
			sizeofObject( \
				sizeofShortInteger( PKCS15_KEYID_OPENPGP ) + \
				sizeofObject( pkcs15infoPtr->openPGPKeyIDlength ) );

	return( identifierSize );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeObjectIDs( INOUT STREAM *stream, 
						   const PKCS15_INFO *pkcs15infoPtr,
						   IN_LENGTH_SHORT_MIN( MIN_OBJECT_SIZE ) const int length, 
						   IN_TAG const int tag )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	
	REQUIRES( length >= MIN_OBJECT_SIZE && length < MAX_INTLENGTH_SHORT );
	REQUIRES( tag >= 0 && tag < MAX_TAG_VALUE );

	writeConstructed( stream, length, tag );
	writeSequence( stream,
				   sizeofShortInteger( PKCS15_KEYID_SUBJECTKEYIDENTIFIER ) + \
				   sizeofObject( pkcs15infoPtr->keyIDlength ) );
	writeShortInteger( stream, PKCS15_KEYID_SUBJECTKEYIDENTIFIER,
					   DEFAULT_TAG );
	status = writeOctetString( stream, pkcs15infoPtr->keyID,
							   pkcs15infoPtr->keyIDlength, DEFAULT_TAG );
	if( pkcs15infoPtr->iAndSIDlength > 0 )
		{
		writeSequence( stream,
					   sizeofShortInteger( PKCS15_KEYID_ISSUERANDSERIALNUMBERHASH ) + \
					   sizeofObject( pkcs15infoPtr->iAndSIDlength ) );
		writeShortInteger( stream, PKCS15_KEYID_ISSUERANDSERIALNUMBERHASH,
						   DEFAULT_TAG );
		status = writeOctetString( stream, pkcs15infoPtr->iAndSID,
								   pkcs15infoPtr->iAndSIDlength, 
								   DEFAULT_TAG );
		}
	if( pkcs15infoPtr->issuerNameIDlength > 0 )
		{
		writeSequence( stream,
					   sizeofShortInteger( PKCS15_KEYID_ISSUERNAMEHASH ) + \
					   sizeofObject( pkcs15infoPtr->issuerNameIDlength ) );
		writeShortInteger( stream, PKCS15_KEYID_ISSUERNAMEHASH, DEFAULT_TAG );
		status = writeOctetString( stream, pkcs15infoPtr->issuerNameID,
								   pkcs15infoPtr->issuerNameIDlength, 
								   DEFAULT_TAG );
		}
	if( pkcs15infoPtr->subjectNameIDlength > 0 )
		{
		writeSequence( stream,
					   sizeofShortInteger( PKCS15_KEYID_SUBJECTNAMEHASH ) + \
					   sizeofObject( pkcs15infoPtr->subjectNameIDlength ) );
		writeShortInteger( stream, PKCS15_KEYID_SUBJECTNAMEHASH, DEFAULT_TAG );
		status = writeOctetString( stream, pkcs15infoPtr->subjectNameID,
								   pkcs15infoPtr->subjectNameIDlength, 
								   DEFAULT_TAG );
		}
	if( pkcs15infoPtr->pgp2KeyIDlength > 0 )
		{
		writeSequence( stream, sizeofShortInteger( PKCS15_KEYID_PGP2 ) + \
							   sizeofObject( pkcs15infoPtr->pgp2KeyIDlength ) );
		writeShortInteger( stream, PKCS15_KEYID_PGP2, DEFAULT_TAG );
		status = writeOctetString( stream, pkcs15infoPtr->pgp2KeyID,
								   pkcs15infoPtr->pgp2KeyIDlength, 
								   DEFAULT_TAG );
		}
	if( pkcs15infoPtr->openPGPKeyIDlength > 0 )
		{
		writeSequence( stream, sizeofShortInteger( PKCS15_KEYID_OPENPGP ) + \
							   sizeofObject( pkcs15infoPtr->openPGPKeyIDlength ) );
		writeShortInteger( stream, PKCS15_KEYID_OPENPGP, DEFAULT_TAG );
		status = writeOctetString( stream, pkcs15infoPtr->openPGPKeyID,
								   pkcs15infoPtr->openPGPKeyIDlength, 
								   DEFAULT_TAG );
		}

	return( status );
	}

/* Write atributes to a buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 7 ) ) \
int writeKeyAttributes( OUT_BUFFER( privKeyAttributeMaxLen, \
									*privKeyAttributeSize ) 
							void *privKeyAttributes, 
						IN_LENGTH_SHORT_MIN( 16 ) \
							const int privKeyAttributeMaxLen,
						OUT_LENGTH_SHORT_Z int *privKeyAttributeSize, 
						OUT_BUFFER( pubKeyAttributeMaxLen, \
									*pubKeyAttributeSize ) \
							void *pubKeyAttributes, 
						IN_LENGTH_SHORT_MIN( 16 ) \
							const int pubKeyAttributeMaxLen,
						OUT_LENGTH_SHORT_Z int *pubKeyAttributeSize, 
						INOUT PKCS15_INFO *pkcs15infoPtr, 
						IN_HANDLE const CRYPT_HANDLE iCryptContext )
	{
	STREAM stream;
	int commonAttributeSize, commonKeyAttributeSize, keyUsage, status;

	assert( isWritePtr( privKeyAttributes, privKeyAttributeMaxLen ) );
	assert( isWritePtr( privKeyAttributeSize, sizeof( int ) ) );
	assert( isWritePtr( pubKeyAttributes, pubKeyAttributeMaxLen ) );
	assert( isWritePtr( pubKeyAttributeSize, sizeof( int ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( privKeyAttributeMaxLen >= 16 && \
			  privKeyAttributeMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( pubKeyAttributeMaxLen >= 16 && \
			  pubKeyAttributeMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Clear return values */
	memset( privKeyAttributes, 0, min( 16, privKeyAttributeMaxLen ) );
	memset( pubKeyAttributes, 0, min( 16, pubKeyAttributeMaxLen ) );
	*privKeyAttributeSize = *pubKeyAttributeSize = 0;

	/* Get ID information from the context */
	status = getKeyIDs( pkcs15infoPtr, iCryptContext );
	if( cryptStatusError( status ) )
		return( status );

	/* Try and get the validity information.  This isn't used at this point
	   but may be needed before it's set in the certificate write code, for
	   example when adding two certificates that differ only in validity 
	   period to a keyset.  Since we could be adding a raw key we ignore any 
	   return code */
	( void ) getValidityInfo( pkcs15infoPtr, iCryptContext );

	/* Figure out the PKCS #15 key usage flags.  The action flags for an 
	   object can change over time under the influence of another object.  
	   For example when a raw private key is initially written and unless 
	   something else has told it otherwise it'll have all permissible 
	   actions enabled.  When a certificate for the key is later added the 
	   permissible actions for the key may be constrained by the certificate 
	   so the private key flags will change when the object is re-written to 
	   the keyset */
	keyUsage = getKeyUsageFlags( iCryptContext, 
								 pkcs15infoPtr->privKeyUsage );
	if( keyUsage <= 0 )
		return( CRYPT_ERROR_PERMISSION );	/* No easy way to report this one */

	/* Determine how big the private key attribute collections will be */
	commonAttributeSize = ( int) sizeofObject( pkcs15infoPtr->labelLength );
	commonKeyAttributeSize = ( int ) sizeofObject( pkcs15infoPtr->iDlength ) + \
							 sizeofBitString( keyUsage ) + \
							 sizeofBitString( KEYATTR_ACCESS_PRIVATE );
	if( pkcs15infoPtr->validFrom > MIN_TIME_VALUE )
		commonKeyAttributeSize += sizeofGeneralizedTime();
	if( pkcs15infoPtr->validTo > MIN_TIME_VALUE )
		commonKeyAttributeSize += sizeofGeneralizedTime();

	/* Write the private key attributes */
	sMemOpen( &stream, privKeyAttributes, privKeyAttributeMaxLen );
	writeSequence( &stream, commonAttributeSize );
	writeCharacterString( &stream, ( BYTE * ) pkcs15infoPtr->label,
						  pkcs15infoPtr->labelLength, BER_STRING_UTF8 );
	writeSequence( &stream, commonKeyAttributeSize );
	writeOctetString( &stream, pkcs15infoPtr->iD, pkcs15infoPtr->iDlength,
					  DEFAULT_TAG );
	writeBitString( &stream, keyUsage, DEFAULT_TAG );
	status = writeBitString( &stream, KEYATTR_ACCESS_PRIVATE, DEFAULT_TAG );
	if( pkcs15infoPtr->validFrom > MIN_TIME_VALUE )
		status = writeGeneralizedTime( &stream, pkcs15infoPtr->validFrom, 
									   DEFAULT_TAG );
	if( pkcs15infoPtr->validTo > MIN_TIME_VALUE )
		status = writeGeneralizedTime( &stream, pkcs15infoPtr->validTo, 
									   CTAG_KA_VALIDTO );
	if( cryptStatusOK( status ) )
		*privKeyAttributeSize = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	pkcs15infoPtr->privKeyUsage = keyUsage;	/* Update stored usage information */

	/* Determine how big the public key attribute collections will be */
	keyUsage &= PUBKEY_USAGE_MASK;
	commonKeyAttributeSize = ( int ) sizeofObject( pkcs15infoPtr->iDlength ) + \
							 sizeofBitString( keyUsage ) + \
							 sizeofBitString( KEYATTR_ACCESS_PUBLIC );
	if( pkcs15infoPtr->validFrom > MIN_TIME_VALUE )
		commonKeyAttributeSize += sizeofGeneralizedTime();
	if( pkcs15infoPtr->validTo > MIN_TIME_VALUE )
		commonKeyAttributeSize += sizeofGeneralizedTime();

	/* Write the public key attributes */
	sMemOpen( &stream, pubKeyAttributes, pubKeyAttributeMaxLen );
	writeSequence( &stream, commonAttributeSize );
	writeCharacterString( &stream, ( BYTE * ) pkcs15infoPtr->label,
						  pkcs15infoPtr->labelLength, BER_STRING_UTF8 );
	writeSequence( &stream, commonKeyAttributeSize );
	writeOctetString( &stream, pkcs15infoPtr->iD, pkcs15infoPtr->iDlength,
					  DEFAULT_TAG );
	writeBitString( &stream, keyUsage, DEFAULT_TAG );
	status = writeBitString( &stream, KEYATTR_ACCESS_PUBLIC, DEFAULT_TAG );
	if( pkcs15infoPtr->validFrom > MIN_TIME_VALUE )
		status = writeGeneralizedTime( &stream, pkcs15infoPtr->validFrom, 
									   DEFAULT_TAG );
	if( pkcs15infoPtr->validTo > MIN_TIME_VALUE )
		status = writeGeneralizedTime( &stream, pkcs15infoPtr->validTo, 
									   CTAG_KA_VALIDTO );
	if( cryptStatusOK( status ) )
		*pubKeyAttributeSize = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	pkcs15infoPtr->pubKeyUsage = keyUsage;	/* Update stored usage information */

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int writeCertAttributes( OUT_BUFFER( certAttributeMaxLen, *certAttributeSize ) \
							void *certAttributes, 
						 IN_LENGTH_SHORT_MIN( 16 ) const int certAttributeMaxLen,
						 OUT_LENGTH_SHORT_Z int *certAttributeSize, 
						 INOUT PKCS15_INFO *pkcs15infoPtr, 
						 IN_HANDLE const CRYPT_HANDLE iCryptCert )
	{
	STREAM stream;
	BOOLEAN trustedImplicit;
	int commonAttributeSize, commonCertAttributeSize;
	int keyIdentifierDataSize, trustedUsageSize;
	int isCA, trustedUsage, status;

	assert( isWritePtr( certAttributes, certAttributeMaxLen ) );
	assert( isWritePtr( certAttributeSize, sizeof( int ) ) );
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	REQUIRES( certAttributeMaxLen >= 16 && \
			  certAttributeMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* Clear return values */
	memset( certAttributes, 0, min( 16, certAttributeMaxLen ) );
	*certAttributeSize = 0;

	/* Get ID information from the certificate */
	status = getCertIDs( pkcs15infoPtr, iCryptCert, &isCA, 
						 &trustedImplicit, &trustedUsage );
	if( cryptStatusError( status ) )
		return( status );

	/* At this point we could create a pseudo-label by reading the 
	   CRYPT_IATTRIBUTE_HOLDERNAME attribute, however label-less items will 
	   only occur when adding a standalone (i.e. trusted, implicitly-
	   handled) certificate.  If we were to set labels for these then the 
	   keyset would end up acting as a general-purpose certificate store 
	   which it isn't meant to be, so we always leave implicitly handled 
	   certificates label-less */

	/* Determine how big the attribute collection will be */
	trustedUsageSize = ( trustedUsage != CRYPT_UNUSED ) ? \
					   sizeofBitString( trustedUsage ) : 0;
	keyIdentifierDataSize = sizeofObjectIDs( pkcs15infoPtr );
	commonAttributeSize = ( pkcs15infoPtr->labelLength > 0 ) ? \
						  ( int) sizeofObject( pkcs15infoPtr->labelLength ) : 0;
	commonCertAttributeSize = ( int ) \
						sizeofObject( pkcs15infoPtr->iDlength ) + \
						( isCA ? sizeofBoolean() : 0 ) + \
						( ( trustedUsage != CRYPT_UNUSED ) ? \
						  sizeofObject( trustedUsageSize ) : 0 ) + \
						sizeofObject( keyIdentifierDataSize ) + \
						( trustedImplicit ? sizeofBoolean() : 0 ) + \
						sizeofGeneralizedTime() + sizeofGeneralizedTime();

	/* Write the certificate attributes */
	sMemOpen( &stream, certAttributes, certAttributeMaxLen );
	writeSequence( &stream, commonAttributeSize );
	if( commonAttributeSize > 0 )
		writeCharacterString( &stream, pkcs15infoPtr->label,
							  pkcs15infoPtr->labelLength, BER_STRING_UTF8 );
	writeSequence( &stream, commonCertAttributeSize );
	writeOctetString( &stream, pkcs15infoPtr->iD, pkcs15infoPtr->iDlength,
					  DEFAULT_TAG );
	if( isCA )
		writeBoolean( &stream, TRUE, DEFAULT_TAG );
	if( trustedUsage != CRYPT_UNUSED )
		{
		writeConstructed( &stream, trustedUsageSize, CTAG_CA_TRUSTED_USAGE );
		writeBitString( &stream, trustedUsage, DEFAULT_TAG );
		}
	status = writeObjectIDs( &stream, pkcs15infoPtr, keyIdentifierDataSize,
							 CTAG_CA_IDENTIFIERS );
	ENSURES( cryptStatusOK( status ) );
	if( trustedImplicit )
		writeBoolean( &stream, TRUE, CTAG_CA_TRUSTED_IMPLICIT );
	writeGeneralizedTime( &stream, pkcs15infoPtr->validFrom, DEFAULT_TAG );
	status = writeGeneralizedTime( &stream, pkcs15infoPtr->validTo,
								   CTAG_CA_VALIDTO );
	if( cryptStatusOK( status ) )
		*certAttributeSize = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
