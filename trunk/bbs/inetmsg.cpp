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



//////////////////////////////////////////////////////////////////////////////
//
//
// module private functions
//
//


unsigned char *valid_name(unsigned char *s);


static unsigned char translate_table[] = { \
  "................................_!.#$%&.{}*+.-.{0123456789..{=}?" \
  "_abcdefghijklmnopqrstuvwxyz{}}-_.abcdefghijklmnopqrstuvwxyz{|}~." \
  "cueaaaaceeeiiiaaelaooouuyouclypfaiounnao?__..!{}................" \
  "................................abfneouyo0od.0en=+}{fj*=oo..n2.." };



unsigned char *valid_name(unsigned char *s)
{
	static unsigned char szName[60];

	unsigned int j = 0;
	for ( unsigned int i = 0; (i < strlen( reinterpret_cast<char*>( s ) ) ) && (i < 81); i++ )
	{
		if (s[i] != '.')
		{
			szName[j++] = translate_table[s[i]];
		}
	}
	szName[j] = 0;
	return szName;
}


void get_user_ppp_addr()
{
	sess->internetFullEmailAddress = "";
	bool found = false;
    int nNetworkNumber = getnetnum( "FILEnet" );
    sess->SetNetworkNumber( nNetworkNumber );
	if ( nNetworkNumber == -1 )
	{
		return;
	}
	set_net_num( sess->GetNetworkNumber() );
	wwiv::stringUtils::FormatString( sess->internetFullEmailAddress,
		                             "%s@%s",
                                     sess->internetEmailName.c_str(),
                                     sess->internetEmailDomain.c_str() );
    char szAcctFileName[ MAX_PATH ];
	sprintf(szAcctFileName, "%s%s", sess->GetNetworkDataDirectory(), ACCT_INI);
    FILE* fp = fsh_open(szAcctFileName, "rt");
    char szLine[ 255 ];
	if ( fp != NULL )
	{
		while ( fgets(szLine, 100, fp) && !found )
		{
			if (strnicmp(szLine, "USER", 4) == 0)
			{
				int nUserNum = atoi(&szLine[4]);
				if (nUserNum == sess->usernum)
				{
					char* ss = strtok(szLine, "=");
					ss = strtok(NULL, "\r\n");
					if (ss)
					{
						while (ss[0]==' ')
						{
							strcpy(ss, &ss[1]);
						}
						StringTrimEnd(ss);
						if (ss)
						{
							sess->internetFullEmailAddress = ss;
							found = true;
						}
					}
				}
			}
		}
		fclose(fp);
	}
    if ( !found && !sess->internetPopDomain.empty() )
	{
		int j = 0;
        char szLocalUserName[ 255 ];
        strcpy( szLocalUserName, sess->thisuser.GetName() );
		for ( int i = 0; ( i < wwiv::stringUtils::GetStringLength( szLocalUserName ) ) && ( i < 61 ); i++ )
		{
			if ( szLocalUserName[ i ] != '.' )
			{
				szLine[ j++ ] = translate_table[ (int)szLocalUserName[ i ] ];
			}
		}
		szLine[ j ] = '\0';
        wwiv::stringUtils::FormatString( sess->internetFullEmailAddress, "%s@%s", szLine, sess->internetPopDomain.c_str() );
	}
}


void send_inet_email()
{
	if ( sess->thisuser.GetNumEmailSentToday() > getslrec( sess->GetEffectiveSl() ).emails )
	{
		nl();
		sess->bout << "|#6Too much mail sent today.\r\n";
		return;
	}
	write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
    int nNetworkNumber = getnetnum( "FILEnet" );
    sess->SetNetworkNumber( nNetworkNumber );
	if ( nNetworkNumber == -1 )
	{
		return;
	}
	set_net_num( sess->GetNetworkNumber() );
	nl();
	sess->bout << "|#9Your Internet Address:|#1 " <<
	      ( sess->IsInternetUseRealNames() ? sess->thisuser.GetRealName() : sess->thisuser.GetName() ) <<
		  " <" << sess->internetFullEmailAddress << ">";
    nl( 2 );
	sess->bout << "|#9Enter the Internet mail destination address.\r\n|#7:";
	inputl( net_email_name, 75, true );
	if (check_inet_addr(net_email_name))
	{
		unsigned short nUserNumber = 0;
		unsigned short nSystemNumber = 32767;
		irt[0]=0;
		irt_name[0]=0;
		grab_quotes(NULL, NULL);
		if (nUserNumber || nSystemNumber)
		{
			email(nUserNumber, nSystemNumber, false, 0);
		}
	}
	else
	{
		nl();
		if (net_email_name[0])
		{
            sess->bout << "|#6Invalid address format!\r\n";
		}
		else
		{
			sess->bout << "|12Aborted.\r\n";
		}
	}
}


