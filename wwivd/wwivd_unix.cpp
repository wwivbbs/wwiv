/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
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

#include <iostream>
#include <map>
#include <string>

#include <signal.h>
#include <string>
#include <sys/types.h>

#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/vardec.h"
#include "sdk/filenames.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::os;

// From wwivd.cpp

extern pid_t bbs_pid;

static void huphandler(int mysignal) {
  cerr << endl;
  cerr << "Sending SIGHUP to BBS after receiving " << mysignal << "..." << endl;
  kill(bbs_pid, SIGHUP); // send SIGHUP to process group
}

static void sigint_handler(int mysignal) {
  cerr << endl;
  cerr << "Sending SIGINT to BBS after receiving " << mysignal << "..." << endl;
  kill(bbs_pid, SIGINT); // send SIGINT to process group
}

static void setup_sighup_handlers() {
  struct sigaction sa;
  sa.sa_handler = huphandler;
  sa.sa_flags = SA_RESETHAND;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGHUP";
  }
}

static void setup_sigint_handlers() {
  struct sigaction sa;
  sa.sa_handler = sigint_handler;
  sa.sa_flags = SA_RESETHAND;
  sigfillset(&sa.sa_mask);
  if (sigaction(SIGHUP, &sa, nullptr) == -1) {
    LOG(ERROR) << "Unable to install signal handler for SIGINT";
  }
}

void SetupSignalHandlers() {
  setup_sighup_handlers();
  setup_sigint_handlers();
}

