/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "binkp/net_log.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/net/networks.h"
#include <chrono>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv::net {

NetworkLog::NetworkLog(std::string gfiles_directory)
    : gfiles_directory_(std::move(gfiles_directory)) {}
NetworkLog::~NetworkLog() = default;

std::string NetworkLog::CreateLogLine(time_t time, NetworkSide side, int node,
                                      int bytes_sent, int bytes_received,
                                      std::chrono::seconds seconds_elapsed,
                                      const std::string& network_name) const {

  // Format: 01/03/15 20:26:23 To     1, S: 419k, R: 223k,           0.1 min  wwivnet
  // sprintf(s2, "S:%4ldk, R:%4ldk,", sent, recd);
  // sprintf(s, "%s To %5d, %s         %5s min  %s\r\n", s1, sn, s2, tmused, net_name);

  std::ostringstream ss;
  const auto dt = DateTime::from_time_t(time);
  ss << dt.to_string("%m/%d/%y %H:%M:%S") << " ";
  if (side == NetworkSide::FROM) {
    ss << "Fr ";
  } else {
    ss << "To ";
  }
  ss << fmt::sprintf("%5d", node);
  ss << fmt::sprintf(", S:%4dk", (bytes_sent + 1023) / 1024);
  ss << fmt::sprintf(", R:%4dk", (bytes_received + 1023) / 1024);
  ss << "          "; // should be ", %4.0f cps";
  ss << " ";          // last space before time.

  using float_minutes = std::chrono::duration<float, std::ratio<60>>;
  const auto minutes = std::chrono::duration_cast<float_minutes>(seconds_elapsed);
  ss << fmt::sprintf("%4.1f min ", minutes.count());
  ss << " " << network_name;

  return ss.str();
}

std::string NetworkLog::GetContents() const {
  TextFile file(FilePath(gfiles_directory_, NET_LOG), "r");
  if (!file.IsOpen()) {
    return "";
  }
  return file.ReadFileIntoString();
}

bool NetworkLog::Log(time_t time, NetworkSide side, int node, unsigned int bytes_sent,
                     unsigned int bytes_received, std::chrono::seconds seconds_elapsed,
                     const std::string& network_name) {

  const auto log_line =
      CreateLogLine(time, side, node, bytes_sent, bytes_received, seconds_elapsed, network_name);

  // Opening for "w" should truncate the existing file.
  TextFile file(FilePath(gfiles_directory_, NET_LOG), "a+t");
  file.WriteLine(log_line);

  return true;
}

std::string NetworkLog::ToString() const {
  return FilePath(gfiles_directory_, NET_LOG).string();
}

} // namespace wwiv
