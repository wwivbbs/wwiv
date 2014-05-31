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

#include "platform/wlocal_io.h"
#include "net.h"
#include "wcomm.h"

const int WSession::mmkeyMessageAreas   = 0;
const int WSession::mmkeyFileAreas      = 1;
const int WSession::mmkeyChains         = 2;


WSession::WSession( WApplication *pApplication ) {
	m_pLocalIO			                        = new WLocalIO();
	bout.SetLocalIO( m_pLocalIO );

	m_bLastKeyLocal = true;
	m_pApplication  = pApplication;
	m_nEffectiveSl  = 0;

	memset( &newuser_colors, 0, sizeof( newuser_colors ) );
	memset( &newuser_bwcolors, 0, sizeof( newuser_bwcolors ) );
	memset( &asv, 0, sizeof( asv_rec ) );
	memset( &advasv, 0, sizeof( adv_asv_rec ) );
	memset( &cbv, 0, sizeof( cbv_rec ) );


	m_nTopScreenColor                       = 0;
	m_nUserEditorColor                      = 0;
	m_nEditLineColor                        = 0;
	m_nChatNameSelectionColor               = 0;
	m_nMessageColor                         = 0;
	mail_who_field_len                      = 0;
	max_batch                               = 0;
	max_extend_lines                        = 0;
	max_chains                              = 0;
	max_gfilesec                            = 0;
	screen_saver_time                       = 0;
	language_dir = "";
	m_currentSpeed                          = "";
	m_nForcedReadSubNumber                  = 0;
	m_bThreadSubs                           = false;
	m_bAllowCC                              = false;
	m_bUserOnline                           = false;
	m_bQuoting                              = false;
	m_bTimeOnlineLimited                    = false;
	wfcdrvs[0] = wfcdrvs[1] = wfcdrvs[2] = wfcdrvs[3] = wfcdrvs[4] = wfc_status = 0;

	m_nCurrentFileArea                      = 0;
	m_nMMKeyArea                            = 0;
	m_nCurrentReadMessageArea               = 0;
	m_nCurrentMessageArea                   = 0;
	m_nCurrentLanguageNumber                = 0;
	m_nCurrentConferenceFileArea            = 0;
	m_nCurrentConferenceMessageArea         = 0;
	m_nFileAreaCache = m_nMessageAreaCache  = 0;
	m_nBeginDayNodeNumber                   = 0;
	m_nMaxNumberMessageAreas                = 0;
	m_nMaxNumberFileAreas                   = 0;
	m_nNumMessagesReadThisLogon             = 0;
	m_nNetworkNumber                        = 0;
	m_nMaxNetworkNumber                     = 0;
	m_nCurrentNetworkType                   = net_type_wwivnet;
	m_bNewMailWaiting                       = false;
	numbatch = 0;
	numbatchdl = 0;
	m_nNumberOfChains = 0;
	m_nNumberOfEditors = 0;
	m_nNumberOfExternalProtocols = 0;
	numf = 0;
	m_nNumMsgsInCurrentSub = 0;
	num_dirs = 0;
	num_languages = 0;
	num_sec = 0;
	num_subs = 0;
	num_events = 0;
	num_sys_list = 0;
	screenlinest = 0;
	subchg = 0;
	tagging = 0;
	tagptr = 0;
	titled = 0;
	using_modem = 0;
	m_bInternalZmodem = false;
	m_bExecLogSyncFoss = false;
	m_bExecUseWaitForInputIdle = false;
	m_nExecChildProcessWaitTime = 0;
	m_bNewScanAtLogin = false;
	usernum = 0;
	m_pComm = NULL;
}

WSession::~WSession() {
	if ( ok_modem_stuff && m_pComm != NULL ) {
		m_pComm->close();
		if ( m_pComm != NULL ) {
			delete m_pComm;
			m_pComm = NULL;
		}
	}
	if ( m_pLocalIO != NULL ) {
		m_pLocalIO->SetCursor( WLocalIO::cursorNormal );
		delete m_pLocalIO;
		m_pLocalIO = NULL;
	}
}




void WSession::CreateComm(bool bUseSockets, unsigned int nHandle ) {
	m_pComm = WComm::CreateComm( bUseSockets, nHandle );
	GetSession()->bout.SetComm( m_pComm );
}


WLocalIO* WSession::localIO() {
	return m_pLocalIO;
}


WComm* WSession::remoteIO() {
	return m_pComm;
}


bool WSession::ReadCurrentUser() {
	return ReadCurrentUser( usernum, false );
}

bool WSession::ReadCurrentUser( int nUserNumber, bool bForceRead ) {
	WWIV_ASSERT( m_pApplication );
	WWIV_ASSERT( m_pApplication->GetUserManager() );
	return m_pApplication->GetUserManager()->ReadUser( &m_thisuser, nUserNumber, bForceRead );
}

bool WSession::WriteCurrentUser() {
	return WriteCurrentUser( usernum );
}


bool WSession::WriteCurrentUser( int nUserNumber ) {
	WWIV_ASSERT( m_pApplication );
	WWIV_ASSERT( m_pApplication->GetUserManager() );
	return m_pApplication->GetUserManager()->WriteUser( &m_thisuser, nUserNumber );
}


void WSession::DisplaySysopWorkingIndicator( bool displayWait ) {
	const std::string waitString = "-=[WAIT]=-";
	std::string::size_type nNumPrintableChars = waitString.length();
	for (std::string::const_iterator iter = waitString.begin(); iter != waitString.end(); ++iter ) {
		if ( *iter == 3 && nNumPrintableChars > 1 ) {
			nNumPrintableChars -= 2;
		}
	}

	if (displayWait) {
		if ( okansi() ) {
			int nSavedAttribute = curatr;
			bout.SystemColor( GetCurrentUser()->HasColor() ? GetCurrentUser()->GetColor( 3 ) : GetCurrentUser()->GetBWColor( 3 ) );
			bout << waitString << "\x1b[" << nNumPrintableChars << "D";
			bout.SystemColor( static_cast< unsigned char > ( nSavedAttribute ) );
		} else {
			bout << waitString;
		}
	} else {
		if ( okansi() ) {
			for (unsigned int j = 0; j < nNumPrintableChars; j++) {
				bputch(' ');
			}
			bout << "\x1b[" << nNumPrintableChars << "D";
		} else {
			for (unsigned int j = 0; j < nNumPrintableChars; j++) {
				GetSession()->bout.BackSpace();
			}
		}
	}
}

const char* WSession::GetNetworkName() const {
	return net_networks[m_nNetworkNumber].name;
}


const char* WSession::GetNetworkDataDirectory() const {
	return net_networks[m_nNetworkNumber].dir;
}

