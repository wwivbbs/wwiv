/****************************************************************************
*																			*
*							Secure Memory Management						*
*						Copyright Peter Gutmann 1995-2012					*
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

/* The minimum and maximum amount of secure memory that we can ever 
   allocate.  A more normal upper bound is 1K, however the SSL session cache 
   constitutes a single large chunk of secure memory that goes way over this 
   limit */

#define MIN_ALLOC_SIZE			8
#define MAX_ALLOC_SIZE			8192

/* Memory block flags.  These are:

	FLAG_LOCKED: The memory block has been page-locked to prevent it from 
			being swapped to disk and will need to be unlocked when it's 
			freed.

	FLAG_PROTECTED: The memory is read-only, enforced by running a checksum
			over it that's stored at the end of the user-visible block */

#define MEM_FLAG_NONE			0x00	/* No memory flag */
#define MEM_FLAG_LOCKED			0x01	/* Memory block is page-locked */
#define MEM_FLAG_PROTECTED		0x02	/* Memory block can't be changed */
#define MEM_FLAG_MAX			0x03	/* Maximum possible flag value */

#define MEM_FLAG_MASK			0x03	/* Mask for memory flags */

/* To support page locking and other administration tasks we need to store 
   some additional information with the memory block.  We do this by 
   reserving an extra memory block at the start of the allocated block and 
   saving the information there.

   The information stored in the extra block is flags that control the use
   of the memory block, the size of the block, and pointers to the next and 
   previous pointers in the list of allocated blocks (this is used by the 
   thread that walks the block list touching each one).  We also insert a 
   canary at the start and end of each allocated memory block to detect 
   memory overwrites and modification, which is just a checksum of the memory
   header that doubles as a canary.

   The resulting memory block looks as follows:

			External mem.ptr
					|						Canary
					v						  v
		+-------+---+-----------------------+---+
		| Hdr	|###| Memory				|###|
		+-------+---+-----------------------+---+
		^									^	|
		|<----------- memHdrPtr->size --------->|
		|									|
	memPtr (BYTE *)							|
	memHdrPtr (MEM_INFO_HDR *)		memTrlPtr (MEM_INFO_TRL *) */

typedef struct {
	int flags;				/* Flags for this memory block */
	int size;				/* Size of the block (including the size
							   of the header and trailer) */
	void *next, *prev;		/* Next, previous memory block */
	int checksum;			/* Header checksum+canary for spotting overwrites */
	} MEM_INFO_HEADER;

typedef struct {
	int checksum;			/* Memory block checksum or canary (= header chks) */
	} MEM_INFO_TRAILER;

#if INT_MAX <= 32767
  #define MEM_ROUNDSIZE		4
#elif INT_MAX <= 0xFFFFFFFFUL
  #define MEM_ROUNDSIZE		8
#else
  #define MEM_ROUNDSIZE		16
#endif /* 16/32/64-bit systems */
#define MEM_INFO_HEADERSIZE	roundUp( sizeof( MEM_INFO_HEADER ), MEM_ROUNDSIZE )
#define MEM_INFO_TRAILERSIZE sizeof( MEM_INFO_TRAILER )

/****************************************************************************
*																			*
*							OS-Specific Memory Locking						*
*																			*
****************************************************************************/

/* Many OSes support locking pages in memory, the following helper functions 
   implement this locking */

#if defined( __MAC__ )

#include <Memory.h>

/* The Mac has two functions for locking memory, HoldMemory() (which makes 
   the memory ineligible for paging) and LockMemory() (which makes it 
   ineligible for paging and also immovable).  We use HoldMemory() since 
   it's slightly more friendly, but really critical applications could use 
   LockMemory() */

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

#if !defined( CALL_NOT_IN_CARBON ) || CALL_NOT_IN_CARBON
	if( HoldMemory( memPtr, memHdrPtr->size ) == noErr )
		memHdrPtr->flags |= MEM_FLAG_LOCKED;
