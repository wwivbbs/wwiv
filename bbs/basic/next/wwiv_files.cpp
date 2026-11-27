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
#include "bbs/basic/file_base.h"
#include "bbs/basic/next/wwiv_bindings.h"

#include "common/context.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/files/dirs.h"
#include "sdk/files/file_record.h"
#include "sdk/files/files.h"
#include "sdk/vardec.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <cstring>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

using namespace wwiv::strings;

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;

namespace {

// The longest file name or mask: an 8.3 name, as the BBS's file mask prompt
// takes.
constexpr size_t kMaxFileName = 12;

// When a file was last changed, to tell whether a file list read earlier is
// still current.
struct FileStamp {
  std::uintmax_t size{0};
  std::filesystem::file_time_type time{};
  bool exists{false};

  static FileStamp of(const std::filesystem::path& p) {
    std::error_code ec;
    FileStamp s;
    s.size = std::filesystem::file_size(p, ec);
    if (ec) {
      return {};
    }
    s.time = std::filesystem::last_write_time(p, ec);
    s.exists = !ec;
    return s;
  }

  bool operator==(const FileStamp&) const = default;
};

// Keeps the file lists of the last few areas read, so that a script going
// through an area file by file (FILE(n), FIND, EXTENDED_DESCRIPTION) doesn't
// read the whole list each time. A list is read again once its files change
// on disk, so scripts still see new uploads, from this node or another.
class AreaCache {
public:
  explicit AreaCache(FileBase* files) : files_(files) {}

  [[nodiscard]] FileBase* files() const { return files_; }

  // Directory dirnum's file list, or nullptr if it has none yet. Valid until
  // the next call.
  sdk::files::FileArea* open(int dirnum) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->dirnum != dirnum) {
        continue;
      }
      if (it->dir_stamp == FileStamp::of(it->area->path()) &&
          it->ext_stamp == FileStamp::of(it->area->ext_path())) {
        // Most recently used first.
        entries_.splice(entries_.begin(), entries_, it);
        return entries_.front().area.get();
      }
      entries_.erase(it);
      break;
    }
    auto area = files_->open(dirnum);
    if (!area) {
      return nullptr;
    }
    // Read the stamps after opening, as path() and ext_path() come from the
    // area; a change between the two reads is picked up on the next call.
    Entry e{dirnum, FileStamp::of(area->path()), FileStamp::of(area->ext_path()), std::move(area)};
    entries_.push_front(std::move(e));
    if (entries_.size() > kMaxAreas) {
      entries_.pop_back();
    }
    return entries_.front().area.get();
  }

private:
  // A large area's list takes a few hundred kilobytes, so only keep a few.
  static constexpr size_t kMaxAreas = 4;

  struct Entry {
    int dirnum;
    FileStamp dir_stamp;
    FileStamp ext_stamp;
    std::unique_ptr<sdk::files::FileArea> area;
  };

  FileBase* files_;
  std::list<Entry> entries_;
};

// A file area the user can list (one of FileBase::user_dirs()).
struct ScriptFileArea {
  std::shared_ptr<AreaCache> cache;
  int dirnum;
  std::string key;
  // The directory's file name (as in dirs()) when the area was chosen, to
  // tell if dirnum has since come to mean another directory.
  std::string filename;

  [[nodiscard]] FileBase* files() const { return cache->files(); }

  // The directory, if it's still the one the area was chosen as and the user
  // can still list it. The directory list can change while a script holds an
  // area, so that dirnum is another directory, perhaps one the user can't
  // list.
  [[nodiscard]] const sdk::files::directory_t& dir() const {
    const auto user_dirs = files()->user_dirs();
    const auto& dirs = files()->dirs();
    if (dirnum >= 0 && dirnum < dirs.size() && dirs.dir(dirnum).filename == filename) {
      for (const auto& u : user_dirs) {
        if (u.dirnum == dirnum) {
          return dirs.dir(dirnum);
        }
      }
    }
    throw BasicError(fmt::format("file area '{}' no longer exists", key));
  }

  // Stops the script if the directory no longer exists.
  void check() const { static_cast<void>(dir()); }

  // The area's files, current as of now so that they include new uploads,
  // or nullptr if it has none yet. Valid until the next open().
  [[nodiscard]] sdk::files::FileArea* open() const { return cache->open(dirnum); }
};

