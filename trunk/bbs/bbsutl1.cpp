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
#include "WStringUtils.h"


/**
 * Returns true if local sysop functions accessible, else returns false.
 */
bool AllowLocalSysop()
{
    return (syscfg.sysconfig & sysconfig_no_local) ? false : true;
}


/**
 * Finds GetSession()->usernum and system number from pszEmailAddress, sets network number as
 * appropriate.
 * @param pszEmailAddress The text of the email address.
 * @param pUserNumber OUT The User Number
 * @param pSystemmNumber OUT The System Number
 */
void parse_email_info(const char *pszEmailAddress, int *pUserNumber, int *pSystemNumber )
{
	char *ss1, onx[20], ch, *mmk;
	unsigned nUserNumber, nSystemNumber;
	int i, nv, on, xx, onxi, odci;
	net_system_list_rec *csne;
	WWIV_ASSERT( pszEmailAddress );

	char szEmailAddress[ 255 ];
	strcpy( szEmailAddress, pszEmailAddress );

	*pUserNumber = 0;
	*pSystemNumber = 0;
	net_email_name[0] = 0;
	char *ss = strrchr(szEmailAddress, '@');
	if (ss == NULL)
	{
		nUserNumber = finduser1(szEmailAddress);
		if (nUserNumber > 0)
		{
			*pUserNumber = static_cast< unsigned short >( nUserNumber );
		}
        else if ( wwiv::stringUtils::IsEquals( szEmailAddress, "SYSOP" ) )   // Add 4.31 Build3
        {
            *pUserNumber = 1;
        }
		else
		{
			GetSession()->bout << "Unknown user.\r\n";
		}
	}
    else if (atoi(ss + 1) == 0)
	{
		for (i = 0; i < GetSession()->GetMaxNetworkNumber(); i++)
		{
			set_net_num(i);
			if ((strnicmp("internet", GetSession()->GetNetworkName(), 8) == 0) ||
				((strnicmp("filenet", GetSession()->GetNetworkName(), 7) == 0) && (*pSystemNumber == 32767)))
			{
				strcpy(net_email_name, szEmailAddress);
				for (ss1 = net_email_name; *ss1; ss1++)
				{
					if ((*ss1 >= 'A') && (*ss1 <= 'Z'))
					{
						*ss1 += 'a' - 'A';
					}
				}
				*pSystemNumber = 1;
				break;
			}
		}
		if ( i >= GetSession()->GetMaxNetworkNumber() )
		{
			GetSession()->bout << "Unknown user.\r\n";
		}
	}
	else
	{
		ss[0] = 0;
		ss = &(ss[1]);
		i = strlen(szEmailAddress);
		while ((i > 0) && (szEmailAddress[i - 1] == ' '))
		{
			--i;
		}
		szEmailAddress[i] = 0;
		nUserNumber = atoi(szEmailAddress);
		if ((nUserNumber == 0) && (szEmailAddress[0] == '#'))
		{
			nUserNumber = atoi(szEmailAddress + 1);
		}
		if (strchr(szEmailAddress, '@'))
		{
			nUserNumber = 0;
		}
		nSystemNumber = atoi(ss);
		ss1 = strchr(ss, '.');
		if (ss1)
		{
			ss1++;
		}
		if (nUserNumber == 0)
		{
			strcpy(net_email_name, szEmailAddress);
			i = strlen(net_email_name);
			while ((i > 0) && (net_email_name[i - 1] == ' '))
			{
				--i;
			}
			net_email_name[i] = 0;
			if (net_email_name[0])
			{
				*pSystemNumber = static_cast< unsigned short >( nSystemNumber );
			}
			else
			{
				GetSession()->bout << "Unknown user.\r\n";
			}
		}
		else
		{
			*pUserNumber = static_cast< unsigned short >( nUserNumber );
			*pSystemNumber = static_cast< unsigned short >( nSystemNumber );
		}
		if (*pSystemNumber && ss1)
		{
			for (i = 0; i < GetSession()->GetMaxNetworkNumber(); i++)
			{
				set_net_num(i);
				if ( wwiv::stringUtils::IsEqualsIgnoreCase( ss1, GetSession()->GetNetworkName() ) )
				{
					if (!valid_system(*pSystemNumber))
					{
						nl();
						GetSession()->bout << "There is no " << ss1 << " @" << *pSystemNumber << ".\r\n\n";
						*pSystemNumber = *pUserNumber = 0;
					}
					else
					{
						if (*pSystemNumber == net_sysnum)
						{
							*pSystemNumber = 0;
							if (*pUserNumber == 0)
							{
								*pUserNumber = static_cast< unsigned short >( finduser( net_email_name ) );
							}
							if ( *pUserNumber == 0 || *pUserNumber > 32767 )
							{
								*pUserNumber = 0;
								GetSession()->bout << "Unknown user.\r\n";
							}
						}
					}
					break;
				}
			}
			if ( i >= GetSession()->GetMaxNetworkNumber() )
			{
				nl();
				GetSession()->bout << "This system isn't connected to "<< ss1 << "\r\n";
				*pSystemNumber = *pUserNumber = 0;
			}
		}
		else if ( *pSystemNumber && GetSession()->GetMaxNetworkNumber() > 1 )
		{
			odc[0] = 0;
			odci = 0;
			onx[0] = 'Q';
			onx[1] = 0;
			onxi = 1;
			nv = 0;
			on = GetSession()->GetNetworkNumber();
			ss = static_cast<char *>( BbsAllocA( GetSession()->GetMaxNetworkNumber() ) );
			WWIV_ASSERT(ss != NULL);
			xx = -1;
			for ( i = 0; i < GetSession()->GetMaxNetworkNumber(); i++ )
			{
				set_net_num( i );
				if ( net_sysnum == *pSystemNumber )
				{
					xx = i;
				}
				else if ( valid_system( *pSystemNumber ) )
				{
					ss[nv++] = static_cast< char >( i );
				}
			}
			set_net_num(on);
			if (nv == 0)
			{
				if (xx != -1)
				{
					set_net_num(xx);
					*pSystemNumber = 0;
					if (*pUserNumber == 0)
					{
						*pUserNumber = static_cast< unsigned short >( finduser( net_email_name ) );
						if ( *pUserNumber == 0 || *pUserNumber > 32767 )
						{
							*pUserNumber = 0;
							GetSession()->bout << "Unknown user.\r\n";
						}
					}
				}
				else
				{
					nl();
					GetSession()->bout << "Unknown system\r\n";
					*pSystemNumber = *pUserNumber = 0;
				}
			}
			else if (nv == 1)
			{
				set_net_num(ss[0]);
			}
			else
			{
				nl();
				for (i = 0; i < nv; i++)
				{
					set_net_num(ss[i]);
					csne = next_system(*pSystemNumber);
					if (csne)
					{
						if (i < 9)
						{
							onx[onxi++] = static_cast< char >( i + '1' );
							onx[onxi] = 0;
						}
						else
						{
							odci = static_cast< char >( (i + 1) / 10 );
							odc[odci - 1] = static_cast< char >( odci + '0' );
							odc[odci] = 0;
						}
						GetSession()->bout << i + 1 << ". " << GetSession()->GetNetworkName() << " (" << csne->name << ")\r\n";
					}
				}
				GetSession()->bout << "Q. Quit\r\n\n";
				GetSession()->bout << "|#2Which network (number): ";
				if (nv < 9) {
					ch = onek(onx);
					if (ch == 'Q')
					{
						i = -1;
					}
					else
					{
						i = ch - '1';
					}
				}
				else
				{
					mmk = mmkey( 2 );
					if (*mmk == 'Q')
					{
						i = -1;
					}
					else
					{
						i = atoi(mmk) - 1;
					}
				}
				if ((i >= 0) && (i < nv))
				{
					set_net_num(ss[i]);
				}
				else
				{
					GetSession()->bout << "\r\n|12Aborted.\r\n\n";
					*pUserNumber = *pSystemNumber = 0;
				}
			}
			BbsFreeMemory(ss);
    }
	else
	{
		if (*pSystemNumber == net_sysnum)
		{
			*pSystemNumber = 0;
			if (*pUserNumber == 0)
			{
				*pUserNumber = static_cast< unsigned short >( finduser( net_email_name ) );
			}
			if ( *pUserNumber == 0 || *pUserNumber > 32767 )
			{
				*pUserNumber = 0;
				GetSession()->bout << "Unknown user.\r\n";
			}
		}
		else if (!valid_system(*pSystemNumber))
		{
			GetSession()->bout << "\r\nUnknown user.\r\n";
			*pSystemNumber = *pUserNumber = 0;
        }
    }
  }
}