#endif /* Non Mac OS X memory locking */
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory was locked, unlock it now */
#if !defined( CALL_NOT_IN_CARBON ) || CALL_NOT_IN_CARBON
	if( memHdrPtr->flags & MEM_FLAG_LOCKED )
		UnholdMemory( memPtr, memHdrPtr->size );
#endif /* Non Mac OS X memory locking */
	}

#elif defined( __MSDOS__ ) && defined( __DJGPP__ )

#include <dpmi.h>
#include <go32.h>

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* Under 32-bit MSDOS use the DPMI functions to lock memory */
	if( _go32_dpmi_lock_data( memPtr, memHdrPtr->size ) == 0)
		memHdrPtr->flags |= MEM_FLAG_LOCKED;
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* Under 32-bit MSDOS we *could* use the DPMI functions to unlock
	   memory, but as many DPMI hosts implement page locking in a binary
	   form (no lock count maintained), it's better not to unlock anything
	   at all.  Note that this may lead to a shortage of virtual memory in
	   long-running applications */
	}

#elif defined( __UNIX__ )

/* Since the function prototypes for the SYSV/Posix mlock() call are stored
   all over the place depending on the Unix version it's easier to prototype 
   it ourselves here rather than trying to guess its location */

#if defined( __osf__ ) || defined( __alpha__ )
  #include <sys/mman.h>
#elif defined( sun )
  #include <sys/mman.h>
  #include <sys/types.h>
#else
  int mlock( void *address, size_t length );
  int munlock( void *address, size_t length );
#endif /* Unix-variant-specific includes */

/* Under many Unix variants the SYSV/Posix mlock() call can be used, but 
   only by the superuser (with occasional OS-specific variants, for example 
   under some newer Linux variants the caller needs the specific 
   CAP_IPC_LOCK privilege rather than just generally being root).  OSF/1 has 
   mlock(), but this is defined to the nonexistant memlk() so we need to 
   special-case it out.  QNX (depending on the version) either doesn't have 
   mlock() at all or it's a dummy that just returns -1, so we no-op it out.  
   Aches, A/UX, PHUX, Linux < 1.3.something, and Ultrix don't even pretend 
   to have mlock().  Many systems also have plock(), but this is pretty 
   crude since it locks all data, and also has various other shortcomings.  
   Finally, PHUX has datalock(), which is just a plock() variant */

#if defined( _AIX ) || defined( __alpha__ ) || defined( __aux ) || \
	defined( _CRAY ) || defined( __CYGWIN__ ) || defined( __hpux ) || \
	( defined( __linux__ ) && OSVERSION < 2 ) || \
	defined( _M_XENIX ) || defined( __osf__ ) || \
	( defined( __QNX__ ) && OSVERSION <= 6 ) || \
	defined( __TANDEM_NSK__ ) || defined( __TANDEM_OSS__ ) || \
	defined( __ultrix )
  #define mlock( a, b )		1
  #define munlock( a, b )
#endif /* Unix OS-specific defines */

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	if( !mlock( ( void * ) memHdrPtr, memHdrPtr->size ) )
		memHdrPtr->flags |= MEM_FLAG_LOCKED;
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory was locked, unlock it now */
	if( memHdrPtr->flags & MEM_FLAG_LOCKED )
		munlock( ( void * ) memHdrPtr, memHdrPtr->size );
	}

#elif defined( __WIN32__ ) && !defined( NT_DRIVER )

/* For the Win32 debug build we enable extra checking for heap corruption.
   This isn't anywhere near as good as proper memory checkers like Bounds
   Checker, but it can catch some errors */

#if !defined( NDEBUG ) && !defined( NT_DRIVER ) && !defined( __BORLANDC__ )
  #define USE_HEAP_CHECKING
  #include <crtdbg.h>
#endif /* Win32 debug version */

/* Get the start address of a page and, given an address in a page and a 
   size, determine on which page the data ends.  These are used to determine 
   which pages a memory block covers */

#if defined( _MSC_VER ) && ( _MSC_VER >= 1400 )
  #define PTR_TYPE	INT_PTR 
#else
  #define PTR_TYPE	long
