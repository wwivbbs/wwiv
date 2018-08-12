/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "core/wwiv_windows.h"
#include <direct.h>
#include <io.h>
#endif  // WIN32

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
#include "bbs/remote_socket_io.h"
#include "bbs/sysoplog.h"
#include "bbs/remote_io.h"
#include "bbs/application.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/os.h"
#include "local_io/null_local_io.h"
#include "local_io/stdio_local_io.h"
#include "localui/curses_io.h"

#if defined( _WIN32 )
#include "local_io/local_io_win32.h"
#else
#include <unistd.h>
#endif // _WIN32

// Uncomment this line to use curses on Win32
#define WWIV_WIN32_CURSES_IO

static Application* app_;

using std::cout;
using std::endl;
using std::exception;
using std::string;
using namespace wwiv::os;
using namespace wwiv::strings;

Application* a() { return app_; }

// [ VisibleForTesting ]
Application* CreateSession(LocalIO* localIO) {
  app_ = new Application(localIO);
  return app_;
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
