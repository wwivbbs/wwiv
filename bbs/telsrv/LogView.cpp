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
// LogView.cpp : implementation file
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"
#include "LogView.h"


/////////////////////////////////////////////////////////////////////////////
// CLogView

IMPLEMENT_DYNCREATE(CLogView, CEditView)

CLogView::CLogView()
{
}

CLogView::~CLogView()
{
}


BEGIN_MESSAGE_MAP(CLogView, CEditView)
	//{{AFX_MSG_MAP(CLogView)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLogView drawing

void CLogView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}

/////////////////////////////////////////////////////////////////////////////
// CLogView diagnostics

#ifdef _DEBUG
void CLogView::AssertValid() const
{
	CEditView::AssertValid();
}

void CLogView::Dump(CDumpContext& dc) const
{
	CEditView::Dump(dc);
}

CWWIVTelnetServerDoc* CLogView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CWWIVTelnetServerDoc)));
	return DYNAMIC_DOWNCAST(CWWIVTelnetServerDoc, m_pDocument );
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CLogView message handlers

void CLogView::OnInitialUpdate() 
{
	CEditView::OnInitialUpdate();
	CEdit& ctrl = this->GetEditCtrl();
	ctrl.SetReadOnly( TRUE );
	CDocument* doc = this->GetDocument();
	CRuntimeClass *clazz = doc->GetRuntimeClass();
	OutputDebugString( clazz->m_lpszClassName );
	
}

void CLogView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	CString& strLogText = GetDocument()->GetLogText();
    CString strWindowText;
    CEdit &editControl = GetEditCtrl();
    editControl.GetWindowText( strWindowText );

    if ( strLogText.Compare( strWindowText ) != 0 )
    {
    	editControl.SetWindowText( strLogText );
        editControl.LineScroll( editControl.GetLineCount(), 0 );
    }
}
