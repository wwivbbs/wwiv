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
// MainFrame.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"
#include "WWIVTelnetServerDoc.h"

#include "GeneralPage.h"
#include "LoggingPage.h"
#include "SoundsPage.h"
#include "NewsPage.h"
#include "PreferencesSheet.h"

#include "MainFrame.h"
#include "NodeDetailsView.h"
#include "LogView.h"
#include "resource.h"
#include ".\mainframe.h"


const UINT CMainFrame::m_nTaskbarCreatedMsg = ::RegisterWindowMessage(_T("TaskbarCreated"));
const int CMainFrame::m_nTimerID            = 68;
const int CMainFrame::m_nLogMessageID       = 1;

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_COMMAND(ID_WT_PREFS, OnWtPrefs)
	ON_UPDATE_COMMAND_UI(ID_WT_PREFS, OnUpdateWtPrefs)
	ON_MESSAGE(WM_MY_TRAY_NOTIFICATION, OnTrayNotification)
	ON_WM_SYSCOMMAND()
	ON_WM_ACTIVATE()
	ON_COMMAND(ID_SHOW_MAINWINDOW, OnShowMainwindow)
	//}}AFX_MSG_MAP
	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CFrameWnd::OnHelpFinder)
	ON_UPDATE_COMMAND_UI_RANGE(AFX_ID_VIEW_MINIMUM, AFX_ID_VIEW_MAXIMUM, OnUpdateViewStyles)
	ON_COMMAND_RANGE(AFX_ID_VIEW_MINIMUM, AFX_ID_VIEW_MAXIMUM, OnViewStyle)
	ON_MESSAGE(WM_NODESTATUSCHANGED, OnNodeStatusChanged)
	ON_MESSAGE(WM_LOG_MESSAGE, OnLogMessage)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_SHOWWINDOW()
    ON_COMMAND(ID_FILE_RUNLOCALNODE, OnLocalNode)
    ON_UPDATE_COMMAND_UI(ID_FILE_RUNLOCALNODE, OnUpdateLocalNode)
    ON_COMMAND(ID_APP_UPDATES, OnAppUpdates)
    ON_REGISTERED_MESSAGE(CMainFrame::m_nTaskbarCreatedMsg, OnTaskbarCreated)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame() : m_trayIcon( IDR_TRAY )
, m_nTimer(0)
{
	InitializeCriticalSection( &m_criticalSectionNodeUpdated );
	
}

CMainFrame::~CMainFrame()
{
	DeleteCriticalSection( &m_criticalSectionNodeUpdated );
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	InitCommonControls();

	if ( CFrameWnd::OnCreate( lpCreateStruct ) == -1 )
    {
		return -1;
    }
	
	if ( !m_wndToolBar.CreateEx( this ) ||
		 !m_wndToolBar.LoadToolBar( IDR_MAINFRAME ) )
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}
	if (!m_wndDlgBar.Create(this, IDR_MAINFRAME, 
		CBRS_ALIGN_TOP, AFX_IDW_DIALOGBAR))
	{
		TRACE0("Failed to create dialogbar\n");
		return -1;		// fail to create
	}
	if (!m_wndReBar.Create(this) ||
		!m_wndReBar.AddBar(&m_wndToolBar) 
		|| !m_wndReBar.AddBar(&m_wndDlgBar))
	{
		TRACE0("Failed to create rebar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	// Note Remove this if you don't want tool tips
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY);

	m_trayIcon.SetNotificationWnd(this, WM_MY_TRAY_NOTIFICATION);

	return 0;
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/,
	CCreateContext* pContext)
{
	// create splitter window
	if (!m_wndSplitter.CreateStatic(this, 1, 2))
		return FALSE;

	if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CNodeDetailsView), CSize(300, 300), pContext) ||
		!m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CLogView), CSize(100, 100), pContext))
	{
		m_wndSplitter.DestroyWindow();
		return FALSE;
	}

	return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

CNodeDetailsView* CMainFrame::GetNodeView()
{
	CWnd* pWnd = m_wndSplitter.GetPane(0, 0);
	CNodeDetailsView* pView = DYNAMIC_DOWNCAST(CNodeDetailsView, pWnd);
	return pView;
}

