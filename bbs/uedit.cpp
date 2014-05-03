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
#include "printfile.h"


static unsigned long *u_qsc = 0;
static char *sp = NULL;
static char	search_pattern[81];
char *daten_to_date(time_t dt);

int  matchuser( int nUserNumber );
int  matchuser( WUser * pUser );
void changeopt();


void deluser( int nUserNumber ) {
	WUser user;
	GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );

	if ( !user.IsUserDeleted() ) {
		rsm( nUserNumber, &user, false );
		DeleteSmallRecord( user.GetName() );
		user.SetInactFlag( WUser::userDeleted );
		user.SetNumMailWaiting( 0 );
		GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
		WFile *pFileEmail = OpenEmailFile( true );
		WWIV_ASSERT( pFileEmail );
		if ( pFileEmail->IsOpen() ) {
			long lEmailFileLen = pFileEmail->GetLength() / sizeof(mailrec);
			for ( int nMailRecord = 0; nMailRecord < lEmailFileLen; nMailRecord++ ) {
				mailrec m;

				pFileEmail->Seek( nMailRecord * sizeof( mailrec ), WFile::seekBegin );
				pFileEmail->Read( &m, sizeof( mailrec ) );
				if ( ( m.tosys == 0 && m.touser == nUserNumber ) ||
				        ( m.fromsys == 0 && m.fromuser == nUserNumber ) ) {
					delmail( pFileEmail, nMailRecord );
				}
			}
			pFileEmail->Close();
			delete pFileEmail;
		}
		WFile voteFile( syscfg.datadir, VOTING_DAT );
		voteFile.Open( WFile::modeReadWrite | WFile::modeBinary, WFile::shareUnknown, WFile::permReadWrite );
		long nNumVoteRecords = static_cast<int> (voteFile.GetLength() / sizeof(votingrec)) - 1;
		for (long lCurVoteRecord = 0; lCurVoteRecord < 20; lCurVoteRecord++) {
			if ( user.GetVote( lCurVoteRecord ) ) {
				if ( lCurVoteRecord <= nNumVoteRecords ) {
					votingrec v;
					voting_response vr;

					voteFile.Seek( static_cast<long>( lCurVoteRecord * sizeof( votingrec ) ), WFile::seekBegin );
					voteFile.Read( &v, sizeof( votingrec ) );
					vr = v.responses[ user.GetVote( lCurVoteRecord ) - 1 ];
					vr.numresponses--;
					v.responses[ user.GetVote( lCurVoteRecord ) - 1 ] = vr;
					voteFile.Seek( static_cast<long>( lCurVoteRecord * sizeof( votingrec ) ), WFile::seekBegin );
					voteFile.Write( &v, sizeof( votingrec ) );
				}
				user.SetVote( lCurVoteRecord, 0 );
			}
		}
		voteFile.Close();
		GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
		delete_phone_number( nUserNumber, user.GetVoicePhoneNumber() ); // dupphone addition
		delete_phone_number( nUserNumber, user.GetDataPhoneNumber() );  // dupphone addition
	}
}