bool check_inet_addr(const char *inetaddr)
{
    if ( !inetaddr || !*inetaddr )
    {
        return false;
    }

	char szBuffer[80];

	strcpy(szBuffer, inetaddr);
	char *p = strchr(szBuffer,'@');
	if ( p == NULL || strchr(p, '.') == NULL )
	{
		return false;
	}
	return true;
}


char *read_inet_addr( char *addr, int nUserNumber )
{
	if (!nUserNumber)
	{
		return NULL;
	}

	if ( nUserNumber == sess->usernum && check_inet_addr( sess->thisuser.GetEmailAddress() ) )
	{
		strcpy( addr, sess->thisuser.GetEmailAddress() );
	}
	else
	{
		//addr = NULL;
		*addr = 0;
        WFile inetAddrFile( syscfg.datadir, INETADDR_DAT );
        if ( !inetAddrFile.Exists() )
		{
            inetAddrFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
			for (int i = 0; i <= syscfg.maxusers; i++)
			{
				long lCurPos = 80L * static_cast<long>( i );
                inetAddrFile.Seek( lCurPos, WFile::seekBegin );
                inetAddrFile.Write( addr, 80L );
			}
		}
		else
		{
            char szUserName[ 255 ];
            inetAddrFile.Open( WFile::modeReadOnly | WFile::modeBinary );
			long lCurPos = 80L * static_cast<long>( nUserNumber );
            inetAddrFile.Seek( lCurPos, WFile::seekBegin );
            inetAddrFile.Read( szUserName, 80L );
			if (check_inet_addr(szUserName))
			{
				strcpy(addr, szUserName);
			}
			else
			{
				sprintf(addr, "User #%d", nUserNumber);
                WUser user;
                GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
                user.SetEmailAddress( "" );
                GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
			}
		}
        inetAddrFile.Close();
	}
	return addr;
}


void write_inet_addr( const char *addr, int nUserNumber )
{
	if (!nUserNumber)
	{
		return; /*NULL;*/
	}

    WFile inetAddrFile( syscfg.datadir, INETADDR_DAT );
    inetAddrFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
	long lCurPos = 80L * static_cast<long>( nUserNumber );
    inetAddrFile.Seek( lCurPos, WFile::seekBegin );
    inetAddrFile.Write( const_cast<char*>( addr ), 80L );
    inetAddrFile.Close();
    char szDefaultUserAddr[ 255 ];
	sprintf(szDefaultUserAddr, "USER%d", nUserNumber);
    sess->SetNetworkNumber( getnetnum( "FILEnet" ) );
	if ( sess->GetNetworkNumber() != -1 )
	{
        char szInputFileName[ MAX_PATH ], szOutputFileName[ MAX_PATH ];
		set_net_num( sess->GetNetworkNumber() );
		sprintf(szInputFileName, "%s%s", sess->GetNetworkDataDirectory(), ACCT_INI);
		FILE* in = fsh_open(szInputFileName, "rt");
		sprintf(szOutputFileName, "%s%s", syscfgovr.tempdir, ACCT_INI);
		FILE* out = fsh_open(szOutputFileName, "wt+");
		if ( in && out )
		{
            char szLine[ 255 ];
			while (fgets(szLine, 80, in))
			{
                char szSavedLine[ 255 ];
				bool match = false;
				strcpy(szSavedLine, szLine);
				char* ss = strtok(szLine, "=");
				if (ss)
				{
					StringTrim(ss);
					if ( wwiv::stringUtils::IsEqualsIgnoreCase( szLine, szDefaultUserAddr ) )
					{
						match = true;
					}
				}
				if (!match)
				{
					fprintf(out, szSavedLine);
				}
			}
            char szAddress[ 255 ];
			sprintf(szAddress, "\nUSER%d = %s", nUserNumber, addr);
			fprintf(out, szAddress);
			fsh_close(in);
			fsh_close(out);
		}
		WFile::Remove(szInputFileName);
		copyfile(szOutputFileName, szInputFileName, false);
		WFile::Remove(szOutputFileName);
	}
}
