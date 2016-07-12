/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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
#include "wwivutil/net/dump_packet.h"

#include <iostream>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/net_util.h"
#include "sdk/net.h"


using std::cerr;
using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;
using namespace wwiv::net;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {

static string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  StringTrimEnd(&human_date);
  return human_date;
}

int dump_file(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    cerr << "Unable to open file: " << filename << endl;
    return 1;
  }

  bool done = false;
  while (!done) {
    net_header_rec h;
    int num_read = f.Read(&h, sizeof(net_header_rec));
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
    cout << "type:        (" << main_type_name(h.main_type);
    if (h.main_type == main_type_net_info) {
      cout << "/" << net_info_minor_type_name(h.minor_type);
    } else if (h.main_type > 0) {
      cout << "/" << h.minor_type;
    }
    cout << ")" << endl;
    if (h.list_len > 0) {
      cout << "list_len:    " << h.list_len << endl;
    }
    cout << "daten:       " << daten_to_humantime(h.daten) << endl;
    cout << "length:      " << h.length << endl;
    if (h.method > 0) {
      cout << "compression: de" << h.method << endl;
    }

    if (h.list_len > 0) {
      // read list of addresses.
      std::vector<uint16_t> list;
      list.resize(h.list_len);
      f.Read(&list[0], 2 * h.list_len);
      cout << "System List: ";
      for (const auto item : list) {
        cout << item << " ";
      }
      cout << endl;
    }
    if (h.length > 0) {
      string text;
      int length = h.length;
      if (h.method > 0) {
        length -= 146; // sizeof EN/DE header.
        char header[147];
        f.Read(header, 146);
      }
      text.resize(length + 1);
      f.Read(&text[0], length);
      cout << "Text:" << endl << text << endl << endl;
    }
    cout << "==============================================================================" << endl;
  }
  return 0;
}

std::string DumpPacketCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <filename>" << endl;
  ss << "Example: dump S1.NET" << endl;
  return ss.str();
}

int DumpPacketCommand::Execute() {
  if (remaining().empty()) {
    cout << GetUsage() << GetHelp() << endl;
    return 2;
  }
  const string filename(remaining().front());
  return dump_file(filename);
}

bool DumpPacketCommand::AddSubCommands() {
  return true;
}


}
}
