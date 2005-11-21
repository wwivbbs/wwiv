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
#ifdef _MSC_VER
#pragma once
#endif


//
// WSession - Holds information and status data about the current user
// session on the BBS. (i.e. user record, and all data/variables
// associated with the current logon)
//
// This is different from the BbsApp which holds global BBS information
// associated with this instance of WWIV globally (not tied to a user)
//


#if !defined ( __INCLUDED_WSESSION_H__ )
#define __INCLUDED_WSESSION_H__



#include "vardec.h"
#include "WUser.h"
#include "bbs.h"
#include "net.h"
#include "WOutStreamBuffer.h"


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable: 4511 4512 )
#endif // _MSC_VER

extern net_networks_rec *net_networks;

class WBbsApp;

class WSession
{
public:
    WSession( WBbsApp *pApplication );
    ~WSession() {}

public:
    WOutStream bout;

public:
    static const int mmkeyMessageAreas;
    static const int mmkeyFileAreas;
    static const int mmkeyChains;


public:
    WUser thisuser;
	WUser* GetCurrentUser()							{ return &thisuser; }
    bool IsLastKeyLocal() const                     { return m_bLastKeyLocal; }
    void SetLastKeyLocal( bool b )                  { m_bLastKeyLocal = b; }

    bool ReadCurrentUser( int nUserNumber, bool bForceRead = false );
    bool WriteCurrentUser( int nUserNumber );

    void ResetEffectiveSl()                         { m_nEffectiveSl = thisuser.GetSl(); }
    void SetEffectiveSl( int nSl )                  { m_nEffectiveSl = nSl; }
    int  GetEffectiveSl() const                     { return m_nEffectiveSl; }

    int  GetTopScreenColor() const                  { return m_nTopScreenColor; }
    void SetTopScreenColor( int n )                 { m_nTopScreenColor = n; }

    int  GetUserEditorColor() const                 { return m_nUserEditorColor; }
    void SetUserEditorColor( int n )                { m_nUserEditorColor = n; }

    int  GetEditLineColor() const                   { return m_nEditLineColor; }
    void SetEditLineColor( int n )                  { m_nEditLineColor = n; }

    int  GetChatNameSelectionColor() const          { return m_nChatNameSelectionColor; }
    void SetChatNameSelectionColor( int n )         { m_nChatNameSelectionColor = n; }

    int  GetMessageColor() const                    { return m_nMessageColor; }
    void SetMessageColor( int n )                   { m_nMessageColor = n; }

    int  GetForcedReadSubNumber() const             { return m_nForcedReadSubNumber; }
    void SetForcedReadSubNumber( int n )            { m_nForcedReadSubNumber = n; }

    std::string GetCurrentSpeed() const             { return m_currentSpeed; }
    void SetCurrentSpeed( std::string s )           { m_currentSpeed = s; }
    void SetCurrentSpeed( const char *s )           { m_currentSpeed = s; }

	const char* GetNetworkName() const		        { return net_networks[m_nNetworkNumber].name; }
	const char* GetNetworkDataDirectory() const	    { return net_networks[m_nNetworkNumber].dir; }

	bool IsMessageThreadingEnabled() const	        { return m_bThreadSubs; }
	void SetMessageThreadingEnabled( bool b )       { m_bThreadSubs = b; }

	bool IsCarbonCopyEnabled() const		        { return m_bAllowCC; }
	void SetCarbonCopyEnabled( bool b )		        { m_bAllowCC = b; }

	bool IsUserOnline() const				        { return m_bUserOnline; }
	void SetUserOnline( bool b )			        { m_bUserOnline = b; }

    int  GetMMKeyArea() const                       { return m_nMMKeyArea; }
    void SetMMKeyArea( int n )                      { m_nMMKeyArea = n; }

    int  GetFileAreaCacheNumber() const             { return m_nFileAreaCache; }
    void SetFileAreaCacheNumber( int n )            { m_nFileAreaCache = n; }

    int  GetMessageAreaCacheNumber() const          { return m_nMessageAreaCache; }
    void SetMessageAreaCacheNumber( int n )         { m_nMessageAreaCache = n; }

    int  GetCurrentLanguageNumber() const           { return m_nCurrentLanguageNumber; }
    void SetCurrentLanguageNumber( int n )          { m_nCurrentLanguageNumber = n; }

