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

//
// Local funciton prototypes
//

void GetInstanceActivityString( instancerec &ir, char *pszOutActivity );

/*
 * Builds a string (in pszOutInstanceString) like:
 *
 * Instance   1: Offline
 *     LastUser: Sysop #1
 *
 * or
 *
 * Instance  22: Network transmission
 *     CurrUser: Sysop #1
 */
void make_inst_str( int nInstanceNum, char *pszOutInstanceString, int nInstanceFormat ) {
	char s[255];
	snprintf( s, sizeof( s ), "|#1Instance %-3d: |#2", nInstanceNum );

	instancerec ir;
	get_inst_info(nInstanceNum, &ir);

	char szNodeActivity[81];
	GetInstanceActivityString( ir, szNodeActivity );

	switch ( nInstanceFormat ) {
	case INST_FORMAT_WFC:
		strcpy( pszOutInstanceString, szNodeActivity);                // WFC Addition
		break;
	case INST_FORMAT_OLD:
		strncat(s, szNodeActivity, sizeof(s));
		strncat(s, "\r\n|#1", sizeof(s));

		if (ir.flags & INST_FLAGS_ONLINE) {
			strncat(s, "   CurrUser ", sizeof(s));
		} else {
			strncat(s, "   LastUser ", sizeof(s));
		}

		strncat(s, ": |#2", sizeof(s));

		if ( ir.user < syscfg.maxusers && ir.user > 0 ) {
			WUser userRecord;
			GetApplication()->GetUserManager()->ReadUser( &userRecord, ir.user );
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%-30.30s", userRecord.GetUserNameAndNumber( ir.user ) );
			strncat( s, szNodeActivity, sizeof( s ) );
		} else {
			snprintf( szNodeActivity, sizeof( szNodeActivity ), "%-30.30s", "Nobody" );
			strncat( s, szNodeActivity, sizeof( s ) );
		}

		strcpy( pszOutInstanceString, s );
		break;
	case INST_FORMAT_LIST: {
		std::string userName;
		if ( ir.user < syscfg.maxusers && ir.user > 0 ) {
			WUser user;
			GetApplication()->GetUserManager()->ReadUser( &user, ir.user );
			if (ir.flags & INST_FLAGS_ONLINE) {
				userName = user.GetUserNameAndNumber( ir.user );
			} else {
				userName = "Last: ";
				userName += user.GetUserNameAndNumber( ir.user );
			}
		} else {
			userName = "(Nobody)";
		}
		char szBuffer[ 255 ];
		snprintf( szBuffer, sizeof( szBuffer ), "|#5%-4d |#2%-35.35s |#1%-37.37s", nInstanceNum, userName.c_str(), szNodeActivity );
		strcpy( pszOutInstanceString, szBuffer );
	}
	break;
	default:
		snprintf( pszOutInstanceString, sizeof( pszOutInstanceString ), "** INVALID INSTANCE FORMAT PASSED [%d] **", nInstanceFormat );
		break;
	}
}


void multi_instance() {

	GetSession()->bout.NewLine();
	int nNumInstances = num_instances();
	if (nNumInstances < 1) {
		GetSession()->bout << "|#6Couldn't find instance data file.\r\n";
		return;
	}

	GetSession()->bout.WriteFormatted( "|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity" );
	char s1[81], s2[81], s3[81];
	strcpy( s1, charstr(4, '=') );
	strcpy( s2, charstr(35, '=') );
	strcpy( s3, charstr( 37, '=' ) );
	GetSession()->bout.WriteFormatted( "|#7%-4.4s %-35.35s %-37.37s\r\n", s1, s2, s3 );

	for (int nInstance = 1; nInstance <= nNumInstances; nInstance++) {
		char szBuffer[255];
		make_inst_str(nInstance, szBuffer, INST_FORMAT_LIST );
		GetSession()->bout << szBuffer;
		GetSession()->bout.NewLine();
	}
}


int inst_ok( int loc, int subloc ) {
	instancerec instance_temp;

	if (loc == INST_LOC_FSED) {
		return 0;
	}

	int nInstNum = 0;
	WFile instFile(  syscfg.datadir, INSTANCE_DAT );
	if ( !instFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return 0;
	}
	int nNumInstances = static_cast<int> ( instFile.GetLength() / sizeof( instancerec ) );
	instFile.Close();
	for (int nInstance = 1; nInstance < nNumInstances; nInstance++) {
		if ( instFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
			instFile.Seek( nInstance * sizeof( instancerec ), WFile::seekBegin );
			instFile.Read( &instance_temp, sizeof( instancerec ) );
			instFile.Close();
			if ( instance_temp.loc == loc &&
			        instance_temp.subloc == subloc &&
			        instance_temp.number != GetApplication()->GetInstanceNumber() ) {
				nInstNum = instance_temp.number;
			}
		}
	}
	return nInstNum;
}


