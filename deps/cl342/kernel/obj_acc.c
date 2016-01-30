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

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

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
	OBJECT_INFO *objectTable = krnlData->objectTable, *objectInfoPtr;

	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( checkType > ACCESS_CHECK_NONE && \
			  checkType < ACCESS_CHECK_LAST );
	REQUIRES( cryptStatusError( errorCode ) );

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(): It's a valid object owned by the calling
	   thread */
	if( !isValidObject( objectHandle ) || \
		!checkObjectOwnership( objectTable[ objectHandle ] ) )
		return( errorCode );

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ objectHandle ];

	/* Make sure that the object access is valid */
	switch( objectInfoPtr->type )
		{
		case OBJECT_TYPE_CONTEXT:
			/* Used when exporting/importing keying info, valid for contexts 
			   with keys when called from within the kernel */
			if( checkType != ACCESS_CHECK_KEYACCESS )
				return( errorCode );
			if( !isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_CONV ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_MAC ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_PKC ) && \
				!isValidSubtype( objectInfoPtr->subType, SUBTYPE_CTX_GENERIC ) )
				return( errorCode );
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
	ENSURES( ( checkType == ACCESS_CHECK_EXTACCESS && \
			   ( objectInfoPtr->type == OBJECT_TYPE_CERTIFICATE || \
				  objectInfoPtr->type == OBJECT_TYPE_DEVICE ) ) || \
			 ( checkType == ACCESS_CHECK_KEYACCESS && \
			   objectInfoPtr->type == OBJECT_TYPE_CONTEXT ) || \
			 ( checkType == ACCESS_CHECK_SUSPEND && \
			   ( objectInfoPtr->type == OBJECT_TYPE_DEVICE || \
				 objectInfoPtr->type == OBJECT_TYPE_USER ) ) );

	return( CRYPT_OK );
	}

/* Get a pointer to an object's data from its handle */

CHECK_RETVAL \
static int getObject( IN_HANDLE const int objectHandle, 
					  IN_ENUM( OBJECT ) const OBJECT_TYPE type,
					  IN_ENUM( ACCESS_CHECK ) const ACCESS_CHECK_TYPE checkType, 
					  OUT_OPT_PTR_OPT void **objectPtr, 
					  IN_INT_Z const int refCount, 
					  IN_ERROR const int errorCode )
	{
	OBJECT_INFO *objectTable, *objectInfoPtr;
	int status;

	assert( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
				objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
			  objectPtr == NULL && refCount > 0 ) || \
			( !( objectHandle == SYSTEM_OBJECT_HANDLE || \
				 objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
			  isWritePtr( objectPtr, sizeof( void * ) ) && \
			  refCount == CRYPT_UNUSED ) );

	/* Preconditions: It's a valid object */
	REQUIRES( isValidHandle( objectHandle ) );
	REQUIRES( isValidType( type ) && \
			  ( type == OBJECT_TYPE_CONTEXT || \
				type == OBJECT_TYPE_CERTIFICATE || \
				type == OBJECT_TYPE_DEVICE || type == OBJECT_TYPE_USER ) );
	REQUIRES( checkType > ACCESS_CHECK_NONE && \
			  checkType < ACCESS_CHECK_LAST );
	REQUIRES( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
				  objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
				objectPtr == NULL && refCount > 0 ) || \
			  ( !( objectHandle == SYSTEM_OBJECT_HANDLE || \
				   objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
				objectPtr != NULL && refCount == CRYPT_UNUSED ) );

	/* Clear the return value */
	if( objectPtr != NULL )
		*objectPtr = NULL;

	THREAD_NOTIFY_PREPARE( objectHandle );
	MUTEX_LOCK( objectTable );
	objectTable = krnlData->objectTable;

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(), as well as situation-specific additional checks 
	   for correct object types */
	status = checkAccessValid( objectHandle, checkType, errorCode );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( status );
		}

	/* Perform additional checks for correct object types */
	if( ( ( objectHandle == SYSTEM_OBJECT_HANDLE || \
			objectHandle == DEFAULTUSER_OBJECT_HANDLE ) && \
		  objectPtr != NULL ) || \
		objectTable[ objectHandle ].type != type )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( errorCode );
		}

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ objectHandle ];

	/* Inner precondition: The object is of the requested type */
	REQUIRES( objectInfoPtr->type == type && \
			 ( objectInfoPtr->type == OBJECT_TYPE_CONTEXT || \
			   objectInfoPtr->type == OBJECT_TYPE_CERTIFICATE || \
			   objectInfoPtr->type == OBJECT_TYPE_DEVICE || \
			   objectInfoPtr->type == OBJECT_TYPE_USER ) );

	/* If the object is busy, wait for it to become available */
	if( isInUse( objectHandle ) && !isObjectOwner( objectHandle ) )
		status = waitForObject( objectHandle, &objectInfoPtr );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		return( status );
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
		REQUIRES( checkType == ACCESS_CHECK_SUSPEND );
		REQUIRES( objectInfoPtr->lockCount == 0 );
		REQUIRES( refCount > 0 && refCount < 100 );

		objectInfoPtr->lockCount = refCount;
		}
