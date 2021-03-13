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
#include "bbs/bbslist.h"

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/sysopf.h"
#include "common/com.h"
#include "common/input.h"
#include "common/pause.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "core/cereal_utils.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <vector>

// This is defined in keycodes.h and breaks rapidjson since that is what is used
// for the template variable.
#undef CT

#include "common/output.h"

#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective

using std::left;
using std::map;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;

using wwiv::common::InputMode;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;


namespace cereal {
//! Saving for std::map<std::string, std::string> for text based archives
// Note that this shows off some internal cereal traits such as EnableIf,
// which will only allow this template to be instantiated if its predicates
// are true
template <class Archive, class C, class A,
  traits::EnableIf<traits::is_text_archive<Archive>::value> = traits::sfinae> inline
  void save(Archive & ar, std::map<std::string, std::string, C, A> const & map) {
  for (const auto & i : map)
    ar(cereal::make_nvp(i.first, i.second));
}

//! Loading for std::map<std::string, std::string> for text based archives
template <class Archive, class C, class A,
  traits::EnableIf<traits::is_text_archive<Archive>::value> = traits::sfinae> inline
  void load(Archive & ar, std::map<std::string, std::string, C, A> & map) {
  map.clear();

  auto hint = map.begin();
  while (true) {
    const auto namePtr = ar.getNodeName();

    if (!namePtr)
      break;

    std::string key = namePtr;
    std::string value; ar(value);
    hint = map.emplace_hint(hint, std::move(key), std::move(value));
  }
}
} // namespace cereal


namespace wwiv::bbslist {

template <class Archive> void serialize(Archive& ar, BbsListEntry& b) {
  SERIALIZE(b, name);
  SERIALIZE(b, software);
  SERIALIZE(b, sysop_name);
  SERIALIZE(b, location);
  SERIALIZE(b, addresses);
}

template <class Archive> void serialize(Archive& ar, BbsListAddress& a) {
  SERIALIZE(a, type);
  SERIALIZE(a, address);
}

bool LoadFromJSON(const std::filesystem::path& dir, const string& filename,
                  std::vector<BbsListEntry>& entries) {
  entries.clear();

  TextFile file(FilePath(dir, filename), "r");
  if (!file.IsOpen()) {
    return false;
  }
  const auto text = file.ReadFileIntoString();
  std::stringstream ss;
  ss << text;
  cereal::JSONInputArchive load(ss);
  load(cereal::make_nvp("bbslist", entries));

  // Assign id numbers.
  int id = 1;
  for (auto& e : entries) {
    e.id = id++;
  }
  return true;
}

bool SaveToJSON(const std::filesystem::path& dir, const string& filename,
                const std::vector<BbsListEntry>& entries) {
  std::ostringstream ss;
  {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("bbslist", entries));
  }

  TextFile file(FilePath(dir, filename), "w");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to
    // verify that the file exists first.
    return false;
  }

  file.Write(ss.str());
  return true;
}

static bool ConvertLegacyList(const string& dir, const string& legacy_filename,
                              std::vector<BbsListEntry>& entries) {

  TextFile legacy_file(FilePath(dir, legacy_filename), "r");
  if (!legacy_file.IsOpen()) {
    return false;
  }

  string line;
  while (legacy_file.ReadLine(&line)) {
    if (line.size() < 79) {
      // All real bbs entry lines are 79
      continue;
    }
    if (line[3] != '-' || line[7] != '-') {
      // check for ddd-ddd-dddd
      continue;
    }
    BbsListEntry e{};
    auto name = line.substr(14, 42);
    StringTrimEnd(&name);
    e.name = name;
    e.addresses.push_back({"modem", line.substr(0, 12)});
    e.software = line.substr(74, 4);
    entries.emplace_back(e);
  }
  return true;
}

static void ReadBBSList(const vector<BbsListEntry>& entries) {
  auto cnt = 0;
  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " BBS List"));
  for (const auto& entry : entries) {
    bout.Color(++cnt % 2 == 0 ? 1 : 9);
    bout.format("{:<3} : {:<60} ({})\r\n", entry.id, entry.name, entry.software);
    for (const auto& a : entry.addresses) {
      bout.Color((cnt % 2) == 0 ? 1 : 9);
      bout.format("{:<11} : {}\r\n", a.type, a.address);
    }
  }
  bout.nl();
}

void delete_bbslist() {
  vector<BbsListEntry> entries;
  LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);

  if (entries.empty()) {
    bout << "|#6You can not delete an entry when the list is empty." << wwiv::endl;
    bout.pausescr();
    return;
  }

  ReadBBSList(entries);
  bout << "|#9(|#2Q|#9=|#1Quit|#9) Enter Entry Number to Delete: ";
  const auto r = bin.input_number_hotkey(1, {'Q'}, 1, 9999, false);
  if (r.key == 'Q' || r.num == 0) {
    return;
  }
  for (auto b = std::begin(entries); b != std::end(entries); ++b) {
    if (b->id == r.num) {
      entries.erase(b);
      bout << "|10Entry deleted." << wwiv::endl;
      SaveToJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
      return;
    }
  }
}