#endif /* Newer versions of VC++ */

#define getPageStartAddress( address ) \
			( ( PTR_TYPE ) ( address ) & ~( pageSize - 1 ) )
#define getPageEndAddress( address, size ) \
			getPageStartAddress( ( PTR_TYPE ) address + ( size ) - 1 )

/* Under Win95 the VirtualLock() function is implemented as 'return( TRUE )' 
   ("Thank Microsoft kids" - "Thaaaanks Bill").  Under NT the function does 
   actually work, but with a number of caveats.  The main one is that it has 
   been claimed that VirtualLock() only guarantees that the memory won't be 
   paged while a thread in the process is running, and when all threads are 
   preempted the memory is still a target for paging.  This would mean that 
   on a loaded system a process that was idle for some time could have the 
   memory unlocked by the system and swapped out to disk (actually with NT's 
   somewhat strange paging strategy and gradual creeping takeover of free 
   memory for disk buffers, it can get paged even on a completely unloaded 
   system).  However, attempts to force data to be paged under Win2K and XP 
   under various conditions have been unsuccesful so it may be that the 
   behaviour changed in post-NT versions of the OS.  In any case 
   VirtualLock() under these newer OSes seems to be fairly effective in 
   keeping data off disk.

   An additional concern is that although VirtualLock() takes arbitrary 
   memory pointers and a size parameter, the locking is done on a per-page 
   basis so that unlocking a region that shares a page with another locked 
   region means that both reqions are unlocked.  Since VirtualLock() doesn't 
   do reference counting (emulating the underlying MMU page locking even 
   though it seems to implement an intermediate layer above the MMU so it 
   could in theory do this), the only way around this is to walk the chain 
   of allocated blocks and not unlock a block if there's another block 
   allocated on the same page.  Ick.

   For the NT kernel driver, the memory is always allocated from the non-
   paged pool so there's no need for these gyrations */

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	if( VirtualLock( ( void * ) memHdrPtr, memHdrPtr->size ) )
		memHdrPtr->flags |= MEM_FLAG_LOCKED;
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	MEM_INFO_HEADER *currentBlockPtr;
	PTR_TYPE block1PageAddress, block2PageAddress;
	const int pageSize = getSysVar( SYSVAR_PAGESIZE );

	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory block isn't locked, there's nothing to do */
	if( !( memHdrPtr->flags & MEM_FLAG_LOCKED ) )
		return;

	/* Because VirtualLock() works on a per-page basis, we can't unlock a
	   memory block if there's another locked block on the same page.  The
	   only way to manage this is to walk the block list checking to see
	   whether there's another block allocated on the same page.  Although in
	   theory this could make freeing memory rather slow, in practice there
	   are only a small number of allocated blocks to check so it's
	   relatively quick, especially compared to the overhead imposed by the
	   lethargic VC++ allocator.  The only real disadvantage is that the
	   allocation objects remain locked while we do the free, but this
	   isn't any worse than the overhead of touchAllocatedPages().  Note 
	   that the following code assumes that an allocated block will never 
	   cover more than two pages, which is always the case.

	   First we calculate the addresses of the page(s) in which the memory 
	   block resides */
	block1PageAddress = getPageStartAddress( memHdrPtr );
	block2PageAddress = getPageEndAddress( memHdrPtr, memHdrPtr->size );
	if( block1PageAddress == block2PageAddress )
		block2PageAddress = 0;

	/* Walk down the block list checking whether the page(s) contain another 
	   locked block */
	for( currentBlockPtr = krnlData->allocatedListHead; \
		 currentBlockPtr != NULL; currentBlockPtr = currentBlockPtr->next )
		{
		const PTR_TYPE currentPage1Address = \
						getPageStartAddress( currentBlockPtr );
		PTR_TYPE currentPage2Address = \
						getPageEndAddress( currentBlockPtr, currentBlockPtr->size );

		if( currentPage1Address == currentPage2Address )
			currentPage2Address = 0;

		/* If there's another block allocated on either of the pages, don't
		   unlock it */
		if( block1PageAddress == currentPage1Address || \
			block1PageAddress == currentPage2Address )
			{
			block1PageAddress = 0;
			if( !block2PageAddress )
				break;
			}
		if( block2PageAddress == currentPage1Address || \
			block2PageAddress == currentPage2Address )
			{
			block2PageAddress = 0;
			if( !block1PageAddress )
				break;
			}
		}

	/* Finally, if either page needs unlocking, do so.  The supplied size is 
	   irrelevant since the entire page the memory is on is unlocked */
	if( block1PageAddress )
		VirtualUnlock( ( void * ) block1PageAddress, 16 );
	if( block2PageAddress )
		VirtualUnlock( ( void * ) block2PageAddress, 16 );
	}
