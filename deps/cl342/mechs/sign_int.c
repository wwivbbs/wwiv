/****************************************************************************
*																			*
*							Internal Signature Routines						*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "mech.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "mechs/mech.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*							DLP Signature Handling							*
*																			*
****************************************************************************/

/* Create a DLP signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int createDlpSignature( OUT_BUFFER_OPT( bufSize, *length ) void *buffer,
							   IN_RANGE( 0, CRYPT_MAX_PKCSIZE ) const int bufSize, 
							   OUT_LENGTH_Z int *length, 
							   IN_HANDLE const CRYPT_CONTEXT iSignContext,
							   IN_HANDLE const CRYPT_CONTEXT iHashContext,
							   IN_ENUM( SIGNATURE ) \
									const SIGNATURE_TYPE signatureType,
							   const BOOLEAN isECC )
	{
	DLP_PARAMS dlpParams;
	MESSAGE_DATA msgData;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize, status;

	assert( ( buffer == NULL && bufSize == 0 ) || \
			isWritePtr( buffer, bufSize ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( ( buffer == NULL && bufSize == 0 ) || \
			  ( buffer != NULL && \
			    bufSize > MIN_CRYPT_OBJECTSIZE && \
				bufSize <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( signatureType > SIGNATURE_NONE && \
			  signatureType < SIGNATURE_LAST );

	/* Clear return value */
	*length = 0;

	/* Extract the hash value from the context.  If we're doing a length 
	   check then there's no hash value present yet, so we just fill in the 
	   hash length value from the blocksize attribute */
	if( buffer == NULL )
		{
		memset( hash, 0, CRYPT_MAX_HASHSIZE );	/* Keep mem.checkers happy */
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &msgData.length, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		}
	else
		{
		setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_HASHVALUE );
		}
	if( cryptStatusError( status ) )
		return( status );
	hashSize = msgData.length;

	/* Standard DSA is only defined for hash algorithms with a block size of 
	   160 bits, FIPS 186-3 extends this to allow use with larger hashes but 
	   the use with algorithms other than SHA-1 is a bit unclear so we always
	   require 160 bits */
	if( !isECC && hashSize != 20 )
		{
		/* The error reporting here is a bit complex, see the comment in 
		   createSignature() for how this works */
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* If we're doing a length check and the signature is being written in 
	   cryptlib format the length is just an estimate since it can change by 
	   several bytes depending on whether the signature values have the high 
	   bit set or not (which requires zero-padding of the ASN.1-encoded 
	   integers) or have the high bytes set to zero(es).  We use a worst-
	   case estimate here and assume that both integers will be of the 
	   maximum size and need padding, which is rather nasty because it means 
	   that we can't tell how large a signature will be without actually 
	   creating it */
	if( buffer == NULL )
		{
		int sigComponentSize = hashSize;

		if( isECC )
			{
			/* For ECC signatures the reduction is done mod n, which is
			   variable-length, but for standard curves is the same as the
			   key size */
			status = krnlSendMessage( iSignContext, IMESSAGE_GETATTRIBUTE,
									  &sigComponentSize, 
									  CRYPT_CTXINFO_KEYSIZE );
			if( cryptStatusError( status ) )
				return( status );
			}
		*length = ( signatureType == SIGNATURE_PGP ) ? \
					2 * ( 2 + sigComponentSize ) : \
					sizeofObject( ( 2 * sizeofObject( \
											sigComponentSize + 1 ) ) );
		return( CRYPT_OK );
		}

	/* Sign the data */
	setDLPParams( &dlpParams, hash, hashSize, buffer, bufSize );
	if( signatureType == SIGNATURE_PGP )
		dlpParams.formatType = CRYPT_FORMAT_PGP;
	if( signatureType == SIGNATURE_SSH )
		dlpParams.formatType = CRYPT_IFORMAT_SSH;
	status = krnlSendMessage( iSignContext, IMESSAGE_CTX_SIGN, 
							  &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusOK( status ) )
		*length = dlpParams.outLen;
	return( status );
	}