static bool IsBBSPhoneNumberValid(const std::string& phoneNumber) {
  if (phoneNumber.length() != 12) {
    return false;
  }
  if (phoneNumber[3] != '-' || phoneNumber[7] != '-') {
    return false;
  }
  for (const auto ch : phoneNumber) {
    if (!strchr("0123456789-", ch)) {
      return false;
    }
  }
  return true;
}

static bool IsBBSPhoneNumberUnique(const string& phoneNumber, const vector<BbsListEntry>& entries) {
  for (const auto& e : entries) {
    for (const auto& a : e.addresses) {
      if (a.type != "modem") {
        continue;
      }
      if (a.address == phoneNumber) {
        return false;
      }
    }
  }
  return true;
}

static bool AddBBSListEntry(vector<BbsListEntry>& entries) {
  bout.nl();
  bout << "|#9Does this BBS have a modem line? (y/N) : ";
  const auto has_pots = bin.noyes();
  bout << "|#9Is this BBS telnettable? (Y/n)         : ";
  const auto has_telnet = bin.yesno();

  if (!has_telnet && !has_pots) {
    bout.nl();
    bout << "|#6Either a modem or telnet connection is required." << wwiv::endl;
    bout.pausescr();
    return false;
  }

  bout.nl();
  BbsListEntry entry = {};
  if (has_pots) {
    bout << "|#9Enter the Modem Number   : ";
    const auto phone_number = bin.input_phonenumber("", 12);
    bout.nl();
    if (!IsBBSPhoneNumberValid(phone_number)) {
      bout << "\r\n|#6 Error: Please enter number in correct format.\r\n\n";
      return false;
    }
    if (!IsBBSPhoneNumberUnique(phone_number, entries)) {
      bout << "|#6Sorry, It's already in the BBS list.\r\n\n\n";
      return false;
    }
    entry.addresses.push_back({"modem", phone_number});
  }

  if (has_telnet) {
    bout << "|#9Enter the Telnet Address : ";
    const auto telnet_address = bin.input_text("", 50);
    bout.nl();
    if (!telnet_address.empty()) {
      entry.addresses.push_back({"telnet", telnet_address});
    }
  }

  bout << "|#9Enter the BBS Name       : ";
  entry.name = bin.input_text("", 50);
  bout.nl();
  bout << "|#9Enter BBS Type (ex. WWIV): ";
  entry.software = bin.input_upper("WWIV", 12);
  bout.nl();
  bout << "|#9Enter the BBS Location   : ";
  entry.location = bin.input_upper("", 50);
  bout.nl();
  bout << "|#9Enter the Sysop Name     : ";
  entry.sysop_name = bin.input_text("", 50);
  bout.nl(2);
  bout << "|#5Is this information correct? ";
  if (bin.yesno()) {
    entries.emplace_back(entry);
    bout << "\r\n|#3This entry will be added to BBS list.\r\n";
    return true;
  }
  return false;
}

static char ShowBBSListMenuAndGetChoice() {
  bout.nl();
  if (so()) {
    bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9]: (|#1R|#9)ead, (|#1A|#9)dd, (|#1D|#9)elete, "
            "(|#1N|#9)et : ";
    return onek("QRNAD");
  }
  bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9] (|#1R|#9)ead, (|#1A|#9)dd, (|#1N|#9)et : ";
  return onek("QRNA");
}

void add_bbslist() {
  vector<BbsListEntry> entries;
  LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
  if (a()->sess().effective_sl() <= 10) {
    bout << "\r\n\nYou must be a validated user to add to the BBS list.\r\n\n";
    return;
  }
  if (a()->user()->IsRestrictionAutomessage()) {
    bout << "\r\n\nYou can not add to the BBS list.\r\n\n\n";
    return;
  }
  if (AddBBSListEntry(entries)) {
    SaveToJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
  }
}

void read_bbslist() {
  vector<BbsListEntry> entries;
  LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
  if (entries.empty()) {
    ConvertLegacyList(a()->config()->gfilesdir(), BBSLIST_MSG, entries);
    SaveToJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
  }
  ReadBBSList(entries);
}

void BBSList() {
  while (!a()->sess().hangup()) {
    const auto ch = ShowBBSListMenuAndGetChoice();
    switch (ch) {
    case 'A': {
      add_bbslist();
    } break;
    case 'D': {
      delete_bbslist();
    } break;
    case 'N':
      print_net_listing(false);
      break;
    case 'R': {
      read_bbslist();
    } break;
    case 'Q':
      return;
    default: break;
    }
  }
}

} // namespace wwiv::bbslist
