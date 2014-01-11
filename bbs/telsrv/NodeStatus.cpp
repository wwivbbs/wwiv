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
// NodeStatus.cpp: implementation of the NodeStatus class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NodeStatus.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


IMPLEMENT_DYNAMIC( NodeStatus, CObject )

const char * g_szNodeActivityText[] = 
{
    "Offline",
    "Initializing BBS",
    "Sending E-Mail",
    "at Main Menu",
    "at File Menu"
    ,"Playing Online Games",
    "Network Transfer",
    "Browsing Bulletins",
    "Running Begin Day Event",
    "Running Event",
    "In Chat",
    "In Chat",
    "In Chat Room",
    "Logging On",
    "Logging Off",
    "In Full Screen Editor",
    "In UEDIT",
    "In CHAINEDIT",
    "In BOARDEDIT",
    "In DIREDIT",
    "In GFILEEDIT",
    "In CONFEDIT",
    "In DOS",
    "Changing Defauts",
    "REBOOTTING!!",
    "Executing RELOAD",
    "In Voting Booth",
    "In Time Bank",
    "In Auto-Message",
    "Browsing SubBoards",
    "Executing CHUSER",
    "In TEDIT",
    "Reading Mail",
    "Executing RESETQSCAN",
    "In VOTEEDIT",
    "In VOTEPRINT",
    "Executing RESETF",
    "Sending FeedBack ",
    "Removing Sent E-mail",
    "Posting Message",
    "New User Logging On",
    "Reading Mail",
    "Downloading File(s)",
    "Uploading File(s)",
    "Doing a BiDirectional File Transfer",
    "Browsing Network BBS List",
    "Executing Terminal Program",
    "In EVENTEDIT",
    "Getting User Information",
    NULL
};


NodeStatus::NodeStatus()
: m_pInstanceRecord(NULL)
, m_nNumBbsInstances(0)
, m_bPrefsLoaded(false)
{
	InitializeCriticalSection( &m_CriticalSection );
	for ( int i=0; i < 1025; i++ )
	{
		m_bBusyArr[i] = FALSE;
		m_lAddrArr[i] = 0;
	}
	m_nHighNode		 = 0;
	m_nLowNode		 = 0;
	m_lastUpdateTime = CTime::GetCurrentTime();
	m_szIpAddress[0] = _T( '\0' );
}


NodeStatus::~NodeStatus()
{
	DeleteCriticalSection( &m_CriticalSection );
    if ( m_pInstanceRecord != NULL )
    {
        free( m_pInstanceRecord );
        m_pInstanceRecord = NULL;
    }
}

BOOL NodeStatus::IsActive( int nNodeNumber )
{
	EnterCriticalSection( &m_CriticalSection );
	BOOL bResult = ( m_bBusyArr[ nNodeNumber ] == TRUE );
	LeaveCriticalSection( &m_CriticalSection );
	
	return bResult;
}

LPCTSTR NodeStatus::GetNodeIP( int nNodeNumber )
{
	EnterCriticalSection( &m_CriticalSection );
	in_addr a;
	a.S_un.S_addr = m_lAddrArr[ nNodeNumber ];
	lstrcpy( m_szIpAddress, CA2T(inet_ntoa( a )));
	LeaveCriticalSection( &m_CriticalSection );

	return m_szIpAddress;
}


LPCTSTR NodeStatus::GetNodeActivity( int nNodeNumber )
{
    if ( m_nNumBbsInstances == 0 || m_pInstanceRecord == NULL )
    {
        bool bUpdateRet = UpdateActivityList();
        if ( !bUpdateRet )
        {
            return _T( "" );
        }
    }

    CString strTemp;
    if (m_pInstanceRecord[nNodeNumber].loc == INST_LOC_WFC)		
    {			
        strTemp.Format( _T( "Instance #%3.3d: Waiting For Caller" ), nNodeNumber );
        lstrcpy( m_szActivity, strTemp );
    }		
    else if ( m_pInstanceRecord[nNodeNumber].loc == INST_LOC_DOWN )
    {
        strTemp.Format( _T( "Instance #%3.3d: Off-Line" ) , nNodeNumber );
        lstrcpy( m_szActivity, strTemp );
    }		
    else		
    {			
        strTemp.Format( _T( "[User #%d is %s]" ),
                        m_pInstanceRecord[nNodeNumber].user, 
                        g_szNodeActivityText[m_pInstanceRecord[nNodeNumber].loc]);			
        lstrcpy( m_szActivity, strTemp );
    }	
    return m_szActivity;    
}


