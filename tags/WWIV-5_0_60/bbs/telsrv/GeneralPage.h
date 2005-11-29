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
// GeneralPage.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GeneralPage dialog

class GeneralPage : public CPropertyPage
{
	DECLARE_DYNCREATE(GeneralPage)

// Construction
public:
	GeneralPage();
	~GeneralPage();

// Dialog Data
	//{{AFX_DATA(GeneralPage)
	enum { IDD = IDD_DIALOG_PREFERENCES };
	CEdit	m_workDirCtrl;
	CEdit	m_cmdLineCtrl;
	CEdit	m_parametersCtrl;
	BOOL	m_bAutoStart;
	CString	m_cmdLine;
	CString	m_workDir;
	int		m_nEndNode;
	int		m_nPortNum;
	int		m_nStartNode;
	CString	m_parameters;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(GeneralPage)
	public:
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(GeneralPage)
	afx_msg void OnButtonBrowse1();
	afx_msg void OnButtonBrowse2();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void GetDirFromPath( LPCTSTR szPath, CString &dir );
public:
    int m_nLocalNode;
    BOOL m_bLaunchMinimized;
    BOOL m_BeginDayEvent;
	afx_msg void OnBnClickedPrefAutostart();
    afx_msg void OnBnClickedPrefUseballoons();
    BOOL m_bUseBalloons;
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

