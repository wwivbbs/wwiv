/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2016 WWIV Software Services              */
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
/**************************************************************************/

#include "core/log.h"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>

#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"

using std::clog;
using std::endl;
using std::map;
using std::ofstream;
using std::string;
using el::ConfigurationType;
using el::Level;
using el::Loggers;
using namespace wwiv::strings;

INITIALIZE_EASYLOGGINGPP

namespace wwiv {
namespace core {


Logger::Logger() {}

Logger::~Logger() {}

static std::string exit_filename;

//static
void Logger::ExitLogger() {
  time_t t = time(nullptr);
  LOG(INFO) << exit_filename << " exiting at " << asctime(localtime(&t));
}


// static
void Logger::Init(int argc, char** argv) {
  string filename(argv[0]);
  if (ends_with(filename, ".exe") || ends_with(filename, ".EXE")) {
    filename = filename.substr(0, filename.size() - 4);
  }
  std::size_t last_slash = filename.rfind(File::pathSeparatorChar);
  if (last_slash != string::npos) {
    filename = filename.substr(last_slash + 1);
  }
  string log_filename = StrCat(filename, ".log");

  el::Configurations conf;
  conf.setToDefault();
  conf.setGlobally(ConfigurationType::Filename, log_filename);
  conf.setGlobally(ConfigurationType::Format, std::string("%datetime %level  %msg"));
  conf.set(Level::Debug, ConfigurationType::Format, std::string("%datetime %level [%user@%host] [%func] [%loc] %msg"));
  // INFO and WARNING are set to default by Level::Global
  conf.set(Level::Error, ConfigurationType::Format, std::string("%datetime %level %msg"));
  conf.set(Level::Fatal, ConfigurationType::Format, std::string("%datetime %level %msg"));
  conf.set(Level::Verbose, ConfigurationType::Format, std::string("%datetime %level-%vlevel %msg"));
  conf.set(Level::Trace, ConfigurationType::Format, std::string("%datetime %level [%func] [%loc] %msg"));

  el::Loggers::reconfigureAllLoggers(conf);
  START_EASYLOGGINGPP(argc, argv);

  time_t t = time(nullptr);
  string l(asctime(localtime(&t)));
  StringTrim(&l);
  LOG(INFO) << filename << " version " << wwiv_version << beta_version
            << " (" << wwiv_date << ")";
  LOG(INFO) << filename << " starting at " << l;
  if (argc > 1) {
    string cmdline;
    for (int i = 1; i < argc; i++) {
      cmdline += argv[i];
      cmdline += " ";
    }
    LOG(INFO) << "command line: " << cmdline;
  }

  exit_filename = filename;
}

el::base::type::StoragePointer Logger::getLoggerStorage() {
        return el::Helpers::storage();
}

}
}
