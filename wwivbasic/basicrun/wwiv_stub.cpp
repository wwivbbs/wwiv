#include "wwivbasic/basicrun/wwiv_stub.h"

#include "core/strings.h"
#include "fmt/format.h"

#include <string>

namespace wwivbasic {

using wwiv::strings::ToStringUpperCase;

namespace {
using Args = std::vector<Value>;

std::string arg_or(const Args& args, size_t i, const std::string& def) {
  return i < args.size() ? args[i].toString() : def;
}
} // namespace

WwivStub::WwivStub(Interpreter& interp, Console& console) : console_(console) {
  auto& wwiv = interp.add_module("wwiv");
  wwiv.native_function("VERSION", []() { return std::string("5.9.0.stub"); });
  wwiv.native_function("MODULE_NAME", [this]() {
    // Like WWIV, this one also prints its name.
    console_.write("wwiv");
    console_.newline();
    return std::string("wwiv");
  });
  wwiv.native_functionl("COMMAND", [this](const Args& args) {
    console_.write(fmt::format("[command: {} {}]", arg_or(args, 0, ""), arg_or(args, 1, "")));
    console_.newline();
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

  auto& io = interp.add_module("wwiv.io");
  io.native_function("MODULE_NAME", []() { return std::string("wwiv.io"); });
  io.native_function("PUTS", [this](std::string s) { console_.write(s); });
  io.native_function("OUTSTR", [this](std::string s) { console_.write(s); });
  io.native_function("PL", [this](std::string s) {
    console_.write(s);
    console_.newline();
  });
  io.native_function("PRINTFILE", [this](std::string s) {
    console_.write(fmt::format("[printfile: {}]", s));
    console_.newline();
  });
  io.native_function("GETS", [this](int max_len) { return console_.read_line(max_len); });
  io.native_function("GETKEY", [this]() { return console_.read_key(); });
  io.native_function("CLS", [this]() { console_.clear_screen(); });
  io.native_functionl("NL", [this](const Args& args) {
    for (auto n = args.empty() ? 1 : args.front().toInt(); n > 0; n--) {
      console_.newline();
    }
    return Value();
  });
  // YN defaults to No and NY to Yes; both return 1 or 0 like WWIV.
  io.native_functionl("YN", [this](const Args& args) {
    console_.write(arg_or(args, 0, ""));
    const auto s = ToStringUpperCase(console_.read_line(0));
    return Value(!s.empty() && s.front() == 'Y' ? 1 : 0);
  });
  io.native_functionl("NY", [this](const Args& args) {
    console_.write(arg_or(args, 0, ""));
    const auto s = ToStringUpperCase(console_.read_line(0));
    return Value(!s.empty() && s.front() == 'N' ? 0 : 1);
  });
  io.native_function("PAUSE", [this]() {
    console_.write("[pause]");
    console_.newline();
  });

  auto& data = interp.add_module("wwiv.data");
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
