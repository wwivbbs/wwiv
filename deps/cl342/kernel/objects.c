/****************************************************************************
*																			*
*							Kernel Object Management						*
*						Copyright Peter Gutmann 1997-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/* The initial allocation size of the object table, but default 1024 
   entries.  In memory-starved environments we limit the size, in general 
   these are embedded systems or single-tasking OSes that aren't going to 
   need many objects anyway */

#if defined( CONFIG_CONSERVE_MEMORY )
  #define OBJECT_TABLE_ALLOCSIZE	128
  #define INITIAL_LFSRPOLY			0x83
#elif defined( CONFIG_NO_OBJECTS )
  #define OBJECT_TABLE_ALLOCSIZE	CONFIG_NO_OBJECTS
  #if CONFIG_NO_OBJECTS == 512
	#define INITIAL_LFSRPOLY		0x211
  #elif CONFIG_NO_OBJECTS == 1024
	#define INITIAL_LFSRPOLY		0x409
  #elif CONFIG_NO_OBJECTS == 2048
	#define INITIAL_LFSRPOLY		0x805
  #elif CONFIG_NO_OBJECTS == 4096
	#define INITIAL_LFSRPOLY		0x1053
  #elif CONFIG_NO_OBJECTS == 8192
	#define INITIAL_LFSRPOLY		0x201B
  #elif CONFIG_NO_OBJECTS == 16384
	#define INITIAL_LFSRPOLY		0x402B
  #else
	#error CONFIG_NO_OBJECTS must be 512, 1K, 2K, 4K, 8K, or 16K
  #endif /* CONFIG_NO_OBJECTS settings */
#else
  #define OBJECT_TABLE_ALLOCSIZE	1024
  #define INITIAL_LFSRPOLY			0x409
#endif /* Memory-starved environments */

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

/* A template used to initialise object table entries.  Some of the entries
   are either object handles that have to be set to CRYPT_ERROR or values
   for which 0 is significant (so they're set to CRYPT_UNUSED), because of
   this we can't just memset the entry to all zeroes */

static const OBJECT_INFO FAR_BSS OBJECT_INFO_TEMPLATE = {
	OBJECT_TYPE_NONE, SUBTYPE_NONE,	/* Type, subtype */
	NULL, 0,					/* Object data and size */
	OBJECT_FLAG_INTERNAL | OBJECT_FLAG_NOTINITED,	/* Flags */
	0,							/* Action flags */
	1, 0, 0,					/* Int.and ext. ref.counts, lock count */
#ifdef USE_THREADS
	THREAD_INITIALISER,			/* Lock owner */
#endif /* USE_THREADS */
	0,							/* Unique ID */
	CRYPT_UNUSED, CRYPT_UNUSED,	/* Forward count, usage count */
#ifdef USE_THREADS
	THREAD_INITIALISER,			/* Owner */
#endif /* USE_THREADS */
	NULL,						/* Message function */
	CRYPT_ERROR,				/* Owner */
	CRYPT_ERROR, CRYPT_ERROR	/* Dependent object/device */
	};

/* A template used to initialise the object allocation state data */

static const OBJECT_STATE_INFO FAR_BSS OBJECT_STATE_INFO_TEMPLATE = {
	OBJECT_TABLE_ALLOCSIZE,		/* Mask for LFSR output */
	INITIAL_LFSRPOLY,			/* LFSR polynomial */
	-1							/* Initial-1'th object handle */
	};

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

