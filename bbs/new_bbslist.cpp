/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bbs/printfile.h"
#include "bbs/vars.h"  // for syscfg
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/wsession.h"
#include "core/strings.h"
#include "core/wtextfile.h"
#include "sdk/filenames.h"

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include "rapidjson/prettywriter.h"
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using rapidjson::Document;
using rapidjson::FileReadStream;
using rapidjson::FileWriteStream;
using rapidjson::PrettyWriter;
using rapidjson::StringRef;
using rapidjson::Value;
using rapidjson::Writer;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace bbslist {

static ConnectionType ConnectionTypeFromString(const string& s) {
  if (s == "telnet") {
    return ConnectionType::TELNET;
  } else if (s == "ssh") {
    return ConnectionType::SSH;
  } else if (s == "modem") {
    return ConnectionType::MODEM;
  }
  return ConnectionType::TELNET;
}

static string ConnectionTypeString(const ConnectionType t) {
  switch (t) {
  case ConnectionType::MODEM: return "modem";
  case ConnectionType::SSH: return "ssh";
  case ConnectionType::TELNET:
  default: return "telnet";
  }
}

static void ParseAddresses(BbsListEntry* entry, const Value& addresses) {
  if (!addresses.IsArray()) {
    return;
  }
  for (size_t j=0; j < addresses.Size(); j++) {
    const Value& address = addresses[j];
    if (!address.IsObject()) {
      continue;
    }
    ConnectionType type = ConnectionTypeFromString(address["type"].GetString());
    entry->addresses[type] = address["address"].GetString();
  }
}

static BbsListEntry* JsonValueToBbsListEntry(const Value& json_entry, int id) {
  if (!json_entry.IsObject()) {
    return nullptr;
  }
  BbsListEntry* bbs_entry = new BbsListEntry();
  bbs_entry->id = id;
  bbs_entry->name = json_entry["name"].GetString();
  bbs_entry->software = json_entry["software"].GetString();
  if (json_entry.HasMember("addresses")) {
    const Value& addresses = json_entry["addresses"];
    ParseAddresses(bbs_entry, addresses);
  }
  return bbs_entry;
}

bool LoadFromJSON(const string& dir, const string& filename, 
                  std::vector<std::unique_ptr<BbsListEntry>>* entries) {
  int id = 1;
  Document document;
  WTextFile file(dir, filename, "r");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to 
    // verify that the file exists first.
    return false;
  }
  
  char buf[8192];
  FileReadStream stream(file.GetFILE(), buf, sizeof(buf));
  document.ParseStream<rapidjson::kParseDefaultFlags>(stream);

  if (document.HasParseError()) {
    return false;
  }
  if (document.HasMember("bbslist")) {
    const Value& bbslist = document["bbslist"];
    if (!bbslist.IsArray()) {
      return false;
    }
    for (size_t i=0; i<bbslist.Size(); i++) {
      BbsListEntry* entry = JsonValueToBbsListEntry(bbslist[i], id++);
      if (entry != nullptr) {
        entries->emplace_back(entry);
      }
    }
  }
  return true;
}

static Value BbsListEntryToJsonValue(const BbsListEntry& entry, Document::AllocatorType& allocator) {
  Value json_entry(rapidjson::kObjectType);
  json_entry.AddMember("name", StringRef(entry.name.c_str()), allocator);
  json_entry.AddMember("software", StringRef(entry.software.c_str()), allocator);
  if (!entry.addresses.empty()) {
    Value addresses_json_entry(rapidjson::kArrayType);
    for (const auto& a : entry.addresses) {
      Value address_json_entry(rapidjson::kObjectType);
      const string type = ConnectionTypeString(a.first);

      Value type_value(rapidjson::kStringType);
      type_value.SetString(type.c_str(), allocator);
      address_json_entry.AddMember("type", type_value.Move(), allocator);

      Value address_value(rapidjson::kStringType);
      address_value.SetString(a.second.c_str(), allocator);
      address_json_entry.AddMember(StringRef("address"), address_value, allocator);
      
      addresses_json_entry.PushBack(address_json_entry.Move(), allocator);
    }
    json_entry.AddMember(StringRef("addresses"), addresses_json_entry, allocator);
  }

  // rapidjson supports C++11 move semantics.
  return json_entry;
}

bool SaveToJSON(const string& dir, const string& filename, 
                const std::vector<std::unique_ptr<BbsListEntry>>& entries) {
  Document document;
  WTextFile file(dir, filename, "w");
  if (!file.IsOpen()) {
    // rapidjson will assert if the file does not exist, so we need to 
    // verify that the file exists first.
    return false;
  }

  Document::AllocatorType& allocator = document.GetAllocator();
  document.SetObject();
  Value bbs_list(rapidjson::kArrayType);
  for (const auto& e : entries) {
    Value json_entry = BbsListEntryToJsonValue(*e, allocator);
    bbs_list.PushBack(json_entry, allocator);
  }
  document.AddMember("bbslist", bbs_list, allocator);
  
  char buf[8192];
  FileWriteStream stream(file.GetFILE(), buf, sizeof(buf));
  PrettyWriter<FileWriteStream> writer(stream);
  bool result = document.Accept(writer);
  stream.Flush();
  return result;
}

static bool ConvertLegacyList(
    const string& dir, const string& legacy_filename, 
    std::vector<std::unique_ptr<BbsListEntry>>* entries) {

  WTextFile legacy_file(dir, legacy_filename, "r");
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
    unique_ptr<BbsListEntry> e(new BbsListEntry());
    string name = line.substr(14, 42);
    e->name = StringTrimEnd(&name);
    e->addresses.insert(std::make_pair(ConnectionType::MODEM, line.substr(0, 12)));
    e->software = line.substr(74, 4);
    entries->emplace_back(e.release());
  }
  return true;
}

