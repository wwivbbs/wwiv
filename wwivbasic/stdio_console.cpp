#include "wwivbasic/stdio_console.h"

#include <iostream>
#include <string>

namespace wwivbasic {

StdioConsole::StdioConsole() : StdioConsole(std::cout, std::cin) {}

StdioConsole::StdioConsole(std::ostream& out, std::istream& in) : out_(out), in_(in) {}

void StdioConsole::write(std::string_view text) { out_ << text; }

void StdioConsole::newline() { out_ << '\n'; }

std::string StdioConsole::read_line(int max_length) {
  // Show any prompt before waiting for input.
  out_.flush();
  std::string line;
  if (!std::getline(in_, line)) {
    return {};
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  if (max_length > 0 && line.size() > static_cast<size_t>(max_length)) {
    line.resize(static_cast<size_t>(max_length));
  }
  return line;
}

std::string StdioConsole::read_key() { return read_line(1); }

void StdioConsole::clear_screen() { out_ << "[cls]\n"; }

} // namespace wwivbasic