/* Create and destroy the object table.  The destroy process is handled in
   two stages, the first of which is called fairly early in the shutdown
   process to destroy any remaining objects, and the second which is called
   at the end of the shutdown when the kernel data is being deleted.  This
   is because some of the objects are tied to things like external devices,
   and deleting them at the end when everything else has been shut down
   isn't possible */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initObjects( INOUT KERNEL_DATA *krnlDataPtr )
	{
	int i, status;

	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* Perform a consistency check on various things that need to be set
	   up in a certain way for things to work properly */
	static_assert( OBJECT_TABLE_ALLOCSIZE >= 64, \
				   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.type == OBJECT_TYPE_NONE, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.subType == 0, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.objectPtr == NULL, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.objectSize == 0, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.flags == \
							( OBJECT_FLAG_INTERNAL | OBJECT_FLAG_NOTINITED ), \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.actionFlags == 0, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.forwardCount == CRYPT_UNUSED, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.usageCount == CRYPT_UNUSED, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.owner == CRYPT_ERROR, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.dependentDevice == CRYPT_ERROR, \
					   "Object table param" );
	static_assert_opt( OBJECT_INFO_TEMPLATE.dependentObject == CRYPT_ERROR, \
					   "Object table param" );
	static_assert( SYSTEM_OBJECT_HANDLE == NO_SYSTEM_OBJECTS - 2, \
				   "Object table param" );
	static_assert( DEFAULTUSER_OBJECT_HANDLE == NO_SYSTEM_OBJECTS - 1, \
				   "Object table param" );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	/* Allocate and initialise the object table */
	krnlData->objectTable = \
			clAlloc( "initObjectTable",
					 OBJECT_TABLE_ALLOCSIZE * sizeof( OBJECT_INFO ) );
	if( krnlData->objectTable == NULL )
		return( CRYPT_ERROR_MEMORY );
	for( i = 0; i < OBJECT_TABLE_ALLOCSIZE; i++ )
		krnlData->objectTable[ i ] = OBJECT_INFO_TEMPLATE;
	krnlData->objectTableSize = OBJECT_TABLE_ALLOCSIZE;
	krnlData->objectStateInfo = OBJECT_STATE_INFO_TEMPLATE;

	/* Initialise object-related information.  This isn't strictly part of
	   the object table but is used to assign unique ID values to objects
	   within the table, since table entries (object handles) may be reused
	   as objects are destroyed and new ones created in their place */
	krnlData->objectUniqueID = 0;

	/* Initialize any data structures required to make the object table
	   thread-safe */
	MUTEX_CREATE( objectTable, status );
	if( cryptStatusError( status ) )
		{
		clFree( "initObjectTable", krnlData->objectTable );
		retIntError();
		}

	/* Postconditions */
	ENSURES( krnlData->objectTable != NULL );
	ENSURES( krnlData->objectTableSize == OBJECT_TABLE_ALLOCSIZE );
	FORALL( i, 0, OBJECT_TABLE_ALLOCSIZE, \
			!memcmp( &krnlData->objectTable[ i ], \
					 &OBJECT_INFO_TEMPLATE, sizeof( OBJECT_INFO ) ) );
	ENSURES( krnlData->objectStateInfo.lfsrMask == OBJECT_TABLE_ALLOCSIZE && \
			 krnlData->objectStateInfo.lfsrPoly == INITIAL_LFSRPOLY && \
			 krnlData->objectStateInfo.objectHandle == SYSTEM_OBJECT_HANDLE - 1 );
	ENSURES( krnlData->objectUniqueID == 0 );

	return( CRYPT_OK );
	}

void endObjects( void )
	{
	/* Hinc igitur effuge */
	MUTEX_LOCK( objectTable );
	zeroise( krnlData->objectTable,
			 krnlData->objectTableSize * sizeof( OBJECT_INFO ) );
	clFree( "endObjectTable", krnlData->objectTable );
	krnlData->objectTable = NULL;
	krnlData->objectTableSize = 0;
	krnlData->objectUniqueID = 0;
	MUTEX_UNLOCK( objectTable );
	MUTEX_DESTROY( objectTable );
	krnlData = NULL;
	}

/****************************************************************************
*																			*
*							Object Table Management							*
*																			*
****************************************************************************/

/* Destroy an object's instance data and object table entry */

CHECK_RETVAL \
int destroyObjectData( IN_HANDLE const int objectHandle )
	{
	OBJECT_INFO *objectInfoPtr;
	int status;

	/* Precondition: It's a valid object */
	REQUIRES( isValidObject( objectHandle ) );

	objectInfoPtr = &krnlData->objectTable[ objectHandle ];

	/* Inner precondition: There's valid object data present */
	REQUIRES( objectInfoPtr->objectPtr != NULL && \
			  objectInfoPtr->objectSize > 0 && \
			  objectInfoPtr->objectSize < MAX_BUFFER_SIZE );

	/* Destroy the object's data and clear the object table entry */
	if( objectInfoPtr->flags & OBJECT_FLAG_SECUREMALLOC )
		{
		status = krnlMemfree( &objectInfoPtr->objectPtr );
		ENSURES( cryptStatusOK( status ) );
		}
	else
		{
		/* Mors ultima linea rerum est */
		zeroise( objectInfoPtr->objectPtr, objectInfoPtr->objectSize );
		clFree( "destroyObjectData", objectInfoPtr->objectPtr );
		}
	krnlData->objectTable[ objectHandle ] = OBJECT_INFO_TEMPLATE;

	return( CRYPT_OK );
	}

/* Destroy an object.  This is only called when cryptlib is shutting down,
   normally objects are destroyed directly in response to messages */

