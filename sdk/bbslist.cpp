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
#include "sdk/bbslist.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iostream>
#include <iterator>
#include <memory>
#include <map>
#include <sstream>
#include <string>

#include "core/strings.h"
#include "core/inifile.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/graphs.h"
#include "core/log.h"
#include "core/textfile.h"
#include "sdk/connect.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"

using std::endl;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
using wwiv::core::DataFile;
using wwiv::core::IniFile;
using namespace wwiv::strings;
using namespace wwiv::sdk;

namespace wwiv {
namespace sdk {

// [[ VisibleForTesting ]]
bool ParseBbsListNetLine(const string& ss, net_system_list_rec* con, int32_t* reg_no) {
  if (ss.empty() || ss[0] != '@') {
    // skip empty lines and those not starting with @.
    return false;
  }
  memset(con, 0, sizeof(net_system_list_rec));
  *reg_no = 0;
  // LOG << ss;

  for (auto iter = ss.begin(); iter != ss.end(); iter++) {
      switch (*iter) {
      case '@': {
        con->sysnum = StringToUnsignedShort(string(++iter, ss.end()));
      } break;
      case '&':
        con->other |= other_net_coord;
        break;
      case '%':
        con->other |= other_group_coord;
        break;
      case '^':
        con->other |= other_area_coord;
        break;
      case '~':
        con->other |= other_subs_coord;
        break;
      case '$':
        con->other |= other_inet;
        break;
      case '\\':
        con->other |= other_fido;
        break;
      case '|':
        con->other |= other_telnet;
        break;
      case '<':
        con->other |= other_no_links;
        break;
      case '>':
        con->other |= other_fts_blt;
        break;
      case '!':
        con->other |= other_direct;
        break;
      case '/':
        con->other |= other_unregistered;
        break;
      case '?':
        con->other |= other_fax;
        break;
      case '_':
        con->other |= other_end_system;
        break;
      case '+':
        con->other |= other_net_server;
        break;
      case '=':
        con->other |= other_unused;
        break;
      // Phone number
      case '*': {
        ++iter;  // skip past *
        string phone_number;
        while (iter != ss.end() && !isspace(*iter)) {
          phone_number.push_back(*iter++);
        }
        strncpy(con->phone, phone_number.c_str(), sizeof(con->phone));
      }
      break;
      case '#': {
        con->speed = StringToUnsignedShort(string(++iter, ss.end()));
      } break;
      // Reg Number.
      case '[': {
        ++iter;  // skip past [
        string reg_number;
        while (iter != ss.end() && *iter != ']') {
          reg_number.push_back(*iter++);
        }
        *reg_no = StringToInt(reg_number);
      } break;
      case '\"': {
        ++iter;  // skip past first "
        string name;
        while (iter != ss.end() && *iter != '\"') {
          name.push_back(*iter++);
        }
        strncpy(con->name, name.c_str(), sizeof(con->name));
      }
      break;
      default:
        break;
      }
    }
    return true;
}

static bool ParseBbsListNetFile(
  std::map<uint16_t, net_system_list_rec>* node_config_map,
  std::map<uint16_t, int32_t>* reg_number_map,
  const string network_dir,
  wwiv::graphs::Graph& graph,
  uint16_t net_node_number) {
  TextFile bbs_list_file(network_dir, BBSLIST_NET, "rt");
  if (!bbs_list_file.IsOpen()) {
    return false;
  }

  // A line will be of the format @node *phone options [reg] "name"
  string line;
  while (bbs_list_file.ReadLine(&line)) {
    StringTrim(&line);
    net_system_list_rec node_config;
    // TODO: make map of reg #
    int32_t reg_number;
    if (ParseBbsListNetLine(line, &node_config, &reg_number)) {
      // Parsed a line correctly.
      std::list<uint16_t> path = graph.shortest_path_to(node_config.sysnum);
      float cost = graph.cost_to(node_config.sysnum);
      if (!std::isfinite(cost)) {
        // LOG << "high cost " << cost << " to " << node_config.sysnum;
        // std::clog << "Path to " << node_config.sysnum << ": ";
        // std::copy(path.begin(), path.end(), std::ostream_iterator<uint16_t>(std::clog, " "));
        // std::clog << std::endl;
        // graph.DumpCosts();
      }
      if (graph.has_node(node_config.sysnum) && path.front() == net_node_number) {
        path.pop_front();
        // We have a path...
        //std::copy(path.begin(), path.end(), std::ostream_iterator<uint16_t>(std::cout, " "));
        node_config.numhops = static_cast<int16_t>(path.size());
        node_config.xx.cost = cost;
        if (!path.empty()) {
          node_config.forsys = path.front();
        } else {
          node_config.forsys = node_config.sysnum;
        }
      } else {
        LOG << "no path to " << node_config.sysnum;
        node_config.numhops = 10000;
        node_config.xx.cost = 10000;
        node_config.forsys = std::numeric_limits<uint16_t>::max();
      }
      node_config_map->emplace(node_config.sysnum, node_config);
      reg_number_map->emplace(node_config.sysnum, reg_number);
    }
  }
  return true;
}

// static 
BbsListNet BbsListNet::ParseBbsListNet(uint16_t net_node_number, const std::string& network_dir) {
  BbsListNet b;

  //std::clog << "Processing " << network_dir << std::endl;
  // We now need to add in cost and routing information.
  Connect connect(network_dir);

  // Build the network graph
  wwiv::graphs::Graph graph(net_node_number, std::numeric_limits<uint16_t>::max());
  for (const auto& e : connect.node_config()) {
    const auto& c = e.second;
    uint16_t source = c.sysnum;
    
    auto cost_iter = c.cost.begin();
    for (auto dest_iter = c.connect.begin(); dest_iter != std::end(c.connect); dest_iter++, cost_iter++) {
      graph.add_edge(source, *dest_iter, *cost_iter);
    }
  }

  ParseBbsListNetFile(&b.node_config_, &b.reg_number_, network_dir, graph, net_node_number);
  return b;
}

// static 
BbsListNet BbsListNet::ReadBbsDataNet(const std::string& network_dir) {
  BbsListNet b;
  vector<net_system_list_rec> system_list;

  DataFile<net_system_list_rec> file(network_dir, BBSDATA_NET);
  if (file) {
    file.ReadVector(system_list);
    for (const auto s : system_list) {
      b.node_config_.emplace(s.sysnum, s);
    }
    file.Close();
  }
  return b;
}

BbsListNet::BbsListNet() {}

BbsListNet::BbsListNet(std::initializer_list<net_system_list_rec> l) {
  for (const auto& r : l) {
    node_config_.emplace(r.sysnum, r);
  }
}

BbsListNet::~BbsListNet() {}

const net_system_list_rec* BbsListNet::node_config_for(int node) const {
  auto iter = node_config_.find(node);
  if (iter != end(node_config_)) {
    return &iter->second;
  }
  return nullptr;
}

static std::string DumpBbsListNet(const net_system_list_rec& n) {
  std::ostringstream ss;
  ss << "sysnum:        " << n.sysnum << std::endl;
  ss << "name:          " << n.name << std::endl;
  ss << "forsys:        " << n.forsys << std::endl;
  ss << "numhops:       " << n.numhops << std::endl;
  ss << "other:         " << n.other << std::endl;
  ss << "phone:         " << n.phone << std::endl;
  ss << "speed:         " << n.speed << std::endl;
  ss << "cost:          " << n.xx.cost << std::endl;
  ss << "rout_fact:     " << n.xx.rout_fact << std::endl;
  return ss.str();
}

std::string BbsListNet::ToString() const {
  std::ostringstream ss;
  for (const auto& kv : node_config_) {
    ss << DumpBbsListNet(kv.second) << std::endl;
  }
  return ss.str();
}

}  // namespace net
}  // namespace wwiv

