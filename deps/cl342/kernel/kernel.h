/****************************************************************************
*																			*
*							cryptlib Kernel Header File						*
*						Copyright Peter Gutmann 1992-2005					*
*																			*
****************************************************************************/

#ifndef _KERNEL_DEFINED

#define _KERNEL_DEFINED

#if defined( INC_ALL )
  #include "thread.h"
#else
  #include "kernel/thread.h"
#endif /* Compiler-specific includes */

/* RAY and EGON look over code.

   EGON: The structure of this kernel is exactly like the kind of telemetry
         tracker that NASA uses to secure dead pulsars in deep space.

   RAY: All message dispatch mechanisms and callback functions.

   PETER (to other jailbirds): Everyone getting this so far?  So what?  I
         guess they just don't make them like they used to.

   RAY: No!  Nobody ever made them like this!  The architect was either a
        certified genius or an authentic wacko! */

/* "There is a fine line between genius and insanity.
    I have erased this line" - Oscar Levant
	(or "Nullum magnum ingenium sine mixtura dementiae" if you want it in
	the usual style) */

/****************************************************************************
*																			*
*							Parameter Checking Macros						*
*																			*
****************************************************************************/

/* Macros to perform validity checks on objects and handles.  These checks
   are:

	isValidHandle(): Whether a handle is a valid index into the object table.
	isValidObject(): Whether a handle refers to an object in the table.
	isFreeObject(): Whether a handle refers to an empty entry in the table.
	isInternalObject(): Whether an object is an internal object.
	isInvalidObjectState(): Whether an object is in an invalid (error) state.
	isInUse(): Whether an object is currently in use (processing a message).
	isObjectOwner(): If inUse == TRUE, whether this thread is the one using
					 the object.
	isInHighState(): Whether an object is in the 'high' security state.
	isSameOwningObject(): Whether two objects have the same owner.  We also
						  have to handle the situation where the first object
						  is a user object, in which case it has to be the
						  owner of the second object.
	isObjectAccessValid(): Internal/external object access check.
	isValidMessage(): Whether a message type is valid.
	isInternalMessage(): Whether a message is an internal message.
	isValidType(): Whether an object type is valid
	isValidSubtype(): Whether an object subtype is allowed based on access
					  bitflags */

#define isValidHandle( handle ) \
		( ( handle ) >= 0 && ( handle ) < krnlData->objectTableSize )
#define isValidObject( handle ) \
		( isValidHandle( handle ) && \
		  krnlData->objectTable[ ( handle ) ].objectPtr != NULL )
#define isFreeObject( handle ) \
		( isValidHandle( handle ) && \
		  krnlData->objectTable[ ( handle ) ].objectPtr == NULL )
#define isInternalObject( handle ) \
		( krnlData->objectTable[ handle ].flags & OBJECT_FLAG_INTERNAL )
#define isObjectAccessValid( objectHandle, message ) \
		!( isInternalObject( objectHandle ) && \
		   !( message & MESSAGE_FLAG_INTERNAL ) )
#define isInvalidObjectState( handle ) \
		( krnlData->objectTable[ ( handle ) ].flags & OBJECT_FLAGMASK_STATUS )
#define isInUse( handle ) \
		( krnlData->objectTable[ ( handle ) ].lockCount > 0 )
#define isObjectOwner( handle ) \
		THREAD_SAME( krnlData->objectTable[ ( handle ) ].lockOwner, THREAD_SELF() )
#define isInHighState( handle ) \
		( krnlData->objectTable[ ( handle ) ].flags & OBJECT_FLAG_HIGH )
#define isSameOwningObject( handle1, handle2 ) \
		( krnlData->objectTable[ ( handle1 ) ].owner == CRYPT_UNUSED || \
		  krnlData->objectTable[ ( handle2 ) ].owner == CRYPT_UNUSED || \
		  ( krnlData->objectTable[ ( handle1 ) ].owner == \
							krnlData->objectTable[ ( handle2 ) ].owner ) || \
		  ( ( handle1 ) == krnlData->objectTable[ ( handle2 ) ].owner ) )