#else

/* For everything else we no-op it out */

#define lockMemory( memHdrPtr )
#define unlockMemory( memHdrPtr )

#endif /* OS-specific page-locking handling */

/****************************************************************************
*																			*
*						OS-Specific Nonpageable Allocators					*
*																			*
****************************************************************************/

/* Some OSes handle page-locking by explicitly locking an already-allocated
   address range, others require the use of a special allocate-nonpageable-
   memory function.  For the latter class we redefine the standard 
   clAlloc()/clFree() macros to use the appropriate OS-specific allocators */

#if defined( __BEOS__xxx )	/* See comment below */

/* BeOS' create_area(), like most of the low-level memory access functions 
   provided by different OSes, functions at the page level so we round the 
   size up to the page size.  We can mitigate the granularity somewhat by 
   specifying lazy locking, which means that the page isn't locked until it's 
   committed.

   In pre-open-source BeOS, areas were bit of a security tradeoff because 
   they were globally visible(!!!) through the use of find_area(), so that 
   any other process in the system could find them.  An attacker could 
   always find the app's malloc() arena anyway because of this, but putting 
   data directly into areas made the attacker's task somewhat easier.  Open-
   source BeOS fixed this, mostly because it would have taken extra work to 
   make areas explicitly globally visible and no-one could see a reason for 
   this, so it's somewhat safer there.

   However, the implementation of create_area() in the open-source BeOS 
   seems to be rather flaky (simply creating an area and then immediately 
   destroying it again causes a segmentation violation) so it may be 
   necessary to turn it off for some BeOS releases.
   
   In more recent open-source BeOS releases create_area() simply maps to
   mmap(), and that uses a function convert_area_protection_flags() to
   convert the BeOS to Posix flags which simply discards everything but
   AREA_READ, AREA_WRITE, and AREA_EXEC, so it appears that create_area()
   can no longer allocate non-pageable memory.  If the original behaviour is 
   ever restored then the code will need to be amended to add the following
   member to MEM_INFO_HEADER:

	area_id areaID;				// Needed for page locking under BeOS

   and save the areaID after the create_area() call:

	memHdrPtr->areaID = areaID; */

#define clAlloc( string, size )		beosAlloc( size )
#define clFree( string, memblock )	beosFree( memblock )

static void *beosAlloc( const int size )
	{ 
	void *memPtr = NULL; 
	area_id areaID; 

	areaID = create_area( "memory_block", &memPtr, B_ANY_ADDRESS,
						  roundUp( size + MEM_INFO_HEADERSIZE, B_PAGE_SIZE ),
						  B_LAZY_LOCK, B_READ_AREA | B_WRITE_AREA );
	if( areaID < B_NO_ERROR )
		return( NULL );

	return( memPtr );
	}

static void beosFree( void *memPtr )
	{
	MEM_INFO_HEADER *memHdrPtr = memPtr;
	area_id areaID; 

	areaID = memHdrPtr->areaID;
	zeroise( memPtr, memHdrPtr->size );
	delete_area( areaID );
	}

#elif defined( __CHORUS__ )

/* ChorusOS is one of the very few embedded OSes with paging capabilities,
   fortunately there's a way to allocate nonpageable memory if paging is
   enabled */

#include <mem/chMem.h>

