/****************************************************************************
*																			*
*								Kernel Storage								*
*						Copyright Peter Gutmann 1997-2016					*
*																			*
****************************************************************************/

#ifdef __STDC__
  #include <stddef.h>		/* For offsetof() */
#endif /* __STDC__ */
#if defined( INC_ALL )
  #include "crypt.h"
  #ifdef USE_CERTIFICATES
	#include "trustmgr_int.h"
  #endif /* USE_CERTIFICATES */
  #include "device.h"
  #include "acl.h"
  #ifdef USE_TCP
	#include "tcp_int.h"
  #endif /* USE_TCP */
  #include "kernel.h"
  #include "user.h"
  #include "user_int.h"
  #include "random_int.h"
  #ifdef USE_SSL
	#include "scorebrd_int.h"
  #endif /* USE_SSL */
#else
  #include "crypt.h"
  #ifdef USE_CERTIFICATES
	#include "cert/trustmgr_int.h"
  #endif /* USE_CERTIFICATES */
  #include "device/device.h"
  #ifdef USE_TCP
	#include "io/tcp_int.h"
  #endif /* USE_TCP */
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
  #include "misc/user.h"
  #include "misc/user_int.h"
  #include "random/random_int.h"
  #ifdef USE_SSL
	#include "session/scorebrd_int.h"
  #endif /* USE_SSL */
#endif /* Compiler-specific includes */

/* cryptlib uses a preset amount of fixed storage for kernel data structures
   and built-in objects, which can be allocated statically at compile time
   rather than dynamically.  The following structure contains this fixed 
   storage, consisting of the kernel data, the object table, the system and 
   default user object storage, and any other fixed storage blocks that 
   might be needed */

typedef struct {
	/* The kernel data */
	KERNEL_DATA krnlData;

	/* The object table */
	OBJECT_INFO objectTable[ MAX_NO_OBJECTS ];

	/* The system object and default user object.  Since each object has 
	   subtype-specific storage following it, we also allocate a block of
	   storage for the device subtype following it which isn't accessed
	   directly but implicitly follows the object storage */
	DEVICE_INFO systemDevice;
	SYSTEMDEV_INFO systemDeviceStorage;
	USER_INFO defaultUserObject;

	/* The randomness information.  This is normally allocated in non-
	   pageable memory, but for embedded systems it's part of the 
	   statically-allocated system storage */
#ifdef USE_EMBEDDED_OS
	RANDOM_INFO randomInfo;
#endif /* USE_EMBEDDED_OS */

	/* The certificate trust information */
#ifdef USE_CERTIFICATES
	TRUST_INFO_CONTAINER trustInfoContainer;	
#endif /* USE_CERTIFICATES */

	/* The network socket pool */
#ifdef USE_TCP
	SOCKET_INFO socketInfo[ SOCKETPOOL_SIZE ];
#endif /* USE_TCP */

	/* The session scoreboard */
#ifdef USE_SSL
	SCOREBOARD_INDEX_INFO scoreboardIndexInfo;
#endif /* USE_SSL */

	/* The config option information.  This has a size defined by a complex
	   preprocessor expression (it's not a fixed struct) so we allocate it
	   as a byte array and let the caller manage it */
	BYTE optionInfo[ OPTION_INFO_SIZE ];
	} STORAGE_STRUCT;

static STORAGE_STRUCT systemStorage;

/* Initialise and destroy the built-in storage info */

#if defined( __WINDOWS__ )

typedef HRESULT ( WINAPI *WERREGISTEREXCLUDEDMEMORYBLOCK )( const void *address, 
															DWORD size );
static void excludeSystemStorage( void )
	{
	WERREGISTEREXCLUDEDMEMORYBLOCK pWerRegisterExcludedMemoryBlock;
	HANDLE hKernel32;

	/* Try and get WerRegisterExcludedMemoryBlock().  Since this is a 
	   Windows 10 function, we need to dynamically link it */
	if( ( hKernel32 = GetModuleHandle( "Kernel32.dll" ) ) == NULL )
		return;
	pWerRegisterExcludedMemoryBlock = ( WERREGISTEREXCLUDEDMEMORYBLOCK ) \
				GetProcAddress( hKernel32, "WerRegisterExcludedMemoryBlock" );
	if( pWerRegisterExcludedMemoryBlock == NULL )
		return;

	/* Exclude the system storage from being submitted in an error report */
	( void ) pWerRegisterExcludedMemoryBlock( &systemStorage, 
											  sizeof( STORAGE_STRUCT ) );
	}
#elif defined( __UNIX__ )

  #ifdef __linux__ 
	#include <sys/mman.h>
  #endif /* Linux */

static void excludeSystemStorage( void )
	{
	/* Exclude the system storage from core dumps.  MADV_DONTDUMP is only
	   available under Linux, which also requires that the address provided 
	   be page-aligned which the system storage block almost certainly 
	   isn't (we'd have to use mmap() to allocate it, which defeats the 
	   point of using statically-allocated memory), but this is only best-
	   effort anyway until a more universal facility for excluding the 
	   memory block from dumps appears */
#ifdef MADV_DONTDUMP
	( void ) madvise( &systemStorage, sizeof( STORAGE_STRUCT ), 
					  MADV_DONTDUMP );
#endif /* MADV_DONTDUMP */
	}
#else
  #define excludeSystemStorage()