#define isValidMessage( message ) \
		( ( message ) > MESSAGE_NONE && ( message ) < MESSAGE_LAST )
#define isInternalMessage( message ) \
		( ( message ) & MESSAGE_FLAG_INTERNAL )
#define isValidType( type ) \
		( ( type ) > OBJECT_TYPE_NONE && ( type ) < OBJECT_TYPE_LAST )
#define isValidSubtype( subtypeMask, subtype ) \
		( ( ( subtypeMask ) & ( subtype ) ) == ( subtype ) )

/* The set of object checks is used frequently enough that we combine them
   into a composite check that performs all of the checks in one place */

#define fullObjectCheck( objectHandle, message ) \
		( isValidObject( objectHandle ) && \
		  isObjectAccessValid( objectHandle, message ) && \
		  checkObjectOwnership( objectTable[ objectHandle ] ) )

/* Macros to test whether a message falls into a certain class.  These tests
   are:

	isParamMessage(): Whether a message contains an object as a parameter */

#define isParamMessage( message ) \
		( ( message ) == MESSAGE_CRT_SIGN || \
		  ( message ) == MESSAGE_CRT_SIGCHECK )

/* Macros to manage object ownership, if the OS supports it */

#define checkObjectOwnership( objectPtr ) \
		( !( ( objectPtr ).flags & OBJECT_FLAG_OWNED ) || \
		  THREAD_SAME( ( objectPtr ).objectOwner, THREAD_SELF() ) )

/* A macro to turn an abnormal status indicated in an object's flags into a
   status code.  The values are prioritised so that notinited > signalled >
   busy */

#define getObjectStatusValue( flags ) \
		( ( flags & OBJECT_FLAG_NOTINITED ) ? CRYPT_ERROR_NOTINITED : \
		  ( flags & OBJECT_FLAG_SIGNALLED ) ? CRYPT_ERROR_SIGNALLED : CRYPT_OK )

/****************************************************************************
*																			*
*						Object Definitions and Information					*
*																			*
****************************************************************************/

/* The information maintained by the kernel for each object */

typedef struct {
	/* Object type and value */
	OBJECT_TYPE type;			/* Object type */
	OBJECT_SUBTYPE subType;		/* Object subtype */
	void *objectPtr;			/* Object data */
	int objectSize;				/* Object data size */

	/* Object properties */
	int flags;					/* Internal-only, locked, etc */
	int actionFlags;			/* Permitted actions */
	int referenceCount;			/* Number of references to this object */
	int lockCount;				/* Message-processing lock recursion count */
#ifdef USE_THREADS
	THREAD_HANDLE lockOwner;	/* Lock owner if lockCount > 0 */
#endif /* USE_THREADS */
	int uniqueID;				/* Unique ID for this object */
/*	time_t lastAccess;			// Last access time */

	/* Object security properties */
	int forwardCount;			/* Number of times ownership can be transferred */
	int usageCount;				/* Number of times obj.can be used */
#ifdef USE_THREADS
	THREAD_HANDLE objectOwner;	/* The object's owner */
#endif /* USE_THREADS */

	/* Object methods */
	MESSAGE_FUNCTION messageFunction; /* The object's message handler */

	/* Owning and dependent objects */
	CRYPT_USER owner;			/* Owner object handle */
	CRYPT_HANDLE dependentObject;	/* Dependent object (context or cert) */
	CRYPT_HANDLE dependentDevice;	/* Dependent crypto device */
	} OBJECT_INFO;

/* The flags that apply to each object in the table */

#define OBJECT_FLAG_NONE		0x0000	/* Non-flag */
#define OBJECT_FLAG_INTERNAL	0x0001	/* Internal-use only */
#define OBJECT_FLAG_NOTINITED	0x0002	/* Still being initialised */
#define OBJECT_FLAG_HIGH		0x0004	/* In 'high' security state */
#define OBJECT_FLAG_SIGNALLED	0x0008	/* In signalled state */
#define OBJECT_FLAG_SECUREMALLOC 0x0010	/* Uses secure memory */
#define OBJECT_FLAG_OWNED		0x0020	/* Object is bound to a thread */
#define OBJECT_FLAG_ATTRLOCKED	0x0040	/* Security properties can't be modified */

