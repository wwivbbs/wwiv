/****************************************************************************
*																			*
*							cryptlib TLS Client								*
*					   Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

/* Testing the SSL/TLS code gets a bit complicated because in the presence 
   of the session cache every session after the first one will be a resumed 
   session.  To deal with this we disable the client-side session cache in 
   the VC++ 6 debug build.

   Note that changing the follow requires an equivalent change in 
   test/ssl.c */

#if defined( __WINDOWS__ ) && defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && \
	!defined( NDEBUG ) && 1
  #define NO_SESSION_CACHE
#endif /* VC++ 6.0 debug build */

#ifdef USE_SSL

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifdef CONFIG_SUITEB

/* For Suite B the first suite must be ECDHE/AES128-GCM/SHA256 or 
   ECDHE/AES256-GCM/SHA384 depending on the security level and the second 
   suite, at the 128-bit security level, must be ECDHE/AES256-GCM/SHA384 */

CHECK_RETVAL_BOOL \
static int checkSuiteBSelection( IN_RANGE( SSL_FIRST_VALID_SUITE, \
										   SSL_LAST_SUITE - 1 ) \
									const int cipherSuite,
								 const int flags,
								 const BOOLEAN isFirstSuite )
	{
	REQUIRES( cipherSuite >= SSL_FIRST_VALID_SUITE && \
			  cipherSuite < SSL_LAST_SUITE );
	REQUIRES( ( flags & ~( SSL_PFLAG_SUITEB ) ) == 0 );
	REQUIRES( isFirstSuite == TRUE || isFirstSuite == FALSE );

	if( isFirstSuite )
		{
		switch( flags )
			{
			case SSL_PFLAG_SUITEB_128:
				if( cipherSuite == TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 )
					return( TRUE );
				break;

			case SSL_PFLAG_SUITEB_256:
				if( cipherSuite == TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 )
					return( TRUE );
				break;

			default:
				retIntError();
			}
		}
	else
		{
		switch( flags )
			{
			case SSL_PFLAG_SUITEB_128:
				if( cipherSuite == TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 )
					return( TRUE );
				break;

			case SSL_PFLAG_SUITEB_256:
				/* For the 256-bit level there are no further requirements */
				return( TRUE );

			default:
				retIntError();
			}
		}

	return( FALSE );
	}
#endif /* CONFIG_SUITEB */

/* Encode a list of available algorithms */

#define MAX_NO_SUITES	32

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeCipherSuiteList( INOUT STREAM *stream, 
								 IN_RANGE( SSL_MINOR_VERSION_SSL, \
										   SSL_MINOR_VERSION_TLS12 ) \
									const int sslVersion,
								 const BOOLEAN usePSK, 
								 IN_FLAGS_Z( SSL ) const int suiteBinfo )
	{
	const CIPHERSUITE_INFO **cipherSuiteInfo;
	int availableSuites[ MAX_NO_SUITES + 8 ], cipherSuiteCount = 0;
	int suiteIndex, cipherSuiteInfoSize, status;
#ifdef CONFIG_SUITEB
	int suiteNo = 0;
#endif /* CONFIG_SUITEB */
	int LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sslVersion >= SSL_MINOR_VERSION_SSL && \
			  sslVersion <= SSL_MINOR_VERSION_TLS12 );
	REQUIRES( usePSK == TRUE || usePSK == FALSE );
#ifdef CONFIG_SUITEB
	REQUIRES( suiteBinfo >= SSL_PFLAG_NONE && suiteBinfo < SSL_PFLAG_MAX );
