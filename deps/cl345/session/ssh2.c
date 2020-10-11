/****************************************************************************
*																			*
*						cryptlib SSHv2 Session Management					*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssh.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssh.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSH

/* Tables mapping SSHv2 algorithm names to cryptlib algorithm IDs, in 
   preferred algorithm order.  The algorithm may not be a real cryptlib 
   algorithm type but an SSH-specific pseudo-algorithm in the 
   CRYPT_PSEUDOALGO_xxx range, for example CRYPT_PSEUDOALGO_PASSWORD.  
   To deal with this we have to map the pseudo-algorithm value to a 
   CRYPT_ALGO_TYPE via the MK_ALGO() macro.

   ECC support by SSH implementations is rather hit-and-miss.  If we were to 
   advertise ECC only (which we never do), some servers will respond with 
   RSA/DSA keys (even though they're not specified as being supported), and 
   others will respond with an empty host key.
   
   In addition the algorithms aren't just algorithm values but a combination 
   of the algorithm, the key size, and the hash algorithm, with 
   CRYPT_ALGO_ECDH/CRYPT_ALGO_ECDSA being the default P256 curve with 
   SHA-256.  Because the curve types are tied to the oddball SHA-2 hash 
   variants (we can't just use SHA-256 for every curve), we don't support 
   P384 and P512 because we'd have to support an entirely new (and 64-bit-
   only) hash algorithm for each of the curves.

   SSH has multiple ways to specify the same keyex algorithm, either
   negotiated via "diffie-hellman-group-exchange-sha256" and 
   "diffie-hellman-group-exchange-sha1" or explicitly as
   "diffie-hellman-group14-sha256" and "diffie-hellman-group14-sha1".  
   We distinguish between DH + SHA1/SHA256 and DH + SHA1/SHA256 via the
   addition of an algorithm parameter, which contains the DH key size for 
   the selected fixed group.  
   
   We don't support the 1024-bit DH group even though it's mandatory because
   it's too obvious a target for an offline attack.  Unfortunately this 
   presents a problem with assorted Cisco security appliances which are
   "secure" by executive fiat rather than by design, supporting only 
   "diffie-hellman-group1-sha1", and not even the mandatory-to-support 
   "diffie-hellman-group14-sha1" let alone the group-exchange suites */

static const ALGO_STRING_INFO algoStringKeyexTbl[] = {
#if defined( USE_ECDH ) && defined( PREFER_ECC )
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDH && PREFER_ECC */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group14-sha256", 29, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2, bitsToBytes( 2048 ) },
	{ "diffie-hellman-group14-sha1", 27, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1, bitsToBytes( 2048 ) },
#if defined( USE_ECDH ) && !defined( PREFER_ECC ) 
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDH && !PREFER_ECC */
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }
	};
static const ALGO_STRING_INFO algoStringKeyexNoECCTbl[] = {
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group14-sha256", 29, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2, bitsToBytes( 2048 ) },
	{ "diffie-hellman-group14-sha1", 27, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1, bitsToBytes( 2048 ) },
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }
	};

static const ALGO_STRING_INFO algoStringPubkeyTbl[] = {
#if defined( USE_ECDSA ) && defined( PREFER_ECC )
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDSA && PREFER_ECC */
	{ "rsa-sha2-256", 12, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2 },
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1 },
#ifdef USE_DSA
	{ "ssh-dss", 7, CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1 },
#endif /* USE_DSA */
#if defined( USE_ECDSA ) && !defined( PREFER_ECC )
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDSA && !PREFER_ECC */
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0 }
	};

