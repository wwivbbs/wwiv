/****************************************************************************
*																			*
*						cryptlib Internal Time/Timer API					*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Time Functions 								*
*																			*
****************************************************************************/

/* Get the system time safely.  The first function implements hard failures,
   converting invalid time values to zero, which yield a warning date of
   1/1/1970 rather than an out-of-bounds or garbage value.  The second
   function implements soft failures, returning an estimate of the
   approximate current date.  The third function is used for operations such
   as signing certs and timestamping and tries to get the time from a
   hardware time source if one is available.
   
   Because of the implementation-dependent behaviour of the time_t type we 
   perform an explicit check against '( time_t ) -1' as well as a general 
   range check to avoid being caught by conversion problems if time_t is a 
   type too far removed from int */

time_t getTime( void )
	{
	const time_t theTime = time( NULL );

	if( ( theTime == ( time_t ) -1 ) || ( theTime <= MIN_TIME_VALUE ) )
		{
		DEBUG_DIAG(( "No time source available" ));
		assert( DEBUG_WARN );
		return( 0 );
		}
	return( theTime );
	}

time_t getApproxTime( void )
	{
	const time_t theTime = time( NULL );

	if( ( theTime == ( time_t ) -1 ) || ( theTime <= MIN_TIME_VALUE ) )
		{
		DEBUG_DIAG(( "No time source available" ));
		assert( DEBUG_WARN );
		return( CURRENT_TIME_VALUE );
		}
	return( theTime );
	}

time_t getReliableTime( IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	CRYPT_DEVICE cryptDevice;
	MESSAGE_DATA msgData;
	time_t theTime;
	int status;

	REQUIRES_EXT( ( cryptHandle == SYSTEM_OBJECT_HANDLE || \
					isHandleRangeValid( cryptHandle ) ), 0 );

	/* Get the dependent device for the object that needs the time */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETDEPENDENT,
							  &cryptDevice, OBJECT_TYPE_DEVICE );
	if( cryptStatusError( status ) )
		cryptDevice = SYSTEM_OBJECT_HANDLE;

	/* Try and get the time from the device */
	setMessageData( &msgData, &theTime, sizeof( time_t ) );
	status = krnlSendMessage( cryptDevice, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_TIME );
	if( cryptStatusError( status ) && cryptDevice != SYSTEM_OBJECT_HANDLE )
		{
		/* We couldn't get the time from a crypto token, fall back to the
		   system device */
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_TIME );
		}
	if( cryptStatusError( status ) || \
		( theTime == ( time_t ) -1 ) || ( theTime <= MIN_TIME_VALUE ) )
		{
		DEBUG_DIAG(( "Error: No time source available" ));
		assert( DEBUG_WARN );
		return( 0 );
		}
	return( theTime );
	}

/****************************************************************************
*																			*
*								Timer Functions 							*
*																			*
****************************************************************************/

