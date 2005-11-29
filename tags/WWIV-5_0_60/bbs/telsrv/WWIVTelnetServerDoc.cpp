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
// WWIVTelnetServerDoc.cpp : implementation of the CWWIVTelnetServerDoc class
//

#include "stdafx.h"
#include "MainFrame.h"

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerDoc

IMPLEMENT_DYNCREATE(CWWIVTelnetServerDoc, CDocument)

BEGIN_MESSAGE_MAP(CWWIVTelnetServerDoc, CDocument)
	//{{AFX_MSG_MAP(CWWIVTelnetServerDoc)
	ON_COMMAND(ID_TELSRV_START, OnTelsrvStart)
	ON_UPDATE_COMMAND_UI(ID_TELSRV_START, OnUpdateTelsrvStart)
	ON_COMMAND(ID_TELSRV_STOP, OnTelsrvStop)
	ON_UPDATE_COMMAND_UI(ID_TELSRV_STOP, OnUpdateTelsrvStop)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BEGIN_DISPATCH_MAP(CWWIVTelnetServerDoc, CDocument)
	//{{AFX_DISPATCH_MAP(CWWIVTelnetServerDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//      DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_DISPATCH_MAP
END_DISPATCH_MAP()

// Note: we add support for IID_IWWIVTelnetServer to support typesafe binding
//  from VBA.  This IID must match the GUID that is attached to the 
//  dispinterface in the .ODL file.

// {0E343FA6-E99A-4591-BF1D-E5AE0C5F8D65}
static const IID IID_IWWIVTelnetServer =
{ 0xe343fa6, 0xe99a, 0x4591, { 0xbf, 0x1d, 0xe5, 0xae, 0xc, 0x5f, 0x8d, 0x65 } };

BEGIN_INTERFACE_MAP(CWWIVTelnetServerDoc, CDocument)
	INTERFACE_PART(CWWIVTelnetServerDoc, IID_IWWIVTelnetServer, Dispatch)
END_INTERFACE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerDoc construction/destruction

CWWIVTelnetServerDoc::CWWIVTelnetServerDoc()
{
	EnableAutomation();

	AfxOleLockApp();

	m_bIsOpen = FALSE;
	m_bListenerStarted = FALSE;

	WORD wVersionRequested = 0x0101;
	WSAData wsaData;
	int nRet = WSAStartup( wVersionRequested, &wsaData );
	if ( nRet != 0 )
	{
		MessageBox(NULL, _T( "Error Initializing WinSock" ) , _T( "Oops" ), MB_OK );
	}

	if ( AfxIsValidString( wsaData.szDescription ) )
	{
		m_wsaDescription	= wsaData.szDescription;
	}
	if ( AfxIsValidString( wsaData.szSystemStatus ) )
	{
		m_wsaSystemStatus	= wsaData.szSystemStatus;
	}
	if ( AfxIsValidString( wsaData.lpVendorInfo ) )
	{
		m_wsaVendorInfo		= wsaData.lpVendorInfo;
	}

}

CWWIVTelnetServerDoc::~CWWIVTelnetServerDoc()
{
	AfxOleUnlockApp();
    int nRet = WSACleanup( );
    if ( nRet != 0 )
    {
        CString strError;
        strError.Format( _T( "Error [%d] Shutting Down WinSock" ), WSAGetLastError() );
        AfxMessageBox( strError, MB_ICONERROR );
    }
}

BOOL CWWIVTelnetServerDoc::OnNewDocument()
{
	if ( !m_bIsOpen )
	{
		if (!CDocument::OnNewDocument())
		{
			return FALSE;
		}
		Preferences prefs;
		if ( prefs.m_bAutoStart )
		{
			OnTelsrvStart();
		}
	}

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)
	m_bIsOpen = TRUE;

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerDoc serialization

void CWWIVTelnetServerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerDoc diagnostics

#ifdef _DEBUG
void CWWIVTelnetServerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CWWIVTelnetServerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerDoc commands