CHECK_RETVAL \
static int destroyObject( IN_HANDLE const int objectHandle )
	{
	const OBJECT_INFO *objectInfoPtr;
	MESSAGE_FUNCTION messageFunction;

	/* Precondition: It's a valid object */
	REQUIRES( isValidObject( objectHandle ) );

	objectInfoPtr = &krnlData->objectTable[ objectHandle ];
	messageFunction = objectInfoPtr->messageFunction;

	/* If there's no object present at this position, just clear the entry 
	   (it should be cleared anyway) */
	if( messageFunction == NULL )
		{
		krnlData->objectTable[ objectHandle ] = OBJECT_INFO_TEMPLATE;
		return( CRYPT_OK );
		}

	/* Destroy the object and its object table entry */
	if( objectInfoPtr->type == OBJECT_TYPE_DEVICE )
		{
		MESSAGE_FUNCTION_EXTINFO messageExtInfo;

		/* Device objects are unlockable so we have to pass in extended 
		   information to handle this */
		initMessageExtInfo( &messageExtInfo, objectInfoPtr->objectPtr );
		objectInfoPtr->messageFunction( &messageExtInfo, 
										MESSAGE_DESTROY, NULL, 0 );
		}
	else
		{
		objectInfoPtr->messageFunction( objectInfoPtr->objectPtr, 
										MESSAGE_DESTROY, NULL, 0 );
		}
	return( destroyObjectData( objectHandle ) );
	}

/* Destroy all objects at a given nesting level */

CHECK_RETVAL \
static int destroySelectedObjects( IN_RANGE( 1, 3 ) const int currentDepth )
	{
	const OBJECT_INFO *objectTable = krnlData->objectTable;
	int objectHandle, status = CRYPT_OK;

	/* Preconditions: We're destroying objects at a fixed depth */
	REQUIRES( currentDepth >= 1 && currentDepth <= 3 );

	for( objectHandle = NO_SYSTEM_OBJECTS; \
		 objectHandle < krnlData->objectTableSize && \
			objectHandle < MAX_OBJECTS; \
		 objectHandle++ )
		{
		const int dependentObject = \
						objectTable[ objectHandle ].dependentObject;
		int depth = 1;

		/* If there's nothing there, continue */
		if( objectTable[ objectHandle ].objectPtr == NULL )
			continue;

		/* There's an object still present, determine its nesting depth.
		   Dependent devices are terminal so we only follow the path down for
		   dependent objects */
		if( isValidObject( dependentObject ) )
			{
			if( isValidObject( objectTable[ dependentObject ].dependentObject ) )
				depth = 3;
			else
				{
				if( isValidObject( objectTable[ dependentObject ].dependentDevice ) )
					depth = 2;
				}
			}
		else
			{
			if( isValidObject( objectTable[ objectHandle ].dependentDevice ) )
				depth = 2;
			}

		/* If the nesting level of the object matches the current level,
		   destroy it.  We unlock the object table around the access to
		   prevent remaining active objects from blocking the shutdown (the
		   activation of krnlIsExiting() takes care of any other messages 
		   that may arrive during this process).

		   "For death is come up into our windows, and it is entered into
		    our palaces, to cut off the children from the without"
			-- Jeremiah 9:21 */
		if( depth >= currentDepth )
			{
			DEBUG_PRINT(( "Destroying leftover %s.\n", 
						  getObjectDescriptionNT( objectHandle ) ));
			objectTable = NULL;
			MUTEX_UNLOCK( objectTable );
			krnlSendNotifier( objectHandle, IMESSAGE_DESTROY );
			status = CRYPT_ERROR_INCOMPLETE;
			MUTEX_LOCK( objectTable );
			objectTable = krnlData->objectTable;
			}
		}
	ENSURES( objectHandle <= MAX_OBJECTS );

	return( status );
	}

/* Destroy all objects (homini necesse est mori) */

