/****************************************************************************
*																			*
*							Secure Memory Management						*
*						Copyright Peter Gutmann 1995-2015					*
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
   header that doubles as a canary (which also makes it somewhat 
   unpredictable).

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
	SAFE_FLAGS flags;		/* Flags for this memory block.  The memory 
							   header is checksummed so we don't strictly
							   have to use safe flags, but we do it anyway
							   for appearances' sake */
	int size;				/* Size of the block (including the size
							   of the header and trailer) */
	DATAPTR prev, next;		/* Next, previous memory block */
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
		SET_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED );
#endif /* Non Mac OS X memory locking */
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory was locked, unlock it now */
#if !defined( CALL_NOT_IN_CARBON ) || CALL_NOT_IN_CARBON
	if( TEST_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED ) )
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
		SET_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED );
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
   CAP_IPC_LOCK privilege rather than just generally being root).  
   
   OSF/1 has mlock(), but this is defined to the nonexistant memlk() so we 
   need to special-case it out.  
   
   QNX (depending on the version) either doesn't have mlock() at all or it's 
   a dummy that just returns -1, so we no-op it out.  
   
   Aches, A/UX, PHUX, Linux < 1.3.something, and Ultrix don't even pretend 
   to have mlock().
   
   Many systems also have plock(), but this is pretty crude since it locks 
   all data, and also has various other shortcomings.  
   
   Finally, PHUX has datalock(), which is just a plock() variant.
   
   Linux 2.6.32 has a kernel bug in which, under high disk-load conditions 
   (100% disk usge) and with multiple cryptlib threads performing memory 
   locking/unlocking the process can get stuck in the "D" state, a.k.a. 
   TASK_UNINTERRUPTIBLE, which is an uninterruptible disk I/O sleep state.  
   If the process doesn't snap out of it when the I/O completes then it's 
   necessary to reboot the machine to clear the state.  To help find this 
   issue use:

	ps -eo ppid,pid,user,stat,pcpu,comm,wchan:32

   which shows D-state processes via the fourth column, the last column 
   will show the name of the kernel function in which the process is
   currently sleeping (also check dmesg for kernel Oops'es) */

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

/* In theory we could also tell the kernel to treat the memory specially if
   this facility is available.  In practice this doesn't really work though 
   because madvise() works on a granularity of page boundaries, despite the
   appearance of working on arbitrary memory regions.  This means that 
   unless the start address is page-aligned, it'll fail.  In addition it 
   functions at a much lower level than malloc(), so if we madvise() 
   malloc'd memory we'll end up messing with the heap in ways that will 
   break memory allocation.  For example MADV_WIPEONFORK will wipe the 
   entire page or page range containing the heap that the client gets, 
   corrupting the heap on fork.

  What this means is that we'd need to mmap() memory in order to madvise() 
  on it, and then implement our own allocator on top of that.  Or, every 
  time we allocate anything, make it a full page, again via mmap().  The 
  chances of something going wrong when we do our own memory management are 
  probably a lot higher than the chances of something ending up in a core 
  dump when we don't:

	#if defined( __linux__ ) && defined( _BSD_SOURCE ) && 0

	// Tell the kernel to exclude this memory region from core dumps if 
	// possible

	#ifdef MADV_DONTDUMP

	#include <sys/mman.h>

	( void ) madvise( void *addr, size_t length, MADV_DONTDUMP );

	#endif // MADV_DONTDUMP */

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	if( !mlock( ( void * ) memHdrPtr, memHdrPtr->size ) )
		SET_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED );
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory was locked, unlock it now */
	if( TEST_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED ) )
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