void CWWIVTelnetServerDoc::DeleteContents() 
{
	// TODO: Add your specialized code here and/or call the base class
	if ( m_bIsOpen && m_bListenerStarted)
	{
		StopListener();
	}
	CDocument::DeleteContents();
}

BOOL CWWIVTelnetServerDoc::StartListener()
{
	ASSERT( m_bListenerStarted == FALSE );
	m_bListenerStarted = TRUE;
	BOOL bRet = m_telnetserver.StartServer( this );
	if (!bRet)
	{
		m_bListenerStarted = FALSE;
		this->AppendLogText( _T( "ERROR: Telnet Listener Failed to Start" ) );
	}
	else
	{
		this->AppendLogText( _T( "INFO:  Started Telnet Listener" ) );
	}
	UpdateNodeStatus( 0 );
	return bRet;
}

BOOL CWWIVTelnetServerDoc::StopListener()
{
	ASSERT( m_bListenerStarted == TRUE );
	m_bListenerStarted = FALSE;
	BOOL bRet = m_telnetserver.StopServer();
	this->AppendLogText( _T( "INFO:  Stopped Telnet Listener" ) );
	UpdateNodeStatus( 0 );
	return bRet;
}



NodeStatus* CWWIVTelnetServerDoc::GetNodeStatus()
{
	return m_telnetserver.GetNodeStatus();
}


BOOL CWWIVTelnetServerDoc::IsListenerStarted()
{	
	return m_bListenerStarted;
}

BOOL CWWIVTelnetServerDoc::UpdateNodeStatus( int nNodeNumber, int nExitCode )
{
	NodeStatus* nodeStatus = GetNodeStatus();

	if ( nNodeNumber > 0 )
	{
		BOOL bIsActive = nodeStatus->IsActive( nNodeNumber );
		CString msg;
		if ( !bIsActive )
		{
            if ( nExitCode != STILL_ACTIVE )
            {
			    msg.Format( _T( "INFO:  Node #%d disconnected with exit code [%d]" ), nNodeNumber, nExitCode );
            }
            else
            {
			    msg.Format( _T( "INFO:  Node #%d disconnected" ), nNodeNumber );
            }
            Preferences prefs;
            if ( prefs.m_bUseBalloons )
            {
                CString balloonMessage;
                balloonMessage.Format( _T( "Node #%d Disconnected" ), nNodeNumber );
                GetMainFrame()->ShowBalloon( _T( "WWIV Telnet Server" ), balloonMessage );
            }
		}
		else
		{
			CString strIPAddress = nodeStatus->GetNodeIP( nNodeNumber );
			msg.Format( _T( "INFO:  Node #%d connected with remote IP Address [%s]" ), nNodeNumber, strIPAddress );

            Preferences prefs;
            if ( prefs.m_bUseBalloons )
            {
                CString balloonMessage;
                balloonMessage.Format( _T( "Node #%d connected from [%s]" ), nNodeNumber, strIPAddress );
                GetMainFrame()->ShowBalloon( _T( "WWIV Telnet Server" ), balloonMessage );
            }
		}
		AppendLogText( msg );
	}

	if ( nodeStatus->GetLastUpdateTime() > m_lastUpdateTime )
	{
		m_lastUpdateTime = CTime::GetCurrentTime();
		UpdateAllViews( NULL );
	}
	else if (! IsListenerStarted() )
	{
		UpdateAllViews( NULL );
	}

	return TRUE;
}


CMainFrame* CWWIVTelnetServerDoc::GetMainFrame()
{
	CWnd* pWnd = AfxGetMainWnd();
    if ( !pWnd )
    {
        return NULL;
    }
	ASSERT(pWnd->IsKindOf(RUNTIME_CLASS(CMainFrame)));
	CMainFrame* pFrameWnd = DYNAMIC_DOWNCAST(CMainFrame, pWnd);
    return pFrameWnd;
}