CHECK_RETVAL \
int destroyObjects( void )
	{
	int depth, objectHandle, localStatus, status DUMMY_INIT;

	/* Preconditions: We either didn't complete the initialisation and are 
	   shutting down during a krnlBeginInit(), or we've completed 
	   initialisation and are shutting down after a krnlBeginShutdown() */
	REQUIRES( ( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				krnlData->shutdownLevel == SHUTDOWN_LEVEL_NONE ) || \
			  ( krnlData->initLevel == INIT_LEVEL_KRNLDATA && \
				krnlData->shutdownLevel == SHUTDOWN_LEVEL_THREADS ) );

	/* Indicate that we're shutting down the object handling.  From now on
	   all messages other than object-destruction ones will be rejected by
	   the kernel.  This is needed in order to have any remaining active
	   objects exit quickly, since we don't want them to block the shutdown.
	   Note that we do this before we lock the object table to encourage
	   anything that might have the table locked to exit quickly once we try
	   and lock the table */
	krnlData->shutdownLevel = SHUTDOWN_LEVEL_MESSAGES;

	/* Lock the object table to ensure that other threads don't try to
	   access it */
	MUTEX_LOCK( objectTable );

	/* Destroy all system objects except the root system object ("The death
	   of God left the angels in a strange position" - Donald Barthelme, "On
	   Angels").  We have to do this before we destroy any unclaimed
	   leftover objects because some of them may depend on system objects,
	   if the system objects aren't destroyed they'll be erroneously flagged
	   as leftover objects.  The destruction is done explicitly by invoking
	   the object's message function directly because the message dispatcher
	   checks to make sure that they're never destroyed through a standard
	   message, which would indicate a programming error */
	for( objectHandle = SYSTEM_OBJECT_HANDLE + 1;
		 objectHandle < NO_SYSTEM_OBJECTS; objectHandle++ )
		{
		/* In the extremely unlikely event that we encounter an error during 
		   the creation of the system object, no other objects will have been 
		   created, so before we try and destroy an object we make sure that 
		   it's actually been created */
		if( !isValidObject( objectHandle ) )
			continue;

		status = destroyObject( objectHandle );
		ENSURES_MUTEX( cryptStatusOK( status ), objectTable );
		}

	/* Postcondition: All system objects except the root system object have
	   been destroyed */
	FORALL( i, SYSTEM_OBJECT_HANDLE + 1, NO_SYSTEM_OBJECTS,
			!memcmp( &krnlData->objectTable[ i ], &OBJECT_INFO_TEMPLATE, \
					 sizeof( OBJECT_INFO ) ) );

	/* Delete any unclaimed leftover objects.  This is rather more complex
	   than just rumbling through deleting each object we find since some
	   objects have dependent objects underneath them, and deleting the
	   lower-level object causes problems when we later delete their parents
	   (the code handles it cleanly, but we get a kernel trap warning us that
	   we're trying to delete a non-present object).  Because of this we have
	   to delete the objects in order of depth, first all three-level objects
	   (e.g. cert -> context -> device), then all two-level objects, and
	   finally all one-level objects.  This means that we can never delete
	   another object out from under a dependent object */
	for( depth = 3; depth > 0; depth-- )
		{
		localStatus = destroySelectedObjects( depth );
		if( cryptStatusError( localStatus ) )
			status = localStatus;
		}

	/* Postcondition: All objects except the root system object have been
	   destroyed */
	FORALL( i, SYSTEM_OBJECT_HANDLE + 1, krnlData->objectTableSize,
			!memcmp( &krnlData->objectTable[ i ], &OBJECT_INFO_TEMPLATE, \
					 sizeof( OBJECT_INFO ) ) );

	/* Finally, destroy the system root object.  We need to preserve the 
	   possible error status from cleaning up any leftover objects so we use 
	   a local status value to get the destroy-object results */
	localStatus = destroyObject( SYSTEM_OBJECT_HANDLE );
	ENSURES_MUTEX( cryptStatusOK( localStatus ), objectTable );

	/* Unlock the object table to allow access by other threads */
	MUTEX_UNLOCK( objectTable );

	return( status );
	}

/****************************************************************************
*																			*
*							Object Creation/Destruction						*
*																			*
****************************************************************************/

/* Create a new object.  This function has to be very careful about locking
   to ensure that another thread can't manipulate the newly-created object
   while it's in an indeterminate state.  To accomplish this it locks the
   object table and tries to create the new object.  If this succeeds it sets
   the OBJECT_FLAG_NOTINITED flag pending completion of the object's
   initialisation by the caller, unlocks the object table, and returns
   control to the caller.  While the object is in this state, the kernel
   will allow it to process only two message types, either a notification
   from the caller that the init stage is complete (which sets the object's
   state to OK), or a destroy object message, which sets the
   OBJECT_FLAG_SIGNALLED flag pending arrival of the init complete
   notification, whereupon the object is immediately destroyed.  The state
   diagram for this is:
									 State
						  Notinited			Signalled
			--------+-------------------+-----------------
			-> OK	| state -> OK,		| Msg -> Destroy
					| ret( OK )			|
	Msg.	Destroy	| state -> Sig'd,	| state -> Sig'd,
					| ret( OK )			| ret( OK )
			CtrlMsg	| process as usual	| process as usual
			NonCtrl	| ret( Notinited )	| ret( Sig'd )

   The initialisation process for an object is therefore:

	status = krnlCreateObject( ... );
	if( cryptStatusError( status ) )
		return( status );

	// Complete object-specific initialisation
	initStatus = ...;

	status = krnlSendMessage( ..., state -> CRYPT_OK );
	return( ( cryptStatusError( initStatus ) ? initStatus : status );

   If the object is destroyed during the object-specific initialisation
   (either by the init code when an error is encountered or due to an
   external signal), the destroy is deferred until the change state message
   at the end occurs.  If a destroy is pending, the change state is converted
   to a destroy and the newly-created object is destroyed.

   This mechanism ensures that the object table is only locked for a very
   short time (typically for only a few lines of executed code in the create
   object function) so that slow initialisation (for example of keyset
   objects associated with network links) can't block other objects.

   In addition to the locking, we need to be careful with how we create new
   objects because if we just allocate handles sequentially and reuse handles
   as soon as possible, an existing object could be signalled and a new one
   created in its place without the caller or owning object realizing that
   they're now working with a different object (although the kernel can tell
   them apart because it maintains an internal unique ID for each object).
   Unix systems handle this by always incrementing pids and assuming that
   there won't be any problems when they wrap, we do the same thing but in
   addition allocate handles in a non-sequential manner using an LFSR to
   step through the object table.  There's no strong reason for this apart
   from helping disabuse users of the notion that any cryptlib objects have
   stdin/stdout-style fixed handles, but it only costs a few extra clocks so
   we may as well do it.
   
   There is one case in which we can still get handle reuse and that's then 
   the object table is heavily loaded but not quite enough to trigger a 
   resize.  For example with a table of size 1024 if the caller creates 1023
   objects and then repeatedly creates and destroys a final object it'll 
   always be reassigned the same handle if no other objects are destroyed in
   the meantime.  This is rather unlikely unless the caller is using 
   individual low-level objects (all of the high-level objects create 
   object-table churn through their dependent objects) so it's not worth
   adding special-case handling for it */