#endif /* CONFIG_SUITEB */

	/* Get the information for the supported cipher suites */
	status = getCipherSuiteInfo( &cipherSuiteInfo, &cipherSuiteInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Walk down the list of algorithms (and the corresponding cipher
	   suites) remembering each one that's available for use */
	LOOP_EXT_INITCHECK( suiteIndex = 0,
						suiteIndex < cipherSuiteInfoSize && \
							cipherSuiteInfo[ suiteIndex ]->cipherSuite != SSL_NULL_WITH_NULL,
						MAX_NO_SUITES + 1 )
		{
		const CIPHERSUITE_INFO *cipherSuiteInfoPtr = cipherSuiteInfo[ suiteIndex ];
		const CRYPT_ALGO_TYPE keyexAlgo = cipherSuiteInfoPtr->keyexAlgo;
		const CRYPT_ALGO_TYPE cryptAlgo = cipherSuiteInfoPtr->cryptAlgo;
		const CRYPT_ALGO_TYPE authAlgo = cipherSuiteInfoPtr->authAlgo;
		const CRYPT_ALGO_TYPE macAlgo = cipherSuiteInfoPtr->macAlgo;
		const int suiteFlags = cipherSuiteInfoPtr->flags;
		int LOOP_ITERATOR_ALT;

		/* Make sure that the cipher suite is appropriate for SuiteB use if 
		   necessary */
#ifdef CONFIG_SUITEB
		if( suiteNo == 0 || suiteNo == 1 )
			{
			if( !checkSuiteBSelection( cipherSuiteInfoPtr->cipherSuite,
									   ( suiteBinfo == 0 ) ? \
											SSL_PFLAG_SUITEB_128 : suiteBinfo, 
									   ( suiteNo == 0 ) ? TRUE : FALSE ) )
				{
				suiteIndex++;
				continue;
				}
			suiteNo++;
			}
#endif /* CONFIG_SUITEB */

		/* Make sure that the suite is appropriate for the SSL/TLS version
		   that we're using.  Normally we can use TLS suites in SSL (e.g. 
		   the AES suites), but we can't use ECC both because this requires 
		   extensions and because the SSLv3 PRF mechanisms can't handle 
		   ECC's premaster secrets */
		if( ( ( suiteFlags & CIPHERSUITE_FLAG_ECC ) && \
			  sslVersion <= SSL_MINOR_VERSION_SSL ) || \
			( ( suiteFlags & CIPHERSUITE_FLAG_TLS12 ) && \
			  sslVersion <= SSL_MINOR_VERSION_TLS11 ) )
			{
			suiteIndex++;
			continue;
			}

		/* If it's a PSK suite but we're not using PSK, skip it */
		if( ( suiteFlags & CIPHERSUITE_FLAG_PSK ) && !usePSK )
			{
			suiteIndex++;
			continue;
			}

		/* If the keyex algorithm for this suite isn't enabled for this 
		   build of cryptlib, skip all suites that use it.  We have to 
		   explicitly exclude the special case where there's no keyex 
		   algorithm in order to accomodate the bare TLS-PSK suites (used 
		   without DH/ECDH or RSA), whose keyex mechanism is pure PSK */
		if( keyexAlgo != CRYPT_ALGO_NONE && !algoAvailable( keyexAlgo ) )
			{
			LOOP_EXT_CHECKINC_ALT( \
					cipherSuiteInfo[ suiteIndex ]->keyexAlgo == keyexAlgo && \
						suiteIndex < cipherSuiteInfoSize,
					suiteIndex++, MAX_NO_SUITES + 1 );
			ENSURES( LOOP_BOUND_OK_ALT );
			ENSURES( suiteIndex < cipherSuiteInfoSize );
			continue;
			}

		/* If the bulk encryption algorithm or MAC algorithm for this suite 
		   isn't enabled for this build of cryptlib, skip all suites that 
		   use it */
		if( !algoAvailable( cryptAlgo ) )
			{
			LOOP_EXT_CHECKINC_ALT( \
					cipherSuiteInfo[ suiteIndex ]->cryptAlgo == cryptAlgo && \
						suiteIndex < cipherSuiteInfoSize,
					suiteIndex++, MAX_NO_SUITES + 1 );
			ENSURES( LOOP_BOUND_OK_ALT );
			ENSURES( suiteIndex < cipherSuiteInfoSize );
			continue;
			}
		if( !algoAvailable( macAlgo ) )
			{
			LOOP_EXT_CHECKINC_ALT( \
					cipherSuiteInfo[ suiteIndex ]->macAlgo == macAlgo && \
						suiteIndex < cipherSuiteInfoSize,
					suiteIndex++, MAX_NO_SUITES + 1 );
			ENSURES( LOOP_BOUND_OK_ALT );
			ENSURES( suiteIndex < cipherSuiteInfoSize );
			continue;
			}

		/* The suite is supported, remember it.  In theory there's only a
		   single combination of the various algorithms present, but these 
		   can be subsetted into different key sizes (because they're there, 
		   that's why) so we have to iterate the recording of available 
		   suites instead of just assigning a single value on match */
		LOOP_EXT_CHECK_ALT( \
				cipherSuiteInfo[ suiteIndex ]->keyexAlgo == keyexAlgo && \
				cipherSuiteInfo[ suiteIndex ]->authAlgo == authAlgo && \
				cipherSuiteInfo[ suiteIndex ]->cryptAlgo == cryptAlgo && \
				cipherSuiteInfo[ suiteIndex ]->macAlgo == macAlgo && \
				suiteIndex < cipherSuiteInfoSize, MAX_NO_SUITES + 1 )
			{
			availableSuites[ cipherSuiteCount++ ] = \
						cipherSuiteInfo[ suiteIndex++ ]->cipherSuite;
#ifdef CONFIG_SUITEB
			if( suiteNo == 0 || suiteNo == 1 )
				break;	/* Suite B has special requirements for initial suites */
#endif /* CONFIG_SUITEB */
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		ENSURES( suiteIndex < cipherSuiteInfoSize );
		ENSURES( cipherSuiteCount < MAX_NO_SUITES );
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( suiteIndex < cipherSuiteInfoSize );
	ENSURES( cipherSuiteCount > 0 && cipherSuiteCount < MAX_NO_SUITES );

	/* Encode the list of available cipher suites */
	status = writeUint16( stream, cipherSuiteCount * UINT16_SIZE );
	LOOP_EXT( suiteIndex = 0, 
			  cryptStatusOK( status ) && suiteIndex < cipherSuiteCount, 
			  suiteIndex++, MAX_NO_SUITES + 1 )
		{
		status = writeUint16( stream, availableSuites[ suiteIndex ] );
		}
	ENSURES( LOOP_BOUND_OK );

	return( status );
	}

/* Process the server's certificate request:

		byte	ID = SSL_HAND_SERVER_CERTREQUEST
		uint24	len
		byte	certTypeLen
		byte[]	certType
	  [	uint16	sigHashListLen		-- TLS 1.2 ]
	  [		byte	hashAlgoID		-- TLS 1.2 ]
	  [		byte	sigAlgoID		-- TLS 1.2 ]
		uint16	caNameListLen
			uint16	caNameLen
			byte[]	caName

   We don't really care what's in the certificate request packet since the 
   contents are irrelevant, in a number of cases servers have been known to 
   send out superfluous certificate requests without the admins even knowing 
   that they're doing it, in other cases servers send out requests for every 
   CA that they know of.  This can produce certificate requests containing 
   150-160 CAs, either because they've carefully catalogued every CA they've 
   ever heard of or because the software enumerates every CA in the local 
   system's trust list, leading in extreme cases to certificate request 
   messages that overflow the maximum SSL message size.  This is pretty much 
   meaningless since they can't possibly trust all of those CAs to authorise 
   access to their site.  In addition virtually all clients just have a 
   single certificate, so if there's one available we try with the one 
   certificate we've got rather than failing the handshake because the 
   server sent out the wrong CA name.

   Because of this, all that we do here is perform a basic sanity check and 
   remember that we may need to submit a certificate later on if we've got 
   one, with special-case handling for certificate requests fragmented 
   across multiple SSL packets because the server wants to list every CA in 
   the known universe.

   See the long comment in the cert-request handling code in ssl_svr.c 
   for the handling of the sigHashList */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processFragmentedRequest( INOUT SESSION_INFO *sessionInfoPtr,
									 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
									 INOUT STREAM *stream,
									 IN_RANGE( 1, MAX_PACKET_SIZE * 2 ) \
											const int totalLength )
	{
	const int dataLeft = sMemDataLeft( stream );
	const int remainder = totalLength - dataLeft;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( totalLength >= 1 && \
			  totalLength <= MAX_PACKET_SIZE + ( MAX_PACKET_SIZE / 2 ) );
	REQUIRES( isShortIntegerRange( dataLeft ) );
	REQUIRES( isShortIntegerRangeNZ( remainder ) );

	/* Skip the remaining data in the packet if there is any */
	if( dataLeft > 0 )
		{
		status = sSkip( stream, sMemDataLeft( stream ), MAX_INTLENGTH_SHORT );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Read the next packet */
	status = refreshHSStream( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		{
		/* Returning at this point is problematic because refreshHSStream()
		   has disconnected the memory stream while reading in new data for
		   it, so that when the caller tries to disconnect the stream in
		   response to getting an error status it'll trigger a sanity-check
		   exception.  This isn't normally a problem because the top-level
		   code exits without trying to disconnect the stream in response to
		   an error from refreshHSStream(), but in this case it's being 
		   called from inside a low-level function to deal with a fragmented
		   packet rather than at the top level.  To deal with this we 
		   temporarily connect the stream to the receive buffer in order for
		   the caller to have something to disconnect */
		sMemConnect( stream, sessionInfoPtr->receiveBuffer, 1 );
		return( status );
		}
	return( sSkip( stream, remainder, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processCertRequest( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							   INOUT STREAM *stream )
	{
	int packetLength, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Although the spec says that at least one CA name entry must be 
	   present, some implementations send a zero-length list so we allow 
	   this as well.  The spec was changed in late TLS 1.1 drafts to reflect 
	   this practice.
	   
	   This packet may be longer than the encapsulating SSL packet, 
	   checkHSPacketHeader() has code to special-case this and allow a
	   packetLength of up to 1 1/2 SSL packet lengths */
	status = checkHSPacketHeader( sessionInfoPtr, stream, &packetLength,
								  SSL_HAND_SERVER_CERTREQUEST,
								  1 + 1 + ( ( sessionInfoPtr->version >= \
											  SSL_MINOR_VERSION_TLS12 ) ? \
										  ( UINT16_SIZE + 1 + 1 ) : 0 ) + \
								  UINT16_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	status = length = sgetc( stream );
	if( !cryptStatusError( status ) )
		{
		if( length < 1 || length > 64 )
			status = CRYPT_ERROR_BADDATA;
		}
	if( !cryptStatusError( status ) )
		status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid certificate request certificate-type "
				  "information" ) );
		}

	/* If it's TLS 1.2, skip the assorted additional algorithm 
	   information */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		status = length = readUint16( stream );
		if( !cryptStatusError( status ) )
			{
			if( length < UINT16_SIZE || length > 64 || \
				( length % UINT16_SIZE ) != 0 )
				status = CRYPT_ERROR_BADDATA;
			}
		if( !cryptStatusError( status ) )
			status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid certificate request signature/hash "
					  "algorithm information" ) );
			}
		}

	/* Finally, skip the CA name list.  Since the packet could be an over-
	   length, and therefore fragmented, packet we can't use 
	   readUniversal16() but have to apply custom proccessing that deals 
	   with over-long packets */
	status = length = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		{
		/* Zero-length CA name list */
		return( CRYPT_OK );
		}
	if( length < ( UINT16_SIZE + 16 ) || \
		length > sMemDataLeft( stream ) + ( MAX_PACKET_SIZE / 2 ) )
		status = CRYPT_ERROR_BADDATA;
	else
		{
		/* If the server sent an insanely long certificate request packet
		   that's fragmented across multiple SSL packets, process the
		   fragments */
		if( length > sMemDataLeft( stream ) )
			{
			status = processFragmentedRequest( sessionInfoPtr, handshakeInfo, 
											   stream, length );
			}
		else
			status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid certificate request CA name list" ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Handle Client/Server Keyex						*
*																			*
****************************************************************************/

/* Process the identity hint sent with the server keyex.  It's uncertain 
   what we're supposed to do with this so we just skip it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readIdentityHint( INOUT STREAM *stream )
	{
	int length, status;

	status = length = readUint16( stream );
	if( !cryptStatusError( status ) && !isShortIntegerRange( length ) )
		status = CRYPT_ERROR_BADDATA;
	if( !cryptStatusError( status ) && length > 0 )
		status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
	return( status );
	}

/* Read a server's DH/ECDH key agreement data:

	   DH:
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	   ECDH:
		byte		curveType
		uint16		namedCurve
		uint8		ecPointLen		-- NB uint8 not uint16
		byte[]		ecPoint */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readServerKeyexDH( INOUT STREAM *stream, 
							  OUT KEYAGREE_PARAMS *keyAgreeParams,
							  OUT_HANDLE_OPT CRYPT_CONTEXT *dhContextPtr,
							  const BOOLEAN isTLSLTS )
	{
	void *keyData;
	const int keyDataOffset = stell( stream );
	int keyDataLength, dummy, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyAgreeParams, sizeof( KEYAGREE_PARAMS ) ) );
	assert( isWritePtr( dhContextPtr, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( isTLSLTS == TRUE || isTLSLTS == FALSE );

	/* Clear return values */
	memset( keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	*dhContextPtr = CRYPT_ERROR;

	/* Read the server DH public key data */
	status = readInteger16UChecked( stream, NULL, &dummy, 
									MIN_PKCSIZE_THRESHOLD, 
									CRYPT_MAX_PKCSIZE );
	if( cryptStatusOK( status ) && isTLSLTS )
		{
		status = readInteger16UChecked( stream, NULL, &dummy, 
										MIN_PKCSIZE_THRESHOLD, 
										CRYPT_MAX_PKCSIZE );
		}
	if( cryptStatusOK( status ) )
		{
		status = readInteger16U( stream, NULL, &dummy, 1, 
								 CRYPT_MAX_PKCSIZE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Create a DH context from the public key data */
	keyDataLength = stell( stream ) - keyDataOffset;
	status = sMemGetDataBlockAbs( stream, keyDataOffset, &keyData, 
								  keyDataLength );
	if( cryptStatusOK( status ) )
		{
		status = initDHcontextSSL( dhContextPtr, keyData, keyDataLength, 
								   CRYPT_UNUSED, CRYPT_ECCCURVE_NONE, 
								   isTLSLTS );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the DH public value */
	return( readInteger16UChecked( stream, keyAgreeParams->publicValue,
								   &keyAgreeParams->publicValueLen,
								   MIN_PKCSIZE_THRESHOLD, 
								   CRYPT_MAX_PKCSIZE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readServerKeyexECDH( INOUT STREAM *stream, 
								OUT KEYAGREE_PARAMS *keyAgreeParams,
								OUT_HANDLE_OPT CRYPT_CONTEXT *dhContextPtr )
	{
	void *keyData;
	const int keyDataOffset = stell( stream );
	int keyDataLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyAgreeParams, sizeof( KEYAGREE_PARAMS ) ) );
	assert( isWritePtr( dhContextPtr, sizeof( CRYPT_CONTEXT ) ) );

	/* Clear return values */
	memset( keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	*dhContextPtr = CRYPT_ERROR;

	/* Read the server ECDH public key data */
	( void ) sgetc( stream );
	status = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Create an ECDH context from the public key data.  We set a dummy 
	   curve type, the actual value is determined by the parameters sent 
	   by the server */
	keyDataLength = stell( stream ) - keyDataOffset;
	status = sMemGetDataBlockAbs( stream, keyDataOffset, &keyData, 
								  keyDataLength );
	if( cryptStatusOK( status ) )
		{
		status = initDHcontextSSL( dhContextPtr, keyData, keyDataLength, 
								   CRYPT_UNUSED, CRYPT_ECCCURVE_P256, 
								   FALSE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the ECDH public value */
	return( readEcdhValue( stream, keyAgreeParams->publicValue,
						   CRYPT_MAX_PKCSIZE, 
						   &keyAgreeParams->publicValueLen ) );
	}

/* Process the optional server keyex:

		byte		ID = SSL_HAND_SERVER_KEYEXCHANGE
		uint24		len
	   DH:
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	 [	byte		hashAlgoID		-- TLS 1.2 ]
	 [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature 
	   DH-PSK:
		uint16		pskIdentityHintLen
		byte		pskIdentityHint
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	   ECDH:
		byte		curveType
		uint16		namedCurve
		uint8		ecPointLen		-- NB uint8 not uint16
		byte[]		ecPoint
	 [	byte		hashAlgoID		-- TLS 1.2 ]
	 [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 6 ) ) \
static int processServerKeyex( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							   INOUT STREAM *stream,
							   OUT_BUFFER( keyexPublicValueMaxLen, \
										   *keyexPublicValueLen ) \
									void *keyexPublicValue,
							   IN_LENGTH_SHORT const int keyexPublicValueMaxLen,
							   OUT_DATALENGTH_Z int *keyexPublicValueLen )

	{
	KEYAGREE_PARAMS keyAgreeParams, tempKeyAgreeParams;
	void *keyData DUMMY_INIT_PTR;
	const BOOLEAN isECC = isEccAlgo( handshakeInfo->keyexAlgo ) ? \
						  TRUE : FALSE;
	const BOOLEAN isPSK = ( handshakeInfo->authAlgo == CRYPT_ALGO_NONE ) ? \
						  TRUE : FALSE;
	const BOOLEAN isTLSLTS = TEST_FLAG( sessionInfoPtr->protocolFlags, 
										SSL_PFLAG_TLS12LTS ) ? \
							 TRUE : FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int keyDataOffset, keyDataLength DUMMY_INIT;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( keyexPublicValue, keyexPublicValueMaxLen ) );
	assert( isWritePtr( keyexPublicValueLen, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( keyexPublicValueMaxLen ) );

	/* Clear return values */
	memset( keyexPublicValue, 0, min( 16, keyexPublicValueMaxLen ) );
	*keyexPublicValueLen = 0;

	/* Make sure that we've got an appropriate server keyex packet.  We set 
	   the minimum key size to MIN_PKCSIZE_THRESHOLD/MIN_PKCSIZE_ECC_THRESHOLD 
	   instead of MIN_PKCSIZE/MIN_PKCSIZE_ECC in order to provide better 
	   diagnostics if the server is using weak keys since otherwise the data 
	   will be rejected in the packet read long before we get to the keysize 
	   check */
	status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
					SSL_HAND_SERVER_KEYEXCHANGE, 
					isECC ? \
						( 1 + UINT16_SIZE + \
						  1 + MIN_PKCSIZE_ECCPOINT_THRESHOLD + \
						  UINT16_SIZE + MIN_PKCSIZE_ECCPOINT_THRESHOLD ) : \
						( UINT16_SIZE + MIN_PKCSIZE_THRESHOLD + \
						  UINT16_SIZE + 1 + \
						  UINT16_SIZE + MIN_PKCSIZE_THRESHOLD + \
						  UINT16_SIZE + MIN_PKCSIZE_THRESHOLD ) );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "checkHSPacketHeader" );

	/* If we're using a PSK suite then the keyex information is preceded by 
	   a PSK identity hint */
	if( isPSK )
		{
		status = readIdentityHint( stream );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid PSK identity hint" ) );
			}
		}

	/* Read the server's keyex information and convert it into a DH/ECDH 
	   context */
	keyDataOffset = stell( stream );
	if( isECC )
		{
		status = readServerKeyexECDH( stream, &keyAgreeParams, 
									  &handshakeInfo->dhContext );
		}
	else
		{
		status = readServerKeyexDH( stream, &keyAgreeParams,
									&handshakeInfo->dhContext, isTLSLTS );
		}
	if( cryptStatusOK( status ) )
		{
		keyDataLength = stell( stream ) - keyDataOffset;
		status = sMemGetDataBlockAbs( stream, keyDataOffset, &keyData, 
									  keyDataLength );
		}
	if( cryptStatusError( status ) )
		{
		/* Some misconfigured servers may use very short keys, we perform a 
		   special-case check for these and return a more specific message 
		   than the generic bad-data error */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure key used in key exchange" ) );
			}

		retExt( cryptArgError( status ) ? \
				CRYPT_ERROR_BADDATA : status,
				( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status,
				  SESSION_ERRINFO, 
				  "Invalid server key agreement parameters" ) );
		}
	ANALYSER_HINT( keyData != NULL );
	CFI_CHECK_UPDATE( "readServerKeyex" );

#ifdef CONFIG_FUZZ
	/* Skip the server's signature, set up a dummy premaster secret, and 
	   exit */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		/* Skip algorithm IDs */
		sgetc( stream );
		sgetc( stream );
		}
	status = readUniversal16( stream );
	if( cryptStatusError( status ) )
		return( status );
	memset( handshakeInfo->premasterSecret, '*', SSL_SECRET_SIZE );
	handshakeInfo->premasterSecretSize = SSL_SECRET_SIZE;
	FUZZ_SKIP();
#endif /* CONFIG_FUZZ */

	/* Check the server's signature on the DH/ECDH parameters, unless it's a 
	   PSK suite in which case the exchange is authenticated via the PSK */
	if( !isPSK )
		{
		status = checkKeyexSignature( sessionInfoPtr, handshakeInfo, stream, 
									  keyData, keyDataLength, isECC );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Invalid server key agreement parameter signature" ) );
			}
		}
	CFI_CHECK_UPDATE( "checkKeyexSignature" );

	/* Perform phase 1 of the DH/ECDH key agreement process and save the 
	   result so that we can send it to the server later on.  The order of 
	   the SSL messages is a bit unfortunate since we get the one for phase 
	   2 before we need the phase 1 value, so we have to cache the phase 1 
	   result for when we need it later on */
	memset( &tempKeyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	status = krnlSendMessage( handshakeInfo->dhContext,
							  IMESSAGE_CTX_ENCRYPT, &tempKeyAgreeParams,
							  sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		{
		zeroise( &tempKeyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		return( status );
		}
	REQUIRES( rangeCheck( tempKeyAgreeParams.publicValueLen, 1,
						  keyexPublicValueMaxLen ) );
	memcpy( keyexPublicValue, tempKeyAgreeParams.publicValue,
			tempKeyAgreeParams.publicValueLen );
	*keyexPublicValueLen = tempKeyAgreeParams.publicValueLen;
	zeroise( &tempKeyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_ENCRYPT" );

	/* Perform phase 2 of the DH/ECDH key agreement */
	status = krnlSendMessage( handshakeInfo->dhContext, IMESSAGE_CTX_DECRYPT, 
							  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		return( status );
		}
	if( isECC && !isTLSLTS )
		{
		const int xCoordLen = ( keyAgreeParams.wrappedKeyLen - 1 ) / 2;

		/* The output of the ECDH operation is an ECC point, but for some 
		   unknown reason TLS only uses the x coordinate and not the full 
		   point.  To work around this we have to rewrite the point as a 
		   standalone x coordinate, which is relatively easy because we're 
		   using an uncompressed point format: 

			+---+---------------+---------------+
			|04	|		qx		|		qy		|
			+---+---------------+---------------+
				|<- fldSize --> |<- fldSize --> | */
		REQUIRES( keyAgreeParams.wrappedKeyLen >= MIN_PKCSIZE_ECCPOINT && \
				  keyAgreeParams.wrappedKeyLen <= MAX_PKCSIZE_ECCPOINT && \
				  ( keyAgreeParams.wrappedKeyLen & 1 ) == 1 && \
				  keyAgreeParams.wrappedKey[ 0 ] == 0x04 );
		REQUIRES( boundsCheck( 1, xCoordLen, CRYPT_MAX_PKCSIZE ) );
		memmove( keyAgreeParams.wrappedKey, 
				 keyAgreeParams.wrappedKey + 1, xCoordLen );
		keyAgreeParams.wrappedKeyLen = xCoordLen;
		}
	REQUIRES( rangeCheck( keyAgreeParams.wrappedKeyLen, 1,
						  CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE ) );
	memcpy( handshakeInfo->premasterSecret, keyAgreeParams.wrappedKey,
			keyAgreeParams.wrappedKeyLen );
	handshakeInfo->premasterSecretSize = keyAgreeParams.wrappedKeyLen;
	zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_DECRYPT" );

	REQUIRES( CFI_CHECK_SEQUENCE_5( "checkHSPacketHeader", "readServerKeyex", 
									"checkKeyexSignature", "IMESSAGE_CTX_ENCRYPT", 
									"IMESSAGE_CTX_DECRYPT" ) );
	return( CRYPT_OK );
	}

/* Build the client key exchange packet:

	  [	byte		ID = SSL_HAND_CLIENT_KEYEXCHANGE ]
	  [	uint24		len				-- Written by caller ]
	   DH:
		uint16		yLen
		byte[]		y
	   DH-PSK:
		uint16		userIDLen
		byte[]		userID
		uint16		yLen
		byte[]		y
	   ECDH:
		uint8		ecPointLen		-- NB uint8 not uint16
		byte[]		ecPoint
	   PSK:
		uint16		userIDLen
		byte[]		userID
	   RSA:
	  [ uint16		encKeyLen		-- TLS only ]
		byte[]		rsaPKCS1( byte[2] { 0x03, 0x0n } || byte[46] random ) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int createClientKeyex( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							  INOUT STREAM *stream,
							  IN_BUFFER_OPT( keyexPublicValueLen ) \
								const BYTE *keyexPublicValue,
							  IN_LENGTH_PKC_Z const int keyexPublicValueLen,
							  const BOOLEAN isPSK )
	{
	const ATTRIBUTE_LIST *userNamePtr DUMMY_INIT_PTR;
	const ATTRIBUTE_LIST *passwordPtr DUMMY_INIT_PTR;
#ifdef USE_RSA_SUITES
	BYTE wrappedKey[ CRYPT_MAX_PKCSIZE + 8 ];
	int wrappedKeyLength;
#endif /* USE_RSA_SUITES */
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ( keyexPublicValue == NULL && keyexPublicValueLen == 0 ) || \
			isReadPtrDynamic( keyexPublicValue, keyexPublicValueLen ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( ( keyexPublicValue == NULL && keyexPublicValueLen == 0 ) || \
			  ( keyexPublicValueLen > 0 && \
				keyexPublicValueLen <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( isPSK == TRUE || isPSK == FALSE );

	/* If we're using a PSK mechanism, get the user name and password/PSK */
	if( isPSK )
		{
		userNamePtr = findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME );
		passwordPtr = findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD );

		REQUIRES( passwordPtr != NULL );
		REQUIRES( userNamePtr != NULL );
		}

	/* If we're using DH/ECDH (with optional PSK additions), write the 
	   necessary information */
	if( keyexPublicValue != NULL )
		{
		/* If it's a PSK algorithm, convert the DH/ECDH premaster secret to
		   a DH/ECDH + PSK premaster secret and write the client identity 
		   that's required by PSK */
		if( isPSK )
			{
			BYTE premasterTempBuffer[ CRYPT_MAX_PKCSIZE + 8 ];
			const int premasterTempSize = handshakeInfo->premasterSecretSize;

			/* Since this operation rewrites the premaster secret, we have to
			   save the original contents into a temporary buffer first */
			REQUIRES( rangeCheck( handshakeInfo->premasterSecretSize, 1,
								  CRYPT_MAX_PKCSIZE ) );
			memcpy( premasterTempBuffer, handshakeInfo->premasterSecret,
					handshakeInfo->premasterSecretSize );
			status = createSharedPremasterSecret( \
							handshakeInfo->premasterSecret,
							CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE,
							&handshakeInfo->premasterSecretSize,
							passwordPtr->value, 
							passwordPtr->valueLength, 
							premasterTempBuffer, premasterTempSize,
							TEST_FLAG( passwordPtr->flags, 
									   ATTR_FLAG_ENCODEDVALUE ) ? \
								TRUE : FALSE );
			zeroise( premasterTempBuffer, premasterTempSize );
			if( cryptStatusError( status ) )
				{
				retExt( status,
						( status, SESSION_ERRINFO, 
						  "Couldn't create master secret from shared "
						  "secret/password value" ) );
				}

			/* Write the PSK client identity */
			writeUint16( stream, userNamePtr->valueLength );
			status = swrite( stream, userNamePtr->value,
							 userNamePtr->valueLength );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Write the DH/ECDH public value that we saved earlier when we
		   performed phase 1 of the key agreement process */
		if( isEccAlgo( handshakeInfo->keyexAlgo ) )
			{
			sputc( stream, keyexPublicValueLen );
			status = swrite( stream, keyexPublicValue,
							 keyexPublicValueLen );
			}
		else
			{
			status = writeInteger16U( stream, keyexPublicValue,
									  keyexPublicValueLen );
			}
		
		return( status );
		}

	/* To get to this point, we can't have been using DH/ECDH */
	REQUIRES( keyexPublicValue == NULL && keyexPublicValueLen == 0 );

	/* If we're using straight PSK, write the client identity */
	if( isPSK )
		{
		/* Create the shared premaster secret from the user password */
		status = createSharedPremasterSecret( \
							handshakeInfo->premasterSecret,
							CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE,
							&handshakeInfo->premasterSecretSize,
							passwordPtr->value, 
							passwordPtr->valueLength, NULL, 0,
							TEST_FLAG( passwordPtr->flags, 
									   ATTR_FLAG_ENCODEDVALUE ) ? \
								TRUE : FALSE );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Couldn't create master secret from shared "
					  "secret/password value" ) );
			}

		/* Write the PSK client identity */
		writeUint16( stream, userNamePtr->valueLength );
		return( swrite( stream, userNamePtr->value,
						userNamePtr->valueLength ) );
		}

#ifdef USE_RSA_SUITES
	/* It's an RSA keyex, write the RSA-wrapped premaster secret */
	status = wrapPremasterSecret( sessionInfoPtr, handshakeInfo, wrappedKey, 
								  CRYPT_MAX_PKCSIZE, &wrappedKeyLength );
	if( cryptStatusError( status ) )
		return( status );
#ifdef USE_SSL3
	if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
		{
		/* The original Netscape SSL implementation didn't provide a length 
		   for the encrypted key and everyone copied that so it became the 
		   de facto standard way to do it (sic faciunt omnes.  The spec 
		   itself is ambiguous on the topic).  This was fixed in TLS 
		   (although the spec is still ambiguous) so the encoding differs 
		   slightly between SSL and TLS */
		return( swrite( stream, wrappedKey, wrappedKeyLength ) );
		}
#endif /* USE_SSL3 */

	return( writeInteger16U( stream, wrappedKey, wrappedKeyLength ) );
#else
	retIntError();
#endif /* USE_RSA_SUITES */
	}

/****************************************************************************
*																			*
*					Server Certificate Checking Functions					*
*																			*
****************************************************************************/

/* Matching names in certificates can get quite tricky due to the usual 
   erratic nature of what gets put in them.  Firstly, supposed FQDNs can be 
   unqualified names and/or can be present as full URLs rather than just 
   domain names, so we use sNetParseURL() on them before doing anything else 
   with them.  Secondly, PKIX tries to pretend that wildcard certificates 
   don't exist, and so there are no official guidelines for how they should 
   be laid out.  To minimise the potential for mischief we only allow a
   wildcard at the start of the domain, and don't allow wildcards for the 
   first- or second-level names.
   
   Since this code is going to result in server names (and therefore 
   connections) being rejected, it's unusually loquacious about the reasons 
   for the rejection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int matchName( INOUT_BUFFER_FIXED( serverNameLength ) \
						BYTE *serverName,
					  IN_LENGTH_DNS const int serverNameLength,
					  INOUT_BUFFER_FIXED( originalCertNameLength ) \
						BYTE *certName,
					  IN_LENGTH_DNS const int originalCertNameLength,
					  OUT ERROR_INFO *errorInfo )
	{
	URL_INFO urlInfo;
	BOOLEAN hasWildcard = FALSE;
	int certNameLength, dotCount = 0, i, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( serverName, serverNameLength ) );
	assert( isWritePtrDynamic( certName, originalCertNameLength ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( serverNameLength > 0 && serverNameLength <= MAX_DNS_SIZE );
	REQUIRES( originalCertNameLength > 0 && \
			  originalCertNameLength <= MAX_DNS_SIZE );

	/* Clear return value */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Extract the FQDN portion from the certificate name */
	status = sNetParseURL( &urlInfo, certName, originalCertNameLength, 
						   URL_TYPE_NONE );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo,
				  "Invalid host name '%s' in server's certificate",
				  sanitiseString( certName, CRYPT_MAX_TEXTSIZE,
								  originalCertNameLength ) ) );
		}
	certName = ( BYTE * ) urlInfo.host;
	certNameLength = urlInfo.hostLen;
	ENSURES( certNameLength > 0 && certNameLength <= MAX_URL_SIZE ); 

	/* If the name in the certificate is longer than the server name then it 
	   can't be a match */
	if( certNameLength > serverNameLength )
		{
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo,
				  "Server name '%s' doesn't match host name '%s' in "
				  "server's certificate", 
				  sanitiseString( serverName, CRYPT_MAX_TEXTSIZE,
								  serverNameLength ),
				  sanitiseString( certName, CRYPT_MAX_TEXTSIZE,
								  certNameLength ) ) );
		}

	/* Make sure that, if it's a wildcarded name, it follows the pattern 
	   "'*' ... '.' ... '.' ..." */
	LOOP_EXT( i = 0, i < certNameLength, i++, MAX_URL_SIZE + 1 )
		{
		const int ch = byteToInt( certName[ i ] );

		if( ch == '*' )
			{
			if( i != 0 )
				{
				/* The wildcard character isn't the first one in the name, 
				   it's invalid */
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, errorInfo,
						  "Host name '%s' in server's certificate contains "
						  "wildcard at invalid location",
						  sanitiseString( certName, CRYPT_MAX_TEXTSIZE,
										  certNameLength ) ) );
				}
			hasWildcard = TRUE;
			}
		if( ch == '.' )
			dotCount++;
		}
	ENSURES( LOOP_BOUND_OK );
	if( hasWildcard && dotCount < 2 )
		{
		/* The wildcard applies to the first- or second-level domain, it's 
		   invalid */
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, errorInfo,
				  "Host name '%s' in server's certificate contains "
				  "wildcard at invalid domain level",
				  sanitiseString( certName, CRYPT_MAX_TEXTSIZE,
								  certNameLength ) ) );
		}

	/* Match the certificate name and the server name, taking into account
	   wildcarding */
	if( hasWildcard )
		{
		const int delta = serverNameLength - ( certNameLength - 1 );

		ENSURES_B( delta > 0 && delta < serverNameLength );

		/* Match the suffix past the wildcard */
		if( !memcmp( certName + 1, serverName + delta, certNameLength - 1 ) )
			return( CRYPT_OK );
		}
	else
		{
		/* It's a straight match */
		if( certNameLength == serverNameLength && \
			!memcmp( certName, serverName, certNameLength ) )
			return( CRYPT_OK );
		}

	/* The name doesn't match */
	retExt( CRYPT_ERROR_INVALID,
			( CRYPT_ERROR_INVALID, errorInfo,
			  "Server name '%s' doesn't match host name '%s' in server's " 
			  "certificate", 
			  sanitiseString( serverName, CRYPT_MAX_TEXTSIZE,
							  serverNameLength ),
			  sanitiseString( certName, CRYPT_MAX_TEXTSIZE,
							  certNameLength ) ) );
	}

