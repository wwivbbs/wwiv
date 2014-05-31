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
#include "instmsg.h"

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
                                           "................................abfneouyo0od.0en=+}{fj*=oo..n2.."
                                         };



unsigned char *valid_name(unsigned char *s) {
	static unsigned char szName[60];

	unsigned int j = 0;
	for ( unsigned int i = 0; (i < strlen( reinterpret_cast<char*>( s ) ) ) && (i < 81); i++ ) {
		if (s[i] != '.') {
			szName[j++] = translate_table[s[i]];
		}
	}
	szName[j] = 0;
	return szName;
}


void get_user_ppp_addr() {
	GetSession()->internetFullEmailAddress = "";
	bool found = false;
	int nNetworkNumber = getnetnum( "FILEnet" );
	GetSession()->SetNetworkNumber( nNetworkNumber );
	if ( nNetworkNumber == -1 ) {
		return;
	}
	set_net_num( GetSession()->GetNetworkNumber() );
	GetSession()->internetFullEmailAddress = wwiv::strings::StringPrintf("%s@%s",
	                                 GetSession()->internetEmailName.c_str(),
	                                 GetSession()->internetEmailDomain.c_str());
	WTextFile acctFile( GetSession()->GetNetworkDataDirectory(), ACCT_INI, "rt" );
	char szLine[ 260 ];
	if ( acctFile.IsOpen() ) {
		while ( acctFile.ReadLine(szLine, 255) && !found ) {
			if (WWIV_STRNICMP(szLine, "USER", 4) == 0) {
				int nUserNum = atoi(&szLine[4]);
				if (nUserNum == GetSession()->usernum) {
					char* ss = strtok(szLine, "=");
					ss = strtok(NULL, "\r\n");
					if (ss) {
						while (ss[0]==' ') {
							strcpy(ss, &ss[1]);
						}
						StringTrimEnd(ss);
						if (ss) {
							GetSession()->internetFullEmailAddress = ss;
							found = true;
						}
					}
				}
			}
		}
		acctFile.Close();
	}
	if ( !found && !GetSession()->internetPopDomain.empty() ) {
		int j = 0;
		char szLocalUserName[ 255 ];
		strcpy( szLocalUserName, GetSession()->GetCurrentUser()->GetName() );
		for ( int i = 0; ( i < wwiv::strings::GetStringLength( szLocalUserName ) ) && ( i < 61 ); i++ ) {
			if ( szLocalUserName[ i ] != '.' ) {
				szLine[ j++ ] = translate_table[ (int)szLocalUserName[ i ] ];
			}
		}
		szLine[ j ] = '\0';
		GetSession()->internetFullEmailAddress = wwiv::strings::StringPrintf("%s@%s", szLine, GetSession()->internetPopDomain.c_str());
	}
}


void send_inet_email() {
	if ( GetSession()->GetCurrentUser()->GetNumEmailSentToday() > getslrec( GetSession()->GetEffectiveSl() ).emails ) {
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#6Too much mail sent today.\r\n";
		return;
	}
	write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
	int nNetworkNumber = getnetnum( "FILEnet" );
	GetSession()->SetNetworkNumber( nNetworkNumber );
	if ( nNetworkNumber == -1 ) {
		return;
	}
	set_net_num( GetSession()->GetNetworkNumber() );
	GetSession()->bout.NewLine();
	GetSession()->bout << "|#9Your Internet Address:|#1 " <<
	                   ( GetSession()->IsInternetUseRealNames() ? GetSession()->GetCurrentUser()->GetRealName() : GetSession()->GetCurrentUser()->GetName() ) <<
	                   " <" << GetSession()->internetFullEmailAddress << ">";
	GetSession()->bout.NewLine( 2 );
	GetSession()->bout << "|#9Enter the Internet mail destination address.\r\n|#7:";
	inputl( net_email_name, 75, true );
	if (check_inet_addr(net_email_name)) {
		unsigned short nUserNumber = 0;
		unsigned short nSystemNumber = 32767;
		irt[0]=0;
		irt_name[0]=0;
		grab_quotes(NULL, NULL);
		if (nUserNumber || nSystemNumber) {
			email(nUserNumber, nSystemNumber, false, 0);
		}
	} else {
		GetSession()->bout.NewLine();
		if (net_email_name[0]) {
			GetSession()->bout << "|#6Invalid address format!\r\n";
		} else {
			GetSession()->bout << "|#6Aborted.\r\n";
		}
	}
}