/* The flags that convey information about an object's status */

#define OBJECT_FLAGMASK_STATUS \
		( OBJECT_FLAG_NOTINITED | OBJECT_FLAG_SIGNALLED )

/****************************************************************************
*																			*
*							Kernel Data Structures							*
*																			*
****************************************************************************/

/* The object allocation state data.  This controls the allocation of
   handles to newly-created objects.  The first NO_SYSTEM_OBJECTS handles
   are system objects that exist with fixed handles, the remainder are
   allocated pseudorandomly under the control of an LFSR */

typedef struct {
	long lfsrMask, lfsrPoly;	/* LFSR state values */
	int objectHandle;			/* Current object handle */
	} OBJECT_STATE_INFO;

/* A structure to store the details of a message sent to an object, and the
   size of the message queue.  This defines the maximum nesting depth of
   messages sent by an object.  Because of the way krnlSendMessage() handles
   message processing, it's extremely difficult to ever have more than two
   or three messages in the queue unless an object starts recursively
   sending itself messages */

typedef struct {
	int objectHandle;			/* Handle to send message to */
	const void *handlingInfoPtr;/* Message handling info */
	MESSAGE_TYPE message;
	const void *messageDataPtr;
	int messageValue;			/* Message parameters */
	} MESSAGE_QUEUE_DATA;

#define MESSAGE_QUEUE_SIZE	16

/* Semaphores are one-shots, so that once set and cleared they can't be
   reset.  This is handled by enforcing the following state transitions:

	Uninited -> Set | Clear
	Set -> Set | Clear
	Clear -> Clear

   The handling is complicated somewhat by the fact that on some systems the
   semaphore has to be explicitly deleted, but only the last thread to use
   it can safely delete it.  In order to handle this, we reference-count the
   semaphore and let the last thread out delete it.  In order to do this we
   introduce an additional state, preClear, which indicates that while the
   semaphore object is still present, the last thread out should delete it,
   bringing it to the true clear state */

typedef enum {
	SEMAPHORE_STATE_UNINITED,
	SEMAPHORE_STATE_CLEAR,
	SEMAPHORE_STATE_PRECLEAR,
	SEMAPHORE_STATE_SET,
	SEMAPHORE_STATE_LAST
	} SEMAPHORE_STATE;

typedef struct {
	SEMAPHORE_STATE state;		/* Semaphore state */
	MUTEX_HANDLE object;		/* Handle to system synchronisation object */
	int refCount;				/* Reference count for handle */
	} SEMAPHORE_INFO;

/* A structure to store the details of a thread */

typedef struct {
	THREAD_FUNCTION threadFunction;	/* Function to call from thread */
	THREAD_PARAMS threadParams;		/* Thread function parameters */
	SEMAPHORE_TYPE semaphore;		/* Optional semaphore to set */
	MUTEX_HANDLE syncHandle;		/* Handle to use for thread sync */
	} THREAD_INFO;

/* When the kernel starts up and closes down it does so in a multi-stage 
   process that's equivalent to Unix runlevels.  For the startup at the
   first level the kernel data block and all kernel-level primitive
   objects like mutexes have been initialised.
   
   For the shutdown, at the first level all internal worker threads/tasks 
   must exist.  At the next level all messages to objects except destroy 
   messages fail.  At the final level all kernel-managed primitives such as 
   mutexes and semaphores are no longer available */

typedef enum {
	INIT_LEVEL_NONE,			/* Uninitialised */
	INIT_LEVEL_KRNLDATA,		/* Kernel data block initialised */
	INIT_LEVEL_FULL,			/* Full initialisation */
	INIT_LEVEL_LAST				/* Last possible init level */
	} INIT_LEVEL;

