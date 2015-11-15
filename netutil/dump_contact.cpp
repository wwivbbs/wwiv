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
#include "netutil/dump_contact.h"

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
#include <map>
#include <vector>
#include "core/file.h"
#include "core/strings.h"
#include "networkb/contact.h"
#include "networkb/connection.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using wwiv::sdk::Config;
using wwiv::net::Contact;
using wwiv::sdk::Networks;
using namespace wwiv::strings;

void dump_contact_usage() {
  cout << "Usage:   dump_contact" << endl;
  cout << "Example: dump_contact" << endl;
}

int dump_contact(map<const string, Contact> contacts, int argc, char** argv) {
  for (const auto& c : contacts) {
    std::cout << "CONTACT.NET information: : " << c.first << std::endl;
    std::cout << "===========================================================" << std::endl;
    std::cout << c.second.ToString() << std::endl;
  }

  return 0;
}