/* Check the remote host name against one or more server names in the 
   certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 5 ) ) \
static int checkHostName( const CRYPT_CERTIFICATE iCryptCert,
						  INOUT_BUFFER_FIXED( serverNameLength ) void *serverName,
						  IN_LENGTH_DNS const int serverNameLength,
						  const BOOLEAN multipleNamesPresent,
						  OUT ERROR_INFO *errorInfo )
	{
	MESSAGE_DATA msgData;
	static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
	static const int altNameValue = CRYPT_CERTINFO_SUBJECTALTNAME;
	static const int dnsNameValue = CRYPT_CERTINFO_DNSNAME;
	char certName[ MAX_DNS_SIZE + 8 ];
	int certNameLength = CRYPT_ERROR, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( serverName, serverNameLength ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( serverNameLength > 0 && serverNameLength <= MAX_DNS_SIZE );
	REQUIRES( multipleNamesPresent == TRUE || \
			  multipleNamesPresent == FALSE );

	/* Clear return value */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Get the CN and check it against the host name */
	setMessageData( &msgData, certName, MAX_DNS_SIZE );
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_CERTINFO_COMMONNAME );
	if( cryptStatusOK( status ) )
		{
		certNameLength = msgData.length;
		status = matchName( serverName, serverNameLength, certName,
							certNameLength, errorInfo );
		if( cryptStatusOK( status ) )
			return( status );

		/* If this was the only name that's present then we can't go any 
		   further (the extended error information will have been provided 
		   by matchName()) */
		if( !multipleNamesPresent )
			return( CRYPT_ERROR_INVALID );
		}

	/* The CN didn't match, check the altName */
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							  ( MESSAGE_CAST ) &altNameValue, 
							  CRYPT_ATTRIBUTE_CURRENT );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &dnsNameValue, 
								  CRYPT_ATTRIBUTE_CURRENT_INSTANCE );
		}
	if( cryptStatusError( status ) )
		{
		/* We couldn't find a DNS name in the altName, which means that the 
		   server name doesn't match the name (from the previously-used CN) 
		   in the certificate.
		   
		   If there's no CN present then there's no certificate host name to 
		   use in the error message and we have to construct our own one, 
		   otherwise it'll have been provided by the previous call to 
		   matchName() */
		krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
						 ( MESSAGE_CAST ) &nameValue, 
						 CRYPT_ATTRIBUTE_CURRENT );	/* Re-select subject DN */
		if( certNameLength == CRYPT_ERROR )
			{
			retExt( CRYPT_ERROR_INVALID,
					( CRYPT_ERROR_INVALID, errorInfo,
					  "Server name '%s' doesn't match host name in "
					  "server's certificate",
					  sanitiseString( serverName, CRYPT_MAX_TEXTSIZE,
									  serverNameLength ) ) );
			}
		return( CRYPT_ERROR_INVALID );	/* See comment above */
		}
	LOOP_LARGE_CHECKINC( cryptStatusOK( status ), 
						 status = krnlSendMessage( iCryptCert, 
											IMESSAGE_SETATTRIBUTE, 
											MESSAGE_VALUE_CURSORNEXT,
											CRYPT_ATTRIBUTE_CURRENT_INSTANCE ) )
		{
		setMessageData( &msgData, certName, MAX_DNS_SIZE );
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_CERTINFO_DNSNAME );
		if( cryptStatusOK( status ) )
			{
			status = matchName( serverName, serverNameLength, certName,
								msgData.length, errorInfo );
			if( cryptStatusOK( status ) )
				return( status );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &nameValue, CRYPT_ATTRIBUTE_CURRENT );
					 /* Re-select subject DN */

	/* We couldn't find any name that matches the server name */
	retExt( CRYPT_ERROR_INVALID,
			( CRYPT_ERROR_INVALID, errorInfo,
			  "Server name '%s' doesn't match any of the host names in "
			  "server's certificate",
			  sanitiseString( serverName, CRYPT_MAX_TEXTSIZE,
							  serverNameLength ) ) );
	}

