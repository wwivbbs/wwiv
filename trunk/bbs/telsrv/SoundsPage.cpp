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
// SoundsPage.cpp : implementation file
//

#include "stdafx.h"
#include "WWIVTelnetServer.h"
#include "SoundsPage.h"
#include "mmsystem.h"			


// SoundsPage dialog

IMPLEMENT_DYNAMIC(SoundsPage, CPropertyPage)
SoundsPage::SoundsPage()
	: CPropertyPage(SoundsPage::IDD)
    , m_strLogonSound(_T(""))
    , m_strLogoffSound(_T(""))
    , m_bSoundsEnabled(FALSE)
{
}

SoundsPage::~SoundsPage()
{
}

void SoundsPage::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PREF_SND_LOGON, m_ctrlLogonSound);
    DDX_Control(pDX, IDC_PREF_SND_LOGOFF, m_ctrlLogoffSound);
    DDX_Text(pDX, IDC_PREF_SND_LOGON, m_strLogonSound);
    DDX_Text(pDX, IDC_PREF_SND_LOGOFF, m_strLogoffSound);
    DDX_Check(pDX, IDC_PREF_SND_USESOUNDS, m_bSoundsEnabled);
}


BEGIN_MESSAGE_MAP(SoundsPage, CPropertyPage)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE1, OnBnClickedLogonSound)
    ON_BN_CLICKED(IDC_BUTTON_BROWSE2, OnBnClickedLogoffSound)
    ON_BN_CLICKED(IDC_BUTTON_PLAY_LOGON, OnBnClickedButtonPlayLogon)
    ON_BN_CLICKED(IDC_BUTTON_PLAY_LOGOFF, OnBnClickedButtonPlayLogoff)
END_MESSAGE_MAP()


// SoundsPage message handlers

void SoundsPage::OnBnClickedLogonSound()
{
    DoBrowseDialog( m_ctrlLogonSound );
}

void SoundsPage::OnBnClickedLogoffSound()
{
    DoBrowseDialog( m_ctrlLogoffSound );
}

void SoundsPage::DoBrowseDialog(CEdit& control)
{
    CString oldFileName;
    control.GetWindowText( oldFileName );

    // "*.my" for "MyType Files" and "*.*' for "All Files."
    wchar_t szFilters[] = _T( "WAV Files (*.wav)|*.wav|MIDI Files (*.mid)|*.mid|All Files (*.*)|*.*||" );

    // Create an Open dialog; the default file name extension is ".my".
    DWORD dwFlags = OFN_HIDEREADONLY;
    CFileDialog fileDlg (TRUE, _T( "wav" ), oldFileName, dwFlags, szFilters, this);

    // Display the file dialog. When user clicks OK, fileDlg.DoModal() 
    // returns IDOK.
    if( fileDlg.DoModal () == IDOK )
    {
        CString pathName = fileDlg.GetPathName();
        control.SetWindowText( pathName );
    }
}

void SoundsPage::OnBnClickedButtonPlayLogon()
{
    CString soundName;
    m_ctrlLogonSound.GetWindowText( soundName );
    PlaySound( soundName, NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NOWAIT );
}

void SoundsPage::OnBnClickedButtonPlayLogoff()
{
    CString soundName;
    m_ctrlLogoffSound.GetWindowText( soundName );
    PlaySound( soundName, NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP | SND_NOWAIT );
}