static const ALGO_STRING_INFO algoStringEncrTbl[] = {
	{ "aes128-cbc", 10, CRYPT_ALGO_AES, CRYPT_MODE_CBC, bitsToBytes( 128 ) },
	{ "aes256-cbc", 10, CRYPT_ALGO_AES, CRYPT_MODE_CBC, bitsToBytes( 256 ) },
#ifdef USE_SSH_CTR
	{ "aes128-ctr", 10, CRYPT_ALGO_AES, CRYPT_MODE_ECB, bitsToBytes( 128 ) },
	{ "aes256-ctr", 10, CRYPT_ALGO_AES, CRYPT_MODE_ECB, bitsToBytes( 256 ) },
#endif /* USE_SSH_CTR */
#ifdef USE_3DES
	{ "3des-cbc", 8, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, bitsToBytes( 192 ) },
#endif /* USE_3DES */
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO algoStringMACTbl[] = {
	{ "hmac-sha2-256", 13, CRYPT_ALGO_HMAC_SHA2 },
	{ "hmac-sha1", 9, CRYPT_ALGO_HMAC_SHA1 },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO algoStringCoprTbl[] = {
	{ "none", 4, CRYPT_ALGO_AES /* Always-valid placeholder */ },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

/* A grand unified version of the above, used to write algorithm names */

static const ALGO_STRING_INFO algoStringMapTbl[] = {
	/* Keyex algorithms */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group14-sha256", 29, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2, bitsToBytes( 2048 ) },
	{ "diffie-hellman-group14-sha1", 27, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1, bitsToBytes( 2048 ) },
#ifdef USE_ECDH
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDH */

	/* Signature algorithms */
	{ "rsa-sha2-256", 12, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2 },
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1 },
#ifdef USE_DSA
	{ "ssh-dss", 7, CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1 },
#endif /* USE_DSA */
#ifdef USE_ECDSA
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, bitsToBytes( 256 ) },
#endif /* USE_ECDSA */

	/* Encryption algorithms */
	{ "aes128-cbc", 10, CRYPT_ALGO_AES, CRYPT_MODE_CBC, bitsToBytes( 128 ) },
	{ "aes256-cbc", 10, CRYPT_ALGO_AES, CRYPT_MODE_CBC, bitsToBytes( 256 ) },
#ifdef USE_SSH_CTR
	{ "aes128-ctr", 10, CRYPT_ALGO_AES, CRYPT_MODE_ECB, bitsToBytes( 128 ) },
	{ "aes256-ctr", 10, CRYPT_ALGO_AES, CRYPT_MODE_ECB, bitsToBytes( 256 ) },
#endif /* USE_SSH_CTR */
#ifdef USE_3DES
	{ "3des-cbc", 8, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, bitsToBytes( 192 ) },
#endif /* USE_3DES */

	/* MAC algorithms */
	{ "hmac-sha2-256", 13, CRYPT_ALGO_HMAC_SHA2 },
	{ "hmac-sha1", 9, CRYPT_ALGO_HMAC_SHA1 },

	/* Miscellaneous */
	{ "password", 8, MK_ALGO( PSEUDOALGO_PASSWORD ) },
	{ "none", 4, CRYPT_ALGO_LAST },	/* Catch-all */

	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Initialise crypto-related handshake information */

STDC_NONNULL_ARG( ( 1 ) ) \
void initHandshakeCrypt( INOUT SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	/* Set the initial hash algorithm used to authenticate the handshake */
	handshakeInfo->exchangeHashAlgo = CRYPT_ALGO_SHA1;

	/* Most of the SSH <-> cryptlib mapping tables are fixed, however the 
	   pubkey table table is pointed to by the handshakeInfo and may
	   later be changed dynamically on the server depending on the server's 
	   key type */
	handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyTbl;
	handshakeInfo->algoStringPubkeyTblNoEntries = \
			FAILSAFE_ARRAYSIZE( algoStringPubkeyTbl, ALGO_STRING_INFO );
	}

/* Hash the SSH ID strings that are exchanged as pre-handshake out-of-band 
   data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int hashHandshakeStrings( INOUT SSH_HANDSHAKE_INFO *handshakeInfo,
						  IN_BUFFER( clientStringLength ) \
								const void *clientString,
						  IN_LENGTH_SHORT const int clientStringLength,
						  IN_BUFFER( serverStringLength ) \
								const void *serverString,
						  IN_LENGTH_SHORT const int serverStringLength )
	{
	int status;

	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isReadPtrDynamic( clientString, clientStringLength ) );
	assert( isReadPtrDynamic( serverString, serverStringLength ) );

	REQUIRES( isShortIntegerRangeNZ( clientStringLength ) );
	REQUIRES( isShortIntegerRangeNZ( serverStringLength ) );

	/* SSH hashes the handshake ID strings for integrity-protection purposes, 
	   first the client string and then the server string, encoded as SSH 
	   string values.  In addition since the handshake can retroactively 
	   switch to a different hash algorithm mid-exchange we have to 
	   speculatively hash the messages with alternative algorithms in case 
	   the other side decides to switch */
	status = hashAsString( handshakeInfo->iExchangeHashContext, 
						   clientString, clientStringLength );
	if( cryptStatusOK( status ) )
		{
		status = hashAsString( handshakeInfo->iExchangeHashContext,
							   serverString, serverStringLength );
		}
	if( handshakeInfo->iExchangeHashAltContext == CRYPT_ERROR )
		return( status );
	status = hashAsString( handshakeInfo->iExchangeHashAltContext, 
						   clientString, clientStringLength );
	if( cryptStatusOK( status ) )
		{
		status = hashAsString( handshakeInfo->iExchangeHashAltContext,
							   serverString, serverStringLength );
		}
	return( status );
	}

/****************************************************************************
*																			*
*							Read/Write Algorithm Info						*
*																			*
****************************************************************************/

/* Convert an SSH algorithm list to a cryptlib ID in preferred-algorithm 
   order.  For some bizarre reason the algorithm information is communicated 
   as a comma-delimited list (in an otherwise binary protocol) so we have to 
   unpack and pack them into this cumbersome format alongside just choosing 
   which algorithm to use.  In addition the algorithm selection mechanism 
   differs depending on whether we're the client or the server, and what set 
   of algorithms we're matching.  Unlike SSL, which uses the offered-suites/
   chosen-suites mechanism, in SSH both sides offer a selection of cipher 
   suites and then the server chooses the first one that appears on both it 
   and the client's list, with special-case handling for the keyex and 
   signature algorithms if the match isn't the first one on the list.  This 
   means that the client can choose as it pleases from the server's list if 
   it waits for the server hello (see the comment in the client/server hello 
   handling code on the annoying nature of this portion of the SSH handshake) 
   but the server has to perform a complex double-match of its own vs.the 
   client's list.  The cases that we need to handle are:

	BEST_MATCH: Get the best matching algorithm (that is, the one 
		corresponding to the strongest crypto mechanism), used by the client 
		to match the server.

	FIRST_MATCH: Get the first matching algorithm, used by the server to 
		match the client.

	FIRST_MATCH_WARN: Get the first matching algorithm and warn if it isn't 
		the first one on the list of possible algorithms, used by the server 
		to match the client for the keyex and public-key algorithms.

   This is a sufficiently complex and screwball function that we need to
   define a composite structure to pass all of the control information in
   and out */

typedef enum {
	GETALGO_NONE,			/* No match action */
	GETALGO_FIRST_MATCH,	/* Get first matching algorithm */
	GETALGO_FIRST_MATCH_WARN,/* Get first matching algo, warn if not first */
	GETALGO_BEST_MATCH,		/* Get best matching algorithm */
	GETALGO_LAST			/* Last possible match action */
	} GETALGO_TYPE;

typedef struct {
	/* Match information passed in by the caller */
	ARRAY_FIXED( noAlgoInfoEntries ) \
	const ALGO_STRING_INFO *algoInfo;/* Algorithm selection information */
	int noAlgoInfoEntries;
	CRYPT_ALGO_TYPE preferredAlgo;	/* Preferred algo for first-match */
	GETALGO_TYPE getAlgoType;		/* Type of match to perform */
	BOOLEAN allowECC;				/* Whether to allow ECC algos */
	BOOLEAN allowExtIndicator;		/* Whether to allow extension indicator */

	/* Information returned by the read-algorithm function */
	CRYPT_ALGO_TYPE algo;			/* Matched algorithm */
	CRYPT_ALGO_TYPE subAlgo;		/* Sub-algorithm (e.g. hash for keyex) */
	int parameter;					/* Optional algorithm parameter */
	BOOLEAN prefAlgoMismatch;		/* First match != preferredAlgo */
	BOOLEAN extensionIndicator;		/* Whether extension indicator was found */
	} ALGOID_INFO;

#if defined( USE_ECDH ) || defined( USE_ECDSA )
  #define ALLOW_ECC		TRUE
#else
  #define ALLOW_ECC		FALSE
#endif /* USE_ECDH || USE_ECDSA */

#define MAX_NO_SUBSTRINGS		32	/* Max.no of algorithm substrings */
#define MAX_SUBSTRING_SIZE		128	/* Max.size of each substring */

#define setAlgoIDInfo( algoIDInfo, algoStrInfo, algoStrInfoEntries, getType ) \
	{ \
	memset( ( algoIDInfo ), 0, sizeof( ALGOID_INFO ) ); \
	( algoIDInfo )->algoInfo = ( algoStrInfo ); \
	( algoIDInfo )->noAlgoInfoEntries = ( algoStrInfoEntries ); \
	( algoIDInfo )->preferredAlgo = CRYPT_ALGO_NONE; \
	( algoIDInfo )->getAlgoType = ( getType ); \
	( algoIDInfo )->allowECC = ALLOW_ECC; \
	( algoIDInfo )->allowExtIndicator = FALSE; \
	}
#define setAlgoIDInfoEx( algoIDInfo, algoStrInfo, algoStrInfoEntries, prefAlgo, getType ) \
	{ \
	memset( ( algoIDInfo ), 0, sizeof( ALGOID_INFO ) ); \
	( algoIDInfo )->algoInfo = ( algoStrInfo ); \
	( algoIDInfo )->noAlgoInfoEntries = ( algoStrInfoEntries ); \
	( algoIDInfo )->preferredAlgo = ( prefAlgo ); \
	( algoIDInfo )->getAlgoType = ( getType ); \
	( algoIDInfo )->allowECC = ALLOW_ECC; \
	( algoIDInfo )->allowExtIndicator = FALSE; \
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readAlgoStringEx( INOUT STREAM *stream, 
							 INOUT ALGOID_INFO *algoIDInfo,
							 INOUT ERROR_INFO *errorInfo )
	{
	const ALGO_STRING_INFO *algoInfoPtr;
	BOOLEAN foundMatch = FALSE;
	void *string DUMMY_INIT_PTR;
	int stringPos, stringLen, substringLen, algoIndex = 999;
	int noStrings, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( algoIDInfo, sizeof( ALGOID_INFO ) ) );
	assert( isReadPtrDynamic( algoIDInfo->algoInfo, \
							  sizeof( ALGO_STRING_INFO ) * \
									algoIDInfo->noAlgoInfoEntries ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( ( algoIDInfo->getAlgoType == GETALGO_BEST_MATCH && \
				algoIDInfo->preferredAlgo == CRYPT_ALGO_NONE ) || \
			  ( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH ) ||
			  ( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH_WARN && \
				( algoIDInfo->preferredAlgo > CRYPT_ALGO_NONE && \
				  algoIDInfo->preferredAlgo < CRYPT_ALGO_LAST_EXTERNAL ) ) );
			  /* FIRST_MATCH uses CRYPT_ALGO_NONE on the first match of an
				 algorithm pair and the first algorithm chosen on the second
				 match */
	REQUIRES( algoIDInfo->noAlgoInfoEntries > 0 && \
			  algoIDInfo->noAlgoInfoEntries < 20 );
	REQUIRES( algoIDInfo->allowECC == TRUE || algoIDInfo->allowECC == FALSE );
	REQUIRES( algoIDInfo->allowExtIndicator == TRUE || \
			  algoIDInfo->allowExtIndicator == FALSE );

	/* Get the string length and data and make sure that it's valid */
	status = stringLen = readUint32( stream );
	if( !cryptStatusError( status ) && \
		( stringLen < SSH2_MIN_ALGOID_SIZE || \
		  stringLen >= MAX_INTLENGTH_SHORT ) )
		{
		/* Quick-reject check for an obviously-invalid string */
		status = CRYPT_ERROR_BADDATA;
		}
	if( !cryptStatusError( status ) )
		status = sMemGetDataBlock( stream, &string, stringLen );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, stringLen, MAX_INTLENGTH_SHORT );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid algorithm ID string" ) );
		}
	ENSURES( stringLen >= SSH2_MIN_ALGOID_SIZE && \
			 stringLen < MAX_INTLENGTH_SHORT );
	ANALYSER_HINT( string != NULL );

	/* Walk down the string looking for a recognised algorithm.  Since our
	   preference may not match the other side's preferences we have to walk
	   down the entire list to find our preferred choice:

		  stringPos			stringLen
			   |			   |
			   v			   v
		"algo1,algo2,algo3,algoN"
			   ^   ^		   ^
			   |   |		   |
			   |substrLen	   |
			   +- subStrMaxLen +

	   This works by walking an index stringPos down the string, with each 
	   substring delimited by { stringPos, subStringLen }, which is checked
	   against the table of algorithm names */
	LOOP_MED( ( stringPos = 0, noStrings = 0 ),
			  stringPos <= stringLen - SSH2_MIN_ALGOID_SIZE && \
					noStrings < MAX_NO_SUBSTRINGS, 
			  ( stringPos += substringLen + 1, noStrings++ ) )
		{
		const ALGO_STRING_INFO *matchedAlgoInfo = NULL;
		const BYTE *substringPtr = ( BYTE * ) string + stringPos;
		const int substringMaxLen = stringLen - stringPos;
		BOOLEAN algoMatched = TRUE;
		int currentAlgoIndex, LOOP_ITERATOR_ALT;

		/* Find the length of the next algorithm name */
		LOOP_LARGE_ALT( substringLen = 0,
						substringLen < substringMaxLen && \
							substringPtr[ substringLen ] != ',' && \
							substringLen < MAX_SUBSTRING_SIZE, \
						substringLen++ );
		ENSURES( LOOP_BOUND_OK_ALT );
		if( substringLen >= MAX_SUBSTRING_SIZE )
			{
			retExt( CRYPT_ERROR_OVERFLOW,
					( CRYPT_ERROR_OVERFLOW, errorInfo, 
					  "Excessively long (more than %d characters) SSH "
					  "algorithm string encountered", substringLen ) );
			}
		if( substringLen < SSH2_MIN_ALGOID_SIZE || \
			substringLen > CRYPT_MAX_TEXTSIZE )
			{
			/* Empty or too-short algorithm name (or excessively long one), 
			   continue.  Note that even with an (invalid) zero-length 
			   substring we'll still progress down the string since the loop
			   increment is the substring length plus one */
			continue;
			}

		/* Check for the presence of the special-case extension-info 
		   indicator if required.  At the moment we only respond to a 
		   client-side extension indicator so we don't need to distinguish 
		   between which type we look for */
		if( algoIDInfo->allowExtIndicator )
			{
			if( substringLen == 10 && \
				!memcmp( substringPtr, "ext-info-c", 10 ) )
				{
				algoIDInfo->extensionIndicator = TRUE;

				/* If we've already found matching algorithm information, 
				   we're done */
				if( foundMatch )
					break;

				continue;
				}

			/* If we've already found a match then all that we're looking 
			   for is the extension info indicator */
			if( foundMatch )
				continue;
			}

		/* Check whether it's something that we can handle */
		LOOP_MED( currentAlgoIndex = 0, 
				  currentAlgoIndex < algoIDInfo->noAlgoInfoEntries && \
					algoIDInfo->algoInfo[ currentAlgoIndex ].name != NULL,
				  currentAlgoIndex++ )
			{
			const ALGO_STRING_INFO *algoIDInfoPtr = \
							&algoIDInfo->algoInfo[ currentAlgoIndex ];

			if( algoIDInfoPtr->nameLen == substringLen && \
				!memcmp( algoIDInfoPtr->name, substringPtr, substringLen ) )
				{
				matchedAlgoInfo = algoIDInfoPtr;
				break;
				}
			}
		ENSURES( LOOP_BOUND_OK );
		ENSURES( currentAlgoIndex < algoIDInfo->noAlgoInfoEntries );
		if( matchedAlgoInfo == NULL )
			{
			/* Unrecognised algorithm name, remember to warn the caller if 
			   we have to match the first algorithm on the list, then move 
			   on to the next name */
			if( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH_WARN )
				algoIDInfo->prefAlgoMismatch = TRUE;
			continue;
			}
		DEBUG_PRINT(( "Offered suite: %s.\n", matchedAlgoInfo->name ));

		/* Make sure that the required algorithms and optional sub-
		   algorithms are available.  We don't check the sub-algorithm type 
		   for the conventional algorithms because in this case it's an 
		   encryption mode, not an algorithm */
		if( !isPseudoAlgo( matchedAlgoInfo->algo ) && \
			!algoAvailable( matchedAlgoInfo->algo ) )
			algoMatched = FALSE;
		if( !isConvAlgo( matchedAlgoInfo->algo ) && \
			matchedAlgoInfo->subAlgo != CRYPT_ALGO_NONE && \
			!algoAvailable( matchedAlgoInfo->subAlgo ) )
			algoMatched = FALSE;

		/* If this is an ECC algorithm and the use of ECC algorithms has 
		   been prevented by external conditions such as the server key
		   not being an ECC key, we can't use it even if ECC algorithms in
		   general are available */
		if( algoMatched && !algoIDInfo->allowECC && \
			isEccAlgo( matchedAlgoInfo->algo ) )
			algoMatched = FALSE;

		/* If the matched algorithm isn't available, remember to warn the 
		   caller if we have to match the first algorithm on the list, then 
		   move on to the next name */
		if( !algoMatched )
			{
			if( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH_WARN )
				algoIDInfo->prefAlgoMismatch = TRUE;
			continue;
			}

		switch( algoIDInfo->getAlgoType )
			{
			case GETALGO_BEST_MATCH:
				/* If we're looking for the best (highest-ranked algorithm)
				   match, see whether the current match ranks higher than
				   the existing one */
				if( currentAlgoIndex < algoIndex )
					{
					algoIndex = currentAlgoIndex;
					if( algoIndex <= 0 )
						foundMatch = TRUE;	/* Gruener werd's net */
					DEBUG_PRINT(( "Accepted suite: %s.\n", 
								  matchedAlgoInfo->name ));
					}
				break;

			case GETALGO_FIRST_MATCH:
				/* If we've found an acceptable algorithm, remember it and
				   exit */
				if( algoIDInfo->preferredAlgo == CRYPT_ALGO_NONE || \
					algoIDInfo->preferredAlgo == matchedAlgoInfo->algo )
					{
					algoIndex = currentAlgoIndex;
					foundMatch = TRUE;
					DEBUG_PRINT(( "Accepted suite: %s.\n", 
								  matchedAlgoInfo->name ));
					}
				break;

			case GETALGO_FIRST_MATCH_WARN:
				/* If we found the algorithm that we're after, remember it
				   and exit */
				if( algoIDInfo->preferredAlgo != matchedAlgoInfo->algo )
					{
					/* We didn't match the first algorithm on the list, warn
					   the caller */
					algoIDInfo->prefAlgoMismatch = TRUE;
					DEBUG_PRINT(( "Accepted suite: %s.\n", 
								  matchedAlgoInfo->name ));
					}
				algoIndex = currentAlgoIndex;
				foundMatch = TRUE;
				break;

			default:
				retIntError();
			}

		/* If we've found a match, we're done unless we're looking for an 
		   extension-info indicator, in which case we have to parse the 
		   entire string */
		if( foundMatch && !algoIDInfo->allowExtIndicator )
			break;	
		}
	ENSURES( LOOP_BOUND_OK );
	if( noStrings >= MAX_NO_SUBSTRINGS )
		{
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, errorInfo, 
				  "Excessive number (more than %d) of SSH algorithm "
				  "strings encountered", noStrings ) );
		}
	if( algoIndex > 50 )	/* Initialisated to 999 at start */
		{
		char algoString[ 256 + 8 ];
		const int algoStringLen = min( stringLen, \
									   min( MAX_ERRMSG_SIZE - 80, 255 ) );

		REQUIRES( algoStringLen > 0 && \
				  algoStringLen <= min( MAX_ERRMSG_SIZE - 80, 255 ) );

		/* We couldn't find anything to use, tell the caller what was
		   available */
		REQUIRES( rangeCheck( algoStringLen, 1, 256 ) );
		memcpy( algoString, string, algoStringLen );
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, errorInfo, 
				  "No algorithm compatible with the remote system's "
				  "selection was found: '%s'", 
				  sanitiseString( algoString, 256, stringLen ) ) );
		}

	/* We found a more-preferred algorithm than the default, go with that */
	algoInfoPtr = &algoIDInfo->algoInfo[ algoIndex ];
	algoIDInfo->algo = algoInfoPtr->algo;
	algoIDInfo->subAlgo = algoInfoPtr->subAlgo;
	algoIDInfo->parameter = algoInfoPtr->parameter;
	DEBUG_PRINT(( "Final accepted suite: %s.\n", algoInfoPtr->name ));

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
int readAlgoString( INOUT STREAM *stream, 
					IN_ARRAY( noAlgoStringEntries ) \
						const ALGO_STRING_INFO *algoInfo,
					IN_RANGE( 1, 100 ) const int noAlgoStringEntries, 
					OUT_ALGO_Z CRYPT_ALGO_TYPE *algo, 
					const BOOLEAN useFirstMatch, 
					INOUT ERROR_INFO *errorInfo )
	{
	ALGOID_INFO algoIDInfo;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( algoInfo, sizeof( ALGO_STRING_INFO ) * \
								 noAlgoStringEntries ) );
	assert( isWritePtr( algo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( noAlgoStringEntries > 0 && noAlgoStringEntries <= 100 );
	REQUIRES( useFirstMatch == TRUE || useFirstMatch == FALSE );

	/* Clear return value */
	*algo = CRYPT_ALGO_NONE;

	setAlgoIDInfo( &algoIDInfo, algoInfo, noAlgoStringEntries, 
				   useFirstMatch ? GETALGO_FIRST_MATCH : GETALGO_BEST_MATCH );
	status = readAlgoStringEx( stream, &algoIDInfo, errorInfo );
	if( cryptStatusOK( status ) )
		*algo = algoIDInfo.algo;
	return( status );
	}

/* Algorithms used to protect data packets are used in pairs, one for
   incoming and the other for outgoing data.  To keep things simple we
   always force these to be the same, first reading the algorithm for one
   direction and then making sure that the one for the other direction
   matches this.  All implementations seem to do this anyway, many aren't
   even capable of supporting asymmetric algorithm choices */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5, 6, 9 ) ) \
