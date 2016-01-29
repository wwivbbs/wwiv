/****************************************************************************
*																			*
*							Kernel Message Dispatcher						*
*						Copyright Peter Gutmann 1997-2012					*
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

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

/* The ACL used to check objects passed as message parameters, in this case
   for certificate sign/sig-check messages */

static const MESSAGE_ACL FAR_BSS messageParamACLTbl[] = {
	/* Certificates can only be signed by (private-key) PKC contexts */
	{ MESSAGE_CRT_SIGN,
	  { ST_CTX_PKC,
		ST_NONE, ST_NONE } },

	/* Signatures can be checked with a raw PKC context or a certificate 
	   (but specifically not a certificate chain, see the long discussion
	   in certs/certschk.c for details on this).  The object being checked 
	   can also be checked against a CRL or CRL-equivalent like an RTCS or
	   OCSP response, against revocation data in a certificate store, or 
	   against an RTCS or OCSP responder */
	{ MESSAGE_CRT_SIGCHECK,
	  { ST_CTX_PKC | MKTYPE_CERTIFICATES( ST_CERT_CERT ) | \
			MKTYPE_CERTREV( ST_CERT_CRL ) | MKTYPE_CERTVAL( ST_CERT_RTCS_RESP ) | \
			MKTYPE_CERTREV( ST_CERT_OCSP_RESP ),
	    MKTYPE_DBMS( ST_KEYSET_DBMS ) | MKTYPE_DBMS( ST_KEYSET_DBMS_STORE ),
		MKTYPE_RTCS( ST_SESS_RTCS ) | MKTYPE_OCSP( ST_SESS_OCSP ) } },

	/* End-of-ACL marker */
	{ MESSAGE_NONE, { ST_NONE, ST_NONE, ST_NONE } },
	{ MESSAGE_NONE, { ST_NONE, ST_NONE, ST_NONE } }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sometimes a message is explicitly non-routable (i.e. it has to be sent
   directly to the appropriate target object).  The following function checks
   that the target object is one of the required types */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int checkTargetType( IN_HANDLE const CRYPT_HANDLE originalObjectHandle, 
					 OUT_HANDLE_OPT CRYPT_HANDLE *targetObjectHandle,
					 const long targets )
	{
	const OBJECT_TYPE target = targets & 0xFF;
	const OBJECT_TYPE altTarget = targets >> 8;
	OBJECT_INFO *objectTable = krnlData->objectTable;

	/* Precondition: Source is a valid object, destination(s) are valid
	   target(s) */
	REQUIRES( isValidObject( originalObjectHandle ) );
	REQUIRES( isValidType( target ) );
	REQUIRES( altTarget == OBJECT_TYPE_NONE || isValidType( altTarget ) );

	/* Clear return value */
	*targetObjectHandle = CRYPT_ERROR;

	/* Check whether the object matches the required type.  We don't have to
	   check whether the alternative target has a value or not since the
	   object can never be a OBJECT_TYPE_NONE */
	if( !isValidObject( originalObjectHandle ) || \
		( objectTable[ originalObjectHandle ].type != target && \
		  objectTable[ originalObjectHandle ].type != altTarget ) )
		return( CRYPT_ERROR );

	/* Postcondition */
	ENSURES( objectTable[ originalObjectHandle ].type == target || \
			 objectTable[ originalObjectHandle ].type == altTarget );

	*targetObjectHandle = originalObjectHandle;
	return( CRYPT_OK );
	}

/* Find the ACL for a parameter object */

CHECK_RETVAL_PTR \
static const MESSAGE_ACL *findParamACL( IN_MESSAGE const MESSAGE_TYPE message )
	{
	int i;

	/* Precondition: It's a message that takes an object parameter */
	REQUIRES_N( isParamMessage( message ) );

	/* Find the ACL entry for this message type.  There's no need to 
	   explicitly handle the internal-error condition since any loop
	   exit is treated as an error */
	for( i = 0; messageParamACLTbl[ i ].type != MESSAGE_NONE && \
				i < FAILSAFE_ARRAYSIZE( messageParamACLTbl, MESSAGE_ACL ); 
		 i++ )
		{
		if( messageParamACLTbl[ i ].type == message )
			return( &messageParamACLTbl[ i ] );
		}

	retIntError_Null();
	}

/* Wait for an object to become available so that we can use it, with a 
   timeout for blocked objects (dulcis et alta quies placidaeque similima 
   morti).  We spin for WAITCOUNT_SLEEP_THRESHOLD turns, then sleep (see
   the comment in waitForObject() for more on this), and finally bail out
   once MAX_WAITCOUNT is reached.  
   
   This is an internal function that's used when mapping an object handle to 
   object data, and is never called directly.  
   
   As an aid in identifying objects acting as bottlenecks, we provide a 
   function to warn about excessive waiting, along with information on the 
   object that was waited on, in debug mode.  A wait count threshold of 
   100 is generally high enough to avoid false positives caused by (for 
   example) network subsystem delays */

#define WAITCOUNT_SLEEP_THRESHOLD		100
#define WAITCOUNT_WARN_THRESHOLD		100
#define MAX_WAITCOUNT					1000

#if !defined( NDEBUG )

/* Get a text description of an object associated with a given object 
   handle */

static void getObjectDescription( IN_HANDLE const int objectHandle, 
								  OUT char *description )
	{
	static const char *objectTypeNames[] = {
		"none", "context", "keyset", "envelope", "certificate", "device",
		"session", "user", "none", "none"
		};
	typedef struct {
		const OBJECT_SUBTYPE subType;
		const char *description;
		} OBJECT_DESCRIPTION_MAP;
	static const OBJECT_DESCRIPTION_MAP descriptionMap[] = {
		{ SUBTYPE_CTX_CONV, "conventional encryption" },
		{ SUBTYPE_CTX_PKC, "public-key encryption" },
		{ SUBTYPE_CTX_HASH, "hash" },
		{ SUBTYPE_CTX_MAC, "MAC" },
		{ SUBTYPE_CTX_GENERIC, "generic" },
		{ SUBTYPE_CERT_CERT, "certificate" },
		{ SUBTYPE_CERT_CERTREQ, "PKCS #10 cert.request" },
		{ SUBTYPE_CERT_REQ_CERT, "CRMF cert.request" },
		{ SUBTYPE_CERT_REQ_REV, "CRMF rev.request" },
		{ SUBTYPE_CERT_CERTCHAIN, "cert.chain" },
		{ SUBTYPE_CERT_ATTRCERT, "attribute cert." },
		{ SUBTYPE_CERT_CRL, "CRK" },
		{ SUBTYPE_CERT_CMSATTR, "CMS attributes" },
		{ SUBTYPE_CERT_RTCS_REQ, "RTCS request" },
		{ SUBTYPE_CERT_RTCS_RESP, "RTCS response" },
		{ SUBTYPE_CERT_OCSP_REQ, "OCSP request" },
		{ SUBTYPE_CERT_OCSP_RESP, "OCSP response" },
		{ SUBTYPE_CERT_PKIUSER, "PKI user" },
		{ SUBTYPE_ENV_ENV, "PKCS #7/CMS envelope" },
		{ SUBTYPE_ENV_ENV_PGP, "PGP envelope" },
		{ SUBTYPE_ENV_DEENV, "de-envelope" },
		{ SUBTYPE_KEYSET_FILE, "file" },
		{ SUBTYPE_KEYSET_FILE_PARTIAL, "file (partial)" },
		{ SUBTYPE_KEYSET_FILE_READONLY, "file (readonly)" },
		{ SUBTYPE_KEYSET_DBMS, "database" },
		{ SUBTYPE_KEYSET_DBMS_STORE, "database store" },
		{ SUBTYPE_KEYSET_HTTP, "HTTP" },
		{ SUBTYPE_KEYSET_LDAP, "LDAP" },
		{ SUBTYPE_DEV_SYSTEM, "system" },
		{ SUBTYPE_DEV_PKCS11, "PKCS #11" },
		{ SUBTYPE_DEV_CRYPTOAPI, "CryptoAPI" },
		{ SUBTYPE_DEV_HARDWARE, "hardware" },
		{ SUBTYPE_SESSION_SSH, "SSH" },
		{ SUBTYPE_SESSION_SSH_SVR, "SSH server" },
		{ SUBTYPE_SESSION_SSL, "SSL" },
		{ SUBTYPE_SESSION_SSL_SVR, "SSL server" },
		{ SUBTYPE_SESSION_RTCS, "RTCS" },
		{ SUBTYPE_SESSION_RTCS_SVR, "RTCS server" },
		{ SUBTYPE_SESSION_OCSP, "OCSP" },
		{ SUBTYPE_SESSION_OCSP_SVR, "OCSP server" },
		{ SUBTYPE_SESSION_TSP, "TSP" },
		{ SUBTYPE_SESSION_TSP_SVR, "TSP server" },
		{ SUBTYPE_SESSION_CMP, "CMP" },
		{ SUBTYPE_SESSION_CMP_SVR, "CMP server" },
		{ SUBTYPE_SESSION_SCEP, "SCEP" },
		{ SUBTYPE_SESSION_SCEP_SVR, "SCEP server" },
		{ SUBTYPE_SESSION_CERT_SVR, "cerificate store" },
		{ SUBTYPE_USER_SO, "SO user" },
		{ SUBTYPE_USER_NORMAL, "standard user" },
		{ SUBTYPE_USER_CA, "CA user" },
		{ SUBTYPE_NONE, "NONE" }, { SUBTYPE_NONE, "NONE" },
		};
	const OBJECT_INFO *objectInfoPtr = &krnlData->objectTable[ objectHandle ];
	int i;

	assert( isValidObject( objectHandle ) );

	REQUIRES_V( isValidType( objectInfoPtr->type ) );

	if( objectHandle == SYSTEM_OBJECT_HANDLE )
		{
		strlcpy_s( description, 128, "system object" );
		return;
		}
	if( objectHandle == DEFAULTUSER_OBJECT_HANDLE )
		{
		strlcpy_s( description, 128, "default user object" );
		return;
		}
	for( i = 0; descriptionMap[ i ].subType != objectInfoPtr->subType && \
				descriptionMap[ i ].subType != SUBTYPE_NONE; i++ );
	sprintf_s( description, 128, "object %d (%s/%s)", objectHandle, 
			   objectTypeNames[ objectInfoPtr->type ],
			   descriptionMap[ i ].description );
	}

/* Non thread-safe version of the above that can be used directly in
   printf() statements.  This isn't really that bad, firstly it's mostly 
   only called for diagnostics during startup/shutdown where there's 
   guaranteed to be only one thread active, and secondly it uses TLS
   where possible which means it'll only be really non-thread-safe on
   systems that don't have threading anyway (embedded systems that use
   the tasking model where there's typically only one task anyway) */

#if defined( _MSC_VER )
  #define THREAD_STORAGE_STATIC		__declspec( thread ) static
#elif defined( __GNUC__ ) || defined( __clang__ ) || \
	  defined( __SUNPRO_C ) || defined( __xlc__ )
  #define THREAD_STORAGE_STATIC		static __thread
#else
  #define THREAD_STORAGE_STATIC
#endif /* Compiler-specific TLS */
#if defined( __APPLE__ )
  #undef THREAD_STORAGE_STATIC
  #define THREAD_STORAGE_STATIC
#endif /* OS X */

const char *getObjectDescriptionNT( IN_HANDLE const int objectHandle )
	{
	THREAD_STORAGE_STATIC char buffer[ 128 ];

	getObjectDescription( objectHandle, buffer );
	return( buffer );
	}

/* Warn about an excessive wait for an object to become available */

static void waitWarn( IN_HANDLE const int objectHandle, 
					  IN_INT const int waitCount )
	{
	char description[ 128 + 8 ];

	assert( isValidObject( objectHandle ) );
	assert( waitCount > WAITCOUNT_WARN_THRESHOLD && \
			waitCount <= MAX_WAITCOUNT );

	getObjectDescription( objectHandle, description );
	DEBUG_PRINT(( "\nWarning: Thread %lX waited %d iteration%s for %s.\n",
				  ( unsigned long ) THREAD_SELF(), waitCount, 
				  ( waitCount == 1 ) ? "" : "s", description ));
	}
#endif /* Debug mode only */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int waitForObject( IN_HANDLE const int objectHandle, 
				   OUT_PTR_COND OBJECT_INFO **objectInfoPtrPtr )
	{
	OBJECT_INFO *objectTable = krnlData->objectTable;
	const int uniqueID = objectTable[ objectHandle ].uniqueID;
	int waitCount = 0;

	/* Preconditions: The object is in use by another thread */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( isInUse( objectHandle ) && !isObjectOwner( objectHandle ) );

	/* Clear return value */
	*objectInfoPtrPtr = NULL;

	/* While the object is busy, put the thread to sleep (Pauzele lungi si
	   dese; Cheia marilor succese).  This is the only really portable way
	   to wait on the resource, which gives up this thread's timeslice to
	   allow other threads (including the one using the object) to run.
	   Somewhat better methods methods such as mutexes with timers are
	   difficult to manage portably across different platforms.

	   Even this can cause problems in some circumstances.  The idea behind 
	   the mechanism below is that by yielding the CPU, whichever thread 
	   currently holds the object gets to finish with it and then the 
	   current thread resumes.  However if there's a thundering-herd 
	   situation where a dozen other threads are waiting on the lock and 
	   they've voluntarily yielded the CPU and the scheduler prioritises 
	   them above other threads because of this then they'll all fight for 
	   the lock and so the thread that holds the object that they're waiting 
	   on never gets to run.
	   
	   This seems somewhat unlikely, but it's cropped up on multicore 
	   hyperthreaded Linux machines running large numbers of threads, 
	   probably because of the thread-scheduling pecularities of HT CPUs
	   combined with the thread-scheduling peculiarities of Linux (it
	   doesn't occur on the same systems running Windows or other OSes).  
	   The problem seems to be that if a thread yields on a CPU other than
	   the one that holds the resource and no other threads are waiting to
	   run then it's immediately re-scheduled, because the yield only 
	   applies to threads on the same CPU.

	   To deal with this we turn the basic thread-timeslice-yield into a 
	   more aggressive thread-sleep (which really does yield the CPU, even
	   with the thread-scheduling described above) if 
	   WAITCOUNT_SLEEP_THRESHOLD yields are exceeded */
	while( isValidObject( objectHandle ) && \
		   objectTable[ objectHandle ].uniqueID == uniqueID && \
		   isInUse( objectHandle ) && waitCount < MAX_WAITCOUNT && \
		   krnlData->shutdownLevel < SHUTDOWN_LEVEL_MESSAGES )
		{
		objectTable = NULL;
		MUTEX_UNLOCK( objectTable );
		waitCount++;
		THREAD_YIELD();
		if( waitCount > WAITCOUNT_SLEEP_THRESHOLD )
			{
			/* We've waited for over WAITCOUNT_SLEEP_THRESHOLD thread
			   timeslices, explicitly put the thread to sleep rather than 
			   just yielding its timeslice */
			THREAD_SLEEP( 1 );
			}
		MUTEX_LOCK( objectTable );
		objectTable = krnlData->objectTable;
		}
#if !defined( NDEBUG ) && !defined( __WIN16__ )
	if( waitCount > WAITCOUNT_WARN_THRESHOLD )
		{
		/* If we waited more than WAITCOUNT_WARN_THRESHOLD iterations for
		   something this could be a sign of a resource usage bottleneck
		   (typically caused by users who don't understand threading), warn
		   the user that there's a potential problem */
		waitWarn( objectHandle, waitCount );
		}
#endif /* NDEBUG on systems with stdio */

	/* If cryptlib is shutting down, exit */
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES )
		return( CRYPT_ERROR_PERMISSION );

	/* If we timed out waiting for the object, return a timeout error */
	if( waitCount >= MAX_WAITCOUNT )
		{
		DEBUG_DIAG(( "Object wait exceeded %d iterations", MAX_WAITCOUNT ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_TIMEOUT );
		}

	/* Make sure that nothing happened to the object while we were waiting
	   on it */
	if( !isValidObject( objectHandle ) || \
		objectTable[ objectHandle ].uniqueID != uniqueID )
		return( CRYPT_ERROR_SIGNALLED );

	/* Update the object info pointer in case the object table was updated
	   while we had yielded control */
	*objectInfoPtrPtr = &objectTable[ objectHandle ];

	/* Postconditions: The object is available for use */
	ENSURES( isValidObject( objectHandle ) );
	ENSURES( !isInUse( objectHandle ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Message Routing							*
*																			*
****************************************************************************/

/* Find the ultimate target of an object attribute manipulation message by
   walking down the chain of controlling -> dependent objects.  For example
   a message targeted at a device and sent to a certificate would be routed
   to the certificate's dependent object (which would typically be a 
   context).  The device message targeted at the context would in turn be 
   routed to the context's dependent device, which is its final destination */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int findTargetType( IN_HANDLE const CRYPT_HANDLE originalObjectHandle, 
					OUT_HANDLE_OPT CRYPT_HANDLE *targetObjectHandle,
					const long targets )
	{
	const OBJECT_TYPE target = targets & 0xFF;
	const OBJECT_TYPE altTarget1 = ( targets >> 8 ) & 0xFF;
	const OBJECT_TYPE altTarget2 = ( targets >> 16 ) & 0xFF;
	OBJECT_INFO *objectTable = krnlData->objectTable;
	OBJECT_TYPE type = objectTable[ originalObjectHandle ].type;
	int objectHandle = originalObjectHandle, iterations;

	/* Preconditions: Source is a valid object, destination(s) are valid
	   target(s) */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( isValidType( target ) );
	REQUIRES( altTarget1 == OBJECT_TYPE_NONE || isValidType( altTarget1 ) );
	REQUIRES( altTarget2 == OBJECT_TYPE_NONE || isValidType( altTarget2 ) );

	/* Clear return value */
	*targetObjectHandle = CRYPT_ERROR;

	/* Route the request through any dependent objects as required until we
	   reach the required target object type.  "And thou shalt make
	   loops..." -- Exodus 26:4 */
	for( iterations = 0; \
		 iterations < 3 && isValidObject( objectHandle ) && \
		   !( target == type || \
			  ( altTarget1 != OBJECT_TYPE_NONE && altTarget1 == type ) || \
			  ( altTarget2 != OBJECT_TYPE_NONE && altTarget2 == type ) ); \
		 iterations++ )
		{
		/* Loop invariants.  "Fifty loops thou shalt make" -- Exodus 26:5
		   (some of the OT verses shouldn't be taken too literally,
		   apparently the 50 used here merely means "many" as in "more than
		   one or two" in the same way that "40 days and nights" is now
		   generally taken as meaning "Lots, but that's as far as we're
		   prepared to count") */
		ENSURES( isValidObject( objectHandle ) );
		ENSURES( iterations < 3 );

		/* Find the next potential target object */
		if( target == OBJECT_TYPE_DEVICE && \
			objectTable[ objectHandle ].dependentDevice != CRYPT_ERROR )
			objectHandle = objectTable[ objectHandle ].dependentDevice;
		else
			{
			if( target == OBJECT_TYPE_USER )
				{
				/* If we've reached the system object (the parent of all 
				   other objects) we can't go any further */
				objectHandle = ( objectHandle != SYSTEM_OBJECT_HANDLE ) ? \
							   objectTable[ objectHandle ].owner : CRYPT_ERROR;
				}
			else
				objectHandle = objectTable[ objectHandle ].dependentObject;
			}
		if( isValidObject( objectHandle ) )
			type = objectTable[ objectHandle ].type;

		/* If we've got a new object, it has the same owner as the original
		   target candidate */
		ENSURES( !isValidObject( objectHandle ) || \
				 isSameOwningObject( originalObjectHandle, objectHandle ) || \
				 objectTable[ originalObjectHandle ].owner == objectHandle );
		}
	ENSURES( iterations < 3 );
	if( !isValidObject( objectHandle ) )
		return( CRYPT_ARGERROR_OBJECT );

	/* Postcondition: We've reached the target object */
	ENSURES( isValidObject( objectHandle ) && \
			 ( isSameOwningObject( originalObjectHandle, objectHandle ) || \
			   objectTable[ originalObjectHandle ].owner == objectHandle ) && \
			 ( target == type || \
			   ( altTarget1 != OBJECT_TYPE_NONE && altTarget1 == type ) || \
			   ( altTarget2 != OBJECT_TYPE_NONE && altTarget2 == type ) ) );


	*targetObjectHandle = objectHandle;
	return( CRYPT_OK );
	}

/* Find the ultimate target of a compare message by walking down the chain
   of controlling -> dependent objects.  For example a message targeted at a
   device and sent to a certificate would be routed to the certificate's 
   dependent object (which would typically be a context).  The device 
   message targeted at the context would be routed to the context's 
   dependent device, which is its final destination */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int routeCompareMessageTarget( IN_HANDLE const CRYPT_HANDLE originalObjectHandle, 
									  OUT_HANDLE_OPT CRYPT_HANDLE *targetObjectHandle,
									  IN_ENUM( MESSAGE_COMPARE ) \
											const long messageValue )
	{
	OBJECT_TYPE targetType = OBJECT_TYPE_NONE;
	int status;

	/* Preconditions */
	REQUIRES( isValidObject( originalObjectHandle ) );
	REQUIRES( messageValue == MESSAGE_COMPARE_HASH || \
			  messageValue == MESSAGE_COMPARE_ICV || \
			  messageValue == MESSAGE_COMPARE_KEYID || \
			  messageValue == MESSAGE_COMPARE_KEYID_PGP || \
			  messageValue == MESSAGE_COMPARE_KEYID_OPENPGP || \
			  messageValue == MESSAGE_COMPARE_SUBJECT || \
			  messageValue == MESSAGE_COMPARE_ISSUERANDSERIALNUMBER || \
			  messageValue == MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER || \
			  messageValue == MESSAGE_COMPARE_FINGERPRINT_SHA1 || \
			  messageValue == MESSAGE_COMPARE_FINGERPRINT_SHA2 || \
			  messageValue == MESSAGE_COMPARE_FINGERPRINT_SHAng || \
			  messageValue == MESSAGE_COMPARE_CERTOBJ );

	/* Clear return value */
	*targetObjectHandle = CRYPT_ERROR;

	/* Determine the ultimate target type for the message.  We don't check for
	   keysets, envelopes and sessions as dependent objects since this never
	   occurs */
	switch( messageValue )
		{
		case MESSAGE_COMPARE_HASH:
		case MESSAGE_COMPARE_ICV:
		case MESSAGE_COMPARE_KEYID:
		case MESSAGE_COMPARE_KEYID_PGP:
		case MESSAGE_COMPARE_KEYID_OPENPGP:
			targetType = OBJECT_TYPE_CONTEXT;
			break;

		case MESSAGE_COMPARE_SUBJECT:
		case MESSAGE_COMPARE_ISSUERANDSERIALNUMBER:
		case MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER:
		case MESSAGE_COMPARE_FINGERPRINT_SHA1:
		case MESSAGE_COMPARE_FINGERPRINT_SHA2:
		case MESSAGE_COMPARE_FINGERPRINT_SHAng:
		case MESSAGE_COMPARE_CERTOBJ:
			targetType = OBJECT_TYPE_CERTIFICATE;
			break;

		default:
			retIntError();
		}

	/* Route the message through to the appropriate object */
	status = findTargetType( originalObjectHandle, targetObjectHandle, 
							 targetType );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_OBJECT );

	/* Postcondition: We've found a valid target object */
	ENSURES( isValidObject( *targetObjectHandle ) && \
			 isSameOwningObject( originalObjectHandle, \
								 *targetObjectHandle ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Message Dispatch ACL							*
*																			*
****************************************************************************/

/* Each message type has certain properties such as whether it's routable,
   which object types it applies to, what checks are performed on it, whether
   it's processed by the kernel or dispatched to an object, etc etc.  These
   are all defined in the following table.

   In addition to the usual checks, we also make various assertions about the
   parameters we're passed.  Note that these don't check user data (that's
   checked programmatically and an error code returned) but values passed by
   cryptlib code */

typedef enum {
	PARAMTYPE_NONE_NONE,	/* Data = 0, value = 0 */
		PARAMTYPE_NONE = PARAMTYPE_NONE_NONE,/* For code analyser */
	PARAMTYPE_NONE_ANY,		/* Data = 0, value = any */
	PARAMTYPE_NONE_BOOLEAN,	/* Data = 0, value = boolean */
	PARAMTYPE_NONE_CHECKTYPE,/* Data = 0, value = check type */
	PARAMTYPE_DATA_NONE,	/* Data, value = 0 */
	PARAMTYPE_DATA_ANY,		/* Data, value = any */
	PARAMTYPE_DATA_ATTRIBUTE,/* Data, value = attribute type */
	PARAMTYPE_DATA_LENGTH,	/* Data, value >= 0 */
	PARAMTYPE_DATA_OBJTYPE,	/* Data, value = object type */
	PARAMTYPE_DATA_MECHTYPE,/* Data, value = mechanism type */
	PARAMTYPE_DATA_ITEMTYPE,/* Data, value = keymgmt.item type */
	PARAMTYPE_DATA_FORMATTYPE,/* Data, value = cert format type */
	PARAMTYPE_DATA_COMPARETYPE,/* Data, value = compare type */
	PARAMTYPE_DATA_SETDEPTYPE,/* Data, value = setdep.option type */
	PARAMTYPE_DATA_CERTMGMTTYPE,/* Data, value = cert.mgmt.type */
	PARAMTYPE_ANY_USERMGMTTYPE,/* Data = any, value = user mgmt.type */
	PARAMTYPE_ANY_TRUSTMGMTTYPE,/* Data = any, value = trust mgmt.type */
	PARAMTYPE_LAST			/* Last possible parameter check type */
	} PARAMCHECK_TYPE;

/* Symbolic defines for message handling types, used to make it clearer
   what's going on

	PRE_DISPATCH	- Action before message is dispatched
	POST_DISPATCH	- Action after message is dispatched
	HANDLE_INTERNAL	- Message handled by the kernel */

#define PRE_DISPATCH( function )	preDispatch##function, NULL
#define POST_DISPATCH( function )	NULL, postDispatch##function
#define PRE_POST_DISPATCH( preFunction, postFunction ) \
		preDispatch##preFunction, postDispatch##postFunction
#define HANDLE_INTERNAL( function )	NULL, NULL, MESSAGE_HANDLING_FLAG_INTERNAL, function

/* Flags to indicate (potential) special-case handling for a message.  These
   are:

	FLAG_INTERNAL: The message is handled internally by the kernel rather
			than being sent to an external handler.

	FLAG_MAYUNLOCK: The message handler may unlock the object (via 
			krnlReleaseObject()) to allow other threads access.  In this 
			case the first parameter to the handler function should be a
			MESSAGE_FUNCTION_EXTINFO structure to contain unlocking
			information */

#define MESSAGE_HANDLING_FLAG_NONE		0	/* No special handling */
#define MESSAGE_HANDLING_FLAG_MAYUNLOCK	1	/* Handler may unlock object */
#define MESSAGE_HANDLING_FLAG_INTERNAL	2	/* Message handle by kernel */

/* The handling information, declared in the order in which it's applied */

typedef struct {
	/* The message type, used for consistency checking */
	const MESSAGE_TYPE messageType;

	/* Message routing information if the message is routable.  If the target
	   is implicitly determined via the message value, the routing target is
	   OBJECT_TYPE_NONE; if the target is explicitly determined, the routing
	   target is identified in the target.  If the routing function is null,
	   the message isn't routed */
	const long routingTarget;			/* Target type if routable */
	ROUTING_FUNCTION routingFunction;

	/* Object type checking information: Object subtypes for which this
	   message is valid (for object-type-specific message) */
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
										/* Object subtype for which msg.valid */

	/* Message type checking information used to assertion-check the function
	   preconditions */
	const PARAMCHECK_TYPE paramCheck;	/* Parameter check assertion type */

	/* Pre- and post-message-dispatch handlers.  These perform any additional
	   checking and processing that may be necessary before and after a
	   message is dispatched to an object */
	int ( *preDispatchFunction )( const int objectHandle,
								  const MESSAGE_TYPE message,
								  const void *messageDataPtr,
								  const int messageValue, const void *auxInfo );
	int ( *postDispatchFunction )( const int objectHandle,
								   const MESSAGE_TYPE message,
								   const void *messageDataPtr,
								   const int messageValue, const void *auxInfo );

	/* Flags to indicate (potential) special-case handling for this message, 
	   and the (optional) internal handler function that's used if the 
	   message is handled directly by the kernel */
	int flags;							/* Special-case handling flags */
	int ( *internalHandlerFunction )( const int objectHandle, const int arg1,
									  const void *arg2,
									  const BOOLEAN isInternal );
	} MESSAGE_HANDLING_INFO;

static const MESSAGE_HANDLING_INFO FAR_BSS messageHandlingInfo[] = {
	{ MESSAGE_NONE, ROUTE_NONE, 0, 0, 0, PARAMTYPE_NONE_NONE },

	/* Control messages.  These messages aren't routed, are valid for all
	   object types and subtypes, take no (or minimal) parameters, and are
	   handled by the kernel */
	{ MESSAGE_DESTROY,				/* Destroy the object */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_NONE_NONE,
	  PRE_POST_DISPATCH( SignalDependentObjects, SignalDependentDevices ) },
	{ MESSAGE_INCREFCOUNT,			/* Increment object ref.count */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_NONE_NONE,
	  HANDLE_INTERNAL( incRefCount ) },
	{ MESSAGE_DECREFCOUNT,			/* Decrement object ref.count */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_NONE_NONE,
	  HANDLE_INTERNAL( decRefCount ) },
	{ MESSAGE_GETDEPENDENT,			/* Get dependent object */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_OBJTYPE,
	  HANDLE_INTERNAL( getDependentObject ) },
	{ MESSAGE_SETDEPENDENT,			/* Set dependent object (e.g. ctx->dev) */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_SETDEPTYPE,
	  HANDLE_INTERNAL( setDependentObject ) },
	{ MESSAGE_CLONE,				/* Clone the object (only valid for ctxs) */
	  ROUTE_FIXED( OBJECT_TYPE_CONTEXT ), ST_CTX_CONV | ST_CTX_HASH, ST_NONE, ST_NONE, 
	  PARAMTYPE_NONE_ANY,
	  HANDLE_INTERNAL( cloneObject ) },

	/* Attribute messages.  These messages are implicitly routed by attribute
	   type, more specific checking is performed using the attribute ACL's */
	{ MESSAGE_GETATTRIBUTE,			/* Get numeric object attribute */
	  ROUTE_IMPLICIT, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_ATTRIBUTE,
	  PRE_POST_DISPATCH( CheckAttributeAccess, MakeObjectExternal ) },
	{ MESSAGE_GETATTRIBUTE_S,		/* Get string object attribute */
	  ROUTE_IMPLICIT, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_ATTRIBUTE,
	  PRE_DISPATCH( CheckAttributeAccess ) },
	{ MESSAGE_SETATTRIBUTE,			/* Set numeric object attribute */
	  ROUTE_IMPLICIT, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_ATTRIBUTE,
	  PRE_POST_DISPATCH( CheckAttributeAccess, ChangeStateOpt ) },
	{ MESSAGE_SETATTRIBUTE_S,		/* Set string object attribute */
	  ROUTE_IMPLICIT, ST_ANY_A, ST_ANY_B, ST_ANY_C, 
	  PARAMTYPE_DATA_ATTRIBUTE,
	  PRE_POST_DISPATCH( CheckAttributeAccess, ChangeStateOpt ) },
	{ MESSAGE_DELETEATTRIBUTE,		/* Delete object attribute */
	  ROUTE_IMPLICIT, ST_CTX_ANY | ST_CERT_ANY, ST_NONE, ST_SESS_ANY | ST_USER_NORMAL | ST_USER_SO,
	  PARAMTYPE_NONE_ANY,
	  PRE_DISPATCH( CheckAttributeAccess ) },

	/* General messages to objects */
	{ MESSAGE_COMPARE,				/* Compare objs.or obj.properties */
	  ROUTE_SPECIAL( CompareMessageTarget ), ST_CTX_ANY | ST_CERT_ANY, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_COMPARETYPE,
	  PRE_DISPATCH( CheckCompareParam ) },
	{ MESSAGE_CHECK,				/* Check object info */
	  ROUTE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C,
	  PARAMTYPE_NONE_CHECKTYPE,
	  PRE_POST_DISPATCH( CheckCheckParam, ForwardToDependentObject ) },
	{ MESSAGE_SELFTEST,				/* Perform a self-test */
	  ROUTE_FIXED( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_SYSTEM, ST_NONE, 
	  PARAMTYPE_NONE_NONE,
	  NULL, NULL,
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },

	/* Messages sent from the kernel to object message handlers.  These
	   messages are sent directly to the object from inside the kernel in
	   response to a control message, so we set the checking to disallow
	   everything to catch any that arrive from outside */
	{ MESSAGE_CHANGENOTIFY,			/* Notification of obj.status chge.*/
	  ROUTE_NONE, ST_NONE, ST_NONE, ST_NONE, PARAMTYPE_NONE_NONE },

	/* Object-type-specific messages: Contexts */
	{ MESSAGE_CTX_ENCRYPT,			/* Context: Action = encrypt */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_CONV | ST_CTX_PKC, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_LENGTH,
	  PRE_POST_DISPATCH( CheckActionAccess, UpdateUsageCount ) },
	{ MESSAGE_CTX_DECRYPT,			/* Context: Action = decrypt */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_CONV | ST_CTX_PKC, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_LENGTH,
	  PRE_POST_DISPATCH( CheckActionAccess, UpdateUsageCount ) },
	{ MESSAGE_CTX_SIGN,				/* Context: Action = sign */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_PKC, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_LENGTH,
	  PRE_POST_DISPATCH( CheckActionAccess, UpdateUsageCount ) },
	{ MESSAGE_CTX_SIGCHECK,			/* Context: Action = sigcheck */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_PKC, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_LENGTH,
	  PRE_POST_DISPATCH( CheckActionAccess, UpdateUsageCount ) },
	{ MESSAGE_CTX_HASH,				/* Context: Action = hash */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_HASH | ST_CTX_MAC, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_LENGTH,
	  PRE_POST_DISPATCH( CheckActionAccess, UpdateUsageCount ) },
	{ MESSAGE_CTX_GENKEY,			/* Context: Generate a key */
	  ROUTE( OBJECT_TYPE_CONTEXT ), 
		ST_CTX_CONV | ST_CTX_PKC | ST_CTX_MAC | ST_CTX_GENERIC, ST_NONE, ST_NONE, 
	  PARAMTYPE_NONE_NONE,
	  PRE_POST_DISPATCH( CheckState, ChangeState ) },
	{ MESSAGE_CTX_GENIV,			/* Context: Generate an IV */
	  ROUTE( OBJECT_TYPE_CONTEXT ), ST_CTX_CONV, ST_NONE, ST_NONE, 
	  PARAMTYPE_NONE_NONE },

	/* Object-type-specific messages: Certificates */
	{ MESSAGE_CRT_SIGN,				/* Cert: Action = sign certificate */
	  ROUTE( OBJECT_TYPE_CERTIFICATE ),
		ST_CERT_ANY_CERT | ST_CERT_ATTRCERT | ST_CERT_CRL | \
		ST_CERT_OCSP_REQ | ST_CERT_OCSP_RESP, ST_NONE, ST_NONE, 
	  PARAMTYPE_NONE_ANY,
	  PRE_POST_DISPATCH( CheckStateParamHandle, ChangeState ) },
	{ MESSAGE_CRT_SIGCHECK,			/* Cert: Action = check/verify certificate */
	  ROUTE( OBJECT_TYPE_CERTIFICATE ),
		ST_CERT_ANY_CERT | ST_CERT_ATTRCERT | ST_CERT_CRL | \
		ST_CERT_RTCS_RESP | ST_CERT_OCSP_RESP, ST_NONE, ST_NONE, 
	  PARAMTYPE_NONE_ANY,
	  PRE_DISPATCH( CheckParamHandleOpt ) },
	{ MESSAGE_CRT_EXPORT,			/* Cert: Export encoded certificate data */
	  ROUTE( OBJECT_TYPE_CERTIFICATE ), ST_CERT_ANY, ST_NONE, ST_NONE, 
	  PARAMTYPE_DATA_FORMATTYPE,
	  PRE_DISPATCH( CheckExportAccess ) },

	/* Object-type-specific messages: Devices */
	{ MESSAGE_DEV_QUERYCAPABILITY,	/* Device: Query capability */
	  ROUTE_FIXED( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_ANY },
	{ MESSAGE_DEV_EXPORT,			/* Device: Action = export key */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismWrapAccess ), 
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_IMPORT,			/* Device: Action = import key */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismWrapAccess ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_SIGN,				/* Device: Action = sign */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismSignAccess ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_SIGCHECK,			/* Device: Action = sig.check */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismSignAccess ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_DERIVE,			/* Device: Action = derive key */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismDeriveAccess ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_KDF,				/* Device: Action = KDF key */
	  ROUTE( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_MECHTYPE,
	  PRE_DISPATCH( CheckMechanismKDFAccess ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_CREATEOBJECT,		/* Device: Create object */
	  ROUTE_FIXED( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_OBJTYPE,
	  PRE_POST_DISPATCH( CheckCreate, MakeObjectExternal ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },
	{ MESSAGE_DEV_CREATEOBJECT_INDIRECT,/* Device: Create obj.from data */
	  ROUTE_FIXED( OBJECT_TYPE_DEVICE ), ST_NONE, ST_DEV_ANY, ST_NONE, 
	  PARAMTYPE_DATA_OBJTYPE,
	  PRE_POST_DISPATCH( CheckCreate, MakeObjectExternal ),
	  MESSAGE_HANDLING_FLAG_MAYUNLOCK },

	/* Object-type-specific messages: Envelopes */
	{ MESSAGE_ENV_PUSHDATA,			/* Envelope: Push data */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_ENVELOPE, OBJECT_TYPE_SESSION ),
		ST_NONE, ST_ENV_ANY, ST_SESS_ANY_DATA,
	  PARAMTYPE_DATA_NONE,
	  PRE_DISPATCH( CheckData ) },
	{ MESSAGE_ENV_POPDATA,			/* Envelope: Pop data */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_ENVELOPE, OBJECT_TYPE_SESSION ),
		ST_NONE, ST_ENV_ANY, ST_SESS_ANY_DATA,
	  PARAMTYPE_DATA_NONE,
	  PRE_DISPATCH( CheckData ) },

	/* Object-type-specific messages: Keysets */
	{ MESSAGE_KEY_GETKEY,			/* Keyset: Instantiate ctx/certificate */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_KEYSET, OBJECT_TYPE_DEVICE ),
		ST_NONE, ST_KEYSET_ANY | ST_DEV_ANY_STD, ST_NONE,
	  PARAMTYPE_DATA_ITEMTYPE,
	  PRE_POST_DISPATCH( CheckKeysetAccess, MakeObjectExternal ) },
	{ MESSAGE_KEY_SETKEY,			/* Keyset: Add ctx/certificate */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_KEYSET, OBJECT_TYPE_DEVICE ),
		ST_NONE, ST_KEYSET_ANY | ST_DEV_ANY_STD, ST_NONE,
	  PARAMTYPE_DATA_ITEMTYPE,
	  PRE_DISPATCH( CheckKeysetAccess ) },
	{ MESSAGE_KEY_DELETEKEY,		/* Keyset: Delete key */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_KEYSET, OBJECT_TYPE_DEVICE ),
		ST_NONE, ST_KEYSET_ANY | ST_DEV_ANY_STD, ST_NONE,
	  PARAMTYPE_DATA_ITEMTYPE,
	  PRE_DISPATCH( CheckKeysetAccess ) },
	{ MESSAGE_KEY_GETFIRSTCERT,		/* Keyset: Get first cert in sequence */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_KEYSET, OBJECT_TYPE_DEVICE ),
		ST_NONE, ST_KEYSET_ANY | ST_DEV_ANY_STD, ST_NONE,
	  PARAMTYPE_DATA_ITEMTYPE,
	  PRE_DISPATCH( CheckKeysetAccess ) },
	{ MESSAGE_KEY_GETNEXTCERT,		/* Keyset: Get next cert in sequence */
	  ROUTE_FIXED_ALT( OBJECT_TYPE_KEYSET, OBJECT_TYPE_DEVICE ),
		ST_NONE, ST_KEYSET_ANY | ST_DEV_ANY_STD, ST_NONE,
	  PARAMTYPE_DATA_ITEMTYPE,
	  PRE_POST_DISPATCH( CheckKeysetAccess, MakeObjectExternal ) },
	{ MESSAGE_KEY_CERTMGMT,			/* Keyset: Certificate management */
	  ROUTE_FIXED( OBJECT_TYPE_KEYSET ),
		ST_NONE, ST_KEYSET_DBMS_STORE, ST_NONE,
	  PARAMTYPE_DATA_CERTMGMTTYPE,
	  PRE_POST_DISPATCH( CheckCertMgmtAccess, MakeObjectExternal ) },

	/* Object-type-specific messages: Users */
	{ MESSAGE_USER_USERMGMT,		/* User: User management */
	  ROUTE_FIXED( OBJECT_TYPE_USER ), ST_NONE, ST_NONE, ST_USER_SO, 
	  PARAMTYPE_ANY_USERMGMTTYPE,
	  PRE_POST_DISPATCH( CheckUserMgmtAccess, HandleZeroise ) },
	{ MESSAGE_USER_TRUSTMGMT,		/* User: Trust management */
	  ROUTE_FIXED( OBJECT_TYPE_USER ), ST_NONE, ST_NONE, ST_USER_SO, 
	  PARAMTYPE_ANY_TRUSTMGMTTYPE,
	  PRE_DISPATCH( CheckTrustMgmtAccess ) },

	/* End-of-ACL marker */
	{ MESSAGE_NONE, ROUTE_NONE, 0, PARAMTYPE_NONE_NONE },
	{ MESSAGE_NONE, ROUTE_NONE, 0, PARAMTYPE_NONE_NONE }
	};

/* Check the basic validity of message parameters.  Note that this only 
   checks for coding errors (krnlSendMessage() having been called 
   correctly), it doesn't perform parameter validation, which is the
   job of the full ACL checks */

CHECK_RETVAL_BOOL
static BOOLEAN checkParams( IN_ENUM( PARAMTYPE ) \
								const PARAMCHECK_TYPE paramCheck,
							const void *messageDataPtr, 
							const int messageValue )
	{
	REQUIRES_B( paramCheck >= PARAMTYPE_NONE_NONE && \
				paramCheck < PARAMTYPE_LAST );

	switch( paramCheck )
		{
		case PARAMTYPE_NONE_NONE:
			return( messageDataPtr == NULL && messageValue == 0 );

		case PARAMTYPE_NONE_ANY:
			return( messageDataPtr == NULL );

		case PARAMTYPE_NONE_BOOLEAN:
			return( messageDataPtr == NULL && \
					( messageValue == FALSE || messageValue == TRUE ) );

		case PARAMTYPE_NONE_CHECKTYPE:
			return( messageDataPtr == NULL && \
					( messageValue > MESSAGE_CHECK_NONE && \
					  messageValue < MESSAGE_CHECK_LAST ) );

		case PARAMTYPE_DATA_NONE:
			return( messageDataPtr != NULL && messageValue == 0 );

		case PARAMTYPE_DATA_ANY:
			return( messageDataPtr != NULL );

		case PARAMTYPE_DATA_ATTRIBUTE:
			return( messageDataPtr != NULL && \
					( ( messageValue > CRYPT_ATTRIBUTE_NONE && \
						messageValue < CRYPT_ATTRIBUTE_LAST ) || \
					  ( messageValue > CRYPT_IATTRIBUTE_FIRST && \
					    messageValue < CRYPT_IATTRIBUTE_LAST ) ) );

		case PARAMTYPE_DATA_LENGTH:
			return( messageDataPtr != NULL && messageValue >= 0 );

		case PARAMTYPE_DATA_OBJTYPE:
			return( messageDataPtr != NULL && \
					( messageValue > OBJECT_TYPE_NONE && \
					  messageValue < OBJECT_TYPE_LAST ) );

		case PARAMTYPE_DATA_MECHTYPE:
			return( messageDataPtr != NULL && \
					( messageValue > MECHANISM_NONE && \
					  messageValue < MECHANISM_LAST ) );

		case PARAMTYPE_DATA_ITEMTYPE:
			return( messageDataPtr != NULL && \
					( messageValue > KEYMGMT_ITEM_NONE && \
					  messageValue < KEYMGMT_ITEM_LAST ) );

		case PARAMTYPE_DATA_FORMATTYPE:
			return( messageDataPtr != NULL && \
					( messageValue > CRYPT_CERTFORMAT_NONE && \
					  messageValue < CRYPT_CERTFORMAT_LAST ) );

		case PARAMTYPE_DATA_COMPARETYPE:
			return( messageDataPtr != NULL && \
					( messageValue > MESSAGE_COMPARE_NONE && \
					  messageValue < MESSAGE_COMPARE_LAST ) );

		case PARAMTYPE_DATA_SETDEPTYPE:
			return( messageDataPtr != NULL && \
					messageValue > SETDEP_OPTION_NONE && \
					messageValue < SETDEP_OPTION_LAST );

		case PARAMTYPE_DATA_CERTMGMTTYPE:
			return( messageDataPtr != NULL && \
					messageValue > CRYPT_CERTACTION_NONE && \
					messageValue < CRYPT_CERTACTION_LAST );

		case PARAMTYPE_ANY_USERMGMTTYPE:
			return( messageValue > MESSAGE_USERMGMT_NONE && \
					messageValue < MESSAGE_USERMGMT_LAST );

		case PARAMTYPE_ANY_TRUSTMGMTTYPE:
			return( messageValue > MESSAGE_TRUSTMGMT_NONE && \
					messageValue < MESSAGE_TRUSTMGMT_LAST );

		default:
			retIntError_Boolean();
		}

	retIntError_Boolean();
	}

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initSendMessage( INOUT KERNEL_DATA *krnlDataPtr )
	{
	int i;

	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* If we're running a fuzzing build, skip the lengthy self-checks */
#ifdef CONFIG_FUZZ
	krnlData = krnlDataPtr;
	return( CRYPT_OK );
#endif /* CONFIG_FUZZ */

	/* Perform a consistency check on various things that need to be set
	   up in a certain way for things to work properly */
	static_assert( MESSAGE_CTX_DECRYPT == MESSAGE_CTX_ENCRYPT + 1, \
				   "Message value" );
	static_assert( MESSAGE_CTX_SIGN == MESSAGE_CTX_DECRYPT + 1, \
				   "Message value" );
	static_assert( MESSAGE_CTX_SIGCHECK == MESSAGE_CTX_SIGN + 1, \
				   "Message value" );
	static_assert( MESSAGE_CTX_HASH == MESSAGE_CTX_SIGCHECK + 1, \
				   "Message value" );
	static_assert( MESSAGE_CTX_GENKEY == MESSAGE_CTX_HASH + 1, \
				   "Message value" );
	static_assert( MESSAGE_GETATTRIBUTE_S == MESSAGE_GETATTRIBUTE + 1, \
				   "Message value" );
	static_assert( MESSAGE_SETATTRIBUTE == MESSAGE_GETATTRIBUTE_S + 1, \
				   "Message value" );
	static_assert( MESSAGE_SETATTRIBUTE_S == MESSAGE_SETATTRIBUTE + 1, \
				   "Message value" );
	static_assert( MESSAGE_DELETEATTRIBUTE == MESSAGE_SETATTRIBUTE_S + 1, \
				   "Message value" );

	/* Perform a consistency check on various internal values and constants */
	assert( ACTION_PERM_COUNT == 6 );

	/* Perform a consistency check on the parameter ACL */
	for( i = 0; messageParamACLTbl[ i ].type != MESSAGE_NONE && \
				i < FAILSAFE_ARRAYSIZE( messageParamACLTbl, MESSAGE_ACL ); 
		 i++ )
		{
		const MESSAGE_ACL *messageParamACL = &messageParamACLTbl[ i ];

		ENSURES( isParamMessage( messageParamACL->type ) && \
				 !( messageParamACL->objectACL.subTypeA & ( SUBTYPE_CLASS_B | \
															SUBTYPE_CLASS_C ) ) && \
				 !( messageParamACL->objectACL.subTypeB & ( SUBTYPE_CLASS_A | \
															SUBTYPE_CLASS_C ) ) && \
				 !( messageParamACL->objectACL.subTypeC & ( SUBTYPE_CLASS_A | \
															SUBTYPE_CLASS_B ) ) );
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( messageParamACLTbl, MESSAGE_ACL ) );

	/* Perform a consistency check on the message handling information */
	for( i = MESSAGE_NONE + 1; i < MESSAGE_LAST; i++ )
		{
		const MESSAGE_HANDLING_INFO *messageInfo = &messageHandlingInfo[ i ];

		ENSURES( messageInfo->messageType == i && \
				 messageInfo->paramCheck >= PARAMTYPE_NONE_NONE && \
				 messageInfo->paramCheck < PARAMTYPE_LAST );
		ENSURES( ( messageInfo->messageType >= MESSAGE_ENV_PUSHDATA && \
				   messageInfo->messageType <= MESSAGE_KEY_GETNEXTCERT ) || \
				 ( messageInfo->routingTarget >= OBJECT_TYPE_NONE && \
				   messageInfo->routingTarget <= OBJECT_TYPE_LAST ) );
		ENSURES( messageInfo->messageType == MESSAGE_CLONE || \
				 messageInfo->messageType == MESSAGE_COMPARE || \
				 ( messageInfo->routingTarget == OBJECT_TYPE_NONE && \
				   messageInfo->routingFunction == NULL ) || \
				 ( messageInfo->routingTarget != OBJECT_TYPE_NONE && \
				   messageInfo->routingFunction != NULL ) );
		ENSURES( !( messageInfo->subTypeA & ( SUBTYPE_CLASS_B | \
											  SUBTYPE_CLASS_C ) ) && \
				 !( messageInfo->subTypeB & ( SUBTYPE_CLASS_A | \
											  SUBTYPE_CLASS_C ) ) && \
				 !( messageInfo->subTypeC & ( SUBTYPE_CLASS_A | \
											  SUBTYPE_CLASS_B ) ) );
		ENSURES( ( messageInfo->flags & MESSAGE_HANDLING_FLAG_INTERNAL ) || \
				 messageInfo->messageType == MESSAGE_SELFTEST || \
				 messageInfo->messageType == MESSAGE_CHANGENOTIFY || \
				 messageInfo->messageType == MESSAGE_CTX_GENIV || \
				 messageInfo->messageType == MESSAGE_DEV_QUERYCAPABILITY || \
				 messageInfo->preDispatchFunction != NULL );
		ENSURES( messageInfo->messageType == MESSAGE_SELFTEST || \
				 messageInfo->messageType == MESSAGE_CHANGENOTIFY || \
				 messageInfo->messageType == MESSAGE_CTX_GENIV || \
				 messageInfo->messageType == MESSAGE_DEV_QUERYCAPABILITY || \
				 ( messageInfo->preDispatchFunction != NULL || \
				   messageInfo->postDispatchFunction != NULL || \
				   messageInfo->internalHandlerFunction != NULL ) );
		ENSURES( ( ( messageInfo->flags & MESSAGE_HANDLING_FLAG_INTERNAL ) && \
					 messageInfo->internalHandlerFunction != NULL ) || \
				 ( !( messageInfo->flags & MESSAGE_HANDLING_FLAG_INTERNAL ) && \
					  messageInfo->internalHandlerFunction == NULL ) );
		}

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	return( CRYPT_OK );
	}

void endSendMessage( void )
	{
	krnlData = NULL;
	}

/****************************************************************************
*																			*
*								Message Queue								*
*																			*
****************************************************************************/

/* Enqueue a message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int enqueueMessage( IN_HANDLE const int objectHandle,
						   const MESSAGE_HANDLING_INFO *handlingInfoPtr,
						   IN_MESSAGE const MESSAGE_TYPE message,
						   IN_OPT const void *messageDataPtr,
						   const int messageValue )
	{
	MESSAGE_QUEUE_DATA *messageQueue = krnlData->messageQueue;
	int queuePos, i, iterationCount;
	ORIGINAL_INT_VAR( queueEnd, krnlData->queueEnd );

	assert( isReadPtr( handlingInfoPtr, sizeof( MESSAGE_HANDLING_INFO ) ) );

	/* Precondition: It's a valid message being sent to a valid object */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( isValidMessage( message & MESSAGE_MASK ) );

	/* Sanity-check the state/make sure that we don't overflow the queue 
	   (this object is not responding to messages... now all we need is 
	   GPF's).  We return a timeout error on overflow to indicate that there 
	   are too many messages queued for this (or other) objects */
	if( krnlData->queueEnd < 0 || \
		krnlData->queueEnd >= MESSAGE_QUEUE_SIZE - 1 )
		{
		ENSURES( krnlData->queueEnd >= 0 );
		DEBUG_DIAG(( "Invalid kernel message queue state" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_TIMEOUT );
		}

	/* Precondition: There's room to enqueue the message */
	REQUIRES( krnlData->queueEnd >= 0 && \
			  krnlData->queueEnd < MESSAGE_QUEUE_SIZE - 1 );

	/* Check whether a message to this object is already present in the
	   queue */
	for( queuePos = krnlData->queueEnd - 1, iterationCount = 0; 
		 queuePos >= 0 && iterationCount++ < MESSAGE_QUEUE_SIZE; queuePos-- )
		{
		if( messageQueue[ queuePos ].objectHandle == objectHandle )
			break;
		}
	ENSURES( iterationCount < MESSAGE_QUEUE_SIZE );

	/* Postcondition: queuePos = -1 if not present, position in queue if
	   present */
	ENSURES( queuePos == -1 || \
			 ( queuePos >= 0 && queuePos < krnlData->queueEnd ) );

	/* Sanity-check the queue positioning */
	ENSURES( queuePos >= -1 && queuePos < krnlData->queueEnd );

	/* Enqueue the message:

		+---------------+		+---------------+
		|.|.|x|x|y|z|   |	->	|.|.|x|x|#|y|z| |
		+---------------+		+---------------+
			   ^	 ^					 ^	   ^
			  qPos	qEnd				qPos  qEnd */
	queuePos++;		/* Insert after current position */
	for( i = krnlData->queueEnd - 1, iterationCount = 0; 
		 i >= queuePos && iterationCount < MESSAGE_QUEUE_SIZE; 
		 i--, iterationCount++ )
		messageQueue[ i + 1 ] = messageQueue[ i ];
	ENSURES( iterationCount < MESSAGE_QUEUE_SIZE );
	memset( &messageQueue[ queuePos ], 0, sizeof( MESSAGE_QUEUE_DATA ) );
	messageQueue[ queuePos ].objectHandle = objectHandle;
	messageQueue[ queuePos ].handlingInfoPtr = handlingInfoPtr;
	messageQueue[ queuePos ].message = message;
	messageQueue[ queuePos ].messageDataPtr = messageDataPtr;
	messageQueue[ queuePos ].messageValue = messageValue;
	krnlData->queueEnd++;

	/* Postcondition: The queue is within bounds and has grown by one 
	   element */
	ENSURES( krnlData->queueEnd > 0 && \
			 krnlData->queueEnd <= MESSAGE_QUEUE_SIZE - 1 );
	ENSURES( krnlData->queueEnd == ORIGINAL_VALUE( queueEnd ) + 1 );

	/* If a message for this object is already present tell the caller to 
	   defer processing */
	if( queuePos > 0 )
		return( OK_SPECIAL );

	return( CRYPT_OK );
	}

/* Dequeue a message */

CHECK_RETVAL \
static int dequeueMessage( IN_RANGE( 0, MESSAGE_QUEUE_SIZE ) \
								const int messagePosition )
	{
	MESSAGE_QUEUE_DATA *messageQueue = krnlData->messageQueue;
	int i, iterationCount;
	ORIGINAL_INT_VAR( queueEnd, krnlData->queueEnd );

	/* Precondition: We're deleting a valid queue position */
	REQUIRES( messagePosition >= 0 && \
			  messagePosition < krnlData->queueEnd );
	REQUIRES( krnlData->queueEnd > 0 && \
			  krnlData->queueEnd < MESSAGE_QUEUE_SIZE );

	/* Move the remaining messages down and clear the last entry */
	for( i = messagePosition, iterationCount = 0; 
		 i < krnlData->queueEnd - 1 && \
			iterationCount++ < MESSAGE_QUEUE_SIZE; i++ )
		messageQueue[ i ] = messageQueue[ i + 1 ];
	ENSURES( iterationCount < MESSAGE_QUEUE_SIZE );
	zeroise( &messageQueue[ krnlData->queueEnd - 1 ],
			 sizeof( MESSAGE_QUEUE_DATA ) );
	krnlData->queueEnd--;

	/* Postcondition: the queue is one element shorter, all queue entries 
	   are valid, and all non-queue entries are empty */
	ENSURES( krnlData->queueEnd == ORIGINAL_VALUE( queueEnd ) - 1 );
	ENSURES( krnlData->queueEnd >= 0 && \
			 krnlData->queueEnd < MESSAGE_QUEUE_SIZE - 1 );
	FORALL( i, 0, krnlData->queueEnd,
			messageQueue[ i ].handlingInfoPtr != NULL );
	FORALL( i, krnlData->queueEnd, MESSAGE_QUEUE_SIZE,
			messageQueue[ i ].handlingInfoPtr == NULL );

	return( CRYPT_OK );
	}

/* Get the next message in the queue */

CHECK_RETVAL_BOOL \
static BOOLEAN getNextMessage( IN_HANDLE const int objectHandle,
							   OUT_OPT MESSAGE_QUEUE_DATA *messageQueueInfo )
	{
	MESSAGE_QUEUE_DATA *messageQueue = krnlData->messageQueue;
	int i;

	assert( messageQueueInfo == NULL || \
			isWritePtr( messageQueueInfo, sizeof( MESSAGE_QUEUE_DATA ) ) );

	/* Preconditions: It's a valid object table entry.  It's not necessarily
	   a valid object since we may be de-queueing messages for it because 
	   it's just been destroyed */
	REQUIRES_B( objectHandle == SYSTEM_OBJECT_HANDLE || \
				objectHandle == DEFAULTUSER_OBJECT_HANDLE || \
				isHandleRangeValid( objectHandle ) );

	/* Clear return value */
	if( messageQueueInfo != NULL )
		memset( messageQueueInfo, 0, sizeof( MESSAGE_QUEUE_DATA ) );

	/* Sanity-check the state */
	ENSURES_B( krnlData->queueEnd >= 0 && \
			   krnlData->queueEnd < MESSAGE_QUEUE_SIZE );

	/* Find the next message for this object.  Since other messages can have
	   come and gone in the meantime, we have to scan from the start each
	   time */
	for( i = 0; i < krnlData->queueEnd && i < MESSAGE_QUEUE_SIZE; i++ )
		{
		if( messageQueue[ i ].objectHandle == objectHandle )
			{
			int status;

			if( messageQueueInfo != NULL )
				*messageQueueInfo = messageQueue[ i ];
			status = dequeueMessage( i );
			if( cryptStatusError( status ) )
				return( FALSE );

			return( TRUE );
			}
		}
	ENSURES_B( i < MESSAGE_QUEUE_SIZE );

	/* Postcondition: There are no more messages for this object present in
	   the queue */
	FORALL( i, 0, krnlData->queueEnd,
			messageQueue[ i ].objectHandle != objectHandle );

	return( FALSE );
	}

/* Dequeue all messages for an object in the queue */

static void dequeueAllMessages( IN_HANDLE const int objectHandle )
	{
	int iterationCount;

	/* Preconditions: It's a valid object table entry.  It's not necessarily
	   a valid object since we may be de-queueing messages for it because 
	   it's just been destroyed */
	REQUIRES_V( objectHandle == SYSTEM_OBJECT_HANDLE || \
				objectHandle == DEFAULTUSER_OBJECT_HANDLE || \
				isHandleRangeValid( objectHandle ) );
	
	/* Dequeue all messages for a given object */
	for( iterationCount = 0;
		 getNextMessage( objectHandle, NULL ) && \
			iterationCount < MESSAGE_QUEUE_SIZE;
		 iterationCount++ );
	ENSURES_V( iterationCount < MESSAGE_QUEUE_SIZE );

	/* Postcondition: There are no more messages for this object present in
	   the queue */
	FORALL( i, 0, krnlData->queueEnd,
			krnlData->messageQueue[ i ].objectHandle != objectHandle );
	}

/****************************************************************************
*																			*
*							Message Dispatcher								*
*																			*
****************************************************************************/

/* Process a message that's handled internally by the kernel, for example 
   one that accesses an object's kernel attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int processInternalMessage( IN_HANDLE const int localObjectHandle,
								   const MESSAGE_HANDLING_INFO *handlingInfoPtr,
								   IN_MESSAGE const MESSAGE_TYPE message,
								   IN_OPT void *messageDataPtr,
								   const int messageValue,
								   IN_OPT const void *aclPtr )
	{
	int status;

	assert( isReadPtr( handlingInfoPtr, sizeof( MESSAGE_HANDLING_INFO ) ) );

	/* Precondition: It's a valid message being sent to a valid object */
	REQUIRES( isValidObject( localObjectHandle ) );
	REQUIRES( isValidMessage( message & MESSAGE_MASK ) );

	/* If there's a pre-dispatch handler, invoke it */
	if( handlingInfoPtr->preDispatchFunction != NULL )
		{
		status = handlingInfoPtr->preDispatchFunction( localObjectHandle,
									message, messageDataPtr, messageValue,
									aclPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Inner precondition: Either the message as a whole is internally 
	   handled or it's a property attribute */
	REQUIRES( handlingInfoPtr->internalHandlerFunction != NULL || \
			  isAttributeMessage( message & MESSAGE_MASK ) );

	/* If it's an object property attribute (which is handled by the kernel), 
	   get or set its value */
	if( handlingInfoPtr->internalHandlerFunction == NULL )
		{
		/* Precondition: Object properties are always numeric attributes, 
		   and there's always a message value present */
		REQUIRES( handlingInfoPtr->messageType == MESSAGE_GETATTRIBUTE || \
				  handlingInfoPtr->messageType == MESSAGE_SETATTRIBUTE );
		REQUIRES( messageDataPtr != NULL );

		if( handlingInfoPtr->messageType == MESSAGE_GETATTRIBUTE )
			status = getPropertyAttribute( localObjectHandle, messageValue, 
										   messageDataPtr );
		else
			status = setPropertyAttribute( localObjectHandle, messageValue, 
										   messageDataPtr );
		}
	else
		{
		/* It's a kernel-handled message, process it */
		status = handlingInfoPtr->internalHandlerFunction( localObjectHandle, 
												messageValue, messageDataPtr, 
												isInternalMessage( message ) );
		}
	if( cryptStatusError( status ) )
		{
		/* Postcondition: It's a genuine error, or a special-case condition
		   such as the object creation being aborted, which produces an 
		   OK_SPECIAL status to tell the caller to convert the message that
		   triggered this into a MESSAGE_DESTROY */
		ENSURES( cryptStatusError( status ) || status == OK_SPECIAL );

		return( status );
		}

	/* If there's a post-dispatch handler, invoke it */
	if( handlingInfoPtr->postDispatchFunction != NULL )
		{
		status = handlingInfoPtr->postDispatchFunction( localObjectHandle, 
									message, messageDataPtr, messageValue, 
									aclPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	
	return( CRYPT_OK );
	}

/* Dispatch a message to an object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int dispatchMessage( IN_HANDLE const int localObjectHandle,
							const MESSAGE_QUEUE_DATA *messageQueueData,
							INOUT OBJECT_INFO *objectInfoPtr,
							IN_OPT const void *aclPtr )
	{
	const MESSAGE_HANDLING_INFO *handlingInfoPtr = \
									messageQueueData->handlingInfoPtr;
	const MESSAGE_FUNCTION messageFunction = objectInfoPtr->messageFunction;
	const MESSAGE_TYPE localMessage = messageQueueData->message & MESSAGE_MASK;
	MESSAGE_FUNCTION_EXTINFO messageExtInfo;
	void *objectPtr = objectInfoPtr->objectPtr;
	BOOLEAN mayUnlock = FALSE;
	int status;
	ORIGINAL_INT_VAR( lockCount, objectInfoPtr->lockCount );

	assert( isReadPtr( messageQueueData, sizeof( MESSAGE_QUEUE_DATA ) ) );
	assert( isWritePtr( objectInfoPtr, sizeof( OBJECT_INFO ) ) );

	REQUIRES( isValidObject( localObjectHandle ) );
	REQUIRES( !isInUse( localObjectHandle ) || \
			  isObjectOwner( localObjectHandle ) );
	REQUIRES( messageFunction != NULL );

	/* If there's a pre-dispatch handler present, apply it */
	if( handlingInfoPtr->preDispatchFunction != NULL )
		{
		status = handlingInfoPtr->preDispatchFunction( localObjectHandle,
										messageQueueData->message,
										messageQueueData->messageDataPtr,
										messageQueueData->messageValue,
										aclPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Some objects (generally the system object and to a lesser extent other
	   devices and the default user object) may unlock themselves while 
	   processing a message when they forward the message elsewhere or perform 
	   non-object-specific processing.  If this may be the case then we pass 
	   in an extended message structure to record this information */
	initMessageExtInfo( &messageExtInfo, objectPtr );
	if( ( objectInfoPtr->type == OBJECT_TYPE_DEVICE ) || \
		( handlingInfoPtr->flags & MESSAGE_HANDLING_FLAG_MAYUNLOCK ) )
		{
		mayUnlock = TRUE;
		objectPtr = &messageExtInfo;
		}

	/* Mark the object as busy so that we have it available for our
	   exclusive use and further messages to it will be enqueued, dispatch
	   the message with the object table unlocked, and mark the object as
	   non-busy again */
	objectInfoPtr->lockCount++;
#ifdef USE_THREADS
	objectInfoPtr->lockOwner = THREAD_SELF();
#endif /* USE_THREADS */
	MUTEX_UNLOCK( objectTable );
	status = messageFunction( objectPtr, localMessage,
							  ( MESSAGE_CAST ) messageQueueData->messageDataPtr,
							  messageQueueData->messageValue );
	MUTEX_LOCK( objectTable );
	objectInfoPtr = &krnlData->objectTable[ localObjectHandle ];
	if( !isValidType( objectInfoPtr->type ) )
		retIntError();	/* Something catastrophic happened while unlocked */
	if( !( mayUnlock && isMessageObjectUnlocked( &messageExtInfo ) ) )
		objectInfoPtr->lockCount--;

	/* Postcondition: The lock count is non-negative and, if it's not the
	   system object, has been reset to its previous value */
	ENSURES( objectInfoPtr->lockCount >= 0 && \
			 ( localObjectHandle == SYSTEM_OBJECT_HANDLE ||
			   objectInfoPtr->lockCount == ORIGINAL_VALUE( lockCount ) ) );

	/* If there's a post-dispatch handler present, apply it.  Since a
	   destroy object message always succeeds but can return an error code
	   (typically CRYPT_ERROR_INCOMPLETE), we don't treat an error return as
	   a real error status for the purposes of further processing */
	if( ( cryptStatusOK( status ) || localMessage == MESSAGE_DESTROY ) && \
		handlingInfoPtr->postDispatchFunction != NULL )
		{
		const BOOLEAN isIncomplete = ( localMessage == MESSAGE_DESTROY && \
									   status == CRYPT_ERROR_INCOMPLETE ) ? \
									 TRUE : FALSE;

		status = handlingInfoPtr->postDispatchFunction( localObjectHandle,
												messageQueueData->message,
												messageQueueData->messageDataPtr,
												messageQueueData->messageValue,
												aclPtr );
		if( isIncomplete )
			{
			/* Normally we don't call the post-dispatch handler on error, 
			   however if it's a destroy message then we have to call it 
			   in order to handle any additional cleanup operations since 
			   the object is about to be destroyed, so if the destroy 
			   message to the object returned an incomplete error a we 
			   override any post-dispatch status with the original error
			   status */
			status = CRYPT_ERROR_INCOMPLETE;
			}
		}
	return( status );
	}

/* Send a message to an object */

RETVAL \
PARAMCHECK_MESSAGE( MESSAGE_DESTROY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_INCREFCOUNT, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DECREFCOUNT, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_GETDEPENDENT, OUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_SETDEPENDENT, IN, IN ) \
PARAMCHECK_MESSAGE( MESSAGE_CLONE, PARAM_NULL, IN_HANDLE ) \
PARAMCHECK_MESSAGE( MESSAGE_GETATTRIBUTE, OUT, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_GETATTRIBUTE_S, INOUT, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_SETATTRIBUTE, IN, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_SETATTRIBUTE_S, IN, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_DELETEATTRIBUTE, PARAM_NULL, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_COMPARE, IN, IN_ENUM( MESSAGE_COMPARE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CHECK, PARAM_NULL, IN_ENUM( MESSAGE_CHECK ) ) \
PARAMCHECK_MESSAGE( MESSAGE_SELFTEST, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CHANGENOTIFY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_ENCRYPT, INOUT, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_DECRYPT, INOUT, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_SIGN, IN, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_SIGCHECK, IN, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_HASH, IN, IN_LENGTH_Z ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_GENKEY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_GENIV, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_SIGN, PARAM_NULL, IN_HANDLE ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_SIGCHECK, PARAM_NULL, IN_HANDLE_OPT ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_EXPORT, INOUT, IN_ENUM( CRYPT_CERTFORMAT ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_QUERYCAPABILITY, OUT, IN_ALGO ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_EXPORT, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_IMPORT, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_SIGN, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_SIGCHECK, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_DERIVE, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_KDF, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_CREATEOBJECT, INOUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_CREATEOBJECT_INDIRECT, INOUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_ENV_PUSHDATA, INOUT, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_ENV_POPDATA, INOUT, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_SETKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_DELETEKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETFIRSTCERT, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETNEXTCERT, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_CERTMGMT, INOUT, IN_ENUM( CRYPT_CERTACTION ) ) \
PARAMCHECK_MESSAGE( MESSAGE_USER_USERMGMT, INOUT, IN_ENUM( MESSAGE_USERMGMT ) ) \
PARAMCHECK_MESSAGE( MESSAGE_USER_TRUSTMGMT, IN, IN_ENUM( MESSAGE_TRUSTMGMT ) ) \
					/* Actually INOUT for MESSAGE_TRUSTMGMT_GETISSUER, but too \
					   complex to annotate */ \
int krnlSendMessage( IN_HANDLE const int objectHandle, 
					 IN_MESSAGE const MESSAGE_TYPE message,
					 void *messageDataPtr, const int messageValue )
	{
	const ATTRIBUTE_ACL *attributeACL = NULL;
	const MESSAGE_HANDLING_INFO *handlingInfoPtr;
	OBJECT_INFO *objectTable, *objectInfoPtr;
	MESSAGE_QUEUE_DATA enqueuedMessageData;
	const BOOLEAN isInternalMessage = isInternalMessage( message ) ? \
									  TRUE : FALSE;
	const void *aclPtr = NULL;
	MESSAGE_TYPE localMessage = message & MESSAGE_MASK;
	int localObjectHandle = objectHandle, iterationCount;
	int status = CRYPT_OK;

	assert( isWritePtr( krnlData, sizeof( KERNEL_DATA ) ) );

	/* Preconditions.  For external messages we don't provide any assertions
	   at this point since they're coming straight from the user and could
	   contain any values, and for internal messages we only trap on
	   programming errors (thus for example isValidHandle() vs.
	   isValidObject(), since this would trap if a message is sent to a
	   destroyed object) */
	REQUIRES( isValidMessage( localMessage ) );
	REQUIRES( !isInternalMessage || isValidHandle( objectHandle ) );

	/* Get the information that we need to handle this message */
	handlingInfoPtr = &messageHandlingInfo[ localMessage ];

	/* Inner preconditions now that we have the handling information: Message
	   parameters must be within the allowed range */
	REQUIRES( checkParams( handlingInfoPtr->paramCheck, messageDataPtr, 
						   messageValue ) );

	/* If it's an object-manipulation message get the attribute's mandatory
	   ACL; if it's an object-parameter message get the parameter's mandatory
	   ACL.  Since these doesn't require access to any object information, we
	   can do it before we lock the object table */
	if( isAttributeMessage( localMessage ) )
		{
		attributeACL = findAttributeACL( messageValue, isInternalMessage );
		if( attributeACL == NULL )
			return( CRYPT_ARGERROR_VALUE );
		aclPtr = attributeACL;

		/* Because cryptlib can be called through a variety of different 
		   language bindings it's not guaranteed that the values of TRUE and 
		   FALSE (as supplied by the caller) will always be 1 and 0.  For
		   example Visual Basic uses the value -1 for TRUE because of 
		   obscure backwards-compatibility and implementation issues with 
		   16-bit versions of VB.  In order to avoid complaints from various
		   checks at lower levels of cryptlib, we replace any boolean values
		   with a setting other than FALSE with the explicit boolean value 
		   TRUE.  This is a bit of an ugly kludge but it avoids having to
		   special-case these values at all sorts of other locations in the
		   code */
		if( localMessage == MESSAGE_SETATTRIBUTE && \
			attributeACL->valueType == ATTRIBUTE_VALUE_BOOLEAN )
			{
			REQUIRES( messageDataPtr != NULL );

			if( *( ( BOOLEAN * ) messageDataPtr ) )
				messageDataPtr = MESSAGE_VALUE_TRUE;
			}
		}
	if( isParamMessage( localMessage ) )
		{
		aclPtr = findParamACL( localMessage );
		ENSURES( aclPtr != NULL );
		}

	/* Inner precondition: If it's an attribute-manipulation message, we have
	   a valid ACL for the attribute present */
	REQUIRES( !isAttributeMessage( localMessage ) || attributeACL != NULL );

	/* If we're in the middle of a shutdown, don't allow any further
	   messages except ones related to object destruction (the status read
	   is needed for objects capable of performing async ops, since the
	   shutdown code needs to determine whether they're currently busy).
	   The check outside the object-table lock is done in order to have any
	   remaining active objects exit quickly without tying up the object
	   table, since we don't want them to block the shutdown.  In addition
	   if the thread is a leftover/long-running thread that's still active
	   after the shutdown has occurred, we can't access the object table
	   lock since it'll have been deleted */
	if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES && \
		!( localMessage == MESSAGE_DESTROY || \
		   localMessage == MESSAGE_DECREFCOUNT || \
		   ( localMessage == MESSAGE_GETATTRIBUTE && \
			 messageValue == CRYPT_IATTRIBUTE_STATUS ) ) )
		{
		/* Exit without even trying to acquire the object table lock */
		return( CRYPT_ERROR_PERMISSION );
		}

	/* Lock the object table to ensure that other threads don't try to
	   access it */
	MUTEX_LOCK( objectTable );
	objectTable = krnlData->objectTable;

	/* The first line of defence: Make sure that the message is being sent
	   to a valid object and that the object is externally visible and
	   accessible to the caller if required by the message.  The checks
	   performed are:

		if( handle does not correspond to an object )
			error;
		if( message is external )
			{
			if( object is internal )
				error;
			if( object isn't owned by calling thread )
				error;
			}

	   This is equivalent to the shorter form fullObjectCheck() that used
	   elsewhere.  The error condition reported in all of these cases is
	   that the object handle isn't valid */
	if( !isValidObject( objectHandle ) )
		status = CRYPT_ARGERROR_OBJECT;
	else
		{
		if( !isInternalMessage && \
			( isInternalObject( objectHandle ) || \
			  !checkObjectOwnership( objectTable[ objectHandle ] ) ) )
			status = CRYPT_ARGERROR_OBJECT;
		}
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		return( status );
		}

	/* Inner precondition now that the outer check has been passed: It's a
	   valid, accessible object and not a system object that can never be
	   explicitly destroyed or have its refCount altered */
	REQUIRES_MUTEX( isValidObject( objectHandle ), objectTable );
	REQUIRES_MUTEX( isInternalMessage || ( !isInternalObject( objectHandle ) && \
					checkObjectOwnership( objectTable[ objectHandle ] ) ), \
					objectTable );
	REQUIRES_MUTEX( fullObjectCheck( objectHandle, message ), objectTable );
	REQUIRES_MUTEX( objectHandle >= NO_SYSTEM_OBJECTS || \
					( localMessage != MESSAGE_DESTROY && \
					  localMessage != MESSAGE_DECREFCOUNT && \
					  localMessage != MESSAGE_INCREFCOUNT ), objectTable );

	/* If this message is routable, find its target object */
	if( handlingInfoPtr->routingFunction != NULL )
		{
		/* If it's implicitly routed, route it based on the attribute type */
		if( isImplicitRouting( handlingInfoPtr->routingTarget ) )
			{
			REQUIRES_MUTEX( attributeACL != NULL, objectTable );

			if( attributeACL->routingFunction != NULL )
				{
				status = attributeACL->routingFunction( objectHandle,
											&localObjectHandle,
											attributeACL->routingTarget );
				}
			}
		else
			{
			/* It's explicitly or directly routed, route it based on the
			   message type or fixed-target type */
			status = handlingInfoPtr->routingFunction( objectHandle,
											&localObjectHandle,
						isExplicitRouting( handlingInfoPtr->routingTarget ) ? \
						messageValue : handlingInfoPtr->routingTarget );
			}
		if( cryptStatusError( status ) )
			{
			MUTEX_UNLOCK( objectTable );
			return( CRYPT_ARGERROR_OBJECT );
			}
		}

	/* Inner precodition: It's a valid destination object */
	REQUIRES_MUTEX( isValidObject( localObjectHandle ), objectTable );

	/* Sanity-check the message routing */
	if( !isValidObject( localObjectHandle ) )
		{
		MUTEX_UNLOCK( objectTable );
		retIntError();
		}

	/* It's a valid object, get its info */
	objectInfoPtr = &objectTable[ localObjectHandle ];

	/* Now that the message has been routed to its intended target, make sure
	   that it's valid for the target object subtype */
	if( !isValidSubtype( handlingInfoPtr->subTypeA, objectInfoPtr->subType ) && \
		!isValidSubtype( handlingInfoPtr->subTypeB, objectInfoPtr->subType ) && \
		!isValidSubtype( handlingInfoPtr->subTypeC, objectInfoPtr->subType ) )
		{
		MUTEX_UNLOCK( objectTable );
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* Inner precondition: The message is valid for this object subtype */
	REQUIRES_MUTEX( isValidSubtype( handlingInfoPtr->subTypeA, \
									objectInfoPtr->subType ) || \
					isValidSubtype( handlingInfoPtr->subTypeB, \
									objectInfoPtr->subType ) || \
					isValidSubtype( handlingInfoPtr->subTypeC, \
									objectInfoPtr->subType ), \
					objectTable );

	/* If this message is processed internally, handle it now.  These
	   messages aren't affected by the object's state so they're always
	   processed */
	if( handlingInfoPtr->internalHandlerFunction != NULL || \
		( attributeACL != NULL && \
		  attributeACL->flags & ATTRIBUTE_FLAG_PROPERTY ) )
		{
		status = processInternalMessage( localObjectHandle, handlingInfoPtr, 
										 message, messageDataPtr, 
										 messageValue, aclPtr );
		if( status != OK_SPECIAL )
			{
			/* The message was processed normally, exit */
			MUTEX_UNLOCK( objectTable );
			return( status );
			}

		/* The object has entered an invalid state (for example it was
		   signalled while it was being initialised) and can't be used any
		   more, destroy it, convert the (local copy of the) message into a
		   destroy object message */
		localMessage = MESSAGE_DESTROY;
		status = CRYPT_OK;
		}

	/* If the object isn't already processing a message and the message isn't
	   a special type such as MESSAGE_DESTROY, dispatch it immediately rather
	   than enqueueing it for later dispatch.  This scoreboard mechanism
	   greatly reduces the load on the queue */
	if( !isInUse( localObjectHandle ) && localMessage != MESSAGE_DESTROY )
		{
		CONST_INIT_STRUCT_5( MESSAGE_QUEUE_DATA messageQueueData, \
							 localObjectHandle, handlingInfoPtr, message, \
							 messageDataPtr, messageValue );

		CONST_SET_STRUCT( messageQueueData.objectHandle = localObjectHandle; \
						  messageQueueData.handlingInfoPtr = handlingInfoPtr; \
						  messageQueueData.message = message; \
						  messageQueueData.messageDataPtr = messageDataPtr; \
						  messageQueueData.messageValue = messageValue );

		/* If the object isn't in a valid state, we can't do anything with it.
		   There are no messages that can be sent to it at this point, get/
		   set property messages have already been handled earlier and the
		   destroy message isn't handled here */
		if( isInvalidObjectState( localObjectHandle ) )
			{
			status = getObjectStatusValue( objectInfoPtr->flags );
			MUTEX_UNLOCK( objectTable );
			return( status );
			}

		/* In case a shutdown was signalled while we were performing other
		   processing, exit now before we try and do anything with the
		   object.  It's safe to perform the check at this point since no
		   message sent during shutdown will get here */
		if( krnlData->shutdownLevel >= SHUTDOWN_LEVEL_MESSAGES )
			{
			MUTEX_UNLOCK( objectTable );
			return( CRYPT_ERROR_PERMISSION );
			}

		/* Inner precondition: The object is in a valid state */
		REQUIRES_MUTEX( !isInvalidObjectState( localObjectHandle ), \
						objectTable );

		/* Dispatch the message to the object */
		status = dispatchMessage( localObjectHandle, &messageQueueData,
								  objectInfoPtr, aclPtr );
		MUTEX_UNLOCK( objectTable );

		/* If it's a zeroise, perform a kernel shutdown.  In theory we could 
		   do this from the post-dispatch handler, but we need to make sure 
		   that there are no further kernel actions to be taken before we 
		   perform the shutdown, so we do it at this level instead */
		if( cryptStatusOK( status ) && \
			( messageQueueData.message & MESSAGE_MASK ) == MESSAGE_USER_USERMGMT && \
			messageQueueData.messageValue == MESSAGE_USERMGMT_ZEROISE )
			{
			/* Since it's a zeroise we return the status of the overall 
			   zeroise operation rather than any possible non-OK status from
			   shutting down the kernel at the end of the zeroise */
			( void ) endCryptlib();
			}

		/* Postcondition: The return status is valid */
		ENSURES( cryptStandardError( status ) || \
				 cryptArgError( status ) || status == OK_SPECIAL );

		return( status );
		}

	/* Inner precondition: The object is in use or it's a destroy object
	   message, we have to enqueue it */
	REQUIRES_MUTEX( isInUse( localObjectHandle ) || \
					localMessage == MESSAGE_DESTROY, \
					objectTable );

	/* If we're stuck in a loop processing recursive messages, bail out.
	   This would happen automatically anyway once we fill the message queue,
	   but this early-out mechanism prevents a single object from filling the
	   queue to the detriment of other objects */
	if( objectInfoPtr->lockCount > MESSAGE_QUEUE_SIZE / 2 )
		{
		MUTEX_UNLOCK( objectTable );
		DEBUG_DIAG(( "Invalid kernel message queue state" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_TIMEOUT );
		}

	/* If the object is in use by another thread, wait for it to become
	   available */
	if( isInUse( localObjectHandle ) && !isObjectOwner( localObjectHandle ) )
		{
		status = waitForObject( localObjectHandle, &objectInfoPtr );
#if !defined( NDEBUG ) && defined( USE_THREADS )
		if( cryptStatusOK( status ) && isInUse( localObjectHandle ) )
			{
			/* dispatchMessage() expects us to be the lock owner if the
			   object is in use, however if the object has been */
			objectInfoPtr->lockOwner = THREAD_SELF();
			}
#endif /* !NDEBUG && USE_THREADS */
		}
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( objectTable );
		return( status );
		}
	assert( !isInUse( localObjectHandle ) || \
			isObjectOwner( localObjectHandle ) );

	/* Enqueue the message */
	if( ( message & MESSAGE_MASK ) != localMessage )
		{
		/* The message was converted during processing, this can only happen
		   when a message sent to an invalid-state object is converted into
		   a destroy-object message.  What we therefore enqueue is a
		   destroy-object message, but with the messageValue parameter set
		   to TRUE to indicate that it's a converted destroy message */
		REQUIRES_MUTEX( localMessage == MESSAGE_DESTROY, objectTable );

		status = enqueueMessage( localObjectHandle,
								 &messageHandlingInfo[ MESSAGE_DESTROY ],
								 MESSAGE_DESTROY, messageDataPtr, TRUE );
		}
	else
		{
		status = enqueueMessage( localObjectHandle, handlingInfoPtr, message,
								 messageDataPtr, messageValue );
		}
	if( cryptStatusError( status ) )
		{
		/* A message for this object is already present in the queue, defer
		   processing until later */
		MUTEX_UNLOCK( objectTable );
		return( ( status == OK_SPECIAL ) ? CRYPT_OK : status );
		}
	assert( !isInUse( localObjectHandle ) || \
			isObjectOwner( localObjectHandle ) );

	/* While there are more messages for this object present, dequeue them
	   and dispatch them.  Since messages will only be enqueued if
	   krnlSendMessage() is called recursively, we only dequeue messages for
	   the current object in this loop.  Queued messages for other objects
	   will be handled at a different level of recursion.
	   
	   Bounding this loop is a bit tricky because new messages can arrive as
	   the existing ones are dequeued, so that in theory the arrival rate
	   could match the dispatch rate.  However in practice a situation like
	   this would be extremely unusual, so we bound the loop at
	   FAILSAFE_ITERATIONS_LARGE */
	for( iterationCount = 0;
		 getNextMessage( localObjectHandle, &enqueuedMessageData ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const BOOLEAN isDestroy = \
			( enqueuedMessageData.message & MESSAGE_MASK ) == MESSAGE_DESTROY;
		const BOOLEAN isZeroise = \
			( enqueuedMessageData.message & MESSAGE_MASK ) == MESSAGE_USER_USERMGMT && \
			enqueuedMessageData.messageValue == MESSAGE_USERMGMT_ZEROISE;

		/* If there's a problem with the object, initiate special processing.
		   The exception to this is a destroy message that started out as a 
		   different type of message (that is, it was converted into a 
		   destroy object message due to the object being in an invalid 
		   state, indicated by the messageValue parameter being set to TRUE
		   when it's normally zero for a destroy message), which is let 
		   through */
		if( isInvalidObjectState( localObjectHandle ) && \
			!( isDestroy && ( enqueuedMessageData.messageValue == TRUE ) ) )
			{
			/* If it's a destroy object message being sent to an object in
			   the process of being created, set the state to signalled and
			   continue.  The object will be destroyed when the caller
			   notifies the kernel that the init is complete */
			if( isDestroy && ( objectInfoPtr->flags & OBJECT_FLAG_NOTINITED ) )
				{
				objectInfoPtr->flags |= OBJECT_FLAG_SIGNALLED;
				status = CRYPT_OK;
				}
			else
				{
				/* Remove all further messages for this object and return
				   to the caller */
				dequeueAllMessages( localObjectHandle );
				status = getObjectStatusValue( objectInfoPtr->flags );
				}
			continue;
			}
		assert( !isInUse( localObjectHandle ) || \
				isObjectOwner( localObjectHandle ) );

		/* Inner precondition: The object is in a valid state or it's a
		   destroy message that was converted from a different message 
		   type */
		REQUIRES_MUTEX( !isInvalidObjectState( localObjectHandle ) || \
						( isDestroy && ( enqueuedMessageData.messageValue == TRUE ) ), \
						objectTable );

		/* Dispatch the message to the object */
		status = dispatchMessage( localObjectHandle, &enqueuedMessageData,
								  objectInfoPtr, aclPtr );

		/* If the message is a destroy object message, we have to explicitly
		   remove it from the object table and dequeue all further messages
		   for it since the object's message handler can't do this itself.
		   Since a destroy object message always succeeds but can return an
		   error code (typically CRYPT_ERROR_INCOMPLETE), we don't treat an
		   error return as a real error status for the purposes of further
		   processing */
		if( isDestroy )
			{
			int destroyStatus;	/* Preserve original status value */

			destroyStatus = destroyObjectData( localObjectHandle );
			ENSURES_MUTEX( cryptStatusOK( destroyStatus ), objectTable );
			dequeueAllMessages( localObjectHandle );
			}
		else
			{
			/* If we ran into a problem or this is a zeroise (i.e. a 
			   localised shutdown), dequeue all further messages for this 
			   object.  This causes getNextMessage() to fail and we drop out 
			   of the loop */
			if( cryptStatusError( status ) || \
				( cryptStatusOK( status ) && isZeroise ) )
				{
				dequeueAllMessages( localObjectHandle );
				}
			}
		}
	ENSURES_MUTEX( iterationCount < FAILSAFE_ITERATIONS_LARGE, \
				   objectTable );

	/* Unlock the object table to allow access by other threads */
	MUTEX_UNLOCK( objectTable );

	/* If it's a zeroise, perform a kernel shutdown.  In theory we could do 
	   this from the post-dispatch handler, but we need to make sure that 
	   there are no further kernel actions to be taken before we perform the 
	   shutdown, so we do it at this level instead */
	if( cryptStatusOK( status ) && localMessage == MESSAGE_USER_USERMGMT && \
		messageValue == MESSAGE_USERMGMT_ZEROISE )
		{
		/* Since it's a zeroise we return the status of the overall zeroise 
		   operation rather than any possible non-OK status from shutting 
		   down the kernel at the end of the zeroise */
		( void ) endCryptlib();
		}

	/* Postcondition: The return status is valid */
	ENSURES( cryptStandardError( status ) || cryptArgError( status ) || \
			 status == OK_SPECIAL );

	return( status );
	}
