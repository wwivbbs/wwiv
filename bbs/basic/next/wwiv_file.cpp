/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#include "bbs/basic/next/wwiv_bindings.h"

#include "common/context.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;
using Args = std::vector<Value>;

namespace {

// Where wwiv.io.file opens files.
enum class file_dir_t { gfiles, menus };

// What OPEN_OPTIONS returns.
struct FileOptions {
  file_dir_t dir{file_dir_t::gfiles};
};

// What OPEN returns. The file stays open until the script ends.
struct ScriptFile {
  std::filesystem::path path;
  std::unique_ptr<TextFile> file;
  bool writable{false};

  [[nodiscard]] bool is_open() const { return file && file->IsOpen(); }
};

// Like the legacy engine (to_file_location_t in bbs/basic/util.cpp), any name
// other than MENUS, TEMP or BBS is GFILES. TEMP and BBS can't be opened.
file_dir_t to_file_dir(const std::string& s) {
  if (iequals(s, "MENUS")) {
    return file_dir_t::menus;
  }
  if (iequals(s, "TEMP") || iequals(s, "BBS")) {
    throw BasicError(
        fmt::format("OPEN_OPTIONS: invalid location '{}' (use GFILES or MENUS)", s));
  }
  return file_dir_t::gfiles;
}

void write_each(const char* fn, ScriptFile& f, const Args& args, bool newline) {
  if (!f.writable) {
    throw BasicError(
        fmt::format("{}: the file was not opened for writing (OPEN with mode \"W\")", fn));
  }
  if (!f.is_open()) {
    throw BasicError(fmt::format("{}: unable to write to '{}'", fn, f.path.filename().string()));
  }
  for (auto it = args.begin() + 1; it != args.end(); ++it) {
    const auto s = wwivbasic::from_value<std::string>(*it);
    if (newline) {
      f.file->WriteLine(s);
    } else {
      f.file->Write(s);
    }
  }
}

} // namespace

// The wwiv.io.file module (bbs/basic/wwiv_file.cpp), registered only when the
// sysop enables it. Files are read from (and written to) the gfiles directory,
// or read from the current menu set's directory.
void RegisterWwivIoFile(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.io.file");
  auto& options_class = m.class_<FileOptions>("file_options");
  auto& file_class = m.class_<ScriptFile>("file");

  // Checks the argument count (max_args 0: any number from min_args up) and
  // returns the file that is the first argument.
  auto file_arg = [&file_class](const char* fn, const Args& args, size_t min_args,
                                size_t max_args) {
    if (args.size() < min_args || (max_args && args.size() > max_args)) {
      throw BasicError(max_args == min_args
                           ? fmt::format("{}: expected {} argument(s), got {}", fn, min_args,
                                         args.size())
                           : fmt::format("{}: expected at least {} argument(s), got {}", fn,
                                         min_args, args.size()));
    }
    auto f = file_class.unwrap(args[0]);
    if (!f) {
      throw BasicError(fmt::format("{}: expected a file from wwiv.io.file.open, got {}", fn,
                                   args[0].a_type_name()));
    }
    return f;
  };

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.io.file"); });

  // OPEN_OPTIONS(location): location is "GFILES" or "MENUS".
  m.native_function("OPEN_OPTIONS", [&options_class](std::string location) {
    return options_class.wrap(FileOptions{to_file_dir(location)});
  });

  // OPEN(options, name, mode): mode "W" truncates the file and opens it for
  // WRITE and WRITELINE (gfiles only); anything else opens it for reading.
  // Only the file name part of name is used. A file that can't be opened
  // reads as empty, as in the legacy engine.
  m.native_function("OPEN", [b, &options_class, &file_class](Value options, std::string name,
                                                             std::string mode) {
    const auto opts = options_class.unwrap(options);
    if (!opts) {
      throw BasicError(fmt::format("OPEN: expected options from wwiv.io.file.open_options, got {}",
                                   options.a_type_name()));
    }
    const auto filename = std::filesystem::path(name).filename();
    if (filename.empty() || filename == "." || filename == "..") {
      throw BasicError(fmt::format("OPEN: '{}' is not a file name", name));
    }
    const auto writable = iequals(mode, "W");
    if (writable && opts->dir == file_dir_t::menus) {
      throw BasicError("OPEN: files in MENUS can only be opened for reading");
    }
    if (!b.ctx) {
      throw BasicError("OPEN: no session to find the BBS's directories");
    }
    const auto& dirs = b.ctx->session_context().dirs();
    const auto dir =
        opts->dir == file_dir_t::menus ? dirs.current_menu_directory() : dirs.gfiles_directory();
    if (dir.empty()) {
      throw BasicError(opts->dir == file_dir_t::menus ? "OPEN: there is no current menu directory"
                                                      : "OPEN: there is no gfiles directory");
    }
    auto f = std::make_shared<ScriptFile>();
    f->path = FilePath(dir, filename);
    f->writable = writable;
    f->file = std::make_unique<TextFile>(f->path, writable ? "wt" : "rt");
    return file_class.wrap(f);
  });

  m.native_functionl("READINTOSTRING", [file_arg](const Args& args) {
    auto f = file_arg("READINTOSTRING", args, 1, 1);
    if (f->writable || !f->is_open()) {
      return Value(std::string());
    }
    return Value(f->file->ReadFileIntoString());
  });

  // READLINES(file, list) appends the file's lines to the list.
  m.native_functionl("READLINES", [file_arg](const Args& args) {
    auto f = file_arg("READLINES", args, 2, 2);
    if (!args[1].is_list()) {
      throw BasicError(
          fmt::format("READLINES: expected a LIST to read into, got {}", args[1].a_type_name()));
    }
    if (f->writable || !f->is_open()) {
      return Value();
    }
    auto& items = args[1].list("READLINES")->items;
    for (const auto& line : f->file->ReadFileIntoVector()) {
      items.emplace_back(line);
    }
    return Value();
  });

  // LAST_MODIFIED(file): when the file was last written, as a time_t.
  m.native_functionl("LAST_MODIFIED", [file_arg](const Args& args) {
    auto f = file_arg("LAST_MODIFIED", args, 1, 1);
    if (!File::Exists(f->path)) {
      return Value(0);
    }
    return wwivbasic::to_value(File::last_write_time(f->path));
  });

  // New in the new engine: write to a file opened with mode "W".
  // WRITE(file, text...) writes each argument as is.
  m.native_functionl("WRITE", [file_arg](const Args& args) {
    auto f = file_arg("WRITE", args, 1, 0);
    write_each("WRITE", *f, args, false);
    return Value();
  });
  // WRITELINE(file, text...) writes each argument followed by a newline.
  m.native_functionl("WRITELINE", [file_arg](const Args& args) {
    auto f = file_arg("WRITELINE", args, 1, 0);
    write_each("WRITELINE", *f, args, true);
    return Value();
  });
}

} // namespace wwiv::bbs::basic