typedef enum {
	SHUTDOWN_LEVEL_NONE,		/* Normal operation */
	SHUTDOWN_LEVEL_THREADS,		/* Internal threads must exit */
	SHUTDOWN_LEVEL_MESSAGES,	/* Only destroy messages are valid */
	SHUTDOWN_LEVEL_MUTEXES,		/* Kernel objects become invalid */
	SHUTDOWN_LEVEL_ALL,			/* Complete shutdown */
	SHUTDOWN_LEVEL_LAST			/* Last possible shutdown level */
	} SHUTDOWN_LEVEL;

/* The kernel data block, containing all variables used by the kernel.  With
   the exception of the special-case values at the start, all values in this
   block should be set to use zero/NULL as their ground state (for example a
   boolean variable should have a ground state of FALSE (zero) rather than
   TRUE (nonzero)).

   If the objectTable giant lock (or more strictly speaking monolithic lock, 
   since the kernel's message-handling is designed to be straight-line code 
   and so never blocks for any amount of time like the Linux giant lock can) 
   ever proves to be a problem then the solution would be to use lock 
   striping, dividing the load of the object table across NO_TABLE_LOCKS 
   locks.  This gets a bit tricky because the object table is dynamically
   resizeable, a basic mod_NO_TABLE_LOCKS strategy where every n-th entry 
   uses the same lock works but then we'd still need a giant lock to check 
   whether the table is being resized.  To avoid this we can use a lock-free 
   implementation that operates by acquiring each lock (to make sure we have 
   complete control of the table), checking whether another thread beat us to 
   it, and if not resizing the table.  The pseudocode for this is as 
   follows:

	// Remember the original table size
	const int oldSize = krnlData->objectTableSize;

	// Acquire each lock
	for( i = 0; i < NO_LOCKS; i++ )
		THREAD_LOCK( krnlData->locks[ i ] );

	// Check whether another thread beat us to the resize while we were 
	// acquiring locks
	if( krnlData->objectTableSize != oldSize )
		{
		// Unlock all the locks
		// ... //
		return;
		}

	// We hold all the locks and therefore have exclusive control of the 
	// table, resize it
	// ... //

	// Release each lock again //
	for( i = 0; i < NO_LOCKS; i++ )
		THREAD_UNLOCK( krnlData->locks[ i ] );

   This is a conventional lock-free implementation of such an algorithm but 
   is conceptually ugly in that it accesses protected data outside the lock, 
   which will cause concurrency-checking tools to complain.  Until the fast-
   path through the kernel actually becomes a real bottleneck it's probably 
   best to leave well enough alone */

typedef struct {
	/* The kernel initialisation state and a lock to protect it.  The
	   lock and shutdown level value are handled externally and aren't
	   cleared when the kernel data block as a whole is cleared.  Note
	   that the shutdown level has to be before the lock so that we can
	   statically initialise the data with '{ 0 }', which won't work if
	   the lock data is non-scalar */
	SHUTDOWN_LEVEL shutdownLevel;		/* Kernel shutdown level */
#ifdef USE_THREADS
	MUTEX_DECLARE_STORAGE( initialisation );
#endif /* USE_THREADS */
	/* Everything from this point on is cleared at init and shutdown */
	int initLevel;						/* Kernel initialisation level */

	/* The kernel object table and object table management info */
	OBJECT_INFO *objectTable;			/* Pointer to object table */
	int objectTableSize;				/* Current table size */
	int objectUniqueID;					/* Unique ID for next object */
	OBJECT_STATE_INFO objectStateInfo;	/* Object allocation state */
#ifdef USE_THREADS
	MUTEX_DECLARE_STORAGE( objectTable );
#endif /* USE_THREADS */

	/* The kernel message dispatcher queue */
	BUFFER( MESSAGE_QUEUE_SIZE, queueEnd ) \
	MESSAGE_QUEUE_DATA messageQueue[ MESSAGE_QUEUE_SIZE + 8 ];
	int queueEnd;						/* Points past last queue element */

	/* The kernel semaphores */
	BUFFER_FIXED( SEMAPHORE_LAST ) \
	SEMAPHORE_INFO semaphoreInfo[ SEMAPHORE_LAST + 8 ];
#ifdef USE_THREADS
	MUTEX_DECLARE_STORAGE( semaphore );
#endif /* USE_THREADS */

	/* The kernel mutexes.  Since mutexes usually aren't scalar values and
	   are declared and accessed via macros that manipulate various fields,
	   we have to declare a pile of them individually rather than using an
	   array of mutexes */
#ifdef USE_THREADS
	MUTEX_DECLARE_STORAGE( mutex1 );
	MUTEX_DECLARE_STORAGE( mutex2 );
	MUTEX_DECLARE_STORAGE( mutex3);
#endif /* USE_THREADS */

	/* The kernel thread data */
#ifdef USE_THREADS
	THREAD_INFO threadInfo;
#endif /* USE_THREADS */

	/* The kernel secure memory list and a lock to protect it */
	void *allocatedListHead, *allocatedListTail;
#ifdef USE_THREADS
	MUTEX_DECLARE_STORAGE( allocation );
#endif /* USE_THREADS */

	/* A marker for the end of the kernel data, used during init/shutdown */
	int endMarker;
	} KERNEL_DATA;

