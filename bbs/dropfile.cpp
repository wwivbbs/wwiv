/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include "WTextFile.h"

//
// Local functions
//
int GetDoor32Emulation();
int GetDoor32CommType();
int GetDoor32TimeLeft(double seconds);
void GetNamePartForDropFile(bool lastName, char *pszName);
void create_drop_files();


const std::string create_filename( int nDropFileType )
{
    std::string fileName = syscfgovr.tempdir;
    switch ( nDropFileType )
	{
    case CHAINFILE_CHAIN:
        fileName += "chain.txt";
        break;
    case CHAINFILE_DORINFO:
        fileName += "dorinfo1.def";
        break;
    case CHAINFILE_PCBOARD:
        fileName += "pcboard.sys";
        break;
    case CHAINFILE_CALLINFO:
        fileName += "callinfo.bbs";
        break;
    case CHAINFILE_DOOR:
        fileName += "door.sys";
        break;
    case CHAINFILE_DOOR32:
		fileName += "door32.sys";
		break;
    default:
		// Default to CHAIN.TXT since this is the native WWIV dormat
        fileName += "chain.txt";
        break;
    }
    return std::string( fileName );
}


/**
 * Returns first or last name from string (s) back into s
 */
void GetNamePartForDropFile( bool lastName, char *pszName )
{
    if ( !lastName )
    {
        char *ss = strchr( pszName, ' ' );
        if ( ss )
        {
            pszName[ strlen( pszName ) - strlen( ss ) ] = '\0';
        }
    }
    else
    {
        char *ss = strrchr( pszName, ' ' );
        sprintf( pszName, "%s", ( ss ) ? ++ss : "" );
    }
}

bool GetComSpeedInDropfileFormat( std::string& cspeed, unsigned long lComSpeed  )
{
    if ( lComSpeed == 1 || lComSpeed == 49664 )
	{
        cspeed = "115200";
	}
    else
    {
    	wwiv::stringUtils::FormatString( cspeed, "%u", lComSpeed );
    }
    return true;
}

long GetMinutesRemainingForDropFile()
{
    long lMinutesLeft = (static_cast<long>( nsl() / 60 )) - 1L;
    lMinutesLeft = std::max<long>( lMinutesLeft, 0 );
    return lMinutesLeft;
}


/** make DORINFO1.DEF (RBBS and many others) dropfile */
void CreateDoorInfoDropFile()
{
    std::string fileName = create_filename( CHAINFILE_DORINFO );
    WFile::Remove(fileName);
    WTextFile fileDorInfoSys( fileName, "wt");
    if (fileDorInfoSys.IsOpen())
	{
        fileDorInfoSys.WriteFormatted( "%s\n%s\n\nCOM%d\n", syscfg.systemname, syscfg.sysopname,
            incom ? syscfgovr.primaryport : 0);
        fileDorInfoSys.WriteFormatted( "%lu ", ((GetSession()->using_modem) ? com_speed : 0));
        fileDorInfoSys.WriteFormatted( "BAUD,N,8,1\n0\n");
        if (syscfg.sysconfig & sysconfig_no_alias)
		{
            char szTemp[81];
            strcpy( szTemp, GetSession()->GetCurrentUser()->GetRealName() );
            GetNamePartForDropFile( false, szTemp );
            fileDorInfoSys.WriteFormatted( "%s\n", szTemp );
            strcpy( szTemp, GetSession()->GetCurrentUser()->GetRealName() );
            GetNamePartForDropFile( true, szTemp );
            fileDorInfoSys.WriteFormatted( "%s\n", szTemp );
        }
		else
		{
            fileDorInfoSys.WriteFormatted( "%s\n\n", GetSession()->GetCurrentUser()->GetName() );
		}
        if (syscfg.sysconfig & sysconfig_extended_info)
		{
            fileDorInfoSys.WriteFormatted( "%s, %s\n", GetSession()->GetCurrentUser()->GetCity(), GetSession()->GetCurrentUser()->GetState() );
		}
        else
		{
            fileDorInfoSys.WriteFormatted( "\n");
		}
        fileDorInfoSys.WriteFormatted( "%c\n%d\n%ld\n", GetSession()->GetCurrentUser()->HasAnsi() ? '1' : '0',
                 GetSession()->GetCurrentUser()->GetSl(), GetMinutesRemainingForDropFile() );
        fileDorInfoSys.Close();
    }
}

