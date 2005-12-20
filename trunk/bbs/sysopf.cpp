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

bool isr1( int nUserNumber, int nNumUsers, const char *pszName )
{
    int cp = 0;
    while ( cp < nNumUsers &&
            wwiv::stringUtils::StringCompare( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) > 0 )
	{
		++cp;
	}
    for ( int i = nNumUsers; i > cp; i-- )
	{
		smallist[i] = smallist[i - 1];
	}
	smalrec sr;
	strcpy( reinterpret_cast<char*>( sr.name ), pszName );
	sr.number = static_cast<unsigned short>( nUserNumber );
    smallist[cp] = sr;
    return true;
}


void reset_files()
{
	WUser user;

	WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    pStatus->SetNumUsers( 0 );
	GetSession()->bout.NewLine();
	int nNumUsers = GetApplication()->GetUserManager()->GetNumberOfUserRecords();
    WFile userFile( syscfg.datadir, USER_LST );
    if ( userFile.Open( WFile::modeBinary | WFile::modeReadWrite ) )
	{
		for ( int i = 1; i <= nNumUsers; i++ )
		{
			long pos = static_cast<long>( syscfg.userreclen ) * static_cast<long>( i );
            userFile.Seek( pos, WFile::seekBegin );
            userFile.Read( &user.data, syscfg.userreclen );
            if ( !user.isUserDeleted() )
			{
                user.FixUp();
                if ( isr1( i, nNumUsers, user.GetName() ) )
                {
                    pStatus->IncrementNumUsers();
                }
			}
			else
			{
				memset( &user.data, 0, syscfg.userreclen );
                user.SetInactFlag( 0 );
                user.SetInactFlag( inact_deleted );
			}
            userFile.Seek( pos, WFile::seekBegin );
            userFile.Write( &user.data, syscfg.userreclen );
			if ( ( i % 10 ) == 0 )
			{
                userFile.Close();
				GetSession()->bout << i << "\r ";
                userFile.Open( WFile::modeBinary | WFile::modeReadWrite );
			}
		}
        userFile.Close();
	}
	GetSession()->bout << "\r\n\r\n";

    WFile namesFile( syscfg.datadir, NAMES_LST );
    if ( !namesFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeTruncate ) )
	{
        std::cout << namesFile.GetFullPathName() << " NOT FOUND" << std::endl;
		GetApplication()->AbortBBS( true );
	}
    namesFile.Write( smallist, sizeof(smalrec) * pStatus->GetNumUsers() );
    namesFile.Close();
	GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
}



void prstatus( bool bIsWFC )
{
	GetApplication()->GetStatusManager()->RefreshStatusCache();
	GetSession()->bout.ClearScreen();
	if ( syscfg.newuserpw[0] != '\0' )
	{
		GetSession()->bout << "|#9New User Pass   : " << syscfg.newuserpw << wwiv::endl;
	}
	GetSession()->bout << "|#9Board is        : " << ( syscfg.closedsystem ? "Closed" : "Open" ) << wwiv::endl;

    std::auto_ptr<WStatus> pStatus( GetApplication()->GetStatusManager()->GetStatus() );
	if ( !bIsWFC )
	{
		// All of this information is on the WFC Screen
        GetSession()->bout << "|#9Number Users    : |#2" << pStatus->GetNumUsers() << wwiv::endl;
        GetSession()->bout << "|#9Number Calls    : |#2" << pStatus->GetCallerNumber() << wwiv::endl;
        GetSession()->bout << "|#9Last Date       : |#2" << pStatus->GetLastDate() << wwiv::endl;
		GetSession()->bout << "|#9Time            : |#2" << times() << wwiv::endl;
        GetSession()->bout << "|#9Active Today    : |#2" << pStatus->GetMinutesActiveToday() << wwiv::endl;
        GetSession()->bout << "|#9Calls Today     : |#2" << pStatus->GetNumCallsToday() << wwiv::endl;
        GetSession()->bout << "|#9Net Posts Today : |#2" << ( pStatus->GetNumMessagesPostedToday() - pStatus->GetNumLocalPosts() ) << wwiv::endl;
        GetSession()->bout << "|#9Local Post Today: |#2" << pStatus->GetNumLocalPosts() << wwiv::endl;
		GetSession()->bout << "|#9E Sent Today    : |#2" << pStatus->GetNumEmailSentToday() << wwiv::endl;
        GetSession()->bout << "|#9F Sent Today    : |#2" << pStatus->GetNumFeedbackSentToday() << wwiv::endl;
        GetSession()->bout << "|#9Uploads Today   : |#2" << pStatus->GetNumUploadsToday() << wwiv::endl;
		GetSession()->bout << "|#9Feedback Waiting: |#2" << fwaiting << wwiv::endl;
        GetSession()->bout << "|#9Sysop           : |#2" << ( ( sysop2() ) ? "Available" : "NOT Available" ) << wwiv::endl;
	}
	GetSession()->bout << "|#9Q-Scan Pointer  : |#2" << pStatus->GetQScanPointer() << wwiv::endl;

    if ( num_instances() > 1 )
	{
		multi_instance();
	}
}