#define clAlloc( string, size )		chorusAlloc( size )
#define clFree( string, memblock )	chorusFree( memblock )

static void *chorusAlloc( const int size )
	{ 
	KnRgnDesc rgnDesc = { K_ANYWHERE, size + MEM_INFO_HEADERSIZE, \
						  K_WRITEABLE | K_NODEMAND };

	if( rgnAllocate( K_MYACTOR, &rgnDesc ) != K_OK )
		return( NULL );

	return( rgnDesc.startAddr );
	}

static void chorusFree( void *memPtr )
	{
	MEM_INFO_HEADER *memHdrPtr = memPtr;
	KnRgnDesc rgnDesc = { K_ANYWHERE, 0, 0 };

	rgnDesc.size = memHdrPtr->size;
	rgnDesc.startAddr = memPtr;
	zeroise( memPtr, memHdrPtr->size );
	rgnFree( K_MYACTOR, &rgnDesc );
	}
#endif /* OS-specific nonpageable allocation handling */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Calculate the checksum for a memory header block */

STDC_NONNULL_ARG( ( 1 ) ) \
static int checksumMemHdr( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	const int memHdrChecksum = memHdrPtr->checksum;
	int checksum;

	memHdrPtr->checksum = 0;
	checksum = checksumData( memHdrPtr, MEM_INFO_HEADERSIZE );
	memHdrPtr->checksum = memHdrChecksum;

	return( checksum );
	}

/* Set the checksum for a block of memory */

STDC_NONNULL_ARG( ( 1 ) ) \
static void setMemChecksum( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	MEM_INFO_TRAILER *memTrlPtr;

	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER * ) ) );

	memHdrPtr->checksum = 0;	/* Set mutable members to zero */
	memHdrPtr->checksum = checksumData( memHdrPtr, MEM_INFO_HEADERSIZE );
	memTrlPtr = ( MEM_INFO_TRAILER * ) \
				( ( BYTE * ) memHdrPtr + memHdrPtr->size - MEM_INFO_TRAILERSIZE );
	memTrlPtr->checksum = memHdrPtr->checksum;
	}

/* Check that a memory block is in order */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkMemBlockHdr( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	const MEM_INFO_TRAILER *memTrlPtr;
	int checksum;

	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER * ) ) );

	/* Make sure that the general header information is valid.  This is a 
	   quick check for obviously-invalid blocks, as well as ensuring that a 
	   corrupted size member doesn't result in us reading off into the 
	   weeds */
	if( memHdrPtr->size < MEM_INFO_HEADERSIZE + MIN_ALLOC_SIZE + \
						  MEM_INFO_TRAILERSIZE || \
		memHdrPtr->size > MEM_INFO_HEADERSIZE + MAX_ALLOC_SIZE + \
						  MEM_INFO_TRAILERSIZE )
		return( FALSE );
	if( memHdrPtr->flags & ~MEM_FLAG_MASK )
		return( FALSE );

	/* Everything seems kosher so far, check that the header hasn't been 
	   altered */
	checksum = checksumMemHdr( memHdrPtr );
	if( checksum != memHdrPtr->checksum )
		return( FALSE );

	/* Check that the trailer hasn't been altered */
	memTrlPtr = ( MEM_INFO_TRAILER * ) \
				( ( BYTE * ) memHdrPtr + memHdrPtr->size - MEM_INFO_TRAILERSIZE );
	if( memHdrPtr->checksum != memTrlPtr->checksum )
		return( FALSE );
	
	return( TRUE );
	}

