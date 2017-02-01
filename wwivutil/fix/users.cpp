/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
  struct tm * pTm = localtime(&t);

  snprintf(date_string, sizeof(date_string), "%02d/%02d/%02d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
  return date_string;
}

static char *dateFromTimeTForLog(time_t t) {
  static char date_string[11];
  struct tm * pTm = localtime(&t);

  snprintf(date_string, sizeof(date_string), "%02d%02d%02d", pTm->tm_year % 100, pTm->tm_mon + 1, pTm->tm_mday);
  return date_string;
}

static bool checkFileSize(File &file, unsigned long len) {
  if (!file.IsOpen()) {
    int nFileMode = File::modeReadOnly | File::modeBinary;
    file.Open(nFileMode);
  }
  unsigned long actual = file.GetLength();
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
  File statusDat(datadir, STATUS_DAT);

  statusDat.Open(File::modeReadWrite | File::modeBinary);
  statusDat.Write(&st, sizeof(statusrec_t));
  statusDat.Close();
}

static bool initStatusDat(const std::string& datadir) {
  int nFileMode = File::modeReadOnly | File::modeBinary;
  bool update = false;
  File statusDat(datadir, STATUS_DAT);
  if (!statusDat.Exists()) {
    LOG(INFO) << statusDat.full_pathname() << " NOT FOUND!";
    LOG(INFO) << "Recreating " << statusDat.full_pathname() ;
    memset(&st, 0, sizeof(statusrec_t));
    strcpy(st.date1, "00/00/00");
    strcpy(st.date2, st.date1);
    strcpy(st.date3, st.date1);
    strcpy(st.log1, "000000.log");
    strcpy(st.log2, "000000.log");
    strcpy(st.gfiledate, "00/00/00");
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

    time_t val = time(nullptr);
    char *curDate = dateFromTimeT(val);
    if (strcmp(st.date1, curDate)) {
      strcpy(st.date1, curDate);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date1) corrected";
    }

    val -= 86400L;
    curDate = dateFromTimeT(val);
    if (strcmp(st.date2, curDate)) {
      strcpy(st.date2, curDate);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date2) corrected";
    }
    char logFile[512];
    snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
    if (strcmp(st.log1, logFile)) {
      strcpy(st.log1, logFile);
      update = true;
      LOG(INFO) << "Log filename error in STATUS.DAT (st.log1) corrected";
    }

    val -= 86400L;
    curDate = dateFromTimeT(val);
    if (strcmp(st.date3, curDate)) {
      strcpy(st.date3, curDate);
      update = true;
      LOG(INFO) << "Date error in STATUS.DAT (st.date3) corrected";
    }
    snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
    if (strcmp(st.log2, logFile)) {
      strcpy(st.log2, logFile);
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
  File userFile(config()->config()->datadir(), USER_LST);
	if (!userFile.Exists()) {
    LOG(ERROR) << userFile.full_pathname() << " does not exist.";
    return 1;
	}

	UserManager userMgr(config()->config()->datadir(), sizeof(userrec), 
      config()->config()->config()->maxusers);
  LOG(INFO) << "Checking USER.LST... found " << userMgr.GetNumberOfUserRecords() << " user records.";
  LOG(INFO) << "TBD: Check for trashed user recs.";
	if (userMgr.GetNumberOfUserRecords() > config()->config()->config()->maxusers) {
    LOG(INFO) << "Might be too many.";
    if (!arg("exp").as_bool()) {
      return 1;
    }
	} else {
    LOG(INFO) << "Reasonable number.";
	}

	std::vector<smalrec> smallrecords;
	std::set<std::string> names;

  const int num_user_records = userMgr.GetNumberOfUserRecords();
	for(int i = 1; i <= num_user_records; i++) {
		User user;
		userMgr.ReadUser(&user, i);
		user.FixUp();
		userMgr.WriteUser(&user, i);
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
	File nameFile(config()->config()->datadir(), NAMES_LST);
	if (!nameFile.Exists()) {
    LOG(INFO) << nameFile.full_pathname() << " does not exist, regenerating with "
         << smallrecords.size() << " names";
		nameFile.Open(File::modeCreateFile | File::modeBinary | File::modeWriteOnly);
		nameFile.Write(&smallrecords[0], sizeof(smalrec) * smallrecords.size());
		nameFile.Close();
	} else {
		if (nameFile.Open(File::modeReadOnly | File::modeBinary)) {
			long size = nameFile.GetLength();
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
