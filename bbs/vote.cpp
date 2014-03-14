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


void print_quest(int mapp, int map[21]) {
	votingrec v;

	GetSession()->bout.ClearScreen();
	if ( okansi() ) {
		GetSession()->bout.DisplayLiteBar( "[ %s Voting Questions ]", syscfg.systemname );
	} else {
		GetSession()->bout << "|#5Voting Questions:\r\n\n";
	}
	bool abort = false;
	WFile voteFile( syscfg.datadir, VOTING_DAT );
	if ( !voteFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return;
	}

	for ( int i = 1; i <= mapp && !abort; i++ ) {
		voteFile.Seek( map[ i ] * sizeof( votingrec ), WFile::seekBegin );
		voteFile.Read( &v, sizeof( votingrec ) );

		char szBuffer[255];
		snprintf( szBuffer, sizeof( szBuffer ), "|#6%c |#2%2d|#7) |#1%s",
		          GetSession()->GetCurrentUser()->GetVote( map[ i ] ) ? ' ' : '*', i, v.question );
		pla( szBuffer, &abort );
	}
	voteFile.Close();
	GetSession()->bout.NewLine();
	if ( abort ) {
		GetSession()->bout.NewLine();
	}
}


bool print_question( int i, int ii ) {
	votingrec v;

	WFile voteFile( syscfg.datadir, VOTING_DAT );
	if ( !voteFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return false;
	}
	voteFile.Seek( ii * sizeof( votingrec ), WFile::seekBegin );
	voteFile.Read( &v, sizeof( votingrec ) );
	voteFile.Close();
	bool abort = false;

	if (!v.numanswers) {
		return false;
	}

	GetSession()->bout.ClearScreen();
	char szBuffer[255];
	sprintf( szBuffer, "%s%d", "|#5Voting question #", i );
	pla( szBuffer, &abort );
	GetSession()->bout.Color( 1 );
	pla( v.question, &abort );
	GetSession()->bout.NewLine();
	int t = 0;
	voting_response vr;
	for ( int i1 = 0; i1 < v.numanswers; i1++ ) {
		vr = v.responses[i1];
		t += vr.numresponses;
	}

	GetApplication()->GetStatusManager()->RefreshStatusCache();
	sprintf( szBuffer , "|#9Users voting: |#2%4.1f%%\r\n",
	         static_cast<double>( t ) / static_cast<double>( GetApplication()->GetStatusManager()->GetUserCount() ) * 100.0 );
	pla( szBuffer, &abort );
	int t1 = ( t ) ? t : 1;
	pla( " |#20|#9) |#9No Comment", &abort );
	for ( int i2 = 0; i2 < 5; i2++ ) {
		odc[i2] = '\0';
	}
	for ( int i3 = 0; i3 < v.numanswers && !abort; i3++ ) {
		vr = v.responses[ i3 ];
		if ( ( ( i3 + 1 ) % 10 ) == 0 ) {
			odc[ ( ( i3 + 1 ) / 10 ) - 1 ] = '0' + static_cast<char>( ( i3 + 1 ) / 10 );
		}
		sprintf( szBuffer, "|#2%2d|#9) |#9%-60s   |#3%4d  |#1%5.1f%%",
		         i3 + 1, vr.response, vr.numresponses,
		         static_cast<float>( vr.numresponses ) / static_cast<float>( t1 ) * 100.0 );
		pla( szBuffer , &abort );
	}
	GetSession()->bout.NewLine();
	if ( abort ) {
		GetSession()->bout.NewLine();
	}
	return ( abort ) ? false : true;
}


