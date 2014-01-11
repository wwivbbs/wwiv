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
// WWIVTelnetServerDoc.h : interface of the CWWIVTelnetServerDoc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_WWIVTELNETSERVERDOC_H__A3FD97FE_3501_48C3_9767_7F06DEA367AF__INCLUDED_)
#define AFX_WWIVTELNETSERVERDOC_H__A3FD97FE_3501_48C3_9767_7F06DEA367AF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CMainFrame;

class CWWIVTelnetServerDoc : public CDocument
{
protected: // create from serialization only
	CWWIVTelnetServerDoc();
	DECLARE_DYNCREATE(CWWIVTelnetServerDoc)

// Attributes
public:
	BOOL m_bListenerStarted;

private:
	TelnetServer m_telnetserver;

// Operations
public:
	NodeStatus* GetNodeStatus();
	BOOL StopListener();
	BOOL StartListener();
	void SetStatusBarText( LPCTSTR szText );


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWWIVTelnetServerDoc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	virtual void DeleteContents();
	virtual void OnCloseDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	//}}AFX_VIRTUAL

// Implementation
public:
	void AppendLogText( LPCTSTR pszNewText );
	void ClearLogText();
	CString& GetLogText();
	BOOL UpdateNodeStatus( int nNodeNumber = 0, int nExitCode = 0 );
	BOOL IsListenerStarted();
	virtual ~CWWIVTelnetServerDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CWWIVTelnetServerDoc)
	afx_msg void OnTelsrvStart();
	afx_msg void OnUpdateTelsrvStart(CCmdUI* pCmdUI);
	afx_msg void OnTelsrvStop();
	afx_msg void OnUpdateTelsrvStop(CCmdUI* pCmdUI);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// Generated OLE dispatch map functions
	//{{AFX_DISPATCH(CWWIVTelnetServerDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DISPATCH
	DECLARE_DISPATCH_MAP()
	DECLARE_INTERFACE_MAP()
private:
	CString m_strLogText;
	CTime m_lastUpdateTime;
	BOOL m_bIsOpen;

	CString m_wsaSystemStatus;
	CString m_wsaDescription;


    CMainFrame* GetMainFrame();
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WWIVTELNETSERVERDOC_H__A3FD97FE_3501_48C3_9767_7F06DEA367AF__INCLUDED_)