void valuser( int nUserNumber )
{
	char s[81], s1[81], s2[81], s3[81], ar1[20], dar1[20];

	WUser user;
    GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
    if ( !user.isUserDeleted() )
	{
		GetSession()->bout.NewLine();
        GetSession()->bout << "|#9Name: |#2" << user.GetUserNameAndNumber( nUserNumber ) << wwiv::endl;
        GetSession()->bout << "|#9RN  : |#2" << user.GetRealName() << wwiv::endl;
        GetSession()->bout << "|#9PH  : |#2" << user.GetVoicePhoneNumber() << wwiv::endl;
        GetSession()->bout << "|#9Age : |#2" << user.GetAge() << " " << user.GetGender() << wwiv::endl;
        GetSession()->bout << "|#9Comp: |#2" << ctypes( user.GetComputerType() ) << wwiv::endl;
        if ( user.GetNote()[0] )
		{
            GetSession()->bout << "|#9Note: |#2" << user.GetNote() << wwiv::endl;
		}
        GetSession()->bout << "|#9SL  : |#2" << user.GetSl() << wwiv::endl;
		if ( user.GetSl() != 255 && user.GetSl() < GetSession()->GetEffectiveSl() )
		{
            GetSession()->bout << "|#9New : ";
			input( s, 3, true );
			if (s[0])
			{
				int nSl = atoi( s );
				if ( !GetApplication()->GetWfcStatus() && nSl >= GetSession()->GetEffectiveSl() )
				{
					nSl = -2;
				}
				if ( nSl >= 0 && nSl < 255 )
				{
					user.SetSl( nSl );
				}
				if ( nSl == -1 )
				{
					GetSession()->bout.NewLine();
					GetSession()->bout << "|#9Delete? ";
					if ( yesno() )
					{
						deluser( nUserNumber );
						GetSession()->bout.NewLine();
						GetSession()->bout << "|12Deleted.\r\n\n";
					}
					else
					{
						GetSession()->bout.NewLine();
						GetSession()->bout << "|13NOT deleted.\r\n";
					}
					return;
				}
			}
		}
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#9DSL : |#2" << user.GetDsl() << wwiv::endl;
		if ( user.GetDsl() != 255 && user.GetDsl() < GetSession()->GetCurrentUser()->GetDsl() )
		{
			GetSession()->bout << "|#9New ? ";
			input( s, 3, true );
			if (s[0])
			{
				int nDsl = atoi(s);
				if ( !GetApplication()->GetWfcStatus() && nDsl >= GetSession()->GetCurrentUser()->GetDsl() )
				{
					nDsl = -1;
				}
				if ( nDsl >= 0 && nDsl < 255 )
				{
                    user.SetDsl( nDsl );
				}
			}
		}
		strcpy( s3, restrict_string );
		int ar2     = 1;
		int dar2    = 1;
		ar1[0]      = RETURN;
		dar1[0]     = RETURN;
		for ( int i = 0; i <= 15; i++ )
		{
			if ( user.hasArFlag( 1 << i  ))
			{
				s[i] = static_cast<char>( 'A' + i );
			}
			else
			{
				s[i] = SPACE;
			}
			if ( GetSession()->GetCurrentUser()->hasArFlag( 1 << i ) )
			{
				ar1[ar2++] = static_cast<char>( 'A' + i );
			}
			if (user.hasDarFlag(1 << i))
			{
				s1[i] = static_cast<char>( 'A' + i );
			}
			else
			{
				s1[i] = SPACE;
			}
			if ( GetSession()->GetCurrentUser()->hasDarFlag( 1 << i ) )
			{
				dar1[dar2++] = static_cast<char>( 'A' + i );
			}
			if ( user.hasRestrictionFlag( 1 << i ) )
			{
				s2[i] = s3[i];
			}
			else
			{
				s2[i] = SPACE;
			}
		}
		s[16]       = '\0';
		s1[16]      = '\0';
		s2[16]      = '\0';
		ar1[ar2]    = '\0';
		dar1[dar2]  = '\0';
		GetSession()->bout.NewLine();
		char ch1 = '\0';
		if ( ar2 > 1 )
		{
			do
			{
				GetSession()->bout << "|#9AR  : |#2" << s << wwiv::endl;
				GetSession()->bout << "|#9Togl? ";
				ch1 = onek( ar1 );
				if ( ch1 != RETURN )
				{
					ch1 -= 'A';
					if ( s[ch1] == SPACE )
					{
						s[ch1] = ch1 + 'A';
					}
					else
					{
						s[ch1] = SPACE;
					}
					user.toggleArFlag(1 << ch1);
					ch1 = 0;
				}
			} while ( !hangup && ch1 != RETURN );
		}
		GetSession()->bout.NewLine();
		ch1 = 0;
		if (dar2 > 1)
		{
			do
			{
				GetSession()->bout << "|#9DAR : |#2" << s1 << wwiv::endl;
				GetSession()->bout << "|#9Togl? ";
				ch1 = onek( dar1 );
				if (ch1 != RETURN)
				{
					ch1 -= 'A';
					if (s1[ch1] == SPACE)
					{
						s1[ch1] = ch1 + 'A';
					}
					else
					{
						s1[ch1] = SPACE;
					}
					user.toggleDarFlag(1 << ch1);
					ch1 = 0;
				}
			} while ( !hangup && ch1 != RETURN );
		}
		GetSession()->bout.NewLine();
		ch1     = 0;
		s[0]    = RETURN;
		s[1]    = '?';
		strcpy(&(s[2]), restrict_string);
		do
		{
			GetSession()->bout << "      |#2" << s3 << wwiv::endl;
			GetSession()->bout << "|#9Rstr: |#2" << s2 << wwiv::endl;
			GetSession()->bout << "|#9Togl? ";
			ch1 = onek( s );
			if ( ch1 != RETURN && ch1 != SPACE && ch1 != '?' )
			{
				int i = -1;
				for ( int i1 = 0; i1 < 16; i1++ )
				{
					if ( ch1 == s[i1 + 2] )
					{
						i = i1;
					}
				}
				if ( i > -1 )
				{
					user.toggleRestrictionFlag( 1 << i );
					if ( s2[i] == SPACE )
					{
						s2[i] = s3[i];
					}
					else
					{
						s2[i] = SPACE;
					}
				}
				ch1 = 0;
			}
			if ( ch1 == '?' )
			{
				ch1 = 0;
				printfile(SRESTRCT_NOEXT);
			}
		} while ( !hangup && ch1 == 0 );
        GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
		GetSession()->bout.NewLine();
  }
  else
  {
	  GetSession()->bout << "\r\n|12No Such User.\r\n\n";
  }
}