void vote_question(int i, int ii) {
	votingrec v;

	bool pqo = print_question(i, ii);
	if ( GetSession()->GetCurrentUser()->IsRestrictionVote() || GetSession()->GetEffectiveSl() <= 10 ||!pqo ) {
		return;
	}


	WFile voteFile( syscfg.datadir, VOTING_DAT );
	if ( !voteFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return;
	}
	voteFile.Seek( ii * sizeof( votingrec ), WFile::seekBegin );
	voteFile.Read( &v, sizeof( votingrec ) );
	voteFile.Close();

	if ( !v.numanswers ) {
		return;
	}

	std::string message = "|#9Your vote: |#1";
	if ( GetSession()->GetCurrentUser()->GetVote( ii ) ) {
		message.append(  v.responses[ GetSession()->GetCurrentUser()->GetVote( ii ) - 1 ].response );
	} else {
		message +=  "No Comment";
	}
	GetSession()->bout <<  message;
	GetSession()->bout.NewLine( 2 );
	GetSession()->bout << "|#5Change it? ";
	if (!yesno()) {
		return;
	}

	GetSession()->bout << "|#5Which (0-" << static_cast<int>( v.numanswers ) << ")? ";
	GetSession()->bout.ColorizedInputField( 2 );
	char* pszAnswer = mmkey( 2 );
	int i1 = atoi( pszAnswer );
	if (i1 > v.numanswers) {
		i1 = 0;
	}
	if ( i1 == 0 && !wwiv::strings::IsEquals( pszAnswer, "0" ) ) {
		return;
	}

	if ( !voteFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return;
	}
	voteFile.Seek( ii * sizeof( votingrec ), WFile::seekBegin );
	voteFile.Read( &v, sizeof( votingrec ) );

	if (!v.numanswers) {
		voteFile.Close();
		return;
	}
	if ( GetSession()->GetCurrentUser()->GetVote( ii ) ) {
		v.responses[ GetSession()->GetCurrentUser()->GetVote( ii ) - 1 ].numresponses--;
	}
	GetSession()->GetCurrentUser()->SetVote( ii, i1 );
	if (i1) {
		v.responses[ GetSession()->GetCurrentUser()->GetVote( ii ) - 1 ].numresponses++;
	}
	voteFile.Seek( ii * sizeof( votingrec ), WFile::seekBegin );
	voteFile.Write( &v, sizeof( votingrec ) );
	voteFile.Close();
	GetSession()->bout.NewLine( 2 );
}


void vote() {
	votingrec v;

	WFile voteFile( syscfg.datadir, VOTING_DAT );
	if ( !voteFile.Open( WFile::modeReadOnly | WFile::modeBinary ) ) {
		return;
	}

	int n = static_cast<int>( voteFile.GetLength() / sizeof( votingrec ) ) - 1;
	if (n < 20) {
		v.question[0] = 0;
		v.numanswers = 0;
		for (int i = n; i < 20; i++) {
			voteFile.Write( &v, sizeof( votingrec ) );
		}
	}

	for (int i = 0; i < 5; i++) {
		odc[i] = 0;
	}

	int map[21], mapp = 0;
	for ( int i1 = 0; i1 < 20; i1++ ) {
		voteFile.Seek( i1 * sizeof( votingrec ), WFile::seekBegin );
		voteFile.Read( &v, sizeof( votingrec ) );
		if (v.numanswers) {
			map[++mapp] = i1;
			if ((mapp % 10) == 0) {
				odc[(mapp / 10) - 1] = '0' + static_cast<char>( mapp / 10 );
			}
		}
	}
	voteFile.Close();

	char sodc[ 10 ];
	strcpy( sodc, odc );
	if ( mapp == 0 ) {
		GetSession()->bout << "\r\n\n|#6No voting questions currently.\r\n\n";
		return;
	}
	bool done = false;
	do {
		print_quest( mapp, &map[0] );
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#9(|#2Q|#9=|#2Quit|#9) Voting: |#2# |#9: ";
		strcpy( odc, sodc );
		GetSession()->bout.ColorizedInputField( 2 );
		char* pszAnswer = mmkey( 2 );
		int nQuestionNum = atoi( pszAnswer );
		if ( nQuestionNum > 0 && nQuestionNum <= mapp ) {
			vote_question( nQuestionNum, map[ nQuestionNum ] );
		} else if ( wwiv::strings::IsEquals( pszAnswer, "Q" ) ) {
			done = true;
		} else if ( wwiv::strings::IsEquals( pszAnswer, "?" ) ) {
			print_quest( mapp, &map[0] );
		}
	} while ( !done && !hangup );
}

