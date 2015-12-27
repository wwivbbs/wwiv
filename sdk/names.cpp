/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2015 WWIV Software Services              */
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
#include "sdk/names.h"

#include <exception>
#include <stdexcept>
#include <string>

#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::clog;
using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

Names::Names(wwiv::sdk::Config& config) : data_directory_(config.datadir()) {
  DataFile<smalrec> file(data_directory_, NAMES_LST);
  if (file) {
    file.ReadVector(names_);
    loaded_ = true;
  }
}

static smalrec smalrec_for(uint32_t user_number, const std::vector<smalrec>& names) {
  for (const auto& n : names) {
    if (n.number == user_number) {
      return n;
    }
  }
  return smalrec{"", 0};
}

std::string Names::UserName(uint32_t user_number) const {
  smalrec sr = smalrec_for(user_number, names_);
  if (sr.number == 0) {
    return "";
  }
  string name = properize(string(reinterpret_cast<char*>(sr.name)));
  return StringPrintf("%s #%u", name.c_str(), user_number);
}

std::string Names::UserName(uint32_t user_number, uint32_t system_number) const {
  const string base = UserName(user_number);
  if (base.empty()) {
    return "";
  }
  return StringPrintf("%s @%u", base.c_str(), system_number);
}

bool Names::Add(const std::string name, uint32_t user_number) {
  string upper_case_name(name);
  StringUpperCase(&upper_case_name);
  auto it = names_.begin();
  for (; it != names_.end()
    && StringCompare(upper_case_name.c_str(), reinterpret_cast<char*>((*it).name)) > 0;
    it++) {
  }
  smalrec sr;
  strcpy(reinterpret_cast<char*>(sr.name), upper_case_name.c_str());
  sr.number = static_cast<unsigned short>(user_number);
  names_.insert(it, sr);
  return true;
}

bool Names::Remove(uint32_t user_number) {
  const string name(reinterpret_cast<char*>(smalrec_for(user_number, names_).name));
  if (name.empty()) {
    return false;
  }

  string upper_case_name(name);
  StringUpperCase(&upper_case_name);
  auto it = names_.begin();
  for (; it != names_.end()
    && StringCompare(upper_case_name.c_str(), reinterpret_cast<char*>((*it).name)) > 0;
    it++) {
  }
  if (!wwiv::strings::IsEquals(upper_case_name.c_str(), reinterpret_cast<char*>((*it).name))) {
    return false;
  }
  names_.erase(it);
  return true;
}

Names::~Names() {
  if (!save_on_exit_) {
    return;
  }
  DataFile<smalrec> file(data_directory_, NAMES_LST,
    File::modeReadWrite | File::modeBinary | File::modeTruncate);
  if (file) {
    file.WriteVector(names_);
  } else {
    clog << "Error saving NAMES.LST" << endl;
  }
}

}
}