#endif /* OS-specific exclusion from memory dumps */

void initBuiltinStorage( void )
	{
	memset( &systemStorage, 0, sizeof( STORAGE_STRUCT ) );
	excludeSystemStorage();
	}

void destroyBuiltinStorage( void )
	{
	memset( &systemStorage, 0, sizeof( STORAGE_STRUCT ) );
	}

/* When we start up and shut down the kernel, we need to clear the kernel
   data.  However, the init lock may have been set by an external management
   function, so we can't clear that part of the kernel data.  In addition,
   on shutdown the shutdown level value must stay set so that any threads
   still running will be forced to exit at the earliest possible instance,
   and remain set after the shutdown has completed.  To handle this, we use
   the following macro to clear only the appropriate area of the kernel data
   block */

void clearKernelData( void )
	{
	KERNEL_DATA *krnlDataPtr = &systemStorage.krnlData;

#ifdef __STDC__
	zeroise( ( BYTE * ) krnlDataPtr + offsetof( KERNEL_DATA, initLevel ), 
			 sizeof( KERNEL_DATA ) - offsetof( KERNEL_DATA, initLevel ) );
#else
	assert( &krnlDataPtr->endMarker - &krnlDataPtr->initLevel < sizeof( KERNEL_DATA ) ); 
	zeroise( ( void * ) &krnlDataPtr->initLevel, 
			 &krnlDataPtr->endMarker - &krnlDataPtr->initLevel );
#endif /* C89 compilers */
	}

/* Access functions for the built-in storage */

CHECK_RETVAL_PTR_NONNULL \
KERNEL_DATA *getKrnlData( void )
	{
	return( &systemStorage.krnlData );
	}

CHECK_RETVAL_PTR_NONNULL \
OBJECT_INFO *getObjectTable( void )
	{
	return( systemStorage.objectTable );
	}

CHECK_RETVAL_PTR_NONNULL \
void *getSystemDeviceStorage( void )
	{
	return( &systemStorage.systemDevice );
	}

CHECK_RETVAL_PTR_NONNULL \
void *getDefaultUserObjectStorage( void )
	{
	return( &systemStorage.defaultUserObject );
	}

#ifdef USE_EMBEDDED_OS
CHECK_RETVAL_PTR_NONNULL \
void *getRandomInfoStorage( void )
	{
	return( &systemStorage.randomInfo );
	}
#endif /* USE_EMBEDDED_OS */

#ifdef USE_CERTIFICATES
CHECK_RETVAL_PTR_NONNULL \
void *getTrustMgrStorage( void )
	{
	return( &systemStorage.trustInfoContainer );
	}
#endif /* USE_CERTIFICATES */

#ifdef USE_TCP
CHECK_RETVAL_PTR_NONNULL \
void *getSocketPoolStorage( void )
	{
	return( &systemStorage.socketInfo );
	}
#endif /* USE_TCP */

#ifdef USE_SSL
CHECK_RETVAL_PTR_NONNULL \
void *getScoreboardInfoStorage( void )
	{
	return( &systemStorage.scoreboardIndexInfo );
	}
#endif /* USE_SSL */

CHECK_RETVAL_PTR_NONNULL \
void *getOptionInfoStorage( void )
	{
	return( &systemStorage.optionInfo );
	}

/* Helper functions used when debugging.  These return the sizes of the 
   various data structures for use with fault-injection testing */

#ifndef NDEBUG

CHECK_RETVAL_LENGTH_NOERROR \
int getKrnlDataSize( void )
	{
	return( sizeof( KERNEL_DATA ) );
	}

CHECK_RETVAL_LENGTH_NOERROR \
int getObjectTableSize( void )
	{
	return( sizeof( OBJECT_INFO ) );
	}

CHECK_RETVAL_LENGTH_NOERROR \
int getSystemDeviceStorageSize( void )
	{
	return( sizeof( DEVICE_INFO ) + sizeof( SYSTEMDEV_INFO ) );
	}

CHECK_RETVAL_LENGTH_NOERROR \
int getDefaultUserObjectStorageSize( void )
	{
	return( sizeof( USER_INFO )  );
	}

#ifdef USE_EMBEDDED_OS
CHECK_RETVAL_LENGTH_NOERROR \
int getRandomInfoStorageSize( void )
	{
	return( sizeof( RANDOM_INFO ) );
	}
#endif /* USE_EMBEDDED_OS */

#ifdef USE_CERTIFICATES
CHECK_RETVAL_LENGTH_NOERROR \
int getTrustMgrStorageSize( void )
	{
	return( sizeof( TRUST_INFO_CONTAINER ) );
	}
#endif /* USE_CERTIFICATES */

#ifdef USE_TCP
CHECK_RETVAL_LENGTH_NOERROR \
int getSocketPoolStorageSize( void )
	{
	return( sizeof( SOCKET_INFO ) * SOCKETPOOL_SIZE );
	}
#endif /* USE_TCP */

#ifdef USE_SSL
CHECK_RETVAL_LENGTH_NOERROR \
int getScoreboardInfoStorageSize( void )
	{
	return( sizeof( SCOREBOARD_INDEX_INFO ) );
	}
#endif /* USE_SSL */

CHECK_RETVAL_LENGTH_NOERROR \
int getOptionInfoStorageSize( void )
	{
	return( OPTION_INFO_SIZE );
	}
#endif /* !NDEBUG */
