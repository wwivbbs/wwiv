/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "sdk/usermanager.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/filenames.h"
#include "sdk/phone_numbers.h"
#include "sdk/ssm.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/msgapi/email_wwiv.h"
#include "sdk/msgapi/message_api_wwiv.h"

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::msgapi;

namespace wwiv::sdk {

/////////////////////////////////////////////////////////////////////////////
// class UserManager

UserManager::UserManager(const wwiv::sdk::Config& config)
  : config_(config),
    data_directory_(config.datadir()), 
    userrec_length_(config.config()->userreclen), 
    max_number_users_(config.config()->maxusers),
    allow_writes_(true){
  if (config.versioned_config_dat()) {
    CHECK_EQ(config.config()->userreclen, sizeof(userrec))
      << "For WWIV 5.2 or later, we expect the userrec length to match what's written\r\n"
      << " in config.dat.\r\n"
      << "Please use a version of this tool compiled with your WWIV BBS.\r\n"
      << "If you see this message running against a WWIV 4.x instance, it is a bug.\r\n"
      << "config.config()->userreclen: " << config.config()->userreclen
      << "; sizeof(userrec): " << sizeof(userrec) << "\r\n";
  }
}

UserManager::~UserManager() { }

int  UserManager::num_user_records() const {
  File userList(PathFilePath(data_directory_, USER_LST));
  if (userList.Open(File::modeReadOnly | File::modeBinary)) {
    auto nSize = userList.length();
    auto nNumRecords = static_cast<int>(nSize / userrec_length_) - 1;
    return nNumRecords;
  }
  return 0;
}

bool UserManager::readuser_nocache(User *pUser, int user_number) {
  File userList(PathFilePath(data_directory_, USER_LST));
  if (!userList.Open(File::modeReadOnly | File::modeBinary)) {
    pUser->data.inact = inact_deleted;
    pUser->FixUp();
    return false;
  }
  auto nSize = userList.length();
  int nNumUserRecords = static_cast<int>(nSize / userrec_length_) - 1;

  if (user_number > nNumUserRecords) {
    pUser->data.inact = inact_deleted;
    pUser->FixUp();
    return false;
  }
  long pos = static_cast<long>(userrec_length_) * static_cast<long>(user_number);
  userList.Seek(pos, File::Whence::begin);
  userList.Read(&pUser->data, userrec_length_);
  pUser->FixUp();
  return true;
}

bool UserManager::readuser(User *pUser, int user_number) {
  return this->readuser_nocache(pUser, user_number);
}

bool UserManager::writeuser_nocache(User *pUser, int user_number) {
  File userList(PathFilePath(data_directory_, USER_LST));
  if (userList.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    auto pos = static_cast<long>(userrec_length_) * static_cast<long>(user_number);
    userList.Seek(pos, File::Whence::begin);
    userList.Write(&pUser->data, userrec_length_);
    return true;
  }
  return false;
}

bool UserManager::writeuser(User *pUser, int user_number) {
  if (user_number < 1 || user_number > max_number_users_ || !user_writes_allowed()) {
    return true;
  }

  return this->writeuser_nocache(pUser, user_number);
}

// Deletes a record from NAMES.LST (DeleteSmallRec)
static void DeleteSmallRecord(StatusMgr& sm, Names& names, const char *name) {
  int found_user = names.FindUser(name);
  if (found_user < 1) {
    LOG(ERROR) << "#*#*#*#*#*#*#*# '" << name << "' NOT ABLE TO BE DELETED";
    LOG(ERROR) << "#*#*#*#*#*#*#*# Run //RESETF to fix it.";
    return;
  }

  sm.Run([&](WStatus& s) {
    names.Remove(found_user);
    s.DecrementNumUsers();
    s.IncrementFileChangedFlag(WStatus::fileChangeNames);
    names.Save();
  });
}

// Inserts a record into NAMES.LST
static void InsertSmallRecord(StatusMgr& sm, Names& names, int user_number, const char *name) {
  sm.Run([&](WStatus& s) {
    names.Add(name, user_number);
    names.Save();
    s.IncrementNumUsers();
    s.IncrementFileChangedFlag(WStatus::fileChangeNames);
  });
}

static bool delete_votes(const std::string datadir, User& user) {
  DataFile<votingrec> voteFile(PathFilePath(datadir, VOTING_DAT),
                               File::modeReadWrite | File::modeBinary);
  if (!voteFile) {
    return false;
  }
  std::vector<votingrec> votes;
  voteFile.ReadVector(votes);
  auto num_vote_records = voteFile.number_of_records();
  for (auto cur_vote = 0; cur_vote < 20; cur_vote++) {
    if (user.GetVote(cur_vote)) {
      if (cur_vote <= num_vote_records) {
        auto &v = votes.at(cur_vote);
        v.responses[user.GetVote(cur_vote) - 1].numresponses--;
      }
      user.SetVote(cur_vote, 0);
    }
  }
  voteFile.Seek(0);
  return voteFile.WriteVector(votes);
}

static bool deluser(int user_number, const Config& config, UserManager& um, 
  StatusMgr& sm, Names& names, WWIVMessageApi& api) {
  User user;
  um.readuser(&user, user_number);

  if (user.IsUserDeleted()) {
    return true;
  }
  SSM ssm(config, um);
  ssm.delete_local_to_user(user_number);
  DeleteSmallRecord(sm, names, user.GetName());
  user.SetInactFlag(User::userDeleted);
  user.SetNumMailWaiting(0);
  {
    std::unique_ptr<WWIVEmail> email(api.OpenEmail());
    email->DeleteAllMailToOrFrom(user_number);
  }

  delete_votes(config.datadir(), user);
  um.writeuser(&user, user_number);

  // TODO(rushfan): It's unclear if this is really the right place
  // to do this.  We could go either wya on this, let the caller handle
  // the other things (like phone numbers), or 
  PhoneNumbers pn(config);
  if (!pn.IsInitialized()) {
    return false;
  }
  pn.erase(user_number, user.GetVoicePhoneNumber());
  pn.erase(user_number, user.GetDataPhoneNumber());
  return true;
}

bool UserManager::delete_user(int user_number) {
  StatusMgr sm(config_.datadir(), [](int) {});
  Names names(config_);
  MessageApiOptions options;
  WWIVMessageApi api(options, config_, {}, new NullLastReadImpl());

  deluser(user_number, config_, *this, sm, names, api);
  return true;
}

bool UserManager::restore_user(int user_number) {
  User user;
  this->readuser(&user, user_number);

  if (!user.IsUserDeleted()) {
    // Not deleted. Nothing to do.
    return true;
  }

  StatusMgr sm(config_.datadir(), [](int) {});
  Names names(config_);
  InsertSmallRecord(sm, names, user_number, user.GetName());
  user.ClearInactFlag(User::userDeleted);
  this->writeuser(&user, user_number);

  PhoneNumbers pn(config_);
  if (!pn.IsInitialized()) {
    return false;
  }
  pn.insert(user_number, user.GetVoicePhoneNumber());
  pn.insert(user_number, user.GetDataPhoneNumber());
  return true;
}


} // namespace wwiv