void print_data(int nUserNumber, WUser *pUser, bool bLongFormat, bool bClearScreen) {
	char s[81], s1[81], s2[81], s3[81];

	if ( bClearScreen ) {
		GetSession()->bout.ClearScreen();
	}
	if ( pUser->IsUserDeleted() ) {
		GetSession()->bout << "|#6>>> This User Is DELETED - Use 'R' to Restore <<<\r\n\n";
	}
	GetSession()->bout << "|#2N|#9) Name/Alias   : |#1" << pUser->GetName() << " #" << nUserNumber << wwiv::endl;
	GetSession()->bout << "|#2L|#9) Real Name    : |#1" << pUser->GetRealName() << wwiv::endl;
	if (bLongFormat) {
		if (pUser->GetStreet()[0]) {
			GetSession()->bout << "|#2%%|#9) Address      : |#1" << pUser->GetStreet() << wwiv::endl;
		}
		if ( pUser->GetCity()[0] || pUser->GetState()[0] ||
		        pUser->GetCountry()[0] || pUser->GetZipcode()[0] ) {
			GetSession()->bout << "|#9   City         : |#1" << pUser->GetCity() << ", " <<
			                   pUser->GetState() << "  " << pUser->GetZipcode() << " |#9(|#1" <<
			                   pUser->GetCountry() << "|#9)\r\n";
		}
	}

	if ( pUser->GetRegisteredDateNum() != 0 ) {
		GetSession()->bout << "|#2X|#9) Registration : |#1" << daten_to_date( pUser->GetRegisteredDateNum() ) <<
		                   "       Expires on   : " << daten_to_date( pUser->GetExpiresDateNum() ) << wwiv::endl;
	}

	if (  pUser->GetCallsign()[0] != '\0' ) {
		GetSession()->bout << "|#9C) Call         : |#1" << pUser->GetCallsign() << wwiv::endl;
	}

	GetSession()->bout << "|#2P|#9) Voice Phone #: |#1" << pUser->GetVoicePhoneNumber() << wwiv::endl;
	if ( pUser->GetDataPhoneNumber()[0] != '\0' ) {
		GetSession()->bout << "|#9   Data Phone # : |#1" << pUser->GetDataPhoneNumber() << wwiv::endl;
	}

	GetSession()->bout.WriteFormatted( "|#2G|#9) Birthday/Age : |#1(%02d/%02d/%02d) (Age: %d) (Gender: %c) \r\n",
	                                   pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(),
	                                   pUser->GetBirthdayYear(), pUser->GetAge(), pUser->GetGender() );

	GetSession()->bout << "|#2M|#9) Comp         : |#1" << ( pUser->GetComputerType() == -1 ? "Waiting for answer" : ctypes ( pUser->GetComputerType() ) ) << wwiv::endl;
	if ( pUser->GetForwardUserNumber() != 0 ) {
		GetSession()->bout << "|#9   Forwarded To : |#1";
		if ( pUser->GetForwardSystemNumber() != 0 ) {
			if ( GetSession()->GetMaxNetworkNumber() > 1 ) {
				GetSession()->bout << net_networks[ pUser->GetForwardNetNumber() ].name <<
				                   " #" << pUser->GetForwardUserNumber() <<
				                   " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
			} else {
				GetSession()->bout << "#" << pUser->GetForwardUserNumber() << " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
			}
		} else {
			GetSession()->bout << "#" << pUser->GetForwardUserNumber() << wwiv::endl;
		}
	}
	if (bLongFormat) {

		GetSession()->bout << "|#2H|#9) Password     : |#1";
		if ( AllowLocalSysop() ) {
			GetSession()->localIO()->LocalPuts( pUser->GetPassword() );
		}

		if ( incom && GetSession()->GetCurrentUser()->GetSl() == 255 ) {
			rputs( pUser->GetPassword() );
		} else {
			rputs( "(not shown remotely)" );
		}
		GetSession()->bout.NewLine();

		GetSession()->bout << "|#9   First/Last On: |#9(Last: |#1" << pUser->GetLastOn() << "|#9)   (First: |#1" << pUser->GetFirstOn() << "|#9)\r\n";

		GetSession()->bout.WriteFormatted("|#9   Message Stats: |#9(Post:|#1%u|#9) (Email:|#1%u|#9) (Fd:|#1%u|#9) (Wt:|#1%u|#9) (Net:|#1%u|#9) (Del:|#1%u|#9)\r\n",
		                                  pUser->GetNumMessagesPosted(), pUser->GetNumEmailSent(),
		                                  pUser->GetNumFeedbackSent(), pUser->GetNumMailWaiting(), pUser->GetNumNetEmailSent(), pUser->GetNumDeletedPosts() );

		GetSession()->bout.WriteFormatted("|#9   Call Stats   : |#9(Total: |#1%u|#9) (Today: |#1%d|#9) (Illegal: |#6%d|#9)\r\n",
		                                  pUser->GetNumLogons(), ( !wwiv::strings::IsEquals( pUser->GetLastOn(), date() ) ) ? 0 : pUser->GetTimesOnToday(), pUser->GetNumIllegalLogons() );

		GetSession()->bout.WriteFormatted("|#9   Up/Dnld Stats: |#9(Up: |#1%u |#9files in |#1%lu|#9k)  (Dn: |#1%u |#9files in |#1%lu|#9k)\r\n",
		                                  pUser->GetFilesUploaded(), pUser->GetUploadK(), pUser->GetFilesDownloaded(), pUser->GetDownloadK() );

		GetSession()->bout << "|#9   Last Baud    : |#1" << pUser->GetLastBaudRate() << wwiv::endl;
	}
	if ( pUser->GetNote()[0] != '\0' ) {
		GetSession()->bout << "|#2O|#9) Sysop Note   : |#1" << pUser->GetNote() << wwiv::endl;
	}
	if ( pUser->GetAssPoints() ) {
		GetSession()->bout << "|#9   Ass Points   : |#1" << pUser->GetAssPoints() << wwiv::endl;
	}
	GetSession()->bout << "|#2S|#9) SL           : |#1" << pUser->GetSl() << wwiv::endl;
	GetSession()->bout << "|#2T|#9) DSL          : |#1" << pUser->GetDsl() << wwiv::endl;
	if ( u_qsc ) {
		if ( ( *u_qsc ) != 999) {
			GetSession()->bout << "|#9    Sysop Sub #: |#1" << *u_qsc << wwiv::endl;
		}
	}
	if ( pUser->GetExempt() != 0 ) {
		GetSession()->bout.WriteFormatted( "|#9   Exemptions   : |#1%s %s %s %s %s (%d)\r\n",
		                                   pUser->IsExemptRatio() ? "XFER" : "    ",
		                                   pUser->IsExemptTime() ? "TIME" : "    ",
		                                   pUser->IsExemptPost() ? "POST" : "    ",
		                                   pUser->IsExemptAll() ? "ALL " : "    ",
		                                   pUser->IsExemptAutoDelete()? "ADEL" : "    ",
		                                   pUser->GetExempt() );
	}
	strcpy(s3, restrict_string);
	for (int i = 0; i <= 15; i++) {
		if (pUser->HasArFlag(1 << i)) {
			s[i] = static_cast<char>( 'A' + i );
		} else {
			s[i] = SPACE;
		}
		if (pUser->HasDarFlag(1 << i)) {
			s1[i] = static_cast<char>( 'A' + i );
		} else {
			s1[i] = SPACE;
		}
		if ( pUser->GetRestriction() & ( 1 << i ) ) {
			s2[i] = s3[i];
		} else {
			s2[i] = SPACE;
		}
	}
	s[16]   = '\0';
	s1[16]  = '\0';
	s2[16]  = '\0';
	if ( pUser->GetAr() != 0 ) {
		GetSession()->bout << "|#2A|#9) AR Flags     : |#1" << s << wwiv::endl;
	}
	if ( pUser->GetDar() != 0 ) {
		GetSession()->bout << "|#2I|#9) DAR Flags    : |#1" << s1 << wwiv::endl;
	}
	if ( pUser->GetRestriction() != 0 ) {
		GetSession()->bout << "|#2Z|#9) Restrictions : |#1" << s2 << wwiv::endl;
	}
	if ( pUser->GetWWIVRegNumber() ) {
		GetSession()->bout << "|#9   WWIV Reg Num : |#1" << pUser->GetWWIVRegNumber() << wwiv::endl;
	}
	// begin callback changes

	if (bLongFormat) {
		print_affil( pUser );
		if ( GetApplication()->HasConfigFlag( OP_FLAGS_CALLBACK ) ) {
			GetSession()->bout.WriteFormatted( "|#1User has%s been callback verified.  ",
			                                   ( pUser->GetCbv() & 1) == 0 ? " |#6not" : "");
		}
		if ( GetApplication()->HasConfigFlag( OP_FLAGS_VOICE_VAL ) ) {
			GetSession()->bout.WriteFormatted( "|#1User has%s been voice verified.",
			                                   ( pUser->GetCbv() & 2) == 0 ? " |#6not" : "");
		}
		GetSession()->bout.NewLine( 2 );
	}
	// end callback changes
}