/* When we start up and shut down the kernel, we need to clear the kernel
   data.  However, the init lock may have been set by an external management
   function, so we can't clear that part of the kernel data.  In addition,
   on shutdown the shutdown level value must stay set so that any threads
   still running will be forced to exit at the earliest possible instance,
   and remain set after the shutdown has completed.  To handle this, we use
   the following macro to clear only the appropriate area of the kernel data
   block */

#ifdef __STDC__

#include <stddef.h>		/* For offsetof() */

#define CLEAR_KERNEL_DATA()     \
		zeroise( ( BYTE * ) krnlData + offsetof( KERNEL_DATA, initLevel ), \
				 sizeof( KERNEL_DATA ) - offsetof( KERNEL_DATA, initLevel ) )
#else

#define CLEAR_KERNEL_DATA()     \
		assert( &krnlDataBlock.endMarker - \
				&krnlDataBlock.initLevel < sizeof( krnlDataBlock ) ); \
		zeroise( ( void * ) ( &krnlDataBlock.initLevel ), \
				 &krnlDataBlock.endMarker - &krnlDataBlock.initLevel )
#endif /* C89 compilers */

/****************************************************************************
*																			*
*								ACL Functions								*
*																			*
****************************************************************************/

/* Prototypes for functions in certm_acl.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckCertMgmtAccess( IN_HANDLE const int objectHandle,
									IN_MESSAGE const MESSAGE_TYPE message,
									IN_BUFFER_C( sizeof( MESSAGE_CERTMGMT_INFO ) ) \
										const void *messageDataPtr,
									IN_ENUM( CRYPT_CERTACTION ) \
										const int messageValue,
									STDC_UNUSED const void *dummy );

/* Prototypes for functions in key_acl.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckKeysetAccess( IN_HANDLE const int objectHandle,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  IN_BUFFER_C( sizeof( MESSAGE_KEYMGMT_INFO ) ) \
										const void *messageDataPtr,
								  IN_ENUM( KEYMGMT_ITEM ) const int messageValue,
								  STDC_UNUSED const void *dummy );

/* Prototypes for functions in mech_acl.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismWrapAccess( IN_HANDLE const int objectHandle,
										 IN_MESSAGE const MESSAGE_TYPE message,
										 IN_BUFFER_C( sizeof( MECHANISM_WRAP_INFO ) ) \
											TYPECAST( MECHANISM_WRAP_INFO * ) \
											const void *messageDataPtr,
										 IN_ENUM( MECHANISM ) const int messageValue,
										 STDC_UNUSED const void *dummy );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismSignAccess( IN_HANDLE const int objectHandle,
										 IN_MESSAGE const MESSAGE_TYPE message,
										 IN_BUFFER_C( sizeof( MECHANISM_SIGN_INFO ) ) \
											TYPECAST( MECHANISM_SIGN_INFO * ) \
											const void *messageDataPtr,
										 IN_ENUM( MECHANISM ) const int messageValue,
										 STDC_UNUSED const void *dummy );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismDeriveAccess( IN_HANDLE const int objectHandle,
										   IN_MESSAGE const MESSAGE_TYPE message,
										   IN_BUFFER_C( sizeof( MECHANISM_DERIVE_INFO ) ) \
												TYPECAST( MECHANISM_DERIVE_INFO * ) \
												const void *messageDataPtr,
										   IN_ENUM( MECHANISM ) const int messageValue,
										   STDC_UNUSED const void *dummy );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismKDFAccess( IN_HANDLE const int objectHandle,
										IN_MESSAGE const MESSAGE_TYPE message,
										IN_BUFFER_C( sizeof( MECHANISM_KDF_INFO ) ) \
											TYPECAST( MECHANISM_KDF_INFO * ) \
											const void *messageDataPtr,
										IN_ENUM( MECHANISM ) const int messageValue,
										STDC_UNUSED const void *dummy );

/* Prototypes for functions in msg_acl.c */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN paramAclConsistent( const PARAM_ACL *paramACL,
							const BOOLEAN mustBeEmpty );
