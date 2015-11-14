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
#include "networkb/net_log.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::IniFile;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace net {

NetworkLog::NetworkLog(const std::string& gfiles_directory) 
    : gfiles_directory_(gfiles_directory) {}
NetworkLog::~NetworkLog() {}

std::string date_time(time_t t) {
  struct tm* local = localtime(&t);
  return StringPrintf("%02d/%02d/%02d %02d:%02d:%02d",
    local->tm_mon + 1, local->tm_mday, local->tm_year % 100,
    local->tm_hour, local->tm_min, local->tm_sec);
}

std::string NetworkLog::CreateLogLine(
  time_t time, NetworkSide side, int16_t node,
  unsigned int bytes_sent, unsigned int bytes_received,
  std::chrono::seconds seconds_elapsed, const std::string& network_name) {

  // Format: 01/03/15 20:26:23 To     1, S: 419k, R: 223k,           0.1 min  wwivnet
  // sprintf(s2, "S:%4ldk, R:%4ldk,", sent, recd);
  // sprintf(s, "%s To %5d, %s         %5s min  %s\r\n", s1, sn, s2, tmused, net_name);

  std::ostringstream ss;
  ss << date_time(time) << " ";
  if (side == NetworkSide::FROM) {
    ss << "Fr ";
  }
  else {
    ss << "To ";
  }
  ss << StringPrintf("%5d", node);
  if (bytes_sent > 0) {
    ss << StringPrintf(", S:%4uk", (bytes_sent + 1023) / 1024);
  }
  else {
    // TODO(rushfan): Should this be: ", Tried S" ? like network0 does when it tries to receive
    // Since we always try to send and receive?
    ss << "         ";
  }
  if (bytes_received > 0) {
    ss << StringPrintf(", R:%4uk", (bytes_received + 1023) / 1024);
  } else {
    // TODO(rushfan): Should this be: ", Tried R" ?
    ss << "        ";
  }
  ss << "          ";  // should be ", %4.0f cps";
  ss << " ";  // last space before time.

  using float_minutes = std::chrono::duration<float, std::ratio<60>>;
  auto minutes = std::chrono::duration_cast<float_minutes>(seconds_elapsed);
  ss << StringPrintf("%4.1f min ", minutes.count());
  ss << " " << network_name;

  return ss.str();
}

std::string NetworkLog::GetContents() const {
  TextFile file(gfiles_directory_, "NET.LOG", "r");
  if (!file.IsOpen()) {
    return "";
  }
  return file.ReadFileIntoString();
}

bool NetworkLog::Log(
    time_t time, NetworkSide side, int16_t node,
    unsigned int bytes_sent, unsigned int bytes_received,
    std::chrono::seconds seconds_elapsed, const std::string& network_name) {

  string previous_contents = GetContents();
  string log_line = CreateLogLine(
      time, side, node, bytes_sent, bytes_received, seconds_elapsed, network_name);

  // Opening for "w" should truncate the existing file.
  TextFile file(gfiles_directory_, "NET.LOG", "w");
  file.WriteLine(log_line);
  file.Write(previous_contents);

  return true;
}


std::string NetworkLog::ToString() const {
  return StringPrintf("%s%cNET.LOG", gfiles_directory_.c_str(), File::pathSeparatorChar);
}

}  // namespace net
}  // namespace wwiv