static int readAlgoStringPair( INOUT STREAM *stream, 
							   IN_ARRAY( noAlgoStringEntries ) \
									const ALGO_STRING_INFO *algoInfo,
							   IN_RANGE( 1, 100 ) const int noAlgoStringEntries,
							   OUT_ALGO_Z CRYPT_ALGO_TYPE *algo, 
							   OUT_ENUM_OPT( CRYPT_MODE ) CRYPT_MODE_TYPE *mode,
							   OUT_INT_SHORT_Z int *parameter,
							   const BOOLEAN isServer,
							   const BOOLEAN allowAsymmetricAlgos,
							   INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_ALGO_TYPE pairPreferredAlgo;
	ALGOID_INFO algoIDInfo;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( algoInfo, sizeof( ALGO_STRING_INFO ) * \
								 noAlgoStringEntries ) );
	assert( isWritePtr( algo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( noAlgoStringEntries > 0 && noAlgoStringEntries <= 100 );
	REQUIRES( isServer == TRUE || isServer == FALSE );
	REQUIRES( allowAsymmetricAlgos == TRUE || \
			  allowAsymmetricAlgos == FALSE );

	/* Clear return values */
	*algo = CRYPT_ALGO_NONE;
	*mode = CRYPT_MODE_NONE;
	*parameter = 0;

	/* Get the first algorithm */
	setAlgoIDInfo( &algoIDInfo, algoInfo, noAlgoStringEntries, 
				   isServer ? GETALGO_FIRST_MATCH : GETALGO_BEST_MATCH );
	status = readAlgoStringEx( stream, &algoIDInfo, errorInfo );
	if( cryptStatusError( status ) )
		return( status );
	pairPreferredAlgo = algoIDInfo.algo;

	/* Get the matched second algorithm.  Some buggy implementations request
	   mismatched algorithms (at the moment this is only for compression 
	   algorithms) but have no problems in accepting the same algorithm in 
	   both directions, so if we're talking to one of these then we ignore 
	   an algorithm mismatch */
	setAlgoIDInfoEx( &algoIDInfo, algoInfo, noAlgoStringEntries,
					 pairPreferredAlgo, GETALGO_FIRST_MATCH );
	status = readAlgoStringEx( stream, &algoIDInfo, errorInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( pairPreferredAlgo != algoIDInfo.algo && !allowAsymmetricAlgos )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Client algorithm %d doesn't match server algorithm %d "
				  "in algorithm pair", pairPreferredAlgo, 
				  algoIDInfo.algo ) );
		}
	*algo = algoIDInfo.algo;
	*mode = ( CRYPT_MODE_TYPE ) algoIDInfo.subAlgo;
	*parameter = algoIDInfo.parameter;

	return( status );
	}