int matchuser(int nUserNumber) {
	WUser user;
	GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
	sp = search_pattern;
	return matchuser( &user );
}


int matchuser( WUser *pUser ) {
	int ok = 1, _not = 0, less = 0, cpf = 0, cpp = 0;
	int  _and = 1, gotfcn = 0, evalit = 0, tmp, tmp1, tmp2;
	char fcn[20], parm[80], ts[40];
	time_t l;

	bool done = false;
	do {
		if (*sp == 0) {
			done = true;
		} else {
			if (strchr("()|&!<>", *sp)) {
				switch (*sp++) {
				case '(':
					evalit = 2;
					break;
				case ')':
					done = true;
					break;
				case '|':
					_and = 0;
					break;
				case '&':
					_and = 1;
					break;
				case '!':
					_not = 1;
					break;
				case '<':
					less = 1;
					break;
				case '>':
					less = 0;
					break;
				}
			} else if (*sp == '[') {
				gotfcn = 1;
				sp++;
			} else if (*sp == ']') {
				evalit = 1;
				++sp;
			} else if ( *sp != ' ' || gotfcn ) {
				if (gotfcn) {
					if (cpp < 22) {
						parm[cpp++] = *sp++;
					} else {
						sp++;
					}
				} else {
					if ( cpf < static_cast<signed int>( sizeof( fcn ) ) - 1 ) {
						fcn[ cpf++ ] = *sp++;
					} else {
						sp++;
					}
				}
			} else {
				++sp;
			}
			if (evalit) {
				if (evalit == 1) {
					fcn[cpf] = 0;
					parm[cpp] = 0;
					tmp = 1;
					tmp1 = atoi(parm);

					if ( wwiv::strings::IsEquals( fcn, "SL" ) ) {
						if (less) {
							tmp = (tmp1 > pUser->GetSl() );
						} else {
							tmp = ( tmp1 < pUser->GetSl() );
						}
					} else if ( wwiv::strings::IsEquals( fcn, "DSL" ) ) {
						if (less) {
							tmp = ( tmp1 > pUser->GetDsl() );
						} else {
							tmp = (tmp1 < pUser->GetDsl() );
						}
					} else if ( wwiv::strings::IsEquals( fcn, "AR" ) ) {
						if ( parm[0] >= 'A' && parm[0] <= 'P' ) {
							tmp1 = 1 << (parm[0] - 'A');
							tmp = pUser->HasArFlag( tmp1 ) ? 1 : 0;
						} else {
							tmp = 0;
						}
					} else if ( wwiv::strings::IsEquals( fcn, "DAR" ) ) {
						if ((parm[0] >= 'A') && (parm[0] <= 'P')) {
							tmp1 = 1 << (parm[0] - 'A');
							tmp = pUser->HasDarFlag( tmp1 ) ? 1 : 0;
						} else {
							tmp = 0;
						}
					} else if ( wwiv::strings::IsEquals( fcn, "SEX" ) ) {
						tmp = parm[0] == pUser->GetGender();
					} else if ( wwiv::strings::IsEquals( fcn, "AGE" ) ) {
						if (less) {
							tmp = ( tmp1 > pUser->GetAge() );
						} else {
							tmp = ( tmp1 < pUser->GetAge() );
						}
					} else if ( wwiv::strings::IsEquals( fcn, "LASTON" ) ) {
						time(&l);
						tmp2 = static_cast<unsigned int>( ( l - pUser->GetLastOnDateNumber() ) / HOURS_PER_DAY_FLOAT / SECONDS_PER_HOUR_FLOAT );
						if (less) {
							tmp = tmp2 < tmp1;
						} else {
							tmp = tmp2 > tmp1;
						}
					} else if ( wwiv::strings::IsEquals( fcn, "AREACODE" ) ) {
						tmp = !strncmp( parm, pUser->GetVoicePhoneNumber(), 3 );
					} else if ( wwiv::strings::IsEquals( fcn, "RESTRICT" ) ) {
						;
					} else if ( wwiv::strings::IsEquals( fcn, "LOGONS" ) ) {
						if (less) {
							tmp = pUser->GetNumLogons() < tmp1;
						} else {
							tmp = pUser->GetNumLogons() > tmp1;
						}
					} else if ( wwiv::strings::IsEquals( fcn, "REALNAME" ) ) {
						strcpy( ts, pUser->GetRealName() );
						WWIV_STRUPR( ts );
						tmp = ( strstr( ts, parm ) != NULL );
					} else if ( wwiv::strings::IsEquals( fcn, "BAUD" ) ) {
						if (less) {
							tmp = pUser->GetLastBaudRate() < tmp1;
						} else {
							tmp = pUser->GetLastBaudRate() > tmp1;
						}

						// begin callback additions

					} else if ( wwiv::strings::IsEquals( fcn, "CBV" ) ) {
						if (less) {
							tmp = pUser->GetCbv() < tmp1;
						} else {
							tmp = pUser->GetCbv() > tmp1;
						}

						// end callback additions

					} else if ( wwiv::strings::IsEquals( fcn, "COMP_TYPE" ) ) {
						tmp = pUser->GetComputerType() == tmp1;
					}
				} else {
					tmp = matchuser( pUser );
				}

				if (_not) {
					tmp = !tmp;
				}
				if (_and) {
					ok = ok && tmp;
				} else {
					ok = ok || tmp;
				}

				_not = less = cpf = cpp = gotfcn = evalit = 0;
				_and = 1;
			}
		}
	} while ( !done );
	return ok;
}