/* Monotonic timer interface that protects against the system clock being 
   changed during a timed operation like network I/O.  Even without 
   deliberate fiddling with the system clock, a timeout during a DST switch 
   can cause something like a 5s wait to turn into a 1hr 5s wait, so we have 
   to abstract the standard time API into a monotonic time API.  Since these 
   functions are purely peripheral to other operations (for example handling 
   timeouts for network I/O) they never fail but simply return good-enough 
   results if there's a problem (although they assert in debug mode).  This 
   is because we don't want to abort a network session just because we've 
   detected some trivial clock irregularity.

   The way this works is that we record the following information for each
   timing interval:

			(endTime - \
				timeRemaining)			 endTime
	................+-----------------------+...............
		^			|						|		^
	currentTime		|<--- timeRemaining --->|	currentTime'
		  ....<------- origTimeout -------->|

   When currentTime falls outside the timeRemaining interval we know that a 
   clock change has occurred and can try and correct it.  Moving forwards
   by an unexpected amount is a bit more tricky than moving back because 
   it's hard to define "unexpected", so we use an estimation method that 
   detects the typical reasons for a clock leap (DST adjust) without 
   yielding false positives */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheck( const MONOTIMER_INFO *timerInfo )
	{
	/* Make sure that the basic timer values are within bounds.  We can't
	   check endTime for a maximum range value since it's a time_t */
	if( timerInfo->origTimeout < 0 || \
		timerInfo->origTimeout > MAX_INTLENGTH || \
		timerInfo->timeRemaining < 0 || \
		timerInfo->timeRemaining > MAX_INTLENGTH || \
		timerInfo->endTime < 0 )
		return( FALSE );

	/* Make sure that time ranges are withing bounds.  This can generally 
	   only happen when a time_t over/underflow has occurred */
	if( timerInfo->endTime < timerInfo->timeRemaining || \
		timerInfo->origTimeout < timerInfo->timeRemaining )
		return( FALSE );

	return( TRUE );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void handleTimeOutOfBounds( INOUT MONOTIMER_INFO *timerInfo )
	{
	assert( isWritePtr( timerInfo, sizeof( MONOTIMER_INFO ) ) );

	DEBUG_DIAG(( "time_t underflow/overflow has occurred" ));
	assert( DEBUG_WARN );

	/* We've run into an overflow condition in the calculations that we've
	   performed on a time_t, this is a bit tricky to handle because we 
	   can't just give up on (say) performing network I/O just because we 
	   can't reliably set a timeout.  The best that we can do is warn in 
	   debug mode and set a zero timeout so that at least one lot of I/O 
	   will still take place */
	timerInfo->origTimeout = timerInfo->timeRemaining = 0;
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN correctMonoTimer( INOUT MONOTIMER_INFO *timerInfo,
								 const time_t currentTime )
	{
	BOOLEAN needsCorrection = FALSE;

	assert( isWritePtr( timerInfo, sizeof( MONOTIMER_INFO ) ) );

	/* If a time_t over/underflow has occurred, make a best-effort attempt 
	   to recover */
	if( !sanityCheck( timerInfo ) )
		{
		handleTimeOutOfBounds( timerInfo );
		return( FALSE );
		}

	/* If the clock has been rolled back to before the start time, we need 
	   to correct this.  The range check for endTime vs. timeRemaining has
	   already been done as part of the sanity check */
	if( currentTime < timerInfo->endTime - timerInfo->timeRemaining )
		needsCorrection = TRUE;
	else
		{
		/* If we're past the timer end time, check to see whether it's 
		   jumped by a suspicious amount.  If we're more than 30 minutes 
		   past the timeout (which will catch things like DST changes) and 
		   the initial timeout was less than the change (to avoid a false 
		   positive if we've been waiting > 30 minutes for a legitimate 
		   timeout), we need to correct this.  This can still fail if (for 
		   example) we have a relatively short timeout and we're being run
		   in a VM that gets suspended for more than 30 minutes and then
		   restarted, but by then any peer communicating with us should have
		   long since given up waiting for a response and timed out the
		   connection.  In any case someone fiddling with suspending 
		   processes in this manner, which will cause problems for anything
		   doing network I/O, should be prepared to handle any problems that
		   arise, for example by ensuring that current network I/O has
		   completed before suspending the process */
		if( currentTime > timerInfo->endTime )
			{
			const time_t delta = currentTime - timerInfo->endTime;

			if( ( delta < 0 || delta > ( 30 * 60 ) ) && \
				timerInfo->origTimeout < delta )
				needsCorrection = TRUE;
			}
		}
	if( !needsCorrection )
		return( TRUE );

	/* The time information has been changed, correct the recorded time
	   information for the new time */
	if( currentTime >= ( MAX_INTLENGTH - timerInfo->timeRemaining ) )
		{
		DEBUG_DIAG(( "Invalid monoTimer time correction period" ));
		assert( DEBUG_WARN );
		handleTimeOutOfBounds( timerInfo );
		return( FALSE );
		}
	timerInfo->endTime = currentTime + timerInfo->timeRemaining;
	if( timerInfo->endTime < currentTime || \
		timerInfo->endTime < currentTime + max( timerInfo->timeRemaining,
												timerInfo->origTimeout ) )
		{
		/* There's a problem with the time calculations, handle the overflow
		   condition and tell the caller not to try anything further */
		handleTimeOutOfBounds( timerInfo );
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setMonoTimer( INOUT MONOTIMER_INFO *timerInfo, 
				  IN_INT const int duration )
	{
	const time_t currentTime = getApproxTime();
	BOOLEAN initOK;

	assert( isWritePtr( timerInfo, sizeof( MONOTIMER_INFO ) ) );
	
	REQUIRES( duration >= 0 && duration < MAX_INTLENGTH );

	memset( timerInfo, 0, sizeof( MONOTIMER_INFO ) );
	if( currentTime >= ( MAX_INTLENGTH - duration ) )
		{
		DEBUG_DIAG(( "Invalid monoTimer time period" ));
		assert( DEBUG_WARN );
		handleTimeOutOfBounds( timerInfo );
		return( CRYPT_OK );
		}
	timerInfo->endTime = currentTime + duration;
	timerInfo->timeRemaining = timerInfo->origTimeout = duration;
	initOK = correctMonoTimer( timerInfo, currentTime );
	ENSURES( initOK );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void extendMonoTimer( INOUT MONOTIMER_INFO *timerInfo, 
					  IN_INT const int duration )
	{
	const time_t currentTime = getApproxTime();

	assert( isWritePtr( timerInfo, sizeof( MONOTIMER_INFO ) ) );
	
	REQUIRES_V( duration >= 0 && duration < MAX_INTLENGTH );

	/* Correct the timer for clock skew if required */
	if( !correctMonoTimer( timerInfo, currentTime ) )
		return;

	/* Extend the monotonic timer's timeout interval to allow for further
	   data to be processed */
	if( timerInfo->origTimeout >= ( MAX_INTLENGTH - duration ) || \
		timerInfo->endTime >= ( MAX_INTLENGTH - duration ) || \
		timerInfo->endTime < currentTime )
		{
		DEBUG_DIAG(( "Invalid monoTimer time period extension" ));
		assert( DEBUG_WARN );
		handleTimeOutOfBounds( timerInfo );
		return;
		}
	timerInfo->origTimeout += duration;
	timerInfo->endTime += duration;
	timerInfo->timeRemaining = timerInfo->endTime - currentTime;

	/* Re-correct the timer in case overflow occurred */
	( void ) correctMonoTimer( timerInfo, currentTime );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkMonoTimerExpiryImminent( INOUT MONOTIMER_INFO *timerInfo,
									  IN_INT const int timeLeft )
	{
	const time_t currentTime = getApproxTime();
	time_t timeRemaining;

	assert( isWritePtr( timerInfo, sizeof( MONOTIMER_INFO ) ) );

	REQUIRES_B( timeLeft >= 0 && timeLeft < MAX_INTLENGTH );

	/* If the timeout has expired, don't try doing anything else */
	if( timerInfo->timeRemaining <= 0 )
		return( TRUE );

	/* Correct the monotonic timer for clock skew if required */
	if( !correctMonoTimer( timerInfo, currentTime ) )
		return( TRUE );

	/* Check whether the time will expire within timeLeft seconds */
	if( timerInfo->endTime < currentTime )
		{
		DEBUG_DIAG(( "Invalid monoTimer expiry time period" ));
		assert( DEBUG_WARN );
		handleTimeOutOfBounds( timerInfo );
		return( TRUE );
		}
	timeRemaining = timerInfo->endTime - currentTime;
	if( timeRemaining > timerInfo->timeRemaining )
		{
		handleTimeOutOfBounds( timerInfo );
		timeRemaining = 0;
		}
	timerInfo->timeRemaining = timeRemaining;
	return( ( timerInfo->timeRemaining < timeLeft ) ? TRUE : FALSE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkMonoTimerExpired( INOUT MONOTIMER_INFO *timerInfo )
	{
	return( checkMonoTimerExpiryImminent( timerInfo, 0 ) );
	}