CHECK_RETVAL \
int preDispatchSignalDependentObjects( IN_HANDLE const int objectHandle,
									   STDC_UNUSED const MESSAGE_TYPE dummy1,
									   STDC_UNUSED const void *dummy2,
									   STDC_UNUSED const int dummy3,
									   STDC_UNUSED const void *dummy4 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 5 ) ) \
int preDispatchCheckAttributeAccess( IN_HANDLE const int objectHandle,
									 IN_MESSAGE const MESSAGE_TYPE message,
									 IN_OPT const void *messageDataPtr,
									 IN_ATTRIBUTE const int messageValue,
									 IN TYPECAST( ATTRIBUTE_ACL * ) \
										const void *auxInfo );
CHECK_RETVAL \
int preDispatchCheckCompareParam( IN_HANDLE const int objectHandle,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  const void *messageDataPtr,
								  IN_ENUM( MESSAGE_COMPARE ) const int messageValue,
								  STDC_UNUSED const void *dummy2 );
CHECK_RETVAL \
int preDispatchCheckCheckParam( IN_HANDLE const int objectHandle,
								IN_MESSAGE const MESSAGE_TYPE message,
								STDC_UNUSED const void *dummy1,
								IN_ENUM( MESSAGE_CHECK ) const int messageValue,
								STDC_UNUSED const void *dummy2 );
CHECK_RETVAL \
int preDispatchCheckActionAccess( IN_HANDLE const int objectHandle,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  STDC_UNUSED const void *dummy1,
								  STDC_UNUSED const int dummy2,
								  STDC_UNUSED const void *dummy3 );
CHECK_RETVAL \
int preDispatchCheckState( IN_HANDLE const int objectHandle,
						   IN_MESSAGE const MESSAGE_TYPE message,
						   STDC_UNUSED const void *dummy1,
						   STDC_UNUSED const int dummy2, 
						   STDC_UNUSED const void *dummy3 );
CHECK_RETVAL \
int preDispatchCheckParamHandleOpt( IN_HANDLE const int objectHandle,
									IN_MESSAGE const MESSAGE_TYPE message,
									STDC_UNUSED const void *dummy1,
									const int messageValue,
									IN TYPECAST( MESSAGE_ACL * ) \
										const void *auxInfo );
CHECK_RETVAL \
int preDispatchCheckStateParamHandle( IN_HANDLE const int objectHandle,
									  IN_MESSAGE const MESSAGE_TYPE message,
									  STDC_UNUSED const void *dummy1,
									  const int messageValue,
									  IN TYPECAST( MESSAGE_ACL * ) \
											const void *auxInfo );
