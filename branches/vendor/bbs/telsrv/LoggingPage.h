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
#pragma once
// LoggingPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// LoggingPage dialog

class LoggingPage : public CPropertyPage
{
	DECLARE_DYNCREATE(LoggingPage)

// Construction
public:
	LoggingPage();
	~LoggingPage();

// Dialog Data
	//{{AFX_DATA(LoggingPage)
	enum { IDD = IDD_DIALOG_LOGGING };
	CButton	m_btnBrowse;
	CEdit	m_ctrlFileName;
	CButton	m_radioFile;
	CString	m_strFileName;
	int		m_radioButtons;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(LoggingPage)
	public:
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(LoggingPage)
	afx_msg void OnRadioEvent();
	afx_msg void OnRadioFile();
	afx_msg void OnButtonBrowseLogfilename();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	BOOL UpdateDialogControls();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