/* Insert and unlink a memory block from a list of memory blocks, with 
   appropriate updates of memory checksums and other information.  We keep 
   the code for this in distinct functions to make sure that an exception-
   condition doesn't force an exit without the memory mutex unlocked */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int insertMemBlock( INOUT MEM_INFO_HEADER **allocatedListHeadPtr, 
						   INOUT MEM_INFO_HEADER **allocatedListTailPtr, 
						   INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	MEM_INFO_HEADER *allocatedListHead = *allocatedListHeadPtr;
	MEM_INFO_HEADER *allocatedListTail = *allocatedListTailPtr;

	assert( isWritePtr( allocatedListHeadPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListHead == NULL || \
			isWritePtr( allocatedListHead, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( allocatedListTailPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListTail == NULL || \
			isWritePtr( allocatedListTail, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER * ) ) );

	/* Precondition: The memory block list is empty, or there's at least a 
	   one-entry list present */
	REQUIRES( ( allocatedListHead == NULL && allocatedListTail == NULL ) || \
			  ( allocatedListHead != NULL && allocatedListTail != NULL ) );

	/* If it's a new list, set up the head and tail pointers and return */
	if( allocatedListHead == NULL )
		{
		/* In yet another of gcc's endless supply of compiler bugs, if the
		   following two lines of code are combined into a single line then
		   the write to the first value, *allocatedListHeadPtr, ends up 
		   going to some arbitrary memory location and only the second
		   write goes to the correct location (completely different code is
		   generated for the two writes)  This leaves 
		   krnlData->allocatedListHead as a NULL pointer, leading to an
		   exception being triggered the next time that it's accessed */
#if defined( __GNUC__ ) && ( __GNUC__ == 4 )
		*allocatedListHeadPtr = memHdrPtr;
		*allocatedListTailPtr = memHdrPtr;
#else
		*allocatedListHeadPtr = *allocatedListTailPtr = memHdrPtr;
#endif /* gcc 4.x compiler bug */

		return( CRYPT_OK );
		}

	/* It's an existing list, add the new element to the end */
	REQUIRES( checkMemBlockHdr( allocatedListTail ) );
	allocatedListTail->next = memHdrPtr;
	setMemChecksum( allocatedListTail );
	memHdrPtr->prev = allocatedListTail;
	*allocatedListTailPtr = memHdrPtr;

	/* Postcondition: The new block has been linked into the end of the 
	   list */
	ENSURES( allocatedListTail->next == memHdrPtr && \
			 memHdrPtr->prev == allocatedListTail && \
			 memHdrPtr->next == NULL );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int unlinkMemBlock( INOUT MEM_INFO_HEADER **allocatedListHeadPtr, 
						   INOUT MEM_INFO_HEADER **allocatedListTailPtr, 
						   INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	MEM_INFO_HEADER *allocatedListHead = *allocatedListHeadPtr;
	MEM_INFO_HEADER *allocatedListTail = *allocatedListTailPtr;
	MEM_INFO_HEADER *nextBlockPtr = memHdrPtr->next;
	MEM_INFO_HEADER *prevBlockPtr = memHdrPtr->prev;

	assert( isWritePtr( allocatedListHeadPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListHead == NULL || \
			isWritePtr( allocatedListHead, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( allocatedListTailPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListTail == NULL || \
			isWritePtr( allocatedListTail, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER * ) ) );

	/* If we're removing the block from the start of the list, make the
	   start the next block */
	if( memHdrPtr == allocatedListHead )
		{
		REQUIRES( prevBlockPtr == NULL );

		*allocatedListHeadPtr = nextBlockPtr;
		}
	else
		{
		REQUIRES( prevBlockPtr != NULL && \
				  prevBlockPtr->next == memHdrPtr );

		/* Delete from the middle or end of the list */
		REQUIRES( checkMemBlockHdr( prevBlockPtr ) );
		prevBlockPtr->next = nextBlockPtr;
		setMemChecksum( prevBlockPtr );
		}
	if( nextBlockPtr != NULL )
		{
		REQUIRES( nextBlockPtr->prev == memHdrPtr );

		REQUIRES( checkMemBlockHdr( nextBlockPtr ) );
		nextBlockPtr->prev = prevBlockPtr;
		setMemChecksum( nextBlockPtr );
		}

	/* If we've removed the last element, update the end pointer */
	if( memHdrPtr == allocatedListTail )
		{
		REQUIRES( nextBlockPtr == NULL );

		*allocatedListTailPtr = prevBlockPtr;
		}

	/* Clear the current block's pointers, just to be clean */
	memHdrPtr->next = memHdrPtr->prev = NULL;

	return( CRYPT_OK );
	}

#if 0	/* Currently unused, in practice would be called from a worker thread
		   that periodically touches all secure-data pages */

/* Walk the allocated block list touching each page.  In most cases we don't
   need to explicitly touch the page since the allocated blocks are almost
   always smaller than the MMU's page size and simply walking the list
   touches them, but in some rare cases we need to explicitly touch each
   page */

static void touchAllocatedPages( void )
	{
	MEM_INFO_HEADER *memHdrPtr;
	const int pageSize = getSysVar( SYSVAR_PAGESIZE );

	/* Lock the allocation object to ensure that other threads don't try to
	   access them */
	MUTEX_LOCK( allocation );

	/* Walk down the list (which implicitly touches each page).  If the
	   allocated region is larger than the page size, explicitly touch each 
	   additional page */
	for( memHdrPtr = krnlData->allocatedListHead; memHdrPtr != NULL;
		 memHdrPtr = memHdrPtr->next )
		{
		/* If the allocated region has pages beyond the first one (which 
		   we've already touched by accessing the header), explicitly
		   touch those pages as well */
		if( memHdrPtr->size > pageSize )
			{
			BYTE *memPtr = ( BYTE * ) memHdrPtr + pageSize;
			int memSize = memHdrPtr->size;

			/* Touch each page.  The rather convoluted expression is to try
			   and stop it from being optimised away - it always evaluates to
			   true since we only get here if allocatedListHead != NULL, but
			   hopefully the compiler won't be able to figure that out */
			while( memSize > pageSize )
				{
				if( *memPtr || krnlData->allocatedListHead != NULL )
					memPtr += pageSize;
				memSize -= pageSize;
				}
			}
		}

	/* Unlock the allocation object to allow access by other threads */
	MUTEX_UNLOCK( allocation );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

/* Create and destroy the secure allocation information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initAllocation( INOUT KERNEL_DATA *krnlDataPtr )
	{
	int status;

	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	/* Clear the list head and tail pointers */
	krnlData->allocatedListHead = krnlData->allocatedListTail = NULL;

	/* Initialize any data structures required to make the allocation thread-
	   safe */
	MUTEX_CREATE( allocation, status );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}

void endAllocation( void )
	{
	/* Destroy any data structures required to make the allocation thread-
	   safe */
	MUTEX_DESTROY( allocation );

	krnlData = NULL;
	}

/****************************************************************************
*																			*
*					Windows Secure Memory Allocation Functions				*
*																			*
****************************************************************************/

/* A safe malloc function that performs page locking if possible */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemalloc( OUT_BUFFER_ALLOC_OPT( size ) void **pointer, 
				  IN_LENGTH int size )
	{
	MEM_INFO_HEADER *memHdrPtr;
	MEM_INFO_TRAILER *memTrlPtr;
	BYTE *memPtr;
	const int alignedSize = roundUp( size, MEM_ROUNDSIZE );
	const int memSize = MEM_INFO_HEADERSIZE + alignedSize + \
						MEM_INFO_TRAILERSIZE;
	int status;

	static_assert( MEM_INFO_HEADERSIZE >= sizeof( MEM_INFO_HEADER ), \
				   "Memlock header size" );

	/* Make sure that the parameters are in order */
	if( !isWritePtrConst( pointer, sizeof( void * ) ) )
		retIntError();
	
	REQUIRES( size >= MIN_ALLOC_SIZE && size <= MAX_ALLOC_SIZE );

	/* Clear return values */
	*pointer = NULL;

	/* Allocate and clear the memory */
	if( ( memPtr = clAlloc( "krnlMemAlloc", memSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( memPtr, 0, memSize );

	/* Set up the memory block header and trailer */
	memHdrPtr = ( MEM_INFO_HEADER * ) memPtr;
	memTrlPtr = ( MEM_INFO_TRAILER * ) ( memPtr + MEM_INFO_HEADERSIZE + alignedSize );
	memHdrPtr->flags = MEM_FLAG_NONE;
	memHdrPtr->size = memSize;

	/* Try to lock the pages in memory */
	lockMemory( memHdrPtr );

	/* Lock the memory list */
	MUTEX_LOCK( allocation );

	/* Insert the new block into the list */
	status = insertMemBlock( ( MEM_INFO_HEADER ** ) &krnlData->allocatedListHead, 
							 ( MEM_INFO_HEADER ** ) &krnlData->allocatedListTail, 
							 memHdrPtr );

	/* Calculate the checksums for the memory block */
	if( cryptStatusOK( status ) )
		setMemChecksum( memHdrPtr );

	/* Perform heap sanity-checking if the functionality is available */
#ifdef USE_HEAP_CHECKING
	/* Sanity check to detect memory chain corruption */
	assert( _CrtIsValidHeapPointer( memHdrPtr ) );
	assert( memHdrPtr->next == NULL );
	assert( krnlData->allocatedListHead == krnlData->allocatedListTail || \
			_CrtIsValidHeapPointer( memHdrPtr->prev ) );
#endif /* USE_HEAP_CHECKING */

	MUTEX_UNLOCK( allocation );

	if( cryptStatusOK( status ) )
		*pointer = memPtr + MEM_INFO_HEADERSIZE;

	return( status );
	}

/* A safe free function that scrubs memory and zeroes the pointer.

	"You will softly and suddenly vanish away
	 And never be met with again"	- Lewis Carroll,
									  "The Hunting of the Snark" */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemfree( INOUT_PTR void **pointer )
	{
	MEM_INFO_HEADER *memHdrPtr;
	BYTE *memPtr;
	int status;

	assert( isReadPtr( pointer, sizeof( void * ) ) );

	/* Make sure that the parameters are in order */
	if( !isReadPtrConst( pointer, sizeof( void * ) ) || \
		!isReadPtrConst( *pointer, MIN_ALLOC_SIZE ) )
		retIntError();

	/* Recover the actual allocated memory block data from the pointer */
	memPtr = ( ( BYTE * ) *pointer ) - MEM_INFO_HEADERSIZE;
	if( !isReadPtrConst( memPtr, MEM_INFO_HEADERSIZE ) )
		retIntError();
	memHdrPtr = ( MEM_INFO_HEADER * ) memPtr;

	/* Lock the memory list */
	MUTEX_LOCK( allocation );

	/* Make sure that the memory header information and canaries are 
	   valid */
	if( !checkMemBlockHdr( memHdrPtr ) )
		{
		MUTEX_UNLOCK( allocation );

		/* The memory block doesn't look right, don't try and go any 
		   further */
		DEBUG_DIAG(( "Attempt to free invalid memory segment at %lX inside "
					 "memory block at %lX", *pointer, memHdrPtr ));
		retIntError();
		}

	/* Perform heap sanity-checking if the functionality is available */
#ifdef USE_HEAP_CHECKING
	/* Sanity check to detect memory chain corruption */
	assert( _CrtIsValidHeapPointer( memHdrPtr ) );
	assert( memHdrPtr->next == NULL || \
			_CrtIsValidHeapPointer( memHdrPtr->next ) );
	assert( memHdrPtr->prev == NULL || \
			_CrtIsValidHeapPointer( memHdrPtr->prev ) );
#endif /* USE_HEAP_CHECKING */

	/* Unlink the memory block from the list */
	status = unlinkMemBlock( ( MEM_INFO_HEADER ** ) &krnlData->allocatedListHead, 
							 ( MEM_INFO_HEADER ** ) &krnlData->allocatedListTail, 
							 memHdrPtr );

	MUTEX_UNLOCK( allocation );

	/* Zeroise the memory (including the memlock info), free it, and zero
	   the pointer */
	zeroise( memPtr, memHdrPtr->size );
	unlockMemory( memHdrPtr );
	clFree( "krnlMemFree", memPtr );
	*pointer = NULL;

	return( status );
	}
