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

bool VerifyStatusDates();


void isr1( int nUserNumber, const char *pszName )
{
	int cp = 0;
	while ( cp < status.users &&
            wwiv::stringUtils::StringCompare( pszName, reinterpret_cast<char*>( smallist[cp].name ) ) > 0 )
	{
		++cp;
	}
	for ( int i = status.users; i > cp; i-- )
	{
		smallist[i] = smallist[i - 1];
	}
	smalrec sr;
	strcpy( reinterpret_cast<char*>( sr.name ), pszName );
	sr.number = static_cast<unsigned short>( nUserNumber );
    smallist[cp] = sr;
	++status.users;
}


void reset_files()
{
	WUser user;
	unsigned short users = 0;

	app->statusMgr->Read();
	status.users = 0;
	nl();
	int nNumUsers = app->userManager->GetNumberOfUserRecords();
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
				status.users = users;
                isr1( i, user.GetName() );
				users = status.users;
			}
			else
			{
				memset(&user.data, 0, syscfg.userreclen);
                user.SetInactFlag( 0 );
                user.SetInactFlag( inact_deleted );
			}
            userFile.Seek( pos, WFile::seekBegin );
            userFile.Write( &user.data, syscfg.userreclen );
			if ( ( i % 10 ) == 0 )
			{
                userFile.Close();
				sess->bout << i << "\r ";
                userFile.Open( WFile::modeBinary | WFile::modeReadWrite );
			}
		}
        userFile.Close();
	}
	sess->bout << "\r\n\r\n";

	app->statusMgr->Lock();

    WFile namesFile( syscfg.datadir, NAMES_LST );
    if ( !namesFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeTruncate ) )
	{
        std::cout << namesFile.GetFullPathName() << " NOT FOUND" << std::endl;
		app->AbortBBS( true );
	}
    namesFile.Write( smallist, sizeof(smalrec) * status.users );
    namesFile.Close();
	status.users = users;
	app->statusMgr->Write();
}



void prstatus( bool bIsWFC )
{
	app->statusMgr->Read();
	ClearScreen();
	if ( syscfg.newuserpw[0] != '\0' )
	{
		sess->bout << "|#9New User Pass   : " << syscfg.newuserpw << wwiv::endl;
	}
	sess->bout << "|#9Board is        : " << ( syscfg.closedsystem ? "Closed" : "Open" ) << wwiv::endl;

	if ( !bIsWFC )
	{
		// All of this information is on the WFC Screen
        sess->bout << "|#9Number Users    : |#2" << status.users << wwiv::endl;
		sess->bout << "|#9Number Calls    : |#2" << status.callernum1 << wwiv::endl;
		sess->bout << "|#9Last Date       : |#2" << status.date1 << wwiv::endl;
		sess->bout << "|#9Time            : |#2" << times() << wwiv::endl;
		sess->bout << "|#9Active Today    : |#2" << status.activetoday << wwiv::endl;
		sess->bout << "|#9Calls Today     : |#2" << status.callstoday << wwiv::endl;
		sess->bout << "|#9Net Posts Today : |#2" << ( status.msgposttoday - status.localposts ) << wwiv::endl;
		sess->bout << "|#9Local Post Today: |#2" << status.localposts << wwiv::endl;
		sess->bout << "|#9E Sent Today    : |#2" << status.emailtoday << wwiv::endl;
		sess->bout << "|#9F Sent Today    : |#2" << status.fbacktoday << wwiv::endl;
		sess->bout << "|#9Uploads Today   : |#2" << status.uptoday << wwiv::endl;
		sess->bout << "|#9Feedback Waiting: |#2" << fwaiting << wwiv::endl;
        sess->bout << "|#9Sysop           : |#2" << ( ( sysop2() ) ? "Available" : "NOT Available" ) << wwiv::endl;
	}
	sess->bout << "|#9Q-Scan Pointer  : |#2" << status.qscanptr << wwiv::endl;

    if ( num_instances() > 1 )
	{
		multi_instance();
	}
}


