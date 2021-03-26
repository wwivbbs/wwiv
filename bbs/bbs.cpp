/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/application.h"
#include "common/exceptions.h"
#include "core/cp437.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "local_io/stdio_local_io.h"
#include "sdk/config.h"
#include <exception>
#include <iostream>

#if !defined( _WIN32 )
#include <unistd.h>
#endif // !_WIN32

// Uncomment this line to use curses on Win32
#define WWIV_WIN32_CURSES_IO

static Application* app_;

using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;

Application* a() { return app_; }

// [ VisibleForTesting ]
Application* CreateSession(LocalIO* localIO) {
  app_ = new Application(localIO);
  return app_;
}
using wwiv::core::Logger;
using wwiv::core::LoggerConfig;
using wwiv::sdk::LogDirFromConfig;

int bbsmain(int argc, char *argv[]) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  std::unique_ptr<Application> bbs;
  try {
#ifdef WWIV_PAUSE_AT_START
    std::cout << "pause";
    getchar();
#endif
    set_wwiv_codepage(wwiv::core::wwiv_codepage_t::utf8);

    // Create a default session using stdio, we'll reset the LocalIO
    // later once we know what type to use.
    bbs.reset(CreateSession(new StdioLocalIO()));
    const auto return_code = bbs->Run(argc, argv); 
    return bbs->ExitBBSImpl(return_code, true);
  } catch (const wwiv::common::hangup_error& e) {
    if (e.hangup_type() == wwiv::common::hangup_type_t::user_disconnected) {
      LOG(ERROR) << "BBS User Hung Up: " << e.what();
    }
    if (bbs) {
      return bbs->ExitBBSImpl(Application::exitLevelOK, true);
    }
    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "BBS Terminated by exception: " << e.what();
    if (bbs) {
      return bbs->ExitBBSImpl(Application::exitLevelNotOK, true);
    }
  }
  return 1;
}
