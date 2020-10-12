/****************************************************************************
*																			*
*							Object Alternative Access						*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

/* Sending a message to an object only makes the one object which is the
   target of the message available for use.  When we need simultaneous
   access to two objects (for example when copying a collection of cert
   extensions from one cert to another), we have to use the
   krnlAcquireObject()/krnlReleaseObject() functions to obtain access to
   the second object's internals.

   There is a second situation in which we need access to an object's
   internals, and that occurs when we need to export/import a key from/to
   a context.  This is handled via the key extract functions at the end
   of this module, see the comments there for further information */

#if defined( INC_ALL )
  #include "crypt.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/* Optionally include the Intel Thread Checker API to control analysis of 
   the object mutexes */

#if defined( _MSC_VER ) && ( _MSC_VER == 1200 ) && 0
  #include "../../../Intel/VTune/tcheck/Include/libittnotify.h"
  #pragma comment( lib, "C:/Program Files/Intel/VTune/Analyzer/Lib/libittnotify.lib" )

  #define THREAD_NOTIFY_PREPARE( id )	__itt_notify_sync_prepare( ( void * ) id );
  #define THREAD_NOTIFY_CANCELLED( id )	__itt_notify_sync_prepare( ( void * ) id );
  #define THREAD_NOTIFY_ACQUIRED( id )	__itt_notify_sync_acquired( ( void * ) id );
  #define THREAD_NOTIFY_RELEASED( id )	__itt_notify_sync_releasing( ( void * ) id );
#else
  #define THREAD_NOTIFY_PREPARE( dummy )
  #define THREAD_NOTIFY_CANCELLED( dummy )
  #define THREAD_NOTIFY_ACQUIRED( dummy )
  #define THREAD_NOTIFY_RELEASED( dummy )
#endif /* VC++ 6.0 with Intel Thread Checker */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* The type of checking that we perform for the object access.  The check 
   types are:

	CHECK_EXTACCESS
		Kernel-external call with a cert or crypto device to allow access to
		object-internal data.

	CHECK_KEYACCESS
		Kernel-internal call with a context for key export/import.

	CHECK_SUSPEND
		Kernel-external call with a user or system object to temporarily
		suspend object use and allow others access, providing a (somewhat 
		crude) mechanism for making kernel calls interruptible */

typedef enum {
	ACCESS_CHECK_NONE,		/* No access check type */
	ACCESS_CHECK_EXTACCESS,	/* Generic external call: Cert or crypt.dev.*/
	ACCESS_CHECK_KEYACCESS,	/* Internal call: Context for key export */
	ACCESS_CHECK_SUSPEND,	/* Suspend object use: User or sys.obj.*/
	ACCESS_CHECK_LAST		/* Last access check type */
	} ACCESS_CHECK_TYPE;

/* Check that this is an object for which direct access is valid.  We can 
   only access the following object types:

	Certificates: EXTACCESS, used when copying internal state such as cert 
		extensions or CRL info from one cert object to another.

	Contexts: KEYACCESS, used when importing/exporting keys to/from contexts 
		during key wrap/unwrap operations.

	Crypto hardware devices other than the system object: EXTACCESS, used 
		when a context tied to a device needs to perform an operation using 
		the device.

	System object: ACCESS_CHECK_SUSPEND, used when performing a randomness 
		data read/write, which can take some time to complete.

	User objects: ACCESS_CHECK_SUSPEND, used when committing config data to 
		persistent storage.  We don't actually use the object data but 
		merely unlock it to allow others access while performing the 
		potentially lengthy update.  Also used when performing the self-
		test */

