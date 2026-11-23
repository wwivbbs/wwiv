using namespace wwiv::graphs;

int main() {
  // remember to insert edges both ways for an undirected graph
  adjacency_list_t adjacency_list(std::numeric_limits<uint16_t>::max());
  adjacency_list[1].push_back(edge(2, 10));
  adjacency_list[1].push_back(edge(3, 15));

  adjacency_list[2].push_back(edge(1, 10));
  adjacency_list[2].push_back(edge(3, 11));
  adjacency_list[2].push_back(edge(5, 2));

  adjacency_list[3].push_back(edge(1, 15));
  adjacency_list[3].push_back(edge(2, 11));
  adjacency_list[3].push_back(edge(4, 6));

  adjacency_list[4].push_back(edge(3, 6));
  adjacency_list[4].push_back(edge(5, 9));

  adjacency_list[5].push_back(edge(2, 2));
  adjacency_list[5].push_back(edge(4, 9));

  std::vector<float> cost_;
  std::vector<uint16_t> previous;
  all_paths(1, adjacency_list, cost_, previous);
  std::cout << "Cost from 1 to 5: " << cost_[5] << std::endl;
  std::copy_if(previous.begin(), previous.end(),
               std::ostream_iterator<uint16_t>(std::cout, " "),
               [](uint16_t i) { return i != NO_NODE; });
  std::list<uint16_t> path = shortest_path(5, previous);
  std::cout << "Path : ";
  std::copy(path.begin(), path.end(), std::ostream_iterator<uint16_t>(std::cout, " "));
  std::cout << std::endl;

  return 0;
}