bool check_inet_addr(const char *inetaddr) {
	if ( !inetaddr || !*inetaddr ) {
		return false;
	}

	char szBuffer[80];

	strcpy(szBuffer, inetaddr);
	char *p = strchr(szBuffer,'@');
	if ( p == NULL || strchr(p, '.') == NULL ) {
		return false;
	}
	return true;
}


char *read_inet_addr( char *pszInternetEmailAddress, int nUserNumber ) {
	if (!nUserNumber) {
		return NULL;
	}

	if ( nUserNumber == GetSession()->usernum && check_inet_addr( GetSession()->GetCurrentUser()->GetEmailAddress() ) ) {
		strcpy( pszInternetEmailAddress, GetSession()->GetCurrentUser()->GetEmailAddress() );
	} else {
		//pszInternetEmailAddress = NULL;
		*pszInternetEmailAddress = 0;
		WFile inetAddrFile( syscfg.datadir, INETADDR_DAT );
		if ( !inetAddrFile.Exists() ) {
			inetAddrFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
			for (int i = 0; i <= syscfg.maxusers; i++) {
				long lCurPos = 80L * static_cast<long>( i );
				inetAddrFile.Seek( lCurPos, WFile::seekBegin );
				inetAddrFile.Write( pszInternetEmailAddress, 80L );
			}
		} else {
			char szUserName[ 255 ];
			inetAddrFile.Open( WFile::modeReadOnly | WFile::modeBinary );
			long lCurPos = 80L * static_cast<long>( nUserNumber );
			inetAddrFile.Seek( lCurPos, WFile::seekBegin );
			inetAddrFile.Read( szUserName, 80L );
			if (check_inet_addr(szUserName)) {
				strcpy(pszInternetEmailAddress, szUserName);
			} else {
				sprintf(pszInternetEmailAddress, "User #%d", nUserNumber);
				WUser user;
				GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
				user.SetEmailAddress( "" );
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
			}
		}
		inetAddrFile.Close();
	}
	return pszInternetEmailAddress;
}


void write_inet_addr( const char *pszInternetEmailAddress, int nUserNumber ) {
	if (!nUserNumber) {
		return; /*NULL;*/
	}

	WFile inetAddrFile( syscfg.datadir, INETADDR_DAT );
	inetAddrFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite );
	long lCurPos = 80L * static_cast<long>( nUserNumber );
	inetAddrFile.Seek( lCurPos, WFile::seekBegin );
	inetAddrFile.Write( pszInternetEmailAddress, 80L );
	inetAddrFile.Close();
	char szDefaultUserAddr[ 255 ];
	sprintf(szDefaultUserAddr, "USER%d", nUserNumber);
	GetSession()->SetNetworkNumber( getnetnum( "FILEnet" ) );
	if ( GetSession()->GetNetworkNumber() != -1 ) {
		set_net_num( GetSession()->GetNetworkNumber() );
		WTextFile in( GetSession()->GetNetworkDataDirectory(), ACCT_INI, "rt" );
		WTextFile out( syscfgovr.tempdir, ACCT_INI, "wt+" );
		if ( in.IsOpen() && out.IsOpen() ) {
			char szLine[ 260 ];
			while (in.ReadLine(szLine, 255)) {
				char szSavedLine[ 260 ];
				bool match = false;
				strcpy(szSavedLine, szLine);
				char* ss = strtok(szLine, "=");
				if (ss) {
					StringTrim(ss);
					if ( wwiv::strings::IsEqualsIgnoreCase( szLine, szDefaultUserAddr ) ) {
						match = true;
					}
				}
				if (!match) {
					out.WriteFormatted( szSavedLine );
				}
			}
			out.WriteFormatted( "\nUSER%d = %s", nUserNumber, pszInternetEmailAddress);
			in.Close();
			out.Close();
		}
		WFile::Remove(in.GetFullPathName());
		copyfile(out.GetFullPathName(), in.GetFullPathName(), false);
		WFile::Remove(out.GetFullPathName());
	}
}
