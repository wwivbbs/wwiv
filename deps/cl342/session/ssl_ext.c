/****************************************************************************
*																			*
*					cryptlib TLS Extension Management						*
*					Copyright Peter Gutmann 1998-2011						*
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

#ifdef USE_SSL

/****************************************************************************
*																			*
*							TLS Extension Definitions						*
*																			*
****************************************************************************/

/* TLS extension information.  Further types are defined at
   http://www.iana.org/assignments/tls-extensiontype-values.

   We specify distinct minimum and maximum lengths for client- and server-
   side use (so minLengthClient is the minimum length that a client can 
   send).  A value of CRYPT_ERROR means that this extension isn't valid when 
   sent by the client or server */

typedef struct {
	const int type;					/* Extension type */
	const int minLengthClient, minLengthServer;	/* Min.length */
	const int maxLength;			/* Max.length */
	const char *typeName;			/* Name for error messages */
	} EXT_CHECK_INFO;

static const EXT_CHECK_INFO extCheckInfoTbl[] = {
	/* Server name indication (SNI), RFC 4366:

		uint16		listLen
			byte	nameType
			uint16	nameLen
			byte[]	name */
	{ TLS_EXT_SERVER_NAME, 1, 0, 8192, 
	  "server name indication" },

	/* Maximm fragment length, RFC 4366:

		byte		fragmentLength */
	{ TLS_EXT_MAX_FRAGMENT_LENTH, 1, 1, 1, 
	  "fragment length" },

	/* Client certificate URL.  This dangerous extension allows a client to 
	   direct a server to grope around in arbitrary external (and untrusted) 
	   URLs trying to locate certificates, provinding a convenient mechanism 
	   for bounce attacks and all manner of similar firewall/trusted-host 
	   subversion problems:

		byte		chainType
		uint16		urlAndHashList
			uint16	urlLen
			byte[]	url
			byte	hashPresent
			byte[20] hash			-- If hashPresent flag set */
	{ TLS_EXT_CLIENT_CERTIFICATE_URL, 
	  1 + UINT16_SIZE + UINT16_SIZE + MIN_URL_SIZE + 1, CRYPT_ERROR, 8192,
	  "client certificate URL" },

	/* Trusted CA certificate(s), RFC 4366.  This allows a client to specify 
	   which CA certificates it trusts and by extension which server 
	   certificates it trusts, supposedly to reduce handshake messages in 
	   constrained clients.  Since the server usually has only a single 
	   certificate signed by a single CA, specifying the CAs that the client 
	   trusts doesn't serve much purpose:

		uint16		caList
			byte	idType
			[ choice of keyHash, certHash, or DN, another 
			  ASN.1-as-TLS structure ] */
	{ TLS_EXT_TRUSTED_CA_KEYS, UINT16_SIZE + 1, CRYPT_ERROR, 8192, 
	  "trusted CA" },

	/* Truncate the HMAC to a nonstandard 80 bits (rather than the de 
	   facto IPsec cargo-cult standard of 96 bits), RFC 4366 */
	{ TLS_EXT_TRUNCATED_HMAC, 0, 0, 0, 
	  "truncated HMAC" },

	/* OCSP status request, RFC 4366.  Another bounce-attack enabler, this 
	   time on both the server and an OCSP responder.  Both the responder
	   list and the extensions list may have length zero:

		byte		statusType
		uint16		ocspResponderList	-- May be length 0
			uint16	responderLength
			byte[]	responder
		uint16	extensionLength			-- May be length 0
			byte[]	extensions */
	{ TLS_EXT_STATUS_REQUEST, 
	  1 + UINT16_SIZE + UINT16_SIZE, CRYPT_ERROR, 8192, 
	  "OCSP status request" },

	/* User mapping.  Used with a complex RFC 4680 mechanism (the extension 
	   itself is in RFC 4681):

		byte		mappingLength
		byte[]		mapping */
	{ TLS_EXT_USER_MAPPING, 1 + 1, CRYPT_ERROR, 1 + 255, 
	  "user-mapping" },

	/* Authorisation exteniosn.  From an experimental RFC for adding 
	   additional authorisation data to the TLS handshake, RFC 5878:

		byte		authzFormatsList
			byte	authzFormatType

	   with the additional authorisation data being carried in the 
	   SupplementalData handshake message */
	{ TLS_EXT_CLIENT_AUTHZ, 1 + 1, CRYPT_ERROR, 1 + 255,
	  "client-authz" },
	{ TLS_EXT_SERVER_AUTHZ, CRYPT_ERROR, 1 + 1, 1 + 255,
	  "server-authz" },

	/* OpenPGP key.  From an experimental (later informational) RFC with 
	   support for OpenPGP keys, RFC 5081/6091:

		byte		certTypeListLength
		byte[]		certTypeList */
	{ TLS_EXT_CERTTYPE, 1 + 1, CRYPT_ERROR, 1 + 255, 
	  "cert-type (OpenPGP keying)" },

	/* Supported ECC curve IDs, RFC 4492 modified by RFC 5246/TLS 1.2:

		uint16		namedCurveListLength
		uint16[]	namedCurve */
	{ TLS_EXT_ELLIPTIC_CURVES, UINT16_SIZE + UINT16_SIZE, CRYPT_ERROR, 512, 
	  "ECDH/ECDSA curve ID" },

	/* Supported ECC point formats, RFC 4492 modified by RFC 5246/TLS 1.2:

		byte		pointFormatListLength
		byte[]		pointFormat */
	{ TLS_EXT_EC_POINT_FORMATS, 1 + 1, 1 + 1, 255, 
	  "ECDH/ECDSA point format" },

	/* SRP user name, RFC 5054:

		byte		userNameLength
		byte[]		userName */
	{ TLS_EXT_SRP, 1 + 1, CRYPT_ERROR, 1 + 255, 
	  "SRP username" },

	/* Signature algorithms, RFC 5246/TLS 1.2:

		uint16		algorithmListLength
			byte	hashAlgo
			byte	sigAlgo */
	{ TLS_EXT_SIGNATURE_ALGORITHMS, UINT16_SIZE + 1 + 1, CRYPT_ERROR, 512, 
	  "signature algorithm" },

	/* Session ticket, RFC 4507/5077.  The client can send a zero-length 
	   session ticket to indicate that it supports the extension but doesn't 
	   have a session ticket yet, and the server can send a zero-length 
	   ticket to indicate that it'll send the client a new ticket later in 
	   the handshake.  Confusing this even more, the specification says that 
	   the client sends "a zero-length ticket" but the server sends "a 
	   zero-length extension".  The original specification, RFC 4507, was 
	   later updated by a second version, RFC 5077, that makes explicit (via 
	   Appendix A) the fact that there's no "ticket length" field in the 
	   extension, so that the entire extension consists of the opaque ticket 
	   data:

		byte[]		sessionTicket (implicit size) */
	{ TLS_EXT_SESSIONTICKET, 0, 0, 8192,
	  "session ticket" },

	/* Secure renegotiation indication, RFC 5746.  See the comment below for
	   why we (apparently) support this even though we don't do 
	   renegotiation.  We give the length as one, corresponding to zero-
	   length content, the one is for the single-byte length field in the 
	   extension itself, set to a value of zero.  It can in theory be larger 
	   as part of the secure renegotiation process but this would be an 
	   indication of an attack since we don't do renegotiation */
	{ TLS_EXT_SECURE_RENEG, 1, 1, 1,
	  "secure renegotiation" },

	/* End-of-list marker */
	{ CRYPT_ERROR, 0, 0, 0, NULL }, { CRYPT_ERROR, 0, 0, 0, NULL }
	};