/** make PCBOARD.SYS (PC Board) drop file */
void CreatePCBoardSysDropFile()
{
    std::string fileName = create_filename( CHAINFILE_PCBOARD );
    WFile pcbFile( fileName );
    pcbFile.Delete();
    if ( pcbFile.Open(  WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                        WFile::shareUnknown, WFile::permReadWrite ) )
	{
		pcboard_sys_rec pcb;
        memset(&pcb, 0, sizeof(pcb));
        strcpy(pcb.display, "-1");
        strcpy(pcb.printer, "0");	// -1 if logging is to the printer, 0 otherwise;
        strcpy(pcb.page_bell, " 0");
        strcpy(pcb.alarm, ( GetSession()->localIO()->GetSysopAlert() ) ? "-1" : " 0");
        strcpy(pcb.errcheck, (modem_flag & flag_ec) ? "-1" : " 0");
        if ( okansi() )
		{
            pcb.graphics = 'Y';
            pcb.ansi = '1';
        }
		else
		{
            pcb.graphics = 'N';
            pcb.ansi = '0';
        }
        pcb.nodechat = 32;
        std::string cspeed;
        GetComSpeedInDropfileFormat(cspeed, com_speed);
        sprintf( pcb.openbps, "%-5.5s", cspeed.c_str() );
        if ( !incom )
		{
            strcpy( pcb.connectbps, "Local" );
		}
        else
		{
            sprintf( pcb.connectbps, "%-5.5u", modem_speed );
		}
        pcb.usernum = static_cast<short>( GetSession()->usernum );
        char szName[ 255 ];
        sprintf( szName, "%-25.25s", GetSession()->GetCurrentUser()->GetName() );
        char *pszFirstName = strtok(szName, " \t");
        sprintf(pcb.firstname, "%-15.15s", pszFirstName);
		// Don't write password  security
        strcpy(pcb.password, "XXX");
        pcb.time_on = static_cast<short>( GetSession()->GetCurrentUser()->GetTimeOn() / 60 );
        pcb.prev_used = 0;
        double d = GetSession()->GetCurrentUser()->GetTimeOn() / 60;
        int h1 = static_cast<int>(d / 60) / 10;
        int h2 = static_cast<int>(d / 60) - (h1 * 10);
        int m1 = static_cast<int>(d - ((h1 * 10 + h2) * 60)) / 10;
        int m2 = static_cast<int>(d - ((h1 * 10 + h2) * 60)) - (m1 * 10);
        pcb.time_logged[0] = static_cast<char>( h1 + '0' );
        pcb.time_logged[1] = static_cast<char>( h2 + '0' );
        pcb.time_logged[2] = ':';
        pcb.time_logged[3] = static_cast<char>( m1 + '0' );
        pcb.time_logged[4] = static_cast<char>( m2 + '0' );
        pcb.time_limit = static_cast<short>( nsl() );
        pcb.down_limit = 1024;
        pcb.curconf = static_cast<char>( GetSession()->GetCurrentConferenceMessageArea() );
        strcpy(pcb.slanguage, cur_lang_name);
        strcpy( pcb.name, GetSession()->GetCurrentUser()->GetName() );
        pcb.sminsleft = pcb.time_limit;
		pcb.snodenum = static_cast<char>( (num_instances() > 1) ? GetApplication()->GetInstanceNumber() : 0 );
        strcpy(pcb.seventtime, "01:00");
        strcpy(pcb.seventactive, (syscfg.executetime && syscfg.executestr[0]) ?
            "-1" : " 0");
        strcpy(pcb.sslide, " 0");
        pcb.scomport = syscfgovr.primaryport + '0';
        pcb.packflag = 27;
        pcb.bpsflag = 32;
        // Added for PCB 14.5 Revision
        std::auto_ptr<WStatus> pStatus(GetApplication()->GetStatusManager()->GetStatus());
        strcpy(pcb.lastevent, pStatus->GetLastDate());
        pcb.exittodos = '0';
        pcb.eventupcoming = '0';
        pcb.lastconfarea = static_cast<short>( GetSession()->GetCurrentConferenceMessageArea() );
        // End Additions

        pcbFile.Write( &pcb, sizeof( pcb ) );
        pcbFile.Close();
    }
}

