/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014,WWIV Software Services             */
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

#include <termios.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

#include "bbs/wcomm.h"

class WIOUnix : public WComm {
 private:
  int tty_open;
  struct termios ttysav;
  FILE *ttyf;
  void set_terminal(bool initMode);

 public:
  WIOUnix();
  virtual ~WIOUnix();
  virtual unsigned int open() override;
  virtual void close(bool bIsTemporary) override;
  virtual unsigned char getW() override;
  virtual bool dtr(bool raise) override;
  virtual void purgeIn() override;
  virtual unsigned int put(unsigned char ch) override;
  virtual unsigned int read(char *buffer, unsigned int count) override;
  virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation) override;
  virtual bool carrier() override;
  virtual bool incoming() override;
  virtual unsigned int GetHandle() const override;
  virtual unsigned int GetDoorHandle() const override;
};

#endif  // #if !defined (__INCLUDED_WIOU_H__)

