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
// WWIVTelnetServer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"

#include "MainFrame.h"
#include "WWIVTelnetServerDoc.h"
#include "NodeDetailsView.h"
#include "afxwin.h"

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerApp

BEGIN_MESSAGE_MAP(CWWIVTelnetServerApp, CWinApp)
	//{{AFX_MSG_MAP(CWWIVTelnetServerApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()


#include "../version.cpp"

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerApp construction

CWWIVTelnetServerApp::CWWIVTelnetServerApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CWWIVTelnetServerApp object

CWWIVTelnetServerApp theApp;

// This identifier was generated to be statistically unique for your app.
// You may change it if you prefer to choose a specific identifier.

// {DFDAD935-A0A2-4847-8082-76D41AC6028F}
static const CLSID clsid =
{ 0xdfdad935, 0xa0a2, 0x4847, { 0x80, 0x82, 0x76, 0xd4, 0x1a, 0xc6, 0x2, 0x8f } };

/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerApp initialization

BOOL CWWIVTelnetServerApp::InitInstance()
{
	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#elif _MSC_VER < 1300
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	SetRegistryKey( Preferences::GetRegistryKeyName() );

	LoadStdProfileSettings( 0 );  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CWWIVTelnetServerDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CNodeDetailsView));
	AddDocTemplate(pDocTemplate);

	// Connect the COleTemplateServer to the document template.
	//  The COleTemplateServer creates new documents on behalf
	//  of requesting OLE containers by using information
	//  specified in the document template.
	m_server.ConnectTemplate(clsid, pDocTemplate, TRUE);
		// Note: SDI applications register server objects only if /Embedding
		//   or /Automation is present on the command line.

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Check to see if launched as OLE server
	if ( cmdInfo.m_bRunEmbedded || cmdInfo.m_bRunAutomated)
	{
		// Register all OLE server (factories) as running.  This enables the
		//  OLE libraries to create objects from other applications.
		COleTemplateServer::RegisterAll();

		// Application was run with /Embedding or /Automation.  Don't show the
		//  main window in this case.
		return TRUE;
	}

	// When a server application is launched stand-alone, it is a good idea
	//  to update the system registry in case it has been damaged.
	m_server.UpdateRegistry( OAT_DISPATCH_OBJECT );
	COleObjectFactory::UpdateRegistryAll();

	// Dispatch commands specified on the command line
	if ( !ProcessShellCommand( cmdInfo ) )
	{
		return FALSE;
	}

	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow( SW_SHOW );
	m_pMainWnd->UpdateWindow();

    // Ok, this ulgy code gets pointers to the main frame and doc, if the server was
    // auto-started then start the timer now since the main frame isn't created when
    // the document is created.
    CWnd * pWnd = AfxGetMainWnd();
	ASSERT(pWnd->IsKindOf(RUNTIME_CLASS(CMainFrame)));
	CMainFrame* pFrameWnd = DYNAMIC_DOWNCAST(CMainFrame, pWnd);
    CDocument * pDoc = pFrameWnd->GetActiveDocument();
    ASSERT( pDoc->IsKindOf( RUNTIME_CLASS( CWWIVTelnetServerDoc ) ) );
    CWWIVTelnetServerDoc * pTelSrvDoc = DYNAMIC_DOWNCAST( CWWIVTelnetServerDoc, pDoc );
    if ( pTelSrvDoc->IsListenerStarted() )
    {
        pFrameWnd->StartAppTimer();
    }

	return TRUE;
}


int CWWIVTelnetServerApp::ExitInstance() 
{
	return CWinApp::ExitInstance();
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data

	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
    CStatic m_versionString;
    virtual BOOL OnInitDialog();
    CString m_version;
	CStatic m_copyrightString;
	afx_msg void OnStnClickedStaticCopyright();
	CString m_copyright;
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
, m_version(_T(""))
, m_copyright(_T(""))
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
	DDX_Control(pDX, IDC_STATIC_VERSION, m_versionString);
	DDX_Text(pDX, IDC_STATIC_VERSION, m_version);
	DDX_Control(pDX, IDC_STATIC_COPYRIGHT, m_copyrightString);
	DDX_Text(pDX, IDC_STATIC_COPYRIGHT, m_copyright);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
	ON_STN_CLICKED(IDC_STATIC_COPYRIGHT, OnStnClickedStaticCopyright)
END_MESSAGE_MAP()


// App command to run the dialog
void CWWIVTelnetServerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.m_version = CA2T(wwiv_version);
	aboutDlg.m_version.Append(CA2T(beta_version));
	aboutDlg.m_copyright.Format( _T( "Copyright (c) 2001-2013 Rushfan" ) );

    aboutDlg.DoModal();
}


/////////////////////////////////////////////////////////////////////////////
// CWWIVTelnetServerApp message handlers


BOOL CAboutDlg::OnInitDialog()
{
    return CDialog::OnInitDialog();
}

void CAboutDlg::OnStnClickedStaticCopyright()
{
	// TODO: Add your control notification handler code here
}
