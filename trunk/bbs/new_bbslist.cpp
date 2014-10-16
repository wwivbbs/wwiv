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

#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "printfile.h"
#include "core/strings.h"
#include "core/wtextfile.h"
#include "bbs/vars.h"  // for syscfg
#include <bbs/bbs.h>
#include <bbs/fcns.h>
#include <bbs/wsession.h>

#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using rapidjson::Document;
using rapidjson::FileReadStream;
using rapidjson::FileWriteStream;
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

bool LoadFromJSON(const string& dir, const string& filename, 
                  std::vector<std::unique_ptr<BbsListEntry>>* entries) {
  int id = 1;
  Document document;
  WTextFile file(dir, "bbslist.json", "r");
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

    for (int i=0; i<bbslist.Size(); i++) {
      const Value& json_entry = bbslist[i];
      if (!json_entry.IsObject()) {
        continue;
      }
      unique_ptr<BbsListEntry> scoped_bbs_entry(new BbsListEntry());
      scoped_bbs_entry->name = json_entry["name"].GetString();
      scoped_bbs_entry->software = json_entry["software"].GetString();
      // Get the raw pointer from the smart pointer, then release the smart pointer into
      // the output once we are sure this entry is viable.
      BbsListEntry* bbs_entry = scoped_bbs_entry.get();
      bbs_entry->id = id++;
      entries->emplace_back(scoped_bbs_entry.release());
      if (!json_entry.HasMember("addresses")) {
        continue;
      }
      const Value& addresses = json_entry["addresses"];
      if (!addresses.IsArray()) {
        continue;
      }
      for (int j=0; j < addresses.Size(); j++) {
        const Value& address = addresses[j];
        ConnectionType type = ConnectionTypeFromString(address["type"].GetString());
        bbs_entry->addresses[type] = address["address"].GetString();
      }
    }
  }

  return true;
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

  document.SetObject();
  Value bbs_list(rapidjson::kArrayType);
  for (const auto& e : entries) {
    Value json_entry(rapidjson::kObjectType);
    json_entry.AddMember("name", StringRef(e->name.c_str()), document.GetAllocator());
    json_entry.AddMember("software", StringRef(e->software.c_str()), document.GetAllocator());
    if (!e->addresses.empty()) {
      Value addresses_json_entry(rapidjson::kArrayType);
      for (const auto& a : e->addresses) {
        Value address_json_entry(rapidjson::kObjectType);
        const string type = ConnectionTypeString(a.first);
        Value type_value(rapidjson::kStringType);
        type_value.SetString(type.c_str(), document.GetAllocator());
        address_json_entry.AddMember("type", type_value, document.GetAllocator());
        Value address_value(rapidjson::kStringType);
        address_value.SetString(a.second.c_str(), document.GetAllocator());
        address_json_entry.AddMember("address", address_value, document.GetAllocator());
        addresses_json_entry.PushBack(address_json_entry, document.GetAllocator());
      }
      json_entry.AddMember(StringRef("addresses"), addresses_json_entry, document.GetAllocator());
    }
    bbs_list.PushBack(json_entry, document.GetAllocator());
  }
  document.AddMember("bbslist", bbs_list, document.GetAllocator());
  
  char buf[8192];
  FileWriteStream stream(file.GetFILE(), buf, sizeof(buf));
  Writer<FileWriteStream> writer(stream);
  bool result = document.Accept(writer);
  stream.Flush();
  return result;
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

