/****************************************************************************
*																			*
*						Macintosh Randomness-Gathering Code					*
*			Copyright Peter Gutmann and Matthijs van Duin 1997-2002			*
*																			*
****************************************************************************/

/* This module is part of the cryptlib continuously seeded pseudorandom
   number generator.  For usage conditions, see random.c */

/* Mac threads are cooperatively scheduled (so they're what Win32 calls
   fibers rather than true threads) and there isn't any real equivalent of a
   mutex (only critical sections which prevent any other thread from being
   scheduled, which defeats the point of multithreading), so we don't support
   this pseudo-threading for randomness polling.  If proper threading were
   available, we'd use NewThread()/DisposeThread() to create/destroy the
   background randomness-polling thread */

/* General includes */

#include "crypt.h"
#include "random/random.h"

/* OS-specific includes */
/* Filled in by Matthijs van Duin */

#include <Power.h>
#include <Sound.h>
#include <Threads.h>
#include <Events.h>
#include <Scrap.h>
#include <MacTypes.h>
#include <Serial.h>
#include <Processes.h>
#include <Devices.h>
#include <Disks.h>
#include <OSUtils.h>
#include <Start.h>
#include <AppleTalk.h>
#include <DeskBus.h>
#include <Retrace.h>
#include <SCSI.h>
#include <SpeechSynthesis.h>
#include <Resources.h>
#include <Script.h>

#if defined __MWERKS__
  #pragma mpwc_relax off
  #pragma extended_errorcheck on
#endif

/* The size of the intermediate buffer used to accumulate polled data */

#define RANDOM_BUFSIZE	4096

void fastPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
/*	BatteryTimeRec batteryTimeInfo; */
	SMStatus soundStatus;
	ThreadID threadID;
	ThreadState threadState;
	EventRecord eventRecord;
	Point point;
	WindowPtr windowPtr;
	PScrapStuff scrapInfo;
	UnsignedWide usSinceStartup;
	BYTE dataBuffer[ 2 + 8 ];
/*	short driverRefNum; */
	UInt32 dateTime;
/*	int count, dummy; */
	NumVersion version;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Get the status of the last alert, how much battery time is remaining
	   and the voltage from all batteries, the internal battery status, the
	   current date and time and time since system startup in ticks, the
	   application heap limit and current and heap zone, free memory in the
	   current and system heap, microseconds since system startup, whether
	   QuickDraw has finished drawing, modem status, SCSI status
	   information, maximum block allocatable without compacting, available
	   stack space, the last QuickDraw error code */
/*	addRandomValue( randomState, GetAlertStage() );
	count = BatteryCount();
	while( count-- > 0 )
		{
		addRandomValue( randomState,
				   GetBatteryVoltage( count ) );
		GetBatteryTimes( count, &batteryTimeInfo );
		addRandomData( randomState, &batteryTimeInfo,
					   sizeof( BatteryTimeRec ) );
		}
	if( !BatteryStatus( buffer, dataBuffer + 1 ) )
		addRandomValue( randomState, dataBuffer );
*/	GetDateTime( &dateTime );
	addRandomValue( randomState, dateTime );
	addRandomValue( randomState, TickCount() );
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
	addRandomValue( randomState, GetApplLimit() );
	addRandomValue( randomState, GetZone() );
	addRandomValue( randomState, SystemZone() );
	addRandomValue( randomState, FreeMem() );
	addRandomValue( randomState, FreeMemSys() );
#endif
/*	MicroSeconds( &usSinceStartup );
	addRandomData( randomState, &usSinceStartup, sizeof( UnsignedWide ) ); */
	addRandomValue( randomState, QDDone( NULL ) );
/*	ModemStatus( dataBuffer );
	addRandomValue( randomState, dataBuffer[ 0 ] ); */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
	addRandomValue( randomState, SCSIStat() );
#endif
	addRandomValue( randomState, MaxBlock() );
	addRandomValue( randomState, StackSpace() );
	addRandomValue( randomState, QDError() );

	/* Get the event code and message, time, and mouse location for the next
	   event in the event queue and the OS event queue */
	if( EventAvail( everyEvent, &eventRecord ) )
		addRandomData( randomState, &eventRecord, sizeof( EventRecord ) );
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
	if( OSEventAvail( everyEvent, &eventRecord ) )
		addRandomData( randomState, &eventRecord, sizeof( EventRecord ) );
