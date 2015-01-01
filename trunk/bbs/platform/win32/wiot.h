/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015,WWIV Software Services             */
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
#ifndef __INCLUDED_WIOT_H__
#define __INCLUDED_WIOT_H__

#include "WComm.h"

#include <cstdint>
#include <queue>

#if defined( _WIN32 )
extern "C" {
#include <winsock2.h>
}
#endif // _WIN32

class WIOTelnet : public WComm {
 public:
  static const char CHAR_TELNET_OPTION_IAC = '\xFF';;
  static const uint8_t TELNET_OPTION_IAC = 255;
  static const uint8_t TELNET_OPTION_NOP = 241;
  static const uint8_t TELNET_OPTION_BRK = 243;

  static const uint8_t TELNET_OPTION_WILL = 251;
  static const uint8_t TELNET_OPTION_WONT = 252;
  static const uint8_t TELNET_OPTION_DO = 253;
  static const uint8_t TELNET_OPTION_DONT = 254;

  static const uint8_t TELNET_SB = 250;
  static const uint8_t TELNET_SE = 240;

  static const uint8_t TELNET_OPTION_BINARY = 0;
  static const uint8_t TELNET_OPTION_ECHO = 1;
  static const uint8_t TELNET_OPTION_RECONNECTION = 2;
  static const uint8_t TELNET_OPTION_SUPPRESSS_GA = 3;
  static const uint8_t TELNET_OPTION_TERMINAL_TYPE = 24;
  static const uint8_t TELNET_OPTION_WINDOW_SIZE = 31;
  static const uint8_t TELNET_OPTION_TERMINAL_SPEED = 32;
  static const uint8_t TELNET_OPTION_LINEMODE = 34;

 public:
  WIOTelnet(unsigned int nHandle);
  virtual bool open() override;
  virtual void close(bool bIsTemporary) override;
  virtual unsigned char getW() override;
  virtual bool dtr(bool raise) override;
  virtual void purgeIn() override;
  virtual unsigned int put(unsigned char ch) override;
  virtual unsigned int read(char *buffer, unsigned int count) override;
  virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false) override;
  virtual bool carrier() override;
  virtual bool incoming() override;
  void StopThreads();
  void StartThreads();
  virtual ~WIOTelnet();
  virtual unsigned int GetHandle() const;
  virtual unsigned int GetDoorHandle() const;

 public:
  static void InitializeWinsock();

 private:
  void HandleTelnetIAC(unsigned char nCmd, unsigned char nParam);
  void AddStringToInputBuffer(int nStart, int nEnd, char *pszBuffer);

 private:
  static void InboundTelnetProc(void *pTelnet);

 protected:
  std::queue<char> queue_;
  mutable HANDLE mu_;
  SOCKET socket_;
  SOCKET duplicate_socket_;
  HANDLE read_thread_;
  HANDLE stop_event_;
  bool threads_started_;
};

#endif  // __INCLUDED_WIOT_H__