void CMainFrame::OnUpdateViewStyles(CCmdUI* pCmdUI)
{
	// TODO: customize or extend this code to handle choices on the
	// View menu.

	CNodeDetailsView* pView = GetNodeView(); 

	// if the right-hand pane hasn't been created or isn't a view,
	// disable commands in our range

	if (pView == NULL)
		pCmdUI->Enable(FALSE);
	else
	{
		DWORD dwStyle = pView->GetStyle() & LVS_TYPEMASK;

		// if the command is ID_VIEW_LINEUP, only enable command
		// when we're in LVS_ICON or LVS_SMALLICON mode

		if (pCmdUI->m_nID == ID_VIEW_LINEUP)
		{
			if (dwStyle == LVS_ICON || dwStyle == LVS_SMALLICON)
				pCmdUI->Enable();
			else
				pCmdUI->Enable(FALSE);
		}
		else
		{
			// otherwise, use dots to reflect the style of the view
			pCmdUI->Enable();
			BOOL bChecked = FALSE;

			switch (pCmdUI->m_nID)
			{
			case ID_VIEW_DETAILS:
				bChecked = (dwStyle == LVS_REPORT);
				break;

			case ID_VIEW_SMALLICON:
				bChecked = (dwStyle == LVS_SMALLICON);
				break;

			case ID_VIEW_LARGEICON:
				bChecked = (dwStyle == LVS_ICON);
				break;

			case ID_VIEW_LIST:
				bChecked = (dwStyle == LVS_LIST);
				break;

			default:
				bChecked = FALSE;
				break;
			}

			pCmdUI->SetRadio( bChecked ? 1 : 0 );
		}
	}
}



void CMainFrame::OnViewStyle(UINT nCommandID)
{
	// TODO: customize or extend this code to handle choices on the
	// View menu.
	CNodeDetailsView* pView = GetNodeView();

	// if the right-hand pane has been created and is a CNodeDetailsView,
	// process the menu commands...
	if (pView != NULL)
	{
		DWORD dwStyle = -1;

		switch (nCommandID)
		{
		case ID_VIEW_LINEUP:
			{
				// ask the list control to snap to grid
				CListCtrl& refListCtrl = pView->GetListCtrl();
				refListCtrl.Arrange(LVA_SNAPTOGRID);
			}
			break;

		// other commands change the style on the list control
		case ID_VIEW_DETAILS:
			dwStyle = LVS_REPORT;
			break;

		case ID_VIEW_SMALLICON:
			dwStyle = LVS_SMALLICON;
			break;

		case ID_VIEW_LARGEICON:
			dwStyle = LVS_ICON;
			break;

		case ID_VIEW_LIST:
			dwStyle = LVS_LIST;
			break;
		}

		// change the style; window will repaint automatically
		if (dwStyle != -1)
        {
			pView->ModifyStyle(LVS_TYPEMASK, dwStyle);
        }
	}
}


void CMainFrame::OnWtPrefs() 
{
	Preferences prefsData;

	CPreferencesSheet dlgPreferences( _T("Preferences"), this );

	GeneralPage generalPage;
	generalPage.m_bAutoStart	    = prefsData.m_bAutoStart;
	generalPage.m_nPortNum		    = prefsData.m_nPortNum;
	generalPage.m_nStartNode	    = prefsData.m_nStartNode;
	generalPage.m_nEndNode		    = prefsData.m_nEndNode;
	generalPage.m_cmdLine		    = prefsData.m_cmdLine;
	generalPage.m_workDir		    = prefsData.m_workDir;
	generalPage.m_parameters	    = prefsData.m_parameters;
    generalPage.m_nLocalNode        = prefsData.m_nLocalNode;
    generalPage.m_bLaunchMinimized  = ( prefsData.m_bLaunchMinimized ) ? TRUE : FALSE;
    generalPage.m_BeginDayEvent     = ( prefsData.m_bRunBeginDayEvent ) ? TRUE : FALSE;
    generalPage.m_bUseBalloons      = ( prefsData.m_bUseBalloons ) ? TRUE : FALSE;

	dlgPreferences.AddPage( &generalPage );

	LoggingPage loggingPage;
	loggingPage.m_strFileName	    = prefsData.m_strLogFileName;
	loggingPage.m_radioButtons      = prefsData.m_nLogStyle;

	dlgPreferences.AddPage( &loggingPage );

    SoundsPage soundsPage;
    soundsPage.m_bSoundsEnabled     = ( prefsData.m_bUseSounds ) ? TRUE : FALSE;
    soundsPage.m_strLogonSound      = prefsData.m_strLogonSound;
    soundsPage.m_strLogoffSound     = prefsData.m_strLogoffSound;
    dlgPreferences.AddPage( &soundsPage );

	NewsPage newsPage;
	newsPage.m_bEnableNNTPServer    = prefsData.m_bEnableNNTPServer ? TRUE : FALSE;
	dlgPreferences.AddPage( &newsPage );

	int nRet = dlgPreferences.DoModal();

	if ( nRet == IDOK )
	{
		// Marshal the data back over to the other class.
		prefsData.m_bAutoStart		    = generalPage.m_bAutoStart;
		prefsData.m_nPortNum		    = generalPage.m_nPortNum;
		prefsData.m_nStartNode		    = generalPage.m_nStartNode;
		prefsData.m_nEndNode		    = generalPage.m_nEndNode;
		prefsData.m_cmdLine			    = generalPage.m_cmdLine;
		prefsData.m_workDir			    = generalPage.m_workDir;
		prefsData.m_parameters		    = generalPage.m_parameters;
        prefsData.m_nLocalNode          = generalPage.m_nLocalNode;
        prefsData.m_bLaunchMinimized    = ( generalPage.m_bLaunchMinimized ) ? true : false;
        prefsData.m_bRunBeginDayEvent   = ( generalPage.m_BeginDayEvent ) ? true : false;
        prefsData.m_bUseBalloons        = ( generalPage.m_bUseBalloons ) ? true : false;

		prefsData.m_strLogFileName	    = loggingPage.m_strFileName;
		prefsData.m_nLogStyle		    = loggingPage.m_radioButtons;

        prefsData.m_bUseSounds          = ( soundsPage.m_bSoundsEnabled ) ? true : false;
        prefsData.m_strLogonSound       = soundsPage.m_strLogonSound;
        prefsData.m_strLogoffSound      = soundsPage.m_strLogoffSound;

		prefsData.m_bEnableNNTPServer	= newsPage.m_bEnableNNTPServer ? true : false;

		if ( !prefsData.Commit() )
		{
			// %%TODO: Show error message, but Commit always returns
			// TRUE right now...
		}

		UpdateNodeStatus( 0 );
	}
	
}



