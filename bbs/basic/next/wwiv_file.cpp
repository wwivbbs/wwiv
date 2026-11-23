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
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <memory>
#include <system_error>
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

// What OPEN returns. The file stays open until CLOSE or the end of the script.
struct ScriptFile {
  std::filesystem::path path;
  std::unique_ptr<TextFile> file;
  bool writable{false};
  bool closed{false};

  [[nodiscard]] bool is_open() const { return file && file->IsOpen(); }
};

// The files a script has opened, to stop it opening one twice when either
// handle writes: TextFile locks files (flock on Unix, sharing modes on
// Windows), so on Unix the second OPEN would wait forever.
using OpenFiles = std::vector<std::weak_ptr<ScriptFile>>;

// Spellings of the same path compare equal (the file may not exist yet).
std::filesystem::path open_key(const std::filesystem::path& p) {
  std::error_code ec;
  auto key = std::filesystem::weakly_canonical(p, ec);
  if (ec) {
    key = p.lexically_normal();
  }
#ifdef _WIN32
  return ToStringLowerCase(key.string());
#else
  return key;
#endif
}

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

// Names Windows treats as devices, whatever the extension ("NUL.txt" too).
bool is_device_name(const std::filesystem::path& filename) {
  auto stem = ToStringUpperCase(filename.string());
  stem = stem.substr(0, stem.find('.'));
  static const char* const kDevices[] = {"CON", "PRN", "AUX", "NUL", "CONIN$", "CONOUT$", "CLOCK$"};
  for (const auto* d : kDevices) {
    if (stem == d) {
      return true;
    }
  }
  // COM1-COM9 and LPT1-LPT9.
  return stem.size() == 4 && (starts_with(stem, "COM") || starts_with(stem, "LPT")) &&
         stem[3] >= '1' && stem[3] <= '9';
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
  // Flush each call, so the text is on disk if the script opens the file
  // again (there's no CLOSE), and so a failed write is reported here.
  auto* fp = f.file->GetFILE();
  if (std::fflush(fp) != 0 || std::ferror(fp)) {
    throw BasicError(fmt::format("{}: error writing to '{}'", fn, f.path.filename().string()));
  }
}

// The file a read function reads; it must have been opened for reading.
void check_readable(const char* fn, const ScriptFile& f) {
  if (f.writable) {
    throw BasicError(
        fmt::format("{}: the file was opened for writing (mode \"W\"), not reading", fn));
  }
}

} // namespace

// The wwiv.io.file module (bbs/basic/wwiv_file.cpp), registered only when the
// sysop enables it. Files are read from (and written to) the gfiles directory,
// or read from the current menu set's gfiles directory.
void RegisterWwivIoFile(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.io.file");
  auto& options_class = m.class_<FileOptions>("file_options");
  auto& file_class = m.class_<ScriptFile>("file");
  auto open_files = std::make_shared<OpenFiles>();

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
    if (f->closed) {
      throw BasicError(fmt::format("{}: '{}' was closed", fn, f->path.filename().string()));
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
  m.native_function("OPEN", [b, &options_class, &file_class, open_files](
                                Value options, std::string name, std::string mode) {
    const auto opts = options_class.unwrap(options);
    if (!opts) {
      throw BasicError(fmt::format("OPEN: expected options from wwiv.io.file.open_options, got {}",
                                   options.a_type_name()));
    }
    // Check the name as given for ':': on Windows, path("a:b").filename() is
    // "b" ("a:" is a drive), and "b:c" could name an NTFS stream.
    const auto filename = std::filesystem::path(name).filename();
    if (name.find(':') != std::string::npos || filename.empty() || filename == "." ||
        filename == ".." || is_device_name(filename)) {
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
    // Menu text files are in the menu set's gfiles directory, where the rest
    // of the BBS (printfile) looks for them.
    if (opts->dir == file_dir_t::menus && dirs.current_menu_directory().empty()) {
      throw BasicError("OPEN: there is no current menu directory");
    }
    const auto dir = opts->dir == file_dir_t::menus ? dirs.current_menu_gfiles_directory()
                                                    : dirs.gfiles_directory();
    if (dir.empty()) {
      throw BasicError(opts->dir == file_dir_t::menus ? "OPEN: there is no current menu directory"
                                                      : "OPEN: there is no gfiles directory");
    }
    const auto path = FilePath(dir, filename);
    const auto key = open_key(path);
    std::erase_if(*open_files, [](const auto& w) { return w.expired(); });
    for (const auto& w : *open_files) {
      const auto other = w.lock();
      if (other && !other->closed && (writable || other->writable) &&
          open_key(other->path) == key) {
        throw BasicError(fmt::format(
            "OPEN: '{}' is already open{} in this script; CLOSE it first", filename.string(),
            other->writable ? " for writing" : ""));
      }
    }
    auto f = std::make_shared<ScriptFile>();
    f->path = path;
    f->writable = writable;
    f->file = std::make_unique<TextFile>(f->path, writable ? "wt" : "rt");
    open_files->push_back(f);
    return file_class.wrap(f);
  });

  m.native_functionl("READINTOSTRING", [file_arg](const Args& args) {
    auto f = file_arg("READINTOSTRING", args, 1, 1);
    check_readable("READINTOSTRING", *f);
    if (!f->is_open()) {
      return Value(std::string());
    }
    // ReadFileIntoString reads from the start, however much was read before.
    return Value(f->file->ReadFileIntoString());
  });

  // READLINES(file, list) appends the file's lines to the list.
  m.native_functionl("READLINES", [file_arg](const Args& args) {
    auto f = file_arg("READLINES", args, 2, 2);
    if (!args[1].is_list()) {
      throw BasicError(
          fmt::format("READLINES: expected a LIST to read into, got {}", args[1].a_type_name()));
    }
    check_readable("READLINES", *f);
    if (!f->is_open()) {
      return Value();
    }
    // Read from the start, even after an earlier READLINES or READINTOSTRING
    // (the legacy engine read nothing the second time).
    std::rewind(f->file->GetFILE());
    auto& items = args[1].list("READLINES")->items;
    for (const auto& line : f->file->ReadFileIntoVector()) {
      items.emplace_back(line);
    }
    return Value();
  });

  // LAST_MODIFIED(file): when the file was last written, as a time_t.
  m.native_functionl("LAST_MODIFIED", [file_arg](const Args& args) {
    auto f = file_arg("LAST_MODIFIED", args, 1, 1);
    // 0 if the file is missing, including if it goes away while we look.
    try {
      if (!File::Exists(f->path)) {
        return Value(0);
      }
      return wwivbasic::to_value(File::last_write_time(f->path));
    } catch (const std::exception& e) {
      LOG(INFO) << "LAST_MODIFIED: " << f->path << ": " << e.what();
      return Value(0);
    }
  });

  // New in the new engine: CLOSE(file) closes the file, so the script can
  // open it again (to read back what it wrote, say). Closing a closed file
  // does nothing; any other use of a closed file is an error.
  m.native_functionl("CLOSE", [&file_class](const Args& args) {
    if (args.size() != 1) {
      throw BasicError(fmt::format("CLOSE: expected 1 argument(s), got {}", args.size()));
    }
    const auto f = file_class.unwrap(args[0]);
    if (!f) {
      throw BasicError(fmt::format("CLOSE: expected a file from wwiv.io.file.open, got {}",
                                   args[0].a_type_name()));
    }
    if (!f->closed) {
      f->closed = true;
      if (f->file) {
        f->file->Close();
      }
    }
    return Value();
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