#ifdef USE_THREADS
	objectInfoPtr->lockOwner = THREAD_SELF();
#endif /* USE_THREADS */
	if( objectPtr != NULL )
		*objectPtr = objectInfoPtr->objectPtr;

	MUTEX_UNLOCK( objectTable );
	THREAD_NOTIFY_ACQUIRED( objectHandle );
	return( status );
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
	OBJECT_INFO *objectTable, *objectInfoPtr;
	int status;
	DECLARE_ORIGINAL_INT( lockCount );

	assert( ( ( checkType == ACCESS_CHECK_EXTACCESS || \
				checkType == ACCESS_CHECK_KEYACCESS ) && \
			  refCount == NULL ) || \
			( checkType == ACCESS_CHECK_SUSPEND && \
			  isWritePtr( refCount, sizeof( int ) ) ) );

	/* Preconditions */
	REQUIRES( checkType > ACCESS_CHECK_NONE && \
			  checkType < ACCESS_CHECK_LAST );
	REQUIRES( ( ( checkType == ACCESS_CHECK_EXTACCESS || \
				  checkType == ACCESS_CHECK_KEYACCESS ) && \
				refCount == NULL ) || \
			  ( checkType == ACCESS_CHECK_SUSPEND && \
				refCount != NULL ) );

	THREAD_NOTIFY_PREPARE( objectHandle );
	MUTEX_LOCK( objectTable );
	objectTable = krnlData->objectTable;

	/* Preconditions: It's a valid object in use by the caller.  Since these 
	   checks require access to the object table we can only perform them 
	   after we've locked it */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( isInUse( objectHandle ) && isObjectOwner( objectHandle ) );

	/* Perform similar access checks to the ones performed in
	   krnlSendMessage(), as well as situation-specific additional checks 
	   for correct object types */
	status = checkAccessValid( objectHandle, checkType, 
							   CRYPT_ERROR_PERMISSION );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		THREAD_NOTIFY_CANCELLED( objectHandle );
		retIntError_Ext( status );
		}

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
		ENSURES( objectInfoPtr->lockCount == \
								ORIGINAL_VALUE( lockCount ) - 1 );
		ENSURES( objectInfoPtr->lockCount >= 0 && \
				 objectInfoPtr->lockCount < MAX_INTLENGTH );
		}
	else
		{
		/* It's an external access to free the object for access by others, 
		   clear the reference count */
		REQUIRES( checkType == ACCESS_CHECK_SUSPEND );

		*refCount = objectInfoPtr->lockCount;
		objectInfoPtr->lockCount = 0;

		/* Postcondition: The object has been completely released */
		ENSURES( !isInUse( objectHandle ) );
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initObjectAltAccess( INOUT KERNEL_DATA *krnlDataPtr )
	{
	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	return( CRYPT_OK );
	}

void endObjectAltAccess( void )
	{
	krnlData = NULL;
	}

/****************************************************************************
*																			*
*						Direct Object Access Functions						*
*																			*
****************************************************************************/

/* Acquire/release an object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int krnlAcquireObject( IN_HANDLE const int objectHandle, 
					   IN_ENUM( OBJECT ) const OBJECT_TYPE type,
					   OUT_OPT_PTR void **objectPtr, 
					   IN_ERROR const int errorCode )
	{
	REQUIRES( objectPtr != NULL );	/* getObject() allows it to be NULL */
	
	return( getObject( objectHandle, type, ACCESS_CHECK_EXTACCESS, 
					   objectPtr, CRYPT_UNUSED, errorCode ) );
	}