/* Check that the certificate from the remote system is in order.  This 
   involves checking that the name of the host that we've connected to
   matches one of the names in the certificate, and that the certificate 
   itself is in order */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkCertificateInfo( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const CRYPT_CERTIFICATE iCryptCert = sessionInfoPtr->iKeyexCryptContext;
	MESSAGE_DATA msgData;
	const ATTRIBUTE_LIST *serverNamePtr = \
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_SERVER_NAME );
	static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
	static const int altNameValue = CRYPT_CERTINFO_SUBJECTALTNAME;
	const int verifyFlags = \
				GET_FLAGS( sessionInfoPtr->protocolFlags,
						   SSL_PFLAG_DISABLE_NAMEVERIFY | \
						   SSL_PFLAG_DISABLE_CERTVERIFY );
	BOOLEAN multipleNamesPresent = FALSE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* If all verification has been disabled then there's nothing to do */
	if( verifyFlags == ( SSL_PFLAG_DISABLE_NAMEVERIFY | \
						 SSL_PFLAG_DISABLE_CERTVERIFY ) )
		return( CRYPT_OK );

	/* The server name is traditionally given as the certificate's CN, 
	   however it may also be present as an altName.  First we check whether 
	   there's an altName present, this is used to control error handling 
	   since if there isn't one present we stop after a failed CN check and 
	   if there is one present we continue on to the altName(s) */
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							  ( MESSAGE_CAST ) &altNameValue, 
							  CRYPT_ATTRIBUTE_CURRENT );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_CERTINFO_DNSNAME );
		if( cryptStatusOK( status ) )
			multipleNamesPresent = TRUE;
		}
	krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
					 ( MESSAGE_CAST ) &nameValue, CRYPT_ATTRIBUTE_CURRENT );
					 /* Re-select the subject DN */

	/* If there's a server name present and name checking hasn't been 
	   disabled, make sure that it matches one of the names in the 
	   certificate */
	if( serverNamePtr != NULL && \
		!( verifyFlags & SSL_PFLAG_DISABLE_NAMEVERIFY ) )
		{
		status = checkHostName( iCryptCert, serverNamePtr->value, 
								serverNamePtr->valueLength, 
								multipleNamesPresent, SESSION_ERRINFO );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If certificate verification hasn't been disabled, make sure that the 
	   server's certificate verifies */
	if( !( verifyFlags & SSL_PFLAG_DISABLE_CERTVERIFY ) )
		{
		/* This is something that can't be easily enabled by default since 
		   cryptlib isn't a web browser and therefore won't follow the 
		   "trust anything from a commercial CA" model.  In particular 
		   cryptlib users tend to fall into two classes, commercial/
		   government users, often in high-security environments, who run 
		   their own PKIs and definitely won't want anything from a 
		   commercial CA to be accepted, and noncommercial users who won't 
		   be buying certificates from a commercial CA.  In neither of these 
		   cases is trusting certs from a commercial CA useful, in the 
		   former case it leads to a serious security breach (as some US 
		   government agencies have discovered), in the latter case it leads 
		   to all certificates being rejected since they weren't bought from 
		   a commercial CA.  The recommended solutions to this problem are 
		   covered in the cryptlib manual */
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Client-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int beginClientHandshake( INOUT SESSION_INFO *sessionInfoPtr,
								 INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
#ifndef NO_SESSION_CACHE
	const ATTRIBUTE_LIST *attributeListPtr;
#endif /* NO_SESSION_CACHE */
	STREAM *stream = &handshakeInfo->stream;
	SCOREBOARD_INFO scoreboardInfo = { 0 };
	MESSAGE_DATA msgData;
	BYTE sentSessionID[ MAX_SESSIONID_SIZE + 8 ];
	BOOLEAN sessionIDsent = FALSE, resumeSession = FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int packetOffset, clientHelloLength, serverHelloLength;
	int sentSessionIDlength DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Check whether we have (potentially) cached data available for the
	   server.  If we've had the connection to the remote system provided
	   by the user (for example as an already-connected socket) then there
	   won't be any server name information present, so we can only
	   perform a session resume if we've established the connection
	   ourselves */
#ifndef NO_SESSION_CACHE
	attributeListPtr = findSessionInfo( sessionInfoPtr, 
										CRYPT_SESSINFO_SERVER_NAME );
	if( attributeListPtr != NULL )
		{
		void *scoreboardInfoPtr = \
				DATAPTR_GET( sessionInfoPtr->sessionSSL->scoreboardInfoPtr );
		int resumedSessionID;

		REQUIRES( scoreboardInfoPtr != NULL );

		resumedSessionID = \
			lookupScoreboardEntry( scoreboardInfoPtr, SCOREBOARD_KEY_FQDN, 
								   attributeListPtr->value,
								   attributeListPtr->valueLength, 
								   &scoreboardInfo );
		if( !cryptStatusError( resumedSessionID ) )
			{
			/* We've got cached data for the server available, remember the 
			   session ID so that we can send it to the server */
			status = attributeCopyParams( handshakeInfo->sessionID, 
										  MAX_SESSIONID_SIZE, 
										  &handshakeInfo->sessionIDlength,
										  scoreboardInfo.key, 
										  scoreboardInfo.keySize );
			ENSURES( cryptStatusOK( status ) );

			/* Make a copy of the session ID that we're sending so that we 
			   can check it against what the server sends back to us later.  
			   This is required for when the server can't resume the session
			   and sends us a fresh session ID */
			REQUIRES( rangeCheck( handshakeInfo->sessionIDlength, 1, 
								  MAX_SESSIONID_SIZE ) );
			memcpy( sentSessionID, handshakeInfo->sessionID, 
					handshakeInfo->sessionIDlength );
			sentSessionIDlength = handshakeInfo->sessionIDlength;
			}
		}
#endif /* NO_SESSION_CACHE */
	CFI_CHECK_UPDATE( "lookupScoreboardEntry" );

	/* Get the nonce that's used to randomise all crypto ops */
	setMessageData( &msgData, handshakeInfo->clientNonce, SSL_NONCE_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusError( status ) )
		return( status );

	/* Build the client hello packet:

		byte		ID = SSL_HAND_CLIENT_HELLO
		uint24		len
		byte[2]		version = { 0x03, 0x0n }
		byte[32]	nonce
		byte		sessIDlen
		byte[]		sessID
		uint16		suiteLen
		uint16[]	suite
		byte		coprLen = 1
		byte[]		copr = { 0x00 }
	  [	uint16	extListLen			-- RFC 3546/RFC 4366/RFC 6066
			byte	extType
			uint16	extLen
			byte[]	extData ] 

	   Extensions present a bit of an interoperability problem on the client
	   side, if we use them then we have to add them to the client hello 
	   before we know whether the server can handle them, and some older 
	   servers can't.  This is particularly bad in cryptlib's case where 
	   it's used with embedded systems, which have ancient and buggy SSL/TLS
	   implementations that are rarely if ever updated.  A reasonable 
	   compromise is to only enable the use of extensions when the user has 
	   asked for TLS 1.1+ support, however if ECC suites are being used then
	   we have to write them because they're required for ECC use */
	status = openPacketStreamSSL( stream, sessionInfoPtr, CRYPT_USE_DEFAULT,
								  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	status = continueHSPacketStream( stream, SSL_HAND_CLIENT_HELLO, 
									 &packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	sputc( stream, SSL_MAJOR_VERSION );
	sputc( stream, sessionInfoPtr->version );
	handshakeInfo->clientOfferedVersion = sessionInfoPtr->version;
	swrite( stream, handshakeInfo->clientNonce, SSL_NONCE_SIZE );
	sputc( stream, handshakeInfo->sessionIDlength );
	if( handshakeInfo->sessionIDlength > 0 )
		{
		swrite( stream, handshakeInfo->sessionID, 
				handshakeInfo->sessionIDlength );
		sessionIDsent = TRUE;
		}
	status = writeCipherSuiteList( stream, sessionInfoPtr->version,
						findSessionInfo( sessionInfoPtr,
										 CRYPT_SESSINFO_USERNAME ) != NULL ? \
							TRUE : FALSE,
							TEST_FLAG( sessionInfoPtr->protocolFlags, 
									   SSL_PFLAG_SUITEB ) ? TRUE : FALSE );
	if( cryptStatusOK( status ) )
		{
		sputc( stream, 1 );		/* No compression */
		status = sputc( stream, 0 );
		}
	if( cryptStatusOK( status ) && \
		( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS11 || \
		  ( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS && \
			algoAvailable( CRYPT_ALGO_ECDH ) ) ) )
		{
		/* Extensions are only written when newer versions of TLS are 
		   enabled, unless they're required for ECC use, see the comment 
		   at the start of this code block for details */
		status = writeClientExtensions( stream, sessionInfoPtr );
		}
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusOK( status ) )
		status = sendPacketSSL( sessionInfoPtr, stream, FALSE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	clientHelloLength = stell( stream ) - SSL_HEADER_SIZE;
	CFI_CHECK_UPDATE( "sendPacketSSL" );

	/* Perform the assorted hashing of the client hello in between the 
	   network ops where it's effectively free */
	status = hashHSPacketWrite( handshakeInfo, stream, 0 );
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "hashHSPacketWrite" );

	/* Process the server hello.  The server will usually send us a session 
	   ID to indicate a resumable session (if we've sent one in our 
	   request).  This indicated by a return status of OK_SPECIAL, if this 
	   is present then we try and resume the session.

	   Note that this processing leads to a slight inefficiency in hashing 
	   when multiple hash algorithms need to be accommodated (as they do
	   when TLS 1.2+ is enabled) because readHSPacketSSL() hashes the 
	   incoming packet data as it arrives, and if all possible server 
	   handshake packets are combined into a single SSL message packet then 
	   they'll arrive, and need to be hashed, before the server hello is
	   processed and we can selectively disable the hash algorithms that
	   won't be needed.  Fixing this would require adding special-case
	   processing to readHSPacketSSL() to detect the use of 
	   SSL_MSG_FIRST_HANDSHAKE for the client and skip the hashing, relying
	   on the calling code to then pick out the portions that need to be
	   hashed.  This probably isn't worth the effort required, since it adds
	   a lot of code complexity and custom processing just to save a small
	   amount of hashing on the client, which will generally be the less
	   resource-constrained of the two parties */
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, 
							  &serverHelloLength, SSL_MSG_FIRST_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, serverHelloLength );
	status = processHelloSSL( sessionInfoPtr, handshakeInfo, stream, FALSE );
	if( status == OK_SPECIAL )
		{
		/* The server has provided a session ID, if we sent one in our 
		   request and it matches what the server returned then this is a 
		   resumed session */
		if( sessionIDsent && \
			handshakeInfo->sessionIDlength == sentSessionIDlength && \
			!memcmp( handshakeInfo->sessionID, sentSessionID,
					 sentSessionIDlength ) )
			{
			DEBUG_PUTS(( "Server negotiated session resumption" ));
			resumeSession = TRUE;
			}
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	serverHelloLength = stell( stream );	/* May be followed by other packets */
	CFI_CHECK_UPDATE( "processHelloSSL" );

	/* The server has acknowledged our attempt to resume a session, handle 
	   the session resumption */
	if( resumeSession )
		{
		sMemDisconnect( stream );

		/* We're resuming a previous session, if extended TLS facilities 
		   were in use then make sure that the resumed session uses the same 
		   facilities */
		if( !TEST_FLAGS( sessionInfoPtr->protocolFlags, 
						 SSL_RESUMEDSESSION_FLAGS, 
						 scoreboardInfo.metaData ) )
			{
			DEBUG_PRINT(( "Server negotiated resumption of session with "
						  "options %x using options %x.\n", 
						  scoreboardInfo.metaData,
						  GET_FLAGS( sessionInfoPtr->protocolFlags,
									 SSL_RESUMEDSESSION_FLAGS ) ));
			return( CRYPT_ERROR_INVALID );
			}

		/* Remember the premaster secret for the resumed session */
		status = attributeCopyParams( handshakeInfo->premasterSecret, 
									  SSL_SECRET_SIZE,
									  &handshakeInfo->premasterSecretSize,
									  scoreboardInfo.data, 
									  scoreboardInfo.dataSize );
		ENSURES( cryptStatusOK( status ) );

		/* Tell the caller that it's a resumed session */
		DEBUG_PRINT(( "Resuming session with server based on "
					  "sessionID = \n" ));
		DEBUG_DUMP_DATA( handshakeInfo->sessionID, 
						 handshakeInfo->sessionIDlength );
		status = OK_SPECIAL;
		}
	CFI_CHECK_UPDATE( "resumeSession" );

	/* TLS 1.2 LTS implicitly enables various other crypto options, now that
	   we've got past the initial negotiations, enable those too */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_TLS12LTS ) )
		{
		SET_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_EMS );
		if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
			{
			SET_FLAG( sessionInfoPtr->protocolFlags, 
					  SSL_PFLAG_ENCTHENMAC );
			}

		/* Hash the hello messages */
		getHashParameters( CRYPT_ALGO_SHA2, 0, &handshakeInfo->helloHashFunction, 
						   &handshakeInfo->helloHashSize );
		handshakeInfo->helloHashFunction( handshakeInfo->helloHashInfo, NULL, 0,
										  sessionInfoPtr->sendBuffer + SSL_HEADER_SIZE, 
										  clientHelloLength, HASH_STATE_START );
		handshakeInfo->helloHashFunction( handshakeInfo->helloHashInfo, NULL, 0, 
										  sessionInfoPtr->receiveBuffer, 
										  serverHelloLength, HASH_STATE_CONTINUE );
		}
	CFI_CHECK_UPDATE( "TLS12LTS" );

	/* Return CRYPT_OK for a standard session, OK_SPECIAL for a resumed 
	   one */
	REQUIRES( CFI_CHECK_SEQUENCE_6( "lookupScoreboardEntry", "sendPacketSSL", 
									"hashHSPacketWrite", "processHelloSSL",
									"resumeSession", "TLS12LTS" ) );
	return( status );
	}

