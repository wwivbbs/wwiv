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
// NodeStatus.h: interface for the NodeStatus class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "../vardec.h"
#include "../instmsg.h"

class NodeStatus : public CObject 
{
	DECLARE_DYNAMIC( NodeStatus )

public:
	CTime& GetLastUpdateTime();
	CTime m_lastUpdateTime;
	int GetNextAvailableNode();
	BOOL SetNodeInfo( int nNodeNum, BOOL bBusy, LPCTSTR szIpAddress );
	LPCTSTR GetNodeIP( int nNodeNumber );
	LPCTSTR GetNodeActivity( int nNodeNumber );
	bool UpdateActivityList();
	BOOL IsActive( int nNodeNumber );
	NodeStatus();
	virtual ~NodeStatus();
	int GetHighNode();
	int GetLowNode();
	void SetHighNode( int nodeNum );
	void SetLowNode( int nodeNum );

private:
	BOOL m_bBusyArr[1025];
	long m_lAddrArr[1025];
	int m_nHighNode;
	int m_nLowNode;
	CRITICAL_SECTION m_CriticalSection;
	TCHAR m_szIpAddress[41];
	TCHAR m_szActivity[255];

// Debug and Diagnostics Support
#ifdef _DEBUG
public:
   virtual void Dump( CDumpContext& dc ) const;
   virtual void AssertValid() const;   // Override
#endif


private:
    // Instance Records from WWIV's INSTANCE.DAT
    instancerec *m_pInstanceRecord;
    configrec m_configRecord;
    // Number of Instances contained in INSTANCE.DAT
    long m_nNumBbsInstances;
    bool IsInstanceOnline( int nNodeStatus );
    bool m_bPrefsLoaded;
    CString m_BbsDir;
    CString m_DataDir;
    bool LoadConfiguration(void);
};