void CMainFrame::OnUpdateWtPrefs(CCmdUI* pCmdUI) 
{
	CDocument* pDoc = GetActiveDocument();
	CWWIVTelnetServerDoc* pTSDoc = DYNAMIC_DOWNCAST( CWWIVTelnetServerDoc, pDoc );
	pCmdUI->Enable( pTSDoc->IsListenerStarted() ? FALSE : TRUE );
}


BOOL CMainFrame::UpdateNodeStatus( int nNodeNumber, int nExitCode )
{
	CDocument* pDoc = GetActiveDocument();
	CWWIVTelnetServerDoc* pTSDoc = DYNAMIC_DOWNCAST( CWWIVTelnetServerDoc, pDoc );

	BOOL bRet = pTSDoc->UpdateNodeStatus( nNodeNumber );
	ASSERT( bRet );
	return bRet;
}



LRESULT CMainFrame::OnNodeStatusChanged(WPARAM w, LPARAM l)
{
	BOOL bRet = UpdateNodeStatus( w, l );
	ASSERT( bRet );
	return 0;
}



//////////////////
// Handle notification from tray icon: display a message.
//
LRESULT CMainFrame::OnTrayNotification(WPARAM uID, LPARAM lEvent)
{
	// let tray icon do default stuff
	return m_trayIcon.OnTrayNotification(uID, lEvent);
}

void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam) 
{
	if ( nID == SC_MINIMIZE )
	{
		ShowWindow( SW_HIDE );
		m_trayIcon.SetIcon( IDI_ICON1,  _T( "WWIV Telnet Server Active" ) );
	}
	else
	{
		CFrameWnd::OnSysCommand(nID, lParam);
	}
}

LRESULT CMainFrame::OnTaskbarCreated( WPARAM uID, LPARAM lEvent ) 
{
    m_trayIcon.SetIcon( 0 );
    m_trayIcon.SetIcon( IDI_ICON1, _T( "WWIV Telnet Server Active" ) );
	return 0;
}


void CMainFrame::OnShowMainwindow() 
{
	ShowWindow( SW_SHOW );
	m_trayIcon.SetIcon( 0 );
}


LRESULT CMainFrame::OnLogMessage(WPARAM w, LPARAM l)
{
    if ( w == CMainFrame::m_nLogMessageID )
    {
        CString* pCString = ( CString* ) l;
        ASSERT( pCString );
        CString strMessage = (LPCTSTR) *pCString;

    	CDocument* pDoc = GetActiveDocument();
        ASSERT( pDoc );
	    CWWIVTelnetServerDoc* pTSDoc = DYNAMIC_DOWNCAST( CWWIVTelnetServerDoc, pDoc );
        ASSERT( pTSDoc );
        pTSDoc->AppendLogText( strMessage );
        if ( pCString )
        {
            delete pCString;
        }
    }
    return 0;
}