/**
 * Queries user and verifies system password.
 * @return true if the password entered is valid.
 */
bool ValidateSysopPassword()
{
	nl();
	if ( so() )
	{
		if ( incom )
		{
            std::string password;
			input_password( "|#7SY: ", password, 20 );
            if ( password == syscfg.systempw )
			{
				return true;
			}
		}
		else if ( AllowLocalSysop() )
		{
			return true;
		}
	}
	return false;
}



/**
 * Hangs up the modem if user online. Whether using modem or not, sets
 * hangup to 1.
 */
void hang_it_up()
{
	hangup = true;
#ifndef _UNIX

	if (!ok_modem_stuff)
	{
		return;
	}

	GetApplication()->GetComm()->dtr( false );
	if (!GetApplication()->GetComm()->carrier())
    {
        return;
    }

	wait1( 9 );
	if (!GetApplication()->GetComm()->carrier())
    {
        return;
    }

	wait1( 9 );
	if (!GetApplication()->GetComm()->carrier())
    {
        return;
    }
    int i = 0;
	GetApplication()->GetComm()->dtr( true );
	while ( i++ < 2 && GetApplication()->GetComm()->carrier() )
	{
		wait1( 27 );
		rputs("\x1\x1\x1");
		wait1( 54 );
        rputs( (modem_i->hang[0]) ? modem_i->hang : "ATH\r" );
		wait1( 6 );
	}
	GetApplication()->GetComm()->dtr( true );
#endif
}

