/****************************************************************************
*																			*
*						cryptlib SSHv2 Session Management					*
*						Copyright Peter Gutmann 1998-2014					*
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

   Note that ECC support is very hit-and-miss.  If we were to advertise ECC 
   only (which we never do), some servers will respond with RSA/DSA keys 
   (even though they're not specified as being supported), and others will 
   respond with an empty host key.
   
   In addition the algorithms aren't just algorithm values but a combination 
   of the algorithm, the key size, and the hash algorithm, with 
   CRYPT_ALGO_ECDH/CRYPT_ALGO_ECDSA being the default P256 curve with 
   SHA-256.  Because the curve types are tied to the oddball SHA-2 hash 
   variants (we can't just use SHA-256 for every curve), we don't support 
   P384 and P512 because we'd have to support an entirely new (and 64-bit-
   only) hash algorithm for each of the curves */

static const ALGO_STRING_INFO FAR_BSS algoStringKeyexTbl[] = {
#if defined( USE_ECDH ) && defined( PREFER_ECC )
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },
#endif /* USE_ECDH && PREFER_ECC */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group1-sha1", 26, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
#if defined( USE_ECDH ) && !defined( PREFER_ECC ) 
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },
#endif /* USE_ECDH && !PREFER_ECC */
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
	};
static const ALGO_STRING_INFO FAR_BSS algoStringKeyexNoECCTbl[] = {
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group1-sha1", 26, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringPubkeyTbl[] = {
#ifdef PREFER_ECC
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
#endif /* PREFER_ECC */
	{ "rsa-sha2-256", 12, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2 },
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1 },
	{ "ssh-dss", 7, CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1 },
