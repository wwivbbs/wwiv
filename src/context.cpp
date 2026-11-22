#include "context.h"
#include "core/textfile.h"
#include "core/stl.h"
#include "utils.h"
#include "fmt/format.h"
#include "stdlib/collections.h"
#include "stdlib/common.h"
#include "stdlib/numbers.h"
#include "stdlib/strings.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace wwiv::stl;

namespace wwivbasic {

Var* Module::find_var(const std::string& name) {
  // Variables are visible from the current function's scope and the
  // module's global scope; callers' locals are not visible.
  auto& current = scopes.back();
  if (auto it = current.local_vars.find(name); it != current.local_vars.end()) {
    return &it->second;
  }
  auto& global = scopes.front();
  if (auto it = global.local_vars.find(name); it != global.local_vars.end()) {
    return &it->second;
  }
  return nullptr;
}

void Module::upsert(const std::string& name, const Value& value) {
  if (auto* v = find_var(name)) {
    v->value(value);
    return;
  }
  scopes.back().local_vars.emplace(name, Var(name, value));
}

std::optional<Var> Module::var(const std::string& name) {
  if (auto* v = find_var(name)) {
    return *v;
  }
  return std::nullopt;
}

bool Module::has_var(const std::string& name) const {
  return contains(scopes.back().local_vars, name) || contains(scopes.front().local_vars, name);
}

bool Module::has_fn(const std::string& name) const {
  return contains(functions, name);
}

Context::Context(Console& console) : console(console) {
  // Start off with only global scope
  root = module = &add_module("");

  // Numeric
  stdlib::register_numbers(*root);

  // common
  root->native_functionl("LEN", stdlib::len);
  root->native_functionl("VAL", stdlib::val);

  //string
  root->native_function("ASC", stdlib::asc);
  root->native_function("CHR", stdlib::chr);
  root->native_function("LEFT", stdlib::left);
  root->native_function("RIGHT", stdlib::right);
  root->native_functionl("MID", stdlib::mid);
  root->native_function("STR", [](Value v) { return v.toString(); });

  // collections
  stdlib::register_collections(*root);
}

Module& Context::add_module(const std::string& name) {
  // Never replace a module: its class bindings may be referenced by native
  // functions that captured them.
  return modules.try_emplace(name, name).first->second;
}

void BasicParserErrorListener::syntaxError(antlr4::Recognizer*, antlr4::Token*, size_t line,
                                           size_t charPositionInLine, const std::string& msg,
                                           std::exception_ptr) {
  // Columns are reported 1-based, like most compilers.
  su_->errors.push_back(
      fmt::format("{}:{}:{}: error: {}", su_->filename_, line, charPositionInLine + 1, msg));
}

void Context::upsert(const std::string& name, const Value& value) {
  if (const auto [pkg, id] = split_package_from_id(name); !pkg.empty()) {
    if (contains(modules, pkg)) {
      modules.at(pkg).upsert(id, value);
      return;
    }
    throw BasicError(fmt::format("Unknown module '{}' in assignment to '{}'", pkg, name));
  }

  // Not fully qualified case.
  if (!module->has_var(name) && root->has_var(name)) {
    // No existing variable in current module, but one at the root, update it.
    root->upsert(name, value);
  } else {
    module->upsert(name, value);
  }
}

std::optional<Var> Context::var(const std::string& name) {
  if (const auto [pkg, id] = split_package_from_id(name); !pkg.empty()) {
    if (contains(modules, pkg)) {
      return modules.at(pkg).var(id);
    }
    return std::nullopt;
  }
  if (!module->has_var(name) && root->has_var(name)) {
    return root->var(name);
  }
  return module->var(name);
}

std::optional<ResolvedFunction> Context::try_find_function(const std::string& function_name) {
  if (const auto [pkg, id] = split_package_from_id(function_name); !pkg.empty()) {
    if (!contains(modules, pkg)) {
      return std::nullopt;
    }
    auto& m = modules.at(pkg);
    if (auto it = m.functions.find(id); it != m.functions.end()) {
      return ResolvedFunction{&m, &it->second};
    }
    return std::nullopt;
  }

  for (auto* m : {module, root}) {
    if (auto it = m->functions.find(function_name); it != m->functions.end()) {
      return ResolvedFunction{m, &it->second};
    }
  }
  // Modules imported with IMPORT "@name" can be called without the prefix.
  // Check the most recently registered (longest) names first.
  for (const auto* m : {module, root}) {
    for (auto it = m->imported_modules.rbegin(); it != m->imported_modules.rend(); ++it) {
      if (!contains(modules, *it)) {
        continue;
      }
      auto& im = modules.at(*it);
      if (auto fit = im.functions.find(function_name); fit != im.functions.end()) {
        return ResolvedFunction{&im, &fit->second};
      }
    }
  }
  return std::nullopt;
}

ResolvedFunction Context::find_function(const std::string& function_name) {
  if (auto f = try_find_function(function_name)) {
    return *f;
  }
  if (const auto [pkg, id] = split_package_from_id(function_name);
      !pkg.empty() && !contains(modules, pkg)) {
    throw BasicError(fmt::format("Unknown module '{}' calling '{}'", pkg, function_name));
  }
  throw BasicError(fmt::format("Unknown function '{}'", function_name));
}

std::filesystem::path Context::import_key(const std::filesystem::path& path) {
  std::error_code ec;
  auto key = std::filesystem::weakly_canonical(path, ec);
  return ec ? path.lexically_normal() : key;
}


bool Context::add_source(const std::filesystem::path& path, const std::string& raw_text) {
  if (contains(sources, path)) {
    return true;
  }
  auto text = raw_text;
  // Every statement ends in a newline, so make sure the last one does too.
  if (text.empty() || text.back() != '\n') {
    text.push_back('\n');
  }
  auto su = std::make_unique<SourceUnit>(path.string(), text);
  if (!su->errors.empty()) {
    errors.insert(std::end(errors), std::begin(su->errors), std::end(su->errors));
  }
  sources.insert_or_assign(path, std::move(su));
  return true;
}

bool Context::add_source(const std::filesystem::path& path) {
  TextFile f(path, "rb");
  if (!f) {
    errors.push_back(fmt::format("{}: error: Unable to open file", path.string()));
    return false;
  }
  return add_source(path, f.ReadFileIntoString());
}

std::optional<antlr4::tree::ParseTree*> Context::parseTree(const std::filesystem::path& filename) {
  if (!contains(sources, filename)) {
    return std::nullopt;
  }
  auto& su = sources.at(filename);
  if (!su->errors.empty()) {
    return std::nullopt;
  }
  return su->tree();
}

} // namespace wwivbasic
