/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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


//
// Local functions
//
unsigned long GetSockOrCommHandle();
int GetDoor32Emulation();
int GetDoor32CommType();
int GetDoor32TimeLeft(double seconds);
void GetNamePartForDropFile(bool lastName, char *s);
void create_drop_files();


void create_filename( int nDropFileType, char *pszOutputFileName )
{
    switch ( nDropFileType )
	{
    case CHAINFILE_CHAIN:
        sprintf( pszOutputFileName, "%schain.txt", syscfgovr.tempdir );
        break;
    case CHAINFILE_DORINFO:
        sprintf( pszOutputFileName, "%sdorinfo1.def", syscfgovr.tempdir );
        break;
    case CHAINFILE_PCBOARD:
        sprintf( pszOutputFileName, "%spcboard.sys", syscfgovr.tempdir );
        break;
    case CHAINFILE_CALLINFO:
        sprintf( pszOutputFileName, "%scallinfo.bbs", syscfgovr.tempdir );
        break;
    case CHAINFILE_DOOR:
        sprintf( pszOutputFileName, "%sdoor.sys", syscfgovr.tempdir );
        break;
    case CHAINFILE_DOOR32:
		sprintf( pszOutputFileName, "%sdoor32.sys", syscfgovr.tempdir );
		break;
    default:
		// Default to CHAIN.TXT since this is the native WWIV dormat
        sprintf( pszOutputFileName, "%schain.txt", syscfgovr.tempdir );
        break;
    }
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



void create_drop_files()
{
    char s[150], s1[255], s2[81], s3[150], *ss;

	std::string cspeed;
	wwiv::stringUtils::FormatString( cspeed, "%ul", com_speed );
    if ( com_speed == 1 || com_speed == 49664 )
	{
        cspeed = "115200";
	}


    // minutes left
    long l = static_cast<long>( nsl() / 60 );
    l -= 1L;
    if (l < 0L)
	{
        l = 0L;
	}

    // make DORINFO1.DEF (RBBS and many others)
    create_filename(CHAINFILE_DORINFO, s);
    WFile::Remove(s);
    FILE * pFile = fsh_open(s, "wt");
    if (pFile)
	{
        fprintf(pFile, "%s\n%s\n\nCOM%d\n", syscfg.systemname, syscfg.sysopname,
            incom ? syscfgovr.primaryport : 0);
        fprintf(pFile, "%lu ", ((GetSession()->using_modem) ? com_speed : 0));
        fprintf(pFile, "BAUD,N,8,1\n0\n");
        if (syscfg.sysconfig & sysconfig_no_alias)
		{
            strcpy( s, GetSession()->thisuser.GetRealName() );
            GetNamePartForDropFile( false, s );
            fprintf( pFile, "%s\n", s );
            strcpy( s, GetSession()->thisuser.GetRealName() );
            GetNamePartForDropFile( true, s );
            fprintf( pFile, "%s\n", s );
        }
		else
		{
            fprintf( pFile, "%s\n\n", GetSession()->thisuser.GetName() );
		}
        if (syscfg.sysconfig & sysconfig_extended_info)
		{
            fprintf( pFile, "%s, %s\n", GetSession()->thisuser.GetCity(), GetSession()->thisuser.GetState() );
		}
        else
		{
            fprintf(pFile, "\n");
		}
        fprintf( pFile, "%c\n%d\n%ld\n", GetSession()->thisuser.hasAnsi() ? '1' : '0',
                 GetSession()->thisuser.GetSl(), l );
        fsh_close( pFile );
    }



	// make PCBOARD.SYS (PC Board)
    create_filename(CHAINFILE_PCBOARD, s);
    WFile pcbFile( s );
    pcbFile.Delete();
    if ( pcbFile.Open(  WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                        WFile::shareUnknown, WFile::permReadWrite ) )
	{
		pcboard_sys_rec pcb;
        memset(&pcb, 0, sizeof(pcb));
        strcpy(pcb.display, "-1");
        strcpy(pcb.printer, "0");	// -1 if logging is to the printer, 0 otherwise;
        strcpy(pcb.page_bell, " 0");
        strcpy(pcb.alarm, ( GetApplication()->GetLocalIO()->GetSysopAlert() ) ? "-1" : " 0");
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
        sprintf( s, "%-25.25s", GetSession()->thisuser.GetName() );
        ss = strtok(s, " \t");
        sprintf(pcb.firstname, "%-15.15s", ss);
		// Don't write password  security
        strcpy(pcb.password, "XXX");
        pcb.time_on = static_cast<short>( GetSession()->thisuser.GetTimeOn() / 60 );
        pcb.prev_used = 0;
        double d = GetSession()->thisuser.GetTimeOn() / 60;
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
        strcpy( pcb.name, GetSession()->thisuser.GetName() );
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
        strcpy(pcb.lastevent, status.date1);
        pcb.exittodos = '0';
        pcb.eventupcoming = '0';
        pcb.lastconfarea = static_cast<short>( GetSession()->GetCurrentConferenceMessageArea() );
        // End Additions

        pcbFile.Write( &pcb, sizeof( pcb ) );
        pcbFile.Close();
    }


	// make CALLINFO.BBS (WildCat!)
    create_filename(CHAINFILE_CALLINFO, s);
    WFile::Remove(s);
    pFile = fsh_open(s, "wt");
    if (pFile)
	{
        fprintf( pFile, "%s\n", GetSession()->thisuser.GetRealName() );
        switch (modem_speed)
		{
        case 300:
            fprintf(pFile, "1\n");
        case 1200:
            fprintf(pFile, "2\n");
        case 2400:
            fprintf(pFile, "0\n");
        case 19200:
            fprintf(pFile, "4\n");
        default:
            fprintf(pFile, "3\n");
        }
        fprintf(pFile, " \n%d\n%ld\n%s\n%s\n%ld\n%ld\n%.5s\n0\nABCD\n0\n0\n0\n0\n",
            GetSession()->thisuser.GetSl(), l,
            GetSession()->thisuser.hasAnsi() ? "COLOR" : "MONO",
            "X" /* GetSession()->thisuser.GetPassword() */ , GetSession()->usernum, static_cast<long>( timeon / 60 ), times());
        fprintf(pFile, "%s\n%s 00:01\nEXPERT\nN\n%s\n%d\n%d\n1\n%d\n%d\n%s\n%s\n%d\n",
                GetSession()->thisuser.GetVoicePhoneNumber(),
                GetSession()->thisuser.GetLastOn(),
                GetSession()->thisuser.GetLastOn(),
                GetSession()->thisuser.GetNumLogons(),
                GetSession()->thisuser.GetScreenLines(),
                GetSession()->thisuser.GetFilesUploaded(),
                GetSession()->thisuser.GetFilesDownloaded(),
                "8N1",
                (incom) ? "REMOTE" : "LOCAL",
                (incom) ? 0 : syscfgovr.primaryport );
        strcpy(s1, "00/00/00");
        sprintf(s2, "%d", GetSession()->thisuser.GetBirthdayMonth() );
        s2[2] = '\0';
        memmove(&(s1[2 - strlen(s2)]), &(s2[0]), strlen(s2));
        sprintf(s2, "%d", GetSession()->thisuser.GetBirthdayDay() );
        s2[2] = '\0';
        memmove( &( s1[ 5 - strlen( s2 ) ] ), &( s2[0] ), strlen( s2 ) );
        sprintf( s2, "%d", GetSession()->thisuser.GetBirthdayYear() );
        s2[2] = '\0';
        memmove( &( s1[ 8 - strlen( s2 ) ] ), &( s2[0] ), strlen( s2 ) );
        fprintf( pFile, "%s\n", s1 );
		fprintf( pFile, "%s\n", ( incom ) ? cspeed.c_str() : "14400" );
        fsh_close( pFile );
    }

    // Make DOOR32.SYS

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


    create_filename(CHAINFILE_DOOR32, s);
    WFile::Remove(s);
    pFile = fsh_open(s, "wt");
    if (pFile)
    {
		fprintf( pFile, "%d\n",		    GetDoor32CommType() );
		fprintf( pFile, "%lu\n",        GetSockOrCommHandle() );
		fprintf( pFile, "%s\n",		    cspeed.c_str() );
		fprintf( pFile, "WWIV %s\n",    wwiv_version );
		fprintf( pFile, "999999\n");    // we don't want to share this
		fprintf( pFile, "%s\n",	        GetSession()->thisuser.GetRealName() );
		fprintf( pFile, "%s\n",		    GetSession()->thisuser.GetName() );
		fprintf( pFile, "%d\n",		    GetSession()->thisuser.GetSl() );
		fprintf( pFile, "%d\n",		    GetDoor32TimeLeft( nsl() ) );
		fprintf( pFile, "%d\n",		    GetDoor32Emulation() );
		fprintf( pFile, "%u\n",		    GetApplication()->GetInstanceNumber() );
		fsh_close( pFile );
    }


    // make DOOR.SYS (Generic)
    create_filename(CHAINFILE_DOOR, s);
    WFile::Remove(s);
    pFile = fsh_open(s, "wt");
    if (pFile)
	{
        sprintf(s3, "COM%d\n%s\n%c\n%u\n%u\n%c\n%c\n%c\n%c\n%s\n%s, %s\n",
            (GetSession()->using_modem) ? syscfgovr.primaryport : 0,
			cspeed.c_str(),
            '8',
            GetApplication()->GetInstanceNumber(),                       // node
            (GetSession()->using_modem) ? modem_speed : 14400,
            'Y',                            // screen display
            'N',							// log to printer
            'N',                            // page bell
            'N',                            // caller alarm
            GetSession()->thisuser.GetRealName(),
            GetSession()->thisuser.GetCity(),
            GetSession()->thisuser.GetState() );
        fprintf(pFile, s3);
        sprintf(s3, "%s\n%s\n%s\n%d\n%u\n%s\n%ld\n%ld\n",
            GetSession()->thisuser.GetVoicePhoneNumber(),
            GetSession()->thisuser.GetDataPhoneNumber(),
            "X",                            // GetSession()->thisuser.GetPassword()
            GetSession()->thisuser.GetSl(),
            GetSession()->thisuser.GetNumLogons(),
            GetSession()->thisuser.GetLastOn(),
            static_cast<unsigned long>( 60L * l ),
            l);
        fprintf(pFile, s3);
        sprintf(s1, "%s", okansi() ? "GR" : "NG");
        sprintf(s3, "%s\n%u\n%c\n%s\n%lu\n%s\n%lu\n%c\n%u\n%u\n%u\n%u\n",
                s1,
                GetSession()->thisuser.GetScreenLines(),
                GetSession()->thisuser.isExpert() ? 'Y' : 'N',
                "1,2,3",                        // conferences
                GetSession()->GetCurrentMessageArea(),  // current 'conference'
                "12/31/99",                     // expiration date
                GetSession()->usernum,
                'Y',                            // default protocol
                GetSession()->thisuser.GetFilesUploaded(),
                GetSession()->thisuser.GetFilesDownloaded(),
                0,                              // kb dl today
                0 );                            // kb dl/day max
        fprintf(pFile, s3);
        strcpy(s1, "00/00/00");
        sprintf( s2, "%d", GetSession()->thisuser.GetBirthdayMonth() );
        s2[2] = '\0';
        memmove(&(s1[2 - strlen(s2)]), &(s2[0]), strlen(s2));
        sprintf(s2, "%d", GetSession()->thisuser.GetBirthdayDay() );
        s2[2] = '\0';
        memmove(&(s1[5 - strlen(s2)]), &(s2[0]), strlen(s2));
        sprintf(s2, "%d", GetSession()->thisuser.GetBirthdayYear() );
        s2[2] = '\0';
        memmove(&(s1[8 - strlen(s2)]), &(s2[0]), strlen(s2));
        s1[9] = '\0';
        sprintf(s3, "%s\n%s\n%s\n%s\n%s\n%s\n%c\n%c\n%c\n%u\n%u\n%s\n%-.5s\n%s\n",
            s1,
            syscfg.datadir,
            syscfg.gfilesdir,
            syscfg.sysopname,
            GetSession()->thisuser.GetName(),
            "00:01",                        // event time
            (modem_flag & flag_ec) ? 'Y' : 'N',
            ( okansi() ) ? 'N' : 'Y',         // ansi ok but graphics turned off
            'N',                            // record-locking
            GetSession()->thisuser.GetColor( 0 ),
            GetSession()->thisuser.GetTimeBankMinutes(),
            GetSession()->thisuser.GetLastOn(),                // last n-scan date
            times(),
            "00:01");                       // time last call
        fprintf(pFile, s3);
        sprintf(s3, "%u\n%u\n%ld\n%ld\n%s\n%u\n%d\n",
            99,                             // max files dl/day
            0,                              // files dl today so far
            GetSession()->thisuser.GetUploadK(),
            GetSession()->thisuser.GetDownloadK(),
            GetSession()->thisuser.GetNote(),
            GetSession()->thisuser.GetNumChainsRun(),
            GetSession()->thisuser.GetNumMessagesPosted() );
        fprintf(pFile, s3);
        fsh_close(pFile);
    }
}


char *create_chain_file()
{
    char s[MAX_PATH];
	std::string cspeed;
    static char fpn[MAX_PATH];

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
    islname( s );
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

    create_filename( CHAINFILE_CHAIN, fpn );

    WFile::Remove( fpn );
    FILE* pFile = fsh_open(fpn, "wt");
    if (pFile)
	{
        fprintf(pFile,
                "%ld\n%s\n%s\n%s\n%d\n%c\n%10.2f\n%s\n%d\n%d\n%u\n",
				GetSession()->usernum,
                GetSession()->thisuser.GetName(),
                GetSession()->thisuser.GetRealName(),
                GetSession()->thisuser.GetCallsign(),
				GetSession()->thisuser.GetAge(),
                GetSession()->thisuser.GetGender(),
                GetSession()->thisuser.GetGold(),
                GetSession()->thisuser.GetLastOn(),
				GetSession()->thisuser.GetScreenChars(),
                GetSession()->thisuser.GetScreenLines(),
                GetSession()->thisuser.GetSl() );
        fprintf( pFile, "%d\n%d\n%d\n%u\n%10.2f\n%s\n%s\n%s\n",
				cs(), so(), okansi(), incom, nsl(), syscfg.gfilesdir, syscfg.datadir, s );
        if (GetSession()->using_modem)
		{
            fprintf(pFile, "%u\n", modem_speed);
		}
        else
		{
            fprintf(pFile, "KB\n");
		}
        fprintf(pFile, "%d\n%s\n%s\n%ld\n%ld\n%lu\n%u\n%lu\n%u\n%s\n%s\n%u\n",
				syscfgovr.primaryport,
                syscfg.systemname,
                syscfg.sysopname,
                l,
                l1,
				GetSession()->thisuser.GetUploadK(),
                GetSession()->thisuser.GetFilesUploaded(),
                GetSession()->thisuser.GetDownloadK(),
                GetSession()->thisuser.GetFilesDownloaded(),
				"8N1",
				cspeed.c_str(),
                net_sysnum );
        fprintf(pFile, "N\nN\nN\n");
        fprintf( pFile, "%u\n%u\n", GetSession()->thisuser.GetAr(), GetSession()->thisuser.GetDar() );
        fsh_close( pFile );
    }
    syscfgovr.primaryport = nSaveComPortNum;

    return fpn;
}


unsigned long GetSockOrCommHandle()
{
#ifdef _WIN32
	if (GetSession()->hSocket == NULL)
	{
		return reinterpret_cast<unsigned long>( GetSession()->hCommHandle );
	}
	return static_cast<unsigned long>( GetSession()->hDuplicateSocket );
#else
	return 0L;
#endif
}


int GetDoor32CommType()
{
	if (!GetSession()->using_modem)
	{
		return 0;
	}
#ifdef _WIN32
	return (GetSession()->hSocket == NULL) ? 1 : 2;
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


