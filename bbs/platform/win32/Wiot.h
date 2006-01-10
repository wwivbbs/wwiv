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

#if !defined (__INCLUDED_WIOT_H__)
#define __INCLUDED_WIOT_H__


#include "WComm.h"
#include <queue>


const int MAX_WWIV_INPUT_BUFFER_SIZE = 4096;

class WIOTelnet : public WComm
{
public:
    static const char CHAR_TELNET_OPTION_IAC;
    static const int  TELNET_OPTION_IAC;
    static const int  TELNET_OPTION_NOP;
    static const int  TELNET_OPTION_BRK;

    static const int  TELNET_OPTION_WILL;
    static const int  TELNET_OPTION_WONT;
    static const int  TELNET_OPTION_DO;
    static const int  TELNET_OPTION_DONT;

    static const int TELNET_SB;
    static const int TELNET_SE;

    static const int TELNET_OPTION_BINARY;
    static const int TELNET_OPTION_ECHO;
    static const int TELNET_OPTION_RECONNECTION;
    static const int TELNET_OPTION_SUPPRESSS_GA;
    static const int TELNET_OPTION_TERMINAL_TYPE;
    static const int TELNET_OPTION_WINDOW_SIZE;
    static const int TELNET_OPTION_TERMINAL_SPEED;
    static const int TELNET_OPTION_LINEMODE;

public:
    WIOTelnet();
    virtual bool setup(char parity, int wordlen, int stopbits, unsigned long baud);
    virtual unsigned int open();
    virtual void close( bool bIsTemporary );
    virtual unsigned int putW(unsigned char ch);
    virtual unsigned char getW();
    virtual bool dtr(bool raise);
    virtual void flushOut();
    virtual void purgeOut();
    virtual void purgeIn();
    virtual unsigned int put(unsigned char ch);
    virtual char peek();
    virtual unsigned int read(char *buffer, unsigned int count);
    virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false);
    virtual bool carrier();
    virtual bool incoming();
    virtual bool startup();
    virtual bool shutdown();
    virtual void StopThreads();
    virtual void StartThreads();
    virtual ~WIOTelnet();

private:
    void HandleTelnetIAC( unsigned char nCmd, unsigned char nParam );
    void AddStringToInputBuffer( int nStart, int nEnd, char *pszBuffer );
    void AddCharToInputBuffer( char ch );

private:
    static void InboundTelnetProc(void *pTelnet);

protected:
	std::queue<char> inBuffer;
    HANDLE hInBufferMutex;
    SOCKET hSock;
	HANDLE hReadThread;
    HANDLE hReadStopEvent;
    bool bThreadsStarted;

};



#endif  // #if !defined (__INCLUDED_WIOT_H__)

