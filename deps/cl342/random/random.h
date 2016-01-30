/****************************************************************************
*																			*
*						  cryptlib Randomness Interface						*
*						Copyright Peter Gutmann 1995-2006					*
*																			*
****************************************************************************/

#ifndef _RANDOM_DEFINED

#define _RANDOM_DEFINED

/****************************************************************************
*																			*
*					Randomness Polling Internal Functions					*
*																			*
****************************************************************************/

/* Some systems systems require special-case initialisation to allow
   background randomness gathering, where this doesn't apply the routines to
   do this are nop'd out */

#if defined( __WIN32__ ) || defined( __WINCE__ )
  void initRandomPolling( void );
  void endRandomPolling( void );
  CHECK_RETVAL \
  int waitforRandomCompletion( const BOOLEAN force );
#elif defined( __UNIX__ ) && \
	  !( defined( __MVS__ ) || defined( __TANDEM_NSK__ ) || \
		 defined( __TANDEM_OSS__ ) )
  void initRandomPolling( void );
  void endRandomPolling( void );
  CHECK_RETVAL \
  int waitforRandomCompletion( const BOOLEAN force );
#else
  #define initRandomPolling()
  #define endRandomPolling()
  #define waitforRandomCompletion( dummy )	CRYPT_OK
#endif /* !( __WIN32__ || __UNIX__ ) */

/* On Unix systems the randomness pool may be duplicated at any point if
   the process forks (qualis pater, talis filius), so we need to perform a
   complex check to make sure that we're running with a unique copy of the
   pool contents rather than a clone of data held in another process.  The
   following function checks whether we've forked or not, which is used as a
   signal to adjust the pool contents */

#if defined( __UNIX__ ) && \
	!( defined( __MVS__ ) || defined( __TANDEM_NSK__ ) || \
	   defined( __TANDEM_OSS__ ) )
  CHECK_RETVAL_BOOL \
  BOOLEAN checkForked( void );
#else
  #define checkForked()		FALSE
#endif /* __UNIX__ */

/* Prototypes for functions in the OS-specific randomness polling routines */

void slowPoll( void );
void fastPoll( void );

/* In order to make it easier to add lots of arbitrary-sized random data
   values, we make the following functions available to the polling code to
   implement a clustered-write mechanism for small data quantities.  These
   add an integer, long, or (short) string value to a buffer and send it
   through to the system device when the buffer is full.  Using the
   intermediate buffer ensures that we don't have to send a message to the
   device for every bit of data added

   The caller declares a state variable of type RANDOM_STATE, calls
   initRandomData() to initialise it, calls addRandomData() for each
   consecutive piece of data to add to the buffer, and finally calls
   endRandomData() to flush the data through to the system device.  The
   state pointer is declared as a void * because to the caller it's an
   opaque memory block while to the randomData routines it's structured
   storage */

typedef BYTE RANDOM_STATE[ 128 ];

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initRandomData( INOUT void *statePtr, 
					IN_BUFFER( maxSize ) void *buffer, 
					IN_LENGTH_SHORT_MIN( 16 ) const int maxSize );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addRandomData( INOUT void *statePtr, 
				   IN_BUFFER( valueLength ) const void *value, 
				   IN_LENGTH_SHORT const int valueLength );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addRandomLong( INOUT void *statePtr, const long value );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int endRandomData( INOUT void *statePtr, \
				   IN_RANGE( 0, 100 ) const int quality );

/* We also provide an addRandomValue() to make it easier to add function
   return values for getXYZ()-style system calls that return system info as
   their return value, for which we can't pass an address to addRandomData()
   unless we copy it to a temporary var first */

#define addRandomValue( statePtr, value ) \
		addRandomLong( statePtr, ( long ) value )

/****************************************************************************
*																			*
*					Randomness External Interface Functions					*
*																			*
****************************************************************************/

/* Prototypes for functions in random.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initRandomInfo( OUT_OPT_PTR void **randomInfoPtrPtr );
STDC_NONNULL_ARG( ( 1 ) ) \
void endRandomInfo( INOUT void **randomInfoPtrPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int addEntropyData( INOUT void *randomInfoPtr, 
					IN_BUFFER( length ) const void *buffer, 
					IN_LENGTH const int length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addEntropyQuality( INOUT void *randomInfoPtr, 
					   IN_RANGE( 1, 100 ) const int quality );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getRandomData( INOUT void *randomInfoPtr, 
				   OUT_BUFFER_FIXED( length ) void *buffer, 
				   IN_RANGE( 1, MAX_RANDOM_BYTES ) const int length );

#endif /* _RANDOM_DEFINED */