/* Write a cryptlib algorithm ID as an SSH algorithm name */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoStringEx( INOUT STREAM *stream, 
					   IN_ALGO const CRYPT_ALGO_TYPE algo,
					   IN_INT_SHORT_Z const int subAlgo,
					   IN_INT_SHORT_Z const int parameter,
					   IN_ENUM_OPT( SSH_ALGOSTRINGINFO ) \
							const SSH_ALGOSTRINGINFO_TYPE algoStringInfo )
	{
	int algoIndex, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( algo > CRYPT_ALGO_NONE && algo < CRYPT_ALGO_LAST_EXTERNAL );
	REQUIRES( ( subAlgo >= CRYPT_ALGO_NONE && \
				subAlgo < CRYPT_ALGO_LAST_EXTERNAL ) || \
			  ( subAlgo > CRYPT_MODE_NONE && subAlgo < CRYPT_MODE_LAST ) );
	REQUIRES( parameter >= 0 && parameter < MAX_INTLENGTH_SHORT );
	REQUIRES( isEnumRangeOpt( algoStringInfo, SSH_ALGOSTRINGINFO ) );

	/* Locate the name for this algorithm and optional sub-algoritihm and 
	   encode it as an SSH string */
	LOOP_MED( algoIndex = 0, 
			  algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
				algoStringMapTbl[ algoIndex ].algo != algo && \
				algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
												ALGO_STRING_INFO ), 
			  algoIndex++ );
	ENSURES( LOOP_BOUND_OK );
	ENSURES( algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
											 ALGO_STRING_INFO ) );
	ENSURES( algoStringMapTbl[ algoIndex ].algo == algo );

	/* If there are two algorithm groups (which occurs for the 
	   schizophrenically-specified keyex algorithms) then we may need to 
	   write the name from the second group rather than the first.  The
	   handling of this is somewhat ugly since it hardcodes knowledge of the
	   algorithm table, but there's no generalised way to do this without
	   adding a pile of extra complexity */
	if( algoStringInfo == SSH_ALGOSTRINGINFO_EXTINFO_ALTDHALGOS )
		{
		REQUIRES( algoStringMapTbl[ algoIndex ].algo == \
						algoStringMapTbl[ algoIndex + 2 ].algo );
		REQUIRES( algoStringMapTbl[ algoIndex ].subAlgo == \
						algoStringMapTbl[ algoIndex + 2 ].subAlgo );
		algoIndex += 2;
		}

	/* If there's a sub-algorithm or parameter, find the entry for that */
	if( subAlgo != CRYPT_ALGO_NONE )
		{
		LOOP_MED_CHECKINC( algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
							algoStringMapTbl[ algoIndex ].algo == algo && \
							algoStringMapTbl[ algoIndex ].subAlgo != subAlgo && \
							algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
															ALGO_STRING_INFO ), 
						   algoIndex++ );
		ENSURES( LOOP_BOUND_OK );
		ENSURES( algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
												 ALGO_STRING_INFO ) );
		ENSURES( algoStringMapTbl[ algoIndex ].algo == algo && \
				 algoStringMapTbl[ algoIndex ].subAlgo == subAlgo );
		}
	if( parameter != 0 )
		{
		LOOP_MED_CHECKINC( algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
							algoStringMapTbl[ algoIndex ].algo == algo && \
							algoStringMapTbl[ algoIndex ].parameter != parameter && \
							algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
															ALGO_STRING_INFO ), 
						   algoIndex++ );
		ENSURES( LOOP_BOUND_OK );
		ENSURES( algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
												 ALGO_STRING_INFO ) );
		ENSURES( algoStringMapTbl[ algoIndex ].algo == algo && \
				 algoStringMapTbl[ algoIndex ].parameter == parameter );
		}

	/* If we're writing an extension negotiation indicator then we need to 
	   append it to the algorithm ID.  This is always a client-side indicator
	   since the server writes its indicator as part of the algorithm list */
	if( algoStringInfo == SSH_ALGOSTRINGINFO_EXTINFO || \
		algoStringInfo == SSH_ALGOSTRINGINFO_EXTINFO_ALTDHALGOS )
		{
		writeUint32( stream, algoStringMapTbl[ algoIndex ].nameLen + 11 );
		swrite( stream, algoStringMapTbl[ algoIndex ].name, 
				algoStringMapTbl[ algoIndex ].nameLen );
		return( swrite( stream, ",ext-info-c", 11 ) );
		}

	return( writeString32( stream, algoStringMapTbl[ algoIndex ].name, 
						   algoStringMapTbl[ algoIndex ].nameLen ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoString( INOUT STREAM *stream, 
					 IN_ALGO const CRYPT_ALGO_TYPE algo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( algo > CRYPT_ALGO_NONE && algo < CRYPT_ALGO_LAST_EXTERNAL );

	return( writeAlgoStringEx( stream, algo, CRYPT_ALGO_NONE, 0,
							   SSH_ALGOSTRINGINFO_NONE ) );
	}

/* Write a list of algorithms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeAlgoList( INOUT STREAM *stream, 
				   IN_ARRAY( noAlgoStringInfoEntries ) \
						const ALGO_STRING_INFO *algoStringInfoTbl,
				   IN_RANGE( 1, 10 ) const int noAlgoStringInfoEntries )
	{
	int availAlgoIndex[ 16 + 8 ];
	int noAlgos = 0, length = 0, algoIndex, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( algoStringInfoTbl, sizeof( ALGO_STRING_INFO ) * \
										  noAlgoStringInfoEntries ) );

	REQUIRES( noAlgoStringInfoEntries > 0 && noAlgoStringInfoEntries <= 10 );

	/* Walk down the list of algorithms remembering the encoded name of each
	   one that's available for use */
	LOOP_SMALL( algoIndex = 0,
				algoIndex < noAlgoStringInfoEntries && \
					algoStringInfoTbl[ algoIndex ].algo != CRYPT_ALGO_NONE,
				algoIndex++ )
		{
		const ALGO_STRING_INFO *algoStringInfo = &algoStringInfoTbl[ algoIndex ];

		/* Make sure that this algorithm is available for use */
		if( !isPseudoAlgo( algoStringInfo->algo ) && \
			!algoAvailable( algoStringInfo->algo ) )
			continue;

		/* Make sure that any required sub-algorithms are available */
		if( algoStringInfo->subAlgo != CRYPT_ALGO_NONE && \
			algoStringInfo->subAlgo != CRYPT_MODE_ECB && \
			!algoAvailable( algoStringInfo->subAlgo ) )
			continue;

		/* Remember the algorithm details */
		REQUIRES( noAlgos >= 0 && noAlgos < 16 );
		availAlgoIndex[ noAlgos++ ] = algoIndex;
		length += algoStringInfo->nameLen;
		if( noAlgos > 1 )
			length++;			/* Room for comma delimiter */
		}
	ENSURES( LOOP_BOUND_OK );

	/* Encode the list of available algorithms into a comma-separated string */
	status = writeUint32( stream, length );
	LOOP_MED( algoIndex = 0, cryptStatusOK( status ) && algoIndex < noAlgos,
			  algoIndex++ )
		{
		const ALGO_STRING_INFO *algoStringInfo = \
						&algoStringInfoTbl[ availAlgoIndex[ algoIndex ] ];

		if( algoIndex > 0 )
			sputc( stream, ',' );	/* Add comma delimiter */
		status = swrite( stream, algoStringInfo->name,
						 algoStringInfo->nameLen );
		}
	ENSURES( LOOP_BOUND_OK );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoClassList( INOUT STREAM *stream, 
						IN_ENUM( SSH_ALGOCLASS ) \
							const SSH_ALGOCLASS_TYPE algoClass )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isEnumRange( algoClass, SSH_ALGOCLASS ) );

	/* Write the appropriate algorithm list for this algorithm class */
	switch( algoClass )
		{
		case SSH_ALGOCLASS_KEYEX:
			return( writeAlgoList( stream, algoStringKeyexTbl, 
								   FAILSAFE_ARRAYSIZE( algoStringKeyexTbl, \
													   ALGO_STRING_INFO ) ) );

		case SSH_ALGOCLASS_KEYEX_NOECC:
			return( writeAlgoList( stream, algoStringKeyexNoECCTbl,
								   FAILSAFE_ARRAYSIZE( algoStringKeyexNoECCTbl, \
													   ALGO_STRING_INFO ) ) );

		case SSH_ALGOCLASS_ENCR:
			return( writeAlgoList( stream, algoStringEncrTbl, 
								   FAILSAFE_ARRAYSIZE( algoStringEncrTbl, \
													   ALGO_STRING_INFO ) ) );

		case SSH_ALGOCLASS_SIGN:
			return( writeAlgoList( stream, algoStringPubkeyTbl, 
								   FAILSAFE_ARRAYSIZE( algoStringPubkeyTbl, \
													   ALGO_STRING_INFO ) ) );

		case SSH_ALGOCLASS_MAC:
			return( writeAlgoList( stream, algoStringMACTbl,
								   FAILSAFE_ARRAYSIZE( algoStringMACTbl, \
													   ALGO_STRING_INFO ) ) );

		case SSH_ALGOCLASS_COPR:
			return( writeAlgoList( stream, algoStringCoprTbl,
								   FAILSAFE_ARRAYSIZE( algoStringCoprTbl, \
													   ALGO_STRING_INFO ) ) );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							Miscellaneous Functions							*
*																			*
****************************************************************************/

/* Process a client/server hello packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int processHelloSSH( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT SSH_HANDSHAKE_INFO *handshakeInfo, 
					 OUT_LENGTH_SHORT_Z int *keyexLength,
					 const BOOLEAN isServer )
	{
	CRYPT_ALGO_TYPE dummyAlgo;
	CRYPT_MODE_TYPE mode, dummyMode;
	STREAM stream;
	ALGOID_INFO algoIDInfo;
	BOOLEAN preferredAlgoMismatch = FALSE, guessedKeyex = FALSE;
	int length, dummyParameter, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( keyexLength, sizeof( int ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( isServer == TRUE || isServer == FALSE );

	/* Clear return value */
	*keyexLength = 0;

	/* Process the client/server hello:

		byte		type = SSH_MSG_KEXINIT
		byte[16]	cookie
		string		keyex algorithms
		string		pubkey algorithms
		string		client_crypto algorithms
		string		server_crypto algorithms
		string		client_mac algorithms
		string		server_mac algorithms
		string		client_compression algorithms
		string		server_compression algorithms
		string		client_language
		string		server_language
		boolean		first_keyex_packet_follows
		uint32		reserved

	   The cookie isn't explicitly processed since it's done implicitly when 
	   the hello message is hashed */
	status = length = \
				readHSPacketSSH2( sessionInfoPtr, SSH_MSG_KEXINIT, 128 );
	if( cryptStatusError( status ) )
		return( status );
	*keyexLength = length;
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	status = sSkip( &stream, SSH2_COOKIE_SIZE, SSH2_COOKIE_SIZE );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* Read the keyex algorithm information.  Since this is the first 
	   algorithm list read, we also allow the extension indicator at this 
	   point */
	if( isServer )
		{
		int pkcAlgo;

		setAlgoIDInfoEx( &algoIDInfo, algoStringKeyexTbl, 
						 FAILSAFE_ARRAYSIZE( algoStringKeyexTbl, \
											 ALGO_STRING_INFO ),
						 CRYPT_ALGO_DH, GETALGO_FIRST_MATCH_WARN );

		/* By default the use of ECC algorithms is enabled if support for
		   them is present, however if the server key is a non-ECC key then 
		   it can't be used with an ECC keyex so we have to explicitly
		   disable it (technically it is possible to mix ECDH with RSA but
		   this is more likely an error than anything deliberate) */
		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_GETATTRIBUTE, &pkcAlgo,
								  CRYPT_CTXINFO_ALGO );
		if( cryptStatusError( status ) || !isEccAlgo( pkcAlgo ) )
			algoIDInfo.allowECC = FALSE;
		algoIDInfo.allowExtIndicator = TRUE;
		}
	else
		{
		setAlgoIDInfo( &algoIDInfo, algoStringKeyexTbl, 
					   FAILSAFE_ARRAYSIZE( algoStringKeyexTbl, \
										   ALGO_STRING_INFO ),
					   GETALGO_BEST_MATCH );
		}
	status = readAlgoStringEx( &stream, &algoIDInfo, SESSION_ERRINFO );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	handshakeInfo->keyexAlgo = algoIDInfo.algo;
	if( algoIDInfo.prefAlgoMismatch )
		{
		/* We didn't get a match for our first choice, remember that we have
		   to discard any guessed keyex that may follow */
		preferredAlgoMismatch = TRUE;
		}
	if( algoIDInfo.algo == CRYPT_ALGO_DH )
		{
		handshakeInfo->exchangeHashAlgo = algoIDInfo.subAlgo;
		if( algoIDInfo.parameter > 0 )
			{
			handshakeInfo->requestedServerKeySize = algoIDInfo.parameter;
			handshakeInfo->isFixedDH = TRUE;
			}
		else
			{
			/* We're using negotiated rather than explicit DH keys, we need 
			   to negotiate the keyex key before we can perform the 
			   exchange */
			handshakeInfo->requestedServerKeySize = SSH2_DEFAULT_KEYSIZE;
			}
		}
	if( algoIDInfo.algo == CRYPT_ALGO_ECDH )
		{
		/* If we're using an ECDH cipher suite then we need to switch to the
		   appropriate hash algorithm for the keyex hashing */
		handshakeInfo->isECDH = TRUE;
		handshakeInfo->exchangeHashAlgo = algoIDInfo.subAlgo;
		}
	if( algoIDInfo.extensionIndicator )
		handshakeInfo->sendExtInfo = TRUE;

	/* Read the pubkey (signature) algorithm information */
	if( isServer )
		{
		setAlgoIDInfoEx( &algoIDInfo, handshakeInfo->algoStringPubkeyTbl,
						 handshakeInfo->algoStringPubkeyTblNoEntries,
						 handshakeInfo->pubkeyAlgo, 
						 GETALGO_FIRST_MATCH_WARN );
		}
	else
		{
		setAlgoIDInfo( &algoIDInfo, handshakeInfo->algoStringPubkeyTbl,
					   handshakeInfo->algoStringPubkeyTblNoEntries,
					   GETALGO_BEST_MATCH );
		}
	status = readAlgoStringEx( &stream, &algoIDInfo, SESSION_ERRINFO );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	if( isServer && handshakeInfo->pubkeyAlgo != algoIDInfo.algo )
		{
		sMemDisconnect( &stream );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Client requested pubkey algorithm %s when we "
				  "advertised %s", getAlgoName( algoIDInfo.algo ), 
				  getAlgoName( handshakeInfo->pubkeyAlgo ) ) );
		}
	handshakeInfo->pubkeyAlgo = algoIDInfo.algo;
	handshakeInfo->hashAlgo = algoIDInfo.subAlgo;
	if( algoIDInfo.prefAlgoMismatch )
		{
		/* We didn't get a match for our first choice, remember that we have
		   to discard any guessed keyex that may follow */
		preferredAlgoMismatch = TRUE;
		}

	/* Read the encryption and MAC algorithm information */
	status = readAlgoStringPair( &stream, algoStringEncrTbl,
								 FAILSAFE_ARRAYSIZE( algoStringEncrTbl, \
													 ALGO_STRING_INFO ),
								 &sessionInfoPtr->cryptAlgo, &mode,
								 &handshakeInfo->cryptKeysize, isServer, 
								 FALSE, SESSION_ERRINFO );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoStringPair( &stream, algoStringMACTbl,
									 FAILSAFE_ARRAYSIZE( algoStringMACTbl, \
														 ALGO_STRING_INFO ),
									 &sessionInfoPtr->integrityAlgo, 
									 &dummyMode, &dummyParameter, isServer, 
									 FALSE, SESSION_ERRINFO );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );

		/* Some implementations don't support the mandatory-to-implement SSH 
		   encryption/MAC algorithms, in which case we let the caller know 
		   that they're broken */
		if( status == CRYPT_ERROR_NOTAVAIL && \
			TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_NOMTI ) )
			{
			retExtErrAlt( status, 
						  ( status, SESSION_ERRINFO,
							", the server doesn't support the mandatory-to-"
							"implement SSH algorithms" ) );
			}
		return( status );
		}
