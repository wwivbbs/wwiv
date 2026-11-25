#include "wwivbasic/stdlib/strings.h"
#include "core/strings.h"
#include "fmt/format.h"

#include <algorithm>
#include <cstdint>

namespace wwivbasic::stdlib {

int asc(std::string s) {
  return s.empty() ? 0 : static_cast<int>(static_cast<uint8_t>(s.front()));
}

std::string chr(int c) {
  if (c < 0 || c > 255) {
    return {};
  }
  return std::string(1, static_cast<int>(c & 0xff));
}

// LEFT, RIGHT and MID raise an error for a length of 0 or less, or a start
// outside the string, like my_basic. Positions are 0-based. A length longer
// than the string is clipped.

std::string left(std::string s, int len) {
  if (len <= 0) {
    throw BasicError(fmt::format("LEFT: length must be greater than 0, got {}", len));
  }
  return s.substr(0, std::min(static_cast<size_t>(len), s.size()));
}

std::string right(std::string s, int len) {
  if (len <= 0) {
    throw BasicError(fmt::format("RIGHT: length must be greater than 0, got {}", len));
  }
  const auto n = std::min(static_cast<size_t>(len), s.size());
  return s.substr(s.size() - n);
}

// MID(s, start, len) returns at most len characters from start. MID(s, start)
// (not in my_basic) returns the rest of s from start.
Value mid(std::vector<Value> args) {
  if (args.size() < 2 || args.size() > 3) {
    throw BasicError(fmt::format("MID: expected 2 or 3 arguments, got {}", args.size()));
  }
  const auto s = args.at(0).toString();
  const auto start = args.at(1).toInt();
  if (start < 0 || static_cast<size_t>(start) >= s.size()) {
    throw BasicError(fmt::format("MID: start {} is outside the string (length {})", start,
                                 s.size()));
  }
  if (args.size() == 2) {
    return Value(s.substr(static_cast<size_t>(start)));
  }
  const auto len = args.at(2).toInt();
  if (len <= 0) {
    throw BasicError(fmt::format("MID: length must be greater than 0, got {}", len));
  }
  return Value(s.substr(static_cast<size_t>(start), static_cast<size_t>(len)));
}

// The QBasic/VB string functions (without the $). Case changes and trimming
// are ASCII only, like the BBS's own (core/strings).

std::string ucase(std::string s) { return wwiv::strings::ToStringUpperCase(s); }

std::string lcase(std::string s) { return wwiv::strings::ToStringLowerCase(s); }

// Strings SPACE and STRING make are at most this long, so a script can't use
// up memory by mistake.
static constexpr int kMaxRepeat = 65535;

static std::string repeat(const char* fn, int n, char c) {
  if (n <= 0) {
    return {};
  }
  if (n > kMaxRepeat) {
    throw BasicError(fmt::format("{}: {} is more than {} characters", fn, n, kMaxRepeat));
  }
  return std::string(static_cast<size_t>(n), c);
}

std::string space(int n) { return repeat("SPACE", n, ' '); }

// STRING(n, c): c is a string (its first character is used) or a character
// code from 0 to 255.
std::string string_of(int n, Value c) {
  char ch{};
  if (c.is_number()) {
    const auto code = c.toInt();
    if (code < 0 || code > 255) {
      throw BasicError(fmt::format("STRING: character code {} isn't from 0 to 255", code));
    }
    ch = static_cast<char>(code);
  } else {
    const auto s = c.toString();
    if (s.empty()) {
      throw BasicError("STRING: the character is an empty string");
    }
    ch = s.front();
  }
  return repeat("STRING", n, ch);
}

std::string trim(std::string s) { return wwiv::strings::StringTrim(s); }

std::string ltrim(std::string s) {
  wwiv::strings::StringTrimBegin(&s);
  return s;
}

std::string rtrim(std::string s) { return wwiv::strings::StringTrimEnd(s); }

// INSTR(s, find[, start]) is where find first appears in s at or after start,
// counting from 0 as MID does, or -1 if it doesn't.
Value instr(std::vector<Value> args) {
  if (args.size() < 2 || args.size() > 3) {
    throw BasicError(fmt::format("INSTR: expected 2 or 3 arguments, got {}", args.size()));
  }
  const auto s = args.at(0).toString();
  const auto find = args.at(1).toString();
  const auto start = args.size() == 3 ? args.at(2).toInt() : 0;
  if (start < 0) {
    throw BasicError(fmt::format("INSTR: start must be 0 or more, got {}", start));
  }
  if (static_cast<size_t>(start) > s.size()) {
    return Value(-1);
  }
  const auto pos = s.find(find, static_cast<size_t>(start));
  return Value(pos == std::string::npos ? -1 : static_cast<int>(pos));
}
}