// A file record as scripts see it.
struct ScriptFile {
  std::shared_ptr<AreaCache> cache;
  int dirnum{-1};
  // Its position in its area, from 1, when it was read.
  int number{0};
  // Aligned ("FOO     .ZIP"), as the BBS looks up its extended description
  // and its file on disk.
  std::string aligned_name;
  // Exactly as its record has it, to find it again: an old record's name may
  // be in lower case, which aligned_name isn't.
  std::string stored_name;
  std::string name;
  std::string description;
  std::string uploaded_by;
  daten_t date{0};
  uint32_t size{0};
  int downloads{0};
  bool has_extended{false};
  // The area is on a CD-ROM, so the BBS doesn't check that its files exist.
  bool cdrom{false};
};

ScriptFile to_script_file(const ScriptFileArea& a, const sdk::files::directory_t& dir, int number,
                          const sdk::files::FileRecord& f) {
  ScriptFile s;
  s.cache = a.cache;
  s.dirnum = a.dirnum;
  s.number = number;
  s.aligned_name = f.aligned_filename();
  s.stored_name.assign(f.u().filename, strnlen(f.u().filename, sizeof(f.u().filename)));
  s.name = f.unaligned_filename();
  s.description = f.description();
  s.uploaded_by = f.uploaded_by();
  s.date = f.u().daten;
  s.size = f.numbytes();
  s.downloads = f.u().numdloads;
  s.has_extended = f.has_extended_description();
  s.cdrom = (dir.mask & mask_cdrom) != 0;
  return s;
}

// The BBS's file mask (file_mask() in bbs/xfer.cpp) for mask: "*.*" if it's
// empty, with ".*" added if it has no extension, aligned.
std::string to_aligned_mask(const std::string& fn, std::string mask) {
  mask = StringTrim(mask);
  if (mask.size() > kMaxFileName) {
    throw BasicError(
        fmt::format("{}: the file mask '{}' is longer than {} characters", fn, mask, kMaxFileName));
  }
  if (mask.empty()) {
    mask = "*.*";
  }
  if (!mask.contains('.')) {
    mask += ".*";
  }
  return sdk::files::align(mask);
}

// The files in area a matching the aligned mask, in the order the BBS lists
// them. Stops early, with the files read so far, once stopped() is true (the
// caller hung up); the interpreter then stops the script.
std::vector<ScriptFile> read_files(const ScriptFileArea& a, const std::string& aligned_mask,
                                   const std::function<bool()>& stopped) {
  std::vector<ScriptFile> out;
  const auto& dir = a.dir();
  auto* area = a.open();
  if (!area) {
    return out;
  }
  const auto count = area->number_of_files();
  for (auto i = 1; i <= count && !stopped(); i++) {
    const auto f = area->ReadFile(i);
    if (sdk::files::aligned_wildcard_match(aligned_mask, f.aligned_filename())) {
      out.push_back(to_script_file(a, dir, i, f));
    }
  }
  return out;
}

ScriptFile read_file(const ScriptFileArea& a, int number) {
  const auto& dir = a.dir();
  auto* area = a.open();
  const auto count = area ? area->number_of_files() : 0;
  if (!area || number < 1 || number > count) {
    throw BasicError(
        fmt::format("FILE: there is no file {} in '{}' (it has {})", number, dir.name, count));
  }
  return to_script_file(a, dir, number, area->ReadFile(number));
}

// The number of the file whose name in its record is stored_name in area a,
// or 0.
int find_stored(const ScriptFileArea& a, const std::string& stored_name) {
  a.check();
  auto* area = a.open();
  if (!area) {
    return 0;
  }
  const auto pos = area->FindFile(stored_name);
  // Position 0 is the area's header, not a file.
  return pos.value_or(0);
}

// The number of the file named name (case doesn't matter) in area a, or 0.
int find_file(const ScriptFileArea& a, const std::string& name) {
  const auto n = StringTrim(name);
  if (n.empty() || n.size() > kMaxFileName || n.contains('?') || n.contains('*')) {
    return 0;
  }
  return find_stored(a, sdk::files::align(n));
}

// The lines of a file's extended description, or none if it has none.
std::vector<std::string> extended_description(const ScriptFile& f) {
  if (!f.has_extended || !f.cache) {
    return {};
  }
  auto* area = f.cache->open(f.dirnum);
  if (!area) {
    return {};
  }
  const auto ext = area->ext_desc();
  if (!ext) {
    return {};
  }
  return ext.value()->ReadExtendedAsLines(f.aligned_name).value_or(std::vector<std::string>{});
}

// As the BBS's file list (printinfo in bbs/xfer.cpp): the file is on disk,
// or is in a CD-ROM area, whose files the BBS doesn't check.
bool available(const ScriptFile& f) {
  if (f.cdrom) {
    return true;
  }
  if (!f.cache) {
    return false;
  }
  const auto dir = f.cache->files()->path(f.dirnum);
  return !dir.empty() &&
         core::File::Exists(sdk::files::FilePath(dir, sdk::files::FileName(f.aligned_name)));
}

} // namespace

