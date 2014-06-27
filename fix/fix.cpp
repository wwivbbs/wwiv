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
#include "fix.h"

#include <algorithm>
#include <map>
#include <memory>

#define _DEFINE_GLOBALS_
#include "wwiv.h"
#include "log.h"
#include "dirs.h"
#include "users.h"
#include "fix_config.h"

void giveUp();
void maybeGiveUp();

namespace wwiv {
namespace fix {

Command::~Command() {}

BaseCommand::BaseCommand(FixConfiguration* config) : config_(config) {}
BaseCommand::~BaseCommand() {}

class CriticalFilesCommand : public BaseCommand {
public:
    CriticalFilesCommand(FixConfiguration* config) : BaseCommand(config) {}
    virtual ~CriticalFilesCommand() {}

    virtual int Execute() {
         std::vector<std::string> datafiles { ARCHIVER_DAT, WFC_DAT, COLOR_DAT };

	    Print(OK, true, "Checking for Critical DATA files...");
        for (const auto& datafile : datafiles) {
            WFile file(syscfg.datadir, datafile);
		    if(!file.Exists()) {
			    Print(NOK, true, "Critical file: %s is missing", datafile.c_str());
		    }
	    }
	    Print(OK, true, "All critical DATA files found");
        return 0;
    }
};

class NoopCommand : public BaseCommand {
public:
    NoopCommand(FixConfiguration* config, const std::string& name) : BaseCommand(config), name_(name) {}

    virtual int Execute() {
        std::cout << "NoopCommand::Execute: " << name_ << std::endl;
        return 0;
    }
    std::string name_;
};


bool checkDirExists(WFile &dir, const char *desc) {
	bool exist = dir.Exists();
	if(exist) {
        return true;
    }

	Print(NOK, false, "Unable to find dir '%s'", dir.GetFullPathName().c_str());
	Print(NOK, false, "for '%s' dir", desc);
	printf("   Do you wish to CREATE it (y/N)?\n");
	std::string s;
	std::cin >> s;
	if(s[0] == 'Y' || s[0] == 'y') {
		bool exist = WWIV_make_path(dir.GetFullPathName().c_str()) != 0;
		if(!exist) {
			Print(NOK, true, "Unable to create dir '%s' for %s dir.", dir.GetFullPathName().c_str(), desc);
            return false;
		}
	}
	return true;
}

// Kinda hacky but this is needed to giveup, maybeGiveUp.
static FixConfiguration* configuration;

class FixApplication {
 public:
     FixApplication() : num_dirs_(0) {
        ShowBanner();
     	// open the log
	    OpenLogFile("FIX.LOG");
     }

     ~FixApplication() {
        for (auto m : command_map) {
            delete m.second;
        }
        CloseLogFile();
     }

    void ShowBanner() {
        std::cout << "WWIV Bulletin Board System -" << wwiv_version << std::endl
            << "Copyright (c) 1998-2014, WWIV Software Services.\n"
            << "All Rights Reserved.\n\n"
            << "Compile Time : " << wwiv_date << std::endl << std::endl;
    }

    void checkFileSize(WFile &file, unsigned long len) {
	    if(!file.IsOpen()) {
		    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
		    file.Open(nFileMode);
	    }
	    unsigned long actual = file.GetLength();
	    file.Close();
	    if(actual < len) {
		    Print(NOK, true, "%s too short (%ld<%ld).", file.GetFullPathName().c_str(), actual, len);
		    giveUp();
	    }
	    if(actual > len) {
		    Print(NOK, true, "%s too long (%ld>%ld).", file.GetFullPathName().c_str(), actual, len);
		    Print(NOK, true, "Attempting to continue.");
	    }
    }

    void saveStatus() {
	    WFile statusDat(syscfg.datadir, STATUS_DAT);

	    statusDat.Open(WFile::modeReadWrite | WFile::modeBinary);
	    statusDat.Write(&status, sizeof(statusrec));
	    statusDat.Close();
    }

