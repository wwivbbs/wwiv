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
#include "bbs/new_bbslist.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/input.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysopf.h"
#include "bbs/application.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

// This is defined in keycodes.h and breaks rapidjson since that is what is used
// for the template variable.
#undef CT

#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/map.hpp>

using std::left;
using std::map;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;

using wwiv::bbs::InputMode;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace bbslist {

template <class Archive>
void serialize(Archive & ar, wwiv::bbslist::BbsListEntry &b) {
  ar(cereal::make_nvp("name", b.name));
  try {
    ar(cereal::make_nvp("software", b.software));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(cereal::make_nvp("sysop_name", b.sysop_name));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(cereal::make_nvp("location", b.location));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }
  try {
    ar(cereal::make_nvp("addresses", b.addresses));
  } catch (const cereal::Exception& e) {
    ar.setNextName(nullptr);
    LOG(ERROR) << e.what();
  }
}

template <class Archive>
void serialize(Archive & ar, wwiv::bbslist::BbsListAddress &a) {
  ar(cereal::make_nvp("type", a.type));
  ar(cereal::make_nvp("address", a.address));
}

bool LoadFromJSON(const string& dir, const string& filename, 
                  std::vector<BbsListEntry>& entries) {
  entries.clear();

  TextFile file(dir, filename, "r");
  if (!file.IsOpen()) {
    return false;
  }
  string text = file.ReadFileIntoString();
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

bool SaveToJSON(const string& dir, const string& filename, 
                const std::vector<BbsListEntry>& entries) {
  std::ostringstream ss;
  {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("bbslist", entries));
  }
  
  TextFile file(dir, filename, "w");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to 
    // verify that the file exists first.
    return false;
  }

  file.Write(ss.str());
  return true;
}

static bool ConvertLegacyList(
    const string& dir, const string& legacy_filename, 
    std::vector<BbsListEntry>& entries) {

  TextFile legacy_file(dir, legacy_filename, "r");
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
    BbsListEntry e = {};
    string name = line.substr(14, 42);
    StringTrimEnd(&name);
    e.name = name;
    e.addresses.push_back({"modem", line.substr(0, 12)});
    e.software = line.substr(74, 4);
    entries.emplace_back(e);
  }
  return true;
}

static void ReadBBSList(const vector<BbsListEntry>& entries) {
  int cnt = 0;
  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " BBS List"));
  for (const auto& entry : entries) {
    bout.Color((++cnt % 2) == 0 ? 1 : 9);
    bout << left << setw(3) << entry.id << " : " << setw(60) << entry.name << " (" << entry.software << ")" << wwiv::endl;
    for (const auto& a : entry.addresses) {
      bout.Color((cnt % 2) == 0 ? 1 : 9);
      bout << std::setw(11) << a.type << " : " << a.address << wwiv::endl;
    }
  }
  bout.nl();
}

static void DeleteBbsListEntry() {
  vector<BbsListEntry> entries;
  LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);

  if (entries.empty()) {
    bout << "|#6You can not delete an entry when the list is empty." << wwiv::endl;
    pausescr();
    return;
  }

  ReadBBSList(entries);
  bout << "|#9(|#2Q|#9=|#1Quit|#9) Enter Entry Number to Delete: ";
  string s = input(4, true);
  int entry_num = atoi(s.c_str());
  if (entry_num <= 0) {
    // atoi returns a 0 on "" too.
    return;
  }

  for (auto b = std::begin(entries); b != std::end(entries); b++) {
    if (b->id == entry_num) {
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
    if (strchr("0123456789-", ch) == 0) {
      return false;
    }
  }
  return true;
}

static bool IsBBSPhoneNumberUnique(
    const string& phoneNumber,
    const vector<BbsListEntry>& entries) {
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
  bool has_pots = noyes();
  bout << "|#9Is this BBS telnettable? (Y/n)         : ";
  bool has_telnet = yesno();

  if (!has_telnet && !has_pots) {
    bout.nl();
    bout << "|#6Either a modem or telnet connection is required." << wwiv::endl;
    pausescr();
    return false;
  }

  bout.nl();
  BbsListEntry entry = {};
  if (has_pots) {
    bout << "|#9Enter the Modem Number   : ";
    string phone_number = Input1("", 12, true, InputMode::PHONE);
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
    string telnet_address = Input1("", 50, true, InputMode::MIXED);
    bout.nl();
    if (!telnet_address.empty()) {
      entry.addresses.push_back({"telnet", telnet_address});
    }
  }

  bout << "|#9Enter the BBS Name       : ";
  entry.name = Input1("", 50, true, InputMode::MIXED);
  bout.nl();
  bout << "|#9Enter BBS Type (ex. WWIV): ";
  entry.software = Input1("WWIV", 12, true, InputMode::UPPER);
  bout.nl();
  bout << "|#9Enter the BBS Location   : ";
  entry.location = Input1("", 50, true, InputMode::MIXED);
  bout.nl();
  bout << "|#9Enter the Sysop Name     : ";
  entry.sysop_name = Input1("", 50, true, InputMode::MIXED);
  bout.nl(2);
  bout << "|#5Is this information correct? ";
  if (yesno()) {
    entries.emplace_back(entry);
    bout << "\r\n|#3This entry will be added to BBS list.\r\n";
    return true;
  }
  return false;
}

static char ShowBBSListMenuAndGetChoice() {
  bout.nl();
  if (so()) {
    bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9]: (|#1R|#9)ead, (|#1A|#9)dd, (|#1D|#9)elete, (|#1N|#9)et : ";
    return onek("QRNAD");
  } else {
    bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9] (|#1R|#9)ead, (|#1A|#9)dd, (|#1N|#9)et : ";
    return onek("QRNA");
  }
}

void NewBBSList() {
  bool done = false;
  while (!done) {
    char ch = ShowBBSListMenuAndGetChoice();
    switch (ch) {
    case 'A': {
      vector<BbsListEntry> entries;
      LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
      if (a()->GetEffectiveSl() <= 10) {
        bout << "\r\n\nYou must be a validated user to add to the BBS list.\r\n\n";
        break;
      } else if (a()->user()->IsRestrictionAutomessage()) {
        bout << "\r\n\nYou can not add to the BBS list.\r\n\n\n";
        break;
      }
      if (AddBBSListEntry(entries)) {
        SaveToJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
      }
    } break;
    case 'D': {
      DeleteBbsListEntry();
    } break;
    case 'N':
      print_net_listing(false);
      break;
    case 'R': {
      vector<BbsListEntry> entries;
      LoadFromJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
      if (entries.empty()) {
        ConvertLegacyList(a()->config()->gfilesdir(), BBSLIST_MSG, entries);
        SaveToJSON(a()->config()->datadir(), BBSLIST_JSON, entries);
      }
      ReadBBSList(entries);
    } break;
    case 'Q': return;
    }
  }
}

}  // bbslist
}  // wwiv
