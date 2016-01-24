/****************************************************************************
*																			*
*					cryptlib Randomness Management Routines					*
*						Copyright Peter Gutmann 1995-2007					*
*																			*
****************************************************************************/

/* The random pool handling code in this module and the other modules in the
   /random subdirectory represent the cryptlib continuously seeded
   pseudorandom number generator (CSPRNG) as described in my 1998 Usenix
   Security Symposium paper "The generation of practically strong random
   numbers".

   The CSPRNG code is copyright Peter Gutmann (and various others) 1995-2004
   all rights reserved.  Redistribution of the CSPRNG modules and use in
   source and binary forms, with or without modification, are permitted
   provided that the following BSD-style license conditions are met:

   1. Redistributions of source code must retain the above copyright notice
	  and this permission notice in its entirety.

   2. Redistributions in binary form must reproduce the copyright notice in
	  the documentation and/or other materials provided with the distribution.

   3. A copy of any bugfixes or enhancements made must be provided to the
	  author, <pgut001@cs.auckland.ac.nz> to allow them to be added to the
	  baseline version of the code.

   ALTERNATIVELY, the code may be distributed under the terms of the GNU
   General Public License, version 2 or any later version published by the
   Free Software Foundation, in which case the provisions of the GNU GPL are
   required INSTEAD OF the above restrictions.

   ALTERNATIVELY ALTERNATIVELY, the code may be distributed under the terms 
   of the GNU Library/Lesser General Public License, version 2 or any later 
   version published by the Free Software Foundation, in which case the 
   provisions of the GNU LGPL are required INSTEAD OF the above restrictions.

   Although not required under the terms of the GPL or LGPL, it would still 
   be nice if you could make any changes available to the author to allow a
   consistent code base to be maintained */

#if defined( INC_ALL )
  #include "crypt.h"
  #ifdef CONFIG_RANDSEED
	#include "stream.h"
  #endif /* CONFIG_RANDSEED */
  #include "random_int.h"
#else
  #include "crypt.h"
  #ifdef CONFIG_RANDSEED
	#include "io/stream.h"
  #endif /* CONFIG_RANDSEED */
  #include "random/random_int.h"
#endif /* Compiler-specific includes */

/* If we don't have a defined randomness interface, complain */

#if !( defined( __BEOS__ ) || defined( __ECOS__ ) || \
	   defined( __IBM4758__ ) || defined( __MAC__ ) || \
	   defined( __MSDOS__ ) || defined( __MVS__ ) || \
	   defined( __Nucleus__ ) || defined( __OS2__ ) || \
	   defined( __PALMOS__ ) || defined( __TANDEM_NSK__ ) || \
	   defined( __TANDEM_OSS__ ) || defined( __UNIX__ ) || \
	   defined( __VMCMS__ ) || defined( __WIN16__ ) || \
	   defined( __WIN32__ ) || defined( __WINCE__ ) || \
	   defined( __XMK__ ) )
  #error You need to create OS-specific randomness-gathering functions in random/<os-name>.c
#endif /* Various OS-specific defines */

/* If we're using stored seed data, make sure that the seed quality setting
   is in order */

#ifdef CONFIG_RANDSEED
  #ifndef CONFIG_RANDSEED_QUALITY
	/* If the user hasn't provided a quality estimate, default to 80 */
	#define CONFIG_RANDSEED_QUALITY		80
  #endif /* !CONFIG_RANDSEED_QUALITY */
  #if ( CONFIG_RANDSEED_QUALITY < 10 ) || ( CONFIG_RANDSEED_QUALITY > 100 )
	#error CONFIG_RANDSEED_QUALITY must be between 10 and 100
  #endif /* CONFIG_RANDSEED_QUALITY check */

  /* Forward declaration for the add-data function */
  STDC_NONNULL_ARG( ( 1 ) ) \
  static void addStoredSeedData( INOUT RANDOM_INFO *randomInfo );
#endif /* CONFIG_RANDSEED */

/* A custom form of REQUIRES()/ENSURES() that takes care of unlocking the
   randomness mutex on the way out */

#define REQUIRES_MUTEX( x )	\
		if( !( x ) ) { krnlExitMutex( MUTEX_RANDOM ); retIntError(); }
#define ENSURES_MUTEX	REQUIRES_MUTEX

/****************************************************************************
*																			*
*						Randomness Interface Definitions					*
*																			*
****************************************************************************/

/* In order to avoid the pool startup problem (where initial pool data may
   consist of minimally-mixed entropy samples) we require that the pool be
   mixed at least the following number of times before we can draw data from
   it.  This usually happens automatically because a slow poll adds enough
   data to cause many mixing iterations, however if this doesn't happen we
   manually mix it the appropriate number of times to get it up to the
   correct level */

