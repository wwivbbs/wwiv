/****************************************************************************
*																			*
*						PalmOS Randomness-Gathering Code					*
*						Copyright Peter Gutmann 2003-2004					*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* General includes */

#include <sys/time.h>
#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */

#include <AppMgr.h>
#include <Event.h>
#include <FeatureMgr.h>
#include <Font.h>
#include <Form.h>
#include <Menu.h>
#include <MemoryMgr.h>
#include <SystemMgr.h>
#include <SysThread.h>
#include <TimeMgr.h>
#include <VFSMgr.h>
#include <Window.h>

/* The size of the intermediate buffer used to accumulate polled data.  We
   make this somewhat smaller than the default 4K used for other entropy-
   polling modules because the default PalmOS thread stack size is a
   somewhat pathetic 4K, and we don't want to use all of it just on the
   entropy buffer */

#define RANDOM_BUFSIZE	1024

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	WinHandle winHandle;
	Coord xCoord, yCoord;
	Boolean flag;
	uint64_t ticks;
	nsecs_t nsTime;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get the event-available and low-level event-available flag, current
	   pen status, and handle of the window with the input focus */
	flag = EvtEventAvail();
	addRandomValue( randomState, flag );
	flag = EvtSysEventAvail( TRUE );
	addRandomValue( randomState, flag );
	EvtGetPen( &xCoord, &yCoord, &flag );
	addRandomValue( randomState, xCoord );
	addRandomValue( randomState, yCoord );
	winHandle = EvtGetFocusWindow();
	addRandomValue( randomState, winHandle );

	/* Get the number of ticks of the (software) millisecond clock used
	   by the scheduler, and the length of time in nanoseconds since the
	   last reset */
	ticks = TimGetTicks();
	addRandomData( randomState, &ticks, sizeof( uint64_t ) );
	nsTime = SysGetRunTime();
	addRandomData( randomState, &nsTime, sizeof( nsecs_t ) );

	/* Get the value of the real-time and runtime clocks in nanoseconds.
	   One of these may just be a wrapper for SysGetRunTime(), in addition
	   it's likely that they're hardware-specific, being CPU-level cycle
	   counters of some kind */
	nsTime = system_real_time();
	addRandomData( randomState, &nsTime, sizeof( nsecs_t ) );
	nsTime = system_time();
	addRandomData( randomState, &nsTime, sizeof( nsecs_t ) );

	/* Flush any remaining data through */
	endRandomData( randomState, 5 );
	}

