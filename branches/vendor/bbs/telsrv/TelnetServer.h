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
// TelnetServer.h: interface for the TelnetServer class.
//
//////////////////////////////////////////////////////////////////////
#pragma once

//#include "WWIVTelnetServerDoc.h"

class CWWIVTelnetServerDoc; 

class TelnetServer  
{
public:
	SOCKET m_Socket;
	HANDLE m_hExit;
	NodeStatus* m_nodeStatus;
	NodeStatus* GetNodeStatus();

	BOOL StopServer();
	BOOL StartServer( CWWIVTelnetServerDoc* pDoc );
	TelnetServer();
	virtual ~TelnetServer();

public:
    static unsigned __stdcall ListenThread( void *pVoid );
    static unsigned __stdcall ClientThread( void *pVoid );

private:
	unsigned long m_dwListeningThread;
    CWWIVTelnetServerDoc *m_pDoc;
    BOOL LogMessage( LPCTSTR pszMessage );
};