#if !defined( PREFER_ECC )
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
#endif /* !PREFER_ECC */
	{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }, 
		{ NULL, 0, CRYPT_ALGO_NONE, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringEncrTbl[] = {
	{ "aes128-cbc", 10, CRYPT_ALGO_AES },
	{ "3des-cbc", 8, CRYPT_ALGO_3DES },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringMACTbl[] = {
	{ "hmac-sha2-256", 13, CRYPT_ALGO_HMAC_SHA2 },
	{ "hmac-sha1", 9, CRYPT_ALGO_HMAC_SHA1 },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringCoprTbl[] = {
	{ "none", 4, CRYPT_ALGO_AES /* Always-valid placeholder */ },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

/* A grand unified version of the above */

static const ALGO_STRING_INFO FAR_BSS algoStringMapTbl[] = {
	/* Keyex algorithms */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
	{ "diffie-hellman-group1-sha1", 26, CRYPT_ALGO_DH, CRYPT_ALGO_SHA1 },
#ifdef USE_ECDH
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },
#endif /* USE_ECDH */

	/* Signature algorithms */
	{ "rsa-sha2-256", 12, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2 },
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1 },
	{ "ssh-dss", 7, CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1 },
#ifdef USE_ECDSA
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
#endif /* USE_ECDSA */

	/* Encryption algorithms */
	{ "aes128-cbc", 10, CRYPT_ALGO_AES },
	{ "3des-cbc", 8, CRYPT_ALGO_3DES },

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
	assert( isReadPtr( clientString, clientStringLength ) );
	assert( isReadPtr( serverString, serverStringLength ) );

	REQUIRES( clientStringLength > 0 && \
			  clientStringLength < MAX_INTLENGTH_SHORT );
	REQUIRES( serverStringLength > 0 && \
			  serverStringLength < MAX_INTLENGTH_SHORT );

	/* SSH hashes the handshake ID strings for integrity-protection purposes, 
	   first the client string and then the server string, encoded as SSH 
	   string values.  In addition since the handshake can retroactively 
	   switch to a different hash algorithm mid-exchange we have to 
	   speculatively hash the messages with alternative algorithms in case 
	   the other side decides to switch */
	status = hashAsString( handshakeInfo->iExchangeHashContext, 
						   clientString, clientStringLength );
	if( cryptStatusOK( status ) )
		status = hashAsString( handshakeInfo->iExchangeHashContext,
							   serverString, serverStringLength );
	if( handshakeInfo->iExchangeHashAltContext == CRYPT_ERROR )
		return( status );
	status = hashAsString( handshakeInfo->iExchangeHashAltContext, 
						   clientString, clientStringLength );
	if( cryptStatusOK( status ) )
		status = hashAsString( handshakeInfo->iExchangeHashAltContext,
							   serverString, serverStringLength );
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

	/* Information returned by the read-algorithm function */
	CRYPT_ALGO_TYPE algo;			/* Matched algorithm */
	CRYPT_ALGO_TYPE subAlgo;		/* Sub-algorithm (e.g. hash for keyex) */
	BOOLEAN prefAlgoMismatch;		/* First match != preferredAlgo */
	} ALGOID_INFO;

#if defined( USE_ECDH ) || defined( USE_ECDSA )
  #define ALLOW_ECC		TRUE
#else
  #define ALLOW_ECC		FALSE
#endif /* USE_ECDH || USE_ECDSA */

#define setAlgoIDInfo( algoIDInfo, algoStrInfo, algoStrInfoEntries, prefAlgo, getType ) \
	{ \
	memset( ( algoIDInfo ), 0, sizeof( ALGOID_INFO ) ); \
	( algoIDInfo )->algoInfo = ( algoStrInfo ); \
	( algoIDInfo )->noAlgoInfoEntries = ( algoStrInfoEntries ); \
	( algoIDInfo )->preferredAlgo = ( prefAlgo ); \
	( algoIDInfo )->getAlgoType = ( getType ); \
	( algoIDInfo )->allowECC = ALLOW_ECC; \
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readAlgoStringEx( INOUT STREAM *stream, 
							 INOUT ALGOID_INFO *algoIDInfo,
							 INOUT ERROR_INFO *errorInfo )
	{
	BOOLEAN foundMatch = FALSE;
	void *string DUMMY_INIT_PTR;
	int stringPos, stringLen, substringLen, algoIndex = 999;
	int noStrings, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( algoIDInfo, sizeof( ALGOID_INFO ) ) );
	assert( isReadPtr( algoIDInfo->algoInfo, \
					   sizeof( ALGO_STRING_INFO ) * \
							algoIDInfo->noAlgoInfoEntries ) );
	
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
			  algoIDInfo->noAlgoInfoEntries < MAX_INTLENGTH_SHORT );

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
				   ^
				   |
				substrLen 

	   This works by walking an index stringPos down the string, with each 
	   substring delimited by { stringPos, subStringLen }, which is checked
	   against the table of algorithm names */
	for( stringPos = 0, noStrings = 0;
		 stringPos < stringLen && !foundMatch && \
			noStrings < FAILSAFE_ITERATIONS_MED; 
		 stringPos += substringLen + 1, noStrings++ )
		{
		const ALGO_STRING_INFO *matchedAlgoInfo = NULL;
		const BYTE *substringPtr = ( BYTE * ) string + stringPos;
		const int substringMaxLen = stringLen - stringPos;
		BOOLEAN algoMatched = TRUE;
		int currentAlgoIndex;

		/* Find the length of the next algorithm name */
		for( substringLen = 0;
			 substringLen < substringMaxLen && \
				substringPtr[ substringLen ] != ',' && \
				substringLen < FAILSAFE_ITERATIONS_LARGE; \
			 substringLen++ );
		if( substringLen >= FAILSAFE_ITERATIONS_LARGE )
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

		/* Check whether it's something that we can handle */
		for( currentAlgoIndex = 0; 
			 currentAlgoIndex < algoIDInfo->noAlgoInfoEntries && \
				algoIDInfo->algoInfo[ currentAlgoIndex ].name != NULL && \
				currentAlgoIndex < FAILSAFE_ITERATIONS_MED;
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
		ENSURES( currentAlgoIndex < FAILSAFE_ITERATIONS_MED );
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

		/* Make sure that the required algorithms are available */
		if( !isPseudoAlgo( matchedAlgoInfo->algo ) && \
			!algoAvailable( matchedAlgoInfo->algo ) )
			algoMatched = FALSE;
		if( matchedAlgoInfo->subAlgo != CRYPT_ALGO_NONE && \
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
		}
	if( noStrings >= FAILSAFE_ITERATIONS_MED )
		{
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, errorInfo, 
				  "Excessive number (more than %d) of SSH algorithm "
				  "strings encountered", noStrings ) );
		}
	if( algoIndex > 50 )
		{
		char algoString[ 256 + 8 ];
		const int algoStringLen = min( stringLen, \
									   min( MAX_ERRMSG_SIZE - 80, 255 ) );

		REQUIRES( algoStringLen > 0 && \
				  algoStringLen <= min( MAX_ERRMSG_SIZE - 80, 255 ) );

		/* We couldn't find anything to use, tell the caller what was
		   available */
		memcpy( algoString, string, algoStringLen );
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, errorInfo, 
				  "No algorithm compatible with the remote system's "
				  "selection was found: '%s'", 
				  sanitiseString( algoString, 256, stringLen ) ) );
		}

	/* We found a more-preferred algorithm than the default, go with that */
	algoIDInfo->algo = algoIDInfo->algoInfo[ algoIndex ].algo;
	algoIDInfo->subAlgo = algoIDInfo->algoInfo[ algoIndex ].subAlgo;
	DEBUG_PRINT(( "Final accepted suite: %s.\n", 
				  algoIDInfo->algoInfo[ algoIndex ].name ));
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

	/* Clear return value */
	*algo = CRYPT_ALGO_NONE;

	setAlgoIDInfo( &algoIDInfo, algoInfo, noAlgoStringEntries, 
				   CRYPT_ALGO_NONE, useFirstMatch ? GETALGO_FIRST_MATCH : \
													GETALGO_BEST_MATCH );
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 7 ) ) \
static int readAlgoStringPair( INOUT STREAM *stream, 
							   IN_ARRAY( noAlgoStringEntries ) \
									const ALGO_STRING_INFO *algoInfo,
							   IN_RANGE( 1, 100 ) const int noAlgoStringEntries,
							   OUT_ALGO_Z CRYPT_ALGO_TYPE *algo, 
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

	/* Clear return value */
	*algo = CRYPT_ALGO_NONE;

	/* Get the first algorithm */
	setAlgoIDInfo( &algoIDInfo, algoInfo, noAlgoStringEntries, 
				   CRYPT_ALGO_NONE, isServer ? GETALGO_FIRST_MATCH : \
											   GETALGO_BEST_MATCH );
	status = readAlgoStringEx( stream, &algoIDInfo, errorInfo );
	if( cryptStatusError( status ) )
		return( status );
	pairPreferredAlgo = algoIDInfo.algo;

	/* Get the matched second algorithm.  Some buggy implementations request
	   mismatched algorithms (at the moment this is only for compression 
	   algorithms) but have no problems in accepting the same algorithm in 
	   both directions, so if we're talking to one of these then we ignore 
	   an algorithm mismatch */
	setAlgoIDInfo( &algoIDInfo, algoInfo, noAlgoStringEntries,
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

	return( status );
	}

/* Write a cryptlib algorithm ID as an SSH algorithm name */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoStringEx( INOUT STREAM *stream, 
					   IN_ALGO const CRYPT_ALGO_TYPE algo,
					   IN_ALGO_OPT const CRYPT_ALGO_TYPE subAlgo )
	{
	int algoIndex;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( algo > CRYPT_ALGO_NONE && algo < CRYPT_ALGO_LAST_EXTERNAL );
	REQUIRES( subAlgo >= CRYPT_ALGO_NONE && subAlgo < CRYPT_ALGO_LAST_EXTERNAL );

	/* Locate the name for this algorithm and optional sub-algoritihm and 
	   encode it as an SSH string */
	for( algoIndex = 0; 
		 algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
			algoStringMapTbl[ algoIndex ].algo != algo && \
			algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, ALGO_STRING_INFO ); 
		 algoIndex++ );
	ENSURES( algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
											 ALGO_STRING_INFO ) );
	ENSURES( algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE );

	/* If there's a sub-algorithm, find the entry for that */
	if( subAlgo != CRYPT_ALGO_NONE )
		{
		for( ; algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
			   algoStringMapTbl[ algoIndex ].algo == algo && \
			   algoStringMapTbl[ algoIndex ].subAlgo != subAlgo && \
			   algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
											   ALGO_STRING_INFO ); 
			 algoIndex++ );
		ENSURES( algoIndex < FAILSAFE_ARRAYSIZE( algoStringMapTbl, \
												 ALGO_STRING_INFO ) );
		ENSURES( algoStringMapTbl[ algoIndex ].algo != CRYPT_ALGO_NONE );
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

	return( writeAlgoStringEx( stream, algo, CRYPT_ALGO_NONE ) );
	}

