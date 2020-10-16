/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2020, WWIV Software Services             */
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
#include "sdk/files/allow.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/file_record.h"
#include <algorithm>
#include <string>

using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

// TODO(rushfan): Maybe move this to files_record.h and
// get rid of the one in bbs/utility
static std::string _stripfn(const std::string& file_name) {
  const std::filesystem::path p(file_name);
  if (!p.has_filename()) {
    return {};
  }
  return wwiv::sdk::files::unalign(p.filename().string());
}

Allow::Allow(const wwiv::sdk::Config& config) : data_directory_(config.datadir()) {
  loaded_ = Load();
}

static bool icompare_equals(const allow_entry_t& i, const allow_entry_t& j) {
  return StringCompareIgnoreCase(i.a, j.a) == 0;
}

static bool icompare_lessthan(const allow_entry_t& i, const allow_entry_t& j) {
  return StringCompareIgnoreCase(i.a, j.a) < 0;
}

allow_entry_t to_allow_entry(const std::string& fn) {
  allow_entry_t e{};
  to_char_array(e.a, _stripfn(fn));
  return e;
}

bool operator==(const allow_entry_t& lhs, const allow_entry_t& rhs) { 
  return icompare_equals(lhs, rhs); 
}

 bool Allow::Add(const std::string& unaligned_filename) {
   const auto fn = align(unaligned_filename);
  if (fn.size() != 12) {
    LOG(ERROR) << "Can't add filename: '" << fn << "' to allow.dat, not 12 chars";
    return false;
  }
  allow_.emplace_back(to_allow_entry(fn));
  std::sort(std::begin(allow_), std::end(allow_), icompare_lessthan);
  return true;
}

bool Allow::Remove(const std::string& unaligned_filename) {
  const auto e = to_allow_entry(align(unaligned_filename));
  const auto it = std::find(std::begin(allow_), std::end(allow_), e);
  if (it != std::end(allow_)) {
    allow_.erase(it);
    return true;
  }
  return false;
}

bool Allow::Load() {
  DataFile<allow_entry_t> file(wwiv::core::FilePath(data_directory_, ALLOW_DAT));
  if (!file) {
    // Handle empty file for the 1st time.  This is fine.
    return true;
  }
  allow_.clear();
  return file.ReadVector(allow_);
}

bool Allow::Save() {
  DataFile<allow_entry_t> file(wwiv::core::FilePath(data_directory_, ALLOW_DAT),
                               File::modeReadWrite | File::modeBinary | File::modeTruncate | File::modeCreateFile);
  if (!file) {
    LOG(ERROR) << "Error saving allow.dat";
    return false;
  }

  std::sort(std::begin(allow_), std::end(allow_), icompare_lessthan);
  return file.WriteVector(allow_);
}

bool Allow::IsAllowed(const std::string& unaligned_filename) {
  if (!loaded_) {
    LOG(ERROR) << "Allow::IsAllowed called when !loaded_";
    return false;
  }
  const auto e = to_allow_entry(align(unaligned_filename));
  return !std::binary_search(std::begin(allow_), std::end(allow_), e, icompare_lessthan);
}

int Allow::size() const {
  return wwiv::stl::size_int(allow_);
}

Allow::~Allow() {
  if (!save_on_exit_) {
    return;
  }
  try {
    Save();
  } catch (const std::exception& e) {
    LOG(ERROR) << "Caught exception in Allow::~Allow: " << e.what();
  }
}

} // namespace wwiv
