/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2005 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#if !defined (__INCLUDED_WIOS_H__)
#define __INCLUDED_WIOS_H__


#include "../../circbuf.h"
#include "../../WComm.h"


class WIOSerial : public WComm
{

public:
    WIOSerial();
    virtual ~WIOSerial();

    virtual bool setup(char parity, int wordlen, int stopbits, unsigned long baud);
    virtual unsigned int open();
    virtual void close();
	virtual void StopThreads();
	virtual void StartThreads();
    virtual unsigned int putW(unsigned char ch);
    virtual unsigned char getW();
    virtual unsigned char dtr(bool raise);
    virtual void flushOut();
    virtual void purgeOut();
    virtual void purgeIn();
    virtual unsigned int put(unsigned char ch);
    virtual char peek();
    virtual unsigned int read(char *buffer, unsigned int count);
    virtual unsigned int write(char *buffer, unsigned int count);
    unsigned int writeImpl(char *buffer, unsigned int count);
    virtual bool carrier();
    virtual bool incoming();
    virtual bool startup();
    virtual bool shutdown();

protected:
    virtual bool SetBaudRate(unsigned long speed);
	static unsigned int __stdcall OutboundSerialProc(LPVOID hCommHandle);
	static unsigned int __stdcall InboundSerialProc(LPVOID hCommHandle);
	static bool HandleASuccessfulRead(LPCTSTR pszBuffer, DWORD dwNumRead);

protected:
    bool   bOpen;
    DCB    dcb;
	
	HANDLE hReadThread;
	HANDLE hWriteThread;
    HANDLE hComm;

    static cbuf_t inBuffer;
    static cbuf_t outBuffer;
    static HANDLE hInBufferMutex;
    static HANDLE hOutBufferMutex;
    static HANDLE hReadStopEvent;
	static HANDLE hWriteStopEvent;

};



#endif  // #if !defined (__INCLUDED_WIOS_H__)

