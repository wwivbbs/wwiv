/****************************************************************************
*																			*
*						cryptlib SSHv2 Session Management					*
*						Copyright Peter Gutmann 1998-2008					*
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
   preferred algorithm order */

static const ALGO_STRING_INFO FAR_BSS algoStringKeyexTbl[] = {
#ifdef PREFER_ECC_SUITES
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH,
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },
#endif /* PREFER_ECC_SUITES */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_PSEUDOALGO_DHE_ALT,
	  CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_PSEUDOALGO_DHE,
	  CRYPT_ALGO_DH },
	{ "diffie-hellman-group1-sha1", 26, CRYPT_ALGO_DH },
#if !defined( PREFER_ECC_SUITES ) 
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH,
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },
#endif /* !PREFER_ECC_SUITES */
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringCoprTbl[] = {
	{ "none", 4, CRYPT_PSEUDOALGO_COPR },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringPubkeyTbl[] = {
#ifdef PREFER_ECC_SUITES
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA,
	  CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
#endif /* PREFER_ECC_SUITES */
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA },
	{ "ssh-dss", 7, CRYPT_ALGO_DSA },
#if !defined( PREFER_ECC_SUITES )
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA,
	  CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },
#endif /* !PREFER_ECC_SUITES */
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringEncrTbl[] = {
	{ "3des-cbc", 8, CRYPT_ALGO_3DES },
	{ "aes128-cbc", 10, CRYPT_ALGO_AES },
	{ "blowfish-cbc", 12, CRYPT_ALGO_BLOWFISH },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

static const ALGO_STRING_INFO FAR_BSS algoStringMACTbl[] = {
	{ "hmac-sha2-256", 13, CRYPT_ALGO_HMAC_SHA2 },
	{ "hmac-sha1", 9, CRYPT_ALGO_HMAC_SHA1 },
	{ "hmac-md5", 8, CRYPT_ALGO_HMAC_MD5 },
	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

/* A grand unified version of the above */

static const ALGO_STRING_INFO FAR_BSS algoStringMapTbl[] = {
	/* Signature algorithms */
	{ "ssh-rsa", 7, CRYPT_ALGO_RSA },
	{ "ssh-dss", 7, CRYPT_ALGO_DSA },
	{ "ecdsa-sha2-nistp256", 19, CRYPT_ALGO_ECDSA,
	  CRYPT_ALGO_ECDH, CRYPT_ALGO_SHA2 },

	/* Encryption algorithms */
	{ "3des-cbc", 8, CRYPT_ALGO_3DES },
	{ "aes128-cbc", 10, CRYPT_ALGO_AES },
	{ "blowfish-cbc", 12, CRYPT_ALGO_BLOWFISH },

	/* Keyex algorithms */
	{ "diffie-hellman-group-exchange-sha256", 36, CRYPT_PSEUDOALGO_DHE_ALT, 
	  CRYPT_ALGO_DH, CRYPT_ALGO_SHA2 },
	{ "diffie-hellman-group-exchange-sha1", 34, CRYPT_PSEUDOALGO_DHE, 
	  CRYPT_ALGO_DH },
	{ "diffie-hellman-group1-sha1", 26, CRYPT_ALGO_DH },
	{ "ecdh-sha2-nistp256", 18, CRYPT_ALGO_ECDH,
	  CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2 },

	/* MAC algorithms */
	{ "hmac-sha2-256", 13, CRYPT_ALGO_HMAC_SHA2 },
	{ "hmac-sha1", 9, CRYPT_ALGO_HMAC_SHA1 },
	{ "hmac-md5", 8, CRYPT_ALGO_HMAC_MD5 },
	{ "password", 8, CRYPT_PSEUDOALGO_PASSWORD },

	/* Miscellaneous */
	{ "none", 4, CRYPT_PSEUDOALGO_COPR },
	{ "none", 4, CRYPT_ALGO_LAST },	/* Catch-all */

	{ NULL, 0, CRYPT_ALGO_NONE }, { NULL, 0, CRYPT_ALGO_NONE }
	};

CHECK_RETVAL \
int getAlgoStringInfo( OUT const ALGO_STRING_INFO **algoStringInfoPtrPtr,
					   OUT_INT_Z int *noInfoEntries )
	{
	assert( isReadPtr( algoStringInfoPtrPtr, \
					   sizeof( ALGO_STRING_INFO * ) ) );
	assert( isWritePtr( noInfoEntries, sizeof( int ) ) );

	*algoStringInfoPtrPtr = algoStringMapTbl;
	*noInfoEntries = FAILSAFE_ARRAYSIZE( algoStringMapTbl, ALGO_STRING_INFO );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Convert an SSH algorithm list to a cryptlib ID in preferred-algorithm
   order.  For some bizarre reason the algorithm information is communicated
   as a comma-delimited list (in an otherwise binary protocol) so we have to 
   unpack and pack them into this cumbersome format alongside just choosing 
   which algorithm to use.  In addition, the algorithm selection mechanism 
   differs depending on whether we're the client or server, and what set of 
   algorithms we're matching.  Unlike SSL, which uses the offered-suites/
   chosen-suites mechanism, in SSH both sides offer a selection of cipher 
   suites and the server chooses the first one that appears on both it and 
   the client's list, with special-case handling for the keyex and signature 
   algorithms if the match isn't the first one on the list.  This means that 
   the client can choose as it pleases from the server's list if it waits 
   for the server hello (see the comment in the client/server hello handling 
   code on the annoying nature of this portion of the SSH handshake) but the 
   server has to perform a complex double-match of its own vs.the client's 
   list.  The cases that we need to handle are:

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
	BOOLEAN prefAlgoMismatch;		/* First match != preferredAlgo */
	} ALGOID_INFO;

#define setAlgoIDInfo( algoIDInfo, algoStrInfo, algoStrInfoEntries, prefAlgo, getType ) \
	{ \
	memset( ( algoIDInfo ), 0, sizeof( ALGOID_INFO ) ); \
	( algoIDInfo )->algoInfo = ( algoStrInfo ); \
	( algoIDInfo )->noAlgoInfoEntries = ( algoStrInfoEntries ); \
	( algoIDInfo )->preferredAlgo = ( prefAlgo ); \
	( algoIDInfo )->getAlgoType = ( getType ); \
	( algoIDInfo )->allowECC = TRUE; \
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readAlgoStringEx( INOUT STREAM *stream, 
							 INOUT ALGOID_INFO *algoIDInfo,
							 INOUT ERROR_INFO *errorInfo )
	{
	BOOLEAN foundMatch = FALSE;
	void *string = DUMMY_INIT_PTR;
	int stringPos, stringLen, substringLen, algoIndex = 999;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( algoIDInfo, sizeof( ALGOID_INFO ) ) );
	assert( isReadPtr( algoIDInfo->algoInfo, \
					   sizeof( ALGO_STRING_INFO ) * \
							algoIDInfo->noAlgoInfoEntries ) );
	
	ENSURES( ( algoIDInfo->getAlgoType == GETALGO_BEST_MATCH && \
			   algoIDInfo->preferredAlgo == CRYPT_ALGO_NONE ) || \
			 ( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH ) ||
			 ( algoIDInfo->getAlgoType == GETALGO_FIRST_MATCH_WARN && \
			   ( algoIDInfo->preferredAlgo > CRYPT_ALGO_NONE && \
				 algoIDInfo->preferredAlgo < CRYPT_ALGO_LAST_EXTERNAL ) ) );
			 /* FIRST_MATCH uses CRYPT_ALGO_NONE on the first match of an
			    algorithm pair and the first algorithm chosen on the second
				match */

	/* Get the string length and data and make sure that it's valid */
	status = stringLen = readUint32( stream );
	if( !cryptStatusError( status ) && stringLen < SSH2_MIN_ALGOID_SIZE )
		status = CRYPT_ERROR_BADDATA;	/* Quick-rej.for too-short strings */
	if( !cryptStatusError( status ) )
		status = sMemGetDataBlock( stream, &string, stringLen );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, stringLen );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid algorithm ID string" ) );
		}

	/* Walk down the string looking for a recognised algorithm.  Since our
	   preference may not match the other side's preferences we have to walk
	   down the entire list to find our preferred choice:

		  stringPos			stringLen
			   |			   |
			   v			   v
		"algo1,algo2,algo3,algoN"
				   ^
				   |
				substrLen */
	for( stringPos = 0, iterationCount = 0;
		 stringPos < stringLen && !foundMatch && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 stringPos += substringLen + 1, iterationCount++ )
		{
		const ALGO_STRING_INFO *matchedAlgoInfo = NULL;
		const BYTE *stringPtr = string;
		BOOLEAN algoMatched = TRUE;
		int currentAlgoIndex;

		/* Find the length of the next algorithm name */
		for( substringLen = stringPos; \
			 substringLen < stringLen && stringPtr[ substringLen ] != ','; \
			 substringLen++ );
		substringLen -= stringPos;
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
			if( substringLen == algoIDInfo->algoInfo[ currentAlgoIndex ].nameLen && \
				!memcmp( algoIDInfo->algoInfo[ currentAlgoIndex ].name, 
						 stringPtr + stringPos, substringLen ) )
				{
				matchedAlgoInfo = &algoIDInfo->algoInfo[ currentAlgoIndex ];
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

		/* If it's a cipher suite, make sure that the algorithms that it's 
		   made up of are available */
		if( isPseudoAlgo( matchedAlgoInfo->algo ) )
			{
			if( matchedAlgoInfo->checkCryptAlgo != CRYPT_ALGO_NONE && \
				!algoAvailable( matchedAlgoInfo->checkCryptAlgo ) )
				algoMatched = FALSE;
			if( matchedAlgoInfo->checkHashAlgo != CRYPT_ALGO_NONE && \
				!algoAvailable( matchedAlgoInfo->checkHashAlgo ) )
				algoMatched = FALSE;
			}
		else
			{
			/* It's a straight algorithm, make sure that it's available */
			if( !algoAvailable( matchedAlgoInfo->algo ) )
				algoMatched = FALSE;
			}

		/* If this is an ECC algorithm and the use of ECC algorithms has 
		   been prevented by external conditions such as the server key
		   not being an ECC key, we can't use it even if ECC algorithms in
		   general are available */
		if( ( isEccAlgo( matchedAlgoInfo->algo ) || \
			  ( matchedAlgoInfo->checkCryptAlgo != CRYPT_ALGO_NONE && \
				isEccAlgo( matchedAlgoInfo->checkCryptAlgo ) ) ) && \
			!algoIDInfo->allowECC )
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
					}
				algoIndex = currentAlgoIndex;
				foundMatch = TRUE;
				break;

			default:
				retIntError();
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( algoIndex > 50 )
		{
		char algoString[ 256 + 8 ];
		const int algoStringLen = min( stringLen, \
									   min( MAX_ERRMSG_SIZE - 80, 255 ) );

		/* We couldn't find anything to use, tell the caller what was
		   available */
		if( algoStringLen > 0 )
			memcpy( algoString, string, algoStringLen );
		retExt( CRYPT_ERROR_NOTAVAIL,
				( CRYPT_ERROR_NOTAVAIL, errorInfo, 
				  "No algorithm compatible with the remote system's "
				  "selection was found: '%s'", 
				  sanitiseString( algoString, 256, stringLen ) ) );
		}

	/* We found a more-preferred algorithm than the default, go with that */
	algoIDInfo->algo = algoIDInfo->algoInfo[ algoIndex ].algo;
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

/* Convert a cryptlib algorithm ID to an SSH algorithm name */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoString( INOUT STREAM *stream, 
					 IN_ALGO const CRYPT_ALGO_TYPE algo )
	{
	int i;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( algo >= CRYPT_ALGO_NONE && algo < CRYPT_ALGO_LAST_EXTERNAL );

	/* Locate the name for this algorithm and encode it as an SSH string */
	for( i = 0; algoStringMapTbl[ i ].algo != CRYPT_ALGO_LAST && \
				algoStringMapTbl[ i ].algo != algo && \
				i < FAILSAFE_ARRAYSIZE( algoStringMapTbl, ALGO_STRING_INFO ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( algoStringMapTbl, ALGO_STRING_INFO ) );
	ENSURES( algoStringMapTbl[ i ].algo != CRYPT_ALGO_LAST );
	return( writeString32( stream, algoStringMapTbl[ i ].name, 
						   algoStringMapTbl[ i ].nameLen ) );
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

	   The cookie isn't explicitly processed as with SSHv1 since SSHv2
	   hashes the entire hello message */
	status = length = \
		readHSPacketSSH2( sessionInfoPtr, SSH_MSG_KEXINIT, 128 );
	if( cryptStatusError( status ) )
		return( status );
	*keyexLength = length;
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	sSkip( &stream, SSH2_COOKIE_SIZE );

	/* Read the keyex algorithm information */
	if( isServer )
		{
		int pkcAlgo;

		setAlgoIDInfo( &algoIDInfo, algoStringKeyexTbl, 
					   FAILSAFE_ARRAYSIZE( algoStringKeyexTbl, \
										   ALGO_STRING_INFO ),
					   CRYPT_PSEUDOALGO_DHE, GETALGO_FIRST_MATCH_WARN );

		/* By default the use of ECC algorithms is enabled is support for
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
	if( algoIDInfo.algo == CRYPT_PSEUDOALGO_DHE || \
		algoIDInfo.algo == CRYPT_PSEUDOALGO_DHE_ALT )
		{
		/* If we're using the non-default exchange hash mechanism, switch to
		   the alternative algorithm */
		if( algoIDInfo.algo == CRYPT_PSEUDOALGO_DHE_ALT )
			handshakeInfo->exchangeHashAlgo = CRYPT_ALGO_SHA2;

		/* If we're using ephemeral rather than static DH keys we need to
		   negotiate the keyex key before we can perform the exchange */
		handshakeInfo->requestedServerKeySize = SSH2_DEFAULT_KEYSIZE;
		}
	if( algoIDInfo.algo == CRYPT_ALGO_ECDH || \
		algoIDInfo.algo == CRYPT_PSEUDOALGO_ECDH_P384 || \
		algoIDInfo.algo == CRYPT_PSEUDOALGO_ECDH_P521 )
		{
		/* If we're using an ECDH cipher suite we need to use SHA2 for the 
		   keyex hashing */
		handshakeInfo->isECDH = TRUE;
		handshakeInfo->exchangeHashAlgo = CRYPT_ALGO_SHA2;
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
	if( !isServer )
		handshakeInfo->pubkeyAlgo = algoIDInfo.algo;
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
	status = readUniversal32( &stream );
	if( cryptStatusOK( status ) )
		status = readUniversal32( &stream );	/* Language string pair */
	if( cryptStatusOK( status ) )
		{
		if( sgetc( &stream ) )
			guessedKeyex = TRUE;
		status = readUint32( &stream );			/* Reserved value */
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processControlMessage( INOUT SESSION_INFO *sessionInfoPtr,
								  INOUT READSTATE_INFO *readInfo,
								  IN_LENGTH_Z const int payloadLength )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	STREAM stream;
	int localPayloadLength = payloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( readInfo, sizeof( READSTATE_INFO ) ) );

	REQUIRES( payloadLength >= 0 && payloadLength < MAX_INTLENGTH );

	/* Putty 0.59 erroneously sent zero-length SSH_MSG_IGNORE packets, if 
	   we find one of these we convert it into a valid packet.  Writing into 
	   the buffer at this position is safe because we've got padding and at 
	   least sessionInfoPtr->authBlocksize bytes of MAC following the 
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
				  "packet type %d", localPayloadLength, 
				  sshInfo->packetType ) );
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
							   INOUT READSTATE_INFO *readInfo )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	long length;
	int extraLength, removedDataLength = ( ID_SIZE + PADLENGTH_SIZE );
	int status;

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
	   the buffer */
	if( sshInfo->packetType == SSH_MSG_CHANNEL_DATA )
		{
		STREAM stream;
		long payloadLength;

		/* Skip the type, padding length, and channel number and make sure 
		   that the payload length matches the packet length and, if 
		   everything's OK, process the channel data header (this is
		   required in order to handle window size updates) */
		sMemConnect( &stream, bufPtr, SSH2_HEADER_REMAINDER_SIZE );
		sSkip( &stream, ID_SIZE + PADLENGTH_SIZE + UINT32_SIZE );
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
					  "Invalid data packet payload length %ld, should be "
					  "%ld", cryptStatusError( status ) ? 0 : payloadLength,
					  length - ( removedDataLength + sshInfo->padLength ) ) );
			}
		sseek( &stream, ID_SIZE + PADLENGTH_SIZE );
		status = processChannelControlMessage( sessionInfoPtr, &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Move the remainder down to the start of the buffer.  The general idea
	   is to remove all of the header data so that only the payload remains
	   in the buffer, avoiding the need to move it down afterwards.  This is
	   complicated by the fact that (unlike SSL) all of the data (including
	   the header) is encrypted and MAC'd so we can't just read that 
	   separately but have to process it as part of the payload, remove it,
	   and remember anything that's left for later */
	REQUIRES( SSH2_HEADER_REMAINDER_SIZE - removedDataLength > 0 );
	memmove( bufPtr, bufPtr + removedDataLength,
			 SSH2_HEADER_REMAINDER_SIZE - removedDataLength );

	/* Determine how much data we'll be expecting, adjusted for the fixed
	   information that we've removed and the (implicitly present) MAC data */
	sessionInfoPtr->pendingPacketLength = \
			sessionInfoPtr->pendingPacketRemaining = \
					( length + extraLength ) - removedDataLength;
	sshInfo->partialPacketDataLength = SSH2_HEADER_REMAINDER_SIZE - \
									   removedDataLength;

	/* Indicate that we got some payload as part of the header */
	*readInfo = READINFO_HEADERPAYLOAD;
	return( SSH2_HEADER_REMAINDER_SIZE - removedDataLength );
	}

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processBodyFunction( INOUT SESSION_INFO *sessionInfoPtr,
								INOUT READSTATE_INFO *readInfo )
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

			Processed in header read						+--+
		 rBufPos  |											|  | Processed
			|<----v----- pendingPacketLength ---------->|	+--+
			v<- pPPL -->|								|	+--+
		----+-----------+-----------------------+-------+--	|//| Encrypted
			|			|///////////////////////|\\\\\\\|	+--+
		----+-----------+-----------------------+-------+--	+--+
						|<---- dataLength ----->|		|	|\\| MAC
						|<------- dataRemaining ------->|	+--+ */
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
				  "Bad message MAC for packet type %d, length %d",
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
	DEBUG_PRINT(( "Read packet type %d, length %d.\n", 
				  sshInfo->packetType, payloadLength ));
	DEBUG_DUMP_DATA( sessionInfoPtr->receiveBuffer + \
					 sessionInfoPtr->receiveBufPos, payloadLength );

	/* If it's not plain data (which was handled at the readHeaderFunction()
	   stage), handle it as a control message */
	if( sshInfo->packetType != SSH_MSG_CHANNEL_DATA )
		{
		status = processControlMessage( sessionInfoPtr, readInfo, 
										payloadLength );
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
	int length = DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( !( sessionInfoPtr->flags & SESSION_SENDCLOSED ) );
	REQUIRES( dataLength > 0 && dataLength < sessionInfoPtr->sendBufPos && \
			  dataLength < MAX_INTLENGTH );

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
							 dataLength );
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, FALSE, FALSE );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If there's control data enqueued to be written, try and append it to
	   the existing data to be sent.  This may or may not append it 
	   (depending on whether there's room in the send buffer) so we may end
	   up here more than once */
	if( sshInfo->response.type > 0 )
		{
		int length2;

		length2 = appendChannelData( sessionInfoPtr, length );
		if( !cryptStatusError( length2 ) )
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
	   middle of the handshake) this is an abnormal termination, send a
	   disconnect indication:

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
		if( cryptStatusOK( status ) )
			{
			writeUint32( &stream, SSH_DISCONNECT_PROTOCOL_ERROR );
			writeString32( &stream, "Handshake failed", 16 );
			status = writeUint32( &stream, 0 );	/* No language tag */
			}
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

/* Set up access to the SSH session processing.  This function can be called
   twice, initially with handshakeInfo == NULL to set the default SSH 
   session processing to SSHv2 and a second time once the SSH handshake is 
   in progress to initialise the handshake information (if SSHv1 is detected 
   in the peer and enabled then the second call is to initSSH1processing() 
   instead) */

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSH2processing( INOUT SESSION_INFO *sessionInfoPtr,
						 INOUT_OPT SSH_HANDSHAKE_INFO *handshakeInfo,
						 const BOOLEAN isServer )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		FALSE,						/* Request-response protocol */
		SESSION_NONE,				/* Flags */
		SSH_PORT,					/* SSH port */
		SESSION_NEEDS_USERID |		/* Client attributes */
			SESSION_NEEDS_PASSWORD | \
			SESSION_NEEDS_KEYORPASSWORD | \
			SESSION_NEEDS_PRIVKEYSIGN,
				/* The client private key is optional, but if present it has
				   to be signature-capable */
		SESSION_NEEDS_PRIVATEKEY |	/* Server attributes */
			SESSION_NEEDS_PRIVKEYSIGN,
#ifdef USE_SSH1
		2, 1, 2,					/* Version 2 */
#else
		2, 2, 2,					/* Version 2 */
#endif /* USE_SSH1 */

		/* Protocol-specific information */
		EXTRA_PACKET_SIZE + \
			DEFAULT_PACKET_SIZE,	/* Send/receive buffer size */
		SSH2_HEADER_SIZE + \
			SSH2_PAYLOAD_HEADER_SIZE,/* Payload data start */
		DEFAULT_PACKET_SIZE			/* (Default) maximum packet size */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( ( handshakeInfo == NULL ) || \
			isWritePtr( handshakeInfo, sizeof( SSH_HANDSHAKE_INFO ) ) );

	sessionInfoPtr->protocolInfo = &protocolInfo;
	sessionInfoPtr->readHeaderFunction = readHeaderFunction;
	sessionInfoPtr->processBodyFunction = processBodyFunction;
	sessionInfoPtr->preparePacketFunction = preparePacketFunction;
	sessionInfoPtr->shutdownFunction = shutdownFunction;
	if( handshakeInfo != NULL )
		{
		if( isServer )
			initSSH2serverProcessing( sessionInfoPtr, handshakeInfo );
		else
			initSSH2clientProcessing( sessionInfoPtr, handshakeInfo );

		handshakeInfo->algoStringPubkeyTbl = algoStringPubkeyTbl;
		handshakeInfo->algoStringPubkeyTblNoEntries = \
				FAILSAFE_ARRAYSIZE( algoStringPubkeyTbl, ALGO_STRING_INFO );
		}
	}
#endif /* USE_SSH */
