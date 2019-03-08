/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "wwivutil/fix/users.h"

#include <algorithm>
#include <vector>
#include <set>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

namespace {
static statusrec_t st;
}

static char *dateFromTimeT(time_t t) {
  static char date_string[11];
  auto ds = time_t_to_mmddyy(t);
  to_char_array(date_string, ds);
  return date_string;
}

static std::string dateFromTimeTForLog(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string("%y%m%d");
}

static bool checkFileSize(File &file, unsigned long len) {
  if (!file.IsOpen()) {
    int nFileMode = File::modeReadOnly | File::modeBinary;
    file.Open(nFileMode);
  }
  unsigned long actual = file.length();
  file.Close();
  if (actual < len) {
    LOG(INFO) << file.full_pathname() << " too short (" << actual << "<"
      << len << ").";
    return false;
  }
  if (actual > len) {
    LOG(INFO) << file.full_pathname() << " too long (" << actual << ">"
      << len << ").";
    LOG(INFO) << "Attempting to continue.";
  }
  return true;
}

static void saveStatus(const std::string& datadir) {
  File statusDat(FilePath(datadir, STATUS_DAT));

  statusDat.Open(File::modeReadWrite | File::modeBinary);
  statusDat.Write(&st, sizeof(statusrec_t));
  statusDat.Close();
}

static bool initStatusDat(const std::string& datadir) {
  int nFileMode = File::modeReadOnly | File::modeBinary;
  bool update = false;
  File statusDat(FilePath(datadir, STATUS_DAT));
  if (!statusDat.Exists()) {
    LOG(INFO) << statusDat.full_pathname() << " NOT FOUND!";
    LOG(INFO) << "Recreating " << statusDat.full_pathname() ;
    memset(&st, 0, sizeof(statusrec_t));
    to_char_array(st.date1, "00/00/00");
    to_char_array(st.date2, st.date1);
    to_char_array(st.date3, st.date1);
    to_char_array(st.log1, "000000.log");
    to_char_array(st.log2, "000000.log");
    to_char_array(st.gfiledate, "00/00/00");
    st.callernum = 65535;
    st.wwiv_version = wwiv_num_version;
    update = true;
  } else {
    checkFileSize(statusDat, sizeof(statusrec_t));
    LOG(INFO) << "Reading " << statusDat.full_pathname() << "...";
    if (!statusDat.Open(nFileMode)) {
      LOG(INFO) << statusDat.full_pathname() << " NOT FOUND.";
      return false;
    }
    statusDat.Read(&st, sizeof(statusrec_t));
    statusDat.Close();

    // version check
    if (st.wwiv_version > wwiv_num_version) {
      LOG(INFO) << "Incorrect version of fix (this is for "
           << wwiv_num_version << ", you need " << st.wwiv_version << ")";
    }

    auto dt = DateTime::now();
    time_t val = dt.to_time_t();
    auto cur_date = dateFromTimeT(val);
    if (!iequals(st.date1, cur_date)) {
      to_char_array(st.date1, cur_date);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date1) corrected";
    }

    val -= 86400L;
    cur_date = dateFromTimeT(val);
    if (strcmp(st.date2, cur_date)) {
      strcpy(st.date2, cur_date);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date2) corrected";
    }
    auto log_file = StrCat(dateFromTimeTForLog(val), ".log");
    if (!iequals(log_file, st.log1)) {
      to_char_array(st.log1, log_file);
      update = true;
      LOG(INFO) << "Log filename error in STATUS.DAT (st.log1) corrected";
    }

    val -= 86400L;
    cur_date = dateFromTimeT(val);
    if (!iequals(st.date3, cur_date)) {
      strcpy(st.date3, cur_date);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date3) corrected";
    }
    log_file = StrCat(dateFromTimeTForLog(val), ".log");
    if (!iequals(log_file, st.log2)) {
      to_char_array(st.log2, log_file);
      update = true;
      LOG(INFO) << "Log filename error in STATUS.DAT (st.log2) corrected";
    }
  }
  if (update) {
    saveStatus(datadir);
  }
  return true;
}