CHECK_RETVAL \
static int findFreeObjectEntry( int value )
	{
	BOOLEAN freeEntryAvailable = FALSE;
	int oldValue = value, iterations;

	/* Preconditions: We're starting with a valid object handle, and it's not
	   a system object */
	REQUIRES( isValidHandle( value ) );
	REQUIRES( value >= NO_SYSTEM_OBJECTS );

	/* Step through the entire table looking for a free entry */
	for( iterations = 0; isValidHandle( value ) && \
						 iterations < krnlData->objectTableSize && \
						 iterations < MAX_OBJECTS; 
		 iterations++ )
		{
		/* Invariant: We're not stuck in an endless loop */
		ENSURES( iterations < krnlData->objectTableSize );

		/* Get the next value: Multiply by x and reduce by the polynomial */
		value <<= 1;
		if( value & krnlData->objectStateInfo.lfsrMask )
			value ^= krnlData->objectStateInfo.lfsrPoly;

		/* Invariant: We're still within the object table */
		ENSURES( isValidHandle( value ) );

		/* If we've found a free object or we've covered the entire table,
		   exit.  We do this check after we update the value rather than as
		   part of the loop test to ensure that we always progress to a new
		   object handle whenever we call this function.  If we did the
		   check as part of the loop test then deleting and creating an
		   object would result in the handle of the deleted object being
		   re-assigned to the new object */
		if( isFreeObject( value ) )
			{
			/* We've found a free entry, we're done */
			freeEntryAvailable = TRUE;
			break;
			}
		if( value == oldValue )
			{
			/* We've covered the entire object table, we need to expand it
			   to make more room */
			break;
			}
		}
	ENSURES( iterations < krnlData->objectTableSize && \
			 iterations < MAX_OBJECTS )

	/* Postcondition: We're still within the object table */
	ENSURES( isValidHandle( value ) );

	/* If we didn't find a free entry, tell the caller that the tank is 
	   full */
	if( !freeEntryAvailable )
		{
		/* Postcondition: We tried all locations and there are no free slots
		   available (or, vastly less likely, an internal error has
		   occurred) */
		ENSURES( iterations == krnlData->objectTableSize - 2 );
		FORALL( i, 0, krnlData->objectTableSize, \
				!isFreeObject( i ) );

		return( OK_SPECIAL );
		}

	/* Postconditions: We found a handle to a free slot */
	ENSURES( isFreeObject( value ) );

	return( value );
	}

