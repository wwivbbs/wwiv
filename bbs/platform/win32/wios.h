/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#if !defined (__INCLUDED_WIOS_H__)
#define __INCLUDED_WIOS_H__

#include "WComm.h"
#include <queue>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

class WIOSerial : public WComm {

 public:
  WIOSerial(unsigned int nHandle);
  virtual ~WIOSerial();

  virtual bool setup(char parity, int wordlen, int stopbits, unsigned long baud);
  virtual unsigned int open();
  virtual void close(bool bIsTemporary);
  virtual void StopThreads();
  virtual void StartThreads();
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
  unsigned int writeImpl(const char *buffer, unsigned int count);
  virtual bool carrier();
  virtual bool incoming();
  virtual unsigned int GetHandle() const;

 protected:
  virtual bool SetBaudRate(unsigned long speed);
  DWORD GetDCBCodeForSpeed(unsigned long speed);
  static unsigned int __stdcall InboundSerialProc(LPVOID hCommHandle);
  static bool HandleASuccessfulRead(LPCTSTR pszBuffer, DWORD dwNumRead, WIOSerial* pSerial);

 protected:
  bool   bOpen;
  DCB    dcb;
  COMMTIMEOUTS oldtimeouts;

  HANDLE m_hReadThread;
  HANDLE hWriteThread;
  HANDLE hComm;
  HANDLE m_hReadStopEvent;
  std::queue<char> m_inputQueue;
  HANDLE m_hInBufferMutex;
};



#endif  // #if !defined (__INCLUDED_WIOS_H__)