static string GetBbsListEntryAddress(const BbsListEntry* entry) {
  const auto& addresses = entry->addresses;
  if (addresses.size() == 0) {
    return "";
  }
  return addresses.begin()->second;
}

static void ReadBBSList(const vector<unique_ptr<BbsListEntry>>& entries) {
  for (const auto& entry : entries) {
    const string s = StringPrintf("|#9%3d: [|#1%-12s|#9] %-40.40s |#9(%s|#9)",
        entry->id,
        GetBbsListEntryAddress(entry.get()).c_str(),
        entry->name.c_str(), entry->software.c_str());
    GetSession()->bout << s << wwiv::endl;
  }
}

static void DeleteBbsListEntry() {
  vector<unique_ptr<BbsListEntry>> entries;
  LoadFromJSON(syscfg.datadir, BBSLIST_JSON, &entries);

  if (entries.empty()) {
    GetSession()->bout << "|12You can not delete an entry when the list is empty." << wwiv::endl;
    pausescr();
    return;
  }

  ReadBBSList(entries);
  GetSession()->bout << "Enter Entry Number to Delete: ";
  string s;
  input(&s, 4, true);
  int entry_num = atoi(s.c_str());
  if (entry_num <= 0) {
    // atoi returns a 0 on "" too.
    return;
  }

  for (vector<unique_ptr<BbsListEntry>>::iterator b = entries.begin(); b != entries.cend(); b++) {
    if (b->get()->id == entry_num) {
      entries.erase(b);
      SaveToJSON(syscfg.datadir, BBSLIST_JSON, entries);
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
    const vector<unique_ptr<BbsListEntry>>& entries) {
  for (const auto& e : entries) {
    const auto& a = e->addresses.find(ConnectionType::MODEM);
    if (a == e->addresses.end()) {
      continue;
    }
    if (phoneNumber == a->second) {
      return false;
    }
  }
  return true;
}

static bool AddBBSListEntry(vector<unique_ptr<BbsListEntry>>* entries) {
  GetSession()->bout << "\r\nPlease enter phone number:\r\n ###-###-####\r\n:";
  string phone_number;
  input(&phone_number, 12, true);
  if (!IsBBSPhoneNumberValid(phone_number)) {
    GetSession()->bout << "\r\n|#6 Error: Please enter number in correct format.\r\n\n";
    return false;
  }
  if (!IsBBSPhoneNumberUnique(phone_number, *entries)) {
    GetSession()->bout << "|#6Sorry, It's already in the BBS list.\r\n\n\n";
    return false;
  }
  GetSession()->bout << "|#3This number can be added! It is not yet in BBS list.\r\n\n\n"
                      << "|#7Enter the BBS name and comments about it (incl. V.32/HST) :\r\n:";
  unique_ptr<BbsListEntry> entry(new BbsListEntry());
  inputl(&entry->name, 50, true);
  GetSession()->bout << "\r\n|#7Enter BBS type (ie, |#1WWIV|#7):\r\n:";
  input(&entry->software, 12, true);

  entry->addresses.insert(std::make_pair(ConnectionType::MODEM, phone_number));
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#5Is this information correct? ";
  if (yesno()) {
    entries->emplace_back(entry.release());
    GetSession()->bout << "\r\n|#3This entry will be added to BBS list.\r\n";
    return true;
  }
  return false;
}

static char ShowBBSListMenuAndGetChoice() {
  GetSession()->bout.NewLine();
  if (so()) {
    GetSession()->bout <<
                       "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9]: (|#1R|#9)ead, (|#1A|#9)dd, (|#1D|#9)elete, (|#1N|#9)et : ";
    return onek("QRNAD");
  } else {
    GetSession()->bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9] (|#1R|#9)ead, (|#1A|#9)dd, (|#1N|#9)et : ";
    return onek("QRNA");
  }
}

void NewBBSList() {
  bool done = false;
  while (!done) {
    char ch = ShowBBSListMenuAndGetChoice();
    switch (ch) {
    case 'A': {
      vector<unique_ptr<BbsListEntry>> entries;
      LoadFromJSON(syscfg.datadir, BBSLIST_JSON, &entries);
      if (GetSession()->GetEffectiveSl() <= 10) {
        GetSession()->bout << "\r\n\nYou must be a validated user to add to the BBS list.\r\n\n";
        break;
      } else if (GetSession()->GetCurrentUser()->IsRestrictionAutomessage()) {
        GetSession()->bout << "\r\n\nYou can not add to the BBS list.\r\n\n\n";
        break;
      }
      if (AddBBSListEntry(&entries)) {
        SaveToJSON(syscfg.datadir, BBSLIST_JSON, entries);
      }
    } break;
    case 'D': {
      DeleteBbsListEntry();
    } break;
    case 'N':
      print_net_listing(false);
      break;
    case 'R': {
      vector<unique_ptr<BbsListEntry>> entries;
      LoadFromJSON(syscfg.datadir, BBSLIST_JSON, &entries);
      if (entries.empty()) {
        ConvertLegacyList(syscfg.gfilesdir, BBSLIST_MSG, &entries);
        SaveToJSON(syscfg.datadir, BBSLIST_JSON, entries);
      }
      ReadBBSList(entries);
    } break;
    case 'Q': return;
    }
  }
}

}  // bbslist
}  // wwiv