#ifdef USE_SSH_CTR
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_AES && \
		mode == CRYPT_MODE_ECB )
		{
		/* If the indicated mode is ECB, which we use to synthesise CTR 
		   mode, remember that we're using CTR mode encryption */
		SET_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_CTR );
		}
#endif /* USE_SSH_CTR */

	/* Read the remaining algorithm information.  The final reserved value 
	   should always be zero but we don't specifically check for this since 
	   at some point in the future it may become non-zero */
	status = readAlgoStringPair( &stream, algoStringCoprTbl, 
								 FAILSAFE_ARRAYSIZE( algoStringCoprTbl, \
													 ALGO_STRING_INFO ),
								 &dummyAlgo, &dummyMode, &dummyParameter, 
								 isServer, 
								 TEST_FLAG( sessionInfoPtr->protocolFlags, 
											SSH_PFLAG_ASYMMCOPR ) ? \
									TRUE : FALSE, SESSION_ERRINFO );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	readUniversal32( &stream );
	status = readUniversal32( &stream );		/* Language string pair */
	if( cryptStatusOK( status ) )
		{
		int value;

		status = value = sgetc( &stream );
		if( !cryptStatusError( status ) )
			{
			if( value != 0 )
				guessedKeyex = TRUE;
			status = readUint32( &stream );		/* Reserved value */
			}
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid hello packet language string/trailer data" ) );
		}

	/* If we're using an alternative exchange hash algorithm, switch the 
	   contexts around to using the alternative one for hashing from now 
	   on */
	if( handshakeInfo->exchangeHashAlgo == CRYPT_ALGO_SHA2 )
		{
		const CRYPT_CONTEXT tempContext = handshakeInfo->iExchangeHashContext;

		handshakeInfo->iExchangeHashContext = \
				handshakeInfo->iExchangeHashAltContext;
		handshakeInfo->iExchangeHashAltContext = tempContext;
		}

	/* If there's a guessed keyex following this packet and we didn't match
	   the first-choice keyex/pubkey algorithm, tell the caller to skip it */
	if( guessedKeyex && preferredAlgoMismatch )
		return( OK_SPECIAL );

	return( CRYPT_OK );
	}

