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

#if defined( __APPLE__ ) && !defined( __unix__ )
#define __unix__ 1
#endif


bool connect_to1(char *phone, int dy);


void wwivnode( WUser *pUser, int mode) {
	char sysnum[6], s[81];
	int nUserNumber, nSystemNumber;

	if (!mode) {
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#7Are you an active WWIV SysOp (y/N): ";
		if (!yesno()) {
			return;
		}
	}
	GetSession()->bout.NewLine();
	GetSession()->bout << "|#7Node:|#0 ";
	input(sysnum, 5);
	if ( sysnum[0] == 'L' && mode ) {
		print_net_listing( false );
		GetSession()->bout << "|#7Node:|#0 ";
		input(sysnum, 5);
	}
	if ( sysnum[0] == '0' && mode ) {
		pUser->SetForwardNetNumber( 0 );
		pUser->SetHomeUserNumber( 0 );
		pUser->SetHomeSystemNumber( 0 );
		return;
	}
	sprintf(s, "1@%s", sysnum);
	parse_email_info(s, &nUserNumber, &nSystemNumber);
	if (nSystemNumber == 0) {
		GetSession()->bout << "|#2No match for " << sysnum << "." << wwiv::endl;
		pausescr();
		return;
	}
	net_system_list_rec *csne = next_system( nSystemNumber );
	sprintf( s, "Sysop @%u %s %s", nSystemNumber, csne->name, GetSession()->GetNetworkName() );
	std::string ph, ph1;
	if ( !mode ) {
		ph1 = pUser->GetDataPhoneNumber();
		input_dataphone();
		ph = pUser->GetDataPhoneNumber();
		pUser->SetDataPhoneNumber( ph1.c_str() );
		enter_regnum();
	} else {
		ph = csne->phone;
	}
	if ( ph != csne->phone ) {
		GetSession()->bout.NewLine();
		if ( printfile( ASV0_NOEXT ) ) {
			// failed
			GetSession()->bout.NewLine();
			pausescr();
		}
		sprintf( s, "Attempted WWIV SysOp autovalidation." );
		pUser->SetNote( s );
		if ( pUser->GetSl() < syscfg.newusersl) {
			pUser->SetSl( syscfg.newusersl );
		}
		if ( pUser->GetDsl() < syscfg.newuserdsl ) {
			pUser->SetDsl( syscfg.newuserdsl );
		}
		return;
	}
	sysoplog("-+ WWIV SysOp");
	sysoplog(s);
	pUser->SetRestriction( GetSession()->asv.restrict );
	pUser->SetExempt( GetSession()->asv.exempt );
	pUser->SetAr( GetSession()->asv.ar );
	pUser->SetDar( GetSession()->asv.dar );
	if (!mode) {
		GetSession()->bout.NewLine();
		if (printfile(ASV1_NOEXT)) {
			// passed
			GetSession()->bout.NewLine();
			pausescr();
		}
	}
	if ( wwiv::strings::IsEquals( pUser->GetDataPhoneNumber(),
	                                  reinterpret_cast<char*>( csne->phone ) ) ) {
		if ( pUser->GetSl() < GetSession()->asv.sl ) {
			pUser->SetSl( GetSession()->asv.sl );
		}
		if ( pUser->GetDsl() < GetSession()->asv.dsl ) {
			pUser->SetDsl( GetSession()->asv.dsl );
		}
	} else {
		if ( !mode ) {
			GetSession()->bout.NewLine();
			if ( printfile( ASV2_NOEXT ) ) {
				// data phone not bbs
				GetSession()->bout.NewLine();
				pausescr();
			}
		}
	}
	pUser->SetForwardNetNumber( GetSession()->GetNetworkNumber() );
	pUser->SetHomeUserNumber( 1 );
	pUser->SetHomeSystemNumber( nSystemNumber );
	if ( !mode ) {
		print_affil( pUser );
		pausescr();
	}
	pUser->SetForwardUserNumber( pUser->GetHomeUserNumber() );
	pUser->SetForwardSystemNumber( pUser->GetHomeSystemNumber() );
	if ( !mode ) {
		GetSession()->bout.NewLine();
		if ( printfile( ASV3_NOEXT ) ) {
			\
			// mail forwarded
			GetSession()->bout.NewLine();
			pausescr();
		}
	}
	if ( !pUser->GetWWIVRegNumber() ) {
		enter_regnum();
	}
	changedsl();
}