CHECK_RETVAL \
int preDispatchCheckExportAccess( IN_HANDLE const int objectHandle,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  const void *messageDataPtr,
								  IN_ENUM( CRYPT_CERTFORMAT ) const int messageValue,
								  STDC_UNUSED const void *dummy2 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckData( IN_HANDLE const int objectHandle,
						  IN_MESSAGE const MESSAGE_TYPE message,
						  IN_BUFFER_C( sizeof( MESSAGE_DATA ) ) \
								const void *messageDataPtr,
						  STDC_UNUSED const int dummy1,
						  STDC_UNUSED const void *dummy2 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckCreate( IN_HANDLE const int objectHandle,
							IN_MESSAGE const MESSAGE_TYPE message,
							IN_BUFFER_C( sizeof( MESSAGE_CREATEOBJECT_INFO ) ) \
								const void *messageDataPtr,
							IN_ENUM( OBJECT ) const int messageValue,
							STDC_UNUSED const void *dummy );
CHECK_RETVAL \
int preDispatchCheckUserMgmtAccess( IN_HANDLE const int objectHandle, 
									IN_MESSAGE const MESSAGE_TYPE message,
									STDC_UNUSED const void *dummy1,
									IN_ENUM( MESSAGE_USERMGMT ) const int messageValue, 
									STDC_UNUSED const void *dummy2 );
CHECK_RETVAL \
int preDispatchCheckTrustMgmtAccess( IN_HANDLE const int objectHandle, 
									 IN_MESSAGE const MESSAGE_TYPE message,
									 const void *messageDataPtr,
									 STDC_UNUSED const int messageValue, 
									 STDC_UNUSED const void *dummy );
CHECK_RETVAL \
int postDispatchSignalDependentDevices( IN_HANDLE const int objectHandle,
										STDC_UNUSED const MESSAGE_TYPE dummy1,
										STDC_UNUSED const void *dummy2,
										STDC_UNUSED const int dummy3,
										STDC_UNUSED const void *dummy4 );
CHECK_RETVAL \
int postDispatchMakeObjectExternal( STDC_UNUSED const int dummy,
									IN_MESSAGE const MESSAGE_TYPE message,
									const void *messageDataPtr,
									const int messageValue,
									const void *auxInfo );
CHECK_RETVAL \
int postDispatchForwardToDependentObject( IN_HANDLE const int objectHandle,
										  IN_MESSAGE const MESSAGE_TYPE message,
										  STDC_UNUSED const void *dummy1,
										  IN_ENUM( MESSAGE_CHECK ) const int messageValue,
										  STDC_UNUSED const void *dummy2 );
CHECK_RETVAL \
int postDispatchUpdateUsageCount( IN_HANDLE const int objectHandle,
								  STDC_UNUSED const MESSAGE_TYPE dummy1,
								  STDC_UNUSED const void *dummy2,
								  STDC_UNUSED const int dummy3,
								  STDC_UNUSED const void *dummy4 );
CHECK_RETVAL \
int postDispatchChangeState( IN_HANDLE const int objectHandle,
							 STDC_UNUSED const MESSAGE_TYPE dummy1,
							 STDC_UNUSED const void *dummy2,
							 STDC_UNUSED const int dummy3,
							 STDC_UNUSED const void *dummy4 );
CHECK_RETVAL \
int postDispatchChangeStateOpt( IN_HANDLE const int objectHandle,
								STDC_UNUSED const MESSAGE_TYPE dummy1,
								STDC_UNUSED const void *dummy2,
								const int messageValue,
								IN TYPECAST( ATTRIBUTE_ACL * ) const void *auxInfo );
CHECK_RETVAL \
int postDispatchHandleZeroise( IN_HANDLE const int objectHandle, 
							   IN_MESSAGE const MESSAGE_TYPE message,
							   STDC_UNUSED const void *dummy2,
							   IN_ENUM( MESSAGE_USERMGMT ) const int messageValue,
							   STDC_UNUSED const void *dummy3 );

/****************************************************************************
*																			*
*								Kernel Functions							*
*																			*
****************************************************************************/

/* Prototypes for functions in attr_acl.c */

CHECK_RETVAL_PTR \
const void *findAttributeACL( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
							  const BOOLEAN isInternalMessage );

/* Prototypes for functions in int_msg.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getPropertyAttribute( IN_HANDLE const int objectHandle,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  OUT_BUFFER_FIXED_C( sizeof( int ) ) void *messageDataPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int setPropertyAttribute( IN_HANDLE const int objectHandle,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  IN_BUFFER_C( sizeof( int ) ) void *messageDataPtr );
CHECK_RETVAL \
int incRefCount( IN_HANDLE const int objectHandle, 
				 STDC_UNUSED const int dummy1,
				 STDC_UNUSED const void *dummy2, 
				 STDC_UNUSED const BOOLEAN dummy3 );
CHECK_RETVAL \
int decRefCount( IN_HANDLE const int objectHandle, 
				 STDC_UNUSED const int dummy1,
				 STDC_UNUSED const void *dummy2, 
				 const BOOLEAN isInternal );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getDependentObject( IN_HANDLE const int objectHandle, 
						const int targetType,
						IN_BUFFER_C( sizeof( int ) ) \
								const void *messageDataPtr,
							/* This is a bit of a lie since we actually 
							   return the dependent object through this 
							   pointer, however making it non-const means 
							   that we'd have to also un-const every other 
							   use of this parameter in all other functions 
							   accessed via this function pointer */
						STDC_UNUSED const BOOLEAN dummy );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int setDependentObject( IN_HANDLE const int objectHandle, 
						IN_ENUM( SETDEP_OPTION ) const int option,
						IN_BUFFER_C( sizeof( int ) ) \
								const void *messageDataPtr,
						STDC_UNUSED const BOOLEAN dummy );
CHECK_RETVAL \
int cloneObject( IN_HANDLE const int objectHandle, 
				 IN_HANDLE const int clonedObject,
				 STDC_UNUSED const void *dummy1, 
				 STDC_UNUSED const BOOLEAN dummy2 );

/* Prototypes for functions in sendmsg.c */

CHECK_RETVAL \
int checkTargetType( IN_HANDLE const int objectHandle, const long targets );
CHECK_RETVAL \
int findTargetType( IN_HANDLE const int originalObjectHandle, 
					const long targets );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int waitForObject( IN_HANDLE const int objectHandle, 
				   OUT_PTR OBJECT_INFO **objectInfoPtrPtr );

/* Prototypes for functions in objects.c */

CHECK_RETVAL \
int destroyObjectData( IN_HANDLE const int objectHandle );
CHECK_RETVAL \
int destroyObjects( void );

/* Prototypes for functions in semaphore.c */

void setSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore,
				   const MUTEX_HANDLE object );
void clearSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore );

/* Init/shutdown functions for each kernel module */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initAllocation( INOUT KERNEL_DATA *krnlDataPtr );
void endAllocation( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initAttributeACL( INOUT KERNEL_DATA *krnlDataPtr );
void endAttributeACL( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCertMgmtACL( INOUT KERNEL_DATA *krnlDataPtr );
void endCertMgmtACL( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initInternalMsgs( INOUT KERNEL_DATA *krnlDataPtr );
void endInternalMsgs( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initKeymgmtACL( INOUT KERNEL_DATA *krnlDataPtr );
void endKeymgmtACL( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initMechanismACL( INOUT KERNEL_DATA *krnlDataPtr );
void endMechanismACL( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initMessageACL( INOUT KERNEL_DATA *krnlDataPtr );
void endMessageACL( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initObjects( INOUT KERNEL_DATA *krnlDataPtr );
void endObjects( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initObjectAltAccess( INOUT KERNEL_DATA *krnlDataPtr );
void endObjectAltAccess( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSemaphores( INOUT KERNEL_DATA *krnlDataPtr );
void endSemaphores( void );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSendMessage( INOUT KERNEL_DATA *krnlDataPtr );
void endSendMessage( void );

#endif /* _KERNEL_DEFINED */