CHECK_RETVAL \
static int checkAccessValid( IN_HANDLE const int objectHandle,
							 IN_ENUM( ACCESS_CHECK ) \
									const ACCESS_CHECK_TYPE checkType,
							 IN_ERROR const int errorCode )
	{
	OBJECT_INFO *objectTable = getObjectTable(), *objectInfoPtr;

	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( isEnumRange( checkType, ACCESS_CHECK ) );
	REQUIRES( cryptStatusError( errorCode ) );

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(): It's a valid object owned by the calling
	   thread */
	if( !isValidObject( objectHandle ) || \
		!checkObjectOwnership( objectTable[ objectHandle ] ) )
		return( errorCode );

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ objectHandle ];
	REQUIRES( sanityCheckObject( objectInfoPtr ) );

	/* Make sure that the object access is valid */
	switch( objectInfoPtr->type )
		{
		case OBJECT_TYPE_CONTEXT:
			/* Optionally used to allow direct calls to context functions 
			   for real-time APi access */
#ifdef CONFIG_DIRECT_API
			if( checkType == ACCESS_CHECK_EXTACCESS )
				{
				if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_CONV ) && \
					!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_HASH ) && \
					!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_MAC ) )
					return( errorCode );
				}
			else
#endif /* CONFIG_DIRECT_API */
			{
			/* Used when exporting/importing keying info, valid for contexts 
			   with keys when called from within the kernel */
			if( checkType != ACCESS_CHECK_KEYACCESS )
				return( errorCode );
			if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_CONV ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_MAC ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_PKC ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_GENERIC ) )
				return( errorCode );
			}
			break;

		case OBJECT_TYPE_CERTIFICATE:
			/* Used when copying internal state such as cert extensions or
			   CRL info from one cert object to another.  This is valid for
			   all cert types */
			if( checkType != ACCESS_CHECK_EXTACCESS )
				return( errorCode );
			break;

		case OBJECT_TYPE_DEVICE:
			/* If it's an external access operation, it's used when a 
			   context tied to a crypto hardware device needs to perform an 
			   operation using the device.  This is valid for all devices 
			   other than the system object */
			if( checkType == ACCESS_CHECK_EXTACCESS )
				{
				if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_DEV_PKCS11 ) && \
					!isValidSubtype( objectInfoPtr->subType, SUBTYPE_DEV_CRYPTOAPI ) && \
					!isValidSubtype( objectInfoPtr->subType, SUBTYPE_DEV_HARDWARE ) )
					return( errorCode );
				}
			else
				{
				/* If it's a suspend operation, it's used to temporarily 
				   allow access to the system object while other operations
				   are being performed */
				if( checkType != ACCESS_CHECK_SUSPEND )
					return( errorCode );
				if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_DEV_SYSTEM ) )
					return( errorCode );
				}
			break;

		case OBJECT_TYPE_USER:
			/* Used when updating config data, which can take awhile.  The 
			   default user is an SO user, which is why we check for this 
			   user type */
			if( checkType != ACCESS_CHECK_SUSPEND )
				return( errorCode );
			if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_USER_SO ) )
				return( errorCode );
			break;

		default:
			retIntError();
		}

	/* Postcondition: The object is of the appropriate type for the access */
#ifdef CONFIG_DIRECT_API
	ENSURES( ( checkType == ACCESS_CHECK_EXTACCESS && \
			   ( objectInfoPtr->type == OBJECT_TYPE_CONTEXT || \
			     objectInfoPtr->type == OBJECT_TYPE_CERTIFICATE || \
				 objectInfoPtr->type == OBJECT_TYPE_DEVICE ) ) || \
			 ( checkType == ACCESS_CHECK_KEYACCESS && \
			   objectInfoPtr->type == OBJECT_TYPE_CONTEXT ) || \
			 ( checkType == ACCESS_CHECK_SUSPEND && \
			   ( objectInfoPtr->type == OBJECT_TYPE_DEVICE || \
				 objectInfoPtr->type == OBJECT_TYPE_USER ) ) );
#else
	ENSURES( ( checkType == ACCESS_CHECK_EXTACCESS && \
			   ( objectInfoPtr->type == OBJECT_TYPE_CERTIFICATE || \
				 objectInfoPtr->type == OBJECT_TYPE_DEVICE ) ) || \
			 ( checkType == ACCESS_CHECK_KEYACCESS && \
			   objectInfoPtr->type == OBJECT_TYPE_CONTEXT ) || \
			 ( checkType == ACCESS_CHECK_SUSPEND && \
			   ( objectInfoPtr->type == OBJECT_TYPE_DEVICE || \
				 objectInfoPtr->type == OBJECT_TYPE_USER ) ) );