// The wwiv.files module (new engine only): read-only access to the file
// areas the user can list, as the BBS's file list shows them.
void RegisterWwivFiles(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  if (!b.files) {
    return;
  }
  auto* files = b.files;
  auto cache = std::make_shared<AreaCache>(files);
  // True once the caller hangs up.
  std::function<bool()> hung_up = [ctx = b.ctx]() {
    return ctx && ctx->session_context().hangup();
  };
  auto& m = interp.add_module("wwiv.files");
  auto& file_class = m.class_<ScriptFile>("file");
  auto& area_class = m.class_<ScriptFileArea>("file_area");

  file_class.property("number", [](const ScriptFile& f) { return f.number; })
      .property("name", [](const ScriptFile& f) { return f.name; })
      .property("description", [](const ScriptFile& f) { return f.description; })
      .property("uploaded_by", [](const ScriptFile& f) { return f.uploaded_by; })
      .property("date", [](const ScriptFile& f) { return f.date; })
      .property("size", [](const ScriptFile& f) { return f.size; })
      .property("downloads", [](const ScriptFile& f) { return f.downloads; })
      .property("available", [](const ScriptFile& f) { return available(f); })
      // EXTENDED_DESCRIPTION() is the file's extended description as a list
      // of lines, empty if it has none.
      .method("extended_description", [](const ScriptFile& f) { return extended_description(f); });

  area_class.property("key", [](const ScriptFileArea& a) { return a.key; })
      .property("name", [](const ScriptFileArea& a) { return a.dir().name; })
      .property("count",
                [](const ScriptFileArea& a) {
                  a.check();
                  auto* area = a.open();
                  return area ? area->number_of_files() : 0;
                })
      // FILES([mask]) lists the area's files, or those matching mask.
      .methodl("files",
               [&file_class, hung_up](const ScriptFileArea& a, const std::vector<Value>& args) {
                 if (args.size() > 1) {
                   throw wwivbasic::ArgumentError(
                       fmt::format("expected 0 or 1 arguments, got {}", args.size()));
                 }
                 const auto mask =
                     to_aligned_mask("FILES", args.empty() ? "" : args.at(0).toString());
                 std::vector<Value> items;
                 for (auto& f : read_files(a, mask, hung_up)) {
                   items.push_back(file_class.wrap(std::move(f)));
                 }
                 return Value::make_list(std::move(items));
               })
      // FILE(n) is file n (from 1 to COUNT).
      .method("file", [&file_class](const ScriptFileArea& a,
                                    int number) { return file_class.wrap(read_file(a, number)); })
      // FIND(name) is the number of the file named name, or 0. name may be a
      // file from this area, to find it again after files were added.
      .method("find", [&file_class](const ScriptFileArea& a, Value v) {
        if (const auto f = file_class.unwrap(v)) {
          // By the name as stored, which align() of the name scripts see
          // needn't give back (an old record stored in lower case).
          return find_stored(a, f->stored_name);
        }
        if (v.is_list() || v.is_object()) {
          throw BasicError("file_area.find: expected a file name or a file");
        }
        return find_file(a, v.toString());
      });

  auto make_area = [files, cache, &area_class](const FileBase::UserDir& u) {
    return area_class.wrap(
        ScriptFileArea{cache, u.dirnum, u.key, files->dirs().dir(u.dirnum).filename});
  };

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.files"); });

  // AREAS() lists the file areas the user can list, as the BBS lists them.
  m.native_function("AREAS", [files, make_area]() {
    std::vector<Value> items;
    for (const auto& u : files->user_dirs()) {
      items.push_back(make_area(u));
    }
    return Value::make_list(std::move(items));
  });

  // AREA(key) is the area the user chooses with key.
  m.native_function("AREA", [files, make_area](std::string key) {
    for (const auto& u : files->user_dirs()) {
      if (iequals(u.key, key)) {
        return make_area(u);
      }
    }
    throw BasicError(fmt::format("AREA: there is no file area '{}'", key));
  });

  // CURRENT_AREA() is the area the user is in.
  m.native_function("CURRENT_AREA", [files, make_area]() {
    const auto u = files->current_user_dir();
    if (!u) {
      throw BasicError("CURRENT_AREA: the user isn't in a file area");
    }
    return make_area(*u);
  });
}

} // namespace wwiv::bbs::basic