/* Exchange keys with the server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int exchangeClientKeys( INOUT SESSION_INFO *sessionInfoPtr,
							   INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	BYTE keyexPublicValue[ CRYPT_MAX_PKCSIZE + 8 ];
	BOOLEAN needClientCert = FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int packetOffset, length, keyexPublicValueLen = 0, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Process the optional server supplemental data:

		byte		ID = SSL_HAND_SUPPLEMENTAL_DATA
		uint24		len
		uint16		type
		uint16		len
		byte[]		value

	   This is a kitchen-sink mechanism for exchanging arbitrary further 
	   data during the TLS handshake (see RFC 4680).  The presence of the
	   supplemental data has to be negotiated using TLS extensions, however
	   the nature of this negotiation is unspecified so we can't just
	   reject an unexpected supplemental data message as required by the RFC 
	   because it may have been quite legitimately negotiated by a TLS
	   extension that we don't know about.  Because of this we perform
	   basic validity checks on any supplemental data messages that arrive
	   but otherwise ignore them */
	status = refreshHSStream( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sPeek( stream ) == SSL_HAND_SUPPLEMENTAL_DATA )
		{
		status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
									  SSL_HAND_SUPPLEMENTAL_DATA, 
									  UINT16_SIZE + UINT16_SIZE + 1 );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		readUint16( stream );				/* Type */
		status = readUniversal16( stream );	/* Value */
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid supplemental data" ) );
			}
		DEBUG_PRINT(( "Server sent %d bytes TLS supplemental data.\n",
					  length ));
		assert_nofuzz( DEBUG_WARN );
		}
	CFI_CHECK_UPDATE( "checkHSPacketHeader" );

