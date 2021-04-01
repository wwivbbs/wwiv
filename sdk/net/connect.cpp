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
#include "sdk/net/connect.h"

#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/net/networks.h"
#include <cctype>
#include <cfloat>
#include <map>
#include <sstream>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;

namespace wwiv::sdk {

template <typename I, typename C> 
static uint16_t PopStringToUnsignedShort(I& iter, const C& c) {
  std::string s;
  while (iter != std::end(c) && std::isdigit(*iter)) {
    s.push_back(*iter++);
  }
  return to_number<uint16_t>(s);
}

template <typename I, typename C>
static float PopStringToFloat(I& iter, const C& c) {
  std::string s;
  while (iter != std::end(c) && (std::isdigit(*iter) || *iter == '.')) {
    s.push_back(*iter++);
  }
  return to_number<float>(s);
}

template <typename I, typename C>
static void SkipWhiteSpace(I& iter, const C& c) {
  while (iter != std::end(c) && std::isspace(*iter)) {
    // skip whitespace
    ++iter;
  }
}

// [[ VisibleForTesting ]]
bool ParseConnectNetLine(const std::string& ss, net_interconnect_rec* con) {
  // A line will be of the format: @node (node=cost)+
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  // net_interconnect_rec is a struct with vectors now so
  // we shouldn't just memset it.
  con->clear();
  auto iter = ss.begin();
  // We know 1st char is @
  ++iter;
  con->sysnum = PopStringToUnsignedShort(iter, ss);
  SkipWhiteSpace(iter, ss);

  while (iter != std::end(ss)) {
      uint16_t dest = PopStringToUnsignedShort(iter, ss);
    if (*iter != '=') {
      LOG(ERROR) << "= expected. at: '" << *iter << "' line: " << ss;
      return false;
    }
    // skip =
    ++iter;
    float cost = PopStringToFloat(iter, ss);
    SkipWhiteSpace(iter, ss);
    con->connect.push_back(dest);
    if (cost < FLT_EPSILON) {
      //HACK - let's put this into a wwiv::core class
      cost = 0.0f;
    }
    con->cost.push_back(cost);
    con->numsys++;
  }
    return true;
}

static bool ParseConnectFile(std::map<uint16_t, net_interconnect_rec>* node_config_map,
                             const std::filesystem::path& network_dir) {
  TextFile connect_file(FilePath(network_dir, CONNECT_NET), "rt");
  if (!connect_file.IsOpen()) {
    return false;
  }
  // A line will be of the format: @node (node=cost)+
  std::string line;
  while (connect_file.ReadLine(&line)) {
    StringTrim(&line);
    net_interconnect_rec rec{};
    if (ParseConnectNetLine(line, &rec)) {
      // Parsed a line correctly.
      node_config_map->emplace(rec.sysnum, rec);
    }
  }
  return true;
}

Connect::Connect(const std::filesystem::path& network_dir) {
  ParseConnectFile(&node_config_, network_dir);
}

Connect::Connect(std::initializer_list<net_interconnect_rec> l) {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

Connect::~Connect() = default;

const net_interconnect_rec* Connect::node_config_for(int node) const {
  const auto iter = node_config_.find(static_cast<uint16_t>(node));
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

static std::string DumpConnect(const net_interconnect_rec& n) {
  std::ostringstream ss;
  ss << "@" << n.sysnum << " ";
  auto connect = std::begin(n.connect);
  auto cost = std::begin(n.cost);
  while (connect != end(n.connect)) {
    ss << *connect++;
    if (cost != end(n.cost)) {
      ss << "=" << *cost++;
    }
    ss << " ";
  }
  if (n.connect.size() != n.numsys || n.cost.size() != n.numsys) {
    ss << " [" << std::dec << n.numsys << "]";
  }
  return ss.str();
}

std::string Connect::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : node_config_) {
    ss << DumpConnect(kv.second) << std::endl;
  }
  return ss.str();
}

}  // namespace

