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
, m_bUseBalloons( false )
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
	app->WriteProfileInt( pszSectionName,       _T( "AutoStart" ),	( m_bAutoStart ) ? 1 : 0 );
	app->WriteProfileInt( pszSectionName,       _T( "PortNumber" ), m_nPortNum );
	app->WriteProfileInt( pszSectionName,       _T( "StartNode" ),	m_nStartNode );
	app->WriteProfileInt( pszSectionName,       _T( "EndNode" ),	m_nEndNode );
	app->WriteProfileString( pszSectionName,    _T( "CommandLine" ), m_cmdLine );
	app->WriteProfileString( pszSectionName,    _T( "WorkingDir" ), m_workDir );
	app->WriteProfileString( pszSectionName,    _T( "Parameters" ), m_parameters );
    app->WriteProfileInt( pszSectionName,       _T( "LocalNodeNum" ),	m_nLocalNode );
	app->WriteProfileInt( pszSectionName,       _T( "LaunchMinimized" ),	( m_bLaunchMinimized ) ? 1 : 0 );

	app->WriteProfileInt( pszSectionName,       _T( "LogStyle" ), m_nLogStyle );
	app->WriteProfileString( pszSectionName,    _T( "LogFileName" ), m_strLogFileName );

	app->WriteProfileInt( pszSectionName,       _T( "EnableSounds" ),	( m_bUseSounds ) ? 1 : 0 );
    app->WriteProfileString( pszSectionName,    _T( "LogonSound" ), m_strLogonSound );
    app->WriteProfileString( pszSectionName,    _T( "LogoffSound" ), m_strLogoffSound );

    app->WriteProfileInt( pszSectionName,       _T( "RunBeginDayEvent" ), ( m_bRunBeginDayEvent ) ? 1 : 0 );
    app->WriteProfileInt( pszSectionName,       _T( "UseBalloons" ), ( m_bUseBalloons ) ? 1 : 0 );

    app->WriteProfileString( pszSectionName,    _T( "LastBeginEventDate" ), m_strLastBeginEventDate );

	return TRUE;
}

BOOL Preferences::Load()
{
	CWinApp *app = ::AfxGetApp();

	LPCTSTR pszSectionName = _T( "Preferences" );
	int nAutoStart = app->GetProfileInt( pszSectionName, _T( "AutoStart" ), 0 );
	m_bAutoStart = ( nAutoStart ) ? 1 : 0;
	m_nPortNum = app->GetProfileInt( pszSectionName, _T( "PortNumber" ), 23 );
	m_nStartNode = app->GetProfileInt( pszSectionName, _T( "StartNode" ), 2 );
	m_nEndNode = app->GetProfileInt( pszSectionName, _T( "EndNode" ), 4 );
	m_cmdLine = app->GetProfileString( pszSectionName, _T( "CommandLine" ), _T( "C:\\wwiv\\wwiv50.exe" ) );
	m_workDir = app->GetProfileString( pszSectionName, _T( "WorkingDir" ), _T( "C:\\wwiv" ) );
	m_parameters = app->GetProfileString( pszSectionName, _T( "Parameters" ), _T("-XT -H@H -N@N"));
    m_nLocalNode = app->GetProfileInt( pszSectionName, _T( "LocalNodeNum" ), 1 );
	int nLaunchMinimized = app->GetProfileInt( pszSectionName, _T( "LaunchMinimized" ), 0 );
	m_bLaunchMinimized = ( nLaunchMinimized ) ? 1 : 0;
	
	m_nLogStyle = app->GetProfileInt( pszSectionName, _T( "LogStyle" ), Preferences::LOG_NONE );
	m_strLogFileName = app->GetProfileString( pszSectionName, _T( "LogFileName" ), _T("") );

	int nUseSounds = app->GetProfileInt( pszSectionName, _T( "EnableSounds" ), 0 );
    m_bUseSounds = ( nUseSounds ) ? true : false;
	m_strLogonSound = app->GetProfileString(pszSectionName, _T("LogonSound"), _T(""));
	m_strLogoffSound = app->GetProfileString(pszSectionName, _T("LogoffSound"), _T(""));

    int nRunBeginDayEvent = app->GetProfileInt( pszSectionName, _T( "RunBeginDayEvent" ), 0 );
    m_bRunBeginDayEvent = ( nRunBeginDayEvent ) ? true : false;
    
    int nUseBalloons = app->GetProfileInt( pszSectionName, _T( "UseBalloons" ), 1 );
    m_bUseBalloons = ( nUseBalloons ) ? true : false;

    m_strLastBeginEventDate = app->GetProfileString( pszSectionName, _T( "LastBeginEventDate" ), _T("20020101"));

	return TRUE;
}