/* In addition to the variable-format extenions above we also support the 
   secure-renegotiation extension.  This is a strange extension to support 
   because cryptlib doesn't do renegotiation, but we have to send it at the 
   client side in order to avoid being attacked via a (non-cryptlib) server 
   that's vulnerable, and we have to process it on the server side in order 
   to not appear to be a vulnerable server (sigh) */

#define RENEG_EXT_SIZE	5
#define RENEG_EXT_DATA	"\xFF\x01\x00\x01\x00"

/****************************************************************************
*																			*
*							Read TLS Extensions								*
*																			*
****************************************************************************/

/* Process the list of preferred (or at least supported) ECC curves.  This 
   is a somewhat problematic extension because it applies to any use of ECC, 
   so if (for some reason) the server wants to use a P256 ECDH key with a 
   P521 ECDSA signing key then it has to verify that the client reports that 
   it supports both P256 and P521.  Since our limiting factor is the ECDSA 
   key that we use for signing, we require that the client report support 
   for the curve that matches our signing key.  Support for the 
   corresponding ECDH curve is automatic, since we support all curves for 
   ECDH that are supported for ECDSA:

	uint16		namedCurveListLength
	uint16[]	namedCurve */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
static int processSupportedCurveID( INOUT SESSION_INFO *sessionInfoPtr, 
									INOUT STREAM *stream, 
									IN_LENGTH_SHORT_Z const int extLength,
									OUT_ENUM_OPT( CRYPT_ECCCURVE ) \
										CRYPT_ECCCURVE_TYPE *preferredCurveIdPtr,
									OUT_BOOL BOOLEAN *extErrorInfoSet )
	{
	static const MAP_TABLE curveIDTbl[] = {
		{ TLS_CURVE_SECP192R1, CRYPT_ECCCURVE_P192 },
		{ TLS_CURVE_SECP224R1, CRYPT_ECCCURVE_P224 },
		{ TLS_CURVE_SECP256R1, CRYPT_ECCCURVE_P256 },
		{ TLS_CURVE_SECP384R1, CRYPT_ECCCURVE_P384 },
		{ TLS_CURVE_SECP521R1, CRYPT_ECCCURVE_P521 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	static const MAP_TABLE curveSizeTbl[] = {
		{ CRYPT_ECCCURVE_P192, bitsToBytes( 192 ) },
		{ CRYPT_ECCCURVE_P224, bitsToBytes( 224 ) },
		{ CRYPT_ECCCURVE_P256, bitsToBytes( 256 ) },
		{ CRYPT_ECCCURVE_P384, bitsToBytes( 384 ) },
		{ CRYPT_ECCCURVE_P521, bitsToBytes( 521 ) },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	CRYPT_ECCCURVE_TYPE preferredCurveID = CRYPT_ECCCURVE_NONE;
	int listLen, keySize, i, status;
#ifdef CONFIG_SUITEB_TESTS 
	int curvesSeen = 0;
#endif /* CONFIG_SUITEB_TESTS */

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( preferredCurveIdPtr, \
						sizeof( CRYPT_ECCCURVE_TYPE ) ) );

	REQUIRES( extLength >= 0 && extLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*preferredCurveIdPtr = CRYPT_ECCCURVE_NONE;
	*extErrorInfoSet = FALSE;

	/* Get the size of the server's signing key to bound the curve size */
	status = krnlSendMessage( sessionInfoPtr->privateKey,
							  IMESSAGE_GETATTRIBUTE, &keySize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read and check the ECC curve list header */
	status = listLen = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( listLen != extLength - UINT16_SIZE || \
		listLen < UINT16_SIZE || listLen > 256 || \
		( listLen % UINT16_SIZE ) != 0 )
		return( CRYPT_ERROR_BADDATA );

	/* Read the list of curve IDs, recording the most preferred one */
	for( i = 0; i < listLen / UINT16_SIZE; i++ )
		{
		int value, curveID, curveSize;

		status = value = readUint16( stream );
		if( cryptStatusError( status ) )
			return( status );
		status = mapValue( value, &curveID, curveIDTbl, 
						   FAILSAFE_ARRAYSIZE( curveIDTbl, MAP_TABLE ) );
		if( cryptStatusError( status ) )
			continue;	/* Unrecognised curve type */
		status = mapValue( curveID, &curveSize, curveSizeTbl, 
						   FAILSAFE_ARRAYSIZE( curveSizeTbl, MAP_TABLE ) );
		ENSURES( cryptStatusOK( status ) );
#ifdef CONFIG_SUITEB
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB )
			{
			const int suiteBinfo = \
						sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB;

			/* Suite B only allows P256 and P384.  At the 128-bit level both 
			   P256 and P384 are allowed, at the 256-bit level only P384 is 
			   allowed */
			if( curveID != CRYPT_ECCCURVE_P256 && \
				curveID != CRYPT_ECCCURVE_P384 )
				continue;
			if( suiteBinfo == SSL_PFLAG_SUITEB_256 && \
				curveID == CRYPT_ECCCURVE_P256 )
				continue;
  #ifdef CONFIG_SUITEB_TESTS 
			if( suiteBTestValue == SUITEB_TEST_BOTHCURVES )
				{
				/* We're checking whether the client sends both curve IDs, 
				   remember which ones we've seen so far */
				if( curveID == CRYPT_ECCCURVE_P256 )
					curvesSeen |= 1;
				if( curveID == CRYPT_ECCCURVE_P384 )
					curvesSeen |= 2;
				}
  #endif /* CONFIG_SUITEB_TESTS */
			}
#endif /* CONFIG_SUITEB */

		/* Make sure that the curve matches the server's signing key */
		if( curveSize != keySize )
			continue;

		/* We've got a matching curve, remember it.  In theory we could exit
		   at this point but we continue anyway to clear the remainder of 
		   the data */
		preferredCurveID = curveID;
		}
#ifdef CONFIG_SUITEB_TESTS 
	/* If we're checking for the presence of both P256 and P384 as supported 
	   elliptic curves and we don't see them, complain */
	if( suiteBTestValue == SUITEB_TEST_BOTHCURVES && curvesSeen != 3 )
		{
		*extErrorInfoSet = TRUE;
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Supported elliptic curves extension should have "
				  "contained both P256 and P384 but didn't" ) );
		}
#endif /* CONFIG_SUITEB_TESTS */

	*preferredCurveIdPtr = preferredCurveID;

	return( CRYPT_OK );
	}

/* Process the list of signature algorithms that the client can accept.  
   This is a complex and confusing TLS 1.2+ extension with weird 
   requirements attached to it, for example if the client indicates hash 
   algorithm X then the server (section 7.4.2) has to somehow produce a 
   certificate chain signed only using that hash algorithm, as if a 
   particular algorithm choice for TLS use could somehow dictate what 
   algorithms the CA and certificate-processing library that's being used 
   will provide (in addition the spec is rather confused on this issue, 
   giving it first as a MUST and then a paragraph later as a SHOULD).  This 
   also makes some certificate signature algorithms like RSA-PSS impossible 
   even if the hash algorithm used is supported by both the TLS and 
   certificate library, because TLS only allows the specification of PKCS 
   #1 v1.5 signatures.  What's worse, it creates a situation from which 
   there's no recovery because in the case of a problem all that the server 
   can say is "failed", but not "try again using this algorithm", while 
   returning certificates or a signature with the server's available 
   algorithm (ignoring the requirement to summarily abort the handshake in 
   the case of a mismatch) at least tells the client what the problem is.

   To avoid this mess we assume that everyone can do SHA-256, the TLS 1.2 
   default.  A poll of TLS implementers in early 2011 indicated that 
   everyone else ignores this requirement as well (one implementer described
   the requirement as "that is not just shooting yourself in the foot, that 
   is shooting 'the whole nine yards' (the entire ammunition belt) in your 
   foot"), so we're pretty safe here.  Since the extension isn't valid for 
   earlier versions, we ignore it if we're not using TLS 1.2+:

	uint16		algorithmListLength
		byte	hashAlgo
		byte	sigAlgo */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
static int processSignatureAlgos( INOUT SESSION_INFO *sessionInfoPtr, 
								  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
								  INOUT STREAM *stream, 
								  IN_LENGTH_SHORT_Z const int extLength,
								  OUT_BOOL BOOLEAN *extErrorInfoSet )
	{
#ifdef CONFIG_SUITEB
	static const MAP_TABLE curveSizeTbl[] = {
		{ bitsToBytes( 256 ), TLS_HASHALGO_SHA2 },
		{ bitsToBytes( 384 ), TLS_HASHALGO_SHA384 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	const int suiteBinfo = sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB;
	int keySize, hashType;
  #ifdef CONFIG_SUITEB_TESTS 
	int hashesSeen = 0;
  #endif /* CONFIG_SUITEB_TESTS */
#endif /* CONFIG_SUITEB */
	int listLen, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( extLength >= 0 && extLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*extErrorInfoSet = FALSE;

	/* Read and check the signature algorithm list header */
	status = listLen = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( listLen != extLength - UINT16_SIZE || \
		listLen < 1 + 1 || listLen > 64 + 64 || \
		( listLen % UINT16_SIZE ) != 0 )
		return( CRYPT_ERROR_BADDATA );

	/* If we're not using TLS 1.2+, skip the extension */
	if( sessionInfoPtr->version < SSL_MINOR_VERSION_TLS12 )
		return( sSkip( stream, listLen ) );

	/* For the more strict handling requirements in Suite B only 256- or 
	   384-bit algorithms are allowed at the 128-bit level and only 384-bit 
	   algorithms are allowed at the 256-bit level */
#ifdef CONFIG_SUITEB
	/* If we're not running in Suite B mode then there are no additional
	   checks required */
	if( !suiteBinfo )
		{
		handshakeInfo->keyexSigHashAlgo = CRYPT_ALGO_SHA2;

		return( sSkip( stream, listLen ) );
		}

	/* Get the size of the server's signing key to try and match the 
	   client's preferred hash size */
	status = krnlSendMessage( sessionInfoPtr->privateKey, 
							  IMESSAGE_GETATTRIBUTE, &keySize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	status = mapValue( keySize, &hashType, curveSizeTbl, 
					   FAILSAFE_ARRAYSIZE( curveSizeTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		{
		/* Suite B only allows P256 and P384 keys (checked by the higher-
		   level code when we add the server key) so we should never get to 
		   this situation */
		return( status );
		}
	handshakeInfo->keyexSigHashAlgo = CRYPT_ALGO_NONE;

	/* Read the hash and signature algorithms and choose the best one to 
	   use */
	for( ; listLen > 0; listLen -= 1 + 1 )
		{
		int hashAlgo, sigAlgo;

		/* Read the hash and signature algorithm and make sure that it's one 
		   that we can use.  At the 128-bit level both SHA256 and SHA384 are 
		   allowed, at the 256-bit level only SHA384 is allowed */
		hashAlgo = sgetc( stream );			/* Hash algorithm */
		status = sigAlgo = sgetc( stream );	/* Sig.algorithm */
		if( cryptStatusError( status ) )
			return( status );
		if( sigAlgo != TLS_SIGALGO_ECDSA || \
			( hashAlgo != TLS_HASHALGO_SHA2 && \
			  hashAlgo != TLS_HASHALGO_SHA384 ) )
			continue;
		if( suiteBinfo == SSL_PFLAG_SUITEB_256 && \
			hashAlgo != TLS_HASHALGO_SHA384 )
			continue;
  #ifdef CONFIG_SUITEB_TESTS 
		if( suiteBTestValue == SUITEB_TEST_BOTHSIGALGOS )
			{
			/* We're checking whether the client sends both hash algorithm 
			   IDs, remember which ones we've seen so far */
			if( hashAlgo == TLS_HASHALGO_SHA2 )
				hashesSeen |= 1;
			if( hashAlgo == TLS_HASHALGO_SHA384 )
				hashesSeen |= 2;
			}
  #endif /* CONFIG_SUITEB_TESTS */

		/* In addition to the general validity checks, the hash type also has
		   to match the server's key size */
		if( hashType != hashAlgo )
			continue;

		/* We've found one that we can use, set the appropriate variant.  
		   Note that since SHA384 is just a variant of SHA2, we always 
		   choose this if it's available.  Even if the order given is 
		   { SHA384, SHA256 } the parameter value for the original SHA384 
		   will remain set when SHA256 is subsequently read */
		handshakeInfo->keyexSigHashAlgo = CRYPT_ALGO_SHA2;
		if( hashAlgo == TLS_HASHALGO_SHA384 )
			handshakeInfo->keyexSigHashAlgoParam = bitsToBytes( 384 );
		}

	/* For Suite B the client must send either SHA256 or SHA384 as an 
	   option */
	if( handshakeInfo->keyexSigHashAlgo == CRYPT_ALGO_NONE )
		{
		*extErrorInfoSet = TRUE;
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Signature algorithms extension should have "
				  "contained %sP384/SHA384 but didn't",
				  ( suiteBinfo != SSL_PFLAG_SUITEB_256 ) ? \
					"P256/SHA256 and/or " : "" ) );
		}
#ifdef CONFIG_SUITEB_TESTS 
	/* If we're checking for the presence of both SHA256 and SHA384 as 
	   supported hash algorithms and we don't see them, complain */
	if( suiteBTestValue == SUITEB_TEST_BOTHSIGALGOS && hashesSeen != 3 )
		{
		*extErrorInfoSet = TRUE;
		retExt( CRYPT_ERROR_INVALID,
				( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
				  "Signature algortithms extension should have contained "
				  "both P256/SHA256 and P384/SHA384 but didn't" ) );
		}
#endif /* CONFIG_SUITEB_TESTS */

	return( CRYPT_OK );
#else
	handshakeInfo->keyexSigHashAlgo = CRYPT_ALGO_SHA2;

	return( sSkip( stream, listLen ) );
#endif /* CONFIG_SUITEB */
	}

/* Process a single extension.  The internal structure of some of these 
   things shows that they were created by ASN.1 people... */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6 ) ) \
static int processExtension( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream, 
							 IN_RANGE( 0, 65536 ) const int type,
							 IN_LENGTH_SHORT_Z const int extLength,
							 OUT_BOOL BOOLEAN *extErrorInfoSet )
	{
	int value, listLen, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( type >= 0 && type <= 65536 );
	REQUIRES( extLength >= 0 && extLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*extErrorInfoSet = FALSE;

	switch( type )
		{
		case TLS_EXT_SERVER_NAME:
			/* Response: Send zero-length reply to peer:

				uint16		listLen
					byte	nameType
					uint16	nameLen
					byte[]	name 

			   If we're the client and we sent this extension to the server
			   then the server may responed with a zero-length server-name
			   extension for no immediately obvious purpose (if the server
			   doesn't recognise the name then it's required to send an
			   'unrecognised-name' alert so any non-alert return means that
			   the value was accepted, but for some reason it's required to
			   send a zero-length response anyway), in which case we have to 
			   special-case the check for this */
			if( !isServer( sessionInfoPtr ) )
				{
				if( extLength <= 0 )
					return( CRYPT_OK );
				}
			else
				{
				/* Remember that we've seen the server-name extension so that
				   we can send a zero-length reply to the client */
				handshakeInfo->needSNIResponse = TRUE;
				}
			status = listLen = readUint16( stream );
			if( cryptStatusError( status ) )
				return( status );
			if( listLen != extLength - UINT16_SIZE || \
				listLen < 1 + UINT16_SIZE || \
				listLen >= MAX_INTLENGTH_SHORT )
				return( CRYPT_ERROR_BADDATA );

			/* Parsing of further SEQUENCE OF SEQUENCE data omitted */
			return( sSkip( stream, listLen ) );

		case TLS_EXT_MAX_FRAGMENT_LENTH:
			{
/*			static const int fragmentTbl[] = \
					{ 0, 512, 1024, 2048, 4096, 8192, 16384, 16384 }; */

			/* Response: If fragment-size == 3...5, send same to peer.  Note 
			   that we also allow a fragment-size value of 5, which isn't 
			   specified in the standard but should probably be present 
			   since it would otherwise result in a missing value between 
			   4096 and the default of 16384:

				byte		fragmentLength */
			status = value = sgetc( stream );
			if( cryptStatusError( status ) )
				return( status );
			if( value < 1 || value > 5 )
				return( CRYPT_ERROR_BADDATA );

/*			sessionInfoPtr->maxPacketSize = fragmentTbl[ value ]; */
			return( CRYPT_OK );
			}

		case TLS_EXT_ELLIPTIC_CURVES:
			{
			CRYPT_ECCCURVE_TYPE preferredCurveID;

			/* Read and process the list of preferred curves */
			status = processSupportedCurveID( sessionInfoPtr, stream,
											  extLength, &preferredCurveID,
											  extErrorInfoSet );
			if( cryptStatusError( status ) )
				return( status );

			/* If we couldn't find a curve that we have in common with the 
			   other side, disable the use of ECC algorithms.  This is a 
			   somewhat nasty failure mode because it means that something 
			   like a buggy implementation that sends the wrong hello 
			   extension (which is rather more likely than, say, an 
			   implementation not supporting the de facto universal-standard 
			   NIST curves) means that the crypto is quietly switched to 
			   non-ECC with the user being none the wiser, but there's no 
			   way for an implementation to negotiate ECC-only encryption */
			if( preferredCurveID == CRYPT_ECCCURVE_NONE )
				handshakeInfo->disableECC = TRUE;
			else
				handshakeInfo->eccCurveID = preferredCurveID;

			return( CRYPT_OK );
			}

		case TLS_EXT_EC_POINT_FORMATS:
			/* We don't really need to process this extension because every 
			   implementation is required to support uncompressed points (it 
			   also seems to be the universal standard that everyone uses by 
			   default anyway) so all that we do is treat the presence of 
			   the overall extension as an indicator that we should send 
			   back our own one in the server hello:

				byte		pointFormatListLength
				byte[]		pointFormat */
			if( extLength > 0 )
				{
				status = sSkip( stream, extLength );
				if( cryptStatusError( status ) )
					return( status );
				}
			handshakeInfo->sendECCPointExtn = TRUE;

			return( CRYPT_OK );

		case TLS_EXT_SIGNATURE_ALGORITHMS:
			/* Read and process the list of signature algorithms */
			return( processSignatureAlgos( sessionInfoPtr, handshakeInfo, 
										   stream, extLength, 
										   extErrorInfoSet ) );

		case TLS_EXT_SECURE_RENEG:
			/* If we get a nonzero length for this extension (the '1' is the 
			   initial length byte at the start) then it's an indication of 
			   an attack.  The status code to return here is a bit 
			   uncertain, but CRYPT_ERROR_INVALID seems to be the least
			   inappropriate */
			if( extLength != 1 || sgetc( stream ) != 0 )
				return( CRYPT_ERROR_INVALID );

			/* If we're the server, remember that we have to echo the 
			   extension back to the client */
			if( isServer( sessionInfoPtr ) )
				handshakeInfo->needRenegResponse = TRUE;

			return( CRYPT_OK );

		default:
			/* Default: Ignore the extension */
			if( extLength > 0 )
				{
				status = sSkip( stream, extLength );
				if( cryptStatusError( status ) )
					return( status );
				}

			return( CRYPT_OK );
		}

	retIntError();
	}

/* Read TLS extensions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readExtensions( INOUT SESSION_INFO *sessionInfoPtr, 
					INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
					INOUT STREAM *stream, 
					IN_LENGTH_SHORT const int length )
	{
	const int endPos = stell( stream ) + length;
	int minPayloadLength = 1, extListLen, noExtensions, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos > 0 && endPos < MAX_INTLENGTH_SHORT );

	/* If we're the client and we've sent a server-name extension to the
	   server, the server can optionally send back a zero-length server-name
	   extension for no immediately obvious purpose, in which case the 
	   minimum payload length may be zero */
	if( !isServer( sessionInfoPtr ) && \
		sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS )
		minPayloadLength = 0;

	/* Read the extension header and make sure that it's valid:

		uint16		extListLen
			uint16	extType
			uint16	extLen
			byte[]	extData */
	if( length < UINT16_SIZE + UINT16_SIZE + UINT16_SIZE + minPayloadLength )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "TLS hello contains %d bytes extraneous data", length ) );
		}
	status = extListLen = readUint16( stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid TLS extension information" ) );
		}
	if( extListLen != length - UINT16_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid TLS extension data length %d, should be %d",
				  extListLen, length - UINT16_SIZE ) );
		}

	/* Process the extensions */
	for( noExtensions = 0; stell( stream ) < endPos && \
						   noExtensions < FAILSAFE_ITERATIONS_MED; 
		 noExtensions++ )
		{
		const EXT_CHECK_INFO *extCheckInfoPtr = NULL;
		BOOLEAN extErrorInfoSet;
		int type, extLen, i;

		/* Read the header for the next extension and get the extension-
		   checking information.  The length check at this point is just a
		   generic sanity check, more specific checking is done once we've 
		   got the extension type-specific information */
		type = readUint16( stream );
		status = extLen = readUint16( stream );
		if( cryptStatusError( status ) || \
			extLen < 0 || extLen > MAX_PACKET_SIZE )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid TLS extension list item header" ) );
			}
		for( i = 0; extCheckInfoTbl[ i ].type != CRYPT_ERROR && \
					i < FAILSAFE_ARRAYSIZE( extCheckInfoTbl, EXT_CHECK_INFO ); 
			 i++ )
			{
			if( extCheckInfoTbl[ i ].type == type )
				{
				extCheckInfoPtr = &extCheckInfoTbl[ i ];
				break;
				}
			}
		ENSURES( i < FAILSAFE_ARRAYSIZE( extCheckInfoTbl, EXT_CHECK_INFO ) ); 
		if( extCheckInfoPtr != NULL )
			{
			const int minLength = isServer( sessionInfoPtr ) ? \
						extCheckInfoPtr->minLengthClient : \
						extCheckInfoPtr->minLengthServer;

			/* Perform any necessary initial checking of the extension */
			if( minLength == CRYPT_ERROR )
				{
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Received disallowed TLS %s extension from %s", 
						  extCheckInfoPtr->typeName, 
						  isServer( sessionInfoPtr ) ? "server" : "client" ) );
				}
			if( extLen < minLength || extLen > extCheckInfoPtr->maxLength )
				{
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid TLS %s extension length %d, should be "
						  "%d...%d", extCheckInfoPtr->typeName, extLen,
						  minLength, extCheckInfoPtr->maxLength ) );
				}
			}
		DEBUG_PRINT(( "Read extension %s (%d), length %d.\n",
					  ( extCheckInfoPtr != NULL ) ? \
						extCheckInfoPtr->typeName : "<Unknown>", type, extLen ));
		DEBUG_DUMP_STREAM( stream, stell( stream ), extLen );

		/* Process the extension data */
		status = processExtension( sessionInfoPtr, handshakeInfo, stream, 
								   type, extLen, &extErrorInfoSet );
		if( cryptStatusError( status ) )
			{
			if( isInternalError( status ) )
				{
				/* If it's an internal error then we can't rely on the value
				   of the by-reference parameters */
				return( status );
				}
			if( extErrorInfoSet )
				return( status );
			if( extCheckInfoPtr != NULL )
				{
				retExt( CRYPT_ERROR_BADDATA,
						( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
						  "Invalid TLS %s extension data", 
						  extCheckInfoPtr->typeName ) );
				}
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid TLS extension data for extension "
					  "type %d", type ) );
			}
		}
	if( noExtensions >= FAILSAFE_ITERATIONS_MED )
		{
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, SESSION_ERRINFO, 
				  "Excessive number (%d) of TLS extensions encountered", 
				  noExtensions ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Write TLS Extensions							*
*																			*
****************************************************************************/

/* Write the list of supported signature and hash algorithms as a 
   combinatorial explosion of { hash, sig } algorithm pairs (they're called
   SignatureAndHashAlgorithm in the spec, but are actually encoded as
   HashAndSignatureAlgorithm).  This is used both for extensions and for the 
   TLS 1.2 signature format.
   
   This gets a bit complicated because we both have to emit the values in
   preferred-algorithm order and some combinations aren't available, so 
   instead of simply iterating down two lists we have to exhaustively
   enumerate each possible algorithm combination.

   We disallow SHA1 (except for DSA where it's needed due to the hardcoding
   into the signature format) since the whole point of TLS 1.2 was to move 
   away from it, and a poll on the ietf-tls list indicated that all known 
   implementations (both of them) work fine with this configuration */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeSigHashAlgoList( STREAM *stream )
	{
	typedef struct {
		CRYPT_ALGO_TYPE sigAlgo, hashAlgo;
		TLS_SIGALGO_TYPE tlsSigAlgoID;
		TLS_HASHALGO_TYPE tlsHashAlgoID;
		} SIG_HASH_INFO;
	static const SIG_HASH_INFO algoTbl[] = {
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHAng, 
		  TLS_SIGALGO_RSA, 255 },
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 
		  TLS_SIGALGO_RSA, TLS_HASHALGO_SHA2 },
#if 0	/* 2/11/11 Disabled option for SHA1 after poll on ietf-tls list */
		{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 
		  TLS_SIGALGO_RSA, TLS_HASHALGO_SHA1 },
#endif /* 0 */
		{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHAng, 
		  TLS_SIGALGO_DSA, 255 },
		{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA2, 
		  TLS_SIGALGO_DSA, TLS_HASHALGO_SHA2 },
		{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, 
		  TLS_SIGALGO_DSA, TLS_HASHALGO_SHA1 },
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHAng, 
		  TLS_SIGALGO_ECDSA, 255 },
