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
// NodeDetailsView.cpp : implementation of the CNodeDetailsView class
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"

#include "WWIVTelnetServerDoc.h"
#include "NodeDetailsView.h"

/////////////////////////////////////////////////////////////////////////////
// CNodeDetailsView

IMPLEMENT_DYNCREATE(CNodeDetailsView, CListView)

BEGIN_MESSAGE_MAP(CNodeDetailsView, CListView)
	//{{AFX_MSG_MAP(CNodeDetailsView)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
    ON_WM_TIMER()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNodeDetailsView construction/destruction

CNodeDetailsView::CNodeDetailsView()
{
   
}

CNodeDetailsView::~CNodeDetailsView()
{
}

BOOL CNodeDetailsView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CListView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CNodeDetailsView drawing

void CNodeDetailsView::OnDraw(CDC* pDC)
{
	CWWIVTelnetServerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// TODO: add draw code for native data here
}

void CNodeDetailsView::OnInitialUpdate()
{
	CListView::OnInitialUpdate();
	CListCtrl &nodeList = GetListCtrl();

	m_imageList.Create( IDB_FOLDERS, 16, 4, RGB( 255, 255, 255 ) );
	nodeList.SetImageList( &m_imageList, LVSIL_SMALL );

	m_imageListBig.Create( IDB_NODES, 32, 2, RGB( 255, 255, 255 ) );
	nodeList.SetImageList( &m_imageListBig, TVSIL_NORMAL );

	RECT rect;
	nodeList.GetWindowRect(&rect);
	nodeList.InsertColumn(0, _T( "Node Status" ), LVCFMT_LEFT, rect.right - rect.left - 5 );

	ModifyStyle( LVS_TYPEMASK, LVS_REPORT );
    
}

/////////////////////////////////////////////////////////////////////////////
// CNodeDetailsView diagnostics

#ifdef _DEBUG
void CNodeDetailsView::AssertValid() const
{
	CListView::AssertValid();
}

void CNodeDetailsView::Dump(CDumpContext& dc) const
{
	CListView::Dump(dc);
}

CWWIVTelnetServerDoc* CNodeDetailsView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CWWIVTelnetServerDoc)));
	return (CWWIVTelnetServerDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CNodeDetailsView message handlers


void CNodeDetailsView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	CListCtrl &nodeList = GetListCtrl();
	NodeStatus* nodeStatus = GetDocument()->GetNodeStatus();
    nodeStatus->UpdateActivityList();
	
	CString text = _T( "" );

	POSITION pos = nodeList.GetFirstSelectedItemPosition();
	if ( pos )
	{
		int ndx = nodeList.GetNextSelectedItem( pos );
		if ( ndx > -1 )
		{
			text = nodeList.GetItemText( ndx, 0 );
		}
	}

	BOOL bRet = nodeList.DeleteAllItems();
	if ( !bRet )
	{
		::AfxMessageBox( _T( "[CNodeDetailsView::OnUpdate] DeleteAllItems failed" ) );
	}
	ASSERT(nodeList.GetItemCount() == 0);

	if ( GetDocument()->IsListenerStarted() )
	{
		int low = nodeStatus->GetLowNode();
		int high = nodeStatus->GetHighNode();
		for ( int i=low; i <= high; i++ )
		{
			BOOL isActive = nodeStatus->IsActive( i );
			CString s;
			if ( isActive )
			{
                CString act = nodeStatus->GetNodeActivity( i );
    			CString ip  = nodeStatus->GetNodeIP( i );
                s.Format( _T( "Node #%d - %s %s" ), i, ip, act );
			}
			else
			{
				s.Format( _T( "Node #%d - Inactive" ), i );
			}
			int ret = nodeList.InsertItem(i-low, s, ( isActive ) ? 3 : 2 );
			if ( s == text )
			{
				nodeList.SetSelectionMark( ret );
			}
		}
	}	
}