#ifndef CONFIG_FUZZ
	/* Process the optional server certificate chain:

		byte		ID = SSL_HAND_CERTIFICATE
		uint24		len
		uint24		certLen			-- 1...n certificates ordered
		byte[]		certificate		-- leaf -> root */
	if( handshakeInfo->authAlgo != CRYPT_ALGO_NONE )
		{
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );
		status = readSSLCertChain( sessionInfoPtr, handshakeInfo,
							stream, &sessionInfoPtr->iKeyexCryptContext,
							FALSE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}

		/* Check the details in the certificate chain */
		status = checkCertificateInfo( sessionInfoPtr );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
#endif /* CONFIG_FUZZ */
	CFI_CHECK_UPDATE( "readSSLCertChain" );

	/* Process the optional server keyex */
	if( isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );

		status = processServerKeyex( sessionInfoPtr, handshakeInfo, 
									 stream, keyexPublicValue, 
									 CRYPT_MAX_PKCSIZE, 
									 &keyexPublicValueLen );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
	else
		{
		/* If it's a pure PSK mechanism then there may be a pointless server 
		   keyex containing an identity hint whose purpose is never 
		   explained (more specifically the RFC makes it a SHOULD NOT, and
		   clients MUST ignore it), in which case we have to process the 
		   packet to get rid of the identity hint */
		if( handshakeInfo->authAlgo == CRYPT_ALGO_NONE )
			{
			status = refreshHSStream( sessionInfoPtr, handshakeInfo );
			if( cryptStatusError( status ) )
				return( status );
			if( sPeek( stream ) == SSL_HAND_SUPPLEMENTAL_DATA )
				{
				status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
											  SSL_HAND_SERVER_KEYEXCHANGE, 
											  ( UINT16_SIZE + 1 ) );
				if( cryptStatusError( status ) )
					return( status );
				status = readIdentityHint( stream );
				if( cryptStatusError( status ) )
					{
					retExt( CRYPT_ERROR_BADDATA,
							( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
							  "Invalid PSK identity hint" ) );
					}
				}
			}

