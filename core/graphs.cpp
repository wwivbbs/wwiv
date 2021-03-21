// From http://rosettacode.org/wiki/Dijkstra%27s_algorithm
// Also http://stackoverflow.com/a/22566583/1270019
#include "core/graphs.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::graphs {

static constexpr uint16_t NO_NODE = 0;
static constexpr float max_cost = std::numeric_limits<float>::infinity();

Graph::Graph(uint16_t node, uint16_t max_size)
  : node_(node), adjacency_list_(max_size) {
  cost_.clear();
  cost_.resize(max_size, max_cost);
  cost_[node] = 0;
  previous_.clear();
  previous_.resize(max_size, NO_NODE);
}

Graph::~Graph() = default;

bool Graph::add_edge(uint16_t source, uint16_t dest, float cost) {
  if (computed_) {
    return false;
  }

  //VLOG(3) << "adding edge: " << source << " " << dest << " " << cost << " " << std::boolalpha << computed_ << std::endl;
  adjacency_list_[source].emplace_back(dest, cost);
  return true;
}

bool Graph::has_node(uint16_t source) {
  return !adjacency_list_[source].empty();
}


void Graph::Compute() {
  computed_ = true;
  std::set<std::pair<float, uint16_t>> queue;
  queue.insert(std::make_pair(cost_[node_], node_));

  while (!queue.empty()) {
    const auto dist = queue.begin()->first;
    const auto u = queue.begin()->second;
    queue.erase(queue.begin());

    // Visit each edge exiting u
    const auto& edges = adjacency_list_[u];
    for (const auto& e : edges) {
      auto v = e.node_;
      if (const auto cost_through_u = dist + e.cost_; cost_through_u < cost_[v]) {
        queue.erase(std::make_pair(cost_[v], v));
        cost_[v] = cost_through_u;
        previous_[v] = u;
        queue.insert(std::make_pair(cost_[v], v));
      }
    }
  }
  //DumpCosts();
}

float Graph::cost_to(uint16_t destination) {
  if (!computed_) {
    Compute();
  }
  return cost_[destination];
}


std::string Graph::DumpCosts() const {
  std::ostringstream ss;
  ss << "costs_: ";
  for (auto i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    if (const auto cost = cost_[i]; std::isfinite(cost)) {
      ss << i << "[" << cost_[i] << "] ";
    }
  }
  return ss.str();
}


std::list<uint16_t> Graph::shortest_path_to(uint16_t destination) {
  if (!computed_) {
    Compute();
  }
  std::list<uint16_t> path;
  for (; destination != NO_NODE; destination = previous_[destination]) {
    path.push_front(destination);
  }
  return path;
}
} // namespace wwiv