#endif /* CONFIG_DIRECT_API */

	return( CRYPT_OK );
	}

/* Get a pointer to an object's data from its handle */

CHECK_RETVAL \
static int getObject( IN_HANDLE const int objectHandle, 
					  IN_ENUM( OBJECT_TYPE ) const OBJECT_TYPE type,
					  IN_ENUM( ACCESS_CHECK ) const ACCESS_CHECK_TYPE checkType, 
					  OUT_OPT_PTR_COND void **objectPtrPtr, 
					  IN_INT_OPT const int refCount, 
					  IN_ERROR const int errorCode )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	OBJECT_INFO *objectTable, *objectInfoPtr;
	int status;

	assert( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
				objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
			  objectPtrPtr == NULL && refCount > 0 ) || \
			( !( objectHandle == SYSTEM_OBJECT_HANDLE || \
				 objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
			  isWritePtr( objectPtrPtr, sizeof( void * ) ) && \
			  refCount == CRYPT_UNUSED ) );

	/* Preconditions: It's a valid object */
	REQUIRES( isValidHandle( objectHandle ) );
#ifndef CONFIG_FUZZ		/* To directly inject data into objects */
	REQUIRES( isValidType( type ) && \
			  ( type == OBJECT_TYPE_CONTEXT || \
				type == OBJECT_TYPE_CERTIFICATE || \
				type == OBJECT_TYPE_DEVICE || type == OBJECT_TYPE_USER ) );
#endif /* CONFIG_FUZZ */
	REQUIRES( isEnumRange( checkType, ACCESS_CHECK ) );
	REQUIRES( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
				  objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
				objectPtrPtr == NULL && refCount > 0 ) || \
			  ( !( objectHandle == SYSTEM_OBJECT_HANDLE || \
				   objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
				objectPtrPtr != NULL && refCount == CRYPT_UNUSED ) );

	/* Clear the return value */
	if( objectPtrPtr != NULL )
		*objectPtrPtr = NULL;

	THREAD_NOTIFY_PREPARE( objectHandle );
	MUTEX_LOCK( objectTable );
	objectTable = getObjectTable();

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(), as well as situation-specific additional checks 
	   for correct object types */
#ifndef CONFIG_FUZZ		/* To directly inject data into objects */
	status = checkAccessValid( objectHandle, checkType, errorCode );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( status );
		}
#endif /* CONFIG_FUZZ */

	/* Perform additional checks for correct object types */
	if( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
			objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
		  objectPtrPtr != NULL ) || \
		objectTable[ objectHandle ].type != type )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( errorCode );
		}

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ objectHandle ];
	REQUIRES_MUTEX( sanityCheckObject( objectInfoPtr ), objectTable );

	/* Inner precondition: The object is of the requested type */
#ifndef CONFIG_FUZZ		/* To directly inject data into objects */
	REQUIRES_MUTEX( objectInfoPtr->type == type && \
					( objectInfoPtr->type == OBJECT_TYPE_CONTEXT || \
					   objectInfoPtr->type == OBJECT_TYPE_CERTIFICATE || \
					   objectInfoPtr->type == OBJECT_TYPE_DEVICE || \
					   objectInfoPtr->type == OBJECT_TYPE_USER ), 
					objectTable );