void slowPoll( void )
	{
	static BOOLEAN addedFixedItems = FALSE;
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	struct batteryInfoType {
		uint16_t warnThreshold;		/* Percent left for warn */
		uint16_t criticalThreshold;	/* Percent left for critical warn */
		uint16_t shutdownThreshold;	/* Percent left for shutdown */
		uint32_t timeout;			/* Battery timeout */
		SysBatteryKind type;		/* Battery type */
		Boolean pluggedIn;			/* Whether battery plugged in */
		uint8_t powerLevel;			/* Percent power remaining */
		} batteryInfo;
	const FontType *fontPtr;
	MenuBarType *menuPtr;
	void *stackStart, *stackEnd;
	MemHeapInfoType memInfo;
	RectangleType rectangleInfo;
	EvtQueueHandle evtQueueHandle;
	DatabaseID databaseID;
	FontID fontID;
	WinHandle winHandle;
	WinFlagsType winFlags;
	PatternType pattern;
	uint32_t version;
	uint16_t formID;
	uint8_t value;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get the handle of the current thread's event queue, current resource
	   database ID, start and end of the current thread's stack, ID and
	   pointer to the current font, ID and pointer to the currently active
	   form, and pointer to the currently active menu */
	evtQueueHandle = EvtGetThreadEventQueue();
	addRandomValue( randomState, evtQueueHandle );
	SysCurAppDatabase( &databaseID );
	addRandomValue( randomState, databaseID );
	SysGetStackInfo( &stackStart, &stackEnd );
	addRandomData( randomState, &stackStart, sizeof( void * ) );
	addRandomData( randomState, &stackEnd, sizeof( void * ) );
	fontID = FntGetFont();
	addRandomValue( randomState, fontID );
	fontPtr = FntGetFontPtr();
	addRandomData( randomState, &fontPtr, sizeof( FontType * ) );
	formID = FrmGetActiveFormID();
	addRandomValue( randomState, formID );
	if( formID > 0 )
		{
		FormType *formPtr;

		formPtr = FrmGetFormPtr( formID );
		addRandomData( randomState, &formPtr, sizeof( FormType * ) );
		}
	menuPtr = MenuGetActiveMenu();
	addRandomData( randomState, &menuPtr, sizeof( MenuBarType * ) );

	/* Get system memory info: heap base address, total memory, memory in
	   use, number of chunks allocated/free and chunk memory used/free,
	   available memory block info */
	MemDynHeapGetInfo( &memInfo );
	addRandomData( randomState, &memInfo, sizeof( MemHeapInfoType ) );

	/* Get the handle, creation flags, and size of the active window, the
	   screen window created at startup, and the current draw window, the
	   size and clipping rectangle of the draw window, the current pattern
	   type, and the current scaling mode */
	winHandle = WinGetActiveWindow();
	addRandomValue( randomState, winHandle );
	winFlags = WinGetWindowFlags( winHandle );
	addRandomValue( randomState, winFlags );
	WinGetWindowFrameRect( winHandle, &rectangleInfo );
	addRandomData( randomState, &rectangleInfo, sizeof( RectangleType ) );
	winHandle = WinGetDisplayWindow();
	addRandomValue( randomState, winHandle );
	winHandle = WinGetDrawWindow();
	addRandomValue( randomState, winHandle );
	winFlags = WinGetWindowFlags( winHandle );
	addRandomValue( randomState, winFlags );
	WinGetDrawWindowBounds( &rectangleInfo );
	addRandomData( randomState, &rectangleInfo, sizeof( RectangleType ) );
	WinGetClip( &rectangleInfo );
	addRandomData( randomState, &rectangleInfo, sizeof( RectangleType ) );
	pattern = WinGetPatternType();
	addRandomValue( randomState, pattern );
	if( FtrGet( sysFtrCreator, sysFtrNumWinVersion, &version ) == errNone && \
		version >= 5 )
		{
		uint32_t scaleType;

		/* Not implemented before PalmOS 5.3, requires the 1.5x Display
		   Feature Set to avoid generating a fatal alert */
		scaleType = WinGetScalingMode();
		addRandomValue( randomState, scaleType );
		}

	/* Get expansiode card info (capability flags, manufacturer, product,
	   and device info including unique serial number if available), and
	   media info (disk space, partition info, pseudo-HDD metrics) for all
	   expansion slots */
	if( FtrGet( sysFileCExpansionMgr,expFtrIDVersion, &version ) == errNone )
		{
		uint32_t slotIterator = expIteratorStart;
		uint16_t slotRefNum;

		while( slotIterator != expIteratorStop && \
			   ExpSlotEnumerate( &slotRefNum, &slotIterator ) == errNone )
			{
			ExpCardInfoType cardInfo;
			CardMetricsType cardMetrics;

			addRandomValue( randomState, slotRefNum );
			ExpCardInfo( slotRefNum, &cardInfo );
			addRandomData( randomState, &cardInfo, sizeof( ExpCardInfoType ) );
				ExpCardMetrics( slotRefNum, &cardMetrics );
			addRandomData( randomState, &cardMetrics, sizeof( CardMetricsType ) );
			}
		}

	/* Get attributes, filesystem type, mount info, media type, space used,
	   and total space for all mounted volumes */
	if( FtrGet( sysFileCVFSMgr, vfsFtrIDVersion, &version ) == errNone )
		{
		uint32_t volIterator = vfsIteratorStart;
		uint16_t volRefNum;

		while( volIterator != vfsIteratorStop && \
			   VFSVolumeEnumerate( &volRefNum, &volIterator ) == errNone )
			{
			VolumeInfoType volInfo;
			uint32_t volUsed, volTotal;

			addRandomValue( randomState, volRefNum );
			VFSVolumeInfo( volRefNum, &volInfo );
			addRandomData( randomState, &volInfo, sizeof( VolumeInfoType ) );
			VFSVolumeSize( volRefNum, &volUsed, &volTotal );
			addRandomValue( randomState, volUsed );
			addRandomValue( randomState, volTotal );
			}
		}

	/* Get battery state info */
	if( SysBatteryInfo( FALSE, &batteryInfo.warnThreshold,
						&batteryInfo.criticalThreshold,
						&batteryInfo.shutdownThreshold,
						&batteryInfo.timeout, &batteryInfo.type,
						&batteryInfo.pluggedIn,
						&batteryInfo.powerLevel ) == errNone )
		addRandomData( randomState, &batteryInfo,
					   sizeof( struct batteryInfoType ) );

	/* Get the LCD brightness and contrast level */
	value = SysLCDBrightness( FALSE, 0 );
	addRandomValue( randomState, value );
	value = SysLCDContrast( FALSE, 0 );
	addRandomValue( randomState, value );

	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		struct ftrInfoType {
			uint32_t creator;	/* Feature creator */
			uint16_t number;	/* Feature number */
			uint32_t value;		/* Feature value */
			} ftrInfo;
		uint16_t ftrIterator, romTokenSize;
		uint8_t *romToken;

		/* Get system features.  This includes a large amount of
		   information ranging from fairly static (extensive hardware
		   capability info, OS version/configuration data) through to
		   variable (default font, locale, etc) */
		for( ftrIterator = 0; \
			 FtrGetByIndex( ftrIterator, FALSE, &ftrInfo.creator, \
							&ftrInfo.number, &ftrInfo.value ) == errNone; \
			 ftrIterator++ )
			addRandomData( randomState, &ftrInfo, sizeof( struct ftrInfoType ) );

		/* Get the ROM serial number.  This is somewhat complex, for it to
		   be valid the function call has to succeed and the returned
		   pointer has to be non-null and the first byte of the returned
		   data can't be 0xFF */
		if( SysGetROMToken( sysROMTokenSnum, &romToken, \
							&romTokenSize ) == errNone && \
			romToken != NULL && *romToken != 0xFF )
			addRandomData( randomState, &romToken, romTokenSize );

		addedFixedItems = TRUE;
		}

	/* Flush any remaining data through */
	endRandomData( randomState, 100 );
	}
