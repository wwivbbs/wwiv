// From http://rosettacode.org/wiki/Dijkstra%27s_algorithm
// Also http://stackoverflow.com/a/22566583/1270019
#include "core/graphs.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <list>
#include <limits> 
#include <set>
#include <utility>
#include <algorithm> 
#include <iterator>

namespace wwiv {
namespace graphs {

using std::list;
using std::numeric_limits;
using std::vector;

static constexpr uint16_t NO_NODE = 0;
static constexpr float max_cost = numeric_limits<float>::infinity();

Graph::Graph(uint16_t node, uint16_t max_size) 
  : node_(node), max_size_(max_size), adjacency_list_(max_size), cost_() {
    cost_.clear();
    cost_.resize(max_size, max_cost);
    cost_[node] = 0;
    previous_.clear();
    previous_.resize(max_size, NO_NODE);
}

Graph::~Graph() {}

bool Graph::add_edge(uint16_t source, uint16_t dest, float cost) {
  if (computed_) {
    return false;
  }

  //std::clog << "adding edge: " << source << " " << dest << " " << cost << " " << std::boolalpha << computed_ << std::endl;
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
    float dist = queue.begin()->first;
    uint16_t u = queue.begin()->second;
    queue.erase(queue.begin());

    // Visit each edge exiting u
    const std::vector<edge> &edges = adjacency_list_[u];
    for (const auto& e : edges) {
      uint16_t v = e.node_;
      float cost_through_u = dist + e.cost_;
      if (cost_through_u < cost_[v]) {
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


void Graph::DumpCosts() const {
  std::clog << "costs_: ";
  for (int i = 0; i < std::numeric_limits<uint16_t>::max(); i++) {
    float cost = cost_[i];
    if (isfinite(cost)) {
      std::clog << i << "[" << cost_[i] << "] ";
    }
  }
  std::clog << std::endl;
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
}  // namespace graphs
}  // namespace wwiv
