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
// Preferences.cpp: implementation of the Preferences class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Preferences.h"

const int Preferences::LOG_FILESYSTEM	= 0;
const int Preferences::LOG_NONE			= 1;
const int Preferences::LOG_EVENTLOG		= 2;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Preferences::Preferences()
: m_bUseSounds( false )
, m_strLogonSound( _T( "" ) )
, m_strLogoffSound( _T( "" ) )
, m_nLocalNode( 0 )
, m_bLaunchMinimized( false )
, m_bRunBeginDayEvent( false )
, m_strLastBeginEventDate( _T( "" ) )
, m_bEnableNNTPServer( false )
{
	this->Load();
}

Preferences::~Preferences()
{
}

LPCTSTR Preferences::GetRegistryKeyName()
{
	return _T( "WWIV Software Services" );
}

BOOL Preferences::Commit()
{
	CWinApp *app = ::AfxGetApp();
	
	LPCTSTR pszSectionName = _T( "Preferences" );
	app->WriteProfileInt( pszSectionName, "AutoStart",	( m_bAutoStart ) ? 1 : 0 );
	app->WriteProfileInt( pszSectionName, "PortNumber", m_nPortNum );
	app->WriteProfileInt( pszSectionName, "StartNode",	m_nStartNode );
	app->WriteProfileInt( pszSectionName, "EndNode",	m_nEndNode );
	app->WriteProfileString( pszSectionName, "CommandLine", m_cmdLine );
	app->WriteProfileString( pszSectionName, "WorkingDir", m_workDir );
	app->WriteProfileString( pszSectionName, "Parameters", m_parameters );
    app->WriteProfileInt( pszSectionName, "LocalNodeNum",	m_nLocalNode );
	app->WriteProfileInt( pszSectionName, "LaunchMinimized",	( m_bLaunchMinimized ) ? 1 : 0 );

	app->WriteProfileInt( pszSectionName, "LogStyle", m_nLogStyle );
	app->WriteProfileString( pszSectionName, "LogFileName", m_strLogFileName );

	app->WriteProfileInt( pszSectionName, "EnableSounds",	( m_bUseSounds ) ? 1 : 0 );
    app->WriteProfileString( pszSectionName, "LogonSound", m_strLogonSound );
    app->WriteProfileString( pszSectionName, "LogoffSound", m_strLogoffSound );

    app->WriteProfileInt( pszSectionName, "RunBeginDayEvent", ( m_bRunBeginDayEvent ) ? 1 : 0 );

    app->WriteProfileString( pszSectionName, "LastBeginEventDate", m_strLastBeginEventDate );

	return TRUE;
}

BOOL Preferences::Load()
{
	CWinApp *app = ::AfxGetApp();

	LPCTSTR pszSectionName = _T( "Preferences" );
	int nAutoStart = app->GetProfileInt( pszSectionName, "AutoStart", 0 );
	m_bAutoStart = ( nAutoStart ) ? 1 : 0;
	m_nPortNum = app->GetProfileInt( pszSectionName, "PortNumber", 23 );
	m_nStartNode = app->GetProfileInt( pszSectionName, "StartNode", 2 );
	m_nEndNode = app->GetProfileInt( pszSectionName, "EndNode", 4 );
	m_cmdLine = app->GetProfileString( pszSectionName, "CommandLine", "C:\\wwiv\\wwiv50.exe" );
	m_workDir = app->GetProfileString( pszSectionName, "WorkingDir", "C:\\wwiv" );
	m_parameters = app->GetProfileString( pszSectionName, "Parameters", "-XT -H@H -N@N" );
    m_nLocalNode = app->GetProfileInt( pszSectionName, "LocalNodeNum", 1 );
	int nLaunchMinimized = app->GetProfileInt( pszSectionName, "LaunchMinimized", 0 );
	m_bLaunchMinimized = ( nLaunchMinimized ) ? 1 : 0;
	
	m_nLogStyle = app->GetProfileInt( pszSectionName, "LogStyle", Preferences::LOG_NONE );
	m_strLogFileName = app->GetProfileString( pszSectionName, "LogFileName", "" );

	int nUseSounds = app->GetProfileInt( pszSectionName, "EnableSounds", 0 );
    m_bUseSounds = ( nUseSounds ) ? true : false;
    m_strLogonSound = app->GetProfileString( pszSectionName, "LogonSound", "" );
    m_strLogoffSound = app->GetProfileString( pszSectionName, "LogoffSound", "" );

    int nRunBeginDayEvent = app->GetProfileInt( pszSectionName, "RunBeginDayEvent", 0 );
    m_bRunBeginDayEvent = ( nRunBeginDayEvent ) ? true : false;

    m_strLastBeginEventDate = app->GetProfileString( pszSectionName, "LastBeginEventDate", "20020101" );

	return TRUE;
}