#endif

	/* Get all sorts of information such as device-specific info, grafport
	   information, visible and clipping region, pattern, pen, text, and
	   colour information, and other details, on the topmost window.  Also
	   get the window variant.  If there's a colour table record, add the
	   colour table as well */
	if( ( windowPtr = FrontWindow() ) != NULL )
		{
/*		CTabHandle colourHandle; */

#if !defined OPAQUE_TOOLBOX_STRUCTS || !OPAQUE_TOOLBOX_STRUCTS
		addRandomData( randomState, windowPtr, sizeof( GrafPort ) );
#endif
		addRandomValue( randomState, GetWVariant( windowPtr ) );
/*		if( GetAuxWin( windowPtr, colourHandle ) )
			{
			CTabPtr colourPtr;

			HLock( colourHandle );
			colourPtr = *colourHandle;
			addRandomData( randomState, colourPtr, sizeof( ColorTable ) );
			HUnlock( colourHandle );
			} */
		}

	/* Get mouse-related such as the mouse button status and mouse position,
	   information on the window underneath the mouse */
	addRandomValue( randomState, Button() );
	GetMouse( &point );
	addRandomData( randomState, &point, sizeof( Point ) );
	FindWindow( point, &windowPtr );
#if !defined OPAQUE_TOOLBOX_STRUCTS || !OPAQUE_TOOLBOX_STRUCTS
	if( windowPtr != NULL )
		addRandomData( randomState, windowPtr, sizeof( GrafPort ) );
#endif

	/* Get the size, handle, and location of the desk scrap/clipboard */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
	scrapInfo = InfoScrap();
	addRandomData( randomState, scrapInfo, sizeof( ScrapStuff ) );
#endif

	/* Get information on the current thread */
	threadID = kCurrentThreadID; /*GetThreadID( &threadID ); */
	GetThreadState( threadID, &threadState );
	addRandomData( randomState, &threadState, sizeof( ThreadState ) );

	/* Get the sound mananger status.  This gets the number of allocated
	   sound channels and the current CPU load from these channels */
	SndManagerStatus( sizeof( SMStatus ), &soundStatus );
	addRandomData( randomState, &soundStatus, sizeof( SMStatus ) );

	/* Get the speech manager version and status */
/*	version = SpeechManagerVersion();
	addRandomData( randomState, &version, sizeof( NumVersion ) );
	addRandomValue( randomState, SpeechBusy() );
*/
	/* Get the status of the serial port.  This gets information on recent
	   errors, read and write pending status, and flow control values */
/*	if( !OpenDriver( "\p.AIn", &driverRefNum ) )
		{
		SerStaRec serialStatus;

		SetStatus( driverRefNum, &serialStatus );
		addRandomData( randomState, &serialStatus, sizeof( SerStaRec ) );
		}
	if( !OpenDriver( "\p.AOut", &driverRefNum ) )
		{
		SerStaRec serialStatus;

		SetStatus( driverRefNum, &serialStatus );
		addRandomData( randomState, &serialStatus, sizeof( SerStaRec ) );
		} */

	/* Flush any remaining data through */
	endRandomData( randomState, 10 );
	}

