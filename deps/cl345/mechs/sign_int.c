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
static int createDlpSignature( OUT_BUFFER_OPT( bufSize, *length ) \
									void *buffer,
							   IN_RANGE( 0, CRYPT_MAX_PKCSIZE ) \
									const int bufSize, 
							   OUT_LENGTH_BOUNDED_Z( bufSize ) int *length, 
							   IN_HANDLE const CRYPT_CONTEXT iSignContext,
							   IN_HANDLE const CRYPT_CONTEXT iHashContext,
							   IN_ENUM( SIGNATURE ) \
									const SIGNATURE_TYPE signatureType,
							   const BOOLEAN isECC )
	{
	DLP_PARAMS dlpParams;
	MESSAGE_DATA msgData;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int hashSize, status;

	assert( ( buffer == NULL && bufSize == 0 ) || \
			isWritePtrDynamic( buffer, bufSize ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( ( buffer == NULL && bufSize == 0 ) || \
			  ( buffer != NULL && \
			    bufSize > MIN_CRYPT_OBJECTSIZE && \
				bufSize <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( isEnumRange( signatureType, SIGNATURE ) );
	REQUIRES( isECC == TRUE || isECC == FALSE );

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
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE_S" );

	/* SSH hardcodes SHA-1 (or at least two fixed-length values of 20 bytes)
	   into its signature format, so we can't create an SSH signature unless
	   we're using a 20-byte hash */
	if( !isECC && signatureType == SIGNATURE_SSH && hashSize != 20 )
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
		switch( signatureType )
			{
#ifdef USE_PGP
			case SIGNATURE_PGP:
				*length = 2 * ( 2 + sigComponentSize );
				break;
#endif /* USE_PGP */

#ifdef USE_SSH
			case SIGNATURE_SSH:
				*length = 2 * sigComponentSize;
				break;
#endif /* USE_SSH */

#ifdef USE_INT_ASN1
			default:
				*length = sizeofObject( \
								( 2 * sizeofObject( \
										sigComponentSize + 1 ) ) );
				break;
#else
			default:
				retIntError();
#endif /* USE_INT_ASN1 */
			}
		CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE" );

		REQUIRES( CFI_CHECK_SEQUENCE_2( "IMESSAGE_GETATTRIBUTE_S", 
										"IMESSAGE_GETATTRIBUTE" ) );

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
	if( cryptStatusError( status ) )
		return( status );
	*length = dlpParams.outLen;
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_SIGN" );

	REQUIRES( CFI_CHECK_SEQUENCE_2( "IMESSAGE_GETATTRIBUTE_S", 
									"IMESSAGE_CTX_SIGN" ) );
	
	return( CRYPT_OK );
	}

/* Check a DLP signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDlpSignature( IN_BUFFER( signatureDataLength ) \
									const void *signatureData, 
							  IN_LENGTH_SHORT const int signatureDataLength,
							  IN_HANDLE const CRYPT_CONTEXT iSigCheckContext,
							  IN_HANDLE const CRYPT_CONTEXT iHashContext,
							  IN_ENUM( SIGNATURE ) \
									const SIGNATURE_TYPE signatureType )
	{
	DLP_PARAMS dlpParams;
	MESSAGE_DATA msgData;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int hashSize, status;

	REQUIRES( ( signatureType == SIGNATURE_SSH && \
				signatureDataLength == 40 ) || \
			  ( signatureDataLength > 40 && \
				signatureDataLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( isEnumRange( signatureType, SIGNATURE ) );

	/* Extract the hash value from the context */
	setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	hashSize = msgData.length;
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE_S" );

	/* Check the signature validity using the encoded signature data and 
	   hash */
	setDLPParams( &dlpParams, hash, hashSize, NULL, 0 );
	dlpParams.inParam2 = signatureData;
	dlpParams.inLen2 = signatureDataLength;
	if( signatureType == SIGNATURE_PGP )
		dlpParams.formatType = CRYPT_FORMAT_PGP;
	if( signatureType == SIGNATURE_SSH )
		dlpParams.formatType = CRYPT_IFORMAT_SSH;
	status = krnlSendMessage( iSigCheckContext, IMESSAGE_CTX_SIGCHECK,
							  &dlpParams, sizeof( DLP_PARAMS ) );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_SIGCHECK" );

	REQUIRES( CFI_CHECK_SEQUENCE_2( "IMESSAGE_GETATTRIBUTE_S", 
									"IMESSAGE_CTX_SIGCHECK" ) );

	return( CRYPT_OK );
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
					 IN_DATALENGTH_Z const int sigMaxLength, 
					 OUT_DATALENGTH_Z int *signatureLength, 
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
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int signAlgo, hashAlgo, length DUMMY_INIT, hashParam = 0, status;

	assert( ( signature == NULL && sigMaxLength == 0 ) || \
			isWritePtrDynamic( signature, sigMaxLength ) );
	assert( isWritePtr( signatureLength, sizeof( int ) ) );

	REQUIRES( ( signature == NULL && sigMaxLength == 0 ) || \
			  ( signature != NULL && \
			    sigMaxLength > MIN_CRYPT_OBJECTSIZE && \
				sigMaxLength < MAX_BUFFER_SIZE ) );
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

	/* Clear return value */
	if( signature != NULL )
		memset( signature, 0, min( 16, sigMaxLength ) );
	*signatureLength = 0;

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
	if( cryptStatusOK( status ) && isHashMacExtAlgo( hashAlgo ) )
		{
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashParam, CRYPT_CTXINFO_BLOCKSIZE );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE" );

	/* DLP and ECDLP signatures are handled somewhat specially */
	INJECT_FAULT( MECH_CORRUPT_HASH, MECH_CORRUPT_HASH_1 );
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
		CFI_CHECK_UPDATE( "IMESSAGE_DEV_SIGN" );
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
		CFI_CHECK_UPDATE( "IMESSAGE_DEV_SIGN" );
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
	INJECT_FAULT( MECH_CORRUPT_SIG, MECH_CORRUPT_SIG_1 );

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
	CFI_CHECK_UPDATE( "writeSigFunction" );

	/* Clean up */
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	if( cryptStatusError( status ) )
		return( status );

	REQUIRES( CFI_CHECK_SEQUENCE_3( "IMESSAGE_GETATTRIBUTE", 
									"IMESSAGE_DEV_SIGN", "writeSigFunction" ) );

	return( CRYPT_OK );
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
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int signAlgo, hashAlgo, hashAlgoParam = 0;
	int signatureDataLength, compareType = MESSAGE_COMPARE_NONE, status;

	assert( isReadPtrDynamic( signature, signatureLength ) );
	
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
	if( cryptStatusOK( status ) && isHashMacExtAlgo( hashAlgo ) )
		{
		status = krnlSendMessage( iHashContext, IMESSAGE_GETATTRIBUTE,
								  &hashAlgoParam, CRYPT_CTXINFO_BLOCKSIZE );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE" );

	/* Read and check the signature record */
	sMemConnect( &stream, signature, signatureLength );
	status = readSigFunction( &stream, &queryInfo );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}
	CFI_CHECK_UPDATE( "readSigFunction" );

	/* Make sure that we've been given the correct algorithms */
	if( signatureType != SIGNATURE_RAW && signatureType != SIGNATURE_SSL )
		{
		if( signAlgo != queryInfo.cryptAlgo || \
			hashAlgo != queryInfo.hashAlgo )
			status = CRYPT_ERROR_SIGNATURE;
		if( signatureType != SIGNATURE_SSH )
			{
			/* SSH requires complex string-parsing to determine the optional
			   parameters, so the check is done elsewhere */
			if( isHashMacExtAlgo( hashAlgo ) && \
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
	   format supports this type of check */
	switch( signatureType )
		{
		case SIGNATURE_CMS:
			/* This format supports a check with 
			   MESSAGE_COMPARE_ISSUERANDSERIALNUMBER but this has already 
			   been done while procesing the other CMS data before we were 
			   called so we don't need to do it again */
			/* compareType = MESSAGE_COMPARE_ISSUERANDSERIALNUMBER; */
			break;

		case SIGNATURE_CRYPTLIB:
			compareType = MESSAGE_COMPARE_KEYID;
			break;

		case SIGNATURE_PGP:
			compareType = ( queryInfo.version == PGP_VERSION_2 ) ? \
							MESSAGE_COMPARE_KEYID_PGP : \
							MESSAGE_COMPARE_KEYID_OPENPGP;
			break;

		default:
			/* Other format types don't include identification information
			   with the signature */
			break;
		}
	if( compareType != MESSAGE_COMPARE_NONE )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, queryInfo.keyID, queryInfo.keyIDlength );
		status = krnlSendMessage( iSigCheckContext, IMESSAGE_COMPARE,
								  &msgData, compareType );
		if( cryptStatusError( status ) && \
			compareType == MESSAGE_COMPARE_KEYID )
			{
			/* Checking for the keyID gets a bit complicated, in theory it's 
			   the subjectKeyIdentifier from a certificate but in practice 
			   this form is mostly used for certificateless public keys.  
			   Because of this we check for the keyID first and if that 
			   fails fall back to the sKID */
			status = krnlSendMessage( iSigCheckContext, IMESSAGE_COMPARE, 
									  &msgData, 
									  MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER );
			}
		if( cryptStatusError( status ) )
			{
			/* A failed comparison is reported as a generic CRYPT_ERROR,
			   convert it into a wrong-key error */
			zeroise( &queryInfo, sizeof( QUERY_INFO ) );
			return( CRYPT_ERROR_WRONGKEY );
			}
		}
	REQUIRES( boundsCheck( queryInfo.dataStart, queryInfo.dataLength,
						   signatureLength ) );
	signatureData = ( BYTE * ) signature + queryInfo.dataStart;
	signatureDataLength = queryInfo.dataLength;
	zeroise( &queryInfo, sizeof( QUERY_INFO ) );
	CFI_CHECK_UPDATE( "IMESSAGE_COMPARE" );

	/* DLP and ECDLP signatures are handled somewhat specially */
	if( isDlpAlgo( signAlgo ) || isEccAlgo( signAlgo ) )
		{
		/* In addition to the special-case processing for DLP/ECDLP 
		   signatures, we have to provide even further special handling for 
		   SSL signatures, which normally sign a dual hash of MD5 and SHA-1 
		   but for DLP only sign the second SHA-1 hash */
		status = checkDlpSignature( signatureData, signatureDataLength, 
									iSigCheckContext, 
									isSSLsig ? iHashContext2 : iHashContext,
									signatureType );
		if( cryptStatusError( status ) )
			return( status );
		CFI_CHECK_UPDATE( "checkDlpSignature" );

		REQUIRES( CFI_CHECK_SEQUENCE_4( "IMESSAGE_GETATTRIBUTE", 
										"readSigFunction", "IMESSAGE_COMPARE", 
										"checkDlpSignature" ) );
		return( CRYPT_OK );
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
	CFI_CHECK_UPDATE( "IMESSAGE_DEV_SIGCHECK" );

	REQUIRES( CFI_CHECK_SEQUENCE_4( "IMESSAGE_GETATTRIBUTE", "readSigFunction", 
									"IMESSAGE_COMPARE", 
									"IMESSAGE_DEV_SIGCHECK" ) );

	return( CRYPT_OK );
	}