void CWWIVTelnetServerDoc::OnTelsrvStart() 
{
	BOOL bRet = StartListener();

    if ( !bRet )
    {
        CString text = _T( "Failed to Start the Listener." );
        AfxMessageBox( text );
    }
    CMainFrame* pMainWnd = GetMainFrame();
    if ( pMainWnd )
    {
        pMainWnd->StartAppTimer();
    }
}

void CWWIVTelnetServerDoc::OnUpdateTelsrvStart(CCmdUI* pCmdUI) 
{
	BOOL bEnable = ( IsListenerStarted() ? FALSE : TRUE );
	pCmdUI->Enable( bEnable );
}

void CWWIVTelnetServerDoc::OnTelsrvStop() 
{
    CMainFrame * pMainWnd = GetMainFrame();
    if ( pMainWnd )
    {
        pMainWnd->StopAppTimer();
    }
	VERIFY( StopListener() );
	
}

void CWWIVTelnetServerDoc::OnUpdateTelsrvStop(CCmdUI* pCmdUI) 
{
	BOOL bEnable = ( IsListenerStarted() ? TRUE : FALSE );
	pCmdUI->Enable( bEnable );
	
}

void CWWIVTelnetServerDoc::OnCloseDocument() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CDocument::OnCloseDocument();
}

BOOL CWWIVTelnetServerDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;
	
	// TODO: Add your specialized creation code here
	
	return TRUE;
}

BOOL CWWIVTelnetServerDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDocument::OnSaveDocument(lpszPathName);
}

void CWWIVTelnetServerDoc::SetStatusBarText( LPCTSTR szText )
{
	CWnd* pWnd = AfxGetMainWnd();
	ASSERT(pWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd)));
	CFrameWnd* pFrameWnd = DYNAMIC_DOWNCAST(CFrameWnd, pWnd);
	CWnd* pStatusBar = pFrameWnd->GetMessageBar();
	pStatusBar->SetWindowText( szText );

}

CString& CWWIVTelnetServerDoc::GetLogText()
{
	return m_strLogText;
}

void CWWIVTelnetServerDoc::ClearLogText()
{
	m_strLogText.Empty();
	this->UpdateAllViews( NULL, 0L, NULL );
}

void CWWIVTelnetServerDoc::AppendLogText( LPCTSTR pszNewText )
{
	CTime timeNow = CTime::GetCurrentTime();
	CString strTimeNow = timeNow.Format( _T( "[%Y-%m-%d %H:%M:%S]" ) );
	
	CString strLogLine;
	strLogLine.Format( _T( "%s %s\r\n" ), strTimeNow, pszNewText );
	
	m_strLogText += strLogLine;
	
	this->UpdateAllViews( NULL, 0L, NULL );

	Preferences prefs;
	if ( prefs.m_nLogStyle == Preferences::LOG_FILESYSTEM )
	{
		DWORD dwAccess = GENERIC_WRITE | GENERIC_READ;
		DWORD dwShareMode = FILE_SHARE_READ;
		HANDLE hFile = CreateFile(	prefs.m_strLogFileName,
									dwAccess, 
									dwShareMode, 
									NULL, 
									OPEN_ALWAYS, 
									FILE_ATTRIBUTE_NORMAL, 
									NULL );
		if ( hFile == INVALID_HANDLE_VALUE )
		{
			CString strError;
			strError.Format( _T( "%s Error [%ld] while attempting to open to File [%s]\r\n" ), 
								strTimeNow, GetLastError(), prefs.m_strLogFileName );
			m_strLogText += strError;
			return;
		}

		// move to the end of the file
		DWORD dwPos = SetFilePointer( hFile, 0L, NULL, FILE_END );
		DWORD dwNumWritten = 0;
		BOOL bOk = WriteFile( hFile, (LPCTSTR)strLogLine, strLogLine.GetLength(), &dwNumWritten, NULL );
		if ( !bOk )
		{
			CString strError;
			strError.Format( _T( "Error [%ld] while attempting to write to File [%s]\r\n" ), GetLastError(), prefs.m_strLogFileName );
			m_strLogText += strError;
		}
		CloseHandle( hFile );
	}
}
