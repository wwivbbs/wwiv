#pragma once

#include <string>
#include <string_view>

namespace wwivbasic {

// Where a script's output goes and its input comes from. This is the one
// interface the interpreter needs from its embedder: WWIV implements it with
// the BBS's output and input, basicrun with stdin/stdout (StdioConsole), and
// tests with a fake.
class Console {
public:
  virtual ~Console() = default;

  // Writes text. Any interpretation of the text, such as WWIV pipe color
  // codes, is up to the console.
  virtual void write(std::string_view text) = 0;
  // Ends the current line.
  virtual void newline() = 0;
  // Reads a line of input of at most max_length characters (0 means no
  // limit), without the line ending. Returns "" at the end of input.
  virtual std::string read_line(int max_length) = 0;
  // Reads a single key. Returns "" at the end of input.
  virtual std::string read_key() = 0;
  // Clears the screen.
  virtual void clear_screen() = 0;
};

} // namespace wwivbasic
