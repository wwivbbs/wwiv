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
// MainFrame.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "trayicon.h"


#define WM_NODESTATUSCHANGED (WM_USER+1)
#define WM_MY_TRAY_NOTIFICATION WM_USER+2
#define WM_LOG_MESSAGE (WM_USER+3)

class CNodeDetailsView;

class CMainFrame : public CFrameWnd
{
	
protected: // create from serialization only
	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

// Attributes
protected:
	CSplitterWnd	m_wndSplitter;
	CSplitterWnd	m_wndSplitter2;
	CTrayIcon		m_trayIcon;		// my tray icon
	bool			m_bIsHidden;	// display info in main window

public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
	CNodeDetailsView* GetNodeView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members
	CStatusBar  m_wndStatusBar;
	CToolBar    m_wndToolBar;
	CReBar      m_wndReBar;
	CDialogBar      m_wndDlgBar;

// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnWtPrefs();
	afx_msg void OnUpdateWtPrefs(CCmdUI* pCmdUI);
	afx_msg LRESULT OnTrayNotification(WPARAM wp, LPARAM lp);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnShowMainwindow();
	//}}AFX_MSG
	afx_msg void OnUpdateViewStyles(CCmdUI* pCmdUI);
	afx_msg void OnViewStyle(UINT nCommandID);
	afx_msg LRESULT OnNodeStatusChanged(WPARAM w, LPARAM l);
    afx_msg LRESULT OnLogMessage(WPARAM w, LPARAM l);
	DECLARE_MESSAGE_MAP()

protected:
	//CWWIVTelnetServerApp* GetWWIVTelnetServerApp();

private:
	BOOL UpdateNodeStatus( int nNodeNumber = 0, int nExitCode = 0 );
    
public:
    afx_msg void OnTimer(UINT nIDEvent);
private:
    UINT m_nTimer;
public:
    bool StartAppTimer();
    bool StopAppTimer();
    afx_msg void OnDestroy();
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnLocalNode();
    afx_msg void OnUpdateLocalNode(CCmdUI *pCmdUI);
    afx_msg void OnAppUpdates();
    // Executes the beginday event for WWIV
    bool RunBeginDayEvent(void);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