#ifdef CONFIG_SUITEB
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 
		  TLS_SIGALGO_ECDSA, TLS_HASHALGO_SHA384 },
#endif /* CONFIG_SUITEB */
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 
		  TLS_SIGALGO_ECDSA, TLS_HASHALGO_SHA2 },
#if 0	/* 2/11/11 Disabled option for SHA1 after poll on ietf-tls list */
		{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, 
		  TLS_SIGALGO_ECDSA, TLS_HASHALGO_SHA1 },
#endif /* 0 */
		{ CRYPT_ALGO_NONE, CRYPT_ERROR }, { CRYPT_ALGO_NONE, CRYPT_ERROR }
		};
	BYTE algoList[ 32 + 8 ];
	int algoIndex = 0, i;

	/* Determine which signature and hash algorithms are available for use */
	for( i = 0; algoTbl[ i ].sigAlgo != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( algoTbl, SIG_HASH_INFO ); i++ )
		{
		const CRYPT_ALGO_TYPE sigAlgo = algoTbl[ i ].sigAlgo;

		/* If the given signature algorithm isn't enabled, skip any further
		   occurrences of this algorithm */
		if( !algoAvailable( sigAlgo ) )
			{
			while( algoTbl[ i ].sigAlgo == sigAlgo && \
				   i < FAILSAFE_ARRAYSIZE( algoTbl, SIG_HASH_INFO ) )
				 i++;
			ENSURES( i < FAILSAFE_ARRAYSIZE( algoTbl, SIG_HASH_INFO ) );
			i--;	/* Adjust for increment also done in outer loop */

			continue;
			}

		/* If the hash algorithm isn't enabled, skip this entry */
		if( !algoAvailable( algoTbl[ i ].hashAlgo ) )
			continue;

		/* Add the TLS IDs for this signature and hash algorithm combination.  
		   Although the record is called SignatureAndHashAlgorithm, what's 
		   written first is the hash algorithm and not the signature 
		   algorithm */
		algoList[ algoIndex++ ] = intToByte( algoTbl[ i ].tlsHashAlgoID );
		algoList[ algoIndex++ ] = intToByte( algoTbl[ i ].tlsSigAlgoID );
		ENSURES( algoIndex <= 32 );
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( algoTbl, SIG_HASH_INFO ) );

	/* Write the combination of hash and signature algorithms */
	writeUint16( stream, algoIndex );
	return( swrite( stream, algoList, algoIndex ) );
	}

