/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
/**************************************************************************/
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#else  // _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif 
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/net.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;

static string main_type_name(int typ) {
  switch (typ) {
  case main_type_net_info: return "main_type_net_info";
  case main_type_email: return "main_type_email";
  case main_type_post: return "main_type_post";
  case main_type_file: return "main_type_file";
  case main_type_pre_post: return "main_type_pre_post";
  case main_type_external: return "main_type_external";
  case main_type_email_name: return "main_type_email_name";
  case main_type_net_edit: return "main_type_net_edit";
  case main_type_sub_list: return "main_type_sub_list";
  case main_type_extra_data: return "main_type_extra_data";
  case main_type_group_bbslist: return "main_type_group_bbslist";
  case main_type_group_connect: return "main_type_group_connect";
  case main_type_unsed_1: return "main_type_unsed_1";
  case main_type_group_info: return "main_type_group_info";
  case main_type_ssm: return "main_type_ssm";
  case main_type_sub_add_req: return "main_type_sub_add_req";
  case main_type_sub_drop_req: return "main_type_sub_drop_req";
  case main_type_sub_add_resp: return "main_type_sub_add_resp";
  case main_type_sub_drop_resp: return "main_type_sub_drop_resp";
  case main_type_sub_list_info: return "main_type_sub_list_info";
  case main_type_new_post: return "main_type_new_post";
  case main_type_new_extern: return "main_type_new_extern";
  case main_type_game_pack: return "main_type_game_pack";
  default: return wwiv::strings::StringPrintf("UNKNOWN type #%d", typ);
  }
}

void dump_usage() {
  cout << "Usage:   dump <filename>" << endl;
  cout << "Example: dump S1.NET" << endl;
}

string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  wwiv::strings::StringTrimEnd(&human_date);

  return human_date;
}

int dump_file(const std::string& filename) {
  int f = open(filename.c_str(), O_BINARY | O_RDONLY);
  if (f == -1) {
    cerr << "Unable to open file: " << filename << endl;
    return 1;
  }

  bool done = false;
  while (!done) {
    net_header_rec h;
    int num_read = read(f, &h, sizeof(net_header_rec));
    if (num_read == 0) {
      // at the end of the packet.
      cout << "[End of Packet]" << endl;
      return 0;
    }
    if (num_read != sizeof(net_header_rec)) {
      cerr << "error reading header, got short read of size: " << num_read 
           << "; expected: " << sizeof(net_header_rec) << endl;
      return 1;
    }
    cout << "destination: " << h.touser << "@" << h.tosys << endl;
    cout << "from:        " << h.fromuser << "@" << h.fromsys << endl;
    cout << "type:        " << main_type_name(h.main_type);
    if (h.minor_type > 0) {
      cout << "; subtype: " << h.minor_type;
    }
    cout << endl;
    if (h.list_len > 0) {
      cout << "list_len:    " << h.list_len << endl;
    }
    cout << "daten:       " << daten_to_humantime(h.daten) << endl;
    cout << "length:      " << h.length << endl;
    if (h.method > 0) {
      cout << "compression: " << h.method << endl;
    }

    if (h.list_len > 0) {
      // read list of addresses.
      std::vector<uint16_t> list;
      list.resize(h.list_len);
      int list_num_read = read(f, &list[0], 2 * h.list_len);
      for (const auto item : list) {
        cout << item << " ";
      }
      cout << endl;
    }
    if (h.length > 0) {
      string text;
      text.resize(h.length + 1);
      int text_num_read = read(f, &text[0], h.length);
      cout << "Text:" << endl << text << endl << endl;
    }
    cout << "==============================================================================" << endl;
  }
  close(f);
  return 0;
}

int dump(const CommandLineCommand* command) {
  if (command->remaining().empty()) {
    cout << "Usage:   dump <filename>" << endl;
    cout << "Example: dump S1.NET" << endl;
    return 2;
  }
  const string filename(command->remaining().front());
  return dump_file(filename);
}