CHECK_RETVAL \
static int expandObjectTable( void )
	{
	static const long FAR_BSS lfsrPolyTable[] = \
							{	  0x83,		 0x11D,	   0x211,	  0x409,
								 0x805,		0x1053,   0x201B,	 0x402B,
								0x8003L,  0x1002DL,  0x20009L,  0x40027L,
							   0x80027L, 0x100009L, 0x200005L, 0x400003L,
									 0L,		0L };
	OBJECT_INFO *newTable;
	int objectHandle, i, status;
	ORIGINAL_INT_VAR( oldLfsrPoly, krnlData->objectStateInfo.lfsrPoly );

	/* If we're already at the maximum number of allowed objects, don't
	   create any more.  This prevents both accidental runaway code that
	   creates huge numbers of objects and DoS attacks */
	if( krnlData->objectTableSize <= 0 || \
		krnlData->objectTableSize > MAX_OBJECTS / 2 )
		{
		DEBUG_DIAG(( "Maximum object table size of %d objects reached",
					 MAX_OBJECTS ));
		return( CRYPT_ERROR_OVERFLOW );
		}
	DEBUG_DIAG(( "Expanding object table from %d to %d entries",
				 krnlData->objectTableSize, krnlData->objectTableSize * 2 ));

	/* Precondition: We haven't exceeded the maximum number of objects */
	REQUIRES( krnlData->objectTableSize > 0 && \
			  krnlData->objectTableSize <= MAX_OBJECTS / 2 );

	/* Expand the table */
	newTable = clDynAlloc( "krnlCreateObject", \
						   ( krnlData->objectTableSize * 2 ) * \
								sizeof( OBJECT_INFO ) );
	if( newTable == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Copy the information across to the new table, set up the newly-
	   allocated entries, and clear the old table */
	memcpy( newTable, krnlData->objectTable,
			krnlData->objectTableSize * sizeof( OBJECT_INFO ) );
	for( i = krnlData->objectTableSize; 
		 i < krnlData->objectTableSize * 2 && i < MAX_OBJECTS; i++ )
		newTable[ i ] = OBJECT_INFO_TEMPLATE;
	ENSURES( i <= MAX_OBJECTS );
	zeroise( krnlData->objectTable, \
			 krnlData->objectTableSize * sizeof( OBJECT_INFO ) );
	clFree( "krnlCreateObject", krnlData->objectTable );
	krnlData->objectTable = newTable;
	krnlData->objectTableSize *= 2;

	/* Add the new object at the end of the existing table */
	krnlData->objectStateInfo.lfsrMask <<= 1;
	for( i = 0; i < 16; i++ )
		{
		if( lfsrPolyTable[ i ] > krnlData->objectStateInfo.lfsrPoly )
			break;
		}
	ENSURES( i < 16 );
	krnlData->objectStateInfo.lfsrPoly = lfsrPolyTable[ i ];
	status = objectHandle = \
		findFreeObjectEntry( krnlData->objectStateInfo.objectHandle );
	if( cryptStatusError( status ) )
		return( status );

	/* Postcondition: We've moved on to the next LFSR polynomial value,
	   the LFSR output covers the entire table, and we now have room for
	   the new object */
	ENSURES( ( krnlData->objectStateInfo.lfsrPoly & ~0x7F ) == \
			 ( ( ORIGINAL_VALUE( oldLfsrPoly ) & ~0xFF ) << 1 ) );
	ENSURES( krnlData->objectStateInfo.lfsrMask == \
			 ( krnlData->objectStateInfo.lfsrPoly & ~0x7F ) );
	ENSURES( krnlData->objectTableSize == \
							krnlData->objectStateInfo.lfsrMask );
	ENSURES( isValidHandle( objectHandle ) );

	return( objectHandle );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 9 ) ) \