bool NodeStatus::LoadConfiguration()
{
    CString fileName;
    Preferences p;
    p.Load();
    m_BbsDir = p.m_workDir;
    if ( m_BbsDir.Right(1)[0] != '\\' )
    {
        m_BbsDir += '\\';
    }
    fileName.Format( _T( "%sCONFIG.DAT" ), m_BbsDir );
    CFile hConfigFile;
    BOOL bRet = hConfigFile.Open( fileName, CFile::modeRead | CFile::typeBinary );
    if ( !bRet )
    {
        return false;
    }

    UINT nNumRead = hConfigFile.Read( &m_configRecord, sizeof( configrec ) );
    if ( nNumRead != sizeof( configrec ) )
    {
        hConfigFile.Close();
        return false;
    }

    m_DataDir = m_configRecord.datadir;

    hConfigFile.Close();
    return true;
}


bool NodeStatus::UpdateActivityList()
{
    if ( !m_bPrefsLoaded )
    {
        m_bPrefsLoaded = LoadConfiguration();
    }
    CString fileName;
    fileName.Format( _T( "%sINSTANCE.DAT" ), m_DataDir );
    CFile hInstanceFile;
    BOOL bOpenRet = hInstanceFile.Open( fileName, CFile::modeRead | CFile::typeBinary );
    if ( !bOpenRet )	
    {		
        return false;
    }	
    
    if ( m_pInstanceRecord != NULL )
    {
        free( m_pInstanceRecord );
        m_pInstanceRecord = NULL;
    }
    
    long lFileSize      = ( long ) hInstanceFile.GetLength();
    m_nNumBbsInstances  = lFileSize / sizeof( instancerec );
    m_pInstanceRecord   = static_cast<instancerec*>( malloc( lFileSize ) );

    if ( m_pInstanceRecord == NULL )
    {
        return false;
    }

    UINT nNumRead = hInstanceFile.Read( m_pInstanceRecord, lFileSize );

    hInstanceFile.Close();
    return ( (long) nNumRead == lFileSize ) ? true : false;
}



BOOL NodeStatus::SetNodeInfo( int nNodeNum, BOOL bBusy, LPCTSTR szIpAddress )
{
	EnterCriticalSection( &m_CriticalSection );
	m_bBusyArr[ nNodeNum ] = bBusy;
	m_lAddrArr[ nNodeNum ] = inet_addr(CT2A(szIpAddress));
	LeaveCriticalSection( &m_CriticalSection );
	m_lastUpdateTime = CTime::GetCurrentTime();

	return TRUE;
}


int	 NodeStatus::GetHighNode()
{
	EnterCriticalSection( &m_CriticalSection );
	int nResult = m_nHighNode;
	LeaveCriticalSection( &m_CriticalSection );
	
	return nResult;
}


int  NodeStatus::GetLowNode()
{
	EnterCriticalSection( &m_CriticalSection );
	int nResult = m_nLowNode;
	LeaveCriticalSection( &m_CriticalSection );

	return nResult;
}


void NodeStatus::SetHighNode( int nodeNum )
{
	EnterCriticalSection( &m_CriticalSection );
	m_nHighNode = nodeNum;
	LeaveCriticalSection( &m_CriticalSection );
	m_lastUpdateTime = CTime::GetCurrentTime();
}


void NodeStatus::SetLowNode( int nodeNum )
{
	EnterCriticalSection( &m_CriticalSection );
	m_nLowNode = nodeNum;
	LeaveCriticalSection( &m_CriticalSection );
	m_lastUpdateTime = CTime::GetCurrentTime();
}



int NodeStatus::GetNextAvailableNode()
{
	int nResult = 0;

	EnterCriticalSection( &m_CriticalSection );
	for ( int i=m_nLowNode; i <= m_nHighNode; i++ )
	{
		if ( m_bBusyArr[ i ] == FALSE )
		{
			nResult = i;
			break;
		}
	}
	LeaveCriticalSection( &m_CriticalSection );

	return nResult;
}


CTime& NodeStatus::GetLastUpdateTime()
{
	return m_lastUpdateTime;
}


#ifdef _DEBUG
void NodeStatus::Dump( CDumpContext& dc ) const
{
   // call base class function first
   CObject::Dump( dc );

   // now do the stuff for our specific class
   dc << _T( "No Information Available.\n" );
}

void NodeStatus::AssertValid() const
{
   // call inherited AssertValid first
   CObject::AssertValid();

   // check members...
   // ASSERT( true ); 
}

#endif


bool NodeStatus::IsInstanceOnline( int nNodeStatus )
{
    if ( nNodeStatus == INST_LOC_DOWN   ||	
         nNodeStatus == INST_LOC_INIT   ||	
         nNodeStatus == INST_LOC_NET    ||	
         nNodeStatus == INST_LOC_WFC )	
    {	
        return false;
    }
    return true;
}