#endif /* CONFIG_FUZZ */

	/* If the object is busy, wait for it to become available */
	if( isInUse( objectHandle ) && !isObjectOwner( objectHandle ) )
		{
		status = waitForObject( objectHandle, &objectInfoPtr );
		if( cryptStatusError( status ) )
			{
			MUTEX_UNLOCK( objectTable );
			THREAD_NOTIFY_CANCELLED( objectHandle );
			return( status );
			}
		}

	/* If it's an external access to certificate/device info or an internal 
	   access to access the object's keying data, increment the object's 
	   reference count to reserve it for our exclusive use */
	if( checkType == ACCESS_CHECK_EXTACCESS || \
		checkType == ACCESS_CHECK_KEYACCESS )
		objectInfoPtr->lockCount++;
	else
		{
		/* If we're resuming use of an object that we suspended to allow 
		   others access, reset the reference count */
		REQUIRES_MUTEX( checkType == ACCESS_CHECK_SUSPEND, objectTable );
		REQUIRES_MUTEX( objectInfoPtr->lockCount == 0, objectTable );
		REQUIRES_MUTEX( refCount > 0 && refCount < 100, objectTable );

		objectInfoPtr->lockCount = refCount;
		}
#ifdef USE_THREADS
	objectInfoPtr->lockOwner = THREAD_SELF();
#endif /* USE_THREADS */
	if( objectPtrPtr != NULL )
		{
		void *objectPtr = DATAPTR_GET( objectInfoPtr->objectPtr );

		REQUIRES_MUTEX( objectPtr != NULL, objectTable );

		*objectPtrPtr = objectPtr;
		}
	MUTEX_UNLOCK( objectTable );
	THREAD_NOTIFY_ACQUIRED( objectHandle );

	return( CRYPT_OK );
	}

/* Release an object that we previously acquired directly.  We don't require 
   that the return value of this function be checked by the caller since the 
   only time that we can get an error is if it's a 'can't-occur' internal 
   error and it's not obvious what they should do in that case (omnia iam 
   fient fieri quae posse negabam) */

static int releaseObject( IN_HANDLE const int objectHandle,
						  IN_ENUM( ACCESS_CHECK ) const ACCESS_CHECK_TYPE checkType,
						  OUT_OPT_INT_Z int *refCount )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	OBJECT_INFO *objectTable, *objectInfoPtr;
#ifndef CONFIG_FUZZ
	int status;
#endif /* CONFIG_FUZZ */
	DECLARE_ORIGINAL_INT( lockCount );

	assert( ( ( checkType == ACCESS_CHECK_EXTACCESS || \
				checkType == ACCESS_CHECK_KEYACCESS ) && \
			  refCount == NULL ) || \
			( checkType == ACCESS_CHECK_SUSPEND && \
			  isWritePtr( refCount, sizeof( int ) ) ) );

	/* Preconditions */
	REQUIRES( isEnumRange( checkType, ACCESS_CHECK ) );
	REQUIRES( ( ( checkType == ACCESS_CHECK_EXTACCESS || \
				  checkType == ACCESS_CHECK_KEYACCESS ) && \
				refCount == NULL ) || \
			  ( checkType == ACCESS_CHECK_SUSPEND && \
				refCount != NULL ) );

	THREAD_NOTIFY_PREPARE( objectHandle );
	MUTEX_LOCK( objectTable );
	objectTable = getObjectTable();

	/* Preconditions: It's a valid object in use by the caller.  Since these 
	   checks require access to the object table we can only perform them 
	   after we've locked it */
	REQUIRES_MUTEX( isValidObject( objectHandle ), objectTable );
	REQUIRES_MUTEX( isInUse( objectHandle ) && \
					isObjectOwner( objectHandle ), objectTable );

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(), as well as situation-specific additional checks 
	   for correct object types */
#ifndef CONFIG_FUZZ		/* To directly inject data into objects */
	status = checkAccessValid( objectHandle, checkType, 
							   CRYPT_ERROR_PERMISSION );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( status );
		}