#define NET_SEARCH_SUBSTR     0x0001
#define NET_SEARCH_AREACODE   0x0002
#define NET_SEARCH_GROUP      0x0004
#define NET_SEARCH_SC         0x0008
#define NET_SEARCH_AC         0x0010
#define NET_SEARCH_GC         0x0020
#define NET_SEARCH_NC         0x0040
#define NET_SEARCH_PHSUBSTR   0x0080
#define NET_SEARCH_NOCONNECT  0x0100
#define NET_SEARCH_NEXT       0x0200
#define NET_SEARCH_HOPS       0x0400
#define NET_SEARCH_ALL        0xFFFF


void print_net_listing( bool bForcePause )
{
	int i, gn = 0;
	char town[5];
	net_system_list_rec csne;
	unsigned short slist, cmdbit = 0;
	char substr[81], onx[20], acstr[4], phstr[13], *mmk;
	char s[255], s1[101], s2[101], s3[101], s4[101], bbstype;
	bool bHadPause = false;

	GetApplication()->GetStatusManager()->RefreshStatusCache();

	if (!GetSession()->GetMaxNetworkNumber())
	{
		return;
	}

	write_inst(INST_LOC_NETLIST, 0, INST_FLAGS_NONE);

	if ( bForcePause )
	{
		bHadPause  = GetSession()->GetCurrentUser()->hasPause();
		if ( bHadPause )
		{
            GetSession()->GetCurrentUser()->toggleStatusFlag( WUser::pauseOnPage );
		}
	}
	bool done = false;
	while (!done && !hangup)
	{
		GetSession()->bout.ClearScreen();
		if (GetSession()->GetMaxNetworkNumber() > 1)
		{
			odc[0] = 0;
			int odci = 0;
			onx[0] = 'Q';
			onx[1] = 0;
			int onxi = 1;
			GetSession()->bout.NewLine();
			for (i = 0; i < GetSession()->GetMaxNetworkNumber(); i++)
			{
				if (i < 9)
				{
					onx[onxi++] = static_cast<char>( i + '1' );
					onx[onxi] = 0;
				}
				else
				{
					odci = (i + 1) / 10;
					odc[odci - 1] = static_cast<char>( odci + '0' );
					odc[odci] = 0;
				}
                GetSession()->bout << "|#2" << i + 1 << "|#9)|#1 " << net_networks[i].name << wwiv::endl;
			}
			GetSession()->bout << "|#2Q|#9)|#1 Quit\r\n\n";
			GetSession()->bout << "|#9Which network? |#2";
			if (GetSession()->GetMaxNetworkNumber() < 9)
			{
				char ch = onek(onx);
				if (ch == 'Q')
				{
					done = true;
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
					done = true;
				}
				else
				{
					i = atoi(mmk) - 1;
				}
			}

			if (done)
			{
				break;
			}

			if ( ( i < 0 ) || ( i > GetSession()->GetMaxNetworkNumber() ) )
			{
				continue;
			}
		}
		else
		{
			// i is the current network number, if there is only 1, they use it.
			i = 1;
		}

		bool done1 = false;
        bool abort;

		while (!done1)
		{
			int i2 = i;
			set_net_num(i2);
			read_bbs_list_index();
			abort = false;
			cmdbit = 0;
			slist = 0;
			substr[0] = 0;
			acstr[0] = 0;
			phstr[0] = 0;

			GetSession()->bout.ClearScreen();
			GetSession()->bout.NewLine();
            GetSession()->bout << "|#9Network|#2: |#1" << GetSession()->GetNetworkName() << wwiv::endl;
			GetSession()->bout.NewLine();

			GetSession()->bout << "|#21|#9) = |#1List All\r\n";
			GetSession()->bout << "|#22|#9) = |#1Area Code\r\n";
			GetSession()->bout << "|#23|#9) = |#1Group\r\n";
			GetSession()->bout << "|#24|#9) = |#1Subs Coordinators\r\n";
			GetSession()->bout << "|#25|#9) = |#1Area Coordinators\r\n";
			GetSession()->bout << "|#26|#9) = |#1Group Coordinators\r\n";
			GetSession()->bout << "|#27|#9) = |#1Net Coordinator\r\n";
			GetSession()->bout << "|#28|#9) = |#1BBS Name SubString\r\n";
			GetSession()->bout << "|#29|#9) = |#1Phone SubString\r\n";
			GetSession()->bout << "|#20|#9) = |#1Unconnected Systems\r\n";
			GetSession()->bout << "|#2Q|#9) = |#1Quit NetList\r\n";
			GetSession()->bout.NewLine();
			GetSession()->bout << "|#9Select: |#2";
			char cmd = onek("Q1234567890");

			switch (cmd)
			{
			case 'Q':
				if (GetSession()->GetMaxNetworkNumber() < 2)
				{
					done = true;
				}
				done1 = true;
				break;
			case '1':
				cmdbit = NET_SEARCH_ALL;
				break;
			case '2':
				cmdbit = NET_SEARCH_AREACODE;
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#1Enter Area Code|#2: |#0";
				input(acstr, 3);
				if (strlen(acstr) != 3)
				{
					abort = true;
				}
				if (!abort)
				{
					for (i = 0; i < 3; i++)
					{
						if ((acstr[i] < '0') || (acstr[i] > '9'))
						{
							abort = true;
						}
					}
				}
				if (abort)
				{
					GetSession()->bout << "|12Area code must be a 3-digit number!\r\n";
					pausescr();
					cmdbit = 0;
				}
				break;
			case '3':
				cmdbit = NET_SEARCH_GROUP;
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#1Enter group number|#2: |#0";
				input(s, 2);
				if ((s[0] == 0) || (atoi(s) < 1))
				{
					GetSession()->bout << "|12Invalid group number!\r\n";
					pausescr();
					cmdbit = 0;
					break;
				}
				gn = atoi(s);
				break;
			case '4':
				cmdbit = NET_SEARCH_SC;
				break;
			case '5':
				cmdbit = NET_SEARCH_AC;
				break;
			case '6':
				cmdbit = NET_SEARCH_GC;
				break;
			case '7':
				cmdbit = NET_SEARCH_NC;
				break;
			case '8':
				cmdbit = NET_SEARCH_SUBSTR;
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#1Enter SubString|#2: |#0";
				input(substr, 40);
				if (substr[0] == 0)
				{
					GetSession()->bout << "|12Enter a substring!\r\n";
					pausescr();
					cmdbit = 0;
				}
				break;
			case '9':
				cmdbit = NET_SEARCH_PHSUBSTR;
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#1Enter phone substring|#2: |#0";
				input(phstr, 12);
				if (phstr[0] == 0)
				{
					GetSession()->bout << "|12Enter a phone substring!\r\n";
					pausescr();
					cmdbit = 0;
				}
				break;
			case '0':
				cmdbit = NET_SEARCH_NOCONNECT;
				break;
			}

			if (!cmdbit)
			{
				continue;
			}

			if (done1)
			{
				break;
			}

			GetSession()->bout.NewLine();
			GetSession()->bout << "|#1Print BBS region info? ";
			bool useregion = yesno();

            WFile bbsListFile( GetSession()->GetNetworkDataDirectory(), BBSDATA_NET );
            if ( !bbsListFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
			{
                GetSession()->bout << "|12Error opening " << bbsListFile.GetFullPathName() << "!\r\n";
				pausescr();
				continue;
			}
			strcpy(s, "000-000-0000");
			GetSession()->bout.NewLine( 2 );

			for (i = 0; (i < GetSession()->num_sys_list) && (!abort); i++)
			{
				bool matched = false;
                bbsListFile.Read( &csne, sizeof( net_system_list_rec ) );
				if ((csne.forsys == 65535) && (cmdbit != NET_SEARCH_NOCONNECT))
				{
					continue;
				}
				strcpy(s1, csne.phone);

				if (csne.other & other_net_coord)
				{
					bbstype = '&';
				}
				else if (csne.other & other_group_coord)
				{
					bbstype = '%';
				}
				else if (csne.other & other_area_coord)
				{
					bbstype = '^';
				}
				else if (csne.other & other_subs_coord)
				{
					bbstype = '~';
				}
				else
				{
					bbstype = ' ';
				}

				strcpy(s2, csne.name);
				for (int i1 = 0; i1 < wwiv::stringUtils::GetStringLength(s2); i1++)
				{
					s2[i1] = upcase(s2[i1]);
				}

				switch (cmdbit)
				{
				case NET_SEARCH_ALL:
					matched = true;
					break;
				case NET_SEARCH_AREACODE:
					if (strncmp(acstr, csne.phone, 3) == 0)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_GROUP:
					if (gn == csne.group)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_SC:
					if (csne.other & other_subs_coord)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_AC:
					if (csne.other & other_area_coord)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_GC:
					if (csne.other & other_group_coord)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_NC:
					if (csne.other & other_net_coord)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_SUBSTR:
					if (strstr(s2, substr) != NULL)
					{
						matched = true;
					}
					else
					{
						sprintf(s3, "@%u", csne.sysnum);
						if (strstr(s3, substr) != NULL)
						{
							matched = true;
						}
					}
					break;
				case NET_SEARCH_PHSUBSTR:
					if (strstr(s1, phstr) != NULL)
					{
						matched = true;
					}
					break;
				case NET_SEARCH_NOCONNECT:
					if (csne.forsys == 65535)
					{
						matched = true;
					}
					break;
				}

				if ( matched )
				{
					slist++;
					if ( !useregion && slist == 1 )
					{
						pla("|#1 Node  Phone         BBS Name                                 Hop  Next Gr", &abort);
						pla("|#7-----  ============  ---------------------------------------- === ----- --", &abort);
					}
					else
					{
						if ( useregion && strncmp(s, csne.phone, 3) != 0 )
						{
							strcpy( s, csne.phone );
							sprintf(s2, "%s%s%c%s.%-3u", syscfg.datadir, REGIONS_DIR, WWIV_FILE_SEPERATOR_CHAR, REGIONS_DIR, atoi( csne.phone ) );
							if ( WFile::Exists( s2 ) )
							{
								sprintf( town, "%c%c%c", csne.phone[4], csne.phone[5], csne.phone[6] );
								describe_town( atoi( csne.phone ), atoi( town ), s3 );
							}
							else
							{
								describe_area_code(atoi(csne.phone), s3);
							}
							sprintf(s4, "\r\n%s%s\r\n", "|#2Region|#0: |#2", s3);
							pla(s4, &abort);
							pla("|#1 Node  Phone         BBS Name                                 Hop  Next Gr", &abort);
							pla("|#7-----  ============  ---------------------------------------- === ----- --", &abort);
						}
					}
					if (cmdbit != NET_SEARCH_NOCONNECT)
					{
						sprintf(s3, "%5u%c %12s  %-40s %3d %5u %2d",
							csne.sysnum, bbstype, csne.phone, csne.name,
							csne.numhops, csne.forsys, csne.group);
					}
					else
					{
						sprintf(s3, "%5u%c %12s  %-40s%s%2d",
							csne.sysnum, bbstype, csne.phone, csne.name, " |B1|15--- ----- |#0", csne.group);
					}
					pla(s3, &abort);
				}
      }
      bbsListFile.Close();
      if ( !abort && slist )
	  {
		  GetSession()->bout.NewLine();
		  GetSession()->bout << "|#1Systems Listed |#7: |#2" << slist;
      }
      GetSession()->bout.NewLine( 2 );
      pausescr();
    }
  }
  if ( bForcePause && bHadPause)
  {
      GetSession()->GetCurrentUser()->toggleStatusFlag( WUser::pauseOnPage );
  }
}


void read_new_stuff()
{
	zap_bbs_list();
	for ( int i = 0; i < GetSession()->GetMaxNetworkNumber(); i++ )
	{
		set_net_num( i );
		zap_call_out_list();
		zap_contacts();
	}
	set_language_1( GetSession()->GetCurrentLanguageNumber() );
}


void mailr()
{
	mailrec m, m1;
	filestatusrec fsr;

	WFile *pFileEmail = OpenEmailFile( false );
	WWIV_ASSERT( pFileEmail );
	if ( pFileEmail->IsOpen() )
	{
		int nRecordNumber = pFileEmail->GetLength() / sizeof( mailrec ) - 1;
		char c = ' ';
		while ( nRecordNumber >= 0 && c != 'Q' && !hangup )
		{
			pFileEmail->Seek( nRecordNumber * sizeof( mailrec ), WFile::seekBegin );
			pFileEmail->Read( &m, sizeof( mailrec ) );
			if (m.touser != 0)
			{
				pFileEmail->Close();
				do
				{
                    WUser user;
                    GetApplication()->GetUserManager()->ReadUser( &user, m.touser );
                    GetSession()->bout << "|#1  To|#7: |#" << GetSession()->GetMessageColor() << user.GetUserNameAndNumber( m.touser ) << wwiv::endl;
					int tp = 80;
					int nn = 0;
					if (m.status & status_source_verified)
					{
						tp -= 2;
					}
					if (m.status & status_new_net)
					{
						tp -= 1;
						if (wwiv::stringUtils::GetStringLength(m.title) <= tp)
						{
							nn = m.title[tp + 1];
						}
						else
						{
							nn = 0;
						}
					}
					else
					{
						nn = 0;
					}
					set_net_num(nn);
                    GetSession()->bout << "|#1Subj|#7: |#" << GetSession()->GetMessageColor() << m.title << wwiv::endl;
					if (m.status & status_file)
					{
						WFile attachDat( syscfg.datadir, ATTACH_DAT );
						if ( attachDat.Open( WFile::modeReadOnly | WFile::modeBinary, WFile::shareUnknown, WFile::permReadWrite ) )
						{
							bool found = false;
							long lAttachFileSize = attachDat.Read( &fsr, sizeof( fsr ) );
							while ( lAttachFileSize > 0 && !found )
							{
								if ( m.daten == static_cast<unsigned long>( fsr.id ) )
								{
                                    GetSession()->bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0\r\n";
									found = true;
								}
								if ( !found )
								{
									lAttachFileSize = attachDat.Read( &fsr, sizeof( fsr ) );
								}
							}
							if ( !found )
							{
								GetSession()->bout << "|#1Filename|#0.... |#2File : Unknown or Missing|#0\r\n";
							}
							attachDat.Close();
						}
						else
						{
							GetSession()->bout << "|#1Filename|#0.... |#2|#2File : Unknown or Missing|#0\r\n";
						}
					}
					bool next;
					read_message1(&(m.msg), (char) (m.anony & 0x0f), true, &next, "email", m.fromsys, m.fromuser );
					GetSession()->bout << "|#2R,D,Q,<space>  : ";
					if (next)
					{
						c = ' ';
					}
					else
					{
						c = onek("QRD ");
					}
					if (c == 'D')
					{
						pFileEmail = OpenEmailFile( true );
						pFileEmail->Seek( nRecordNumber * sizeof( mailrec ), WFile::seekBegin );
						pFileEmail->Read( &m1, sizeof( mailrec ) );
						if (memcmp(&m, &m1, sizeof(mailrec)) == 0)
						{
							delmail( pFileEmail, nRecordNumber );
							bool found = false;
							if (m.status & status_file)
							{
								WFile attachFile( syscfg.datadir, ATTACH_DAT );
								if ( attachFile.Open( WFile::modeReadWrite | WFile::modeBinary ) )
								{
									long lAttachFileSize = attachFile.Read( &fsr, sizeof( fsr ) );
									while ( lAttachFileSize > 0 && !found )
									{
										if (m.daten == static_cast<unsigned long>( fsr.id ) )
										{
											found = true;
											fsr.id = 0;
											attachFile.Seek( static_cast<long>( sizeof( filestatusrec ) ) * -1L, WFile::seekCurrent );
											attachFile.Write( &fsr, sizeof( filestatusrec ) );
											WFile::Remove( g_szAttachmentDirectory, fsr.filename );
										}
										else
										{
											attachFile.Read( &fsr, sizeof( filestatusrec ) );
										}
									}
									attachFile.Close();
								}
							}
						}
						else
						{
							GetSession()->bout << "Mail file changed; try again.\r\n";
						}
						pFileEmail->Close();
						if ( !GetSession()->IsUserOnline() && m.touser == 1 && m.tosys == 0 )
						{
                            GetSession()->GetCurrentUser()->SetNumMailWaiting( GetSession()->GetCurrentUser()->GetNumMailWaiting() - 1 );
						}
					}
					GetSession()->bout.NewLine( 2 );
				} while ((c == 'R') && (!hangup));

				pFileEmail = OpenEmailFile( false );
				WWIV_ASSERT( pFileEmail );
				if ( !pFileEmail->IsOpen() )
				{
					break;
				}
			}
			nRecordNumber -= 1;
		}
		pFileEmail->Close();
	}
	delete pFileEmail;
}


void chuser()
{
	if ( !ValidateSysopPassword() || !so() )
	{
		return;
	}

    GetSession()->bout << "|#9Enter user to change to: ";
    char szUserName[81];
	input( szUserName, 30, true );
	int nUserNumber = finduser1( szUserName );
	if ( nUserNumber > 0 )
	{
		GetSession()->WriteCurrentUser( GetSession()->usernum );
		write_qscn(GetSession()->usernum, qsc, false);
        GetSession()->ReadCurrentUser( nUserNumber );
		read_qscn(nUserNumber, qsc, false);
		GetSession()->usernum = static_cast<unsigned short>( nUserNumber );
		GetSession()->SetEffectiveSl( 255 );
		sysoplogf( "#*#*#* Changed to %s", GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum ) );
		changedsl();
		GetApplication()->UpdateTopScreen();
	}
	else
	{
		GetSession()->bout << "|12Unknown user.\r\n";
	}
}