int callback() {
#ifndef __unix__
	int i1, ok, count = 1;
	bool res;
	char s[255], s1[81];
	char tempphone[15];

	printfile(CBV1_NOEXT);                       // introduction
	do {
		ok = 1;
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#2MODEM PH:";
		input( s1, 14, true );
		if (!(syscfg.sysconfig & sysconfig_free_phone)) {
			if ((strlen(s1) == 8) || (strlen(s1) == 10) ||
			        (strlen(s1) == 12) || (strlen(s1) == 14)) {
				strcpy(tempphone, s1);
				WWIV_STRREV(tempphone);
				if (tempphone[4] != '-') {
					ok = 0;
				}
				if ((strlen(s1) > 8) && (tempphone[8] != '-')) {
					ok = 0;
				}
				if ((strlen(s1) == 14) && (tempphone[12] != '-')) {
					ok = 0;
				}
				if ((strlen(s1) == 10) || (strlen(s1) == 14)) {
					if (s1[0] == '1') {
						ok = 3;
						if (!(GetSession()->cbv.longdistance)) {
							return ok;
						}
					} else {
						ok = 0;
					}
				}
				if ( s1[0] == '9' && s1[1] == '1' && s1[2] == '1' ) {
					ok = 0;
				}
				if ((s1[0] == '4') && (s1[1] == '1') && (s1[2] == '1')) {
					ok = 0;
				}
				if ((s1[2] == '9') && (s1[3] == '0') && (s1[4] == '0')) {
					ok = 0;
				}
				if ((s1[2] == '8') && (s1[3] == '0') && (s1[4] == '0')) {
					ok = 0;
				}
			} else {
				ok = 0;
			}
		}
		for (i1 = 0; i1 < wwiv::strings::GetStringLength(s1) - 1; i1++) {
			if ( ( s1[i1] < '0' || s1[i1] > '9' ) && s1[i1] != '-' ) {
				ok = 0;
			}
		}
		if (!ok) {
			GetSession()->bout.NewLine();
			printfile(CBV2_NOEXT);                   // badphone
			pausescr();
		}
	} while ( !ok && count++ < 3 );
	if (!ok) {
		return ok;
	}
	sprintf(s, "    CBV ATTEMPT %s / %s / %s / %s", GetSession()->GetCurrentUser()->GetName(), s1, fulldate(), GetSession()->GetCurrentSpeed().c_str() );
	sysoplog(s, false);
	GetSession()->bout.NewLine();
	printfile(CBV3_NOEXT);                       // instructions
	pausescr();
	count = 1;
	do {
		hang_it_up();
		rputs(modem_i->hang);
		imodem( false );
		wait1(20);
		hangup = false;
		GetSession()->bout << "Attempt " << count << " of " << GetSession()->cbv.repeat << "." << wwiv::endl;
		res = connect_to1(s1, 0);
		if (res) {
			hangup = false;
			count = GetSession()->cbv.repeat;
		}
	} while ((ok_modem_stuff) && (count++ < GetSession()->cbv.repeat));
	if ( !GetSession()->remoteIO()->carrier() ) {
		hangup = true;
	}
	if (!hangup) {
		count = 0;
		wait1(20);
		dump();
		do {
			GetSession()->bout.NewLine();
			std::string password;
			input_password( "|#7PW: ", password, 8 );
			if ( password == GetSession()->GetCurrentUser()->GetPassword() ) {
				sprintf( s, "    CBV SUCCESS %s / %s / %s / %s", GetSession()->GetCurrentUser()->GetName(), s1, date(), GetSession()->GetCurrentSpeed().c_str() );
				sysoplog( s, false );
				ssm( 1, 0, s );
				return ok;
			} else {
				++count;
				GetSession()->bout.NewLine();
			}
		} while ( count < 3 );
	}
#endif
	return 0;
}


void dial(char *phone, int xlate) {
#ifndef __unix__
	char s[255];

	strcpy(s, "ATDT");
	if (syscfg.dial_prefix[0]) {
		strncpy(s, syscfg.dial_prefix, 20);
		s[20] = 0;
	}
	if ( modem_i && modem_i->dial ) {
		strncpy(s, modem_i->dial, 80);
		s[80] = 0;
	}
	if (xlate) {
		if (strncmp(phone, syscfg.systemphone, 3)) {
			strcat(s, "1-");
			strcat(s, phone);
		} else {
			strcat(s, &(phone[4]));
		}
	} else {
		strcat(s, phone);
	}
	std::cout << "Dialing: '" << s << "{', 'H' to abort.\n";
	strcat(s, "\r");
	rputs( s );
#endif
}


/* This procedure will attempt to call up & connect to a phone number
 * (which is passed).  This will only dial direct, and won't go through
 * PC Pursuit or anything like that.  If it connects, it will return
 * a 1, if it can't connect (for whatever reason), it returns 0.
 */
bool connect_to1(char *phone, int dy) {
#ifndef __unix__

	GetSession()->SetCurrentSpeed( "" );
	int spd = 0;
	dial( phone, dy );

	dump();
	if ( mode_switch( 45.0, true ) != mode_con ) {
		if ( modem_mode == 0 ) {
			rputch( ' ' );
			Wait( 0.5 );
			dump();
		}
	} else {
		// connected
		spd = modem_speed;
	}
	Wait(1.0);
	if (spd) {
		std::cout << "Connected at " << GetSession()->GetCurrentSpeed() << "." << std::endl;
		return true;
	} else {
		if ( modem_mode == mode_ndt ) {
			std::cout << "No dial tone.\n";
		} else {
			if ( GetSession()->GetCurrentSpeed().length() > 0 ) {
				std::cout << "No connection (" << GetSession()->GetCurrentSpeed() << ")." << std::endl;
			} else {
				std::cout << "No connection." << std::endl;
			}
		}
		return false;
	}
#else
	return false;
#endif
}