static bool ConvertLegacyList(
    const string& dir, const string& filename, 
    std::vector<std::unique_ptr<BbsListEntry>>* entries) {

  WTextFile legacy_file(syscfg.gfilesdir, "bbslist.msg", "r");
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
    StringTrimEnd(&name);
    e->name = name;
    e->addresses.emplace(ConnectionType::MODEM, line.substr(0, 12));
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

static void ReadBBSList() {
  vector<unique_ptr<BbsListEntry>> entries;
  LoadFromJSON(syscfg.datadir, "bbslist.json", &entries);

  if (entries.empty()) {
    ConvertLegacyList(syscfg.datadir, "bbslist.json", &entries);
    SaveToJSON(syscfg.datadir, "bbslist.json", entries);
  }

  for (const auto& entry : entries) {
    const string s = StringPrintf("|#9[|#1%-12s|#9] %-40.40s |#9(%s|#9)", 
        GetBbsListEntryAddress(entry.get()).c_str(),
        entry->name.c_str(), entry->software.c_str());
    GetSession()->bout << s << wwiv::endl;
  }
}

static bool IsBBSPhoneNumberValid(const std::string& phoneNumber) {
  if (phoneNumber.empty()) {
    return false;
  }
  if (phoneNumber[3] != '-' || phoneNumber[7] != '-') {
    return false;
  }
  if (phoneNumber.length() != 12) {
    return false;
  }
  for (std::string::const_iterator iter = phoneNumber.begin(); iter != phoneNumber.end(); iter++) {
    if (strchr("0123456789-", (*iter)) == 0) {
      return false;
    }
  }
  return true;
}

static bool IsBBSPhoneNumberUnique(
    const std::string& phoneNumber,
    const std::vector<std::unique_ptr<BbsListEntry>>& entries) {
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

static bool AddBBSListEntryImpl(std::vector<std::unique_ptr<BbsListEntry>>* entries) {
  GetSession()->bout << "\r\nPlease enter phone number:\r\n ###-###-####\r\n:";
  string bbsPhoneNumber;
  input(&bbsPhoneNumber, 12, true);
  if (!IsBBSPhoneNumberValid(bbsPhoneNumber)) {
    GetSession()->bout << "\r\n|#6 Error: Please enter number in correct format.\r\n\n";
    return false;
  }
  if (!IsBBSPhoneNumberUnique(bbsPhoneNumber, *entries)) {
    GetSession()->bout << "|#6Sorry, It's already in the BBS list.\r\n\n\n";
    return false;
  }
  GetSession()->bout << "|#3This number can be added! It is not yet in BBS list.\r\n\n\n"
                      << "|#7Enter the BBS name and comments about it (incl. V.32/HST) :\r\n:";
  string bbsName;
  inputl(&bbsName, 50, true);
  GetSession()->bout << "\r\n|#7Enter BBS type (ie, |#1WWIV|#7):\r\n:";
  string bbsType;
  input(&bbsType, 4, true);

  unique_ptr<BbsListEntry> entry(new BbsListEntry());
  entry->name = bbsName;
  entry->software = bbsType;
  entry->addresses.emplace(ConnectionType::MODEM, bbsPhoneNumber);
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#5Is this information correct? ";
  if (yesno()) {
    entries->emplace_back(entry.release());
    GetSession()->bout << "\r\n|#3This entry will be added to BBS list.\r\n";
    return true;
  }
  return false;
}

void NewBBSList() {
  bool done = false;
  while (!done) {
    char ch = ShowBBSListMenuAndGetChoice();
    switch (ch) {
    case 'A': {
      vector<unique_ptr<BbsListEntry>> entries;
      LoadFromJSON(syscfg.datadir, "bbslist.json", &entries);
      if (GetSession()->GetEffectiveSl() <= 10) {
        GetSession()->bout << "\r\n\nYou must be a validated user to add to the BBS list.\r\n\n";
        break;
      } else if (GetSession()->GetCurrentUser()->IsRestrictionAutomessage()) {
        GetSession()->bout << "\r\n\nYou can not add to the BBS list.\r\n\n\n";
        break;
      }
      if (AddBBSListEntryImpl(&entries)) {
        SaveToJSON(syscfg.datadir, "bbslist.json", entries);
      }

    } break;
    case 'R':
      ReadBBSList();
      break;
    case 'N':
      print_net_listing(false);
      break;
    case 'Q': return;
    }
  }
}

}  // bbslist
}  // wwiv
