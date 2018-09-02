/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2017, WWIV Software Services             */
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

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::endl;
using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace files {

static constexpr char SPACE = ' ';

static void align_char_filename(char* file_name) {
  // TODO Modify this to handle long filenames
  char szFileName[40], szExtension[40];

  bool bInvalid = false;
  if (file_name[0] == '.') {
    bInvalid = true;
  }

  for (size_t i = 0; i < size(file_name); i++) {
    if (file_name[i] == '\\' || file_name[i] == '/' || file_name[i] == ':' || file_name[i] == '<' ||
        file_name[i] == '>' || file_name[i] == '|') {
      bInvalid = true;
    }
  }
  if (bInvalid) {
    strcpy(file_name, "        .   ");
    return;
  }
  char* s2 = strrchr(file_name, '.');
  if (s2 == nullptr || strrchr(file_name, '\\') > s2) {
    szExtension[0] = '\0';
  } else {
    strcpy(szExtension, &(s2[1]));
    szExtension[3] = '\0';
    s2[0] = '\0';
  }
  strcpy(szFileName, file_name);

  for (int j = strlen(szFileName); j < 8; j++) {
    szFileName[j] = SPACE;
  }
  szFileName[8] = '\0';
  bool bHasWildcard = false;
  bool bHasSpace = false;
  for (int k = 0; k < 8; k++) {
    if (szFileName[k] == '*') {
      bHasWildcard = true;
    }
    if (szFileName[k] == ' ') {
      bHasSpace = true;
    }
    if (bHasSpace) {
      szFileName[k] = ' ';
    }
    if (bHasWildcard) {
      szFileName[k] = '?';
    }
  }

  for (int i2 = strlen(szExtension); i2 < 3; i2++) {
    szExtension[i2] = SPACE;
  }
  szExtension[3] = '\0';
  bHasWildcard = false;
  for (int i3 = 0; i3 < 3; i3++) {
    if (szExtension[i3] == '*') {
      bHasWildcard = true;
    }
    if (bHasWildcard) {
      szExtension[i3] = '?';
    }
  }

  char buffer[MAX_PATH];
  for (int i4 = 0; i4 < 12; i4++) {
    buffer[i4] = SPACE;
  }
  strcpy(buffer, szFileName);
  buffer[8] = '.';
  strcpy(&(buffer[9]), szExtension);
  strcpy(file_name, buffer);
  for (int i5 = 0; i5 < 12; i5++) {
    file_name[i5] = to_upper_case<char>(file_name[i5]);
  }
}

static std::string align_filename(const std::string& file_name) {
  char s[MAX_PATH];
  strcpy(s, file_name.c_str());
  align_char_filename(s);
  return std::string(s);
}

static std::string stripfn(const char* file_name) {
  char szTempFileName[MAX_PATH];

  size_t nSepIndex = -1;
  for (size_t i = 0; i < size(file_name); i++) {
    if (file_name[i] == '\\' || file_name[i] == ':' || file_name[i] == '/') {
      nSepIndex = i;
    }
  }
  if (nSepIndex != -1) {
    strcpy(szTempFileName, &(file_name[nSepIndex + 1]));
  } else {
    strcpy(szTempFileName, file_name);
  }
  for (size_t i1 = 0; i1 < size(szTempFileName); i1++) {
    if (szTempFileName[i1] >= 'A' && szTempFileName[i1] <= 'Z') {
      szTempFileName[i1] = szTempFileName[i1] - 'A' + 'a';
    }
  }
  int j = 0;
  while (szTempFileName[j] != 0) {
    if (szTempFileName[j] == SPACE) {
      strcpy(&szTempFileName[j], &szTempFileName[j + 1]);
    } else {
      ++j;
    }
  }
  return szTempFileName;
}

static std::string stripfn(const std::string& file_name) { return stripfn(file_name.c_str()); }

Allow::Allow(const wwiv::sdk::Config& config) : data_directory_(config.datadir()) {
  loaded_ = Load();
}

static bool icompare_equals(const allow_entry_t& i, const allow_entry_t& j) {
  return StringCompareIgnoreCase(i.a, j.a) == 0;
}

static bool icompare_lessthan(const allow_entry_t& i, const allow_entry_t& j) {
  return StringCompareIgnoreCase(i.a, j.a) < 0;
}

static int icompare_int(const allow_entry_t& i, const allow_entry_t& j) {
  return StringCompareIgnoreCase(i.a, j.a);
}

allow_entry_t to_allow_entry(const std::string& fn) {
  allow_entry_t e{};
  to_char_array(e.a, stripfn(fn));
  return e;
}

bool operator==(const allow_entry_t& lhs, const allow_entry_t& rhs) { 
  return icompare_equals(lhs, rhs); 
}

 bool Allow::Add(const std::string& unaligned_filename) {
  auto fn = align_filename(unaligned_filename);
  if (fn.size() != 12) {
    LOG(ERROR) << "Can't add filename: '" << fn << "' to allow.dat, not 12 chars";
    return false;
  }
  allow_.emplace_back(to_allow_entry(fn));
  std::sort(std::begin(allow_), std::end(allow_), icompare_lessthan);
  return true;
}

bool Allow::Remove(const std::string& unaligned_filename) {
  auto e = to_allow_entry(align_filename(unaligned_filename));
  auto it = std::find(std::begin(allow_), std::end(allow_), e);
  if (it != std::end(allow_)) {
    allow_.erase(it);
    return true;
  }
  return false;
}

bool Allow::Load() {
  DataFile<allow_entry_t> file(FilePath(data_directory_, ALLOW_DAT));
  if (!file) {
    // Handle empty file for the 1st time.  This is fine.
    return true;
  }
  allow_.clear();
  return file.ReadVector(allow_);
}

bool Allow::Save() {
  DataFile<allow_entry_t> file(FilePath(data_directory_, ALLOW_DAT),
                               File::modeReadWrite | File::modeBinary | File::modeTruncate);
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
  auto e = to_allow_entry(align_filename(unaligned_filename));
  return !std::binary_search(std::begin(allow_), std::end(allow_), e, icompare_lessthan);
}

Allow::~Allow() {
  if (!save_on_exit_) {
    return;
  }
  Save();
}

} // namespace files
} // namespace sdk
} // namespace wwiv
