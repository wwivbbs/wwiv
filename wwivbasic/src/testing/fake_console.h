#pragma once

#include "wwivbasic/console.h"

#include <deque>
#include <string>

namespace wwivbasic::testing {

// A Console for tests: output is captured in `output` (newlines as "\n",
// clear_screen as "[cls]\n"), and input is taken from `input`, one entry per
// read_line or read_key call.
class FakeConsole : public Console {
public:
  void write(std::string_view text) override { output += text; }
  void newline() override { output += '\n'; }
  std::string read_line(int max_length) override {
    auto s = next_input();
    if (max_length > 0 && s.size() > static_cast<size_t>(max_length)) {
      s.resize(static_cast<size_t>(max_length));
    }
    return s;
  }
  std::string read_key() override { return next_input().substr(0, 1); }
  void clear_screen() override { output += "[cls]\n"; }

  std::string output;
  std::deque<std::string> input;

private:
  std::string next_input() {
    if (input.empty()) {
      return {};
    }
    auto s = input.front();
    input.pop_front();
    return s;
  }
};

} // namespace wwivbasic::testing
