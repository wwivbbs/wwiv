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
// NewsPage.cpp : implementation file
//

#include "stdafx.h"
#include "wwivtelnetserver.h"
#include "NewsPage.h"


// NewsPage dialog

IMPLEMENT_DYNAMIC(NewsPage, CPropertyPage)
NewsPage::NewsPage()
	: CPropertyPage(NewsPage::IDD)
	, m_bEnableNNTPServer(FALSE)
{ 
}

NewsPage::~NewsPage()
{
}

void NewsPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_PREF_NNTP_ENABLENNTP, m_bEnableNNTPServer);
}


BEGIN_MESSAGE_MAP(NewsPage, CPropertyPage)
	ON_BN_CLICKED(IDC_PREF_NNTP_ENABLENNTP, &NewsPage::OnBnClickedPrefNntpEnablenntp)
END_MESSAGE_MAP()


// NewsPage message handlers


void NewsPage::OnBnClickedPrefNntpEnablenntp()
{
	// TODO: Add your control notification handler code here
}
