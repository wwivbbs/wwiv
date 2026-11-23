#include "utils.h"

// "s" -> s
std::string remove_quotes(std::string s) {
  if (s.size() < 2) {
    return s;
  }
  if (s.size() == 2) {
    return {};
  }
  s.pop_back();
  return s.substr(1);
}

// Splits a package off from identifier, i.e "foo.bar.baz" -> {"foo.bar", "baz"}
std::tuple<std::string, std::string> split_package_from_id(const std::string& s) {
  if (const auto idx = s.rfind('.'); idx != std::string::npos) {
    const auto pkg = s.substr(0, idx);
    const auto id = s.substr(idx + 1);
    return std::make_tuple(pkg, id);
  }
  return std::make_tuple("", s);
}