/* Read/write extension information packets:

	byte		type = SSH_MSG_EXT_INFO
	uint32		no_extensions
		string	name
		string	value (binary data) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readExtensionsSSH( INOUT SESSION_INFO *sessionInfoPtr,
					   INOUT STREAM *stream )
	{
	int noExtensions, i, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );

	/* Get the number of extensions present and make sure that it's valid */
	status = noExtensions = readUint32( stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid SSH extension information" ) );
		}
	if( noExtensions < 1 || noExtensions > 16 )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid SSH extension count %d, should be 0...16", 
				  noExtensions ) );
		}

	/* Process the extensions */
	LOOP_MED( i = 0, i < noExtensions, i++ )
		{
		BYTE nameBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];
		void *dataPtr DUMMY_INIT_PTR;
		int nameLength, dataLength;

		/* Read the extension name */
		status = readString32( stream, nameBuffer, CRYPT_MAX_TEXTSIZE, 
							   &nameLength );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Invalid SSH extension name for extension %d", i ) );
			}

		/* Read the extension data.  This may in theory be of zero length 
		   for some extensions, although currently all zero-length 
		   extensions consist of a redundant 'y' or 'n' to back up the 
		   presence of the extension itself */
		status = dataLength = readUint32( stream );
		if( !cryptStatusError( status ) && dataLength != 0 )
			{
			/* If there's data present then it must have a valid length */
			if( dataLength < 1 || dataLength >= MAX_INTLENGTH_SHORT )
				status = CRYPT_ERROR_BADDATA;
			else
				{
				/* Get a pointer to the data payload */
				status = sMemGetDataBlock( stream, &dataPtr, dataLength );
				if( cryptStatusOK( status ) )
					{
					status = sSkip( stream, dataLength, 
									MAX_INTLENGTH_SHORT );
					}
				}
			}
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid extension data for extension %d, '%s'", i,
					  sanitiseString( nameBuffer, CRYPT_MAX_TEXTSIZE, 
									  nameLength ) ) );
			}
		ENSURES( dataLength >= 0 && dataLength < MAX_INTLENGTH_SHORT );
		ANALYSER_HINT( dataPtr != NULL );
		DEBUG_PRINT(( "Read extension %d, '%s', length %d.\n", i,
					  sanitiseString( nameBuffer, CRYPT_MAX_TEXTSIZE, 
									  nameLength ), 
					  dataLength ));
		DEBUG_DUMP_DATA( dataPtr, dataLength );

		/* Process the extension data.  For now there's nothing much to do 
		   here, the only extension that really affects us is 
		   "server-sig-algs" which in theory is required before using RSA 
		   with SHA-2 signatures for client auth, however the presence of
		   a SHA-2 capability on the server always seems to imply SHA-2 
		   client signature handling so it's not clear whether the extra
		   parsing and processing is really worth it, particularly since
		   using RSA-with-SHA2 when SHA2 is indicated always seems to work
		   while using it only when "server-sig-algs" is present means that 
		   it'll only work on the subset of servers that implement 
		   extensions */
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeExtensionsSSH( INOUT SESSION_INFO *sessionInfoPtr,
						INOUT STREAM *stream )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Write the total extension count.  See the comment for the 
	   no-flow-control extension for why we only write this if basic SSH
	   functionality is enabled */
#ifndef USE_SSH_EXTENDED 
	writeUint32( stream, 2 );
#else
	writeUint32( stream, 1 );
#endif /* USE_SSH_EXTENDED */

	/* Write the server signature algorithms extension */
	status = writeString32( stream, "server-sig-algs", 15 );
	if( cryptStatusOK( status ) )
		status = writeAlgoClassList( stream, SSH_ALGOCLASS_SIGN );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the no-flow-control extension.  This is a real pain to deal 
	   with, the RFC requires, for no obvious reason, that "Implementations 
	   MUST refuse to open more than one simultaneous channel when this 
	   extension is in effect", but then bizarrely adds that "Nevertheless, 
	   server implementations SHOULD support clients opening more than one 
	   non-simultaneous channel".  This confusion will no doubt lead to more
	   or less arbitrary behaviour among implementations, rather than having
	   to fingerprint and identify issues in who knows how many different
	   versions we only send this extension if USE_SSH_EXTENDED isn't 
	   defined, in which case we only allow a single channel no matter 
	   what */
#ifndef USE_SSH_EXTENDED 
	writeString32( stream, "no-flow-control", 15 );
	status = writeString32( stream, "p", 1 );
#endif /* !USE_SSH_EXTENDED */
	
	return( status );
	}

/* Read and check a public key:

   RSA/DSA:
	string		[ server key/certificate ]
		string	"ssh-rsa"	"ssh-dss"
		mpint	e			p			
		mpint	n			q
		mpint				g
		mpint				y

   ECDSA:
	string		[ server key/certificate ]
		string	"ecdsa-sha2-*"
		string	"*"				-- The "*" portion from the above field
		string	Q */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int checkReadPublicKey( INOUT STREAM *stream,
						OUT_ALGO_Z CRYPT_ALGO_TYPE *pubkeyAlgo,
						OUT_INT_SHORT_Z int *keyDataStart,
						INOUT ERROR_INFO *errorInfo )
	{
	int dummy, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyDataStart, sizeof( int ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	/* Clear return values */
	*pubkeyAlgo = CRYPT_ALGO_NONE;
	*keyDataStart = 0;

	status = readUint32( stream );	/* Server key data size */
	if( !cryptStatusError( status ) )
		{
		status = readAlgoString( stream, algoStringPubkeyTbl, 
								 FAILSAFE_ARRAYSIZE( algoStringPubkeyTbl, \
													 ALGO_STRING_INFO ), 
								 pubkeyAlgo, TRUE, errorInfo );
		}
	if( cryptStatusError( status ) )
		return( status );
	streamBookmarkSet( stream, *keyDataStart  );
	switch( *pubkeyAlgo )
		{
		case CRYPT_ALGO_RSA:
			/* RSA e, n */
			readInteger32( stream, NULL, &dummy, 1, CRYPT_MAX_PKCSIZE );
			status = readInteger32Checked( stream, NULL, &dummy, 
										   MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
			break;

#ifdef USE_DSA
		case CRYPT_ALGO_DSA:
			/* DSA p, q, g, y */
			status = readInteger32Checked( stream, NULL, &dummy, 
										   MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
			if( cryptStatusError( status ) )
				break;
			readInteger32( stream, NULL, &dummy, 1, CRYPT_MAX_PKCSIZE );
			readInteger32( stream, NULL, &dummy, 1, CRYPT_MAX_PKCSIZE );
			status = readInteger32Checked( stream, NULL, &dummy, 
										   MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
			break;
#endif /* USE_DSA */

#ifdef USE_ECDSA
		case CRYPT_ALGO_ECDSA:
			readUniversal32( stream );		/* Skip field size */
			status = readInteger32Checked( stream, NULL, &dummy, 
										   MIN_PKCSIZE_ECCPOINT, 
										   MAX_PKCSIZE_ECCPOINT );
			break;
#endif /* USE_ECDSA */

		default:
			retIntError();
		}

	return( status );
	}

/****************************************************************************
*																			*
*								Get/Put Data Functions						*
*																			*
****************************************************************************/

/* Process a control message received during the processBodyFunction() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processControlMessage( INOUT SESSION_INFO *sessionInfoPtr,
								  IN_DATALENGTH_Z const int payloadLength )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	STREAM stream;
	int localPayloadLength = payloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( payloadLength >= 0 && payloadLength < MAX_BUFFER_SIZE );

	/* Putty 0.59 erroneously sent zero-length SSH_MSG_IGNORE packets, if 
	   we find one of these then we convert it into a valid packet.  Writing 
	   into the buffer at this position is safe because we've got padding 
	   and at least sessionInfoPtr->authBlocksize bytes of MAC following the 
	   current position.  We can also modify the localPayloadLength value 
	   for the same reason */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, 
				   SSH_PFLAG_ZEROLENIGNORE ) && \
		sshInfo->packetType == SSH_MSG_IGNORE && localPayloadLength == 0 )
		{
		memset( bufPtr, 0, UINT32_SIZE );
		localPayloadLength = UINT32_SIZE;
		}

	/* Make sure that the message length is valid.  This will be caught 
	   anyway when we try and process the channel control message (and the
	   excessive-length check has already been performed by the packet-read 
	   code) but checking it here avoids an assertion in the debug build 
	   when we connect the stream, as well as just being good programming 
	   practice */
	if( localPayloadLength <= 0 || \
		localPayloadLength > sessionInfoPtr->receiveBufEnd - \
							 sessionInfoPtr->receiveBufPos )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid session control message payload length %d for "
				  "%s (%d), should be 0...%d", localPayloadLength, 
				  getSSHPacketName( sshInfo->packetType ), 
				  sshInfo->packetType, sessionInfoPtr->receiveBufEnd - \
									   sessionInfoPtr->receiveBufPos ) );
		}

	/* Process the control message and reset the receive buffer indicators 
	   to clear it */
	ENSURES( boundsCheckZ( sessionInfoPtr->receiveBufPos, localPayloadLength,
						   sessionInfoPtr->receiveBufEnd ) );
	sMemConnect( &stream, bufPtr, localPayloadLength );
	status = processChannelControlMessage( sessionInfoPtr, &stream );
	sMemDisconnect( &stream );
	sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos;
	sessionInfoPtr->pendingPacketLength = 0;

	return( status );
	}

