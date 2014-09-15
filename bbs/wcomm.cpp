/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "bbs/wcomm.h"
#include "core/wwivport.h"

#if defined ( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "platform/win32/wiot.h"
#elif defined ( __unix__ )
#include "bbs/platform/unix/wiou.h"
#endif

#include "core/scope_exit.h"
#include "core/wwivport.h"

// static
std::string WComm::error_text_;

int WComm::GetComPort() const { return comport_; }
void WComm::SetComPort(int nNewPort) { comport_ = nNewPort; }

const std::string WComm::GetLastErrorText() {
#if defined ( _WIN32 )
  char* error_text;
  wwiv::core::ScopeExit on_exit([&error_text] {LocalFree(error_text);});
  
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    (LPTSTR) &error_text,
    0,
    NULL
  );
  error_text_.assign(error_text);
#endif
  return error_text_;
}

WComm* WComm::CreateComm(unsigned int nHandle) {
#if defined ( _WIN32 )
  return new WIOTelnet(nHandle);
#elif defined ( __unix__ )
  return new WIOUnix();
#endif
}