void changeopt() {
	GetSession()->bout.ClearScreen();
	GetSession()->bout << "Current search string:\r\n";
	GetSession()->bout << ( (search_pattern[0]) ? search_pattern : "-NONE-" );
	GetSession()->bout.NewLine( 3 );
	GetSession()->bout << "|#9Change it? ";
	if (yesno()) {
		GetSession()->bout << "Enter new search pattern:\r\n|#7:";
		input(search_pattern, 75);
	}
}


void auto_val( int n, WUser *pUser ) {
	if ( pUser->GetSl() == 255 ) {
		return;
	}
	pUser->SetSl( syscfg.autoval[n].sl );
	pUser->SetDsl( syscfg.autoval[n].dsl );
	pUser->SetAr( syscfg.autoval[n].ar );
	pUser->SetDar( syscfg.autoval[n].dar );
	pUser->SetRestriction( syscfg.autoval[n].restrict );
}


/**
 * Online User Editor.
 *
 * @param usern User Number to edit
 * @param other UEDIT_NONE, UEDIT_FULLINFO, UEDIT_CLEARSCREEN
 */
void uedit( int usern, int other ) {
	char s[81];
	bool bClearScreen = true;
	WUser user;

	u_qsc = static_cast<unsigned long *>( BbsAllocA( syscfg.qscn_len ) );

	bool full = ( incom ) ? false : true;
	if (other & 1) {
		full = false;
	}
	if (other & 2) {
		bClearScreen = false;
	}
	int nUserNumber = usern;
	bool bDoneWithUEdit = false;
	GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
	int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();
	do {
		GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
		read_qscn( nUserNumber, u_qsc, false );
		bool bDoneWithUser = false;
		bool temp_full = false;
		do {
			print_data(nUserNumber, &user, ( full || temp_full ) ? true : false, bClearScreen);
			GetSession()->bout.NewLine();
			GetSession()->bout << "|#9(|#2Q|#9=|#1Quit, |#2?|#9=|#1Help|#9) User Editor Command: ";
			char ch = 0;
			if ( GetSession()->GetCurrentUser()->GetSl() == 255 || GetApplication()->GetWfcStatus() ) {
				ch = onek( "ACDEFGHILMNOPQRSTUVWXYZ0123456789[]{}/,.?~%:", true );
			} else {
				ch = onek( "ACDEFGHILMNOPQRSTUWYZ0123456789[]{}/,.?%", true );
			}
			switch (ch) {
			case 'A': {
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7Toggle which AR? ";
				char ch1 = onek("\rABCDEFGHIJKLMNOP");
				if (ch1 != RETURN) {
					ch1 -= 'A';
					if ( GetApplication()->GetWfcStatus() || (GetSession()->GetCurrentUser()->HasArFlag(1 << ch1))) {
						user.ToggleArFlag( 1 << ch1 );
						GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
					}
				}
			}
			break;
			case 'C':
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New callsign? ";
				input( s, 6 );
				if ( s[0] ) {
					user.SetCallsign( s );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				} else {
					GetSession()->bout << "|#5Delete callsign? ";
					if (yesno()) {
						user.SetCallsign( "" );
						GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
					}
				}
				break;
			case 'D':
				if (nUserNumber != 1) {
					if ( !user.IsUserDeleted() && GetSession()->GetEffectiveSl() > user.GetSl() ) {
						GetSession()->bout << "|#5Delete? ";
						if (yesno()) {
							deluser(nUserNumber);
							GetApplication()->GetUserManager()->ReadUser( &user, nUserNumber );
						}
					}
				}
				break;
			case 'E': {
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New Exemption? ";
				input(s, 3);
				int nExemption = atoi(s);
				if ( nExemption >= 0 && nExemption <= 255 && s[0] ) {
					user.SetExempt( nExemption );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'F': {
				int nNetworkNumber = getnetnum( "FILEnet" );
				GetSession()->SetNetworkNumber( nNetworkNumber );
				if ( nNetworkNumber != -1 ) {
					set_net_num( GetSession()->GetNetworkNumber() );
					GetSession()->bout << "Current Internet Address\r\n:" << user.GetEmailAddress() << wwiv::endl;
					GetSession()->bout << "New Address\r\n:";
					inputl( s, 75 );
					if ( s[0] != '\0' ) {
						if ( check_inet_addr( s ) ) {
							user.SetEmailAddress( s );
							write_inet_addr( s, nUserNumber );
							user.SetForwardNetNumber( GetSession()->GetNetworkNumber() );
						} else {
							GetSession()->bout.NewLine();
							GetSession()->bout << "Invalid format.\r\n";
						}
					} else {
						GetSession()->bout.NewLine();
						GetSession()->bout << "|#5Remove current address? ";
						if (yesno()) {
							user.SetEmailAddress( "" );
						}
					}
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'G':
				GetSession()->bout << "|#5Are you sure you want to re-enter the birthday? ";
				if (yesno()) {
					GetSession()->bout.NewLine();
					GetSession()->bout.WriteFormatted( "Current birthdate: %02d/%02d/%02d\r\n",
					                                   user.GetBirthdayMonth(), user.GetBirthdayDay(), user.GetBirthdayYear() );
					input_age( &user );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				break;
			case 'H':
				GetSession()->bout << "|#5Change this user's password? ";
				if (yesno()) {
					input_pw( &user );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				break;
			case 'I': {
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7Toggle which DAR? ";
				char ch1 = onek("\rABCDEFGHIJKLMNOP");
				if (ch1 != RETURN) {
					ch1 -= 'A';
					if ( GetApplication()->GetWfcStatus() || (GetSession()->GetCurrentUser()->HasDarFlag(1 << ch1))) {
						user.ToggleDarFlag( 1 << ch1 );
						GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
					}
				}
			}
			break;
			case 'L': {
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New FULL real name? ";
				std::string realName;
				Input1( realName, user.GetRealName(), 20, true, INPUT_MODE_FILE_PROPER );
				if ( !realName.empty() ) {
					user.SetRealName( realName.c_str() );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'M': {
				int nNumCompTypes = 0;
				GetSession()->bout.NewLine();
				GetSession()->bout << "Known computer types:\r\n\n";
				for ( int nCurCompType = 0; ctypes( nCurCompType ); nCurCompType++ ) {
					GetSession()->bout << nCurCompType << ". " << ctypes( nCurCompType ) << wwiv::endl;
					nNumCompTypes++;
				}
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7Enter new computer type: ";
				input(s, 2);
				int nComputerType = atoi(s);
				if ( nComputerType > 0 && nComputerType <= nNumCompTypes ) {
					user.SetComputerType( nComputerType );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'N':
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New name? ";
				input( s, 30 );
				if (s[0]) {
					if (finduser(s) < 1) {
						DeleteSmallRecord( user.GetName() );
						user.SetName( s );
						InsertSmallRecord( nUserNumber, user.GetName() );
						GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
					}
				}
				break;
			case 'O':
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New note? ";
				inputl( s, 60 );
				user.SetNote( s );
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				break;

			case 'P': {
				bool bWriteUser = false;
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New phone number? ";
				std::string phoneNumber;
				Input1( phoneNumber, user.GetVoicePhoneNumber(), 12, true, INPUT_MODE_PHONE );
				if ( !phoneNumber.empty() ) {
					if ( phoneNumber != user.GetVoicePhoneNumber() ) {
						delete_phone_number( nUserNumber, user.GetVoicePhoneNumber() );
						add_phone_number( nUserNumber, phoneNumber.c_str() );
					}
					user.SetVoicePhoneNumber( phoneNumber.c_str() );
					bWriteUser = true;
				}
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New DataPhone (0=none)? ";
				Input1( phoneNumber, user.GetDataPhoneNumber(), 12, true, INPUT_MODE_PHONE );
				if ( !phoneNumber.empty() ) {
					if ( phoneNumber[0] == '0') {
						user.SetDataPhoneNumber( "" );
					} else {
						if ( phoneNumber != user.GetDataPhoneNumber() ) {
							delete_phone_number( nUserNumber, user.GetDataPhoneNumber() );
							add_phone_number( nUserNumber, phoneNumber.c_str() );
						}
						user.SetDataPhoneNumber( phoneNumber.c_str() );
					}
					bWriteUser = true;
				}
				if (bWriteUser) {
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'V': {
				bool bWriteUser = false;
				if ( GetApplication()->HasConfigFlag( OP_FLAGS_CALLBACK ) ) {
					GetSession()->bout << "|#7Toggle callback verify flag (y/N) ? ";
					if (yesno()) {
						if ( user.GetCbv() & 1 ) {
							GetSession()->GetCurrentUser()->SetSl( syscfg.newusersl );
							GetSession()->GetCurrentUser()->SetDsl( syscfg.newuserdsl );
							GetSession()->GetCurrentUser()->SetRestriction( syscfg.newuser_restrict );
							user.SetExempt( 0 );
							user.SetAr( 0 );
							user.SetDar( 0 );
							user.SetCbv( user.GetCbv() - 1 );
						} else {
							if ( user.GetSl() < GetSession()->cbv.sl ) {
								user.SetSl( GetSession()->cbv.sl );
							}
							if ( user.GetDsl() < GetSession()->cbv.dsl ) {
								user.SetDsl( GetSession()->cbv.dsl );
							}
							user.SetRestriction( user.GetRestriction() | GetSession()->cbv.restrict );
							user.SetExempt( user.GetExempt() | GetSession()->cbv.exempt );
							user.SetArFlag( GetSession()->cbv.ar );
							user.SetDarFlag( GetSession()->cbv.dar );
							user.SetCbv( user.GetCbv() | 1 );
						}
						bWriteUser = true;
					}
				}
				if ( GetApplication()->HasConfigFlag( OP_FLAGS_VOICE_VAL ) ) {
					GetSession()->bout << "|#7Toggle voice validated flag (y/N) ? ";
					if (yesno()) {
						if ( user.GetCbv() & 2 ) {
							user.SetCbv( user.GetCbv() - 2 );
						} else {
							user.SetCbv( user.GetCbv() | 2 );
						}
						bWriteUser = true;
					}
				}
				if (bWriteUser) {
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'Q':
				bDoneWithUEdit = true;
				bDoneWithUser = true;
				break;
			case 'R':
				if ( user.IsUserDeleted() ) {
					user.ToggleInactFlag( WUser::userDeleted );
					InsertSmallRecord( nUserNumber, user.GetName() );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );

					// begin dupphone additions

					if ( user.GetVoicePhoneNumber()[0] ) {
						add_phone_number( nUserNumber, user.GetVoicePhoneNumber() );
					}
					if ( user.GetDataPhoneNumber()[0] &&
					        !wwiv::strings::IsEquals( user.GetVoicePhoneNumber(),  user.GetDataPhoneNumber()  ) ) {
						add_phone_number( nUserNumber, user.GetDataPhoneNumber() );
					}

					// end dupphone additions

				}
				break;
			case 'S': {
				if ( user.GetSl() >= GetSession()->GetEffectiveSl() ) {
					break;
				}
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New SL? ";
				std::string sl;
				input( sl, 3 );
				int nNewSL = atoi( sl.c_str() );
				if ( !GetApplication()->GetWfcStatus() && nNewSL >= GetSession()->GetEffectiveSl() && nUserNumber != 1 ) {
					GetSession()->bout << "|#6You can not assign a Security Level to a user that is higher than your own.\r\n";
					pausescr();
					nNewSL = -1;
				}
				if ( nNewSL >= 0 && nNewSL < 255 && sl[0] ) {
					user.SetSl( nNewSL );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
					if ( nUserNumber == GetSession()->usernum ) {
						GetSession()->SetEffectiveSl( nNewSL );
					}
				}
			}
			break;
			case 'T': {
				if ( user.GetDsl() >= GetSession()->GetCurrentUser()->GetDsl() ) {
					break;
				}
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New DSL? ";
				std::string dsl;
				input( dsl, 3 );
				int nNewDSL = atoi( dsl.c_str() );
				if ( !GetApplication()->GetWfcStatus() && nNewDSL >= GetSession()->GetCurrentUser()->GetDsl() && nUserNumber != 1 ) {
					GetSession()->bout << "|#6You can not assign a Security Level to a user that is higher than your own.\r\n";
					pausescr();
					nNewDSL = -1;
				}
				if ( nNewDSL >= 0 && nNewDSL < 255 && dsl[0] ) {
					user.SetDsl( nNewDSL );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case 'U': {
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7User name/number: ";
				std::string name;
				input( name, 30 );
				int nFoundUserNumber = finduser1( name.c_str() );
				if (nFoundUserNumber > 0) {
					nUserNumber = nFoundUserNumber;
					bDoneWithUser = true;
				}
			}
			break;
			// begin callback additions
			case 'W':
				wwivnode( &user, 1 );
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				break;
			// end callback additions
			case 'X': {
				std::string regDate, expDate;
				if ( !GetApplication()->HasConfigFlag( OP_FLAGS_USER_REGISTRATION ) ) {
					break;
				}
				GetSession()->bout.NewLine();
				if ( user.GetRegisteredDateNum() != 0) {
					regDate = daten_to_date( user.GetRegisteredDateNum() );
					expDate = daten_to_date( user.GetExpiresDateNum() );
					GetSession()->bout << "Registered on " << regDate << ", expires on " << expDate << wwiv::endl;
				} else {
					GetSession()->bout << "Not registered.\r\n";
				}
				std::string newRegDate;
				do {
					GetSession()->bout.NewLine();
					GetSession()->bout << "Enter registration date, <CR> for today: \r\n";
					GetSession()->bout << " MM/DD/YY\r\n:";
					input( newRegDate, 8 );
				} while ( newRegDate.length() != 8 && !newRegDate.empty() );

				if ( newRegDate.empty() ) {
					newRegDate = date();
				}

				int m = atoi( newRegDate.c_str() );
				int dd = atoi(&(newRegDate[3]));
				if ( newRegDate.length() == 8 && m > 0 && m <= 12 && dd > 0 && dd < 32 ) {
					user.SetRegisteredDateNum( date_to_daten( newRegDate.c_str() ) );
				} else {
					GetSession()->bout.NewLine();
				}
				std::string newExpDate;
				do {
					GetSession()->bout.NewLine();
					GetSession()->bout << "Enter expiration date, <CR> to clear registration fields: \r\n";
					GetSession()->bout << " MM/DD/YY\r\n:";
					input( newExpDate, 8 );
				} while ( newExpDate.length() != 8 && !newExpDate.empty() );
				if ( newExpDate.length() == 8 ) {
					m = atoi( newExpDate.c_str() );
					dd = atoi(&(newExpDate[3]));
				} else {
					user.SetRegisteredDateNum( 0 );
					user.SetExpiresDateNum( 0 );
				}
				if ( newExpDate.length() == 8 && m > 0 && m <= 12 && dd > 0 && dd < 32 ) {
					user.SetExpiresDateNum( date_to_daten( newExpDate.c_str() ) );
				}
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
			}
			break;
			case 'Y':
				if (u_qsc) {
					GetSession()->bout.NewLine();
					GetSession()->bout << "|#7(999=None) New sysop sub? ";
					std::string sysopSubNum;
					input( sysopSubNum, 3 );
					int nSysopSubNum = atoi( sysopSubNum.c_str() );
					if ( nSysopSubNum >= 0 && nSysopSubNum <= 999 && !sysopSubNum.empty() ) {
						*u_qsc = nSysopSubNum;
						write_qscn(nUserNumber, u_qsc, false);
					}
				}
				break;
			case 'Z': {
				char ch1;
				GetSession()->bout.NewLine();
				GetSession()->bout <<  "        " << restrict_string << wwiv::endl;
				do {
					GetSession()->bout << "|#7([Enter]=Quit, ?=Help) Enter Restriction to Toggle? ";
					s[0] = RETURN;
					s[1] = '?';
					strcpy(&(s[2]), restrict_string);
					ch1 = onek(s);
					if (ch1 == SPACE) {
						ch1 = RETURN;
					}
					if (ch1 == '?') {
						GetSession()->bout.ClearScreen();
						printfile( SRESTRCT_NOEXT );
					}
					if ( ch1 != RETURN && ch1 != '?' ) {
						int nRestriction = -1;
						for (int i1 = 0; i1 < 16; i1++) {
							if (ch1 == s[i1 + 2]) {
								nRestriction = i1;
							}
						}
						if (nRestriction > -1) {
							user.ToggleRestrictionFlag( 1 << nRestriction );
							GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
						}
					}
				} while ( !hangup && ch1 == '?' );
			}
			break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '0': {
				int nAutoValNum = 9;
				if (ch != '0') {
					nAutoValNum = ch - '1';
				}
				if ((GetSession()->GetEffectiveSl() >= syscfg.autoval[nAutoValNum].sl) &&
				        (GetSession()->GetCurrentUser()->GetDsl() >= syscfg.autoval[nAutoValNum].dsl) &&
				        (((~GetSession()->GetCurrentUser()->GetAr()) & syscfg.autoval[nAutoValNum].ar) == 0) &&
				        (((~GetSession()->GetCurrentUser()->GetDar()) & syscfg.autoval[nAutoValNum].dar) == 0)) {
					auto_val( nAutoValNum, &user );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case ']':
				++nUserNumber;
				if (nUserNumber > nNumUserRecords) {
					nUserNumber = 1;
				}
				bDoneWithUser = true;
				break;
			case '[':
				--nUserNumber;
				if (nUserNumber == 0) {
					nUserNumber = nNumUserRecords;
				}
				bDoneWithUser = true;
				break;
			case '}': {
				int tempu = nUserNumber;
				++nUserNumber;
				if (nUserNumber > nNumUserRecords) {
					nUserNumber = 1;
				}
				while ( nUserNumber != tempu && !matchuser(nUserNumber) ) {
					++nUserNumber;
					if (nUserNumber > nNumUserRecords) {
						nUserNumber = 1;
					}
				}
				bDoneWithUser = true;
			}
			break;
			case '{': {
				int tempu = nUserNumber;
				--nUserNumber;
				if (nUserNumber < 1) {
					nUserNumber = nNumUserRecords;
				}
				while ( nUserNumber != tempu && !matchuser(nUserNumber) ) {
					--nUserNumber;
					if (nUserNumber < 1) {
						nUserNumber = nNumUserRecords;
					}
				}
				bDoneWithUser = true;
			}
			break;
			case '/':
				changeopt();
				break;
			case ',':
				temp_full = (!temp_full);
				break;
			case '.':
				full = (!full);
				temp_full = full;
				break;
			case '?': {
				GetSession()->bout.ClearScreen();
				printfile( SUEDIT_NOEXT );
				getkey();
			}
			break;
			case '~':
				user.SetAssPoints( 0 );
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				break;
			case '%': {
				char s1[ 255];
				GetSession()->bout.NewLine();
				GetSession()->bout << "|#7New Street Address? ";
				inputl( s1, 30 );
				if (s1[0]) {
					user.SetStreet( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				GetSession()->bout << "|#7New City? ";
				inputl( s1, 30 );
				if (s1[0]) {
					user.SetCity( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				GetSession()->bout << "|#7New State? ";
				input( s1, 2 );
				if (s1[0]) {
					user.SetState( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				GetSession()->bout << "|#7New Country? ";
				input( s1, 3 );
				if (s1[0]) {
					user.SetCountry( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				GetSession()->bout << "|#7New Zip? ";
				input( s1, 10 );
				if (s1[0]) {
					user.SetZipcode( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
				GetSession()->bout << "|#7New DataPhone (0=none)? ";
				input( s1, 12 );
				if (s1[0]) {
					user.SetDataPhoneNumber( s1 );
					GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
				}
			}
			break;
			case ':': {
				bool done2 = false;
				do {
					GetSession()->bout.NewLine();
					GetSession()->bout << "Zap info...\r\n";
					GetSession()->bout << "1. Realname\r\n";
					GetSession()->bout << "2. Birthday\r\n";
					GetSession()->bout << "3. Street address\r\n";
					GetSession()->bout << "4. City\r\n";
					GetSession()->bout << "5. State\r\n";
					GetSession()->bout << "6. Country\r\n";
					GetSession()->bout << "7. Zip Code\r\n";
					GetSession()->bout << "8. DataPhone\r\n";
					GetSession()->bout << "9. Computer\r\n";
					GetSession()->bout << "Q. Quit\r\n";
					GetSession()->bout << "Which? ";
					char ch1 = onek("Q123456789");
					switch (ch1) {
					case '1':
						user.SetRealName( "" );
						break;
					case '2':
						user.SetBirthdayYear( 0 );
						break;
					case '3':
						user.SetStreet( "" );
						break;
					case '4':
						user.SetCity( "" );
						break;
					case '5':
						user.SetState( "" );
						break;
					case '6':
						user.SetCountry( "" );
						break;
					case '7':
						user.SetZipcode( "" );
						break;
					case '8':
						user.SetDataPhoneNumber( "" );
						break;
					case '9':
						// rf-I don't know why this is 254, it was in 4.31
						// but it probably shouldn't be. Changed to 0
						user.SetComputerType( 0 );
						break;
					case 'Q':
						done2 = true;
						break;
					}
				} while (!done2);
				GetApplication()->GetUserManager()->WriteUser( &user, nUserNumber );
			}
			break;
			}
		} while ( !bDoneWithUser && !hangup );
	} while ( !bDoneWithUEdit && !hangup );

	if (u_qsc) {
		BbsFreeMemory(u_qsc);
	}

	u_qsc = NULL;
}


char *daten_to_date(time_t dt) {
	static char s[9];
	struct tm * pTm = localtime(&dt);

	if ( pTm ) {
		sprintf(s, "%02d/%02d/%02d", pTm->tm_mon, pTm->tm_mday, pTm->tm_year % 100);
	} else {
		strcpy(s, "01/01/00" );
	}
	return s;
}


void print_affil( WUser *pUser ) {
	net_system_list_rec *csne;

	if ( pUser->GetForwardNetNumber() == 0 || pUser->GetHomeSystemNumber() == 0 ) {
		return;
	}
	set_net_num( pUser->GetForwardNetNumber() );
	csne = next_system( pUser->GetHomeSystemNumber() );
	GetSession()->bout << "|#2   Sysp    : |#1";
	if ( csne ) {
		GetSession()->bout << "@" << pUser->GetHomeSystemNumber() << ", " << csne->name << ", on " << GetSession()->GetNetworkName() << ".";
	} else {
		GetSession()->bout << "@" << pUser->GetHomeSystemNumber() << ", <UNKNOWN>, on " << GetSession()->GetNetworkName() << ".";
	}
	GetSession()->bout.NewLine( 2 );
}