/* Read data over the SSH link */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readHeaderFunction( INOUT SESSION_INFO *sessionInfoPtr,
							   OUT_ENUM_OPT( READINFO ) \
									READSTATE_INFO *readInfo )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	int length, extraLength, removedDataLength = ID_SIZE + PADLENGTH_SIZE;
	int partialPayloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );

	/* Clear return value */
	*readInfo = READINFO_NONE;

	/* Make sure that there's room left to handle the speculative read */
	if( sessionInfoPtr->receiveBufPos >= \
								sessionInfoPtr->receiveBufSize - 128 )
		return( 0 );

	/* Try and read the header data from the remote system */
	REQUIRES( sessionInfoPtr->receiveBufPos == sessionInfoPtr->receiveBufEnd );
	status = readPacketHeaderSSH2( sessionInfoPtr, SSH_MSG_CHANNEL_DATA,
								   &length, &extraLength, sshInfo, 
								   readInfo, SSH_PROTOSTATE_DATA );
	if( cryptStatusError( status ) )
		{
		/* OK_SPECIAL means that we got a soft timeout before the entire 
		   header was read, so we return zero bytes read to tell the 
		   calling code that there's nothing more to do */
		if( status == OK_SPECIAL ) 
			return( 0 );

		return( status );
		}

	/* All errors from this point are fatal crypto errors */
	*readInfo = READINFO_FATAL_CRYPTO;

	ENSURES( length >= ID_SIZE + PADLENGTH_SIZE + SSH2_MIN_PADLENGTH_SIZE && \
			 length <= sessionInfoPtr->receiveBufSize - \
					   sessionInfoPtr->receiveBufPos && \
			 length < MAX_BUFFER_SIZE );
	status = checkMacSSHIncremental( sessionInfoPtr->iAuthInContext, 
									 sshInfo->readSeqNo, bufPtr, 
									 MIN_PACKET_SIZE - LENGTH_SIZE,
									 MIN_PACKET_SIZE - LENGTH_SIZE, 
									 length, MAC_START, 
									 sessionInfoPtr->authBlocksize );
	if( cryptStatusError( status ) )
		{
		/* We don't return an extended status at this point because we
		   haven't completed the message MAC calculation/check yet so 
		   any errors will be cryptlib-internal ones */
		return( status );
		}

	/* If it's channel data, strip the encapsulation, which allows us to
	   process the payload directly without having to move it around in
	   the buffer:

	  [	uint32		length (excluding MAC size)	- Processed in rPHSSH2() ]
		byte		padLen
		byte		SSH_MSG_CHANNEL_DATA
			uint32	recipient channel
			uint32	dataLength	| string	data
			byte[]	data		|
	  [ byte[]		padding ]
	  [	byte[]		MAC ] */
	if( sshInfo->packetType == SSH_MSG_CHANNEL_DATA )
		{
		STREAM stream;
		int payloadLength;

		static_assert( SSH_HEADER_REMAINDER_SIZE >= ID_SIZE + \
							PADLENGTH_SIZE + SSH2_MIN_PADLENGTH_SIZE,
					   "SSH header size read" );

		ENSURES( boundsCheckZ( sessionInfoPtr->receiveBufPos, 
							   SSH_HEADER_REMAINDER_SIZE, 
							   sessionInfoPtr->receiveBufSize ) );

		sMemConnect( &stream, bufPtr, SSH_HEADER_REMAINDER_SIZE );

		/* Skip the type, padding length, and channel number and make sure 
		   that the payload length matches the packet length */
		sSkip( &stream, PADLENGTH_SIZE + ID_SIZE + UINT32_SIZE,
			   PADLENGTH_SIZE + ID_SIZE + UINT32_SIZE );
		status = payloadLength = readUint32( &stream );
		if( !cryptStatusError( status ) )
			removedDataLength = stell( &stream );
		if( cryptStatusError( status ) || \
			payloadLength != length - ( removedDataLength + \
										sshInfo->padLength ) )
			{
			sMemDisconnect( &stream );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid data packet payload length %d for "
					  "SSH_MSG_CHANNEL_DATA (94), should be %d", 
					  cryptStatusError( status ) ? 0 : payloadLength,
					  length - ( removedDataLength + sshInfo->padLength ) ) );
			}

		/* Errors are back to standard fatal errors */
		*readInfo = READINFO_FATAL;

		/* Move back to the start of the payload and process the channel 
		   data header, required in order to handle window size updates.  
		   This consists of the channel number and yet another length value,
		   present at the start of the payload which is encoded as an SSH
		   string (uint32 length + data).  This value has already been
		   checked above, and is only accepted if it matches the outer
		   length value.  This is important because the data hasn't been
		   verified by the MAC yet, since we need to process the header in
		   order to find out where the MAC is.  This means that the channel
		   number is processed unverified, but this shouldn't be a major
		   issue since at most an attacker can corrupt the value, and it 
		   will end up being mapped to an invalid channel with a high
		   probability */
		sseek( &stream, PADLENGTH_SIZE + ID_SIZE );
		status = processChannelControlMessage( sessionInfoPtr, &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Errors are back to standard fatal errors */
	*readInfo = READINFO_FATAL;

	/* Move the remainder down to the start of the buffer.  The general idea
	   is to remove all of the header data so that only the payload remains
	   in the buffer, avoiding the need to move it down afterwards:

			 rBufPos
				|
				v				|<-- pPayloadLen -->|
		+-------+---------------+-------------------+-------+
		|		|				|///////////////////|		|
		+-------+---------------+-------------------+-------+
				^<-removedDLen->|
				|
			 bufPtr
	   
	   This is complicated by the fact that (unlike SSL) all of the data 
	   (including the header) is encrypted and MAC'd so we can't just read 
	   that separately but have to process it as part of the payload, remove 
	   it, and remember anything that's left for later */
	REQUIRES( isShortIntegerRangeNZ( removedDataLength ) );
	partialPayloadLength = SSH_HEADER_REMAINDER_SIZE - removedDataLength;
	ENSURES( partialPayloadLength > 0 && \
			 removedDataLength + partialPayloadLength <= \
				sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufPos && \
			 removedDataLength + partialPayloadLength < MAX_BUFFER_SIZE );
	REQUIRES( boundsCheck( sessionInfoPtr->receiveBufPos + removedDataLength, 
						   partialPayloadLength, 
						   sessionInfoPtr->receiveBufSize ) );
	memmove( bufPtr, bufPtr + removedDataLength, partialPayloadLength );

	/* Determine how much data we'll be expecting, adjusted for the fixed
	   information that we've removed and the (implicitly present) MAC data */
	sessionInfoPtr->pendingPacketLength = \
			sessionInfoPtr->pendingPacketRemaining = \
					( length + extraLength ) - removedDataLength;
	ENSURES( sessionInfoPtr->pendingPacketLength > 0 && \
			 sessionInfoPtr->pendingPacketLength < MAX_BUFFER_SIZE );
	sshInfo->partialPacketDataLength = partialPayloadLength;

	/* Indicate that we got some payload as part of the header */
	*readInfo = READINFO_HEADERPAYLOAD;
	return( partialPayloadLength );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processBodyFunction( INOUT SESSION_INFO *sessionInfoPtr,
								OUT_ENUM_OPT( READINFO ) READSTATE_INFO *readInfo )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	BYTE *dataRemainingPtr = sessionInfoPtr->receiveBuffer + \
							 sessionInfoPtr->receiveBufPos + \
							 sshInfo->partialPacketDataLength;
	const int dataRemainingSize = sessionInfoPtr->pendingPacketLength - \
								  sshInfo->partialPacketDataLength;
	const int dataLength = dataRemainingSize - sessionInfoPtr->authBlocksize;
	int payloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( boundsCheck( sessionInfoPtr->receiveBufPos + \
							sshInfo->partialPacketDataLength,
						   dataRemainingSize, sessionInfoPtr->receiveBufEnd ) );
	REQUIRES( dataRemainingSize >= sessionInfoPtr->authBlocksize && \
			  dataLength >= 0 && dataLength < dataRemainingSize );

	/* All errors processing the payload are fatal, and for the following 
	   operations specifically fatal crypto errors */
	*readInfo = READINFO_FATAL_CRYPTO;

	/* Decrypt the packet in the buffer and MAC the payload.  The length may
	   be zero if the entire message fits into the already-processed fixed-
	   length header portion, e.g. for channel-close messages that only 
	   contain a channel number:
																Key:
			Processed in header read							+--+
		recBufPos |												|  | Processed
			|<----v----- pendingPacketLength ---------->|		+--+
			v<- pPDL -->|								|		+--+
		----+-----------+-----------------------+-------+--		|//| Encrypted
			|			|///////////////////////|\\\\\\\|		+--+
		----+-----------+-----------------------+-------+--		+--+
						|<---- dataLength ----->|		|		|\\| MAC
						|<------- dataRemaining ------->|		+--+ */
	if( dataLength > 0 )
		{
#ifdef USE_SSH_CTR
		if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSH_PFLAG_CTR ) )
			{
			status = ctrModeCrypt( sessionInfoPtr->iCryptInContext,
								   sshInfo->readCTR, 
								   sessionInfoPtr->cryptBlocksize,
								   dataRemainingPtr, dataLength );
			}
		else
