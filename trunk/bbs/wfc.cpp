/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/

#include "wwiv.h"


#if !defined ( __unix__ )

// Local Functions
void DisplayWFCScreen( const char *pszBuffer );
static char * pszScreenBuffer = NULL;

static int inst_num;


void wfc_cls() {
	if ( GetApplication()->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) ) {
		GetSession()->bout.ResetColors();
		GetSession()->localIO()->LocalCls();
		GetSession()->wfc_status = 0;
		GetSession()->localIO()->SetCursor( WLocalIO::cursorNormal );
	}
}


void wfc_init() {
	for ( int i = 0; i < 5; i++ ) {
		GetSession()->wfcdrvs[ i ] = 0;
	}

	WIniFile iniFile( WWIV_INI );
	if ( iniFile.Open( INI_TAG ) ) {
		const char *pszDriveList = iniFile.GetValue( "WFC_DRIVES" );
		if ( pszDriveList != NULL ) {
			for ( int j = 0; j < wwiv::strings::GetStringLength( pszDriveList ); j++ ) {
				GetSession()->wfcdrvs[ j ] = pszDriveList[ j ] - '@';
				if ( GetSession()->wfcdrvs[ j ] < 2 ) {
					GetSession()->wfcdrvs[ j ] = 2;
				}
			}
		} else {
			GetSession()->wfcdrvs[ 0 ] = 2;
		}
	}

	GetSession()->localIO()->SetCursor( WLocalIO::cursorNormal );            // add 4.31 Build3
	if ( GetApplication()->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) ) {
		GetSession()->wfc_status = 0;
		inst_num = 1;
	}
}


void wfc_update() {
	if ( !GetApplication()->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) ) {
		return;
	}

	instancerec ir;
	WUser u;

	get_inst_info( inst_num, &ir );
	GetApplication()->GetUserManager()->ReadUserNoCache( &u, ir.user );
	GetSession()->localIO()->LocalXYAPrintf( 57, 18, 15, "%-3d", inst_num );
	if ( ir.flags & INST_FLAGS_ONLINE ) {
		GetSession()->localIO()->LocalXYAPrintf( 42, 19, 14, "%-25.25s", u.GetUserNameAndNumber( ir.user ) );
	} else {
		GetSession()->localIO()->LocalXYAPrintf( 42, 19, 14, "%-25.25s", "Nobody" );
	}

	char szBuffer[ 255 ];
	szBuffer[0] = '\0';
	make_inst_str( inst_num, szBuffer, INST_FORMAT_WFC );
	GetSession()->localIO()->LocalXYAPrintf( 42, 20, 14, "%-25.25s", szBuffer );
	if ( num_instances() > 1 ) {
		do {
			++inst_num;
			if ( inst_num > num_instances() ) {
				inst_num = 1;
			}
		} while ( inst_num == GetApplication()->GetInstanceNumber() );
	}
}


bool iscdrom( char drive ) {
#if !defined ( __unix__ ) && !defined ( __APPLE__ )
	// TODO Make this function platform specific
	char szDrivePath[10];
	sprintf( szDrivePath, "%c:\\", drive + '@' );

	return ( ( GetDriveType( szDrivePath ) == DRIVE_CDROM ) ? true : false );
#else
	return false;
#endif
}

