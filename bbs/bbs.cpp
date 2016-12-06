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
#include <io.h>
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
#include "bbs/stdio_local_io.h"
#include "bbs/sysoplog.h"
#include "bbs/remote_io.h"
#include "bbs/wsession.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/os.h"
#include "localui/curses_io.h"

#if defined( _WIN32 )
#include "bbs/local_io_win32.h"
#else
#include <unistd.h>
#endif // _WIN32

// Uncomment this line to use curses on Win32
#define WWIV_WIN32_CURSES_IO

static WSession* sess_;

using std::cout;
using std::endl;
using std::exception;
using std::string;
using namespace wwiv::os;
using namespace wwiv::strings;

WSession* session() { return sess_; }

// [ VisibleForTesting ]
WSession* CreateSession(LocalIO* localIO) {
  sess_ = new WSession(localIO);
  return sess_;
}

int bbsmain(int argc, char *argv[]) {
  try {
    // Initialize the Logger.
    wwiv::core::Logger::Init(argc, argv);

    // CursesIO
    out = nullptr;

    // Create a default session using stdio, we'll reset the LocalIO
    // later once we know what type to use.
    auto bbs = CreateSession(new StdioLocalIO());
    int return_code = bbs->Run(argc, argv);
    bbs->ExitBBSImpl(return_code, false);
    return return_code;
  } catch (const exception& e) {
    LOG(FATAL) << "BBS Terminated by exception: " << e.what();
    return 1;
  }
}
