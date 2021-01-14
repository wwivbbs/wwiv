/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_WWIV_GRAPHS_OS_H
#define INCLUDED_WWIV_GRAPHS_OS_H

#include "core/stl.h"
#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace wwiv::graphs {


struct edge {
  uint16_t node_;
  float cost_;

  edge(uint16_t node, float cost)
    : node_(node),
      cost_(cost) {
  }
};

typedef std::vector<std::vector<edge>> adjacency_list_t;

/*
 Use:
 Graph net(1, 200);
 net.add_edge(1, 2, 0);
 net.add_edge(2, 3, 0);
 net.add_edge(2, 1, 0);
 net.add_edge(3, 2, 0);

 list<uint16_t> path = net.shortest_path_to(3);
 */
class Graph final {
public:
  Graph(uint16_t node, uint16_t max_size);
  ~Graph();

  bool add_edge(uint16_t source, uint16_t dest, float cost);
  [[nodiscard]] bool has_node(uint16_t source);
  [[nodiscard]] std::list<uint16_t> shortest_path_to(uint16_t destination);
  [[nodiscard]] float cost_to(uint16_t destination);
  [[nodiscard]] std::string DumpCosts() const;

  [[nodiscard]] int num_hops_to(uint16_t destination) {
    const auto path = shortest_path_to(destination);
    if (path.empty()) {
      return 0;
    }
    return stl::size_int(path) - 1;
  }

private:
  uint16_t node_;
  adjacency_list_t adjacency_list_;
  bool computed_{false};
  std::vector<float> cost_;
  std::vector<uint16_t> previous_;

  void Compute();
};

} // namespace

#endif