void wfc_screen() {
	char szBuffer[ 255 ];
	instancerec ir;
	WUser u;
	static double wfc_time=0, poll_time=0;

	if ( !GetApplication()->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) ) {
		return;
	}

	int nNumNewMessages = check_new_mail( GetSession()->usernum );
	std::unique_ptr<WStatus> pStatus(GetApplication()->GetStatusManager()->GetStatus());
	if ( GetSession()->wfc_status == 0 ) {
		GetSession()->localIO()->SetCursor( WLocalIO::cursorNone );
		GetSession()->localIO()->LocalCls();
		if ( pszScreenBuffer == NULL ) {
			pszScreenBuffer = new char[4000];
			WFile wfcFile( syscfg.datadir, WFC_DAT );
			if ( !wfcFile.Open( WFile::modeBinary | WFile::modeReadOnly ) ) {
				wfc_cls();
				std::cout << szBuffer << " NOT FOUND." << std::endl;
				GetApplication()->AbortBBS();
			}
			wfcFile.Read( pszScreenBuffer, 4000 );
		}
		DisplayWFCScreen( pszScreenBuffer );
		sprintf( szBuffer, "Activity and Statistics of %s Node %d", syscfg.systemname, GetApplication()->GetInstanceNumber() );
		GetSession()->localIO()->LocalXYAPrintf( 1 + ( ( 76 - strlen( szBuffer ) ) / 2 ), 4, 15, szBuffer );
		GetSession()->localIO()->LocalXYAPrintf( 8, 1, 14, fulldate() );
		std::string osVersion = WWIV_GetOSVersion();
		GetSession()->localIO()->LocalXYAPrintf( 40, 1, 3, "OS: " );
		GetSession()->localIO()->LocalXYAPrintf(44, 1, 14, osVersion.c_str());
		GetSession()->localIO()->LocalXYAPrintf( 21, 6, 14, "%d", pStatus->GetNumCallsToday() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 7, 14, "%d", fwaiting );
		if ( nNumNewMessages ) {
			GetSession()->localIO()->LocalXYAPrintf( 29, 7 , 3, "New:" );
			GetSession()->localIO()->LocalXYAPrintf( 34, 7 , 12, "%d", nNumNewMessages );
		}
		GetSession()->localIO()->LocalXYAPrintf( 21, 8, 14, "%d", pStatus->GetNumUploadsToday() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 9, 14, "%d", pStatus->GetNumMessagesPostedToday() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 10, 14, "%d", pStatus->GetNumLocalPosts() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 11, 14, "%d", pStatus->GetNumEmailSentToday() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 12, 14, "%d", pStatus->GetNumFeedbackSentToday() );
		GetSession()->localIO()->LocalXYAPrintf( 21, 13, 14, "%d Mins (%.1f%%)", pStatus->GetMinutesActiveToday(),
		        100.0 * static_cast<float>( pStatus->GetMinutesActiveToday() ) / 1440.0 );
		GetSession()->localIO()->LocalXYAPrintf( 58, 6, 14, "%s%s", wwiv_version, beta_version );

		GetSession()->localIO()->LocalXYAPrintf( 58, 7, 14, "%d", pStatus->GetNetworkVersion() );
		GetSession()->localIO()->LocalXYAPrintf( 58, 8, 14, "%d", pStatus->GetNumUsers() );
		GetSession()->localIO()->LocalXYAPrintf( 58, 9, 14, "%ld", pStatus->GetCallerNumber() );
		if ( pStatus->GetDays() ) {
			GetSession()->localIO()->LocalXYAPrintf( 58, 10, 14, "%.2f", static_cast<float>( pStatus->GetCallerNumber() ) /
			        static_cast<float>( pStatus->GetDays() ) );
		} else {
			GetSession()->localIO()->LocalXYAPrintf( 58, 10, 14, "N/A" );
		}
		GetSession()->localIO()->LocalXYAPrintf( 58, 11, 14, sysop2() ? "Available    " : "Not Available" );
		if ( ok_modem_stuff ) {
			GetSession()->localIO()->LocalXYAPrintf( 58, 12, 14, "%-20.20s", modem_i->name );
			GetSession()->localIO()->LocalXYAPrintf( 58, 13, 14, "%-20s", "Waiting For Call" );
		} else {
			GetSession()->localIO()->LocalXYAPrintf( 58, 12, 14, "Local %code", (syscfgovr.primaryport) ? 'M' : 'N' );
			GetSession()->localIO()->LocalXYAPrintf( 58, 13, 14, "Waiting For Command" );
		}

		int i = 0, i1 = 0;
		while ( GetSession()->wfcdrvs[i] > 0 && GetSession()->wfcdrvs[i] < GetApplication()->GetHomeDir()[0] && i1 < 5 ) {
			if ( iscdrom( static_cast<char>( GetSession()->wfcdrvs[i] ) ) ) {
				GetSession()->localIO()->LocalXYAPrintf( 2, 16 + i1, 3, "CDROM %c..", GetSession()->wfcdrvs[i] + '@' );
				GetSession()->localIO()->LocalXYAPrintf( 12 - 1, 17 + i1 - 1, 14, "%10.1f MB", 0.0f);
				i1++;
			} else {
				char szTmpPath[4];
				sprintf( szTmpPath, "%c:\\", GetSession()->wfcdrvs[i] + '@' );
				long lFreeDiskSpace = static_cast<long>( freek1( szTmpPath ) );
				char szTempDiskSize[81];
				if ( lFreeDiskSpace > 0 ) {
					if ( lFreeDiskSpace > 2048 ) {
						sprintf( szTempDiskSize, "%10.1f GB", static_cast<float>( lFreeDiskSpace ) / ( 1024.0 * 1024.0 ) );
					} else {
						sprintf( szTempDiskSize, "%10.1f MB", static_cast<float>( lFreeDiskSpace ) / 1024.0 );
					}
					GetSession()->localIO()->LocalXYAPrintf( 2, 16 + i1, 3, "Drive %c..", GetSession()->wfcdrvs[i] + '@' );
					GetSession()->localIO()->LocalXYAPrintf( 12 - 1, 17 + i1 - 1, 14, "%s", szTempDiskSize );
					i1++;
				}
			}
			i++;
		}

		get_inst_info( GetApplication()->GetInstanceNumber(), &ir );
		if ( ir.user < syscfg.maxusers && ir.user > 0 ) {
			GetApplication()->GetUserManager()->ReadUserNoCache( &u, ir.user );
			GetSession()->localIO()->LocalXYAPrintf( 33, 16, 14, "%-20.20s", u.GetUserNameAndNumber( ir.user ) );
		} else {
			GetSession()->localIO()->LocalXYAPrintf( 33, 16, 14, "%-20.20s", "Nobody" );
		}

		GetSession()->wfc_status = 1;
		wfc_update();
		poll_time = wfc_time = timer();
	} else {
		if ( ( timer() - wfc_time < GetSession()->screen_saver_time ) ||
		        ( GetSession()->screen_saver_time == 0 ) ) {
			GetSession()->localIO()->LocalXYAPrintf( 28, 1, 14, times() );
			GetSession()->localIO()->LocalXYAPrintf( 58, 11, 14, sysop2() ? "Available    " : "Not Available" );
			if ( timer() - poll_time > 10 ) {
				wfc_update();
				poll_time = timer();
			}
		} else {
			if ( ( timer() - poll_time > 10 ) || GetSession()->wfc_status == 1 ) {
				GetSession()->wfc_status = 2;
				GetSession()->localIO()->LocalCls();
				GetSession()->localIO()->LocalXYAPrintf(	WWIV_GetRandomNumber(38),
				        WWIV_GetRandomNumber(24),
				        WWIV_GetRandomNumber( 14 ) + 1,
				        "WWIV Screen Saver - Press Any Key For WWIV" );
				wfc_time = timer() - GetSession()->screen_saver_time - 1;
				poll_time = timer();
			}
		}
	}
}


void DisplayWFCScreen( const char *pszScreenBuffer ) {
	GetSession()->localIO()->LocalWriteScreenBuffer( pszScreenBuffer );
}


#endif // __unix__
