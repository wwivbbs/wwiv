/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*          Copyright (C)2020-2021, WWIV Software Services                */
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
#ifndef INCLUDED_NET_CORE_NETDAT_H
#define INCLUDED_NET_CORE_NETDAT_H

#include "core/clock.h"
#include "core/textfile.h"
#include <memory>
#include <map>
#include <filesystem>
#include <optional>
#include <vector>


namespace  wwiv::sdk::net {
class Network;
}

namespace wwiv::net {


class NetworkStat final {
public:
  NetworkStat() = default;
  ~NetworkStat() = default;
  [[nodiscard]] int k() const;

  int files{0};
  int bytes{0};

};

/**
 * Implements writing to and rolling over the netdat?.log files for
 * WWIVnet message reporting.
 */
class NetDat final {
public:
  enum class netdat_msgtype_t { banner, post, normal, error };
  
  static char NetDatMsgType(netdat_msgtype_t t);

  NetDat(std::filesystem::path gfiles, std::filesystem::path logs,
         const sdk::net::Network& net, char net_cmd, core::Clock& clock);

  ~NetDat();

  bool WriteStats();
  bool WriteLines();
  void add_file_bytes(int node, int bytes);
  void add_message(netdat_msgtype_t t, const std::string& msg);
  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::string ToDebugString() const;
  // Gets the filedate as "MM/DD/YY" from netdat{num}
  [[nodiscard]] std::optional<std::string> netdat_date_mmddyy(int num) const;

private:
  bool rollover();
  void WriteHeader();
  void WriteLine(const std::string& s);
  [[nodiscard]] std::unique_ptr<TextFile> open(const std::string& mode, int num) const;

  std::filesystem::path gfiles_;
  std::filesystem::path logs_;
  const sdk::net::Network& net_;
  const char net_cmd_;
  core::Clock& clock_;
  std::unique_ptr<TextFile> file_;
  std::map<int, NetworkStat> stats_;
  std::vector<std::string> lines_;
};

}

#endif