/* Check a DLP signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDlpSignature( IN_BUFFER( signatureDataLength ) \
									const void *signatureData, 
							  IN_LENGTH_SHORT const int signatureDataLength,
							  IN_HANDLE const CRYPT_CONTEXT iSigCheckContext,
							  IN_HANDLE const CRYPT_CONTEXT iHashContext,
							  IN_ENUM( SIGNATURE ) \
									const SIGNATURE_TYPE signatureType,
							  const BOOLEAN isECC )
	{
	DLP_PARAMS dlpParams;
	MESSAGE_DATA msgData;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize, status;

	REQUIRES( ( signatureType == SIGNATURE_SSH && \
				signatureDataLength == 40 ) || \
			  ( signatureDataLength > 40 && \
				signatureDataLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( signatureType > SIGNATURE_NONE && \
			  signatureType < SIGNATURE_LAST );

	/* Extract the hash value from the context */
	setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	hashSize = msgData.length;

	/* Standard DSA is only defined for hash algorithms with a block size of 
	   160 bits, FIPS 186-3 extends this to allow use with larger hashes but 
	   the use with algorithms other than SHA-1 is a bit unclear so we always
	   require 160 bits */
	if( !isECC && hashSize != 20 )
		{
		/* The error reporting here is a bit complex, see the comment in 
		   createSignature() for how this works */
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Check the signature validity using the encoded signature data and 
	   hash */
	setDLPParams( &dlpParams, hash, hashSize, NULL, 0 );
	dlpParams.inParam2 = signatureData;
	dlpParams.inLen2 = signatureDataLength;
	if( signatureType == SIGNATURE_PGP )
		dlpParams.formatType = CRYPT_FORMAT_PGP;
	if( signatureType == SIGNATURE_SSH )
		dlpParams.formatType = CRYPT_IFORMAT_SSH;
	return( krnlSendMessage( iSigCheckContext, IMESSAGE_CTX_SIGCHECK,
							 &dlpParams, sizeof( DLP_PARAMS ) ) );
	}

/****************************************************************************
*																			*
*								Create a Signature							*
*																			*
****************************************************************************/