/* Write a list of algorithms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeAlgoList( INOUT STREAM *stream, 
				   IN_ARRAY( noAlgoStringInfoEntries ) \
						const ALGO_STRING_INFO *algoStringInfoTbl,
				   IN_RANGE( 1, 10 ) const int noAlgoStringInfoEntries )
	{
	int availAlgoIndex[ 16 + 8 ];
	int noAlgos = 0, length = 0, algoIndex, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( algoStringInfoTbl, sizeof( ALGO_STRING_INFO ) * \
										  noAlgoStringInfoEntries ) );

	REQUIRES( noAlgoStringInfoEntries > 0 && noAlgoStringInfoEntries <= 10 );

	/* Walk down the list of algorithms remembering the encoded name of each
	   one that's available for use */
	for( algoIndex = 0; \
		 algoIndex < noAlgoStringInfoEntries && \
			algoStringInfoTbl[ algoIndex ].algo != CRYPT_ALGO_NONE && \
			algoIndex < FAILSAFE_ITERATIONS_SMALL;
		 algoIndex++ )
		{
		const ALGO_STRING_INFO *algoStringInfo = &algoStringInfoTbl[ algoIndex ];

		/* Make sure that this algorithm is available for use */
		if( !isPseudoAlgo( algoStringInfo->algo ) && \
			!algoAvailable( algoStringInfo->algo ) )
			continue;

		/* Make sure that any required sub-algorithms are available */
		if( algoStringInfo->subAlgo != CRYPT_ALGO_NONE && \
			!algoAvailable( algoStringInfo->subAlgo ) )
			continue;

		/* Remember the algorithm details */
		REQUIRES( noAlgos >= 0 && noAlgos < 16 );
		availAlgoIndex[ noAlgos++ ] = algoIndex;
		length += algoStringInfo->nameLen;
		if( noAlgos > 1 )
			length++;			/* Room for comma delimiter */
		}
	ENSURES( algoIndex < FAILSAFE_ITERATIONS_SMALL );

	/* Encode the list of available algorithms into a comma-separated string */
	status = writeUint32( stream, length );
	for( algoIndex = 0; cryptStatusOK( status ) && algoIndex < noAlgos && \
			algoIndex < FAILSAFE_ITERATIONS_MED; 
		 algoIndex++ )
		{
		const ALGO_STRING_INFO *algoStringInfo = \
				&algoStringInfoTbl[ availAlgoIndex[ algoIndex ] ];

		if( algoIndex > 0 )
			sputc( stream, ',' );	/* Add comma delimiter */
		status = swrite( stream, algoStringInfo->name,
						 algoStringInfo->nameLen );
		}
	ENSURES( algoIndex < FAILSAFE_ITERATIONS_MED );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoClassList( INOUT STREAM *stream, 
						IN_ENUM( SSH_ALGOCLASS ) \
							const SSH_ALGOCLASS_TYPE algoClass )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( algoClass > SSH_ALGOCLASS_NONE && \
			  algoClass < SSH_ALGOCLASS_LAST );

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
	STREAM stream;
	ALGOID_INFO algoIDInfo;
	BOOLEAN preferredAlgoMismatch = FALSE, guessedKeyex = FALSE;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( keyexLength, sizeof( int ) ) );

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

	/* Read the keyex algorithm information */
	if( isServer )
		{
		int pkcAlgo;

		setAlgoIDInfo( &algoIDInfo, algoStringKeyexTbl, 
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
		}
	else
		{
		setAlgoIDInfo( &algoIDInfo, algoStringKeyexTbl, 
					   FAILSAFE_ARRAYSIZE( algoStringKeyexTbl, \
										   ALGO_STRING_INFO ),
					   CRYPT_ALGO_NONE, GETALGO_BEST_MATCH );
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
		/* Switch to the appropriate hash algorithm */
		handshakeInfo->exchangeHashAlgo = algoIDInfo.subAlgo;

		/* If we're using ephemeral rather than static DH keys then we need 
		   to negotiate the keyex key before we can perform the exchange */
		handshakeInfo->requestedServerKeySize = SSH2_DEFAULT_KEYSIZE;
		}
	if( algoIDInfo.algo == CRYPT_ALGO_ECDH )
		{
		/* If we're using an ECDH cipher suite then we need to switch to the
		   appropriate hash algorithm for the keyex hashing */
		handshakeInfo->isECDH = TRUE;
		handshakeInfo->exchangeHashAlgo = algoIDInfo.subAlgo;
		}

	/* Read the pubkey (signature) algorithm information */
	if( isServer )
		{
		setAlgoIDInfo( &algoIDInfo, handshakeInfo->algoStringPubkeyTbl,
					   handshakeInfo->algoStringPubkeyTblNoEntries,
					   handshakeInfo->pubkeyAlgo, GETALGO_FIRST_MATCH_WARN );
		}
	else
		{
		setAlgoIDInfo( &algoIDInfo, handshakeInfo->algoStringPubkeyTbl,
					   handshakeInfo->algoStringPubkeyTblNoEntries,
					   CRYPT_ALGO_NONE, GETALGO_BEST_MATCH );
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
				  "Client requested pubkey algorithm %d when we "
				  "advertised %d", algoIDInfo.algo, 
				  handshakeInfo->pubkeyAlgo ) );
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
							&sessionInfoPtr->cryptAlgo, isServer, FALSE, 
							SESSION_ERRINFO );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoStringPair( &stream, algoStringMACTbl,
									 FAILSAFE_ARRAYSIZE( algoStringMACTbl, \
														 ALGO_STRING_INFO ),
									 &sessionInfoPtr->integrityAlgo,
									 isServer, FALSE, SESSION_ERRINFO );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* Read the remaining algorithm information.  The final reserved value 
	   should always be zero but we don't specifically check for this since 
	   at some point in the future it may become non-zero */
	status = readAlgoStringPair( &stream, algoStringCoprTbl, 
								 FAILSAFE_ARRAYSIZE( algoStringCoprTbl, \
													 ALGO_STRING_INFO ),
								 &dummyAlgo, isServer, 
								 ( sessionInfoPtr->protocolFlags & SSH_PFLAG_ASYMMCOPR ) ? \
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
	if( ( sessionInfoPtr->protocolFlags & SSH_PFLAG_ZEROLENIGNORE ) && \
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
	ENSURES( rangeCheckZ( sessionInfoPtr->receiveBufPos, localPayloadLength,
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
	long length;
	int extraLength, removedDataLength = ( ID_SIZE + PADLENGTH_SIZE );
	int partialPayloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

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
								   readInfo );
	if( cryptStatusError( status ) )
		{
		/* OK_SPECIAL means that we got a soft timeout before the entire 
		   header was read, so we return zero bytes read to tell the 
		   calling code that there's nothing more to do */
		return( ( status == OK_SPECIAL ) ? 0 : status );
		}
	ENSURES( length >= ID_SIZE + PADLENGTH_SIZE + SSH2_MIN_PADLENGTH_SIZE && \
			 length <= sessionInfoPtr->receiveBufSize - \
					   sessionInfoPtr->receiveBufPos );
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
		long payloadLength;

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
					  "Invalid data packet payload length %ld for "
					  "SSH_MSG_CHANNEL_DATA (94), should be %ld", 
					  cryptStatusError( status ) ? 0 : payloadLength,
					  length - ( removedDataLength + sshInfo->padLength ) ) );
			}

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
											/* To handle window adjusts */
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}

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
	REQUIRES( removedDataLength > 0 && \
			  removedDataLength < MAX_INTLENGTH_SHORT );
	partialPayloadLength = SSH_HEADER_REMAINDER_SIZE - removedDataLength;
	ENSURES( partialPayloadLength > 0 && \
			 removedDataLength + partialPayloadLength <= \
				sessionInfoPtr->receiveBufSize - sessionInfoPtr->receiveBufPos && \
			 removedDataLength + partialPayloadLength < MAX_BUFFER_SIZE );
	memmove( bufPtr, bufPtr + removedDataLength, partialPayloadLength );

	/* Determine how much data we'll be expecting, adjusted for the fixed
	   information that we've removed and the (implicitly present) MAC data */
	sessionInfoPtr->pendingPacketLength = \
			sessionInfoPtr->pendingPacketRemaining = \
					( length + extraLength ) - removedDataLength;
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

	REQUIRES( dataRemainingSize >= sessionInfoPtr->authBlocksize && \
			  dataRemainingSize <= sessionInfoPtr->receiveBufEnd - \
								   ( sessionInfoPtr->receiveBufPos + \
									 sshInfo->partialPacketDataLength ) );
	REQUIRES( dataLength >= 0 && dataLength < dataRemainingSize );

	/* All errors processing the payload are fatal */
	*readInfo = READINFO_FATAL;

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

		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_CTX_DECRYPT, dataRemainingPtr,
								  dataLength );
		if( cryptStatusError( status ) )
			return( status );
		status = checkMacSSHIncremental( sessionInfoPtr->iAuthInContext, 0,
										 dataRemainingPtr, dataRemainingSize, 
										 dataLength, 0, MAC_END, 
										 sessionInfoPtr->authBlocksize );
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

	/* Strip the padding and MAC and update the state information */
	payloadLength = sessionInfoPtr->pendingPacketLength - \
					( sshInfo->padLength + sessionInfoPtr->authBlocksize );
	sshInfo->readSeqNo++;
	ENSURES( payloadLength >= 0 && \
			 payloadLength < sessionInfoPtr->pendingPacketLength + dataLength );
			 /* Must be '<' rather than '<=' because of the stripped padding */
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

	REQUIRES( !( sessionInfoPtr->flags & SESSION_SENDCLOSED ) );
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
	status = writeUint32( &stream, dataLength );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	sMemConnect( &stream, sessionInfoPtr->sendBuffer,
				 sessionInfoPtr->sendBufSize );
	status = sSkip( &stream, SSH2_HEADER_SIZE + SSH2_PAYLOAD_HEADER_SIZE + \
							 dataLength, MAX_BUFFER_SIZE );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, FALSE, FALSE );
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

	/* If we haven't entered the secure state yet (i.e. we're still in the
	   middle of the handshake) then this is an abnormal termination, send 
	   a disconnect indication:

		byte		SSH_MSG_DISCONNECT
		uint32		reason_code = SSH_DISCONNECT_PROTOCOL_ERROR
		string		description = "Handshake failed"
		string		language_tag = "" */
	if( !( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE ) )
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
			status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, 
									 FALSE, TRUE );
		if( cryptStatusOK( status ) )
			{
			const int length = stell( &stream );
			void *dataPtr;

			/* Since there's nothing much that we can do at this point in 
			   response to an error except continue and close the network
			   session, we don't check for errors */
			status = sMemGetDataBlockAbs( &stream, 0, &dataPtr, length );
			if( cryptStatusOK( status ) )
				( void ) sendCloseNotification( sessionInfoPtr, dataPtr, 
												length );
			}
		sMemDisconnect( &stream );
		sNetDisconnect( &sessionInfoPtr->stream );
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
	sessionInfoPtr->readHeaderFunction = readHeaderFunction;
	sessionInfoPtr->processBodyFunction = processBodyFunction;
	sessionInfoPtr->preparePacketFunction = preparePacketFunction;
	sessionInfoPtr->shutdownFunction = shutdownFunction;
	}
#endif /* USE_SSH */