void valuser( int nUserNumber )
{
	char s[81], s1[81], s2[81], s3[81], ar1[20], dar1[20];

	WUser user;
    app->userManager->ReadUser( &user, nUserNumber );
    if ( !user.isUserDeleted() )
	{
		nl();
        sess->bout << "|#9Name: |#2" << user.GetUserNameAndNumber( nUserNumber ) << wwiv::endl;
        sess->bout << "|#9RN  : |#2" << user.GetRealName() << wwiv::endl;
        sess->bout << "|#9PH  : |#2" << user.GetVoicePhoneNumber() << wwiv::endl;
        sess->bout << "|#9Age : |#2" << user.GetAge() << " " << user.GetGender() << wwiv::endl;
        sess->bout << "|#9Comp: |#2" << ctypes( user.GetComputerType() ) << wwiv::endl;
        if ( user.GetNote()[0] )
		{
            sess->bout << "|#9Note: |#2" << user.GetNote() << wwiv::endl;
		}
        sess->bout << "|#9SL  : |#2" << user.GetSl() << wwiv::endl;
		if ( user.GetSl() != 255 && user.GetSl() < sess->GetEffectiveSl() )
		{
            sess->bout << "|#9New : ";
			input( s, 3, true );
			if (s[0])
			{
				int nSl = atoi( s );
				if ( !app->localIO->GetWfcStatus() && nSl >= sess->GetEffectiveSl() )
				{
					nSl = -2;
				}
				if ( nSl >= 0 && nSl < 255 )
				{
					user.SetSl( nSl );
				}
				if ( nSl == -1 )
				{
					nl();
					sess->bout << "|#9Delete? ";
					if ( yesno() )
					{
						deluser( nUserNumber );
						nl();
						sess->bout << "|12Deleted.\r\n\n";
					}
					else
					{
						nl();
						sess->bout << "|13NOT deleted.\r\n";
					}
					return;
				}
			}
		}
		nl();
		sess->bout << "|#9DSL : |#2" << user.GetDsl() << wwiv::endl;
		if ( user.GetDsl() != 255 && user.GetDsl() < sess->thisuser.GetDsl() )
		{
			sess->bout << "|#9New ? ";
			input( s, 3, true );
			if (s[0])
			{
				int nDsl = atoi(s);
				if ( !app->localIO->GetWfcStatus() && nDsl >= sess->thisuser.GetDsl() )
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
			if ( sess->thisuser.hasArFlag( 1 << i ) )
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
			if ( sess->thisuser.hasDarFlag( 1 << i ) )
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
		nl();
		char ch1 = '\0';
		if ( ar2 > 1 )
		{
			do
			{
				sess->bout << "|#9AR  : |#2" << s << wwiv::endl;
				sess->bout << "|#9Togl? ";
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
		nl();
		ch1 = 0;
		if (dar2 > 1)
		{
			do
			{
				sess->bout << "|#9DAR : |#2" << s1 << wwiv::endl;
				sess->bout << "|#9Togl? ";
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
		nl();
		ch1     = 0;
		s[0]    = RETURN;
		s[1]    = '?';
		strcpy(&(s[2]), restrict_string);
		do
		{
			sess->bout << "      |#2" << s3 << wwiv::endl;
			sess->bout << "|#9Rstr: |#2" << s2 << wwiv::endl;
			sess->bout << "|#9Togl? ";
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
        app->userManager->WriteUser( &user, nUserNumber );
		nl();
  }
  else
  {
	  sess->bout << "\r\n|12No Such User.\r\n\n";
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

	app->statusMgr->Read();

	if (!sess->GetMaxNetworkNumber())
	{
		return;
	}

	write_inst(INST_LOC_NETLIST, 0, INST_FLAGS_NONE);

	if ( bForcePause )
	{
		bHadPause  = sess->thisuser.hasPause();
		if ( bHadPause )
		{
            sess->thisuser.toggleStatusFlag( WUser::pauseOnPage );
		}
	}
	bool done = false;
	while (!done && !hangup)
	{
		ClearScreen();
		if (sess->GetMaxNetworkNumber() > 1)
		{
			odc[0] = 0;
			int odci = 0;
			onx[0] = 'Q';
			onx[1] = 0;
			int onxi = 1;
			nl();
			for (i = 0; i < sess->GetMaxNetworkNumber(); i++)
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
                sess->bout << "|#2" << i + 1 << "|#9)|#1 " << net_networks[i].name << wwiv::endl;
			}
			sess->bout << "|#2Q|#9)|#1 Quit\r\n\n";
			sess->bout << "|#9Which network? |#2";
			if (sess->GetMaxNetworkNumber() < 9)
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

			if ( ( i < 0 ) || ( i > sess->GetMaxNetworkNumber() ) )
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

			ClearScreen();
			nl();
            sess->bout << "|#9Network|#2: |#1" << sess->GetNetworkName() << wwiv::endl;
			nl();

			sess->bout << "|#21|#9) = |#1List All\r\n";
			sess->bout << "|#22|#9) = |#1Area Code\r\n";
			sess->bout << "|#23|#9) = |#1Group\r\n";
			sess->bout << "|#24|#9) = |#1Subs Coordinators\r\n";
			sess->bout << "|#25|#9) = |#1Area Coordinators\r\n";
			sess->bout << "|#26|#9) = |#1Group Coordinators\r\n";
			sess->bout << "|#27|#9) = |#1Net Coordinator\r\n";
			sess->bout << "|#28|#9) = |#1BBS Name SubString\r\n";
			sess->bout << "|#29|#9) = |#1Phone SubString\r\n";
			sess->bout << "|#20|#9) = |#1Unconnected Systems\r\n";
			sess->bout << "|#2Q|#9) = |#1Quit NetList\r\n";
			nl();
			sess->bout << "|#9Select: |#2";
			char cmd = onek("Q1234567890");

			switch (cmd)
			{
			case 'Q':
				if (sess->GetMaxNetworkNumber() < 2)
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
				nl();
				sess->bout << "|#1Enter Area Code|#2: |#0";
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
					sess->bout << "|12Area code must be a 3-digit number!\r\n";
					pausescr();
					cmdbit = 0;
				}
				break;
			case '3':
				cmdbit = NET_SEARCH_GROUP;
				nl();
				sess->bout << "|#1Enter group number|#2: |#0";
				input(s, 2);
				if ((s[0] == 0) || (atoi(s) < 1))
				{
					sess->bout << "|12Invalid group number!\r\n";
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
				nl();
				sess->bout << "|#1Enter SubString|#2: |#0";
				input(substr, 40);
				if (substr[0] == 0)
				{
					sess->bout << "|12Enter a substring!\r\n";
					pausescr();
					cmdbit = 0;
				}
				break;
			case '9':
				cmdbit = NET_SEARCH_PHSUBSTR;
				nl();
				sess->bout << "|#1Enter phone substring|#2: |#0";
				input(phstr, 12);
				if (phstr[0] == 0)
				{
					sess->bout << "|12Enter a phone substring!\r\n";
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

			nl();
			sess->bout << "|#1Print BBS region info? ";
			bool useregion = yesno();

            WFile bbsListFile( sess->GetNetworkDataDirectory(), BBSDATA_NET );
            if ( !bbsListFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
			{
                sess->bout << "|12Error opening " << bbsListFile.GetFullPathName() << "!\r\n";
				pausescr();
				continue;
			}
			strcpy(s, "000-000-0000");
			nl( 2 );

			for (i = 0; (i < sess->num_sys_list) && (!abort); i++)
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
		  nl();
		  sess->bout << "|#1Systems Listed |#7: |#2" << slist;
      }
      nl( 2 );
      pausescr();
    }
  }
  if ( bForcePause && bHadPause)
  {
      sess->thisuser.toggleStatusFlag( WUser::pauseOnPage );
  }
}


void read_new_stuff()
{
	zap_bbs_list();
	for ( int i = 0; i < sess->GetMaxNetworkNumber(); i++ )
	{
		set_net_num( i );
		zap_call_out_list();
		zap_contacts();
	}
	set_language_1( sess->GetCurrentLanguageNumber() );
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
                    app->userManager->ReadUser( &user, m.touser );
                    sess->bout << "|#1  To|#7: |#" << sess->GetMessageColor() << user.GetUserNameAndNumber( m.touser ) << wwiv::endl;
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
                    sess->bout << "|#1Subj|#7: |#" << sess->GetMessageColor() << m.title << wwiv::endl;
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
                                    sess->bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0\r\n";
									found = true;
								}
								if ( !found )
								{
									lAttachFileSize = attachDat.Read( &fsr, sizeof( fsr ) );
								}
							}
							if ( !found )
							{
								sess->bout << "|#1Filename|#0.... |#2File : Unknown or Missing|#0\r\n";
							}
							attachDat.Close();
						}
						else
						{
							sess->bout << "|#1Filename|#0.... |#2|#2File : Unknown or Missing|#0\r\n";
						}
					}
					bool next;
					read_message1(&(m.msg), (char) (m.anony & 0x0f), true, &next, "email", m.fromsys, m.fromuser );
					sess->bout << "|#2R,D,Q,<space>  : ";
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
							sess->bout << "Mail file changed; try again.\r\n";
						}
						pFileEmail->Close();
						if ( !sess->IsUserOnline() && m.touser == 1 && m.tosys == 0 )
						{
                            sess->thisuser.SetNumMailWaiting( sess->thisuser.GetNumMailWaiting() - 1 );
						}
					}
					nl( 2 );
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

    sess->bout << "|#9Enter user to change to: ";
    char szUserName[81];
	input( szUserName, 30, true );
	int nUserNumber = finduser1( szUserName );
	if ( nUserNumber > 0 )
	{
		sess->WriteCurrentUser( sess->usernum );
		write_qscn(sess->usernum, qsc, false);
        sess->ReadCurrentUser( nUserNumber );
		read_qscn(nUserNumber, qsc, false);
		sess->usernum = static_cast<unsigned short>( nUserNumber );
		sess->SetEffectiveSl( 255 );
		sysoplogf( "#*#*#* Changed to %s", sess->thisuser.GetUserNameAndNumber( sess->usernum ) );
		changedsl();
		app->localIO->UpdateTopScreen();
	}
	else
	{
		sess->bout << "|12Unknown user.\r\n";
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
	nl();
	ClearScreen();
    nl( 2 );
	pla("|#2  Date     Calls  Active   Posts   Email   Fback    U/L    %Act   T/user", &abort);
	pla("|#7--------   -----  ------   -----   -----   -----    ---    ----   ------", &abort);
	while ((i < 97) && (!abort) && (!hangup) && (z.date[0] != 0))
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
			sess->bout << "|#1";
			pla(szBuffer, &abort);
		}
		else
		{
			sess->bout << "|13";
			pla(szBuffer, &abort);
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
	unsigned int nUserNumber = 1;
	do
	{
    	WUser user;
        app->userManager->ReadUser( &user, nUserNumber );
        int nAge = years_old( user.GetBirthdayMonth(), user.GetBirthdayDay(), user.GetBirthdayYear() );
        if ( nAge != user.GetAge() )
		{
            user.SetAge( nAge );
            app->userManager->WriteUser( &user, nUserNumber );
		}
		++nUserNumber;
	} while ( nUserNumber <= status.users );
}


void auto_purge()
{
	char s[80], *ss;
	unsigned int days = 0;
	int skipsl = 0;

	if (ini_init(WWIV_INI, INI_TAG, NULL))
	{
		if ((ss = ini_get("AUTO_USER_PURGE", -1, NULL)) != NULL)
		{
			days = atoi(ss);
		}
		ini_done();
	}
	if (ini_init(WWIV_INI, INI_TAG, NULL))
	{
		if ((ss = ini_get("NO_PURGE_SL", -1, NULL)) != NULL)
		{
			skipsl = atoi(ss);
		}
		ini_done();
	}

	if (days < 60)
	{
        if ( days > 0 )
        {
            sysoplog( "!!! WARNING: Auto-Purge canceled [AUTO_USER_PURGE < 60]", false );
            sysoplog( "!!! WARNING: Edit WWIV.INI and Fix this", false );
        }
		return;
	}

	long lTime = time( NULL );
	unsigned int nUserNumber = 1;
	sysoplogfi( false, "Auto-Purged Inactive Users (over %d days, SL less than %d)", days, skipsl );

	do
	{
        WUser user;
        app->userManager->ReadUser( &user, nUserNumber );
        if ( !user.isExemptAutoDelete() )
		{
            unsigned int d = static_cast<unsigned int>( ( lTime - user.GetLastOnDateNumber() ) / SECONDS_PER_DAY_FLOAT );
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
	} while ( nUserNumber <= status.users );
}


void beginday( bool displayStatus )
{
    if ( ( sess->GetBeginDayNodeNumber() > 0 ) && ( app->GetInstanceNumber() != sess->GetBeginDayNodeNumber() ) )
	{
        // If BEGINDAYNODENUMBER is > 0 or defined in WWIV.INI only handle beginday events on that node number
		app->statusMgr->Read();
		return;
	}
	app->statusMgr->Lock();

	// TODO remove this hack once Dean fixes INIT
	// If the date1 field is hosed, fix up the year to 00 (since it would be 10 as in part of 100).
	if ( status.date1[8] != '\0' )
	{
		status.date1[6] = '0';
		status.date1[7] = '0';
		status.date1[8] = '\0'; // forgot to add null termination
	}

    if ( wwiv::stringUtils::IsEquals( date(), status.date1 ) )
	{
		app->statusMgr->Write();
		return;
	}
	sess->bout << "|#7* |#1Running Daily Maintenance...\r\n";
	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Updating system activity...\r\n";
    }

	zlogrec z;
	strcpy( z.date, status.date1 );
	z.active            = status.activetoday;
	z.calls             = status.callstoday;
	z.posts             = status.localposts;
	z.email             = status.emailtoday;
	z.fback             = status.fbacktoday;
	z.up                = status.uptoday;
	status.callstoday   = 0;
	status.msgposttoday = 0;
	status.localposts   = 0;
	status.emailtoday   = 0;
	status.fbacktoday   = 0;
	status.uptoday      = 0;
	status.activetoday  = 0;
	status.days++;

	//
	// Need to verify the dates aren't trashed otherwise we can crash here.
	//
	VerifyStatusDates();

	strcpy( status.date3, status.date2 );
	strcpy( status.date2, status.date1 );
	strcpy( status.date1, date() );
	strcpy( status.log2, status.log1 );
	slname( status.date2, status.log1 );

	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Cleaning up log files...\r\n";
    }
	WFile::Remove( syscfg.gfilesdir, status.log2 );
	WFile::Remove( syscfg.gfilesdir, USER_LOG );

	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Updating ZLOG information...\r\n";
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
        sess->bout << "  |#7* |#1Updating STATUS.DAT...\r\n";
    }
	app->statusMgr->Write();
	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Checking system directories and user space...\r\n";
    }

	double fk   = freek1(syscfg.datadir);
	int nus     = syscfg.maxusers - status.users;

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
		ExecuteExternalProgram(szCommandLine, app->GetSpawnOptions( SPWANOPT_BEGINDAY ) );
	}
	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Purging inactive users (if enabled)...\r\n";
    }
	auto_purge();
	if ( displayStatus )
    {
        sess->bout << "  |#7* |#1Updating user ages...\r\n";
    }
	set_user_age();
    if ( displayStatus )
    {
        sess->bout << "|#7* |#1Done!\r\n";
    }

    sysoplog( "", false );
    sysoplog( "* Ran Daily Maintenance...", false );
    sysoplog( "", false );

}


bool VerifyStatusDates()
{
	bool bReturn = true;
    std::string currentDate = date();

	if ( status.date3[8] != '\0' )
	{
		status.date3[6] = currentDate[6];
		status.date3[7] = currentDate[7];
		status.date3[8] = '\0';
	}
	if ( status.date2[8] != '\0' )
	{
		status.date2[6] = currentDate[6];
		status.date2[7] = currentDate[7];
		status.date2[8] = '\0';
	}
	if ( status.date1[8] != '\0' )
	{
		status.date1[6] = currentDate[6];
		status.date1[7] = currentDate[7];
		status.date1[8] = '\0';
	}
	if ( status.gfiledate[8] != '\0' )
	{
		status.gfiledate[6] = currentDate[6];
		status.gfiledate[7] = currentDate[7];
		status.gfiledate[8] = '\0';
	}

	return bReturn;
}