/* Common signature-creation routine, used by other sign_xxx.c modules */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int createSignature( OUT_BUFFER_OPT( sigMaxLength, *signatureLength ) \
						void *signature, 
					 IN_LENGTH_Z const int sigMaxLength, 
					 OUT_LENGTH_Z int *signatureLength, 
					 IN_HANDLE const CRYPT_CONTEXT iSignContext,
					 IN_HANDLE const CRYPT_CONTEXT iHashContext,
					 IN_HANDLE_OPT const CRYPT_CONTEXT iHashContext2,
					 IN_ENUM( SIGNATURE ) const SIGNATURE_TYPE signatureType )
	{
	STREAM stream;
	const WRITESIG_FUNCTION writeSigFunction = getWriteSigFunction( signatureType );
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *bufPtr = ( signature == NULL ) ? NULL : buffer;
	const BOOLEAN isSSLsig = ( signatureType == SIGNATURE_SSL ) ? TRUE : FALSE;
	const int bufSize = ( signature == NULL ) ? 0 : CRYPT_MAX_PKCSIZE;
	int signAlgo, hashAlgo, length = DUMMY_INIT, hashParam = 0, status;

	assert( ( signature == NULL && sigMaxLength == 0 ) || \
			isWritePtr( signature, sigMaxLength ) );
	assert( isWritePtr( signatureLength, sizeof( int ) ) );

	REQUIRES( ( signature == NULL && sigMaxLength == 0 ) || \
			  ( signature != NULL && \
			    sigMaxLength > MIN_CRYPT_OBJECTSIZE && \
				sigMaxLength < MAX_INTLENGTH ) );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( ( signatureType == SIGNATURE_SSL && \
				isHandleRangeValid( iHashContext2 ) ) || \
			  ( ( signatureType == SIGNATURE_CMS || \
				  signatureType == SIGNATURE_CRYPTLIB || \
				  signatureType == SIGNATURE_PGP || \
				  signatureType == SIGNATURE_RAW || \
				  signatureType == SIGNATURE_SSH || \
				  signatureType == SIGNATURE_TLS12 || \
				  signatureType == SIGNATURE_X509 ) && \
				iHashContext2 == CRYPT_UNUSED ) );

	/* Make sure that the requested signature format is available */
	if( writeSigFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Extract general information */
	status = krnlSendMessage( iSignContext, IMESSAGE_GETATTRIBUTE, &signAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM1 : status );
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && isHashExtAlgo( hashAlgo ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashParam, CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );

	/* DLP and ECDLP signatures are handled somewhat specially */
	if( isDlpAlgo( signAlgo ) || isEccAlgo( signAlgo ) )
		{
		/* In addition to the special-case processing for DLP/ECDLP 
		   signatures, we have to provide even further special handling for 
		   SSL signatures, which normally sign a dual hash of MD5 and SHA-1 
		   but for DLP only sign the second SHA-1 hash */
		status = createDlpSignature( bufPtr, bufSize, &length, iSignContext, 
									 isSSLsig ? iHashContext2 : iHashContext, 
									 signatureType, 
									 isEccAlgo( signAlgo ) ? TRUE : FALSE );
		}
	else
		{
		MECHANISM_SIGN_INFO mechanismInfo;

		/* It's a standard signature, process it as normal */
		setMechanismSignInfo( &mechanismInfo, bufPtr, bufSize, iHashContext, 
							  iHashContext2, iSignContext );
		status = krnlSendMessage( iSignContext, IMESSAGE_DEV_SIGN, &mechanismInfo,
								  isSSLsig ? MECHANISM_SIG_SSL : \
											 MECHANISM_SIG_PKCS1 );
		if( cryptStatusOK( status ) )
			length = mechanismInfo.signatureLength;
		clearMechanismInfo( &mechanismInfo );
		}
	if( cryptStatusError( status ) )
		{
		/* The mechanism messages place the acted-on object (in this case the
		   hash context) first while the higher-level functions place the
		   signature context next to the signature data, in other words
		   before the hash context.  Because of this we have to reverse
		   parameter error values when translating from the mechanism to the
		   signature function level */
		if( bufPtr != NULL )
			zeroise( bufPtr, CRYPT_MAX_PKCSIZE );
		return( ( status == CRYPT_ARGERROR_NUM1 ) ? \
					CRYPT_ARGERROR_NUM2 : \
				( status == CRYPT_ARGERROR_NUM2 ) ? \
					CRYPT_ARGERROR_NUM1 : status );
		}

	/* If we're perfoming a dummy sign for a length check, set up a dummy 
	   value to write */
	if( signature == NULL )
		memset( buffer, 0x01, length );

	/* Write the signature record to the output */
	sMemOpenOpt( &stream, signature, sigMaxLength );
	status = writeSigFunction( &stream, iSignContext, hashAlgo, hashParam, 
							   signAlgo, buffer, length );
	if( cryptStatusOK( status ) )
		*signatureLength = stell( &stream );
	sMemDisconnect( &stream );

	/* Clean up */
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	return( status );
	}

/****************************************************************************
*																			*
*								Check a Signature							*
*																			*
****************************************************************************/

/* Common signature-checking routine, used by other sign_xxx.c modules */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkSignature( IN_BUFFER( signatureLength ) const void *signature, 
					IN_LENGTH_SHORT const int signatureLength,
					IN_HANDLE const CRYPT_CONTEXT iSigCheckContext,
					IN_HANDLE const CRYPT_CONTEXT iHashContext,
					IN_HANDLE_OPT const CRYPT_CONTEXT iHashContext2,
					IN_ENUM( SIGNATURE ) const SIGNATURE_TYPE signatureType )
	{
	MECHANISM_SIGN_INFO mechanismInfo;
	const READSIG_FUNCTION readSigFunction = getReadSigFunction( signatureType );
	QUERY_INFO queryInfo;
	STREAM stream;
	void *signatureData;
	const BOOLEAN isSSLsig = ( signatureType == SIGNATURE_SSL ) ? TRUE : FALSE;
	int signAlgo, hashAlgo, signatureDataLength, hashAlgoParam = 0, status;

	assert( isReadPtr( signature, signatureLength ) );
	
	REQUIRES( signatureLength > 40 && \
			  signatureLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( ( signatureType == SIGNATURE_SSL && \
				isHandleRangeValid( iHashContext2 ) ) || \
			  ( ( signatureType == SIGNATURE_CMS || \
				  signatureType == SIGNATURE_CRYPTLIB || \
				  signatureType == SIGNATURE_PGP || \
				  signatureType == SIGNATURE_RAW || \
				  signatureType == SIGNATURE_SSH || \
				  signatureType == SIGNATURE_TLS12 || \
				  signatureType == SIGNATURE_X509 ) && \
				iHashContext2 == CRYPT_UNUSED ) );

	/* Make sure that the requested signature format is available */
	if( readSigFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Extract general information */
	status = krnlSendMessage( iSigCheckContext, IMESSAGE_GETATTRIBUTE,
							  &signAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM1 : status );
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
							  &hashAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && isHashExtAlgo( hashAlgo ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashAlgoParam, CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );

	/* Read and check the signature record */
	sMemConnect( &stream, signature, signatureLength );
	status = readSigFunction( &stream, &queryInfo );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Make sure that we've been given the correct algorithms.  Raw
	   signatures specify the algorithm information elsewhere so the check
	   is done at a higher level when we process the signature data */
	if( signatureType != SIGNATURE_RAW && signatureType != SIGNATURE_SSL )
		{
		if( signAlgo != queryInfo.cryptAlgo )
			status = CRYPT_ERROR_SIGNATURE;
		if( signatureType != SIGNATURE_SSH )
			{
			if( hashAlgo != queryInfo.hashAlgo )
				status = CRYPT_ERROR_SIGNATURE;
			if( isHashExtAlgo( hashAlgo ) && \
				hashAlgoParam != queryInfo.hashAlgoParam )
				status = CRYPT_ERROR_SIGNATURE;
			}
		if( cryptStatusError( status ) )
			{
			zeroise( &queryInfo, sizeof( QUERY_INFO ) );
			return( status );
			}
		}

	/* Make sure that we've been given the correct key if the signature
	   format supports this type of check.  SIGNATURE_CMS supports a check
	   with MESSAGE_COMPARE_ISSUERANDSERIALNUMBER but this has already been
	   done while procesing the other CMS data before we were called so we
	   don't need to do it again */
	if( signatureType == SIGNATURE_CRYPTLIB || \
		signatureType == SIGNATURE_PGP )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, queryInfo.keyID, queryInfo.keyIDlength );
		status = krnlSendMessage( iSigCheckContext, IMESSAGE_COMPARE,
								  &msgData, 
								  ( signatureType == SIGNATURE_CRYPTLIB ) ? \
									MESSAGE_COMPARE_KEYID : \
								  ( queryInfo.version == PGP_VERSION_2 ) ? \
									MESSAGE_COMPARE_KEYID_PGP : \
									MESSAGE_COMPARE_KEYID_OPENPGP );
		if( cryptStatusError( status ) )
			{
			/* A failed comparison is reported as a generic CRYPT_ERROR,
			   convert it into a wrong-key error if necessary */
			zeroise( &queryInfo, sizeof( QUERY_INFO ) );
			return( ( status == CRYPT_ERROR ) ? \
					CRYPT_ERROR_WRONGKEY : status );
			}
		}
	REQUIRES( rangeCheck( queryInfo.dataStart, queryInfo.dataLength,
						  signatureLength ) );
	signatureData = ( BYTE * ) signature + queryInfo.dataStart;
	signatureDataLength = queryInfo.dataLength;
	zeroise( &queryInfo, sizeof( QUERY_INFO ) );

	/* DLP and ECDLP signatures are handled somewhat specially */
	if( isDlpAlgo( signAlgo ) || isEccAlgo( signAlgo ) )
		{
		/* In addition to the special-case processing for DLP/ECDLP 
		   signatures, we have to provide even further special handling for 
		   SSL signatures, which normally sign a dual hash of MD5 and SHA-1 
		   but for DLP only sign the second SHA-1 hash */
		return( checkDlpSignature( signatureData, signatureDataLength, 
								   iSigCheckContext, 
								   isSSLsig ? iHashContext2 : iHashContext,
								   signatureType, 
								   isEccAlgo( signAlgo ) ? TRUE : FALSE ) );
		}

	/* It's a standard signature, process it as normal */
	setMechanismSignInfo( &mechanismInfo, signatureData, signatureDataLength, 
						  iHashContext, iHashContext2, iSigCheckContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_SIGCHECK, 
							  &mechanismInfo, isSSLsig ? MECHANISM_SIG_SSL : \
														 MECHANISM_SIG_PKCS1 );
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		{
		/* The mechanism messages place the acted-on object (in this case the 
		   hash context) first while the higher-level functions place the 
		   signature context next to the signature data, in other words 
		   before the hash context.  Because of this we have to reverse 
		   parameter error values when translating from the mechanism to the 
		   signature function level */
		return( ( status == CRYPT_ARGERROR_NUM1 ) ? \
					CRYPT_ARGERROR_NUM2 : \
				( status == CRYPT_ARGERROR_NUM2 ) ? \
					CRYPT_ARGERROR_NUM1 : status );
		}

	return( CRYPT_OK );
	}
