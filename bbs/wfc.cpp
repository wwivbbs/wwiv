/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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


#if !defined ( _UNIX )

// Local Functions
void DisplayWFCScreen(const char *pszBuffer);
static char * pszScreenBuffer = NULL;

static int inst_num;


void wfc_cls()
{
	if ( app->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) )
	{
		reset_colors();
		app->localIO->LocalCls();
		sess->wfc_status = 0;
		app->localIO->SetCursor( WLocalIO::cursorNormal );
	}
}


void wfc_init()
{
    for ( int i = 0; i < 5; i++ )
    {
        sess->wfcdrvs[ i ] = 0;
    }

    if ( ini_init( WWIV_INI, INI_TAG, NULL ) )
    {
        char *ss = NULL;
        if ( ( ss = ini_get( "WFC_DRIVES", -1, NULL ) ) != NULL )
        {
            for ( int j = 0; j < wwiv::stringUtils::GetStringLength( ss ); j++ )
            {
                sess->wfcdrvs[ j ] = ss[ j ] - '@';
                if ( sess->wfcdrvs[ j ] < 2 )
                {
                    sess->wfcdrvs[ j ] = 2;
                }
            }
        }
        else
        {
            sess->wfcdrvs[ 0 ] = 2;
        }
    }

	app->localIO->SetCursor( WLocalIO::cursorNormal );            // add 4.31 Build3
	if ( app->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) )
	{
		sess->wfc_status = 0;
		inst_num = 1;
	}
}


void wfc_update()
{
	if ( !app->HasConfigFlag( OP_FLAGS_WFC_SCREEN ) )
	{
		return;
	}

	instancerec ir;
	WUser u;

	get_inst_info( inst_num, &ir );
    app->userManager->ReadUserNoCache( &u, ir.user );
	app->localIO->LocalXYAPrintf( 57, 18, 15, "%-3d", inst_num );
	if ( ir.flags & INST_FLAGS_ONLINE )
	{
        app->localIO->LocalXYAPrintf( 42, 19, 14, "%-25.25s", u.GetUserNameAndNumber( ir.user ) );
	}
	else
	{
		app->localIO->LocalXYAPrintf( 42, 19, 14, "%-25.25s", "Nobody" );
	}

    char szBuffer[ 255 ];
	szBuffer[0] = '\0';
	make_inst_str( inst_num, szBuffer, INST_FORMAT_WFC );
	app->localIO->LocalXYAPrintf( 42, 20, 14, "%-25.25s", szBuffer );
	if ( num_instances() > 1 )
	{
		do
		{
			++inst_num;
			if ( inst_num > num_instances() )
			{
				inst_num = 1;
			}
		} while ( inst_num == app->GetInstanceNumber() );
	}
}


bool iscdrom( char drive )
{
	// TODO Make this function platform specific
	char szDrivePath[10];
	sprintf( szDrivePath, "%c:\\", drive + '@' );

    return ( ( GetDriveType( szDrivePath ) == DRIVE_CDROM ) ? true : false );
}


