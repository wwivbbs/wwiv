/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015 WWIV Software Services                 */
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
#include "sdk/phone_numbers.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/datafile.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using namespace wwiv::core;

namespace wwiv {
namespace sdk {

PhoneNumbers::PhoneNumbers(const Config& config) 
    : initialized_(config.IsInitialized()), datadir_(config.datadir()) {
  if (initialized_) {
    initialized_ = Load();
  }
}

PhoneNumbers::~PhoneNumbers() {}

bool PhoneNumbers::insert(int user_number, const std::string& phone_number) {
  if (phone_number.find("000-") != string::npos) {
    return false;
  }
  phonerec p{};
  p.usernum = static_cast<int16_t>(user_number);
  strcpy(p.phone, phone_number.c_str());
  phones_.emplace_back(p);
  
  return Save();
}

bool PhoneNumbers::erase(int user_number, const std::string& phone_number) {
  auto predicate = [=](const phonerec& p) {
    return phone_number == p.phone;
  };
  phones_.erase(std::remove_if(std::begin(phones_), std::end(phones_), predicate));
  return Save();
}

int PhoneNumbers::find(const std::string& phone_number) const {
  for (const auto& p : phones_) {
    if (phone_number == p.phone) {
      // TODO(rushfan): Also need check if the user is not deleted exists.
      return p.usernum;
    }
  }
  // not found.
  return 0;
}

bool PhoneNumbers::Load() {
  DataFile<phonerec> file(datadir_, PHONENUM_DAT,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    return false;
  }

  const int num_records = file.number_of_records();
  phones_.clear();
  phones_.resize(num_records);
  return file.Read(&phones_[0], num_records);
}

bool PhoneNumbers::Save() {
  DataFile<phonerec> file(datadir_, PHONENUM_DAT,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
  if (!file) {
    return false;
  }
  return file.Write(&phones_[0], phones_.size());
}


}
}
