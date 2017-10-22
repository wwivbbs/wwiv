/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*               Copyright (C)2017, WWIV Software Services                */
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
#ifndef __INCLUDED_WWIVD_NODE_MANAGER_H__
#define __INCLUDED_WWIVD_NODE_MANAGER_H__

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace wwiv {
namespace wwivd {

enum class ConnectionType { UNKNOWN, SSH, TELNET, BINKP, HTTP };

/**
* \brief
* \param t Type of Connection
* \return Textual description of ConnectionType
*/
std::string to_string(ConnectionType t);

struct NodeStatus {
public:
  ConnectionType type = ConnectionType::UNKNOWN;
  int node = 0;
  std::string description;
  bool connected = false;
};

class NodeManager {
public:
  NodeManager(const std::string& name, ConnectionType type, int start, int end);
  virtual ~NodeManager();

  std::string status_string(const NodeStatus& n) const;

  std::vector<std::string> status_lines() const;

  NodeStatus& status_for_unlocked(int node);

  NodeStatus status_for_copy(int node);

  void set_node(int node, ConnectionType type, const std::string& description);

  void clear_node(int node);

  int nodes_used() const;

  bool AcquireNode(int& node);

  bool ReleaseNode(int node);


  int total_nodes() const { return end_ - start_ + 1; }
  int start_node() const { return start_; }
  int end_node() const { return end_; }

private:
  const std::string name_;
  const ConnectionType type_;
  int start_ = 0;
  int end_ = 0;
  std::map<int, NodeStatus> nodes_;

  mutable std::mutex mu_;
};

}  // namespace wwivd
}  // namespace wwiv

#endif  // __INCLUDED_WWIVD_NODE_MANAGER_H__