/**
 * Plays a sound definition file (*.sdf) through PC speaker. SDF files
 * should reside in the gfiles dir. The only params passed to function are
 * filename and false if playback is unabortable, true if it is abortable. If no
 * extension then .SDF is appended. A full path to file may be specified to
 * override gfiles dir. Format of file is:
 *
 * <freq> <duration in ms> [pause_delay in ms]
 * 1000 1000 50
 *
 * Returns 1 if sucessful, else returns 0. The pause_delay is optional and
 * is used to insert silences between tones.
 */
bool play_sdf( const char *pszSoundFileName, bool abortable )
{
	WWIV_ASSERT(pszSoundFileName);

	char szFileName[ MAX_PATH ];

	// append gfilesdir if no path specified
	if (strchr(pszSoundFileName, WWIV_FILE_SEPERATOR_CHAR) == NULL)
	{
		strncpy(szFileName, syscfg.gfilesdir, sizeof(szFileName));
		strncat(szFileName, pszSoundFileName, sizeof(szFileName));
	}
	else
	{
		strncpy( szFileName, pszSoundFileName, sizeof(szFileName));
	}

	// append .SDF if no extension specified
	if (strchr( szFileName, '.') == NULL)
	{
		strncat( szFileName, ".sdf", sizeof(szFileName));
	}

	// Must Exist
	if (!WFile::Exists(szFileName))
	{
		return false;
	}

	// must be able to open read-only
	FILE* hSoundFile = fsh_open(szFileName, "rt");
	if (!hSoundFile)
	{
		return false;
	}

	// scan each line, ignore lines with words<2
    char szSoundLine[ 255 ];
	while (fgets(szSoundLine, sizeof(szSoundLine), hSoundFile) != NULL)
	{
		if ( abortable && bkbhit() )
		{
			break;
		}
		int nw = wordcount( szSoundLine, DELIMS_WHITE);
		if (nw >= 2)
		{
            char szTemp[ 513 ];
			strncpy(szTemp, extractword(1, szSoundLine, DELIMS_WHITE), sizeof(szTemp));
			int freq = atoi(szTemp);
			strncpy(szTemp, extractword(2, szSoundLine, DELIMS_WHITE), sizeof(szTemp));
			int dur = atoi(szTemp);

			// only play if freq and duration > 0
			if ( freq > 0 && dur > 0 )
			{
                int nPauseDelay = 0;
				if (nw > 2)
				{
					strncpy( szTemp, extractword(3, szSoundLine, DELIMS_WHITE), sizeof( szTemp ) );
					nPauseDelay = atoi( szTemp );
				}
				WWIV_Sound(freq, dur);
				if ( nPauseDelay > 0 )
				{
					WWIV_Delay( nPauseDelay );
				}
			}
		}
	}

	// close and return success
	fsh_close( hSoundFile );
	return true;
}


/**
 * Describes the area code as listed in regions.dat
 * @param nAreaCode The area code to describe
 * @param pszDescription point to return the description for the specified
 *        area code.
 */
void describe_area_code(int nAreaCode, char *pszDescription)
{
	pszDescription[0] = '\0';

    WFile file( syscfg.datadir, REGIONS_DAT );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		// Failed to open REGIONS.DAT
		return;
	}
    char* ss = static_cast<char *>( BbsAllocA( file.GetLength() ) );
    int nNumRead = file.Read( ss, file.GetLength() );
	ss[nNumRead] = '\0';
    file.Close();
	char* ss1 = strtok(ss, "\r\n");
    bool done = false;
	while (ss1 && (!done))
	{
		int i = atoi(ss1);
		if ( i && i == nAreaCode )
		{
			done = true;
		}
		else
		{
			strcpy(pszDescription, ss1);
		}
		ss1 = strtok(NULL, "\r\n");
	}

	BbsFreeMemory(ss);
}


/**
 * Describes the town (area code + prefix) as listed in the regions file.
 * @param nAreaCode The area code to describe
 * @param town The phone number prefix to describe
 * @param pszDescription point to return the description for the specified
 *        area code.
 */
void describe_town( int nAreaCode, int town, char *pszDescription )
{
    char szFileName[ MAX_PATH ];
	pszDescription[0] = '\0';
	sprintf( szFileName,
			 "%s%s%c%s.%-3d",
			 syscfg.datadir,
			 REGIONS_DIR,
			 WWIV_FILE_SEPERATOR_CHAR,
			 REGIONS_DIR,
			 nAreaCode );
    WFile file( szFileName );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
	{
		// Failed to open regions area code file
		return;
	}
    char* ss = static_cast<char *>( BbsAllocA( file.GetLength() ) );
    int nNumRead = file.Read( ss, file.GetLength() );
	ss[nNumRead] = 0;
    file.Close();

	char* ss1 = strtok( ss, "\r\n" );
    bool done = false;
	while ( ss1 && !done )
	{
		int i = atoi(ss1);
		if ( i && i == town )
		{
			done = true;
		}
		else
		{
			strcpy( pszDescription, ss1 );
		}
		ss1 = strtok( NULL, "\r\n" );
	}
	BbsFreeMemory( ss );
}