/* Under Win95/98/ME the VirtualLock() function was implemented as 
   'return( TRUE )' ("Thank Microsoft kids" - "Thaaaanks Bill"), but we 
   don't have to worry about those Windows versions any more.  Under Win32/
   Win64 the function does actually work, but with a number of caveats.  The 
   main one is that it was originally intended that VirtualLock() only locks
   memory into a processes' working set, in other words it guarantees that 
   the memory won't be paged while a thread in the process is running, and 
   when all threads are pre-empted the memory is still a target for paging.  
   This would mean that on a loaded system a process that was idle for some 
   time could have the memory unlocked by the system and swapped out to disk 
   (actually with older Windows incarnations like NT, their somewhat strange 
   paging strategy meant that it could potentially get paged even on a 
   completely unloaded system.  Even on relatively recent systems the 
   gradual creeping takeover of free memory for disk buffers/cacheing can
   cause problems, something that was still affecting Win64 systems during
   the Windows 7 time frame.  Ironically the 1GB cache size limit on Win32
   systems actually helped here because the cache couldn't grow beyond this
   size and most systems had more than 1GB of RAM, while on Win64 systems 
   without this limit there was more scope for excessive reads and writes to 
   consume all available memory due to cacheing).
   
   The lock-into-working-set approach was the original intention, however 
   the memory manager developers never got around to implementing the 
   unlock-if-all-threads idle part.  The behaviour of VirtualLock() was 
   evaluated back under Win2K and XP by trying to force data to be paged 
   under various conditions, which were unsuccesful, so VirtualLock() under 
   these OSes seems to be fairly effective in keeping data off disk.  In 
   newer versions of Windows the contract for VirtualLock() was changed to 
   match the actual implemented behaviour, so that now "pages are guaranteed 
   not to be written to the pagefile while they are locked".

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
   paged pool so there's no need for these gyrations.
   
   In addition to VirtualLock() we could also use VirtualAlloc(), however 
   this allocates in units of the allocation granularity, which is in theory
   system-dependent and obtainable via the dwAllocationGranularity field in
   the SYSTEM_INFO structure returned by GetSystemInfo() but in practice on
   x86 systems is always 64K, this means the memory is nicely aligned for
   efficient access but wastes 64K for every allocation */

static void lockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	if( VirtualLock( ( void * ) memHdrPtr, memHdrPtr->size ) )
		SET_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED );
	}