void CMainFrame::OnTimer(UINT nIDEvent)
{
#ifndef NDEBUG
    {
        CTime t = CTime::GetCurrentTime();
        CString strDebug;
        strDebug.Format( _T( "Timer ID = [%ld] at [%2.2d:%2.2d:%2.2d]\r\n" ), nIDEvent, t.GetHour(), t.GetMinute(), t.GetSecond() );
        
        OutputDebugString( strDebug );
    }
#endif // NDEBUG
    if ( nIDEvent == CMainFrame::m_nTimerID )
    {
        // Handle our stuff here.
        GetActiveDocument()->UpdateAllViews( NULL );
        Preferences prefs;
        if ( prefs.Load() )
        {
            CString strNow;
            CTime t = CTime::GetCurrentTime();
            strNow.Format( _T( "%4.4d%2.2d%2.2d" ), t.GetYear(), t.GetMonth(), t.GetDay() );
            if ( prefs.m_strLastBeginEventDate.Compare( strNow ) != 0 )
            {
                RunBeginDayEvent();
            }
        }
    }

    CFrameWnd::OnTimer(nIDEvent);
}

bool CMainFrame::StartAppTimer()
{
    if ( m_nTimer != 0 )
    {
        return true;
    }

    m_nTimer = SetTimer( CMainFrame::m_nTimerID, 5000, 0 ); // 5 seconds
    return ( m_nTimer != 0 ) ? true : false;
}

bool CMainFrame::StopAppTimer()
{
    if ( m_nTimer != 0 )
    {
        KillTimer( m_nTimer );
        m_nTimer = 0;
    }
    return true;
}

void CMainFrame::OnDestroy()
{
    StopAppTimer();
    CFrameWnd::OnDestroy();
}

void CMainFrame::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CFrameWnd::OnShowWindow(bShow, nStatus);

    // TODO: Add your message handler code here
}

void CMainFrame::OnLocalNode()
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    Preferences prefs;
    prefs.Load();

    CString cmdLine;
    cmdLine.Format( _T( "%s -n%d -m" ), prefs.m_cmdLine, prefs.m_nLocalNode );
    TCHAR szCmdLine[ MAX_PATH ];
    _stprintf( szCmdLine, cmdLine );

    if ( !CreateProcess( NULL,
        szCmdLine,
        NULL,
        NULL,
        FALSE, // inherit handles
        0,
        NULL,
        prefs.m_workDir,
        &si,
        &pi ) )
    {
        //TODO Show error
    }

}

void CMainFrame::OnUpdateLocalNode(CCmdUI *pCmdUI)
{
    Preferences prefs;
    prefs.Load();

    if ( prefs.m_nLocalNode > 0 )
    {
        pCmdUI->Enable( TRUE );
    }
    else
    {
        pCmdUI->Enable( FALSE );
    }

}


void CMainFrame::OnAppUpdates()
{
    // TODO: Add your command handler code here
    MessageBox( _T("This functionality is not enabled yet."), _T( "Check for WWIV Updates"), MB_OK );
}

// Executes the beginday event for WWIV
bool CMainFrame::RunBeginDayEvent(void)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    Preferences prefs;
    prefs.Load();

    CTime t = CTime::GetCurrentTime();
    prefs.m_strLastBeginEventDate.Format( _T( "%4.4d%2.2d%2.2d" ), t.GetYear(), t.GetMonth(), t.GetDay() );
    prefs.Commit();

    CString cmdLine;
    cmdLine.Format( _T( "%s -n%d -e" ), prefs.m_cmdLine, prefs.m_nLocalNode );
    TCHAR szCmdLine[ MAX_PATH ];
    _stprintf( szCmdLine, cmdLine );

    CDocument* pDoc = GetActiveDocument();
    ASSERT( pDoc );
	CWWIVTelnetServerDoc* pTSDoc = DYNAMIC_DOWNCAST( CWWIVTelnetServerDoc, pDoc );
    ASSERT( pTSDoc );


    if ( !CreateProcess( NULL,
        szCmdLine,
        NULL,
        NULL,
        FALSE, // inherit handles
        0,
        NULL,
        prefs.m_workDir,
        &si,
        &pi ) )
    {
        pTSDoc->AppendLogText( _T( "ERROR: Failed to Invoke WWIV For BeginDay Event" ) );

        return false;
        //TODO Show error
    }
    pTSDoc->AppendLogText( _T( "Invoked WWIV For BeginDay Event" ) );
    return true;
}

BOOL CMainFrame::ShowBalloon( LPCTSTR pszTitle, LPCTSTR pszText )
{
    return m_trayIcon.ShowBalloon( pszTitle, pszText );
}