#ifdef CONFIG_FUZZ
		/* Set up a dummy premaster secret */
		memset( handshakeInfo->premasterSecret, '*', SSL_SECRET_SIZE );
		handshakeInfo->premasterSecretSize = SSL_SECRET_SIZE;
#endif /* CONFIG_FUZZ */
		
		/* Keep static analysers happy */
		memset( keyexPublicValue, 0, CRYPT_MAX_PKCSIZE );
		}
	CFI_CHECK_UPDATE( "processServerKeyex" );

	/* Process the optional server certificate request.  Since we're about 
	   to peek ahead into the stream to see if we need to process a server 
	   certificate request, we have to refresh the stream at this point in 
	   case the certificate request wasn't bundled with the preceding 
	   packets */
	status = refreshHSStream( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sPeek( stream ) == SSL_HAND_SERVER_CERTREQUEST )
		{
		status = processCertRequest( sessionInfoPtr, handshakeInfo, 
									 stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		needClientCert = TRUE;
		}
	CFI_CHECK_UPDATE( "processCertRequest" );

	/* Process the server hello done:

		byte		ID = SSL_HAND_SERVER_HELLODONE
		uint24		len = 0 */
	status = refreshHSStream( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );
	status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
								  SSL_HAND_SERVER_HELLODONE, 0 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}

	/* If we're fuzzing the input then we don't need to go through any of 
	   the following crypto calisthenics */
	FUZZ_SKIP();

	/* If we need a client certificate, build the client certificate packet */
	status = openPacketStreamSSL( stream, sessionInfoPtr, CRYPT_USE_DEFAULT,
								  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	if( needClientCert )
		{
		BOOLEAN sentResponse = FALSE;

		/* If we haven't got a certificate available, tell the server.  SSL 
		   and TLS differ here, SSL sends a no-certificate alert while TLS 
		   sends an empty client certificate packet, which is handled 
		   further on */
		if( sessionInfoPtr->privateKey == CRYPT_ERROR )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
#ifdef USE_SSL3
			if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
				{
				static const BYTE noCertAlertSSLTemplate[] = {
					SSL_MSG_ALERT,							/* ID */
					SSL_MAJOR_VERSION, SSL_MINOR_VERSION_SSL,/* Version */
					0, 2,									/* Length */
					SSL_ALERTLEVEL_WARNING, SSL_ALERT_NO_CERTIFICATE
					};

				/* This is an alert-protocol message rather than a handshake
				   message so we don't add it to the handshake packet stream
				   but write it directly to the network stream */
				swrite( &sessionInfoPtr->stream, noCertAlertSSLTemplate, 7 );
				sentResponse = TRUE;
				}
#endif /* USE_SSL3 */

			/* The reaction to the lack of a certificate is up to the server 
			   (some just request one anyway even though they can't do 
			   anything with it) so from here on we just continue as if 
			   nothing had happened */
			SET_FLAG( sessionInfoPtr->protocolFlags, 
					  SSL_PFLAG_CLIAUTHSKIPPED );
			needClientCert = FALSE;
			}

		/* If we haven't sent a response yet, send it now.  If no private 
		   key is available this will send the zero-length chain that's 
		   required by TLS  */
		if( !sentResponse )
			{
			status = writeSSLCertChain( sessionInfoPtr, stream );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( stream );
				return( status );
				}
			}
		}
	CFI_CHECK_UPDATE( "writeSSLCertChain" );

	/* Build the client key exchange packet */
	status = continueHSPacketStream( stream, SSL_HAND_CLIENT_KEYEXCHANGE,
									 &packetOffset );
	if( cryptStatusOK( status ) )
		{
		status = createClientKeyex( sessionInfoPtr, handshakeInfo, stream,
									isKeyxAlgo( handshakeInfo->keyexAlgo ) ? \
										keyexPublicValue : NULL, 
									keyexPublicValueLen,
									( handshakeInfo->authAlgo == CRYPT_ALGO_NONE ) ? \
										TRUE : FALSE );
		}
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "createClientKeyex" );

	/* Wrap up the packet and create the session hash if required */
	status = completePacketStreamSSL( stream, 0 );
	if( cryptStatusOK( status ) )
		status = hashHSPacketWrite( handshakeInfo, stream, 0 );
	if( cryptStatusOK( status ) && \
		( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_EMS ) || \
		  needClientCert ) )
		{
		status = createSessionHash( sessionInfoPtr, handshakeInfo );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "createSessionHash" );

	/* If we need to supply a client certificate, send the signature 
	   generated with the certificate to prove possession of the private 
	   key */
	if( needClientCert )
		{
		int packetStreamOffset;

		/* Write the packet header and drop in the signature data.  Since 
		   we've interrupted the packet stream to perform the hashing we 
		   have to restart it at this point */
		status = continuePacketStreamSSL( stream, sessionInfoPtr, 
										  SSL_MSG_HANDSHAKE, 
										  &packetStreamOffset );
		if( cryptStatusError( status ) )
			return( status );
		status = continueHSPacketStream( stream, SSL_HAND_CLIENT_CERTVERIFY,
										 &packetOffset );
		if( cryptStatusOK( status ) )
			{
			status = createCertVerify( sessionInfoPtr, handshakeInfo, 
									   stream );
			}
		if( cryptStatusOK( status ) )
			status = completeHSPacketStream( stream, packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}

		/* Wrap the packet and perform the assorted hashing for it */
		status = completePacketStreamSSL( stream, packetStreamOffset );
		if( cryptStatusOK( status ) )
			{
			status = hashHSPacketWrite( handshakeInfo, stream, 
										packetStreamOffset );
			}
		if( cryptStatusError( status ) )
			return( status );
		CFI_CHECK_UPDATE( "createCertVerify" );

		REQUIRES( CFI_CHECK_SEQUENCE_8( "checkHSPacketHeader", "readSSLCertChain", 
										"processServerKeyex", 
										"processCertRequest", "writeSSLCertChain",
										"createClientKeyex", "createSessionHash", 
										"createCertVerify" ) );
		return( CRYPT_OK );
		}
	CFI_CHECK_UPDATE( "needClientCert" );

	REQUIRES( CFI_CHECK_SEQUENCE_8( "checkHSPacketHeader", "readSSLCertChain", 
									"processServerKeyex", "processCertRequest", 
									"writeSSLCertChain", "createClientKeyex", 
									"createSessionHash", "needClientCert" ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSLclientProcessing( INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	FNPTR_SET( handshakeInfo->beginHandshake, beginClientHandshake );
	FNPTR_SET( handshakeInfo->exchangeKeys, exchangeClientKeys );
	}
#endif /* USE_SSL */