void slowPoll( void )
	{
	RANDOM_STATE randomState;
	BYTE buffer[ RANDOM_BUFSIZE + 8 ];
	ProcessSerialNumber psn;
	GDHandle deviceHandle;
	GrafPtr currPort;
	QElemPtr queuePtr;
	QHdrPtr queueHdr;
	static BOOLEAN addedFixedItems = FALSE;

	initRandomData( randomState, buffer, RANDOM_BUFSIZE );

	/* Walk through the list of graphics devices adding information about
	   a device (IM VI 21-21) */
	deviceHandle = GetDeviceList();
	while( deviceHandle != NULL )
		{
		GDHandle currentHandle = deviceHandle;
		GDPtr devicePtr;

		HLock( ( Handle ) currentHandle );
		devicePtr = *currentHandle;
		deviceHandle = devicePtr->gdNextGD;
		addRandomData( randomState, devicePtr, sizeof( GDevice ) );
		HUnlock( ( Handle ) currentHandle );
		}

	/* Walk through the list of processes adding information about each
	   process, including the name and serial number of the process, file and
	   resource information, memory usage information, the name of the
	   launching process, launch time, and accumulated CPU time (IM VI 29-17) */
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN = kNoProcess;
	while( !GetNextProcess( &psn ) )
		{
		ProcessInfoRec infoRec;
		GetProcessInformation( &psn, &infoRec );
		addRandomData( randomState, &infoRec, sizeof( ProcessInfoRec ) );
		}

	/* Get the command type, trap address, and parameters for all commands in
	   the file I/O queue.  The parameters are quite complex and are listed
	   on page 117 of IM IV, and include reference numbers, attributes, time
	   stamps, length and file allocation information, finder info, and large
	   amounts of other volume and filesystem-related data */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
	if( ( queueHdr = GetFSQHdr() ) != NULL )
		queuePtr = queueHdr->qHead;
		while( queuePtr != NULL )
			{
			/* The queue entries are variant records of variable length so we
			   need to adjust the length parameter depending on the record
			   type */
			addRandomData( randomState, queuePtr, 32 ); /* dunno how big.. */
			queuePtr = queuePtr->qLink;
			}
#endif
	/* The following are fixed for the lifetime of the process so we only
	   add them once */
	if( !addedFixedItems )
		{
		Str255 appName, volName;
		GDHandle deviceHandle;
		Handle appHandle;
		DrvSts driveStatus;
		MachineLocation machineLocation;
		ProcessInfoRec processInfo;
		QHdrPtr vblQueue;
		SysEnvRec sysEnvirons;
		SysPPtr pramPtr;
		DefStartRec startupInfo;
		DefVideoRec videoInfo;
		DefOSRec osInfo;
		XPPParamBlock appleTalkParams;
		unsigned char *driverNames[] = {
			"\p.AIn", "\p.AOut", "\p.AppleCD", "\p.ATP", "\p.BIn", "\p.BOut", "\p.MPP",
			"\p.Print", "\p.Sony", "\p.Sound", "\p.XPP", NULL
			};
		SInt16 count, dummy, i, node, net, vRefNum, script;
		SInt32 lcount, volume;

		/* Get the current font family ID, node ID of the local AppleMumble
		   router, caret blink delay, CPU speed, double-click delay, sound
		   volume, application and system heap zone, the number of resource
		   types in the application, the number of sounds voices available,
		   the FRef of the current resource file, volume of the sysbeep,
		   primary line direction, computer SCSI disk mode ID, timeout before
		   the screen is dimmed and before the computer is put to sleep,
		   number of available threads in the thread pool, whether hard drive
		   spin-down is disabled, the handle to the i18n resources, timeout
		   time for the internal HDD, */
		addRandomValue( randomState, GetAppFont() );
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		addRandomValue( randomState, GetBridgeAddress() );
#endif
		addRandomValue( randomState, GetCaretTime() );
/*		addRandomValue( randomState, GetCPUSpeed() ); */
		addRandomValue( randomState, GetDblTime() );
		GetSysBeepVolume( &volume );
		addRandomValue( randomState, volume );
		GetDefaultOutputVolume( &volume );
		addRandomValue( randomState, volume );
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		addRandomValue( randomState, ApplicationZone() );
		addRandomValue( randomState, SystemZone() );
#endif
		addRandomValue( randomState, CountTypes() );
/*		CountVoices( &count ); ** seems to crash
		addRandomValue( randomState, count ); */
		addRandomValue( randomState, CurResFile() );
		GetSysBeepVolume( &lcount );
		addRandomValue( randomState, lcount );
		addRandomValue( randomState, GetSysDirection() );
/*		addRandomValue( randomState, GetSCSIDiskModeAddress() );
		addRandomValue( randomState, GetDimmingTimeout() );
		addRandomValue( randomState, GetSleepTimeout() ); */
		GetFreeThreadCount( kCooperativeThread, &count );
		addRandomValue( randomState, count );
/*		addRandomValue( randomState, IsSpindownDisabled() ); */
		addRandomValue( randomState, GetIntlResource( 0 ) );
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		GetTimeout( &count );
		addRandomValue( randomState, count );
#endif

		/* Get the number of documents/files which were selected when the app
		   started and for each document get the vRefNum, name, type, and
		   version -- OBSOLETE
		CountAppFiles( &dummy, &count );
		addRandomValue( randomState, count );
		while( count > 0 )
			{
			AppFile theFile;
			GetAppFiles( count, &theFile );
			addRandomData( randomState, &theFile, sizeof( AppFile ) );
			count--;
			} */

		/* Get the app's name, resource file reference number, and handle to
		   the finder information -- OBSOLETE
		GetAppParams( appName, appHandle, &count );
		addRandomData( randomState, appName, sizeof( Str255 ) );
		addRandomValue( randomState, appHandle );
		addRandomValue( randomState, count ); */

		/* Get all sorts of statistics such as physical information, disk and
		   write-protect present status, error status, and handler queue
		   information, on floppy drives attached to the system.  Also get
		   the volume name, volume reference number and number of bytes free,
		   for the volume in the drive */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( !DriveStatus( 1, &driveStatus ) )
			addRandomData( randomState, &driveStatus, sizeof (DrvSts) );
#endif
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( !GetVInfo( 1, volName, &vRefNum, &lcount ) )
			{
			addRandomData( randomState, volName, sizeof( Str255 ) );
			addRandomValue( randomState, vRefNum );
			addRandomValue( randomState, lcount );
			}
#endif
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( !DriveStatus( 2, &driveStatus ) )
			addRandomData( randomState, &driveStatus, sizeof (DrvSts) );
#endif
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( !GetVInfo( 2, volName, &vRefNum, &lcount ) )
			{
			addRandomData( randomState, volName, sizeof( Str255 ) );
			addRandomValue( randomState, vRefNum );
			addRandomValue( randomState, lcount );
			}
#endif
		/* Get information on the head and tail of the vertical retrace
		   queue */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( ( vblQueue = GetVBLQHdr() ) != NULL )
			addRandomData( randomState, vblQueue, sizeof( QHdr ) );
#endif
		/* Get the parameter RAM settings */
		pramPtr = GetSysPPtr();
		addRandomData( randomState, pramPtr, sizeof( SysParmType ) );

		/* Get information about the machines geographic location */
		ReadLocation( &machineLocation );
		addRandomData( randomState, &machineLocation,
					   sizeof( MachineLocation ) );

		/* Get information on current graphics devices including device
		   information such as dimensions and cursor information, and a
		   number of handles to device-related data blocks and functions, and
		   information about the dimentions and contents of the devices pixel
		   image as well as the images resolution, storage format, depth, and
		   colour usage */
		deviceHandle = GetDeviceList();
		do
			{
			GDPtr gdPtr;

			addRandomValue( randomState, deviceHandle );
			HLock( ( Handle ) deviceHandle );
			gdPtr = ( GDPtr ) *deviceHandle;
			addRandomData( randomState, gdPtr, sizeof( GDevice ) );
			addRandomData( randomState, gdPtr->gdPMap, sizeof( PixMap ) );
			HUnlock( ( Handle ) deviceHandle );
			}
		while( ( deviceHandle = GetNextDevice( deviceHandle ) ) != NULL );

		/* Get the current system environment, including the machine and
		   system software type, the keyboard type, where there's a colour
		   display attached, the AppleTalk driver version, and the VRefNum of
		   the system folder */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		SysEnvirons( curSysEnvVers, &sysEnvirons );
		addRandomData( randomState, &sysEnvirons, sizeof( SysEnvRec ) );
#endif

		/* Get the AppleTalk node ID and network number for this machine */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( GetNodeAddress( &node, &net ) )
			{
			addRandomValue( randomState, node );
			addRandomValue( randomState, net );
			}
#endif
		/* Get information on each device connected to the ADB including the
		   device handler ID, the devices ADB address, and the address of the
		   devices handler and storage area */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		count = CountADBs();
		while( count-- > 0 )
			{
			ADBDataBlock adbInfo;

			GetIndADB( &adbInfo, count );
			addRandomData( randomState, &adbInfo, sizeof( ADBDataBlock ) );
			}
#endif
		/* Open the most common device types and get the general device
		   status information and (if possible) device-specific status.  The
		   general device information contains the device handle and flags,
		   I/O queue information, event information, and other driver-related
		   details */

/* Try something like this again.. and ur a dead man, Peter ;-)
      -xmath */

/*		for( count = 0; driverNames[ count ] != NULL; count++ )
			{
			AuxDCEHandle dceHandle;
			short driverRefNum;

			** Try and open the driver **
			if( OpenDriver( driverNames[ count ], &driverRefNum ) )
				continue;

			** Get a handle to the driver control information (this could
			   also be done with GetDCtlHandle()) **
			Status( driverRefNum, 1, &dceHandle );
			HLock( dceHandle );
			addRandomData( randomState, *dceHandle,
							 sizeof( AuxDCE ) );
			HUnlock( dceHandle );
			CloseDriver( driverRefNum );
			} */

		/* Get the name and volume reference number for the current volume */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		GetVol( volName, &vRefNum );
		addRandomData( randomState, volName, sizeof( Str255 ) );
		addRandomValue( randomState, vRefNum );
#endif
		/* Get the time information, attributes, directory information and
		   bitmap, volume allocation information, volume and drive
		   information, pointers to various pieces of volume-related
		   information, and details on path and directory caches, for each
		   volume */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( ( queueHdr = GetVCBQHdr() ) != NULL )
			queuePtr = queueHdr->qHead;
			while ( queuePtr != NULL )
				{
				addRandomData( randomState, queuePtr, sizeof( VCB ) );
				queuePtr = queuePtr->qLink;
				}
#endif

		/* Get the driver reference number, FS type, and media size for each
		   drive */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		if( ( queueHdr = GetDrvQHdr() ) != NULL )
			queuePtr = queueHdr->qHead;
			while ( queuePtr != NULL )
				{
				addRandomData( randomState, queuePtr, sizeof( DrvQEl ) );
				queuePtr = queuePtr->qLink;
				}
#endif
		/* Get global script manager variables and vectors, including the
		   globals changed count, font, script, and i18n flags, various
		   script types, and cache information */
		for( count = 0; count < 30; count++ )
			addRandomValue( randomState, GetScriptManagerVariable( count ) );

		/* Get the script code for the font script the i18n script, and for
		   each one add the changed count, font, script, i18n, and display
		   flags, resource ID's, and script file information */
		script = FontScript();
		addRandomValue( randomState, script );
		for( count = 0; count < 30; count++ )
			addRandomValue( randomState, GetScriptVariable( script, count ) );
		script = IntlScript();
		addRandomValue( randomState, script );
		for( count = 0; count < 30; count++ )
			addRandomValue( randomState, GetScriptVariable( script, count ) );

		/* Get the device ID, partition, slot number, resource ID, and driver
		   reference number for the default startup device */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		GetDefaultStartup( &startupInfo );
		addRandomData( randomState, &startupInfo, sizeof( DefStartRec ) );
#endif
		/* Get the slot number and resource ID for the default video device */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		GetVideoDefault( &videoInfo );
		addRandomData( randomState, &videoInfo, sizeof( DefVideoRec ) );
#endif
		/* Get the default OS type */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		GetOSDefault( &osInfo );
		addRandomData( randomState, &osInfo, sizeof( DefOSRec ) );
#endif
		/* Get the AppleTalk command block and data size and number of
		   sessions */
#if !defined CALL_NOT_IN_CARBON || CALL_NOT_IN_CARBON
		ASPGetParms( &appleTalkParams, FALSE );
		addRandomData( randomState, &appleTalkParams,
					   sizeof( XPPParamBlock ) );
#endif
		addedFixedItems = TRUE;
		}

	/* Flush any remaining data through */
	endRandomData( randomState, 100 );
	}