#endif /* CONFIG_FUZZ */

	/* Perform additional checks for correct object types.  The ownership 
	   check in checkAccessValid() simply checks whether the current thread 
	   is the overall object owner, isObjectOwner() checks whether the 
	   current thread owns the lock on the object */
	if( !isInUse( objectHandle ) || !isObjectOwner( objectHandle ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( CRYPT_ERROR_PERMISSION );
		}

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ objectHandle ];
	REQUIRES_MUTEX( sanityCheckObject( objectInfoPtr ), objectTable );

	/* If it was an external access to certificate/device info or an 
	   internal access to the object's keying data, decrement the object's 
	   reference count to allow others access again */
	if( checkType == ACCESS_CHECK_EXTACCESS || \
		checkType == ACCESS_CHECK_KEYACCESS )
		{
		STORE_ORIGINAL_INT( lockCount, objectInfoPtr->lockCount );

		objectInfoPtr->lockCount--;

		/* Postcondition: The object's lock count has been decremented and 
		   is non-negative */
		ENSURES_MUTEX( objectInfoPtr->lockCount == \
							ORIGINAL_VALUE( lockCount ) - 1, objectTable );
		ENSURES_MUTEX( isIntegerRange( objectInfoPtr->lockCount ), \
					   objectTable );
		}
	else
		{
		/* It's an external access to free the object for access by others, 
		   clear the reference count */
		REQUIRES_MUTEX( checkType == ACCESS_CHECK_SUSPEND, objectTable );

		*refCount = objectInfoPtr->lockCount;
		objectInfoPtr->lockCount = 0;

		/* Postcondition: The object has been completely released */
		ENSURES_MUTEX( !isInUse( objectHandle ), objectTable );
		}

	MUTEX_UNLOCK( objectTable );
	THREAD_NOTIFY_RELEASED( objectHandle );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

CHECK_RETVAL \
int initObjectAltAccess( void )
	{
	return( CRYPT_OK );
	}

void endObjectAltAccess( void )
	{
	}

/****************************************************************************
*																			*
*						Direct Object Access Functions						*
*																			*
****************************************************************************/

/* Acquire/release an object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int krnlAcquireObject( IN_HANDLE const int objectHandle, 
					   IN_ENUM( OBJECT_TYPE ) const OBJECT_TYPE type,
					   OUT_PTR_COND void **objectPtr, 
					   IN_ERROR const int errorCode )
	{
	REQUIRES( objectPtr != NULL );	/* getObject() allows it to be NULL */
	
	return( getObject( objectHandle, type, ACCESS_CHECK_EXTACCESS, 
					   objectPtr, CRYPT_UNUSED, errorCode ) );
	}

RETVAL \
int krnlReleaseObject( IN_HANDLE const int objectHandle )
	{
	return( releaseObject( objectHandle, ACCESS_CHECK_EXTACCESS, NULL ) );
	}

/* Temporarily suspend use of an object to allow other threads access, and
   resume object use afterwards.  Note that PREfast will erroneously report 
   'attributes inconsistent with previous declaration' for these two 
   functions */

RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int krnlSuspendObject( IN_HANDLE const int objectHandle, 
					   OUT_INT_Z int *refCount )
	{
	return( releaseObject( objectHandle, ACCESS_CHECK_SUSPEND, refCount ) );
	}

CHECK_RETVAL \
int krnlResumeObject( IN_HANDLE const int objectHandle, 
					  IN_INT_Z const int refCount )
	{
	return( getObject( objectHandle, 
					   ( objectHandle == SYSTEM_OBJECT_HANDLE ) ? \
						 OBJECT_TYPE_DEVICE : OBJECT_TYPE_USER, 
					   ACCESS_CHECK_SUSPEND, NULL, refCount, 
					   CRYPT_ERROR_FAILED ) );
	}

/****************************************************************************
*																			*
*							Key Extract Functions							*
*																			*
****************************************************************************/

/* The cryptlib equivalent of trusted downgraders in other security models:
   Functions that extract a key from a context.  These functions need to
   bypass the kernel's security checking in order to allow key export and
   are the only ones that can do this.  This is an unavoidable requirement
   in the complete-isolation model - some bypass mechanism needs to be
   present in order to allow a key to be exported from an encryption action
   object.  The three functions that perform the necessary operations are:

	extractKeyData: Extract a session key from a conventional/MAC/generic-
					secret context prior to encryption with a KEK.
	exportPrivateKey: Write private key data to a stream prior to encryption
					  with a KEK.
	importPrivateKey: Read private key data from a stream after decryption
					  with a KEK.  We use this rather than a generic 
					  external private key load to avoid having the key 
					  marked as an untrusted user-set key, and also because
					  it's easier to read the key data directly into the
					  context's bignum storage rather than adding indirection
					  via a CRYPT_PKCINFO_xxx structure */