#define RANDOMPOOL_MIXES		10

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check the randomness state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const RANDOM_INFO *randomInfo )
	{
	assert( isReadPtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	/* Make sure that the pool index information is within bounds.  The pool
	   index can briefly become the same as RANDOMPOOL_SIZE if we've filled
	   the pool and we've about to mix it */
	if( randomInfo->randomPoolPos < 0 || \
		randomInfo->randomPoolPos > RANDOMPOOL_SIZE )
		return( FALSE );

	/* Make sure that the pool accounting information is within bounds */
	if( randomInfo->randomQuality < 0 || randomInfo->randomQuality > 100 )
		return( FALSE );
	if( randomInfo->randomPoolMixes < 0 || \
		randomInfo->randomPoolMixes > RANDOMPOOL_MIXES )
		return( FALSE );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Random Pool Management Routines						*
*																			*
****************************************************************************/

/* Initialise and shut down the random pool */

STDC_NONNULL_ARG( ( 1 ) ) \
void initRandomPool( INOUT RANDOM_INFO *randomInfo )
	{
	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	memset( randomInfo, 0, sizeof( RANDOM_INFO ) );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endRandomPool( INOUT RANDOM_INFO *randomInfo )
	{
	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	zeroise( randomInfo, sizeof( RANDOM_INFO ) );
	}

/* Stir up the data in the random pool.  Given a circular buffer of length n
   bytes, a buffer position p, and a hash output size of h bytes, we hash
   bytes from p - h...p - 1 (to provide chaining across previous hashes) and
   p...p + 64 (to have as much surrounding data as possible affect the
   current data).  Then we move on to the next h bytes until all n bytes have
   been mixed.  See "Cryptographic Security Architecture Design and 
   Implementation" for the full details of the PRNG design */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int mixRandomPool( INOUT RANDOM_INFO *randomInfo )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE dataBuffer[ CRYPT_MAX_HASHSIZE + 64 + 8 ];
	int hashSize, hashIndex;
	ORIGINAL_INT_VAR( randomPoolMixes, randomInfo->randomPoolMixes );

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	REQUIRES( sanityCheck( randomInfo ) );

	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
							 &hashSize );

	/* Stir up the entire pool.  We can't check the return value of the
	   hashing call because there isn't one, however the hashing code has 
	   gone through a self-test when the randomness subsystem was 
	   initialised */
	for( hashIndex = 0; hashIndex < RANDOMPOOL_SIZE; hashIndex += hashSize )
		{
		int dataBufIndex, poolIndex;

		/* Precondition: We're processing hashSize bytes at a time */
		REQUIRES( hashIndex % hashSize == 0 );

		/* If we're at the start of the pool then the first block that we hash
		   is at the end of the pool, otherwise it's the block immediately
		   preceding the current one */
		poolIndex = ( hashIndex >= hashSize ) ? \
					hashIndex - hashSize : RANDOMPOOL_SIZE - hashSize;
		ENSURES( poolIndex >= 0 && poolIndex <= RANDOMPOOL_SIZE - hashSize );

		/* Copy hashSize bytes from position p - hashSize... p + 64 in the
		   circular pool into the hash data buffer:

				poolIndex
					| hashIndex
					v	v
			--------+---+-----------------------+---------
					| 20|			64			|			Pool
			--------+---+-----------------------+---------
					|							|
					+---------------------------+
					|							|			Buffer
					+---------------------------+
					 \							/
					  \	Hash  ------------------	
					   \	 /
			------------+---+-----------------------------
						| 20|								Pool'
			------------+---+----------------------------- 
					^	^						^
					|	|						|
				   p-h	p					  p+64 */
		for( dataBufIndex = 0; dataBufIndex < hashSize + 64; dataBufIndex++ )
			{
			dataBuffer[ dataBufIndex ] = randomInfo->randomPool[ poolIndex ];
			poolIndex = ( poolIndex + 1 ) % RANDOMPOOL_SIZE;
			}

		/* Postconditions for the state data copy: We got hashSize + 64 bytes 
		   surrounding the current pool position */
		ENSURES( dataBufIndex == hashSize + 64 );

		/* Hash the data in the circular pool, depositing the result at position 
		   p...p + hashSize */
		hashFunctionAtomic( randomInfo->randomPool + hashIndex,
							RANDOMPOOL_ALLOCSIZE - hashIndex, 
							dataBuffer, dataBufIndex );
		}
	zeroise( dataBuffer, CRYPT_MAX_HASHSIZE + 64 );

	/* Postconditions for the pool mixing: The entire pool was mixed and
	   temporary storage was cleared */
	ENSURES( hashIndex >= RANDOMPOOL_SIZE );
	FORALL( i, 0, CRYPT_MAX_HASHSIZE + 64,
			dataBuffer[ i ] == 0 );

	/* Increment the mix count and move the write position back to the start
	   of the pool */
	if( randomInfo->randomPoolMixes < RANDOMPOOL_MIXES )
		randomInfo->randomPoolMixes++;
	randomInfo->randomPoolPos = 0;

	/* Postconditions for the status update: We mixed the pool at least
	   once, and we're back at the start of the pool */
	ENSURES( randomInfo->randomPoolMixes == RANDOMPOOL_MIXES || \
			 randomInfo->randomPoolMixes == \
							ORIGINAL_VALUE( randomPoolMixes ) + 1 );
	ENSURES( randomInfo->randomPoolPos == 0 );

	ENSURES( sanityCheck( randomInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Get Random Data								*
*																			*
****************************************************************************/

/* Get a block of random data from the randomness pool in such a way that
   compromise of the data doesn't compromise the pool and vice versa.  This 
   is done by performing the (one-way) pool mixing operation on the pool and
   on a transformed version of the pool that becomes the key.  This 
   corresponds to the Barak-Halevi construction:

	r || s' = next( s );
	s' = refresh( s ^ new_state );

   where 'r' is the generator output and 's' is the generator state (the RNG
   design here predates Barak-Halevi by about 10 years but the principle is
   the same).
   
   The transformed version of the pool from which the key data will be drawn 
   is then further processed by running each 64-bit block through the X9.17
   generator.  As an additional precaution the key data is folded in half to
   ensure that not even a hashed or encrypted form of the previous contents
   is available.  No pool data ever leaves the pool.

   This function performs a more paranoid version of the FIPS 140 continuous
   tests on both the main pool contents and the X9.17 generator output to
   detect stuck-at faults and short cycles in the output.  In addition the
   higher-level message handler applies FIPS 140-like statistical tests to
   the output and will retry the fetch if the output fails the tests.  This
   additional step is performed at a higher level because it's then applied
   to all randomness sources used by cryptlib and not just the built-in one.

   Because the pool output is folded to mask the PRNG output, the output from 
   each round of mixing is only half the pool size, as defined below */

#define RANDOM_OUTPUTSIZE	( RANDOMPOOL_SIZE / 2 )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int tryGetRandomOutput( INOUT RANDOM_INFO *randomInfo,
							   INOUT RANDOM_INFO *exportedRandomInfo )
	{
	const BYTE *samplePtr = randomInfo->randomPool;
	const BYTE *x917SamplePtr = exportedRandomInfo->randomPool;
	unsigned long sample;
	int i, status;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isWritePtr( exportedRandomInfo, sizeof( RANDOM_INFO ) ) );

	/* Precondition: The pool is ready to go.  This check isn't so much to
	   confirm that this really is the case (it's already been checked
	   elsewhere) but to ensure that the two pool parameters haven't been
	   reversed.  The use of generic pools for all types of random output is
	   useful in terms of providing a nice abstraction, but less useful for
	   type safety */
	REQUIRES( sanityCheck( randomInfo ) );
	REQUIRES( randomInfo->randomQuality >= 100 && \
			  randomInfo->randomPoolMixes >= RANDOMPOOL_MIXES && \
			  randomInfo->x917Inited == TRUE );
	REQUIRES( exportedRandomInfo->randomQuality == 0 && \
			  exportedRandomInfo->randomPoolMixes == 0 && \
			  exportedRandomInfo->x917Inited == FALSE );

	/* Copy the contents of the main pool across to the export pool,
	   transforming it as we go by flipping all of the bits */
	for( i = 0; i < RANDOMPOOL_ALLOCSIZE; i++ )
		exportedRandomInfo->randomPool[ i ] = randomInfo->randomPool[ i ] ^ 0xFF;

	/* Postcondition for the bit-flipping: The two pools differ, and the
	   difference is in the flipped bits */
	ENSURES( memcmp( randomInfo->randomPool, exportedRandomInfo->randomPool,
					 RANDOMPOOL_ALLOCSIZE ) );
	FORALL( i, 0, RANDOMPOOL_ALLOCSIZE, \
			randomInfo->randomPool[ i ] == \
							( exportedRandomInfo->randomPool[ i ] ^ 0xFF ) );

	/* Mix the original and export pools so that neither can be recovered
	   from the other */
	status = mixRandomPool( randomInfo );
	if( cryptStatusOK( status ) )
		status = mixRandomPool( exportedRandomInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Postcondition for the mixing: The two pools differ, and the difference
	   is more than just the bit flipping (this has a ~1e-14 chance of a false
	   positive, which should be safe) */
	ENSURES( memcmp( randomInfo->randomPool, exportedRandomInfo->randomPool,
					 RANDOMPOOL_ALLOCSIZE ) );
	ENSURES( randomInfo->randomPool[ 0 ] != \
					( exportedRandomInfo->randomPool[ 0 ] ^ 0xFF ) ||
			 randomInfo->randomPool[ 8 ] != \
					( exportedRandomInfo->randomPool[ 8 ] ^ 0xFF ) ||
			 randomInfo->randomPool[ 16 ] != \
					( exportedRandomInfo->randomPool[ 16 ] ^ 0xFF ) ||
			 randomInfo->randomPool[ 24 ] != \
					( exportedRandomInfo->randomPool[ 24 ] ^ 0xFF ) ||
			 randomInfo->randomPool[ 32 ] != \
					( exportedRandomInfo->randomPool[ 32 ] ^ 0xFF ) ||
			 randomInfo->randomPool[ 40 ] != \
					( exportedRandomInfo->randomPool[ 40 ] ^ 0xFF ) );

	/* Precondition for sampling the output: It's a sample from the start of
	   the pool */
	ENSURES( samplePtr == randomInfo->randomPool && \
			 x917SamplePtr == exportedRandomInfo->randomPool );

	/* Check for stuck-at faults by comparing a short sample from the current
	   output with samples from the previous RANDOMPOOL_SAMPLES outputs */
	sample = mgetLong( samplePtr );
	for( i = 0; i < RANDOMPOOL_SAMPLES; i++ )
		{
		if( randomInfo->prevOutput[ i ] == sample )
			{
			/* We're repeating previous output, tell the caller to try
			   again */
			return( OK_SPECIAL );
			}
		}

	/* Postcondition: There are no values seen during a previous run present
	   in the output */
	FORALL( i, 0, RANDOMPOOL_SAMPLES, \
			randomInfo->prevOutput[ i ] != sample );

	/* Process the exported pool with the X9.17 generator */
	status = generateX917( randomInfo, exportedRandomInfo->randomPool,
						   RANDOMPOOL_ALLOCSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Check for stuck-at faults in the X9.17 generator by comparing a short
	   sample from the current output with samples from the previous
	   RANDOMPOOL_SAMPLES outputs.  If it's the most recent sample then FIPS
	   140 requires an absolute failure if there's a duplicate rather than
	   simply signalling a problem and letting the higher layer handle it.
	   Because this will lead to false positives even for a perfect 
	   generator we provide a custom check in which if we get a match in the 
	   first 32 bits then we perform a backup check on the full 
	   RANDOMPOOL_SAMPLE_SIZE bytes and return a hard failure if all of the 
	   bits match.

	   There's an implied additional requirement in the sampling process in 
	   which the zero'th iteration of the X9.17 generator doesn't have a 
	   previous sample to compare to and therefore can't meet the 
	   requirements for previous-sample checking, however this is handled by
	   having the generator cranked twice on init/reinit in 
	   getRandomOutput(), which provides the necessary zero'th sample */
	sample = mgetLong( x917SamplePtr );
	for( i = 0; i < RANDOMPOOL_SAMPLES; i++ )
		{
		if( randomInfo->x917PrevOutput[ i ] == sample )
			{
			/* If we've failed on the first sample and the full match also
			   fails, return a hard error */
			if( i == 0 && \
				!memcmp( randomInfo->x917OuputSample,
						 exportedRandomInfo->randomPool,
						 RANDOMPOOL_SAMPLE_SIZE ) )
				{
				retIntError();
				}

			/* We're repeating previous output, tell the caller to try
			   again */
			return( OK_SPECIAL );
			}
		}

	/* Postcondition: There are no values seen during a previous run present
	   in the output */
	FORALL( i, 0, RANDOMPOOL_SAMPLES, \
			randomInfo->x917PrevOutput[ i ] != sample );

	ENSURES( sanityCheck( randomInfo ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getRandomOutput( INOUT RANDOM_INFO *randomInfo, 
							OUT_BUFFER_FIXED( length ) BYTE *buffer, 
							IN_RANGE( 1, RANDOM_OUTPUTSIZE ) const int length )
	{
	RANDOM_INFO exportedRandomInfo;
	BYTE *samplePtr;
	int noRandomRetries, i, status;
	ORIGINAL_INT_VAR( prevOutputIndex, randomInfo->prevOutputIndex );

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	static_assert( RANDOM_OUTPUTSIZE == RANDOMPOOL_SIZE / 2, \
				   "Random pool size" );

	/* Precondition for output quantity: We're being asked for a valid output
	   length and we're not trying to use more than half the pool contents */
	REQUIRES( sanityCheck( randomInfo ) );
	REQUIRES( length > 0 && length <= RANDOM_OUTPUTSIZE && \
			  length <= RANDOMPOOL_SIZE / 2 );

	/* If the X9.17 generator cryptovariables haven't been initialised yet
	   or have reached their use-by date, set the generator key and seed from
	   the pool contents, then mix the pool and crank the generator twice to
	   obscure the data that was used.  This also provides the zero'th sample
	   of output required by the FIPS 140 tests */
	if( !randomInfo->x917Inited || \
		randomInfo->x917Count >= X917_MAX_CYCLES )
		{
		status = mixRandomPool( randomInfo );
		if( cryptStatusOK( status ) )
			status = setKeyX917( randomInfo, randomInfo->randomPool,
								 randomInfo->randomPool + X917_KEYSIZE, 
								 NULL );
		if( cryptStatusOK( status ) )
			status = mixRandomPool( randomInfo );
		if( cryptStatusOK( status ) )
			status = generateX917( randomInfo, randomInfo->randomPool,
								   RANDOMPOOL_ALLOCSIZE );
		if( cryptStatusOK( status ) )
			status = mixRandomPool( randomInfo );
		if( cryptStatusOK( status ) )
			status = generateX917( randomInfo, randomInfo->randomPool,
								   RANDOMPOOL_ALLOCSIZE );
		if( cryptStatusError( status ) )
			return( status );
		memcpy( randomInfo->x917OuputSample, randomInfo->randomPool,
				RANDOMPOOL_SAMPLE_SIZE );	/* Save zero'th output sample */
		}

	/* Precondition for drawing output from the generator: The pool is
	   sufficiently mixed, there's enough entropy present, and the X9.17
	   post-processor is ready for use */
	REQUIRES( randomInfo->randomPoolMixes == RANDOMPOOL_MIXES && \
			  randomInfo->randomQuality >= 100 && randomInfo->x917Inited );

	/* Initialise the pool to contain the exported random data */
	initRandomPool( &exportedRandomInfo );

	/* Try to obtain random data from the pool.  If the initial attempt to 
	   get entropy fails, retry a fixed number of times */
	status = tryGetRandomOutput( randomInfo, &exportedRandomInfo );
	for( noRandomRetries = 1; 
	     status == OK_SPECIAL && noRandomRetries < RANDOMPOOL_RETRIES; 
		 noRandomRetries++ )
		{
		status = tryGetRandomOutput( randomInfo, &exportedRandomInfo );
		}
	if( cryptStatusError( status ) )
		{
		/* We ran out of retries so that we're repeating the same output
		   data or there was some other type of error, fail */
		endRandomPool( &exportedRandomInfo );

		/* Postcondition: Nulla vestigia retrorsum */
		FORALL( i, 0, RANDOMPOOL_ALLOCSIZE, \
				exportedRandomInfo.randomPool[ i ] == 0 );

		/* We can't trust the pool data any more so we set its content
		   value to zero.  Ideally we should flash lights and sound
		   klaxons as well, this is a catastrophic failure */
		randomInfo->randomQuality = randomInfo->randomPoolMixes = 0;
		randomInfo->x917Inited = FALSE;
		retIntError();
		}

	/* Save a short sample from the current output for future checks */
	REQUIRES( randomInfo->prevOutputIndex >= 0 && \
			  randomInfo->prevOutputIndex < RANDOMPOOL_SAMPLES );
	samplePtr = randomInfo->randomPool;
	randomInfo->prevOutput[ randomInfo->prevOutputIndex ] = mgetLong( samplePtr );
	samplePtr = exportedRandomInfo.randomPool;
	randomInfo->x917PrevOutput[ randomInfo->prevOutputIndex ] = mgetLong( samplePtr );
	randomInfo->prevOutputIndex = ( randomInfo->prevOutputIndex + 1 ) % \
								  RANDOMPOOL_SAMPLES;
	memcpy( randomInfo->x917OuputSample, exportedRandomInfo.randomPool,
			RANDOMPOOL_SAMPLE_SIZE );
	ENSURES( randomInfo->prevOutputIndex != ORIGINAL_VALUE( prevOutputIndex ) );
	ENSURES( randomInfo->prevOutputIndex == 0 || \
			 randomInfo->prevOutputIndex == ORIGINAL_VALUE( prevOutputIndex ) + 1 );
	ENSURES( randomInfo->prevOutputIndex >= 0 && \
			 randomInfo->prevOutputIndex < RANDOMPOOL_SAMPLES );

	/* Copy the transformed data to the output buffer, folding it in half as
	   we go to mask the original content */
	for( i = 0; i < length; i++ )
		{
		buffer[ i ] = exportedRandomInfo.randomPool[ i ] ^ \
					  exportedRandomInfo.randomPool[ RANDOM_OUTPUTSIZE + i ];
		}

	/* Postcondition: We drew at most half of the transformed output from the
	   export pool, and the output came from the export pool and not the main
	   pool */
	ENSURES( i <= RANDOMPOOL_SIZE / 2 );
	EXISTS( i, 0, RANDOMPOOL_SIZE / 2, \
			buffer[ i ] != ( randomInfo->randomPool[ i ] ^ \
							 randomInfo->randomPool[ RANDOM_OUTPUTSIZE + i ] ) );

	/* Clean up */
	endRandomPool( &exportedRandomInfo );

	/* Postcondition: Nulla vestigia retrorsum */
	FORALL( i, 0, RANDOMPOOL_ALLOCSIZE, \
			exportedRandomInfo.randomPool[ i ] == 0 );

	ENSURES( sanityCheck( randomInfo ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getRandomData( INOUT TYPECAST( RANDOM_INFO * ) void *randomInfoPtr, 
				   OUT_BUFFER_FIXED( length ) void *buffer, 
				   IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length )
	{
	RANDOM_INFO *randomInfo = ( RANDOM_INFO * ) randomInfoPtr;
	BYTE *bufPtr = buffer;
	int randomQuality, count, iterationCount, status;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	/* Precondition: We're not asking for more data than the maximum that
	   should be needed */
	REQUIRES( length > 0 && length <= MAX_RANDOM_BYTES );

	/* Clear the return value and by extension make sure that we fail the
	   FIPS 140-like entropy tests on the output if there's a problem */
	zeroise( buffer, length );

	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES_MUTEX( sanityCheck( randomInfo ) );

	/* If we're using a stored random seed, add it to the entropy pool if
	   necessary.  Note that we do this here rather than when we initialise
	   the randomness subsystem both because at that point the stream
	   subsystem may not be ready for use yet and because there may be a
	   requirement to periodically re-read the seed data if it's changed
	   by another process/task */
#ifdef CONFIG_RANDSEED
	if( !randomInfo->seedProcessed )
		addStoredSeedData( randomInfo );
#endif /* CONFIG_RANDSEED */

	/* Get the randomness quality before we release the randomness info
	   again */
	randomQuality = randomInfo->randomQuality;

	krnlExitMutex( MUTEX_RANDOM );

	/* Perform a failsafe check to make sure that there's data available.
	   This should only ever be called once per application because after 
	   the first blocking poll that occurs because the user has tried to
	   generate keying material without having first seeded the generator
	   the programmer of the calling application will make sure that 
	   there's a slow poll done earlier on */
	if( randomQuality < 100 )
		slowPoll();

	/* Make sure that any background randomness-gathering process has
	   finished */
	status = waitforRandomCompletion( FALSE );
	if( cryptStatusError( status ) )
		return( status );

	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES_MUTEX( sanityCheck( randomInfo ) );

	/* If we still can't get any random information, let the user know */
	if( randomInfo->randomQuality < 100 )
		{
		krnlExitMutex( MUTEX_RANDOM );
		DEBUG_DIAG(( "No random data available" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_RANDOM );
		}

	/* If the process has forked we need to restart the generator output
	   process but we can't determine this until after we've already
	   produced the output.  If we do need to restart, we do it from this
	   point.

	   There is one variant of this problem that we can't work around and 
	   that's where we're running inside a VM with rollback support.  Some 
	   VMs can take periodic snapshots of the system state to allow rollback 
	   to a known-good state if an error occurs.  Since the VM's rollback is 
	   transparent to the OS there's no way to detect that this has 
	   occcurred (although getSysVars() can detect the presence of some 
	   common VMs it can't detect whether a rollback has occurred).  In this 
	   case we'd roll back to a previous state of the RNG and continue from 
	   there.  OTOH it's hard to identify a situation in which this would 
	   pose a serious threat.  Consider for example SSL or SSH session key 
	   setup/generation: If we haven't committed the data to the remote 
	   system yet it's no problem and if we have then we're now out of sync 
	   with the remote system and the handshake will fail.  Similarly, if 
	   we're generating a DSA signature then we'll end up generating the 
	   same signature again but since it's over the same data there's no 
	   threat involved.  Being able to cause a change in the data being 
	   signed after the random DSA k value is generated would be a problem, 
	   but k is only generated after the data has already been hashed and 
	   the signature is about to be generated.

	   In general this type of attack would require cooperation between the 
	   VM and a hostile external party to, for example, ignore the fact 
	   that the VM has rolled back to an earlier point in the protocol so a 
	   repeat of a previous handshake message will be seen.  In other words 
	   it more or less requires control over the VM by an external party, and 
	   anyone faced with this level of attack has bigger things to worry 
	   about than RNG state rollback */
restartPoint:

	/* Prepare to get data from the randomness pool.  Before we do this we
	   perform a final quick poll of the system to get any last bit of
	   entropy, and mix the entire pool.  If the pool hasn't been sufficiently
	   mixed, we iterate until we've reached the minimum mix count */
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 iterationCount++ )
		{
		DECLARE_ORIGINAL_INT( randomPoolMixes );

		fastPoll();

		/* Mix the pool after the fast poll.  The poll itself can result in 
		   multiple sets of mixing, this final mix ensures that there's no
		   unmixed data left */
		STORE_ORIGINAL_INT( randomPoolMixes, randomInfo->randomPoolMixes );
		status = mixRandomPool( randomInfo );
		if( cryptStatusError( status ) )
			{
			krnlExitMutex( MUTEX_RANDOM );
			return( status );
			}
		ENSURES( randomInfo->randomPoolMixes == RANDOMPOOL_MIXES || \
				 randomInfo->randomPoolMixes == \
								ORIGINAL_VALUE( randomPoolMixes ) + 1 );

		/* If the pool is sufficiently mixed, we're done */
		if( randomInfo->randomPoolMixes >= RANDOMPOOL_MIXES )
			break;
		}
	ENSURES_MUTEX( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Keep producing RANDOMPOOL_OUTPUTSIZE bytes of output until the request
	   is satisfied */
	for( count = 0; count < length; count += RANDOM_OUTPUTSIZE )
		{
		const int outputBytes = min( length - count, RANDOM_OUTPUTSIZE );
		ORIGINAL_PTR( bufPtr );

		/* Precondition for output quantity: Either we're on the last output
		   block or we're producing the maximum-size output quantity, and
		   we're never trying to use more than half the pool contents */
		REQUIRES_MUTEX( length - count < RANDOM_OUTPUTSIZE || \
						outputBytes == RANDOM_OUTPUTSIZE );
		REQUIRES_MUTEX( outputBytes <= RANDOMPOOL_SIZE / 2 );

		status = getRandomOutput( randomInfo, bufPtr, outputBytes );
		if( cryptStatusError( status ) )
			{
			krnlExitMutex( MUTEX_RANDOM );
			return( status );
			}
		bufPtr += outputBytes;

		/* Postcondition: We're filling the output buffer and we wrote the
		   output to the correct portion of the output buffer */
		ENSURES( ( bufPtr > ( BYTE * ) buffer ) && \
				 ( bufPtr <= ( BYTE * ) buffer + length ) );
		ENSURES( bufPtr == ORIGINAL_VALUE( bufPtr ) + outputBytes );
		}

	/* Postcondition: We filled the output buffer with the required amount
	   of output */
	ENSURES_MUTEX( bufPtr == ( BYTE * ) buffer + length );

	/* Check whether the process forked while we were generating output.  If
	   it did, force a complete remix of the pool and restart the output
	   generation process (the fast poll will ensure that the pools in the
	   parent and child differ) */
	if( checkForked() )
		{
		randomInfo->randomPoolMixes = 0;
		bufPtr = buffer;
		goto restartPoint;
		}

	krnlExitMutex( MUTEX_RANDOM );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Init/Shutdown Routines							*
*																			*
****************************************************************************/

/* Initialise the randomness subsystem */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initRandomInfo( OUT_OPT_PTR TYPECAST( RANDOM_INFO ** ) void **randomInfoPtrPtr )
	{
	RANDOM_INFO testRandomInfo, *randomInfoPtr;
	BYTE buffer[ 16 + 8 ];
	int status;

	assert( isWritePtr( randomInfoPtrPtr, sizeof( void * ) ) );

	/* Clear return value */
	*randomInfoPtrPtr = NULL;

	/* Make sure that the crypto that we need is functioning as required */
	status = randomAlgorithmSelfTest();
	ENSURES( cryptStatusOK( status ) );

	/* The underlying crypto is OK, check that the cryptlib PRNG is working
	   correctly */
	initRandomPool( &testRandomInfo );
	status = mixRandomPool( &testRandomInfo );
	if( cryptStatusOK( status ) && \
		memcmp( testRandomInfo.randomPool,
				"\xF6\x8F\x30\xEE\x52\x13\x3E\x40\x06\x06\xA6\xBE\x91\xD2\xD9\x82", 16 ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = mixRandomPool( &testRandomInfo );
	if( cryptStatusOK( status ) && \
		memcmp( testRandomInfo.randomPool,
				"\xAE\x94\x3B\xF2\x86\x5F\xCF\x76\x36\x2B\x80\xD5\x73\x86\x9B\x69", 16 ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusOK( status ) )
		status = mixRandomPool( &testRandomInfo );
	if( cryptStatusOK( status ) && \
		memcmp( testRandomInfo.randomPool,
				"\xBC\x2D\xC1\x03\x8C\x78\x6D\x04\xA8\xBD\xD5\x51\x80\xCA\x42\xF4", 16 ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusError( status ) )
		{
		endRandomPool( &testRandomInfo );
		retIntError();
		}

	/* Check that the ANSI X9.17 PRNG is working correctly */
	status = selfTestX917( &testRandomInfo, testRandomInfo.randomPool );
	if( cryptStatusError( status ) )
		{
		endRandomPool( &testRandomInfo );
		retIntError();
		}

	/* The underlying PRNGs are OK, check the overall random number
	   generation system.  Since we started with an all-zero seed we have
	   to fake the entropy-quality values for the artificial test pool */
	testRandomInfo.randomQuality = 100;
	testRandomInfo.randomPoolMixes = RANDOMPOOL_MIXES;
	status = getRandomOutput( &testRandomInfo, buffer, 16 );
	if( cryptStatusOK( status ) && \
		memcmp( buffer, "\x6B\x59\x1D\xCD\xE1\xB3\xA8\x50\x32\x84\x8C\x8D\x93\xB0\x74\xD7", 16 ) )
		status = CRYPT_ERROR_FAILED;
	if( cryptStatusError( status ) )
		{
		endRandomPool( &testRandomInfo );
		retIntError();
		}
	endRandomPool( &testRandomInfo );

	/* Check the ANSI X9.17 PRNG again, this time using FIPS test vectors */
	status = fipsTestX917( &testRandomInfo );
	if( cryptStatusError( status ) )
		{
		endRandomPool( &testRandomInfo );
		retIntError();
		}

	/* Allocate and initialise the random pool */
	if( ( status = krnlMemalloc( ( void ** ) &randomInfoPtr, \
								 sizeof( RANDOM_INFO ) ) ) != CRYPT_OK )
		return( status );
	initRandomPool( randomInfoPtr );

	/* Initialise any helper routines that may be needed */
	initRandomPolling();

	*randomInfoPtrPtr = randomInfoPtr;
	return( CRYPT_OK );
	}

/* Shut down the randomness subsystem.  Exactly what to do if we can't 
   exit the polling thread or acquire the mutex is a bit complicated, this 
   is a shouldn't-occur exception condition condition so it's not even 
   possible to plan for this since it's uncertain under which conditions (if 
   ever) this situation would occur.  We can't even perform a failsafe 
   zeroise of the pool data because it could lead to the other thread using 
   an all-zero key from the unexpectedly-cleared pool.  For now we play it 
   by the book and don't do anything if we can't exit the thread or acquire 
   the mutex, which avoids a segfault from pulling the random data out from 
   underneath the other thread */

STDC_NONNULL_ARG( ( 1 ) ) \
void endRandomInfo( INOUT TYPECAST( RANDOM_INFO ** ) void **randomInfoPtrPtr )
	{
	RANDOM_INFO *randomInfoPtr = *randomInfoPtrPtr;
	int status;

	assert( isWritePtr( randomInfoPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( *randomInfoPtrPtr, sizeof( RANDOM_INFO ) ) );

	/* Make sure that there are no background threads/processes still trying
	   to send us data */
	status = waitforRandomCompletion( TRUE );
	ENSURES_V( cryptStatusOK( status ) );	/* See comment above */

	/* Call any special-case shutdown functions */
	endRandomPolling();

	/* Shut down the random data pool.  We acquire the randomness mutex 
	   while we're doing this to ensure that any threads still using the
	   randomness info have exited before we destroy it */
	status = krnlEnterMutex( MUTEX_RANDOM );
	ENSURES_V( cryptStatusOK( status ) );	/* See comment above */
	endRandomPool( randomInfoPtr );
	krnlExitMutex( MUTEX_RANDOM );
	status = krnlMemfree( randomInfoPtrPtr );
	ENSURES_V( cryptStatusOK( status ) );	/* See comment above */
	}

/****************************************************************************
*																			*
*							Add Random (Entropy) Data						*
*																			*
****************************************************************************/

/* Add new entropy data and an entropy quality estimate to the random pool */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addEntropyData( INOUT TYPECAST( RANDOM_INFO * ) void *randomInfoPtr, 
					IN_BUFFER( length ) const void *buffer, 
					IN_LENGTH const int length )
	{
	RANDOM_INFO *randomInfo = ( RANDOM_INFO * ) randomInfoPtr;
	const BYTE *bufPtr = ( BYTE * ) buffer;
	int count, status;
#if 0	/* See comment in addEntropyQuality */
	DECLARE_ORIGINAL_INT( entropyByteCount );
#endif /* 0 */

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );
	assert( isReadPtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES_MUTEX( sanityCheck( randomInfo ) );

#if 0	/* See comment in addEntropyQuality */
	STORE_ORIGINAL_INT( entropyByteCount, randomInfo->entropyByteCount );
#endif /* 0 */

	/* Mix the incoming data into the pool.  This operation is resistant to
	   chosen- and known-input attacks because the pool contents are unknown
	   to an attacker so XORing in known data won't help them.  If an
	   attacker could determine pool contents by observing the generator
	   output (which is defeated by the postprocessing) we'd have to perform 
	   an extra input mixing operation to defeat these attacks */
	for( count = 0; count < length; count++ )
		{
		ORIGINAL_INT_VAR( bufVal, bufPtr[ count ] );
		DECLARE_ORIGINAL_INT( poolVal );
		DECLARE_ORIGINAL_INT( newPoolVal );
		DECLARE_ORIGINAL_INT( poolPos );

		/* If the pool write position has reached the end of the pool, mix
		   the pool */
		if( randomInfo->randomPoolPos >= RANDOMPOOL_SIZE )
			{
			status = mixRandomPool( randomInfo );
			if( cryptStatusError( status ) )
				{
				krnlExitMutex( MUTEX_RANDOM );
				return( status );
				}
			ENSURES_MUTEX( randomInfo->randomPoolPos == 0 );
			}

		STORE_ORIGINAL_INT( poolVal,
							randomInfo->randomPool[ randomInfo->randomPoolPos ] );
		STORE_ORIGINAL_INT( poolPos, randomInfo->randomPoolPos );

		/* Precondition: We're adding data inside the pool */
		REQUIRES_MUTEX( randomInfo->randomPoolPos >= 0 && \
						randomInfo->randomPoolPos < RANDOMPOOL_SIZE );

		randomInfo->randomPool[ randomInfo->randomPoolPos++ ] ^= bufPtr[ count ];

		STORE_ORIGINAL_INT( newPoolVal,
							randomInfo->randomPool[ randomInfo->randomPoolPos - 1 ] );

		/* Postcondition: We've updated the byte at the current pool
		   position, and the value really was XORed into the pool rather
		   than (for example) overwriting it as with PGP/xorbytes or
		   GPG/add_randomness.  Note that in this case we can use a non-XOR
		   operation to check that the XOR succeeded, unlike the pool mixing
		   code which requires an XOR to check the original XOR */
		ENSURES( randomInfo->randomPoolPos == \
						ORIGINAL_VALUE( poolPos ) + 1 );
		ENSURES( ( ( ORIGINAL_VALUE( newPoolVal ) == \
						ORIGINAL_VALUE( bufVal ) ) && \
				   ( ORIGINAL_VALUE( poolVal ) == 0 ) ) || \
				 ( ORIGINAL_VALUE( newPoolVal ) != \
						ORIGINAL_VALUE( bufVal ) ) );
		}

#if 0	/* See comment in addEntropyQuality */
	/* Remember how many bytes of entropy we added on this update */
	randomInfo->entropyByteCount += length;
#endif /* 0 */

	/* Postcondition: We processed all of the data */
	ENSURES( count == length );
#if 0	/* See comment in addEntropyQuality */
	ENSURES( randomInfo->entropyByteCount == \
			 ORIGINAL_VALUE( entropyByteCount ) + length );
#endif /* 0 */

	ENSURES_MUTEX( sanityCheck( randomInfo ) );

	krnlExitMutex( MUTEX_RANDOM );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addEntropyQuality( INOUT TYPECAST( RANDOM_INFO * ) void *randomInfoPtr, 
					   IN_RANGE( 1, 100 ) const int quality )
	{
	RANDOM_INFO *randomInfo = ( RANDOM_INFO * ) randomInfoPtr;
	int status;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	REQUIRES( quality > 0 && quality <= 100 );

	status = krnlEnterMutex( MUTEX_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES_MUTEX( sanityCheck( randomInfo ) );

	/* In theory we could check to ensure that the claimed entropy quality
	   corresponds approximately to the amount of entropy data added,
	   however in a multithreaded environment this doesn't work because the
	   entropy addition is distinct from the entropy quality addition so 
	   that (for example) with entropy being added by three threads we could
	   end up with the following:

		entropy1, entropy1,
		entropy2,
		entropy1,
		entropy3,
		entropy1,
		entropy3,
		entropy2,
		quality2, reset to 0
		quality1, fail since reset to 0
		quality3, fail since reset to 0

	   This means that the first entropy quality measure added is applied
	   to all of the previously added entropy after which the entropy byte 
	   count is reset, causing subsequent attempts to add entropy quality to 
	   fail.  In addition the first quality value is applied to all of the 
	   entropy added until that point rather than just the specific entropy 
	   samples that it corresponds to.  In theory this could be addressed by 
	   requiring the entropy source to treat entropy addition as a 
	   database-style BEGIN ... COMMIT transaction but this makes the 
	   interface excessively complex for both source and sink, and more 
	   prone to error than the small gain in entropy quality-checking is 
	   worth */
#if 0
	if( randomInfo->entropyByteCount <= 0 || \
		quality / 2 > randomInfo->entropyByteCount )
		{
		/* If there's not enough entropy data present to justify the
		   claimed entropy quality level, signal an error.  We do however
		   retain the existing entropy byte count for use the next time an
		   entropy quality estimate is added, since it's still contributing
		   to the total entropy quality */
		krnlExitMutex( MUTEX_RANDOM );
		retIntError();
		}
	randomInfo->entropyByteCount = 0;
#endif /* 0 */

	/* If we haven't reached the minimum quality level for generating keys
	   yet, update the quality level */
	if( randomInfo->randomQuality < 100 )
		{
		/* Update the quality count, making sure that it stays within 
		   bounds */
		if( randomInfo->randomQuality + quality > 100 )
			randomInfo->randomQuality = 100;
		else
			randomInfo->randomQuality += quality;
		}

	ENSURES_MUTEX( sanityCheck( randomInfo ) );

	krnlExitMutex( MUTEX_RANDOM );

	return( CRYPT_OK );
	}

#ifdef CONFIG_RANDSEED

/* Add entropy data from a stored seed value */

STDC_NONNULL_ARG( ( 1 ) ) \
static void addStoredSeedData( INOUT RANDOM_INFO *randomInfo )
	{
	STREAM stream;
	BYTE streamBuffer[ STREAM_BUFSIZE + 8 ], seedBuffer[ 1024 + 8 ];
	char seedFilePath[ MAX_PATH_LENGTH + 8 ];
	int seedFilePathLen, poolCount, length, status;

	assert( isWritePtr( randomInfo, sizeof( RANDOM_INFO ) ) );

	/* Try and access the stored seed data */
	status = fileBuildCryptlibPath( seedFilePath, MAX_PATH_LENGTH, 
									&seedFilePathLen, NULL, 0,
									BUILDPATH_RNDSEEDFILE );
	if( cryptStatusOK( status ) )
		{
		/* The file path functions are normally used with krnlSendMessage()
		   which takes { data, length } parameters, since we've calling
		   the low-level function sFileOpen() directly we have to null-
		   terminate the string */
		seedFilePath[ seedFilePathLen ] = '\0';
		status = sFileOpen( &stream, seedFilePath, FILE_FLAG_READ );
		}
	if( cryptStatusError( status ) )
		{
		/* The seed data isn't present, don't try and access it again */
		randomInfo->seedProcessed = TRUE;
		DEBUG_DIAG(( "Error opening random seed file" ));
		assert( DEBUG_WARN );
		return;
		}

	/* Read up to 1K of data from the stored seed */
	sioctlSetString( &stream, STREAM_IOCTL_IOBUFFER, streamBuffer, 
					 STREAM_BUFSIZE );
	sioctlSet( &stream, STREAM_IOCTL_PARTIALREAD, TRUE );
	status = length = sread( &stream, seedBuffer, 1024 );
	sFileClose( &stream );
	zeroise( streamBuffer, STREAM_BUFSIZE );
	if( cryptStatusError( status ) || length <= 16 )
		{
		/* The seed data is present but we can't read it or there's not 
		   enough present to use, don't try and access it again */
		randomInfo->seedProcessed = TRUE;
		DEBUG_DIAG(( "Error reading random seed file" ));
		assert( DEBUG_WARN );
		return;
		}
	ENSURES_V( length >= 16 && length <= 1024 );
	randomInfo->seedSize = length;

	/* Precondition: We got at least some non-zero data */
	EXISTS( i, 0, length,
			seedBuffer[ i ] != 0 );

	/* Add the seed data to the entropy pool.  Both because the entropy-
	   management code gets suspicious about very small amounts of data with
	   claimed high entropy and because it's a good idea to start with all
	   of the pool set to the seed data (rather than most of it set at zero
	   if the seed data is short), we add the seed data repeatedly until
	   we've filled the pool */
	for( poolCount = RANDOMPOOL_SIZE; poolCount > 0; poolCount -= length )
		{
		status = addEntropyData( randomInfo, seedBuffer, length );
		assert( cryptStatusOK( status ) );
		}

	/* There were at least 128 bits of entropy present in the seed, set the 
	   entropy quality to the user-provided value */
	status = addEntropyQuality( randomInfo, CONFIG_RANDSEED_QUALITY );
	assert( cryptStatusOK( status ) );

	zeroise( seedBuffer, 1024 );

	/* Postcondition: Nulla vestigia retrorsum */
	FORALL( i, 0, 1024,
			seedBuffer[ i ] == 0 );
	}
#endif /* CONFIG_RANDSEED */

/****************************************************************************
*																			*
*						Random Pool External Interface						*
*																			*
****************************************************************************/

/* Convenience functions used by the system-specific randomness-polling
   routines to send data to the system device.  These just accumulate as
   close to bufSize bytes of data as possible in a user-provided buffer and
   then forward them to the device object.  Note that addRandomData()
   assumes that the quantity of data being added is small (a fixed-size
   struct or something similar), it shouldn't be used to add large buffers
   full of data since information at the end of the buffer will be lost */

typedef struct {
	BUFFER( bufSize, bufPos ) \
	void *buffer;			/* Entropy buffer */
	int bufPos, bufSize;	/* Current buffer pos.and total size */
	int updateStatus;		/* Error status if update failed */
	} RANDOM_STATE_INFO;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initRandomData( INOUT TYPECAST( RANDOM_STATE_INFO * ) void *statePtr, 
					IN_BUFFER( maxSize ) void *buffer, 
					IN_LENGTH_SHORT_MIN( 16 ) const int maxSize )
	{
	RANDOM_STATE_INFO *state = ( RANDOM_STATE_INFO * ) statePtr;

	assert( isWritePtr( state, sizeof( RANDOM_STATE_INFO ) ) );
	assert( isWritePtr( buffer, maxSize ) );

	static_assert( sizeof( RANDOM_STATE_INFO ) <= sizeof( RANDOM_STATE ),
				   "Random pool state size" );

	REQUIRES( maxSize >= 16 && maxSize < MAX_INTLENGTH_SHORT );

	memset( state, 0, sizeof( RANDOM_STATE_INFO ) );
	state->buffer = buffer;
	state->bufSize = maxSize;

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addRandomData( INOUT TYPECAST( RANDOM_STATE_INFO * ) void *statePtr, 
				   IN_BUFFER( valueLength ) const void *value, 
				   IN_LENGTH_SHORT const int valueLength )
	{
	RANDOM_STATE_INFO *state = ( RANDOM_STATE_INFO * ) statePtr;
	MESSAGE_DATA msgData;
	const BYTE *valuePtr = value;
	int bytesToCopy = min( valueLength, state->bufSize - state->bufPos );
	int totalLength = valueLength, status;

	assert( isWritePtr( state, sizeof( RANDOM_STATE_INFO ) ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( state->bufSize >= 16 && state->bufSize < MAX_INTLENGTH_SHORT );
	REQUIRES( state->bufPos >= 0 && state->bufPos <= state->bufSize );
	REQUIRES( valueLength > 0 && valueLength < MAX_INTLENGTH_SHORT );

	/* If we're in an error state, don't try and do anything */
	if( cryptStatusError( state->updateStatus ) )
		return( state->updateStatus );

	/* Copy as much of the input as we can into the accumulator */
	if( bytesToCopy > 0 )
		{
		ENSURES( state->bufPos + bytesToCopy <= state->bufSize );
		
		ANALYSER_HINT( bytesToCopy > 0 && bytesToCopy <= valueLength );

		memcpy( ( BYTE * ) state->buffer + state->bufPos, valuePtr, bytesToCopy );
		state->bufPos += bytesToCopy;
		valuePtr += bytesToCopy;
		totalLength -= bytesToCopy;
		}
	ENSURES( totalLength >= 0 && totalLength < MAX_INTLENGTH_SHORT );

	/* If everything went into the accumulator, we're done */
	if( state->bufPos < state->bufSize )
		return( CRYPT_OK );

	ENSURES( state->bufPos == state->bufSize );

	/* The accumulator is full, send the data through to the system device */
	setMessageData( &msgData, state->buffer, state->bufPos );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_ENTROPY );
	if( cryptStatusError( status ) )
		{
		/* There was a problem moving the data through, make the error status 
		   persistent.  Normally this is a should-never-occur error 
		   condition but if cryptlib has been shut down from another thread 
		   then the kernel will fail all non shutdown-related calls with a 
		   permission error.  To avoid false alarms we mask out failures due 
		   to permission errors */
		state->updateStatus = status;
		assert( ( status == CRYPT_ERROR_PERMISSION ) || DEBUG_WARN );
		return( status );
		}
	state->bufPos = 0;

	/* If we've consumed all of the data, we're done */
	if( totalLength <= 0 )
		return( CRYPT_OK );

	/* There's uncopied data left, copy it in now.  If there's more data 
	   present than can fit in the accumulator's buffer we discard it 
	   (although we warn in the debug build, which is why the code below
	   has an assert() rather than a REQUIRES()), the caller should be 
	   sending quantities this large directly rather than using the 
	   addRandomData() interface */
	assert( totalLength < state->bufSize );	/* Debug warning only */
	bytesToCopy = min( totalLength, state->bufSize );
	memcpy( state->buffer, valuePtr, bytesToCopy );
	state->bufPos += bytesToCopy;

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addRandomLong( INOUT void *statePtr, const long value )
	{
	return( addRandomData( statePtr, &value, sizeof( long ) ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int endRandomData( INOUT void *statePtr, \
				   IN_RANGE( 0, 100 ) const int quality )
	{
	RANDOM_STATE_INFO *state = ( RANDOM_STATE_INFO * ) statePtr;
	int status = state->updateStatus;

	assert( isWritePtr( state, sizeof( RANDOM_STATE_INFO ) ) );

	REQUIRES( state->bufSize >= 16 && state->bufSize < MAX_INTLENGTH_SHORT );
	REQUIRES( state->bufPos >= 0 && state->bufPos <= state->bufSize );
	REQUIRES( quality >= 0 && quality <= 100 );

	/* If we're in an error state, don't try and do anything */
	if( cryptStatusError( state->updateStatus ) )
		return( state->updateStatus );

	/* If there's data still in the accumulator send it through to the 
	   system device.  A failure at this point is a should-never-occur 
	   condition but if cryptlib has been shut down from another thread then 
	   the kernel will fail all non shutdown-related calls with a permission
	   error.  To avoid false alarms we mask out failures due to permission
	   errors */
	if( state->bufPos > 0 )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, state->buffer, state->bufPos );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_ENTROPY );
		}
	assert( cryptStatusOK( status ) || ( status == CRYPT_ERROR_PERMISSION ) );

	/* If everything went OK, set the quality estimate for the data that
	   we've added */
	if( cryptStatusOK( status ) && quality > 0 )
		{
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &quality,
								  CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
		}
	assert( cryptStatusOK( status ) || ( status == CRYPT_ERROR_PERMISSION ) );

	/* Clear the accumulator and exit */
	zeroise( state->buffer, state->bufSize );
	zeroise( state, sizeof( RANDOM_STATE_INFO ) );
	return( status );
	}
