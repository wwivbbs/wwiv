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
// Preferences.h: interface for the Preferences class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

class Preferences  
{
public:
	static const int LOG_FILESYSTEM;
	static const int LOG_NONE;
	static const int LOG_EVENTLOG;
public:
	CString m_parameters;
	CString m_workDir;
	BOOL Load();
	CString m_cmdLine;
	BOOL Commit();
	int m_nPortNum;
	int m_nEndNode;
	int m_nStartNode;
	BOOL m_bAutoStart;


	CString m_strLogFileName;
	int m_nLogStyle;

	static LPCTSTR GetRegistryKeyName();
	Preferences();
	virtual ~Preferences();

    bool m_bUseSounds;
    CString m_strLogonSound;
    CString m_strLogoffSound;
    int m_nLocalNode;
    bool m_bLaunchMinimized;
    bool m_bRunBeginDayEvent;
    bool m_bUseBalloons;
    CString m_strLastBeginEventDate;
	// // Should the NNTP server be enabled
	bool m_bEnableNNTPServer;
};