void GetInstanceActivityString( instancerec &ir, char *pszOutActivity ) {
	char szNodeActivity[81];

	if ( ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10 ) {
		snprintf( szNodeActivity, sizeof( szNodeActivity ), "%s","WWIV Chatroom");
	} else switch (ir.loc) {
		case INST_LOC_DOWN:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Offline");
			break;
		case INST_LOC_INIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Initializing BBS");
			break;
		case INST_LOC_EMAIL:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Sending Email");
			break;
		case INST_LOC_MAIN:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Main Menu");
			break;
		case INST_LOC_XFER:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Transfer Area");
			if ( so() && ir.subloc < GetSession()->num_dirs ) {
				char szTemp2[ 100 ];
				snprintf( szTemp2, sizeof( szTemp2 ), "%s: %s", "Dir ", stripcolors( directories[ ir.subloc ].name ) );
				strncat(szNodeActivity, szTemp2, sizeof( szNodeActivity ) );
			}
			break;
		case INST_LOC_CHAINS:
			snprintf( szNodeActivity, sizeof( szNodeActivity ), "%s", "Chains" );
			if ( ir.subloc > 0 && ir.subloc <= GetSession()->GetNumberOfChains() ) {
				char szTemp2[ 100 ];
				snprintf( szTemp2, sizeof( szTemp2 ), "Door: %s", stripcolors( chains[ ir.subloc - 1 ].description ) );
				strncat( szNodeActivity, szTemp2, sizeof( szNodeActivity ) );
			}
			break;
		case INST_LOC_NET:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Network Transmission");
			break;
		case INST_LOC_GFILES:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "GFiles");
			break;
		case INST_LOC_BEGINDAY:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Running BeginDay");
			break;
		case INST_LOC_EVENT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Executing Event");
			break;
		case INST_LOC_CHAT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Normal Chat");
			break;
		case INST_LOC_CHAT2:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "SplitScreen Chat");
			break;
		case INST_LOC_CHATROOM:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "ChatRoom");
			break;
		case INST_LOC_LOGON:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Logging On");
			break;
		case INST_LOC_LOGOFF:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Logging off");
			break;
		case INST_LOC_FSED:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "FullScreen Editor");
			break;
		case INST_LOC_UEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In UEDIT");
			break;
		case INST_LOC_CHAINEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In CHAINEDIT");
			break;
		case INST_LOC_BOARDEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In BOARDEDIT");
			break;
		case INST_LOC_DIREDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In DIREDIT");
			break;
		case INST_LOC_GFILEEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In GFILEEDIT");
			break;
		case INST_LOC_CONFEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In CONFEDIT");
			break;
		case INST_LOC_DOS:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In DOS");
			break;
		case INST_LOC_DEFAULTS:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In Defaults");
			break;
		case INST_LOC_REBOOT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Rebooting");
			break;
		case INST_LOC_RELOAD:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Reloading BBS data");
			break;
		case INST_LOC_VOTE:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Voting");
			break;
		case INST_LOC_BANK:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In TimeBank");
			break;
		case INST_LOC_AMSG:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "AutoMessage");
			break;
		case INST_LOC_SUBS:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Reading Messages");
			if ( so() && ir.subloc < GetSession()->num_subs ) {
				char szTemp2[ 100 ];
				snprintf( szTemp2, sizeof( szTemp2 ), "(Sub: %s)", stripcolors( subboards[ ir.subloc ].name ) );
				strncat( szNodeActivity, szTemp2, sizeof( szNodeActivity ) );
			}
			break;
		case INST_LOC_CHUSER:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Changing User");
			break;
		case INST_LOC_TEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In TEDIT");
			break;
		case INST_LOC_MAILR:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Reading All Mail");
			break;
		case INST_LOC_RESETQSCAN:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Resetting QSCAN pointers");
			break;
		case INST_LOC_VOTEEDIT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In VOTEEDIT");
			break;
		case INST_LOC_VOTEPRINT:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Printing Voting Data");
			break;
		case INST_LOC_RESETF:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Resetting NAMES.LST");
			break;
		case INST_LOC_FEEDBACK:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Leaving Feedback");
			break;
		case INST_LOC_KILLEMAIL:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Viewing Old Email");
			break;
		case INST_LOC_POST:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Posting a Message");
			if ( so() && ir.subloc < GetSession()->num_subs ) {
				char szTemp2[ 100 ];
				snprintf( szTemp2, sizeof( szTemp2 ), "(Sub: %s)", stripcolors( subboards[ir.subloc].name ) );
				strncat( szNodeActivity, szTemp2, sizeof( szNodeActivity ) );
			}
			break;
		case INST_LOC_NEWUSER:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Registering a Newuser");
			break;
		case INST_LOC_RMAIL:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Reading Email");
			break;
		case INST_LOC_DOWNLOAD:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Downloading");
			break;
		case INST_LOC_UPLOAD:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Uploading");
			break;
		case INST_LOC_BIXFER:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Bi-directional Transfer");
			break;
		case INST_LOC_NETLIST:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Listing Net Info");
			break;
		case INST_LOC_TERM:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "In a terminal program");
			break;
		case INST_LOC_GETUSER:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Getting User ID");
			break;
		case INST_LOC_WFC:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Waiting for Call");
			break;
		default:
			snprintf( szNodeActivity, sizeof( szNodeActivity ),  "%s", "Unknown BBS Location!");
			break;
		}
	strcpy( pszOutActivity, szNodeActivity );
}