int krnlReleaseObject( IN_HANDLE const int objectHandle )
	{
	return( releaseObject( objectHandle, ACCESS_CHECK_EXTACCESS, NULL ) );
	}

/* Temporarily suspend use of an object to allow other threads access, and
   resume object use afterwards.  Note that PREfast will erroneously report 
   'attributes inconsistent with previous declaration' for these two 
   functions */

STDC_NONNULL_ARG( ( 2 ) ) \
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

	assert( isWritePtr( keyData, keyDataLen ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

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
		!( contextInfoPtr->flags & CONTEXT_FLAG_KEY_SET ) )
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
				DEBUG_DIAG(( "Key data is too long to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
				memcpy( keyData, contextInfoPtr->ctxConv->userKey,
						contextInfoPtr->ctxConv->userKeyLength );
				}
			break;

		case CONTEXT_MAC:
			if( contextInfoPtr->ctxMAC->userKeyLength < MIN_KEYSIZE || \
				contextInfoPtr->ctxMAC->userKeyLength > keyDataLen )
				{
				DEBUG_DIAG(( "Key data is too long to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
				memcpy( keyData, contextInfoPtr->ctxMAC->userKey,
						contextInfoPtr->ctxMAC->userKeyLength );
				}
			break;

		case CONTEXT_GENERIC:
			if( contextInfoPtr->ctxGeneric->genericSecretLength < MIN_KEYSIZE || \
				contextInfoPtr->ctxGeneric->genericSecretLength > keyDataLen )
				{
				DEBUG_DIAG(( "Key data is too long to export" ));
				assert( DEBUG_WARN );
				status = CRYPT_ERROR_OVERFLOW;
				}
			else
				{
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int exportPrivateKeyData( INOUT STREAM *stream, 
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ENUM( KEYFORMAT ) const KEYFORMAT_TYPE formatType,
						  IN_BUFFER( accessKeyLen ) const char *accessKey, 
						  IN_LENGTH_FIXED( 11 ) const int accessKeyLen )
	{
	CONTEXT_INFO *contextInfoPtr;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 11 );

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
		!( contextInfoPtr->flags & CONTEXT_FLAG_KEY_SET ) || \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) )
		{
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* Export the key data from the context */
	status = contextInfoPtr->ctxPKC->writePrivateKeyFunction( stream, 
											contextInfoPtr, formatType, 
											accessKey, accessKeyLen );
	releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int importPrivateKeyData( INOUT STREAM *stream, 
						  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						  IN_ENUM( KEYFORMAT ) \
						  const KEYFORMAT_TYPE formatType )
	{
	CONTEXT_INFO *contextInfoPtr;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );

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
		( contextInfoPtr->flags & CONTEXT_FLAG_KEY_SET ) || \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) )
		{
		releaseObject( iCryptContext, ACCESS_CHECK_KEYACCESS, NULL );
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* Import the key data into the context */
	status = contextInfoPtr->ctxPKC->readPrivateKeyFunction( stream, 
											contextInfoPtr, formatType );
	if( cryptStatusOK( status ) )
		{
		/* If everything went OK, perform an internal load that uses the
		   values already present in the context */
		status = contextInfoPtr->loadKeyFunction( contextInfoPtr, NULL, 0 );
		if( cryptStatusOK( status ) && formatType == KEYFORMAT_PRIVATE_OLD )
			{
			/* This format is unusual in that it stores the public-key data 
			   in encrypted form alongside the private-key data, so that the 
			   public key is read as part of the private key rather than 
			   being set as a CRYPT_IATTRIBUTE_KEY_xxx_PARTIAL attribute.  
			   Because of this it bypasses the standard keyID-calculation
			   process that occurs on public-key load, so we have to 
			   explicitly calculate the keyID data here */
			status = contextInfoPtr->ctxPKC->calculateKeyIDFunction( contextInfoPtr );
			}
		if( cryptStatusOK( status ) )
			{
			krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, 
							 MESSAGE_VALUE_UNUSED, 
							 CRYPT_IATTRIBUTE_INITIALISED );
			contextInfoPtr->flags |= CONTEXT_FLAG_KEY_SET;
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
