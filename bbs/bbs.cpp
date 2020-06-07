/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/bbs.h"
#include "bbs/sysoplog.h"
#include "bbs/application.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/os.h"
#include "local_io/null_local_io.h"
#include "local_io/stdio_local_io.h"
#include "localui/curses_io.h"
#include "sdk/config.h"

#include <exception>
#include <iostream>
#include <string>
#if !defined( _WIN32 )
#include <unistd.h>
#endif // !_WIN32

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
    using wwiv::core::Logger;
    using wwiv::core::LoggerConfig;
    using wwiv::sdk::LogDirFromConfig;

    // Initialize the Logger.
    LoggerConfig config(LogDirFromConfig);
    Logger::Init(argc, argv, config);

    // Create a default session using stdio, we'll reset the LocalIO
    // later once we know what type to use.
    auto* bbs = CreateSession(new StdioLocalIO());
    const auto return_code = bbs->Run(argc, argv);
    bbs->ExitBBSImpl(return_code, false);
    return return_code;
  } catch (const std::exception& e) {
    LOG(FATAL) << "BBS Terminated by exception: " << e.what();
    return 1;
  }
}