void CreateCallInfoBbsDropFile()
{
	// make CALLINFO.BBS (WildCat!)
    std::string fileName = create_filename( CHAINFILE_CALLINFO );
    WFile::Remove( fileName );
    WTextFile file( fileName, "wt");
    if (file.IsOpen())
	{
        file.WriteFormatted( "%s\n", GetSession()->GetCurrentUser()->GetRealName() );
        switch (modem_speed)
		{
        case 300:
            file.WriteFormatted("1\n");
        case 1200:
            file.WriteFormatted("2\n");
        case 2400:
            file.WriteFormatted("0\n");
        case 19200:
            file.WriteFormatted("4\n");
        default:
            file.WriteFormatted("3\n");
        }
        file.WriteFormatted(" \n%d\n%ld\n%s\n%s\n%ld\n%ld\n%.5s\n0\nABCD\n0\n0\n0\n0\n",
            GetSession()->GetCurrentUser()->GetSl(), GetMinutesRemainingForDropFile(),
            GetSession()->GetCurrentUser()->HasAnsi() ? "COLOR" : "MONO",
            "X" /* GetSession()->GetCurrentUser()->GetPassword() */ , GetSession()->usernum, static_cast<long>( timeon / 60 ), times());
        file.WriteFormatted("%s\n%s 00:01\nEXPERT\nN\n%s\n%d\n%d\n1\n%d\n%d\n%s\n%s\n%d\n",
                GetSession()->GetCurrentUser()->GetVoicePhoneNumber(),
                GetSession()->GetCurrentUser()->GetLastOn(),
                GetSession()->GetCurrentUser()->GetLastOn(),
                GetSession()->GetCurrentUser()->GetNumLogons(),
                GetSession()->GetCurrentUser()->GetScreenLines(),
                GetSession()->GetCurrentUser()->GetFilesUploaded(),
                GetSession()->GetCurrentUser()->GetFilesDownloaded(),
                "8N1",
                (incom) ? "REMOTE" : "LOCAL",
                (incom) ? 0 : syscfgovr.primaryport );
        char szDate[81], szTemp[81];
        strcpy(szDate, "00/00/00");
        sprintf(szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayMonth() );
        szTemp[2] = '\0';
        memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
        sprintf(szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayDay() );
        szTemp[2] = '\0';
        memmove( &( szDate[ 5 - strlen( szTemp ) ] ), &( szTemp[0] ), strlen( szTemp ) );
        sprintf( szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayYear() );
        szTemp[2] = '\0';
        memmove( &( szDate[ 8 - strlen( szTemp ) ] ), &( szTemp[0] ), strlen( szTemp ) );
        file.WriteFormatted( "%s\n", szDate );
        std::string cspeed;
        GetComSpeedInDropfileFormat(cspeed, com_speed);
        file.WriteFormatted( "%s\n", ( incom ) ? cspeed.c_str() : "14400" );
        file.Close();
    }
}


/** Make DOOR32.SYS drop file */
void CreateDoor32SysDropFile()
{
/* =========================================================================
   File Format: (available at http://www.mysticbbs.com/door32/d32spec1.txt)
   =========================================================================

   0                       Line 1 : Comm type (0=local, 1=serial, 2=telnet)
   0                       Line 2 : Comm or socket handle
   38400                   Line 3 : Baud rate
   Mystic 1.07             Line 4 : BBSID (software name and version)
   1                       Line 5 : User record position (1-based)
   James Coyle             Line 6 : User's real name
   g00r00                  Line 7 : User's handle/alias
   255                     Line 8 : User's security level
   58                      Line 9 : User's time left (in minutes)
   1                       Line 10: Emulation *See Below
   1                       Line 11: Current node number


	* The following are values we've predefined for the emulation:

		0 = Ascii
		1 = Ansi
		2 = Avatar
		3 = RIP
		4 = Max Graphics

   ========================================================================= */
    std::string fileName = create_filename( CHAINFILE_DOOR32 );
    WFile::Remove( fileName );

    std::string cspeed;
    GetComSpeedInDropfileFormat( cspeed, com_speed );
    WTextFile file( fileName, "wt" );
    if (file.IsOpen())
    {
		file.WriteFormatted( "%d\n",		    GetDoor32CommType() );
		file.WriteFormatted( "%u\n",            GetSession()->remoteIO()->GetDoorHandle() );
		file.WriteFormatted( "%s\n",		    cspeed.c_str() );
		file.WriteFormatted( "WWIV %s\n",       wwiv_version );
		file.WriteFormatted( "999999\n");       // we don't want to share this
		file.WriteFormatted( "%s\n",	        GetSession()->GetCurrentUser()->GetRealName() );
		file.WriteFormatted( "%s\n",		    GetSession()->GetCurrentUser()->GetName() );
		file.WriteFormatted( "%d\n",		    GetSession()->GetCurrentUser()->GetSl() );
		file.WriteFormatted( "%d\n",		    GetDoor32TimeLeft( nsl() ) );
		file.WriteFormatted( "%d\n",		    GetDoor32Emulation() );
		file.WriteFormatted( "%u\n",		    GetApplication()->GetInstanceNumber() );
        file.Close();
    }
}