#define PKC_CONTEXT		/* Indicate that we're working with PKC context */
#if defined( INC_ALL )
  #include "context.h"
  #include "mech_int.h"
#else
  #include "context/context.h"
  #include "mechs/mech_int.h"
#endif /* Compiler-specific includes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
int extractKeyData( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
					OUT_BUFFER_FIXED( keyDataLen ) void *keyData, 
					IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int keyDataLen, 
					IN_BUFFER( accessKeyLen ) const char *accessKey, 
					IN_LENGTH_FIXED( 7 ) const int accessKeyLen )
	{
	CONTEXT_INFO *contextInfoPtr;
	int status;

	assert( isWritePtrDynamic( keyData, keyDataLen ) );
	assert( isReadPtrDynamic( accessKey, accessKeyLen ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( keyDataLen >= MIN_KEYSIZE && keyDataLen < MAX_INTLENGTH_SHORT );
	REQUIRES( accessKeyLen == 7 );

	/* Clear return values */
	memset( keyData, 0, keyDataLen );

	/* Make sure that we really intended to call this function */
	ENSURES( accessKeyLen == 7 && !memcmp( accessKey, "keydata", 7 ) );

	/* Make sure that we've been given a conventional encryption, MAC, or
	   generic-secret context with a key loaded.  This has already been 
	   checked at a higher level, but we perform a sanity check here to be 
	   safe */
	status = getObject( iCryptContext, OBJECT_TYPE_CONTEXT,
						ACCESS_CHECK_KEYACCESS,
						( void ** ) &contextInfoPtr, CRYPT_UNUSED, 
						CRYPT_ARGERROR_OBJECT );
	if( cryptStatusError( status ) )
		return( status );
	if( ( contextInfoPtr->type != CONTEXT_CONV && \
		  contextInfoPtr->type != CONTEXT_MAC && \
		  contextInfoPtr->type != CONTEXT_GENERIC ) || \
		!TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET ) )
		{
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* Export the key data from the context */
	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			if( contextInfoPtr->ctxConv->userKeyLength < MIN_KEYSIZE || \
				contextInfoPtr->ctxConv->userKeyLength > keyDataLen )
				{
				DEBUG_DIAG(( "Conventional-encryption key data is too long "
							 "to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
				REQUIRES( rangeCheck( contextInfoPtr->ctxConv->userKeyLength,
									  1, keyDataLen ) );
				memcpy( keyData, contextInfoPtr->ctxConv->userKey,
						contextInfoPtr->ctxConv->userKeyLength );
				}
			break;

		case CONTEXT_MAC:
			if( contextInfoPtr->ctxMAC->userKeyLength < MIN_KEYSIZE || \
				contextInfoPtr->ctxMAC->userKeyLength > keyDataLen )
				{
				DEBUG_DIAG(( "MAC key data is too long to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
				REQUIRES( rangeCheck( contextInfoPtr->ctxMAC->userKeyLength,
									  1, keyDataLen ) );
				memcpy( keyData, contextInfoPtr->ctxMAC->userKey,
						contextInfoPtr->ctxMAC->userKeyLength );
				}
			break;

		case CONTEXT_GENERIC:
			if( contextInfoPtr->ctxGeneric->genericSecretLength < MIN_KEYSIZE || \
				contextInfoPtr->ctxGeneric->genericSecretLength > keyDataLen )
				{
				DEBUG_DIAG(( "Generic key data is too long to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
				REQUIRES( rangeCheck( contextInfoPtr->ctxGeneric->genericSecretLength,
									  1, keyDataLen ) );
				memcpy( keyData, contextInfoPtr->ctxGeneric->genericSecret,
						contextInfoPtr->ctxGeneric->genericSecretLength );
				}
			break;

		default:
			retIntError();
		}
	releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 6 ) ) \
int exportPrivateKeyData( OUT_BUFFER_OPT( privKeyDataMaxLength, \
										  *privKeyDataLength ) \
							void *privKeyData, 
						  IN_LENGTH_SHORT_Z const int privKeyDataMaxLength,
						  OUT_LENGTH_SHORT_Z int *privKeyDataLength,
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ENUM( KEYFORMAT ) \
							const KEYFORMAT_TYPE formatType,
						  IN_BUFFER( accessKeyLen ) const char *accessKey, 
						  IN_LENGTH_FIXED( 11 ) const int accessKeyLen )
	{
	CONTEXT_INFO *contextInfoPtr;
	PKC_WRITEKEY_FUNCTION writePrivateKeyFunction;
	STREAM stream;
	int status;

	assert( ( privKeyData == NULL && privKeyDataMaxLength == 0 ) || \
			isWritePtrDynamic( privKeyData, privKeyDataMaxLength ) );
	assert( isWritePtr( privKeyDataLength, sizeof( int ) ) );
	assert( isReadPtrDynamic( accessKey, accessKeyLen ) );

	REQUIRES( ( privKeyData == NULL && privKeyDataMaxLength == 0 ) || \
			  ( privKeyData != NULL && \
				privKeyDataMaxLength >= 32 && \
				privKeyDataMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( isEnumRange( formatType, KEYFORMAT ) );
	REQUIRES( accessKeyLen == 11 );

	/* Clear return value */
	*privKeyDataLength = 0;

	/* Make sure that we really intended to call this function */
	ENSURES( accessKeyLen == 11 && !memcmp( accessKey, "private_key", 11 ) );

	/* Make sure that we've been given a PKC context with a private key
	   loaded.  This has already been checked at a higher level, but we
	   perform a sanity check here to be safe */
	status = getObject( iCryptContext, OBJECT_TYPE_CONTEXT, 
						ACCESS_CHECK_KEYACCESS,
						( void ** ) &contextInfoPtr, CRYPT_UNUSED, 
						CRYPT_ARGERROR_OBJECT );
	if( cryptStatusError( status ) )
		return( status );
	if( contextInfoPtr->type != CONTEXT_PKC || \
		!TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET ) || \
		TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY ) )
		{
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		return( CRYPT_ARGERROR_OBJECT );
		}
	writePrivateKeyFunction = ( PKC_WRITEKEY_FUNCTION ) \
			FNPTR_GET( contextInfoPtr->ctxPKC->writePrivateKeyFunction );
	if( writePrivateKeyFunction == NULL )
		{
		/* We have to use a bit of a nonstandard checking process here 
		   because a standard throw-exception check won't release the
		   object, so we synthesize a 
		   REQUIRES( writePrivateKeyFunction != NULL ) that also 
		   releases the object */
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		REQUIRES( writePrivateKeyFunction != NULL );
		}

	/* Export the key data from the context */
	if( privKeyData == NULL )
		sMemNullOpen( &stream );
	else
		sMemOpen( &stream, privKeyData, privKeyDataMaxLength );
	status = writePrivateKeyFunction( &stream, contextInfoPtr, formatType, 
									  accessKey, accessKeyLen );
	if( cryptStatusOK( status ) )
		*privKeyDataLength = stell( &stream );
	if( privKeyData == NULL )
		sMemClose( &stream );
	else
		sMemDisconnect( &stream );
	releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int importPrivateKeyData( IN_BUFFER( privKeyDataLen ) const void *privKeyData, 
						  IN_LENGTH_SHORT_MIN( 32 ) const int privKeyDataLen,
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ENUM( KEYFORMAT ) \
							const KEYFORMAT_TYPE formatType )
	{
	CONTEXT_INFO *contextInfoPtr;
	PKC_READKEY_FUNCTION readPrivateKeyFunction;
	CTX_LOADKEY_FUNCTION loadKeyFunction;
	PKC_CALCULATEKEYID_FUNCTION calculateKeyIDFunction;
	STREAM stream;
	int status;

	assert( isReadPtrDynamic( privKeyData, privKeyDataLen ) );

	REQUIRES( privKeyDataLen >= 32 && privKeyDataLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( isEnumRange( formatType, KEYFORMAT ) );

	/* Make sure that we've been given a PKC context with no private key
	   loaded.  This has already been checked at a higher level, but we
	   perform a sanity check here to be safe */
	status = getObject( iCryptContext, OBJECT_TYPE_CONTEXT, 
						ACCESS_CHECK_KEYACCESS,
						( void ** ) &contextInfoPtr, CRYPT_UNUSED, 
						CRYPT_ARGERROR_OBJECT );
	if( cryptStatusError( status ) )
		return( status );
	if( contextInfoPtr->type != CONTEXT_PKC || \
		TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET ) || \
		TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY ) )
		{
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		return( CRYPT_ARGERROR_OBJECT );
		}
	readPrivateKeyFunction = ( PKC_READKEY_FUNCTION ) \
			FNPTR_GET( contextInfoPtr->ctxPKC->readPrivateKeyFunction );
	loadKeyFunction = ( CTX_LOADKEY_FUNCTION ) \
			FNPTR_GET( contextInfoPtr->loadKeyFunction );
	calculateKeyIDFunction = ( PKC_CALCULATEKEYID_FUNCTION ) \
			FNPTR_GET( contextInfoPtr->ctxPKC->calculateKeyIDFunction );
	if( readPrivateKeyFunction == NULL || loadKeyFunction == NULL || \
		calculateKeyIDFunction == NULL )
		{
		/* We can't use a standard REQUIRES() here because it won't release 
		   the object, so we need to manually perform the equivalent */
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		retIntError();
		}

	/* Import the key data into the context */
	sMemConnect( &stream, privKeyData, privKeyDataLen );
	status = readPrivateKeyFunction( &stream, contextInfoPtr, formatType, 
									 FALSE );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		{
		/* If everything went OK, perform an internal load that uses the
		   values already present in the context */
		status = loadKeyFunction( contextInfoPtr, NULL, 0 );
		if( cryptStatusOK( status ) && formatType == KEYFORMAT_PRIVATE_OLD )
			{
			/* This format is unusual in that it stores the public-key data 
			   in encrypted form alongside the private-key data, so that the 
			   public key is read as part of the private key rather than 
			   being set as a CRYPT_IATTRIBUTE_KEY_xxx_PARTIAL attribute.  
			   Because of this it bypasses the standard keyID-calculation
			   process that occurs on public-key load, so we have to 
			   explicitly calculate the keyID data here */
			status = calculateKeyIDFunction( contextInfoPtr );
			}
		if( cryptStatusOK( status ) && formatType == KEYFORMAT_PRIVATE )
			{
			/* The in-memory key components are now checksummed (via 
			   loadKeyFunction()), if the format supports it verify that the 
			   components correspond to the original key data.  This is done
			   by performing a dummy read of the key data that compares it
			   against what was previously read */
			sMemConnect( &stream, privKeyData, privKeyDataLen );
			status = readPrivateKeyFunction( &stream, contextInfoPtr, 
											 formatType, TRUE );
			sMemDisconnect( &stream );
			}
		if( cryptStatusOK( status ) )
			{
			krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, 
							 MESSAGE_VALUE_UNUSED, 
							 CRYPT_IATTRIBUTE_INITIALISED );
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET );
			}
		else
			{
			/* If the problem was indicated as a function argument error, 
			   map it to a more appropriate code.  The most appropriate code 
			   at this point is CRYPT_ERROR_INVALID since a bad-data error 
			   will be returned as an explicit CRYPT_ERROR_BADDATA while a 
			   parameter error indicates that one or more of the key 
			   components failed a validity check */
			if( cryptArgError( status ) )
				status = CRYPT_ERROR_INVALID;
			}
		}
	releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
	return( status );
	}