    bool IsInternetUseRealNames() const             { return m_bInternetUseRealNames; }
    void SetInternetUseRealNames( bool b )          { m_bInternetUseRealNames = b; }

    int  GetNumMessagesReadThisLogon() const        { return m_nNumMessagesReadThisLogon; }
    void SetNumMessagesReadThisLogon( int n )       { m_nNumMessagesReadThisLogon = n; }

    bool IsQuoting() const                          { return m_bQuoting; }
    void SetQuoting( bool b )                       { m_bQuoting = b; }

    bool IsNewScanAtLogin() const                   { return ( m_bNewScanAtLogin ) ? true : false; }
    void SetNewScanAtLogin( bool b )                { m_bNewScanAtLogin = ( b ) ? 1 : 0; }

    int  GetCurrentFileArea() const                 { return m_nCurrentFileArea; }
    void SetCurrentFileArea( int n )                { m_nCurrentFileArea = n; }

    int  GetCurrentMessageArea() const              { return m_nCurrentMessageArea; }
    void SetCurrentMessageArea( int n )             { m_nCurrentMessageArea = n; }

    int  GetCurrentReadMessageArea() const          { return m_nCurrentReadMessageArea; }
    void SetCurrentReadMessageArea( int n )         { m_nCurrentReadMessageArea = n; }

    int  GetCurrentConferenceMessageArea() const    { return m_nCurrentConferenceMessageArea; }
    void SetCurrentConferenceMessageArea( int n )   { m_nCurrentConferenceMessageArea = n; }

    int  GetCurrentConferenceFileArea() const       { return m_nCurrentConferenceFileArea; }
    void SetCurrentConferenceFileArea( int n )      { m_nCurrentConferenceFileArea = n; }

    bool IsUseInternalZmodem() const                { return m_bInternalZmodem ? true : false; }
    void SetUseInternalZmodem( bool b )             { m_bInternalZmodem = b ? 1 : 0; }

    int  GetNumMessagesInCurrentMessageArea() const { return m_nNumMsgsInCurrentSub; }
    void SetNumMessagesInCurrentMessageArea( int n ){ m_nNumMsgsInCurrentSub = n; }

    int  GetBeginDayNodeNumber() const              { return m_nBeginDayNodeNumber; }
    void SetBeginDayNodeNumber( int n )             { m_nBeginDayNodeNumber = n; }

    int  GetGlobalDebugLevel() const                { return m_nGlobalDebugLevel; }
    void SetGlobalDebugLevel( int n )               { m_nGlobalDebugLevel = n; }

    bool IsExecUseWaitForInputIdle() const          { return m_nExecUseWaitForInputIdle ? true : false; }
    void SetExecUseWaitForInputIdle( bool b )       { m_nExecUseWaitForInputIdle = ( b ) ? 1 : 0; }

    int  GetExecWaitForInputTimeout() const         { return m_nExecUseWaitForInputTimeout ? true : false; }
    void SetExecWaitForInputTimeout( int n )        { m_nExecUseWaitForInputTimeout = n; }

    int  GetExecChildProcessWaitTime() const        { return m_nExecChildProcessWaitTime ? true : false; }
    void SetExecChildProcessWaitTime( int n )       { m_nExecChildProcessWaitTime = n; }

    int  GetExecLogSyncFoss() const                 { return m_nExecLogSyncFoss; }
    void SetExecLogSyncFoss( int n )                { m_nExecLogSyncFoss = n; }

    int  GetMaxNumberMessageAreas() const           { return m_nMaxNumberMessageAreas; }
    void SetMaxNumberMessageAreas( int n )          { m_nMaxNumberMessageAreas = n; }

    int  GetMaxNumberFileAreas() const              { return m_nMaxNumberFileAreas; }
    void SetMaxNumberFileAreas( int n )             { m_nMaxNumberFileAreas = n; }

    bool IsNewMailWatiting() const                  { return m_bNewMailWaiting; }
    void SetNewMailWaiting( bool b )                { m_bNewMailWaiting = b; }

    bool IsTimeOnlineLimited() const                { return m_bTimeOnlineLimited; }
    void SetTimeOnlineLimited( bool b )             { m_bTimeOnlineLimited = b; }