    void initStatusDat() {
	    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
	    bool update = false;
	    WFile statusDat(syscfg.datadir, STATUS_DAT);
	    if(!statusDat.Exists()) {
		    Print(NOK, true, "%s NOT FOUND.", statusDat.GetFullPathName().c_str());
		    Print(OK, true, "Recreating %s.", statusDat.GetFullPathName().c_str());
		    memset(&status, 0, sizeof(statusrec));
		    strcpy(status.date1, "00/00/00");
		    strcpy(status.date2, status.date1);
		    strcpy(status.date3, status.date1);
		    strcpy(status.log1, "000000.log");
		    strcpy(status.log2, "000000.log");
		    strcpy(status.gfiledate, "00/00/00");
		    status.callernum = 65535;
		    status.wwiv_version = wwiv_num_version;
		    update = true;
	    } else {
		    checkFileSize(statusDat, sizeof(statusrec));
		    Print(OK, true, "Reading %s...", statusDat.GetFullPathName().c_str());
		    if (!statusDat.Open(nFileMode)) {
			    Print(NOK, true, "%s NOT FOUND.", statusDat.GetFullPathName().c_str());
			    giveUp();
		    }
		    statusDat.Read(&status, sizeof(statusrec));
		    statusDat.Close();

		    // version check
		    if(status.wwiv_version > wwiv_num_version) {
			    Print(NOK, true, "Incorrect version of fix (this is for %d, you need %d)", wwiv_num_version, status.wwiv_version);
			    giveUp();
		    }

		    time_t val = time(NULL);
		    char *curDate = dateFromTimeT(val);
		    if(strcmp(status.date1, curDate)) {
			    strcpy(status.date1, curDate);
			    update = true;
			    Print(OK, true, "Date error in STATUS.DAT (status.date1) corrected");
		    }

		    val -= 86400L;
		    curDate = dateFromTimeT(val);
		    if(strcmp(status.date2, curDate)) {
			    strcpy(status.date2, curDate);
			    update = true;
			    Print(OK, true, "Date error in STATUS.DAT (status.date2) corrected");
		    }
		    char logFile[512];
		    snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
		    if(strcmp(status.log1, logFile)) {
			    strcpy(status.log1, logFile);
			    update = true;
			    Print(OK, true, "Log filename error in STATUS.DAT (status.log1) corrected");
		    }

		    val -= 86400L;
		    curDate = dateFromTimeT(val);
		    if(strcmp(status.date3, curDate)) {
			    strcpy(status.date3, curDate);
			    update = true;
			    Print(OK, true, "Date error in STATUS.DAT (status.date3) corrected");
		    }
		    snprintf(logFile, sizeof(logFile), "%s.log", dateFromTimeTForLog(val));
		    if(strcmp(status.log2, logFile)) {
			    strcpy(status.log2, logFile);
			    update = true;
			    Print(OK, true, "Log filename error in STATUS.DAT (status.log2) corrected");
		    }
	    }
	    if(update) {
		    saveStatus();
	    }
    }

    void initDirsDat() {
	    WFile dirsDat(syscfg.datadir, DIRS_DAT);
	    if(!dirsDat.Exists()) {
		    Print(NOK, true, "%s NOT FOUND.", dirsDat.GetFullPathName().c_str());
		    maybeGiveUp();
            return;
        }

        Print(OK, true, "Reading %s...", dirsDat.GetFullPathName().c_str());
		int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
		dirsDat.Open(nFileMode);
		directories = (directoryrec *)malloc(dirsDat.GetLength() + 1);
		if(directories == NULL) {
			Print(NOK, true, "Couldn't allocate %ld bytes for %s.", dirsDat.GetLength(), dirsDat.GetFullPathName().c_str());
			giveUp();
		}
		num_dirs_ = (dirsDat.Read(directories, dirsDat.GetLength())) / sizeof(directoryrec);
		dirsDat.Close();
		Print(OK, true, "Found %d directories", num_dirs_);
    }