static void unlockMemory( INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	MEM_INFO_HEADER *currentBlockPtr;
	PTR_TYPE block1PageAddress, block2PageAddress;
	const int pageSize = getSysVar( SYSVAR_PAGESIZE );

	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER ) ) );

	/* If the memory block isn't locked, there's nothing to do */
	if( !TEST_FLAG( memHdrPtr->flags, MEM_FLAG_LOCKED ) )
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
	for( currentBlockPtr = DATAPTR_GET( krnlData->allocatedListHead ); \
		 currentBlockPtr != NULL; 
		 currentBlockPtr = DATAPTR_GET( currentBlockPtr->next ) )
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
	if( !CHECK_FLAGS( memHdrPtr->flags, MEM_FLAG_NONE, 
					  MEM_FLAG_MAX ) )
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
	DATAPTR_SET( allocatedListTail->next, memHdrPtr );
	setMemChecksum( allocatedListTail );
	DATAPTR_SET( memHdrPtr->prev, allocatedListTail );
	*allocatedListTailPtr = memHdrPtr;

	/* Postcondition: The new block has been linked into the end of the 
	   list */
	ENSURES( DATAPTR_GET( allocatedListTail->next ) == memHdrPtr && \
			 DATAPTR_GET( memHdrPtr->prev ) == allocatedListTail && \
			 DATAPTR_ISNULL( memHdrPtr->next ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int unlinkMemBlock( INOUT MEM_INFO_HEADER **allocatedListHeadPtr, 
						   INOUT MEM_INFO_HEADER **allocatedListTailPtr, 
						   INOUT MEM_INFO_HEADER *memHdrPtr )
	{
	MEM_INFO_HEADER *allocatedListHead = *allocatedListHeadPtr;
	MEM_INFO_HEADER *allocatedListTail = *allocatedListTailPtr;
	MEM_INFO_HEADER *nextBlockPtr = DATAPTR_GET( memHdrPtr->next );
	MEM_INFO_HEADER *prevBlockPtr = DATAPTR_GET( memHdrPtr->prev );

	assert( isWritePtr( allocatedListHeadPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListHead == NULL || \
			isWritePtr( allocatedListHead, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( allocatedListTailPtr, sizeof( MEM_INFO_HEADER * ) ) );
	assert( allocatedListTail == NULL || \
			isWritePtr( allocatedListTail, sizeof( MEM_INFO_HEADER ) ) );
	assert( isWritePtr( memHdrPtr, sizeof( MEM_INFO_HEADER * ) ) );

	REQUIRES( DATAPTR_ISVALID( memHdrPtr->next ) );
	REQUIRES( DATAPTR_ISVALID( memHdrPtr->prev ) );

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
				  DATAPTR_GET( prevBlockPtr->next ) == memHdrPtr );

		/* Delete from the middle or end of the list */
		REQUIRES( checkMemBlockHdr( prevBlockPtr ) );
		DATAPTR_SET( prevBlockPtr->next, nextBlockPtr );
		setMemChecksum( prevBlockPtr );
		}
	if( nextBlockPtr != NULL )
		{
		REQUIRES( DATAPTR_GET( nextBlockPtr->prev ) == memHdrPtr );

		REQUIRES( checkMemBlockHdr( nextBlockPtr ) );
		DATAPTR_SET( nextBlockPtr->prev, prevBlockPtr );
		setMemChecksum( nextBlockPtr );
		}

	/* If we've removed the last element, update the end pointer */
	if( memHdrPtr == allocatedListTail )
		{
		REQUIRES( nextBlockPtr == NULL );

		*allocatedListTailPtr = prevBlockPtr;
		}

	/* Clear the current block's pointers, just to be clean */
	DATAPTR_SET( memHdrPtr->next, NULL );
	DATAPTR_SET( memHdrPtr->prev, NULL );

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
	KERNEL_DATA *krnlData = getKrnlData();
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

			/* Touch each page.  The rather convoluted expression in the loop
			   body is to try and stop it from being optimised away - it 
			   always evaluates to true since we only get here if 
			   allocatedListHead != NULL, but hopefully the compiler won't 
			   be able to figure that out */
			LOOP_LARGE( memSize = memHdrPtr->size, memSize > pageSize, 
						memSize -= pageSize )
				{
				if( *memPtr || krnlData->allocatedListHead != NULL )
					memPtr += pageSize;
				ENSURES( LOOP_BOUND_OK );
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

CHECK_RETVAL \
int initAllocation( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	int status;

	assert( isWritePtr( krnlData, sizeof( KERNEL_DATA ) ) );

	/* Clear the allocated block list head and tail pointers */
	DATAPTR_SET( krnlData->allocatedListHead, NULL );
	DATAPTR_SET( krnlData->allocatedListTail, NULL );

	/* Initialize any data structures required to make the allocation thread-
	   safe */
	MUTEX_CREATE( allocation, status );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}

void endAllocation( void )
	{
	KERNEL_DATA *krnlData = getKrnlData();

	/* Destroy any data structures required to make the allocation thread-
	   safe */
	MUTEX_DESTROY( allocation );
	}

/****************************************************************************
*																			*
*						Secure Memory Allocation Functions					*
*																			*
****************************************************************************/

/* A safe malloc function that performs page locking if possible */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemalloc( OUT_BUFFER_ALLOC_OPT( size ) void **pointer, 
				  IN_LENGTH int size )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	MEM_INFO_HEADER *allocatedListHeadPtr, *allocatedListTailPtr; 
	MEM_INFO_HEADER *memHdrPtr;
	BYTE *memPtr;
	const int alignedSize = roundUp( size, MEM_ROUNDSIZE );
	const int memSize = MEM_INFO_HEADERSIZE + alignedSize + \
						MEM_INFO_TRAILERSIZE;
	int status;

	static_assert( MEM_INFO_HEADERSIZE >= sizeof( MEM_INFO_HEADER ), \
				   "Memlock header size" );

	/* Make sure that the parameters are in order */
	if( !isWritePtr( pointer, sizeof( void * ) ) )
		retIntError();
	
	REQUIRES( size >= MIN_ALLOC_SIZE && size <= MAX_ALLOC_SIZE );

	/* Clear return values */
	*pointer = NULL;

	/* Allocate and clear the memory */
	REQUIRES( rangeCheck( memSize, 1, MAX_INTLENGTH ) );
	if( ( memPtr = clAlloc( "krnlMemAlloc", memSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( memPtr, 0, memSize );

	/* Set up the memory block header and trailer */
	memHdrPtr = ( MEM_INFO_HEADER * ) memPtr;
	INIT_FLAGS( memHdrPtr->flags, MEM_FLAG_NONE );
	memHdrPtr->size = memSize;
	DATAPTR_SET( memHdrPtr->next, NULL );
	DATAPTR_SET( memHdrPtr->prev, NULL );

	/* Try to lock the pages in memory */
	lockMemory( memHdrPtr );

	/* Lock the memory list */
	MUTEX_LOCK( allocation );

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( krnlData->allocatedListHead ) || \
		!DATAPTR_ISVALID( krnlData->allocatedListTail ) )
		{
		MUTEX_UNLOCK( allocation );
		clFree( "krnlMemAlloc", memPtr );
		DEBUG_DIAG(( "Kernel memory data corrupted" ));
		retIntError();
		}

	/* Insert the new block into the list */
	allocatedListHeadPtr = DATAPTR_GET( krnlData->allocatedListHead );
	allocatedListTailPtr = DATAPTR_GET( krnlData->allocatedListTail );
	status = insertMemBlock( &allocatedListHeadPtr, &allocatedListTailPtr, 
							 memHdrPtr );
	if( cryptStatusError( status ) )
		{
		MUTEX_UNLOCK( allocation );
		clFree( "krnlMemAlloc", memPtr );
		retIntError();
		}
	DATAPTR_SET( krnlData->allocatedListHead, allocatedListHeadPtr );
	DATAPTR_SET( krnlData->allocatedListTail, allocatedListTailPtr );

	/* Calculate the checksums for the memory block */
	setMemChecksum( memHdrPtr );

	/* Perform heap sanity-checking if the functionality is available */
#ifdef USE_HEAP_CHECKING
	/* Sanity check to detect memory chain corruption */
	assert( _CrtIsValidHeapPointer( memHdrPtr ) );
	assert( DATAPTR_ISNULL( memHdrPtr->next ) );
	assert( DATAPTR_GET( krnlData->allocatedListHead ) == \
				DATAPTR_GET( krnlData->allocatedListTail ) || \
			_CrtIsValidHeapPointer( DATAPTR_GET( memHdrPtr->prev ) ) );
#endif /* USE_HEAP_CHECKING */

	MUTEX_UNLOCK( allocation );

	*pointer = memPtr + MEM_INFO_HEADERSIZE;

	return( CRYPT_OK );
	}

/* A safe free function that scrubs memory and zeroes the pointer.

	"You will softly and suddenly vanish away
	 And never be met with again"	- Lewis Carroll,
									  "The Hunting of the Snark" */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemfree( INOUT_PTR void **pointer )
	{
	KERNEL_DATA *krnlData = getKrnlData();
	MEM_INFO_HEADER *allocatedListHeadPtr, *allocatedListTailPtr; 
	MEM_INFO_HEADER *memHdrPtr;
	BYTE *memPtr;
	int status;

	assert( isReadPtr( pointer, sizeof( void * ) ) );

	/* Make sure that the parameters are in order */
	if( !isReadPtr( pointer, sizeof( void * ) ) || \
		!isReadPtr( *pointer, MIN_ALLOC_SIZE ) )
		retIntError();

	/* Recover the actual allocated memory block data from the pointer */
	memPtr = ( ( BYTE * ) *pointer ) - MEM_INFO_HEADERSIZE;
	if( !isReadPtr( memPtr, MEM_INFO_HEADERSIZE ) )
		retIntError();
	memHdrPtr = ( MEM_INFO_HEADER * ) memPtr;

	/* Lock the memory list */
	MUTEX_LOCK( allocation );

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( krnlData->allocatedListHead ) || \
		!DATAPTR_ISVALID( krnlData->allocatedListTail ) )
		{
		MUTEX_UNLOCK( allocation );
		DEBUG_DIAG(( "Kernel memory data corrupted" ));
		retIntError();
		}

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
	assert( DATAPTR_ISNULL( memHdrPtr->next ) || \
			_CrtIsValidHeapPointer( DATAPTR_GET( memHdrPtr->next ) ) );
	assert( DATAPTR_ISNULL( memHdrPtr->prev ) || \
			_CrtIsValidHeapPointer( DATAPTR_GET( memHdrPtr->prev ) ) );
#endif /* USE_HEAP_CHECKING */

	/* Unlink the memory block from the list */
	allocatedListHeadPtr = DATAPTR_GET( krnlData->allocatedListHead );
	allocatedListTailPtr = DATAPTR_GET( krnlData->allocatedListTail );
	status = unlinkMemBlock( &allocatedListHeadPtr, &allocatedListTailPtr, 
							 memHdrPtr );
	if( cryptStatusOK( status ) )
		{
		DATAPTR_SET( krnlData->allocatedListHead, allocatedListHeadPtr );
		DATAPTR_SET( krnlData->allocatedListTail, allocatedListTailPtr );
		}

	MUTEX_UNLOCK( allocation );

	/* Zeroise the memory (including the memlock info), free it, and zero
	   the pointer */
	zeroise( memPtr, memHdrPtr->size );
	unlockMemory( memHdrPtr );
	clFree( "krnlMemFree", memPtr );
	*pointer = NULL;

	return( status );
	}