void wfc_screen()
{
	char szBuffer[ 255 ];
	char szOSVersion[ 255 ];
	instancerec ir;
	WUser u;
	static double wfc_time=0, poll_time=0;

	if (!app->HasConfigFlag( OP_FLAGS_WFC_SCREEN))
	{
		return;
	}

	int nNumNewMessages = check_new_mail(sess->usernum);

	if (sess->wfc_status == 0)
	{
		app->localIO->SetCursor( WLocalIO::cursorNone );
		app->localIO->LocalCls();
		if (pszScreenBuffer == NULL)
		{
			pszScreenBuffer = new char[4000];
            WFile wfcFile( syscfg.datadir, WFC_DAT );
            if ( !wfcFile.Open( WFile::modeBinary | WFile::modeReadOnly ) )
			{
				wfc_cls();
                std::cout << szBuffer << " NOT FOUND." << std::endl;
				app->AbortBBS();
			}
            wfcFile.Read( pszScreenBuffer, 4000 );
		}
		DisplayWFCScreen( pszScreenBuffer );
		sprintf( szBuffer, "Activity and Statistics of %s Node %d", syscfg.systemname, app->GetInstanceNumber() );
		app->localIO->LocalXYAPrintf( 1 + ( ( 76 - strlen( szBuffer ) ) / 2 ), 4, 15, szBuffer );
		app->localIO->LocalXYAPrintf( 8, 1, 14, fulldate() );
		WWIV_GetOSVersion( szOSVersion, 100, true );
		app->localIO->LocalXYAPrintf( 40, 1, 3, "OS: " );
		app->localIO->LocalXYAPrintf( 44, 1, 14, szOSVersion );
		app->localIO->LocalXYAPrintf( 21, 6, 14, "%d", status.callstoday );
		app->localIO->LocalXYAPrintf( 21, 7, 14, "%d", fwaiting );
		if ( nNumNewMessages )
		{
			app->localIO->LocalXYAPrintf( 29, 7 , 3, "New:" );
			app->localIO->LocalXYAPrintf( 34, 7 , 12, "%d", nNumNewMessages );
		}
		app->localIO->LocalXYAPrintf( 21, 8, 14, "%d", status.uptoday );
		app->localIO->LocalXYAPrintf( 21, 9, 14, "%d", status.msgposttoday );
		app->localIO->LocalXYAPrintf( 21, 10, 14, "%d", status.localposts );
		app->localIO->LocalXYAPrintf( 21, 11, 14, "%d", status.emailtoday );
		app->localIO->LocalXYAPrintf( 21, 12, 14, "%d", status.fbacktoday );
		app->localIO->LocalXYAPrintf( 21, 13, 14, "%d Mins (%.1f%%)", status.activetoday,
			100.0 * static_cast<float>( status.activetoday ) / 1440.0 );
		app->localIO->LocalXYAPrintf( 58, 6, 14, "%s%s", wwiv_version, beta_version );

		app->localIO->LocalXYAPrintf( 58, 7, 14, "%d", status.net_version );
		app->localIO->LocalXYAPrintf( 58, 8, 14, "%d", status.users );
		app->localIO->LocalXYAPrintf( 58, 9, 14, "%ld", status.callernum1 );
		if ( status.days )
		{
			app->localIO->LocalXYAPrintf( 58, 10, 14, "%.2f", static_cast<float>( status.callernum1 ) /
					static_cast<float>( status.days ) );
		}
		else
		{
			app->localIO->LocalXYAPrintf( 58, 10, 14, "N/A" );
		}
		app->localIO->LocalXYAPrintf( 58, 11, 14, sysop2() ? "Available    " : "Not Available" );
		if ( ok_modem_stuff )
		{
			app->localIO->LocalXYAPrintf( 58, 12, 14, "%-20.20s", modem_i->name );
			app->localIO->LocalXYAPrintf( 58, 13, 14, "%-20s", "Waiting For Call" );
		}
		else
		{
			app->localIO->LocalXYAPrintf( 58, 12, 14, "Local %code", (syscfgovr.primaryport) ? 'M' : 'N' );
			app->localIO->LocalXYAPrintf( 58, 13, 14, "Waiting For Command" );
		}

		int i = 0, i1 = 0;
		char szCurDir[MAX_PATH];
		strcpy( szCurDir, app->GetHomeDir() );
		while ( sess->wfcdrvs[i] > 0 && sess->wfcdrvs[i] < szCurDir[0] && i1 < 5 )
		{
			if ( iscdrom( static_cast<char>( sess->wfcdrvs[i] ) ) )
			{
				app->localIO->LocalXYAPrintf( 2, 16 + i1, 3, "CDROM %c..", sess->wfcdrvs[i] + '@' );
				app->localIO->LocalXYAPrintf( 12 - 1, 17 + i1 - 1, 14, "%10.1f MB", 0.0f);
				i1++;
			}
			else
			{
				char szTmpPath[4];
				sprintf( szTmpPath, "%c:\\", sess->wfcdrvs[i] + '@' );
				long lFreeDiskSpace = static_cast<long>( freek1( szTmpPath ) );
				char szTempDiskSize[81];
				if ( lFreeDiskSpace > 0 )
				{
					if ( lFreeDiskSpace > 2048 )
					{
						sprintf( szTempDiskSize, "%10.1f GB", static_cast<float>( lFreeDiskSpace ) / ( 1024.0 * 1024.0 ) );
					}
					else
					{
						sprintf( szTempDiskSize, "%10.1f MB", static_cast<float>( lFreeDiskSpace ) / 1024.0 );
					}
					app->localIO->LocalXYAPrintf( 2, 16 + i1, 3, "Drive %c..", sess->wfcdrvs[i] + '@' );
					app->localIO->LocalXYAPrintf( 12 - 1, 17 + i1 - 1, 14, "%s", szTempDiskSize );
					i1++;
				}
			}
			i++;
		}

		get_inst_info( app->GetInstanceNumber(), &ir );
		if ( ir.user < syscfg.maxusers && ir.user > 0 )
		{
            app->userManager->ReadUserNoCache( &u, ir.user );
            app->localIO->LocalXYAPrintf( 33, 16, 14, "%-20.20s", u.GetUserNameAndNumber( ir.user ) );
		}
		else
		{
			app->localIO->LocalXYAPrintf( 33, 16, 14, "%-20.20s", "Nobody" );
		}

		sess->wfc_status = 1;
		wfc_update();
		poll_time = wfc_time = timer();
  }
  else
  {
	  if ( ( timer() - wfc_time < sess->screen_saver_time ) ||
		   ( sess->screen_saver_time == 0 ) )
	  {
		  app->localIO->LocalXYAPrintf( 28, 1, 14, times() );
		  app->localIO->LocalXYAPrintf( 58, 11, 14, sysop2() ? "Available    " : "Not Available" );
		  if ( timer() - poll_time > 10 )
		  {
			  wfc_update();
			  poll_time = timer();
		  }
	  }
	  else
	  {
		  if ( ( timer() - poll_time > 10 ) || sess->wfc_status == 1 )
		  {
			  sess->wfc_status = 2;
			  app->localIO->LocalCls();
			  app->localIO->LocalXYAPrintf(	WWIV_GetRandomNumber(38),
						                    WWIV_GetRandomNumber(24),
						                    static_cast<unsigned char>( WWIV_GetRandomNumber( 14 ) + 1 ),
						                    "WWIV Screen Saver - Press Any Key For WWIV");
			  wfc_time = timer() - sess->screen_saver_time - 1;
			  poll_time = timer();
		  }
	  }
  }
}


void DisplayWFCScreen( const char *pszScreenBuffer )
{
	app->localIO->LocalWriteScreenBuffer( pszScreenBuffer );
}


#endif // _UNIX
