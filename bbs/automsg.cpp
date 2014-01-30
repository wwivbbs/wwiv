/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include <vector>

char ShowAMsgMenuAndGetInput( const std::string& autoMessageLockFileName );
void write_automessage();


/**
 * Reads the auto message
 */
void read_automessage() {
	GetSession()->bout.NewLine();
	std::unique_ptr<WStatus> status(GetApplication()->GetStatusManager()->GetStatus());
	bool bAutoMessageAnonymous = status->IsAutoMessageAnonymous();

	WTextFile autoMessageFile( syscfg.gfilesdir, AUTO_MSG, "rt" );
	std::string line;
	if (!autoMessageFile.IsOpen() || !autoMessageFile.ReadLine( line ) ) {
		GetSession()->bout << "|#3No auto-message.\r\n";
		GetSession()->bout.NewLine();
		return;
	}

	std::string authorName = StringTrimEnd(line);
	if ( bAutoMessageAnonymous ) {
		if ( getslrec( GetSession()->GetEffectiveSl() ).ability & ability_read_post_anony ) {
			std::stringstream ss;
			ss << "<<< " << line << " >>>";
			authorName = ss.str();
		} else {
			authorName = ">UNKNOWN<";
		}
	}
	GetSession()->bout << "\r\n|#9Auto message by: |#2" << authorName << "|#0\r\n\n";

	int nLineNumber = 0;
	while ( autoMessageFile.ReadLine( line ) && nLineNumber++ < 10 ) {
		StringTrim( line );
		GetSession()->bout.Color( 9 );
		GetSession()->bout << "|#9" << line << wwiv::endl;
	}
	GetSession()->bout.NewLine();
}


/**
 * Writes the auto message
 */
void write_automessage() {
	std::vector<std::string> lines;
	std::string rollOver;

	GetSession()->bout << "\r\n|#9Enter auto-message. Max 5 lines. Colors allowed:|#0\r\n\n";
	for (int i = 0; i < 5; i++) {
		GetSession()->bout << "|#7" << i + 1 << ":|#0";
		std::string line;
		inli( line, rollOver, 70 );
		StringTrimEnd(line);
		lines.push_back(line);
	}
	GetSession()->bout.NewLine();
	bool bAnonStatus = false;
	if ( getslrec( GetSession()->GetEffectiveSl() ).ability & ability_post_anony ) {
		GetSession()->bout << "|#9Anonymous? ";
		bAnonStatus = yesno();
	}

	GetSession()->bout << "|#9Is this OK? ";
	if ( yesno() ) {
		WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
		pStatus->SetAutoMessageAnonymous( bAnonStatus );
		pStatus->SetAutoMessageAuthorUserNumber( GetSession()->usernum );
		GetApplication()->GetStatusManager()->CommitTransaction( pStatus );

		WTextFile file( syscfg.gfilesdir, AUTO_MSG, "wt" );
		std::string authorName = GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum );
		file.WriteFormatted( "%s\r\n", authorName.c_str() );
		sysoplog("Changed Auto-message");
#if defined(_MSC_VER) && (_MSC_VER < 1800)
		for( std::vector<std::string>::const_iterator iter = lines.begin(); iter != lines.end(); ++iter )
		{
			std::string line = (*iter);
			StringTrimEnd( line ); 
#else
		for (const auto& line : lines) {
#endif
			file.Write( line );
			file.Write("\r\n");
			sysoplog( line, true );
		}
		GetSession()->bout << "\r\n|#5Auto-message saved.\r\n\n";
		file.Close();
	}
}


char ShowAMsgMenuAndGetInput( const std::string& autoMessageLockFileName ) {
	bool bCanWrite = false;
	if ( !GetSession()->GetCurrentUser()->IsRestrictionAutomessage() && !WFile::Exists( autoMessageLockFileName ) ) {
		bCanWrite = ( getslrec( GetSession()->GetEffectiveSl() ).posts ) ? true : false;
	}

	char cmdKey = 0;
	if (cs()) {
		GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite, (|#2L|#9)ock, (|#2D|#9)el, (|#2U|#9)nlock : ";
		cmdKey = onek( "QRWALDU", true );
	} else if (bCanWrite) {
		GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply, (|#2W|#9)rite : ";
		cmdKey = onek( "QRWA", true );
	} else {
		GetSession()->bout << "|#9(|#2Q|#9)uit, (|#2R|#9)ead, (|#2A|#9)uto-reply : ";
		cmdKey = onek( "QRA", true );
	}
	return cmdKey;
}

/**
 * Main Automessage menu.  Displays the auto message then queries for input.
 */
void do_automessage() {
	std::stringstream lockFileStream;
	lockFileStream << syscfg.gfilesdir << LOCKAUTO_MSG;
	std::string automessageLockFile = lockFileStream.str();

	std::stringstream autoMessageStream;
	autoMessageStream << syscfg.gfilesdir << AUTO_MSG;
	std::string autoMessageFile = autoMessageStream.str();

	// initally show the auto message
	read_automessage();

	bool done = false;
	do {
		GetSession()->bout.NewLine();
		char cmdKey = ShowAMsgMenuAndGetInput( automessageLockFile );
		switch (cmdKey) {
		case 'Q':
			done = true;
			break;
		case 'R':
			read_automessage();
			break;
		case 'W':
			write_automessage();
			break;
		case 'A': {
			grab_quotes(NULL, NULL);
			WStatus *pStatus = GetApplication()->GetStatusManager()->GetStatus();
			if (pStatus->GetAutoMessageAuthorUserNumber() > 0) {
				strcpy(irt, "Re: AutoMessage");
				email( pStatus->GetAutoMessageAuthorUserNumber(), 0, false, pStatus->IsAutoMessageAnonymous() ? anony_sender : 0 );
			}
			delete pStatus;
		}
		break;
		case 'D':
			GetSession()->bout << "\r\n|#3Delete Auto-message, Are you sure? ";
			if (yesno()) {
				WFile::Remove( autoMessageFile );
			}
			GetSession()->bout.NewLine( 2 );
			break;
		case 'L':
			if ( WFile::Exists( automessageLockFile ) ) {
				GetSession()->bout << "\r\n|#3Message is already locked.\r\n\n";
			} else {
				GetSession()->bout <<  "|#9Do you want to lock the Auto-message? ";
				if ( yesno() ) {
					/////////////////////////////////////////////////////////
					// This makes a file in your GFILES dir 1 bytes long,
					// to tell the board if it is locked or not. It consists
					// of a space.
					//
					WTextFile lockFile( automessageLockFile, "w+t" );
					lockFile.WriteChar( ' ' );
					lockFile.Close();
				}
			}
			break;
		case 'U':
			if ( !WFile::Exists( automessageLockFile ) ) {
				GetSession()->bout << "Message not locked.\r\n";
			} else {
				GetSession()->bout << "|#5Unlock message? ";
				if (yesno()) {
					WFile::Remove( automessageLockFile );
				}
			}
			break;
		}
	} while ( !done && !hangup );
}