/* Write the server name indication (SNI) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeServerName( INOUT STREAM *stream,
							INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *serverNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_SERVER_NAME );
	URL_INFO urlInfo;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( serverNamePtr != NULL );

	/* Extract the server FQDN from the overall server name value */
	status = sNetParseURL( &urlInfo, serverNamePtr->value, 
						   serverNamePtr->valueLength, URL_TYPE_HTTPS );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the server name */
	writeUint16( stream, 1 + UINT16_SIZE + urlInfo.hostLen );
	sputc( stream, 0 );		/* Type = DNS name */
	writeUint16( stream, urlInfo.hostLen );
	return( swrite( stream, urlInfo.host, urlInfo.hostLen ) );
	}

/* Write TLS extensions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeClientExtensions( INOUT STREAM *stream,
						   INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM nullStream;
	const void *eccCurveInfoPtr = DUMMY_INIT_PTR;
	const BOOLEAN hasServerName = \
		( findSessionInfo( sessionInfoPtr->attributeList,
						   CRYPT_SESSINFO_SERVER_NAME ) != NULL ) ? TRUE : FALSE;
	int serverNameExtLen = 0, sigHashHdrLen = 0, sigHashExtLen = 0;
	int eccCurveTypeLen = DUMMY_INIT, eccInfoLen = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS );

	/* Determine the overall length of the extension data, first the server 
	   name indication (SNI) if it's present (it may not be if we're using
	   a raw network socket) */
	if( hasServerName )
		{
		sMemNullOpen( &nullStream );
		status = writeServerName( &nullStream, sessionInfoPtr );
		if( cryptStatusOK( status ) )
			serverNameExtLen = stell( &nullStream );
		sMemClose( &nullStream );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Signature and hash algorithms.  These are only used with TLS 1.2+ so 
	   we only write them if we're using these versions of the protocol */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		sMemNullOpen( &nullStream );
		status = writeSigHashAlgoList( &nullStream );
		if( cryptStatusOK( status ) )
			{
			sigHashHdrLen = UINT16_SIZE + UINT16_SIZE;
			sigHashExtLen = stell( &nullStream );
			}
		sMemClose( &nullStream );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* ECC information.  This is only sent if we're proposing ECC suites in
	   the client hello */
	if( algoAvailable( CRYPT_ALGO_ECDH ) && \
		algoAvailable( CRYPT_ALGO_ECDSA ) )
		{
		static const BYTE eccCurveInfo[] = {
			0, TLS_CURVE_SECP521R1, 0, TLS_CURVE_SECP384R1, 
			0, TLS_CURVE_SECP256R1, 0, TLS_CURVE_SECP224R1, 
			0, TLS_CURVE_SECP192R1, 0, 0 
			};

#ifdef CONFIG_SUITEB
		if( sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB )
			{
			static const BYTE eccCurveSuiteB128Info[] = {
				0, TLS_CURVE_SECP256R1, 0, TLS_CURVE_SECP384R1, 0, 0 
				};
			static const BYTE eccCurveSuiteB256Info[] = {
				0, TLS_CURVE_SECP384R1, 0, 0 
				};
			const int suiteBinfo = \
					sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB;

			if( suiteBinfo == SSL_PFLAG_SUITEB_128 )
				{
				eccCurveInfoPtr = eccCurveSuiteB128Info;
				eccCurveTypeLen = 2 * UINT16_SIZE;
				}
			else				
				{
				eccCurveInfoPtr = eccCurveSuiteB256Info;
				eccCurveTypeLen = UINT16_SIZE;
				}
  #ifdef CONFIG_SUITEB_TESTS 
			/* In some cases for test purposes we have to send invalid ECC
			   information */
			if( suiteBTestValue == SUITEB_TEST_CLIINVALIDCURVE )
				{
				static const BYTE eccCurveSuiteBInvalidInfo[] = {
					0, TLS_CURVE_SECP521R1, 0, 0, TLS_CURVE_SECP192R1, 0, 0 
					};

				eccCurveInfoPtr = eccCurveSuiteBInvalidInfo;
				eccCurveTypeLen = 2 * UINT16_SIZE;
				}
  #endif /* CONFIG_SUITEB_TESTS  */
			}
		else
#endif /* CONFIG_SUITEB */
			{
			eccCurveInfoPtr = eccCurveInfo;
			eccCurveTypeLen = 5 * UINT16_SIZE;
			}
		eccInfoLen = UINT16_SIZE + UINT16_SIZE + \
					 UINT16_SIZE + eccCurveTypeLen;
		eccInfoLen += UINT16_SIZE + UINT16_SIZE + 1 + 1;
		}

	/* Write the list of extensions */
	writeUint16( stream, UINT16_SIZE + UINT16_SIZE + serverNameExtLen + \
						 RENEG_EXT_SIZE + sigHashHdrLen + sigHashExtLen + \
						 eccInfoLen );
	if( hasServerName )
		{
		writeUint16( stream, TLS_EXT_SERVER_NAME );
		writeUint16( stream, serverNameExtLen );
		status = writeServerName( stream, sessionInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension server name indication (%d), length %d.\n",
					  TLS_EXT_SERVER_NAME, serverNameExtLen ));
		DEBUG_DUMP_STREAM( stream, stell( stream ) - serverNameExtLen, 
						   serverNameExtLen );
		}
	status = swrite( stream, RENEG_EXT_DATA, RENEG_EXT_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	DEBUG_PRINT(( "Wrote extension secure renegotiation (%d), length 1.\n",
				  TLS_EXT_SECURE_RENEG ));
	DEBUG_DUMP_STREAM( stream, stell( stream ) - 1, 1 );
	if( sigHashExtLen > 0 )
		{
		writeUint16( stream, TLS_EXT_SIGNATURE_ALGORITHMS );
		writeUint16( stream, sigHashExtLen );
		status = writeSigHashAlgoList( stream );
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension signature algorithm (%d), length %d.\n",
					  TLS_EXT_SIGNATURE_ALGORITHMS, sigHashExtLen ));
		DEBUG_DUMP_STREAM( stream, 
						   stell( stream ) - sigHashExtLen, sigHashExtLen );
		}
	if( eccInfoLen > 0 )
		{
		/* Write the ECC curve type extension */
		writeUint16( stream, TLS_EXT_ELLIPTIC_CURVES );
		writeUint16( stream, UINT16_SIZE + eccCurveTypeLen );/* Ext.len */
		writeUint16( stream, eccCurveTypeLen );		/* Curve list len.*/
		status = swrite( stream, eccCurveInfoPtr, eccCurveTypeLen );	
		if( cryptStatusError( status ) )			/* Curve list */
			return( status );
		DEBUG_PRINT(( "Wrote extension ECC curve type (%d), length %d.\n",
					  TLS_EXT_ELLIPTIC_CURVES, eccCurveTypeLen ));
		DEBUG_DUMP_STREAM( stream, 
						   stell( stream ) - ( UINT16_SIZE + eccCurveTypeLen ), 
						   UINT16_SIZE + eccCurveTypeLen );

		/* Write the ECC point format extension */
		writeUint16( stream, TLS_EXT_EC_POINT_FORMATS );
		writeUint16( stream, 1 + 1 );			/* Extn. length */
		sputc( stream, 1 );						/* Point-format list len.*/
		status = sputc( stream, 0 );			/* Uncompressed points */
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension ECC point format (%d), length %d.\n",
					  TLS_EXT_EC_POINT_FORMATS, 1 + 1 ));
		DEBUG_DUMP_STREAM( stream, stell( stream ) - 1 + 1, 1 + 1 );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeServerExtensions( INOUT STREAM *stream,
						   INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	int extListLen = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* Calculate the size of the extensions */
	if( isEccAlgo( handshakeInfo->keyexAlgo ) && \
		handshakeInfo->sendECCPointExtn )
		extListLen += UINT16_SIZE + UINT16_SIZE + 1 + 1;
	if( handshakeInfo->needSNIResponse )
		extListLen += UINT16_SIZE + UINT16_SIZE;
	if( handshakeInfo->needRenegResponse )
		extListLen += RENEG_EXT_SIZE;
	if( extListLen <= 0 )
		{
		/* No extensions to write, we're done */
		return( CRYPT_OK );
		}

	/* Write the overall extension list length */
	writeUint16( stream, extListLen );

	/* If the client sent an SNI extension then we have to acknowledge it
	   with a zero-length SNI extension response.  This is slightly 
	   dishonest because we haven't passed the SNI data back to the caller,
	   but SNI will (at some point in the future) be sent by default by 
	   clients and since we're highly unlikely to be used with multihomed 
	   servers but likely to be used in oddball environments like ones 
	   without DNS we just accept any SNI and allow a connect.  SNI is 
	   merely a courtesy notification to allow selection of the correct 
	   server certificate for multihomed servers with the actual virtual-
	   host management being done via the HTTP Host: header, so not making 
	   use of it isn't a real problem */
	if( handshakeInfo->needSNIResponse )
		{
		writeUint16( stream, TLS_EXT_SERVER_NAME );
		status = writeUint16( stream, 0 );
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension extension server name indication (%d), "
					  "length 0.\n", TLS_EXT_SERVER_NAME, 0 ));
		}

	/* If the client sent a secure-renegotiation indicator we have to send a
	   response even though we don't support renegotiation.  See the comment 
	   by extCheckInfoTbl for why this odd behaviour is necessary */
	if( handshakeInfo->needRenegResponse )
		{
		status = swrite( stream, RENEG_EXT_DATA, RENEG_EXT_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension secure renegotiation (%d), length 1.\n",
					  TLS_EXT_SECURE_RENEG ));
		DEBUG_DUMP_STREAM( stream, stell( stream ) - 1, 1 );
		}

	/* If the client sent ECC extensions and we've negotiated an ECC cipher 
	   suite, send back the appropriate response.  We don't have to send 
	   back the curve ID that we've chosen because this is communicated 
	   explicitly in the server keyex */
	if( isEccAlgo( handshakeInfo->keyexAlgo ) && \
		handshakeInfo->sendECCPointExtn )
		{
		writeUint16( stream, TLS_EXT_EC_POINT_FORMATS );
		writeUint16( stream, 1 + 1 );	/* Extn. length */
		sputc( stream, 1 );				/* Point-format list len.*/
		status = sputc( stream, 0 );	/* Uncompressed points */
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "Wrote extension ECC point format (%d), length %d.\n",
					  TLS_EXT_EC_POINT_FORMATS, 1 + 1 ));
		DEBUG_DUMP_STREAM( stream, stell( stream ) - 1 + 1, 1 + 1 );
		}

	return( CRYPT_OK );
	}
#endif /* USE_SSL */
