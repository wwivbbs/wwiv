#include "wwiv_stub.h"

#include "context.h"
#include "value.h"
#include "core/strings.h"
#include "fmt/format.h"

#include <algorithm>
#include <istream>
#include <string>

namespace wwivbasic {

using wwiv::strings::ToStringUpperCase;

namespace {
using Args = std::vector<Value>;

std::string arg_or(const Args& args, size_t i, const std::string& def) {
  return i < args.size() ? args[i].toString() : def;
}
} // namespace

std::string WwivStub::read_line() {
  std::string line;
  if (!std::getline(in_, line)) {
    return {};
  }
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  return line;
}

WwivStub::WwivStub(Context& ctx, std::istream& in) : ctx_(ctx), in_(in) {
  auto& wwiv = ctx_.add_module("wwiv");
  wwiv.native_function("VERSION", []() { return std::string("5.9.0.stub"); });
  wwiv.native_function("MODULE_NAME", [this]() {
    // Like WWIV, this one also prints its name.
    ctx_.out("wwiv\n");
    return std::string("wwiv");
  });
  wwiv.native_functionl("COMMAND", [this](const Args& args) {
    ctx_.out(fmt::format("[command: {} {}]\n", arg_or(args, 0, ""), arg_or(args, 1, "")));
    return Value();
  });
  wwiv.native_function("EVAL", [](std::string) { return 1; });
  wwiv.native_function("INTERPRET", [this](std::string s) {
    if (s.empty()) {
      return std::string();
    }
    const auto it = macros.find(s.front());
    return it == macros.end() ? std::string() : it->second;
  });

  auto& io = ctx_.add_module("wwiv.io");
  io.native_function("MODULE_NAME", []() { return std::string("wwiv.io"); });
  io.native_function("PUTS", [this](std::string s) { ctx_.out(s); });
  io.native_function("OUTSTR", [this](std::string s) { ctx_.out(s); });
  io.native_function("PL", [this](std::string s) { ctx_.out(s + "\n"); });
  io.native_function("PRINTFILE",
                     [this](std::string s) { ctx_.out(fmt::format("[printfile: {}]\n", s)); });
  io.native_function("GETS", [this](int max_len) {
    auto s = read_line();
    if (max_len > 0 && s.size() > static_cast<size_t>(max_len)) {
      s.resize(static_cast<size_t>(max_len));
    }
    return s;
  });
  io.native_function("GETKEY", [this]() { return read_line().substr(0, 1); });
  io.native_function("CLS", [this]() { ctx_.out("[cls]\n"); });
  io.native_functionl("NL", [this](const Args& args) {
    const auto n = args.empty() ? 1 : args.front().toInt();
    ctx_.out(std::string(static_cast<size_t>(std::max(n, 0)), '\n'));
    return Value();
  });
  // YN defaults to No and NY to Yes; both return 1 or 0 like WWIV.
  io.native_functionl("YN", [this](const Args& args) {
    ctx_.out(arg_or(args, 0, ""));
    const auto s = ToStringUpperCase(read_line());
    return Value(!s.empty() && s.front() == 'Y' ? 1 : 0);
  });
  io.native_functionl("NY", [this](const Args& args) {
    ctx_.out(arg_or(args, 0, ""));
    const auto s = ToStringUpperCase(read_line());
    return Value(!s.empty() && s.front() == 'N' ? 0 : 1);
  });
  io.native_function("PAUSE", [this]() { ctx_.out("[pause]\n"); });

  auto& data = ctx_.add_module("wwiv.data");
  data.native_function("MODULE_NAME", []() { return std::string("wwiv.data"); });
  // SAVE(scope, list) and LOAD(scope, list): scope is "GLOBAL" or "USER".
  data.native_function("SAVE", [this](std::string scope, Value list) {
    data_[ToStringUpperCase(scope)] = list.list("wwiv.data.save")->items;
  });
  data.native_function("LOAD", [this](std::string scope, Value list) {
    auto& items = list.list("wwiv.data.load")->items;
    const auto it = data_.find(ToStringUpperCase(scope));
    items = it == data_.end() ? std::vector<Value>{} : it->second;
  });
}

} // namespace wwivbasic
