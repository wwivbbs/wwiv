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
#include "afxwin.h"


// SoundsPage dialog

class SoundsPage : public CPropertyPage
{
	DECLARE_DYNAMIC(SoundsPage)

public:
	SoundsPage();
	virtual ~SoundsPage();

// Dialog Data
	enum { IDD = IDD_DIALOG_SOUNDS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
    CEdit m_ctrlLogonSound;
    CEdit m_ctrlLogoffSound;
    afx_msg void OnBnClickedLogonSound();
    afx_msg void OnBnClickedLogoffSound();
private:
    void DoBrowseDialog(CEdit& control);
public:
    CString m_strLogonSound;
    CString m_strLogoffSound;
    afx_msg void OnBnClickedButtonPlayLogon();
    afx_msg void OnBnClickedButtonPlayLogoff();
    BOOL m_bSoundsEnabled;
};
