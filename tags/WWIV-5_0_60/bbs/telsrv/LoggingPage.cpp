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
// LoggingPage.cpp : implementation file
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"
#include "LoggingPage.h"


/////////////////////////////////////////////////////////////////////////////
// LoggingPage property page

IMPLEMENT_DYNCREATE(LoggingPage, CPropertyPage)

LoggingPage::LoggingPage() : CPropertyPage(LoggingPage::IDD)
{
	//{{AFX_DATA_INIT(LoggingPage)
	m_strFileName = _T("");
	m_radioButtons = -1;
	//}}AFX_DATA_INIT
}

LoggingPage::~LoggingPage()
{
}

void LoggingPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LoggingPage)
	DDX_Control(pDX, IDC_BUTTON_BROWSE_LOGFILENAME, m_btnBrowse);
	DDX_Control(pDX, IDC_EDIT_FILENAME, m_ctrlFileName);
	DDX_Control(pDX, IDC_RADIO_FILE, m_radioFile);
	DDX_Text(pDX, IDC_EDIT_FILENAME, m_strFileName);
	DDV_MaxChars(pDX, m_strFileName, 260);
	DDX_Radio(pDX, IDC_RADIO_FILE, m_radioButtons);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(LoggingPage, CPropertyPage)
	//{{AFX_MSG_MAP(LoggingPage)
	ON_BN_CLICKED(IDC_RADIO_EVENT, OnRadioEvent)
	ON_BN_CLICKED(IDC_RADIO_FILE, OnRadioFile)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE_LOGFILENAME, OnButtonBrowseLogfilename)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LoggingPage message handlers


void LoggingPage::OnRadioEvent() 
{
	UpdateDialogControls();
}


void LoggingPage::OnRadioFile() 
{
	UpdateDialogControls();
}


BOOL LoggingPage::UpdateDialogControls()
{
	int nCheck = m_radioFile.GetCheck();
	BOOL bEnabled = ( nCheck ) ? TRUE : FALSE;
	m_ctrlFileName.EnableWindow( bEnabled );
	m_btnBrowse.EnableWindow( bEnabled );
	return TRUE;
}

BOOL LoggingPage::OnSetActive() 
{
	BOOL bRet = UpdateDialogControls();	
	return CPropertyPage::OnSetActive();
}

void LoggingPage::OnButtonBrowseLogfilename() 
{
	// szFilters is a text string that includes two file name filters:
	// "*.my" for "MyType Files" and "*.*' for "All Files."
	char szFilters[] = _T( "Log Files (*.log)|*.log|Text Files (*.txt)|*.txt|All Files (*.*)|*.*||" );
	
	// Create an Open dialog; the default file name extension is ".my".
	DWORD dwFlags = OFN_HIDEREADONLY;
	CFileDialog fileDlg (TRUE, _T( "log" ), m_strFileName, dwFlags, szFilters, this);
	
	// Display the file dialog. When user clicks OK, fileDlg.DoModal() 
	// returns IDOK.
	if( fileDlg.DoModal () == IDOK )
	{
		CString pathName = fileDlg.GetPathName();
		m_ctrlFileName.SetWindowText( pathName );
	}

}
