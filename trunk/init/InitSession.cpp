/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include "wwivinit.h"

const int InitSession::mmkeyMessageAreas   = 0;
const int InitSession::mmkeyFileAreas      = 1;
const int InitSession::mmkeyChains         = 2;

InitSession::InitSession()
{
    m_bLastKeyLocal = true;
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
    m_currentSpeed                          = "";
	pszLanguageDir                          = NULL;
    m_nForcedReadSubNumber                  = 0;
    m_bThreadSubs                           = false;
    m_bAllowCC                              = false;
    m_bUserOnline                           = false;
    m_bQuoting                              = false;
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
    m_nGlobalDebugLevel                     = 0;

    m_nMaxNumberMessageAreas = 0;
    m_nMaxNumberFileAreas = 0;
    m_nNumMessagesReadThisLogon = 0;
    net_num = 0;
    net_num_max = 0;
    m_nCurrentNetworkType = net_type_wwivnet;
    m_bNewMailWaiting = false;
    numbatch = 0;
    numbatchdl = 0;
    numchains = 0;
    numeditors = 0;
    numexterns = 0;
    numf = 0;
    m_nNumMsgsInCurrentSub = 0;
    num_dirs = 0;
    num_languages = 0;
    num_sec = 0;
    num_subs = 0;
    num_events = 0;
    num_sys_list = 0;
    screenbottom = 0;
    screenlinest = 0;
    bbsshutdown = 0;
    subchg = 0;
    tagging = 0;
    tagptr = 0;
    titled = 0;
    topdata = 0;
    topline = 0;
    using_modem = 0;
    m_bInternalZmodem = 0;
    m_nExecLogSyncFoss = 0;
    m_nExecUseWaitForInputIdle = 0;
    m_nExecChildProcessWaitTime = 0;
    m_bNewScanAtLogin = 0;
    usernum = 0;
    shutdowntime = 0.0;
}