#endif /* USE_SSH_CTR */
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_CTX_DECRYPT, dataRemainingPtr,
								  dataLength );
		if( cryptStatusOK( status ) )
			{
			status = checkMacSSHIncremental( sessionInfoPtr->iAuthInContext, 0,
											 dataRemainingPtr, dataRemainingSize, 
											 dataLength, 0, MAC_END, 
											 sessionInfoPtr->authBlocksize );
			}
		}
	else
		{
		status = checkMacSSHIncremental( sessionInfoPtr->iAuthInContext, 0, 
										 dataRemainingPtr, dataRemainingSize, 
										 0, 0, MAC_END, 
										 sessionInfoPtr->authBlocksize );
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Bad message MAC for %s (%d) packet, length %d",
				  getSSHPacketName( sshInfo->packetType ),
				  sshInfo->packetType,
				  sshInfo->partialPacketDataLength + dataLength ) );
		}

	/* Errors are back to standard fatal errors */
	*readInfo = READINFO_FATAL;

	/* Strip the padding and MAC and update the state information */
	payloadLength = sessionInfoPtr->pendingPacketLength - \
					( sshInfo->padLength + sessionInfoPtr->authBlocksize );
	sshInfo->readSeqNo++;
	ENSURES( payloadLength >= 0 && \
			 payloadLength < sessionInfoPtr->pendingPacketLength + dataLength && \
			 payloadLength < MAX_BUFFER_SIZE );
			 /* pendingPacketLength check must be '<' rather than '<=' 
			    because of the stripped padding */
	DEBUG_PRINT(( "Read %s (%d) packet, length %d.\n", 
				  getSSHPacketName( sshInfo->packetType ), 
				  sshInfo->packetType, payloadLength ));
	DEBUG_DUMP_DATA( sessionInfoPtr->receiveBuffer + \
					 sessionInfoPtr->receiveBufPos, payloadLength );

	/* If it's not plain data (which was handled at the readHeaderFunction()
	   stage), handle it as a control message */
	if( sshInfo->packetType != SSH_MSG_CHANNEL_DATA )
		{
		status = processControlMessage( sessionInfoPtr, payloadLength );
		if( cryptStatusError( status ) )
			{
			/* If we got an OK_SPECIAL status then the packet was handled
			   internally and we can try again.  If it was a message that
			   the user has to respond to it's also not a fatal error
			   condition and they can continue afterwards */
			if( status == OK_SPECIAL || status == CRYPT_ENVELOPE_RESOURCE )
				*readInfo = READINFO_NOOP;
			return( status );
			}
		}
	sshInfo->partialPacketDataLength = 0;

	*readInfo = READINFO_NONE;
	return( payloadLength );
	}

/* Write data over the SSH link */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
static int preparePacketFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	STREAM stream;
	const int dataLength = sessionInfoPtr->sendBufPos - \
						   ( SSH2_HEADER_SIZE + SSH2_PAYLOAD_HEADER_SIZE );
	int length DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSession( sessionInfoPtr ) );
	REQUIRES( !TEST_FLAG( sessionInfoPtr->flags, 
						  SESSION_FLAG_SENDCLOSED ) );
	REQUIRES( dataLength > 0 && dataLength < sessionInfoPtr->sendBufPos && \
			  dataLength < MAX_BUFFER_SIZE );

	/* Wrap up the payload ready for sending:

		byte		SSH_MSG_CHANNEL_DATA
		uint32		channel_no
		string		data

	   Since this is wrapping in-place data, we first open a write stream to
	   add the header, then open a read stream covering the full buffer in
	   preparation for wrapping the packet */
	status = openPacketStreamSSHEx( &stream, sessionInfoPtr, 
									SSH2_PAYLOAD_HEADER_SIZE,
									SSH_MSG_CHANNEL_DATA );
	if( cryptStatusError( status ) )
		return( status );
	writeUint32( &stream, getCurrentChannelNo( sessionInfoPtr, \
											   CHANNEL_WRITE ) );
	INJECT_FAULT( SESSION_SSH_CORRUPT_CHANNEL_DATA, 
				  SESSION_SSH_CORRUPT_CHANNEL_DATA_1 );
	status = writeUint32( &stream, dataLength );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	sMemConnect( &stream, sessionInfoPtr->sendBuffer,
				 sessionInfoPtr->sendBufSize );
	status = sSkip( &stream, SSH2_HEADER_SIZE + SSH2_PAYLOAD_HEADER_SIZE + \
							 dataLength, SSKIP_MAX );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, FALSE );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	INJECT_FAULT( SESSION_CORRUPT_DATA, SESSION_CORRUPT_DATA_SSH_1 );

	/* If there's control data enqueued to be written, try and append it to
	   the existing data to be sent.  This may or may not append it 
	   (depending on whether there's room in the send buffer) so we may end
	   up here more than once */
	if( sshInfo->response.type > 0 )
		{
		int length2;

		status = length2 = appendChannelData( sessionInfoPtr, length );
		if( !cryptStatusError( status  ) )
			length += length2;
		}

	return( length );
	}

/* Close a previously-opened SSH session */

STDC_NONNULL_ARG( ( 1 ) ) \
static void shutdownFunction( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_V( sanityCheckSession( sessionInfoPtr ) );

	/* If we haven't entered the secure state yet (i.e. we're still in the
	   middle of the handshake) then this is an abnormal termination, send 
	   a disconnect indication:

		byte		SSH_MSG_DISCONNECT
		uint32		reason_code = SSH_DISCONNECT_PROTOCOL_ERROR
		string		description = "Handshake failed"
		string		language_tag = "" */
	if( !TEST_FLAG( sessionInfoPtr->flags, SESSION_FLAG_ISSECURE_WRITE ) )
		{
		STREAM stream;
		int status;

		status = openPacketStreamSSH( &stream, sessionInfoPtr, 
									  SSH_MSG_DISCONNECT );
		if( cryptStatusError( status ) )
			{
			sNetDisconnect( &sessionInfoPtr->stream );
			return;
			}
		writeUint32( &stream, SSH_DISCONNECT_PROTOCOL_ERROR );
		writeString32( &stream, "Handshake failed", 16 );
		status = writeUint32( &stream, 0 );		/* No language tag */
		if( cryptStatusOK( status ) )
			status = wrapPlaintextPacketSSH2( sessionInfoPtr, &stream, 0 );
		if( cryptStatusOK( status ) )
			{
			const int length = stell( &stream );
			void *dataPtr;

			/* Since there's nothing much that we can do at this point in 
			   response to an error except continue and close the network
			   session, we don't check for errors */
			status = sMemGetDataBlockAbs( &stream, 0, &dataPtr, length );
			if( cryptStatusOK( status ) )
				{
				( void ) sendCloseNotification( sessionInfoPtr, dataPtr, 
												length );
				}
			}
		sMemDisconnect( &stream );
		return;
		}

	/* Close all remaining channels.  Since this is just a cleanup of a 
	   network session that's about to be closed anyway we ignore any errors 
	   that we encounter at this point (a typical error would be the link 
	   going down, in which case the only useful response is to take down 
	   the network session anyway) */
	( void ) closeChannel( sessionInfoPtr, TRUE );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

/* Set up access to the SSH session processing */

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2processing( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
	FNPTR_SET( sessionInfoPtr->readHeaderFunction, readHeaderFunction );
	FNPTR_SET( sessionInfoPtr->processBodyFunction, processBodyFunction );
	FNPTR_SET( sessionInfoPtr->preparePacketFunction, preparePacketFunction );
	FNPTR_SET( sessionInfoPtr->shutdownFunction, shutdownFunction );
	}
#endif /* USE_SSH */