void zlog()
{
    WFile file( syscfg.datadir, ZLOG_DAT );
    if ( !file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
	{
		return;
	}
	int i = 0;
	bool abort = false;
	zlogrec z;
    file.Read( &z, sizeof( zlogrec ) );
	GetSession()->bout.NewLine();
	GetSession()->bout.ClearScreen();
    GetSession()->bout.NewLine( 2 );
	pla("|#2  Date     Calls  Active   Posts   Email   Fback    U/L    %Act   T/user", &abort);
	pla("|#7--------   -----  ------   -----   -----   -----    ---    ----   ------", &abort);
	while ( i < 97 && !abort && !hangup && z.date[0] != 0 )
	{
		int nTimePerUser = 0;
		if ( z.calls )
		{
			nTimePerUser = z.active / z.calls;
		}
        char szBuffer[ 255 ];
		sprintf(szBuffer, "%s    %4d    %4d     %3d     %3d     %3d    %3d     %3d      %3d|B0",
            z.date, z.calls, z.active, z.posts, z.email, z.fback, z.up, 10 * z.active / 144, nTimePerUser );
		// alternate colors to make it easier to read across the lines
		if (i % 2)
		{
			GetSession()->bout << "|#1";
			pla( szBuffer, &abort );
		}
		else
		{
			GetSession()->bout << "|13";
			pla( szBuffer, &abort );
		}
		++i;
		if (i < 97)
		{
            file.Seek( i * sizeof( zlogrec ), WFile::seekBegin );
            file.Read( &z, sizeof( zlogrec ) );
		}
	}
    file.Close();
}


void set_user_age()
{
    std::auto_ptr<WStatus> pStatus( GetApplication()->GetStatusManager()->GetStatus() );
    int nUserNumber = 1;
	do
	{
    	WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
        int nAge = years_old( user.GetBirthdayMonth(), user.GetBirthdayDay(), user.GetBirthdayYear() );
        if ( nAge != user.GetAge() )
		{
            user.SetAge( nAge );
            GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
		}
		++nUserNumber;
    } while ( nUserNumber <= pStatus->GetNumUsers() );
}


void auto_purge()
{
	char s[80];
	unsigned int days = 0;
	int skipsl = 0;

    WIniFile iniFile( WWIV_INI );
    if ( iniFile.Initialize( INI_TAG ) )
	{
        days = iniFile.GetNumericValue( "AUTO_USER_PURGE" );
        skipsl = iniFile.GetNumericValue( "NO_PURGE_SL" );
	}
    iniFile.Close();

	if (days < 60)
	{
        if ( days > 0 )
        {
            sysoplog( "!!! WARNING: Auto-Purge canceled [AUTO_USER_PURGE < 60]", false );
            sysoplog( "!!! WARNING: Edit WWIV.INI and Fix this", false );
        }
		return;
	}

	time_t tTime = time( NULL );
	int nUserNumber = 1;
	sysoplogfi( false, "Auto-Purged Inactive Users (over %d days, SL less than %d)", days, skipsl );

    do
	{
        WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
        if ( !user.isExemptAutoDelete() )
		{
            unsigned int d = static_cast<unsigned int>( ( tTime - user.GetLastOnDateNumber() ) / SECONDS_PER_DAY_FLOAT );
			// if user is not already deleted && SL<NO_PURGE_SL && last_logon
			// greater than AUTO_USER_PURGE days ago
            if ( !user.isUserDeleted() && user.GetSl() < skipsl && d > days )
			{
                sprintf( s, "*** AUTOPURGE: Deleted User: #%3.3d %s", nUserNumber, user.GetName() );
				sysoplog( s, false );
				deluser( nUserNumber );
			}
		}
		++nUserNumber;
    } while ( nUserNumber <= GetApplication()->GetStatusManager()->GetUserCount() );
}


void beginday( bool displayStatus )
{
    if ( ( GetSession()->GetBeginDayNodeNumber() > 0 ) && ( GetApplication()->GetInstanceNumber() != GetSession()->GetBeginDayNodeNumber() ) )
	{
        // If BEGINDAYNODENUMBER is > 0 or defined in WWIV.INI only handle beginday events on that node number
		GetApplication()->GetStatusManager()->RefreshStatusCache();
		return;
	}
	WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    pStatus->ValidateAndFixDates();

    if ( wwiv::stringUtils::IsEquals( date(), pStatus->GetLastDate() ) )
	{
		GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
		return;
	}
	GetSession()->bout << "|#7* |#1Running Daily Maintenance...\r\n";
	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Updating system activity...\r\n";
    }

	zlogrec z;
    strcpy( z.date, pStatus->GetLastDate());
    z.active            = pStatus->GetMinutesActiveToday();
    z.calls             = pStatus->GetNumCallsToday();
    z.posts             = pStatus->GetNumLocalPosts();
    z.email             = pStatus->GetNumEmailSentToday();
    z.fback             = pStatus->GetNumFeedbackSentToday();
    z.up                = pStatus->GetNumUploadsToday();
    pStatus->NewDay();
    

	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Cleaning up log files...\r\n";
    }
    WFile::Remove( syscfg.gfilesdir, pStatus->GetLogFileName(1) );
	WFile::Remove( syscfg.gfilesdir, USER_LOG );

	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Updating ZLOG information...\r\n";
    }
    WFile fileZLog( syscfg.datadir, ZLOG_DAT );
	zlogrec z1;
    if ( !fileZLog.Open( WFile::modeReadWrite | WFile::modeBinary ) )
	{
        fileZLog.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone, WFile::permReadWrite );
		z1.date[0]  = '\0';
		z1.active   = 0;
		z1.calls    = 0;
		z1.posts    = 0;
		z1.email    = 0;
		z1.fback    = 0;
		z1.up       = 0;
		for ( int i = 0; i < 97; i++ )
		{
            fileZLog.Write( &z1, sizeof( zlogrec ) );
		}
	}
	else
	{
		for ( int i = 96; i >= 1; i-- )
		{
            fileZLog.Seek( ( i - 1 ) * sizeof( zlogrec ), WFile::seekBegin );
            fileZLog.Read( &z1, sizeof(zlogrec) );
            fileZLog.Seek( i * sizeof( zlogrec ), WFile::seekBegin );
            fileZLog.Write( &z1, sizeof( zlogrec ) );
		}
	}
    fileZLog.Seek( 0L, WFile::seekBegin );
    fileZLog.Write( &z, sizeof( zlogrec ) );
    fileZLog.Close();

	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Updating STATUS.DAT...\r\n";
    }
    int nus = syscfg.maxusers - pStatus->GetNumUsers();

    GetApplication()->GetStatusManager()->CommitTransaction( pStatus );
	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Checking system directories and user space...\r\n";
    }

	double fk = freek1(syscfg.datadir);

	if ( fk < 512.0 )
	{
		ssm(1, 0, "Only %dk free in data directory.", static_cast<int>( fk ) );
	}
	if ( !syscfg.closedsystem && nus < 15 )
	{
		ssm(1, 0, "Only %d new user slots left.", nus);
	}
	if ( syscfg.beginday_c && *syscfg.beginday_c )
	{
        char szCommandLine[ MAX_PATH ];
		stuff_in(szCommandLine, syscfg.beginday_c, create_chain_file(), "", "", "", "");
		ExecuteExternalProgram(szCommandLine, GetApplication()->GetSpawnOptions( SPWANOPT_BEGINDAY ) );
	}
	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Purging inactive users (if enabled)...\r\n";
    }
	auto_purge();
	if ( displayStatus )
    {
        GetSession()->bout << "  |#7* |#1Updating user ages...\r\n";
    }
	set_user_age();
    if ( displayStatus )
    {
        GetSession()->bout << "|#7* |#1Done!\r\n";
    }

    sysoplog( "", false );
    sysoplog( "* Ran Daily Maintenance...", false );
    sysoplog( "", false );

}