bool FixUsersCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("exp", 'x', "Enable experimental features.", false));
  add_argument(BooleanCommandLineArgument("verbose", 'v', "Enable verbose output.", false));

  return true;
}

std::string FixUsersCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   fix users";
  ss << "Example: WWIVUTIL fix users";
  return ss.str();
}

int FixUsersCommand::Execute() {
  LOG(INFO) << "Runnning FixUsersCommand::Execute";

  initStatusDat(config()->config()->datadir());
  File userFile(FilePath(config()->config()->datadir(), USER_LST));
	if (!userFile.Exists()) {
    LOG(ERROR) << userFile.full_pathname() << " does not exist.";
    return 1;
	}

	UserManager userMgr(*config()->config());
  LOG(INFO) << "Checking USER.LST... found " << userMgr.num_user_records() << " user records.";
  LOG(INFO) << "TBD: Check for trashed user recs.";
	if (userMgr.num_user_records() > config()->config()->max_users()) {
    LOG(INFO) << "Might be too many.";
    if (!arg("exp").as_bool()) {
      return 1;
    }
	} else {
    LOG(INFO) << "Reasonable number.";
	}

	std::vector<smalrec> smallrecords;
	std::set<std::string> names;

  const int num_user_records = userMgr.num_user_records();
	for(int i = 1; i <= num_user_records; i++) {
		User user;
		userMgr.readuser(&user, i);
		user.FixUp();
		userMgr.writeuser(&user, i);
		if (!user.IsUserDeleted() && !user.IsUserInactive()) {
			smalrec sr = { 0 };
			strcpy((char*) sr.name, user.GetName());
			sr.number = static_cast<uint16_t>(i);
			std::string namestring((char*) sr.name);
			if (names.find(namestring) == names.end()) {
				smallrecords.push_back(sr);
				names.insert(namestring);
        if (arg("verbose").as_bool()) {
          LOG(INFO) << "Keeping user: " << sr.name << " #" << sr.number ;
        }
			} else {
				LOG(INFO) << "[skipping duplicate user: " << namestring << " #" << sr.number << "]";
			}
		}
	};

	std::sort(smallrecords.begin(), smallrecords.end(), [](const smalrec& a, const smalrec& b) -> bool {
		int equal = strcmp((char*)a.name, (char*)b.name);

		// Sort by user number if names match.
		if (equal == 0) {
			return a.number < b.number;
		}

		// Otherwise sort by name comparison.
		return equal < 0;
	});

	printf("size=%lu %lu\n", smallrecords.size(), sizeof(smalrec) * smallrecords.size());
  LOG(INFO) << "Checking NAMES.LST";
        File nameFile(FilePath(config()->config()->datadir(), NAMES_LST));
	if (!nameFile.Exists()) {
    LOG(INFO) << nameFile.full_pathname() << " does not exist, regenerating with "
         << smallrecords.size() << " names";
		nameFile.Open(File::modeCreateFile | File::modeBinary | File::modeWriteOnly);
		nameFile.Write(&smallrecords[0], sizeof(smalrec) * smallrecords.size());
		nameFile.Close();
	} else {
		if (nameFile.Open(File::modeReadOnly | File::modeBinary)) {
			long size = nameFile.length();
      uint16_t recs = static_cast<uint16_t>(size / sizeof(smalrec));
			if (recs != st.users) {
				st.users = recs;
        LOG(INFO) << "STATUS.DAT contained an incorrect user count.";
			} else {
        LOG(INFO) << "STATUS.DAT matches expected user count of " << st.users << " users.";
			}
		}
		nameFile.Close();
	}
  return 0;
}

}  // namespace wwivutil
}  // namespace wwiv