int krnlCreateObject( OUT_HANDLE_OPT int *objectHandle,
					  OUT_BUFFER_ALLOC_OPT( objectDataSize ) void **objectDataPtr, 
					  IN_LENGTH_SHORT const int objectDataSize,
					  IN_ENUM( OBJECT_TYPE ) const OBJECT_TYPE type, 
					  IN_ENUM( SUBTYPE ) const OBJECT_SUBTYPE subType,
					  IN_FLAGS_Z( CREATEOBJECT ) const int createObjectFlags, 
					  IN_HANDLE_OPT const CRYPT_USER owner,
					  IN_FLAGS_Z( ACTION_PERM ) const int actionFlags,
					  IN MESSAGE_FUNCTION messageFunction )
	{
	OBJECT_INFO objectInfo;
	OBJECT_STATE_INFO *objectStateInfo = &krnlData->objectStateInfo;
	OBJECT_SUBTYPE bitCount;
	int localObjectHandle;

	assert( isWritePtr( krnlData, sizeof( KERNEL_DATA ) ) );
	assert( isWritePtrConst( objectDataPtr, sizeof( void * ) ) );

	/* Preconditions (the subType check is just the standard hakmem bitcount
	   which ensures that we don't try and create multi-typed objects, the
	   sole exception to this rule is the default user object, which acts as
	   both a user and an SO object) */
	REQUIRES( objectDataSize > 16 && \
			  ( ( !( type == OBJECT_TYPE_CONTEXT && subType == SUBTYPE_CTX_PKC ) && \
				objectDataSize < MAX_INTLENGTH_SHORT ) || \
			  ( type == OBJECT_TYPE_CONTEXT && subType == SUBTYPE_CTX_PKC && \
				objectDataSize < MAX_INTLENGTH ) ) );
	REQUIRES( isValidType( type ) );
	REQUIRES( \
		( bitCount = ( subType & ~SUBTYPE_CLASS_MASK ) - \
					 ( ( ( subType & ~SUBTYPE_CLASS_MASK ) >> 1 ) & 033333333333L ) - \
					 ( ( ( subType & ~SUBTYPE_CLASS_MASK ) >> 2 ) & 011111111111L ) ) != 0 );
	REQUIRES( ( ( bitCount + ( bitCount >> 3 ) ) & 030707070707L ) % 63 == 1 );
	REQUIRES( createObjectFlags >= CREATEOBJECT_FLAG_NONE && \
			  createObjectFlags < CREATEOBJECT_FLAG_MAX );
	REQUIRES( !( createObjectFlags & \
				 ~( CREATEOBJECT_FLAG_SECUREMALLOC | CREATEOBJECT_FLAG_DUMMY | \
					CREATEOBJECT_FLAG_PERSISTENT ) ) );
	REQUIRES( owner == CRYPT_UNUSED || isValidHandle( owner ) );
	REQUIRES( actionFlags >= ACTION_PERM_FLAG_NONE && \
			  actionFlags <= ACTION_PERM_FLAG_MAX );
	REQUIRES( messageFunction != NULL );

	/* Clear return values */
	*objectHandle = CRYPT_ERROR;
	*objectDataPtr = NULL;

	/* If we haven't been initialised yet or we're in the middle of a
	   shutdown, we can't create any new objects */
	if( !isWritePtrConst( krnlData, sizeof( KERNEL_DATA ) ) || \
		krnlData->initLevel <= INIT_LEVEL_NONE )
		return( CRYPT_ERROR_NOTINITED );
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES )
		{
		DEBUG_DIAG(( "Can't create new objects during a shutdown" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_PERMISSION );
		}

	/* Allocate memory for the object and set up as much as we can of the 
	   object table entry (the remainder has to be set up inside the object-
	   table lock).  The object is always created as an internal object, 
	   it's up to the caller to make it externally visible.
	   
	   The apparently redundant clearning of objectInfo before overwriting
	   it with the object-info template is due to yet another gcc bug, in
	   this case in the x64 debug (-o0) build, for which it doesn't memcpy()
	   the template across but sets each field individually, leaving 
	   whatever padding bytes were present beforehand set to random values */
	if( createObjectFlags & CREATEOBJECT_FLAG_SECUREMALLOC )
		{
		int status = krnlMemalloc( objectDataPtr, objectDataSize );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		if( ( *objectDataPtr = clAlloc( "krnlCreateObject", \
										objectDataSize ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		}
	memset( *objectDataPtr, 0, objectDataSize );
	memset( &objectInfo, 0, sizeof( OBJECT_INFO ) ); /* See comment above */
	objectInfo = OBJECT_INFO_TEMPLATE;
	objectInfo.objectPtr = *objectDataPtr;
	objectInfo.objectSize = objectDataSize;
	if( createObjectFlags & CREATEOBJECT_FLAG_SECUREMALLOC )
		objectInfo.flags |= OBJECT_FLAG_SECUREMALLOC;
	objectInfo.owner = owner;
	objectInfo.type = type;
	objectInfo.subType = subType;
	objectInfo.actionFlags = actionFlags;
	objectInfo.messageFunction = messageFunction;

	/* Make sure that the kernel has been initialised and lock the object
	   table for exclusive access */
	MUTEX_LOCK( initialisation );
	MUTEX_LOCK( objectTable );
	MUTEX_UNLOCK( initialisation );

	/* Finish setting up the object table entry with any remaining data */
	objectInfo.uniqueID = krnlData->objectUniqueID;

	/* The first objects created are internal objects with predefined
	   handles (spes lucis aeternae).  As we create these objects we ratchet
	   up through the fixed handles until we reach the last fixed object,
	   whereupon we allocate handles normally */
	localObjectHandle = objectStateInfo->objectHandle;
	if( localObjectHandle < NO_SYSTEM_OBJECTS - 1 )
		{
		REQUIRES_MUTEX( ( localObjectHandle == SYSTEM_OBJECT_HANDLE - 1 && \
						  owner == CRYPT_UNUSED && \
						  type == OBJECT_TYPE_DEVICE && \
						  subType == SUBTYPE_DEV_SYSTEM ) || \
						( localObjectHandle == DEFAULTUSER_OBJECT_HANDLE - 1 && \
						  owner == SYSTEM_OBJECT_HANDLE && \
						  type == OBJECT_TYPE_USER && \
						  subType == SUBTYPE_USER_SO ), 
						objectTable );
		localObjectHandle++;
		ENSURES_MUTEX( isValidHandle( localObjectHandle ) && \
					   localObjectHandle < NO_SYSTEM_OBJECTS && \
					   localObjectHandle == objectStateInfo->objectHandle + 1, \
					   objectTable );
		}
	else
		{
		REQUIRES_MUTEX( isValidHandle( owner ), objectTable );

		/* Search the table for a free entry */
		localObjectHandle = findFreeObjectEntry( localObjectHandle );
		}

	/* If there's a problem with allocating a handle that it either means 
	   that we've run out of entries in the object table or there's a more 
	   general error */
	if( cryptStatusError( localObjectHandle ) )
		{
		/* If we got an OK_SPECIAL status then we've run out of room in the
		   object table, try expanding it to make room for more objects */
		if( localObjectHandle == OK_SPECIAL )
			{
			localObjectHandle = expandObjectTable();
			}
		if( cryptStatusError( localObjectHandle ) )
			{
			MUTEX_UNLOCK( objectTable );

			/* If we've run into some problem unrelated to running out of 
			   room for the object table, warn about it */
			if( localObjectHandle != CRYPT_ERROR_OVERFLOW && \
				localObjectHandle != CRYPT_ERROR_MEMORY )
				{
				DEBUG_DIAG(( "Error %d allocating new object handle", 
							 localObjectHandle ));
				assert( DEBUG_WARN );
				}

			/* Free the object instance data storage that we allocated 
			   earlier and return the error status */
			if( objectInfo.flags & OBJECT_FLAG_SECUREMALLOC )
				{
				int status = krnlMemfree( &objectInfo.objectPtr );
				ENSURES( cryptStatusOK( status ) );
				}
			else
				{
				zeroise( objectInfo.objectPtr, objectInfo.objectSize );
				clFree( "destroyObjectData", objectInfo.objectPtr );
				}
			return( localObjectHandle );
			}
		}

	/* Inner precondition: This object table slot is free */
	REQUIRES_MUTEX( isFreeObject( localObjectHandle ), objectTable );

	/* Set up the new object entry in the table and update the object table
	   state */
	krnlData->objectTable[ localObjectHandle ] = objectInfo;
	if( localObjectHandle == NO_SYSTEM_OBJECTS - 1 )
		{
		time_t theTime;
		
		/* Get a non-constant seed to use for the initial object handle.  
		   See the comment in findFreeObjectEntry() for why this is done, 
		   and why it only uses a relatively weak seed.  Since we may be 
		   running on an embedded system with no reliable time source 
		   available we use getApproxTime() rather than getTime(), the check 
		   for correct functioning of the time source on non-embedded 
		   systems has already been done in the init code */
		theTime = getApproxTime();

		/* If this is the last system object, we've been allocating handles
		   sequentially up to this point.  From now on we start allocating
		   handles starting from a randomised location in the table */
		objectStateInfo->objectHandle = \
				( int ) theTime & ( objectStateInfo->lfsrMask - 1 );
		if( objectStateInfo->objectHandle < NO_SYSTEM_OBJECTS )
			{
			/* Can occur with probability
			   NO_SYSTEM_OBJECTS / OBJECT_TABLE_ALLOCSIZE */
			objectStateInfo->objectHandle = NO_SYSTEM_OBJECTS + 42;
			}
		}
	else
		objectStateInfo->objectHandle = localObjectHandle;

	/* Update the object unique ID value */
	if( krnlData->objectUniqueID < 0 || \
		krnlData->objectUniqueID >= INT_MAX - 1 )
		krnlData->objectUniqueID = NO_SYSTEM_OBJECTS;
	else
		krnlData->objectUniqueID++;
	ENSURES_MUTEX( krnlData->objectUniqueID > 0 && \
				   krnlData->objectUniqueID < INT_MAX, objectTable );

	/* Postconditions: It's a valid object that's been set up as required */
	ENSURES_MUTEX( isValidObject( localObjectHandle ), objectTable );
	ENSURES_MUTEX( objectInfo.objectPtr == *objectDataPtr, objectTable );
	ENSURES_MUTEX( objectInfo.owner == owner, objectTable );
	ENSURES_MUTEX( objectInfo.type == type, objectTable );
	ENSURES_MUTEX( objectInfo.subType == subType, objectTable );
	ENSURES_MUTEX( objectInfo.actionFlags == actionFlags, objectTable );
	ENSURES_MUTEX( objectInfo.messageFunction == messageFunction, 
				   objectTable );

	MUTEX_UNLOCK( objectTable );

	*objectHandle = localObjectHandle;

	return( CRYPT_OK );
	}