    void initSubsDat() {
	    WFile subsDat(syscfg.datadir, SUBS_DAT);
	    if(!subsDat.Exists()) {
		    Print(NOK, true, "%s NOT FOUND.", subsDat.GetFullPathName().c_str());
		    maybeGiveUp();
            return;
	    } 
		Print(OK, true, "Reading %s...", subsDat.GetFullPathName().c_str());
		int nFileMode = WFile::modeReadOnly | WFile::modeBinary;
		subsDat.Open(nFileMode);
		subboards = (subboardrec *)malloc(subsDat.GetLength() + 1);
		if(subboards == NULL) {
			Print(NOK, true, "Couldn't allocate %ld bytes for %s.", subsDat.GetLength(), subsDat.GetFullPathName().c_str());
			giveUp();
		}
		int num_subs = (subsDat.Read(subboards, subsDat.GetLength())) / sizeof(subboardrec);
		subsDat.Close();
		Print(OK, true, "Found %d subs", num_subs);
    }

    void LoadCommands(FixConfiguration*& config) {
        command_map["dirs"] = new FixDirectoriesCommand(config, num_dirs_);
        command_map["critical_files"] = new CriticalFilesCommand(config);
        command_map["users"] = new FixUsersCommand(config);
    }

    void Init() {
	    WFile configFile(CONFIG_DAT);
	    if (!configFile.Exists()) {
		    Print(NOK, true, "%s NOT FOUND.", CONFIG_DAT);
		    giveUp();
	    }
	    int nFileMode = WFile::modeReadOnly | WFile::modeBinary;

	    checkFileSize(configFile, sizeof(configrec));
	    Print(OK, true, "Reading %s...", configFile.GetFullPathName().c_str());
	    configFile.Open(nFileMode);
	    int readIn = configFile.Read(&syscfg, sizeof(configrec));
	    configFile.Close();
	    if(readIn != sizeof(configrec)) {
		    Print(NOK, true, "Failed to read %s (%d != %d)", configFile.GetFullPathName().c_str(), readIn, sizeof(configrec));
		    giveUp();
	    }
	    syscfg.userreclen = sizeof(userrec);

	    WFile dataDir(syscfg.datadir);
	    if(!checkDirExists(dataDir, "Data")) {
		    Print(NOK, true, "Must find DATA directory to continue.");
		    giveUp();
	    }

	    initStatusDat();
	    initDirsDat();
	    initSubsDat();
    }

    void Run(FixConfiguration *config) {
        this->Init();
        this->LoadCommands(config);

        for (const auto& command : config->commands()) {
            std::cout << "executing command: " << command << std::endl;
            command_map.at(command)->Execute();
        }

        this->saveStatus();
    }

private:
    int num_dirs_;
    std::map<std::string, wwiv::fix::Command*> command_map;

};

}  // namespace fix
}  // namespace wwiv

void giveUp() {
	Print(NOK, true, "Giving up.");
	exit(-1);
}

void maybeGiveUp() {
	if (!wwiv::fix::configuration->flag_experimental) {
		Print(OK, true, "Future expansion might try to fix this problem.");
		giveUp();
	}
	else {
		Print(OK, true, "Using Experimental Features");
	}
}

int main(int argc, char *argv[]) {
	time_t startTime = time(nullptr);

    std::vector<std::string> command_names {"critical_files", "dirs", "users"};
    using wwiv::fix::FixConfiguration;
    std::unique_ptr<FixConfiguration> config(new FixConfiguration(command_names));
    wwiv::fix::configuration = config.get();
    wwiv::fix::FixApplication app;
    config->ParseCommandLine(argc, argv);

    app.Run(config.get());

    // TODO(rushfan): Add new commands for find_max_qscan and check_msg_consistency
	time_t endTime = time(nullptr);
	Print(OK, true, "FIX Completed.  Time elapsed: %d seconds\n\n", (endTime-startTime));
	return 0;
}
