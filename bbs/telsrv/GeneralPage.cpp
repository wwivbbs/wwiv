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
// GeneralPage.cpp : implementation file
//

#include "stdafx.h"
#include "wwivtelnetserver.h"
#include "GeneralPage.h"
#include ".\generalpage.h"


/////////////////////////////////////////////////////////////////////////////
// GeneralPage property page

IMPLEMENT_DYNCREATE(GeneralPage, CPropertyPage)

GeneralPage::GeneralPage() : CPropertyPage(GeneralPage::IDD)
, m_nLocalNode(0)
, m_bLaunchMinimized(FALSE)
, m_BeginDayEvent(FALSE)
, m_bUseBalloons(FALSE)
{
	//{{AFX_DATA_INIT(GeneralPage)
	m_bAutoStart = FALSE;
	m_cmdLine = _T("");
	m_workDir = _T("");
	m_nEndNode = 0;
	m_nPortNum = 0;
	m_nStartNode = 0;
	m_parameters = _T("");
	//}}AFX_DATA_INIT
}

GeneralPage::~GeneralPage()
{
}

void GeneralPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(GeneralPage)
    DDX_Control(pDX, IDC_PREF_WORKINGDIR, m_workDirCtrl);
    DDX_Control(pDX, IDC_PREF_CMDLINE, m_cmdLineCtrl);
    DDX_Control(pDX, IDC_PREF_ARGS, m_parametersCtrl);
    DDX_Check(pDX, IDC_PREF_AUTOSTART, m_bAutoStart);
    DDX_Text(pDX, IDC_PREF_CMDLINE, m_cmdLine);
    DDV_MaxChars(pDX, m_cmdLine, 260);
    DDX_Text(pDX, IDC_PREF_WORKINGDIR, m_workDir);
    DDV_MaxChars(pDX, m_workDir, 260);
    DDX_Text(pDX, IDC_PREF_ENDNODE, m_nEndNode);
    DDV_MinMaxInt(pDX, m_nEndNode, 0, 1024);
    DDX_Text(pDX, IDC_PREF_PORTNUM, m_nPortNum);
    DDV_MinMaxInt(pDX, m_nPortNum, 0, 99999);
    DDX_Text(pDX, IDC_PREF_STARTNODE, m_nStartNode);
    DDV_MinMaxInt(pDX, m_nStartNode, 0, 1024);
    DDX_Text(pDX, IDC_PREF_ARGS, m_parameters);
    DDV_MaxChars(pDX, m_parameters, 1024);
    //}}AFX_DATA_MAP
    DDX_Text(pDX, IDC_PREF_LOCALNODE, m_nLocalNode);
    DDV_MinMaxInt(pDX, m_nLocalNode, 0, 1024);
    DDX_Check(pDX, IDC_PREF_MINIMIZE, m_bLaunchMinimized);
    DDX_Check(pDX, IDC_PREF_BEGINDAYEVENT, m_BeginDayEvent);
    DDX_Check(pDX, IDC_PREF_USEBALLOONS, m_bUseBalloons);
}


BEGIN_MESSAGE_MAP(GeneralPage, CPropertyPage)
	//{{AFX_MSG_MAP(GeneralPage)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE1, OnButtonBrowse1)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE2, OnButtonBrowse2)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_PREF_AUTOSTART, OnBnClickedPrefAutostart)
    ON_BN_CLICKED(IDC_PREF_USEBALLOONS, OnBnClickedPrefUseballoons)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// GeneralPage message handlers

// Called by MFC when the page is de-selected or OK is pressed while this
// page is still active.
BOOL GeneralPage::OnKillActive() 
{
	BOOL ok = CPropertyPage::OnKillActive();
	if ( ok )
	{
		// Extra validation above and beyond the DDV provided by MFC
		if ( m_nEndNode < m_nStartNode )
		{
			MessageBox( _T( "Your end node must be greater than your start node" ) );
			return FALSE;
		}

	}
	return ok;
}

void GeneralPage::OnButtonBrowse1() 
{
	CFileDialog browser( TRUE,
						 _T( ".exe" ),
						 _T( "wwiv50.exe" ),
						 OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON,
						 _T( "Executables (*.exe)|*.exe||" ),
						 this );
	if ( browser.DoModal() == IDOK )
	{
		m_cmdLineCtrl.SetWindowText( browser.GetPathName() );
		CString dir;
		GetDirFromPath( browser.GetPathName(), dir );
		m_workDirCtrl.SetWindowText( dir );
	}
}

void GeneralPage::OnButtonBrowse2() 
{
	// TODO: Add your control notification handler code here
	
}

void GeneralPage::GetDirFromPath(LPCTSTR szPath, CString &dir)
{
	char path_buffer[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dirPart[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	lstrcpy( path_buffer, szPath );

	_splitpath( path_buffer, drive, dirPart, fname, ext );
	_makepath( path_buffer, drive, dirPart, NULL, NULL );

	dir = path_buffer;
}

void GeneralPage::OnBnClickedPrefAutostart()
{
	// TODO: Add your control notification handler code here
}

void GeneralPage::OnBnClickedPrefUseballoons()
{
    // TODO: Add your control notification handler code here
}
