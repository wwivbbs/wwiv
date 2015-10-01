/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
#include <algorithm>

#include "bbs/wwiv.h"
#include "core/strings.h"
#include "fix/fix.h"
#include "fix/log.h"
#include "fix/users.h"

// This causes compile errors before wwiv.h That seems broken
#include <algorithm>
#include <vector>
#include <set>

using namespace wwiv::strings;

namespace wwiv {
namespace fix {

int FixUsersCommand::Execute() {
    std::cout << "Runnning FixUsersCommand::Execute" << std::endl;
    	File userFile(syscfg.datadir, USER_LST);
	if(!userFile.Exists()) {
		Print(NOK, true, "%s does not exist.", userFile.full_pathname().c_str());
		giveUp();
	}

	WUserManager userMgr;
	userMgr.InitializeUserManager(syscfg.datadir, sizeof(userrec), syscfg.maxusers);
	Print(OK, true, "Checking USER.LST... found %d user records.", userMgr.GetNumberOfUserRecords());

	Print(OK, true, "TBD: Check for trashed user recs.");
	if(userMgr.GetNumberOfUserRecords() > syscfg.maxusers) {
		Print(OK, true, "Might be too many.");
			maybeGiveUp();
	} else {
		Print(OK, true, "Reasonable number.");
	}

	std::vector<smalrec> smallrecords;
	std::set<std::string> names;

  const int num_user_records = userMgr.GetNumberOfUserRecords();
	for(int i = 1; i <= num_user_records; i++) {
		WUser user;
		userMgr.ReadUser(&user, i);
		user.FixUp();
		userMgr.WriteUser(&user, i);
		if (!user.IsUserDeleted() && !user.IsUserInactive()) {
			smalrec sr = { 0 };
			strcpy((char*) sr.name, user.GetName());
			sr.number = static_cast<unsigned short>(i);
			std::string namestring((char*) sr.name);
			if (names.find(namestring) == names.end()) {
				smallrecords.push_back(sr);
				names.insert(namestring);
        const std::string msg = StringPrintf("Keeping user: %s #%d", sr.name, sr.number);
        Print(OK, true, msg.c_str());
			}
			else {
				std::cout << "[skipping duplicate user: " << namestring << " #" << sr.number << "]";
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

	Print(OK, true, "Checking NAMES.LST");
	File nameFile(syscfg.datadir, NAMES_LST);
	if(!nameFile.Exists()) {
		Print(NOK, true, "%s does not exist, regenerating with %d names", nameFile.full_pathname().c_str(),
			smallrecords.size());
		nameFile.Close();
		nameFile.Open(File::modeCreateFile | File::modeBinary | File::modeWriteOnly);
		nameFile.Write(&smallrecords[0], sizeof(smalrec) * smallrecords.size());
		nameFile.Close();

	} else {
		if(nameFile.Open(File::modeReadOnly | File::modeBinary)) {
			unsigned long size = nameFile.GetLength();
			unsigned short recs = static_cast<unsigned short>(size / sizeof(smalrec));
			if (recs != status.users) {
				status.users = recs;
				Print(NOK, true, "STATUS.DAT contained an incorrect user count.");
			} else {
				Print(OK, true, "STATUS.DAT matches expected user count of %d users.", status.users);
			}
		}
		nameFile.Close();
	}
    return 0;
}

}  // namespace fix
}  // namespace wwiv