/** Create generic DOOR.SYS dropfile */
void CreateDoorSysDropFile()
{
    std::string fileName = create_filename( CHAINFILE_DOOR );
    WFile::Remove( fileName );

    WTextFile file( fileName, "wt" );
    if (file.IsOpen())
	{
        std::string cspeed;
        GetComSpeedInDropfileFormat(cspeed, com_speed);
        char szLine[255];
        sprintf(szLine, "COM%d\n%s\n%c\n%u\n%u\n%c\n%c\n%c\n%c\n%s\n%s, %s\n",
            (GetSession()->using_modem) ? syscfgovr.primaryport : 0,
			cspeed.c_str(),
            '8',
            GetApplication()->GetInstanceNumber(),                       // node
            (GetSession()->using_modem) ? modem_speed : 14400,
            'Y',                            // screen display
            'N',							// log to printer
            'N',                            // page bell
            'N',                            // caller alarm
            GetSession()->GetCurrentUser()->GetRealName(),
            GetSession()->GetCurrentUser()->GetCity(),
            GetSession()->GetCurrentUser()->GetState() );
        file.WriteFormatted( szLine );
        sprintf(szLine, "%s\n%s\n%s\n%d\n%u\n%s\n%ld\n%ld\n",
            GetSession()->GetCurrentUser()->GetVoicePhoneNumber(),
            GetSession()->GetCurrentUser()->GetDataPhoneNumber(),
            "X",                            // GetSession()->GetCurrentUser()->GetPassword()
            GetSession()->GetCurrentUser()->GetSl(),
            GetSession()->GetCurrentUser()->GetNumLogons(),
            GetSession()->GetCurrentUser()->GetLastOn(),
            static_cast<unsigned long>( 60L * GetMinutesRemainingForDropFile() ),
            GetMinutesRemainingForDropFile());
        file.WriteFormatted( szLine );
        std::string ansiStatus = ( okansi() ) ? "GR" : "NG";
        sprintf(szLine, "%s\n%u\n%c\n%s\n%lu\n%s\n%lu\n%c\n%u\n%u\n%u\n%u\n",
                ansiStatus.c_str(),
                GetSession()->GetCurrentUser()->GetScreenLines(),
                GetSession()->GetCurrentUser()->IsExpert() ? 'Y' : 'N',
                "1,2,3",                        // conferences
                GetSession()->GetCurrentMessageArea(),  // current 'conference'
                "12/31/99",                     // expiration date
                GetSession()->usernum,
                'Y',                            // default protocol
                GetSession()->GetCurrentUser()->GetFilesUploaded(),
                GetSession()->GetCurrentUser()->GetFilesDownloaded(),
                0,                              // kb dl today
                0 );                            // kb dl/day max
        file.WriteFormatted( szLine );
        char szDate[21], szTemp[81];
        strcpy(szDate, "00/00/00");
        sprintf( szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayMonth() );
        szTemp[2] = '\0';
        memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
        sprintf(szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayDay() );
        szTemp[2] = '\0';
        memmove(&(szDate[5 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
        sprintf(szTemp, "%d", GetSession()->GetCurrentUser()->GetBirthdayYear() );
        szTemp[2] = '\0';
        memmove(&(szDate[8 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
        szDate[9] = '\0';
        sprintf(szLine, "%s\n%s\n%s\n%s\n%s\n%s\n%c\n%c\n%c\n%u\n%u\n%s\n%-.5s\n%s\n",
            szDate,
            syscfg.datadir,
            syscfg.gfilesdir,
            syscfg.sysopname,
            GetSession()->GetCurrentUser()->GetName(),
            "00:01",                        // event time
            (modem_flag & flag_ec) ? 'Y' : 'N',
            ( okansi() ) ? 'N' : 'Y',         // ansi ok but graphics turned off
            'N',                            // record-locking
            GetSession()->GetCurrentUser()->GetColor( 0 ),
            GetSession()->GetCurrentUser()->GetTimeBankMinutes(),
            GetSession()->GetCurrentUser()->GetLastOn(),                // last n-scan date
            times(),
            "00:01");                       // time last call
        file.WriteFormatted( szLine );
        sprintf(szLine, "%u\n%u\n%ld\n%ld\n%s\n%u\n%d\n",
            99,                             // max files dl/day
            0,                              // files dl today so far
            GetSession()->GetCurrentUser()->GetUploadK(),
            GetSession()->GetCurrentUser()->GetDownloadK(),
            GetSession()->GetCurrentUser()->GetNote(),
            GetSession()->GetCurrentUser()->GetNumChainsRun(),
            GetSession()->GetCurrentUser()->GetNumMessagesPosted() );
        file.WriteFormatted( szLine );
        file.Close();
    }
}



void create_drop_files()
{
    CreateDoorInfoDropFile();
    CreatePCBoardSysDropFile();
    CreateCallInfoBbsDropFile();
    CreateDoor32SysDropFile();
}


const std::string create_chain_file()
{
	std::string cspeed;

    unsigned char nSaveComPortNum = syscfgovr.primaryport;
    if ( syscfgovr.primaryport == 0 && ok_modem_stuff )
    {
        // This is so that we'll use COM1 in DOORS even though our comport is set
        // to 0 in init.  It's not perfect, but it'll make sure doors work more
        // often than not.
        syscfgovr.primaryport = 1;
    }


    if ( com_speed == 1 || com_speed == 49664 )
	{
        cspeed = "115200";
	}
	else
	{
		wwiv::stringUtils::FormatString( cspeed, "%ul", com_speed );
	}

    create_drop_files();
    long l = static_cast<long>( timeon );
    if ( l < 0 )
	{
		l += SECONDS_PER_HOUR * HOURS_PER_DAY;
	}
    long l1 = static_cast<long>( timer() - timeon );
    if ( l1 < 0 )
	{
        l1 += SECONDS_PER_HOUR * HOURS_PER_DAY;
	}

    std::string fileName = create_filename( CHAINFILE_CHAIN );

    WFile::Remove( fileName );
    WTextFile file( fileName, "wt" );
    if (file.IsOpen())
	{
        file.WriteFormatted("%ld\n%s\n%s\n%s\n%d\n%c\n%10.2f\n%s\n%d\n%d\n%u\n",
				GetSession()->usernum,
                GetSession()->GetCurrentUser()->GetName(),
                GetSession()->GetCurrentUser()->GetRealName(),
                GetSession()->GetCurrentUser()->GetCallsign(),
				GetSession()->GetCurrentUser()->GetAge(),
                GetSession()->GetCurrentUser()->GetGender(),
                GetSession()->GetCurrentUser()->GetGold(),
                GetSession()->GetCurrentUser()->GetLastOn(),
				GetSession()->GetCurrentUser()->GetScreenChars(),
                GetSession()->GetCurrentUser()->GetScreenLines(),
                GetSession()->GetCurrentUser()->GetSl() );
        char szTemporaryLogFileName[ MAX_PATH ];
        GetTemporaryInstanceLogFileName( szTemporaryLogFileName );
        file.WriteFormatted( "%d\n%d\n%d\n%u\n%10.2f\n%s\n%s\n%s\n",
				cs(), so(), okansi(), incom, nsl(), syscfg.gfilesdir, syscfg.datadir, szTemporaryLogFileName );
        if (GetSession()->using_modem)
		{
            file.WriteFormatted( "%u\n", modem_speed);
		}
        else
		{
            file.WriteFormatted( "KB\n");
		}
        file.WriteFormatted( "%d\n%s\n%s\n%ld\n%ld\n%lu\n%u\n%lu\n%u\n%s\n%s\n%u\n",
				syscfgovr.primaryport,
                syscfg.systemname,
                syscfg.sysopname,
                l,
                l1,
				GetSession()->GetCurrentUser()->GetUploadK(),
                GetSession()->GetCurrentUser()->GetFilesUploaded(),
                GetSession()->GetCurrentUser()->GetDownloadK(),
                GetSession()->GetCurrentUser()->GetFilesDownloaded(),
				"8N1",
				cspeed.c_str(),
                net_sysnum );
        file.WriteFormatted("N\nN\nN\n");
        file.WriteFormatted( "%u\n%u\n", GetSession()->GetCurrentUser()->GetAr(), GetSession()->GetCurrentUser()->GetDar() );
        file.Close();
    }
    syscfgovr.primaryport = nSaveComPortNum;

    return std::string( fileName );
}


int GetDoor32CommType()
{
	if (!GetSession()->using_modem)
	{
		return 0;
	}
#ifdef _WIN32
	return (GetSession()->remoteIO()->GetHandle() == NULL) ? 1 : 2;
#else
	return 0;
#endif
}


int GetDoor32Emulation()
{
	return ( okansi() ) ? 1 : 0;
}


int GetDoor32TimeLeft(double seconds)
{
	if ( seconds <= 0 )
	{
		return 0;
	}

	int minLeft = static_cast<int>( seconds / 60 );

	return minLeft;

}


