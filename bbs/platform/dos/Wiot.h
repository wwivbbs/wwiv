/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#if !defined (__INCLUDED_WIOT_H__)
#define __INCLUDED_WIOT_H__


#include "../../circbuf.h"
#include "../../WComm.h"

const int MAX_WWIV_INPUT_BUFFER_SIZE = 4096;

void InboundTelnetProc(void *hSocketHandle);

class WIOTelnet : public WComm
{
public:

    static cbuf_t inBuffer;
    static HANDLE hInBufferMutex;
    SOCKET hSock;
    virtual bool setup(char parity, int wordlen, int stopbits, unsigned long baud);
    virtual unsigned int open();
    virtual void close();
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
    virtual bool carrier();
    virtual bool incoming();
    virtual bool startup();
    virtual bool shutdown();
	virtual void StopThreads();
	virtual void StartThreads();
};



#endif  // #if !defined (__INCLUDED_WIOT_H__)

