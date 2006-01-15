/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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

#if !defined (__INCLUDED_WIOU_H__)
#define __INCLUDED_WIOU_H__


#include "WComm.h"
#include <termios.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

class WIOUnix : public WComm
{
private:
	int tty_open;
	struct termios ttysav;
	FILE *ttyf;
    void set_terminal( bool initMode );

public:
    WIOUnix();
    virtual ~WIOUnix();
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
    virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation);
    virtual bool carrier();
    virtual bool incoming();
    virtual void StopThreads();
    virtual void StartThreads();
    virtual unsigned int GetHandle() const;
    virtual unsigned int GetDoorHandle() const;


};



#endif  // #if !defined (__INCLUDED_WIOU_H__)

