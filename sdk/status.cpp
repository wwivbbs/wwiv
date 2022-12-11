/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "sdk/status.h"

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"

#include <memory>
#include <string>
#include <utility>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::sdk {

static std::string sysoplog_filename(const std::string& d) {
  return fmt::sprintf("%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

Status::Status(std::string datadir, const statusrec_t& s)
    : status_(s), datadir_(std::move(datadir)) {}

Status::~Status() = default;

std::string Status::last_date(int days_ago) const {
  DCHECK_GE(days_ago, 0);
  DCHECK_LE(days_ago, 2);
  switch (days_ago) {
  case 0:
    return status_.date1;
  case 1:
    return status_.date2;
  case 2:
    return status_.date3;
  default:
    return status_.date1;
  }
}

std::string Status::log_filename(int nDaysAgo) const {
  DCHECK_GE(nDaysAgo, 0);
  switch (nDaysAgo) {
  case 0:
  {
    return sysoplog_filename(DateTime::now().to_string("%m/%d/%y"));
  }
  case 1:
    return status_.log1;
  case 2:
    return status_.log2;
  default:
    return status_.log1;
  }
}

void Status::ensure_callernum_valid() {
  if (status_.callernum != 65535) {
    this->caller_num(status_.callernum);
    status_.callernum = 65535;
  }
}

void Status::ensure_dates_valid() {
  if (status_.date1[8] != '\0') {
    status_.date1[6] = '0';
    status_.date1[7] = '0';
    status_.date1[8] = '\0'; // forgot to add null termination
  }

  auto currentDate = DateTime::now().to_string("%m/%d/%y");
  if (status_.date3[8] != '\0') {
    status_.date3[6] = currentDate[6];
    status_.date3[7] = currentDate[7];
    status_.date3[8] = '\0';
  }
  if (status_.date2[8] != '\0') {
    status_.date2[6] = currentDate[6];
    status_.date2[7] = currentDate[7];
    status_.date2[8] = '\0';
  }
  if (status_.date1[8] != '\0') {
    status_.date1[6] = currentDate[6];
    status_.date1[7] = currentDate[7];
    status_.date1[8] = '\0';
  }
  if (status_.gfiledate[8] != '\0') {
    status_.gfiledate[6] = currentDate[6];
    status_.gfiledate[7] = currentDate[7];
    status_.gfiledate[8] = '\0';
  }
}

bool Status::NewDay() {
  status_.callstoday = 0;
  status_.msgposttoday = 0;
  status_.localposts = 0;
  status_.emailtoday = 0;
  status_.fbacktoday = 0;
  status_.uptoday = 0;
  status_.activetoday = 0;
  status_.days++;

  // Need to verify the dates aren't trashed otherwise we can crash here.
  ensure_dates_valid();

  strcpy(status_.date3, status_.date2);
  strcpy(status_.date2, status_.date1);
  const auto d = DateTime::now().to_string("%m/%d/%y");
  to_char_array(status_.date1, d);
  strcpy(status_.log2, status_.log1);

  const auto log = sysoplog_filename(last_date(1));
  to_char_array(status_.log1, log);
  return true;
}

// StatusMgr
bool StatusMgr::reload_status() {
  if (auto file = DataFile<statusrec_t>(FilePath(datadir_, STATUS_DAT),
                                        File::modeBinary | File::modeReadWrite)) {
    char oldFileChangeFlags[7];
    for (auto nFcIndex = 0; nFcIndex < 7; nFcIndex++) {
      oldFileChangeFlags[nFcIndex] = statusrec_.filechange[nFcIndex];
    }
    if (!file.Read(0, &statusrec_)) {
      return false;
    }
    if (!callback_) {
      return true;
    }
    for (auto i = 0; i < 7; i++) {
      if (oldFileChangeFlags[i] == statusrec_.filechange[i]) {
        continue;
      }
      // Invoke callback on changes only if one is defined
      callback_(i);
    }
    return true;
  }

  return false;
}

std::unique_ptr<Status> StatusMgr::get_status() {
  this->reload_status();
  return std::make_unique<Status>(datadir_, statusrec_);
}

int StatusMgr::user_count() {
  return get_status()->num_users();
}

bool StatusMgr::Run(status_txn_fn fn) {
  auto at_exit = finally([&] { this->reload_status(); });
  if (auto file = DataFile<statusrec_t>(FilePath(datadir_, STATUS_DAT),
                                        File::modeBinary | File::modeReadWrite)) {
    if (file.Read(0, &statusrec_)) {
      Status status(datadir_, statusrec_);
      fn(status);
      file.Write(0, &status.status_);
    }
    return true;
  }
  return false;
}


}
