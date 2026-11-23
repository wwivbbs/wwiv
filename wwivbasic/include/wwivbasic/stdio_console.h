#pragma once

#include "wwivbasic/console.h"

#include <iosfwd>

namespace wwivbasic {

// A Console on C++ streams (stdout and stdin by default). Input is line
// based: read_key() reads a line and returns its first character, so scripts
// can be driven from a file. clear_screen() writes "[cls]" on its own line.
class StdioConsole : public Console {
public:
  StdioConsole();
  StdioConsole(std::ostream& out, std::istream& in);

  void write(std::string_view text) override;
  void newline() override;
  std::string read_line(int max_length) override;
  std::string read_key() override;
  void clear_screen() override;

private:
  std::ostream& out_;
  std::istream& in_;
};

} // namespace wwivbasic
