/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/phone_numbers.h"
#include "sdk/ssm.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/msgapi/email_wwiv.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include <memory>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::msgapi;

namespace wwiv::sdk {

/////////////////////////////////////////////////////////////////////////////
// class UserManager

UserManager::UserManager(const wwiv::sdk::Config& config)
  : config_(config),
    data_directory_(config.datadir()), 
    userrec_length_(config.userrec_length()), 
    max_number_users_(config.max_users()),
    allow_writes_(true){
  if (config.versioned_config_dat()) {
    CHECK_EQ(config.userrec_length(), sizeof(userrec))
      << "For WWIV 5.2 or later, we expect the userrec length to match what's written\r\n"
      << " in config.dat.\r\n"
      << "Please use a version of this tool compiled with your WWIV BBS.\r\n"
      << "If you see this message running against a WWIV 4.x instance, it is a bug.\r\n"
      << "config.config()->userreclen: " << config.userrec_length()
      << "; sizeof(userrec): " << sizeof(userrec) << "\r\n";
  }
}

UserManager::~UserManager() = default;

int  UserManager::num_user_records() const {
  File userList(FilePath(data_directory_, USER_LST));
  if (userList.Open(File::modeReadOnly | File::modeBinary)) {
    return static_cast<int>(userList.length() / userrec_length_) - 1;
  }
  return 0;
}

bool UserManager::readuser_nocache(User *u, int user_number) const {
  File file(FilePath(data_directory_, USER_LST));
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    u->data.inact = inact_deleted;
    u->FixUp();
    u->user_number_ = user_number;
    return false;
  }
  const auto nSize = file.length();
  const auto num_user_records = nSize / userrec_length_ - 1;

  if (user_number > num_user_records) {
    u->data.inact = inact_deleted;
    u->FixUp();
    u->user_number_ = user_number;
    return false;
  }
  const auto pos = userrec_length_ * user_number;
  file.Seek(pos, File::Whence::begin);
  file.Read(&u->data, userrec_length_);
  u->FixUp();
  u->user_number_ = user_number;
  return true;
}

bool UserManager::readuser(User *pUser, int user_number) const {
  return this->readuser_nocache(pUser, user_number);
}

std::optional<User> UserManager::readuser(int user_number, mask m) const {
  User u{};
  if (readuser(&u, user_number)) {
    if (m == mask::active && (u.deleted() || u.inactive())) {
      return std::nullopt;
    }
    if (m == mask::non_deleted && u.deleted()) {
      return std::nullopt;
    }
    if (m == mask::non_inactive && u.inactive()) {
      return std::nullopt;
    }
    return {u};
  }
  return std::nullopt;
}

std::optional<User> UserManager::readuser_nocache(int user_number) const {
  User u{};
  if (readuser_nocache(&u, user_number)) {
    return {u};
  }
  return std::nullopt;
}

bool UserManager::writeuser_nocache(const User *pUser, int user_number) {
  if (File file(FilePath(data_directory_, USER_LST));
      file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    const auto pos = static_cast<long>(userrec_length_) * static_cast<long>(user_number);
    file.Seek(pos, File::Whence::begin);
    file.Write(&pUser->data, userrec_length_);
    return true;
  }
  return false;
}

bool UserManager::writeuser(const User *pUser, int user_number) {
  if (user_number < 1 || user_number > max_number_users_ || !user_writes_allowed()) {
    return true;
  }

  return this->writeuser_nocache(pUser, user_number);
}

bool UserManager::writeuser(const User& user, int user_number) {
  return writeuser(&user, user_number);
}

bool UserManager::writeuser(const std::optional<User>& user, int user_number) {
  if (!user) {
    return false;
  }
  return writeuser(user.value(), user_number);
}

// Deletes a record from NAMES.LST (DeleteSmallRec)
static void DeleteSmallRecord(StatusMgr& sm, Names& names, const char *name) {
  int found_user = names.FindUser(name);
  if (found_user < 1) {
    LOG(ERROR) << "#*#*#*#*#*#*#*# '" << name << "' NOT ABLE TO BE DELETED";
    LOG(ERROR) << "#*#*#*#*#*#*#*# Run //RESETF to fix it.";
    return;
  }

  sm.Run([&](Status& s) {
    names.Remove(found_user);
    s.decrement_num_users();
    s.increment_filechanged(Status::file_change_names);
    names.Save();
  });
}

// Inserts a record into NAMES.LST
static void InsertSmallRecord(StatusMgr& sm, Names& names, int user_number, const char *name) {
  sm.Run([&](Status& s) {
    names.Add(name, user_number);
    names.Save();
    s.increment_num_users();
    s.increment_filechanged(Status::file_change_names);
  });
}

static bool delete_votes(const std::string& datadir, User& user) {
  DataFile<votingrec> voteFile(FilePath(datadir, VOTING_DAT),
                               File::modeReadWrite | File::modeBinary);
  if (!voteFile) {
    return false;
  }
  std::vector<votingrec> votes;
  voteFile.ReadVector(votes);
  const auto num_vote_records = voteFile.number_of_records();
  for (auto cur_vote = 0; cur_vote < 20; cur_vote++) {
    if (user.GetVote(cur_vote)) {
      if (cur_vote <= num_vote_records) {
        auto &v = wwiv::stl::at(votes, cur_vote);
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

  if (user.deleted()) {
    return true;
  }
  SSM ssm(config, um);
  ssm.delete_local_to_user(user_number);
  DeleteSmallRecord(sm, names, user.GetName());
  user.SetInactFlag(User::userDeleted);
  user.email_waiting(0);
  {
    auto email = api.OpenEmail();
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
  pn.erase(user_number, user.voice_phone());
  pn.erase(user_number, user.data_phone());
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

  if (!user.deleted()) {
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
  pn.insert(user_number, user.voice_phone());
  pn.insert(user_number, user.data_phone());
  return true;
}


} // namespace wwiv
