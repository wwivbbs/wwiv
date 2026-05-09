/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2022, WWIV Software Services             */
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
#ifndef INCLUDED_WWIVD_NODE_MANAGER_H
#define INCLUDED_WWIVD_NODE_MANAGER_H

#include "sdk/config.h"
#include "sdk/instance.h"
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

namespace wwiv::sdk {
  struct wwivd_matrix_entry_t;
}

namespace wwiv::wwivd {

enum class ConnectionType { UNKNOWN, SSH, TELNET, BINKP, HTTP };

/**
* \brief
* returns a human readable version of ConnectionType
*
* \param t Type of Connection
* \return Textual description of ConnectionType
*/
std::string to_string(ConnectionType t);

struct NodeStatus {
  ConnectionType type = ConnectionType::UNKNOWN;
  int node = 0;
  std::string peer;
  time_t connection_time;
  std::string description;
  int pid;
  bool connected = false;
  int user_number = 0;
};

class NodeManager final {
private:
  NodeManager(const wwiv::sdk::Config& config, const std::string& name, ConnectionType type, int start, int end, bool wwiv_bbs);
public:
  explicit NodeManager(const wwiv::sdk::Config& config, const wwiv::sdk::wwivd_matrix_entry_t& bbs);
  explicit NodeManager(const wwiv::sdk::Config& config, ConnectionType type);
  ~NodeManager();

  // Get rid of unwanted forms.
  NodeManager() = delete;
  NodeManager(const NodeManager&) = delete;
  NodeManager(NodeManager&&) = delete;
  NodeManager& operator=(const NodeManager&) = delete;
  NodeManager& operator=(NodeManager&&) = delete;

  [[nodiscard]] static std::string status_string(const NodeStatus& n);

  [[nodiscard]] std::vector<std::string> status_lines() const;

  // Updates the node statuses from the BBS.
  [[nodiscard]] bool update_nodes();

  [[nodiscard]] std::vector<NodeStatus> nodes() const;

  [[nodiscard]] NodeStatus& status_for_unlocked(int node);

  [[nodiscard]] NodeStatus status_for_copy(int node);

  void set_node(int node, ConnectionType type, const std::string& description);
  void set_pid(int node, int pid);

  void clear_node(int node);

  [[nodiscard]] int nodes_used() const;

  bool AcquireNode(int& node, const std::string& peer);

  bool ReleaseNode(int node);

  [[nodiscard]] int total_nodes() const { return end_ - start_ + 1; }
  [[nodiscard]] int start_node() const { return start_; }
  [[nodiscard]] int end_node() const { return end_; }

private:
  wwiv::sdk::Config config_;
  const std::string name_;
  const ConnectionType type_;
  int start_ = 0;
  int end_ = 0;
  bool wwiv_bbs_{ false };
  std::map<int, NodeStatus> nodes_;

  mutable std::mutex mu_;
};


class ConcurrentConnections final {
public:
  explicit ConcurrentConnections(int max_num);
  ~ConcurrentConnections();

  ConcurrentConnections() = delete;
  ConcurrentConnections(const ConcurrentConnections&) = delete;
  ConcurrentConnections(ConcurrentConnections&&) = delete;
  ConcurrentConnections& operator=(const ConcurrentConnections&) = delete;
  ConcurrentConnections& operator=(ConcurrentConnections&&) = delete;

  bool aquire(const std::string& peer);
  bool release(const std::string& peer);

private:
  int max_num_{1};
  std::mutex connection_mu_;
  std::unordered_map<std::string, int> map_;
};

}  // namespace

#endif