    int  GetCurrentNetworkType() const              { return m_nCurrentNetworkType; }
    void SetCurrentNetworkType( int n )             { m_nCurrentNetworkType = n; }

    int  GetNetworkNumber() const                   { return m_nNetworkNumber; }
    void SetNetworkNumber( int n )                  { m_nNetworkNumber = n; }

    int  GetMaxNetworkNumber() const                { return m_nMaxNetworkNumber; }
    void SetMaxNetworkNumber( int n )               { m_nMaxNetworkNumber = n; }

    int  GetNumberOfChains() const                  { return m_nNumberOfChains; }
    void SetNumberOfChains( int n )                 { m_nNumberOfChains = n; }

    int  GetNumberOfEditors() const                 { return m_nNumberOfEditors; }
    void SetNumberOfEditors( int n )                { m_nNumberOfEditors = n; }

    int  GetNumberOfExternalProtocols() const       { return m_nNumberOfExternalProtocols; }
    void SetNumberOfExternalProtocols( int n )      { m_nNumberOfExternalProtocols = n; }


private:
    bool        m_bLastKeyLocal;
    int         m_nEffectiveSl;
    WBbsApp*    m_pApplication;


public:
    //
    // Data from system_operation_rec, make it public for now, and add
    // accessors later on.
    //
    int
                m_nTopScreenColor,
                m_nUserEditorColor,
                m_nEditLineColor,
                m_nChatNameSelectionColor,
                m_nMessageColor;

    std::string m_currentSpeed;

    int	        m_nForcedReadSubNumber;
    bool        m_bThreadSubs;
    bool        m_bAllowCC;
    bool        m_bUserOnline;
    bool        m_bQuoting;
    bool        m_bNewMailWaiting;
    bool        m_bTimeOnlineLimited;

    int         m_nMMKeyArea,
                m_nNumMessagesReadThisLogon,
                m_nFileAreaCache,
                m_nMessageAreaCache,
                m_nCurrentLanguageNumber,
                m_nCurrentFileArea,
                m_nCurrentMessageArea,
                m_nCurrentReadMessageArea,
                m_nCurrentConferenceMessageArea,
                m_nCurrentConferenceFileArea,
		        m_bNewScanAtLogin,
                m_bInternalZmodem,
                m_nNumMsgsInCurrentSub,
                m_nBeginDayNodeNumber,
                m_nGlobalDebugLevel,
		        m_nExecUseWaitForInputIdle,
		        m_nExecUseWaitForInputTimeout,
		        m_nExecChildProcessWaitTime,
		        m_nExecLogSyncFoss,
                m_nMaxNumberMessageAreas,
                m_nMaxNumberFileAreas,
                m_nCurrentNetworkType,
                m_nNetworkNumber,
                m_nMaxNetworkNumber,
                m_nNumberOfChains,
                m_nNumberOfEditors,
                m_nNumberOfExternalProtocols,
                numf,
                num_dirs,
                num_languages,
                num_sec,
                num_subs,
                num_events,
                num_sys_list,
                screenbottom,
                screenlinest,
                subchg,
                tagging,
                tagptr,
                titled,
                topdata,
                topline,
                using_modem,
                numbatch,
                numbatchdl;

    std::string internetPopDomain;
    std::string internetEmailDomain;
    std::string internetEmailName;
	std::string internetFullEmailAddress;
	std::string usenetReferencesLine;
	std::string threadID;
	bool        m_bInternetUseRealNames;


	UINT32		*m_DirectoryDateCache,
				*m_SubDateCache;

    char		*pszLanguageDir;
	char		*cur_lang_name;

	int         wfcdrvs[5],
				wfc_status;

	int         usernum;

    asv_rec		asv;
    adv_asv_rec	advasv;
    cbv_rec		cbv;

    unsigned short
                mail_who_field_len,
                m_nMaxFilesPerBatch,
                max_extend_lines,
                max_chains,
                max_gfilesec,
                screen_saver_time;

    unsigned char
                newuser_colors[10],         // skip for now
                newuser_bwcolors[10];       // skip for now

// TCP/IP Handle
#if defined ( _WIN32 )
    SOCKET		hSocket;
    SOCKET		hDuplicateSocket;
    HANDLE		hCommHandle;
#endif // _WIN32


};

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif  // #if !defined (__INCLUDED_WSESSION_H__)

