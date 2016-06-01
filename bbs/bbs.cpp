/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifdef _WIN32
// include this here so it won't get includes by local_io_win32.h
#include "bbs/wwiv_windows.h"
#include <direct.h>
#endif  // WIN32

#define _DEFINE_GLOBALS_
// vars.h requires  _DEFINE_GLOBALS_
#include "bbs/vars.h"
#undef _DEFINE_GLOBALS_

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <exception>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/local_io_curses.h"
#include "bbs/null_local_io.h"
#include "bbs/remote_socket_io.h"
#include "bbs/sysoplog.h"
#include "bbs/remote_io.h"
#include "bbs/wsession.h"
#include "core/strings.h"
#include "core/os.h"
#include "localui/curses_io.h"

#if defined( _WIN32 )
#include "bbs/local_io_win32.h"
#else
#include <unistd.h>
#endif // _WIN32

static WApplication *app_;
static WSession* sess;

using std::clog;
using std::cout;
using std::endl;
using std::exception;
using std::string;
using namespace wwiv::os;
using namespace wwiv::strings;

WApplication* application() { return app_; }
WSession* session() { return sess; }


int WApplication::BBSMainLoop(int argc, char *argv[]) {
#if defined ( _WIN32 )
  sess = CreateSession(app_, new Win32ConsoleIO());
#else
  //  sess = CreateSession(app_, new NullLocalIO());
  const string title = StringPrintf("WWIV BBS %s%s", wwiv_version, beta_version);
  CursesIO::Init(title);
  sess = CreateSession(app_, new CursesLocalIO());
#endif

  // We are not running in the telnet server, so proceed as planned.
  int nReturnCode = session()->Run(argc, argv);
  session()->ExitBBSImpl(nReturnCode);
  return nReturnCode;
}

WApplication::WApplication() {
  // TODO this should move into the WSystemConfig object (syscfg wrapper) once it is established.
  if (syscfg.userreclen == 0) {
    syscfg.userreclen = sizeof(userrec);
  }
  tzset();
}

bool WApplication::LogMessage(const char* format, ...) {
  va_list ap;
  char szBuffer[2048];

  va_start(ap, format);
  vsnprintf(szBuffer, sizeof(szBuffer), format, ap);
  va_end(ap);
  sysoplog(szBuffer);
  return true;
}


WApplication::~WApplication() {
  if (sess != nullptr) {
    delete sess;
    sess = nullptr;
  }
}

WSession* CreateSession(WApplication* app, LocalIO* localIO) {
  sess = new WSession(app, localIO);
  return sess;
}

int bbsmain(int argc, char *argv[]) {
  try {
    app_ = new WApplication();
    File::SetLogger(app_);
    return app_->BBSMainLoop(argc, argv);
  } catch (exception& e) {
    // TODO(rushfan): Log this to sysop log or where else?
    clog << "BBS Terminated by exception: " << e.what();
    return 1;
  }
}
