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
// msgcompare: reads every post in a BBS's message areas with both the BBS's
// message reader (readfile, parse_type2_message_text and
// split_wwiv_message_lines in bbs/) and sdk/msgapi, and reports where the
// text the reader would show differs.
//
// The BBS's readfile opens message files read-write (and can extend them),
// so BBSDIR is never read in place: config.json, the data directory's files
// and the subs' message files are copied into a work directory first, and
// only the copy is read.
//
//   msgcompare [--sub=FILENAME] [--examples=N] [--work_dir=DIR] [--keep_work_dir] BBSDIR

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/message_file.h"
#include "bbs/read_message.h"
#include "common/null_remote_io.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "local_io/null_local_io.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

namespace {

struct Options {
  fs::path bbsdir;
  fs::path work_dir;
  std::string sub;
  int examples{3};
  bool keep_work_dir{false};
};

void usage() {
  std::cerr << "usage: msgcompare [--sub=FILENAME] [--examples=N] [--work_dir=DIR] "
               "[--keep_work_dir] BBSDIR\n"
               "\n"
               "Reads every post in BBSDIR's message areas with both the BBS's reader and\n"
               "sdk/msgapi, and reports where the text the reader shows differs.\n"
               "BBSDIR is copied to the work directory (a new temp directory unless\n"
               "--work_dir names one, which must not exist) and only the copy is read.\n";
}

std::optional<Options> parse_args(int argc, char* argv[]) {
  Options o;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    if (a.starts_with("--sub=")) {
      o.sub = a.substr(6);
    } else if (a.starts_with("--examples=")) {
      o.examples = to_number<int>(a.substr(11));
    } else if (a.starts_with("--work_dir=")) {
      o.work_dir = a.substr(11);
    } else if (a == "--keep_work_dir") {
      o.keep_work_dir = true;
    } else if (a.starts_with("-")) {
      return std::nullopt;
    } else if (o.bbsdir.empty()) {
      o.bbsdir = a;
    } else {
      return std::nullopt;
    }
  }
  if (o.bbsdir.empty()) {
    return std::nullopt;
  }
  return o;
}

// A directory named in config.json: absolute, or relative to the BBS's root.
fs::path config_dir(const fs::path& root, const std::string& d) {
  const fs::path p(d);
  return p.is_absolute() ? p : root / p;
}

// True if p is dir or inside it. Compares whole path elements, so "/a/bc"
// isn't under "/a/b".
bool is_under(const fs::path& p, const fs::path& dir) {
  const auto cp = fs::weakly_canonical(p).lexically_normal();
  const auto cd = fs::weakly_canonical(dir).lexically_normal();
  const auto [d, _] = std::mismatch(cd.begin(), cd.end(), cp.begin(), cp.end());
  // A trailing separator leaves an empty last element in dir.
  return d == cd.end() || (d->empty() && std::next(d) == cd.end());
}

// Removes the work directory when run() returns or throws, unless it's kept.
class WorkDirCleanup {
public:
  WorkDirCleanup(fs::path dir, bool keep) : dir_(std::move(dir)), keep_(keep) {}
  WorkDirCleanup(const WorkDirCleanup&) = delete;
  WorkDirCleanup& operator=(const WorkDirCleanup&) = delete;
  ~WorkDirCleanup() {
    if (keep_) {
      std::cerr << "Kept the work directory " << dir_ << "\n";
      return;
    }
    std::error_code ec;
    // Leave the directory first; Windows can't remove the current directory.
    fs::current_path(fs::temp_directory_path(ec), ec);
    fs::remove_all(dir_, ec);
  }

private:
  fs::path dir_;
  bool keep_;
};

// Copies the regular files directly in `from` (not subdirectories) that
// `want` accepts into `to`.
template <class F> int copy_files(const fs::path& from, const fs::path& to, F want) {
  int n = 0;
  for (const auto& e : fs::directory_iterator(from)) {
    if (e.is_regular_file() && want(e.path().filename().string())) {
      fs::copy_file(e.path(), to / e.path().filename(), fs::copy_options::overwrite_existing);
      n++;
    }
  }
  return n;
}

// Shows control characters as ^X so differences are visible.
std::string escape(const std::string& s) {
  std::string out;
  for (const auto c : s) {
    const auto u = static_cast<unsigned char>(c);
    if (u < 32) {
      out += fmt::format("^{}", static_cast<char>(u + '@'));
    } else if (u == 127) {
      out += "^?";
    } else {
      out += c;
    }
  }
  return out;
}

std::vector<std::string> shown_lines(const std::string& text) {
  return split_wwiv_message_lines(text, false, 0, std::numeric_limits<int>::max());
}

// The SDK reads the RE: and BY: lines at the top into the header, by design.
void drop_re_by(std::vector<std::string>& lines) {
  while (!lines.empty() && (lines.front().starts_with("RE:") || lines.front().starts_with("BY:"))) {
    lines.erase(lines.begin());
  }
}

std::vector<std::string> rstripped(std::vector<std::string> v) {
  for (auto& l : v) {
    StringTrimEnd(&l);
  }
  return v;
}

std::vector<std::string> without_blank_ends(std::vector<std::string> v) {
  while (!v.empty() && StringTrim(v.back()).empty()) {
    v.pop_back();
  }
  while (!v.empty() && StringTrim(v.front()).empty()) {
    v.erase(v.begin());
  }
  return v;
}

// A rough kind for a text difference, to group them in the report.
std::string classify(const std::vector<std::string>& bbs, const std::vector<std::string>& sdk) {
  if (rstripped(bbs) == rstripped(sdk)) {
    return "text: trailing spaces";
  }
  if (without_blank_ends(bbs) == without_blank_ends(sdk)) {
    return "text: leading or trailing blank lines";
  }
  if (without_blank_ends(rstripped(bbs)) == without_blank_ends(rstripped(sdk))) {
    return "text: blank lines at the ends and trailing spaces";
  }
  return "text: other";
}

struct Example {
  std::string where;
  std::vector<std::string> detail;
};

struct Report {
  std::map<std::string, int> counts;
  std::map<std::string, std::vector<Example>> examples;
  int messages{0};

  void add(const std::string& kind, int max_examples, Example e) {
    counts[kind]++;
    auto& v = examples[kind];
    if (std::cmp_less(v.size(), max_examples)) {
      v.push_back(std::move(e));
    }
  }
};

// The start and end of a post's raw text, for examples.
std::vector<std::string> raw_ends(const std::string& raw) {
  constexpr size_t kHead = 160;
  constexpr size_t kTail = 60;
  if (raw.size() <= kHead + kTail) {
    return {fmt::format("  raw: [{}]", escape(raw))};
  }
  return {fmt::format("  raw start: [{}]", escape(raw.substr(0, kHead))),
          fmt::format("  raw end:   [{}]", escape(raw.substr(raw.size() - kTail)))};
}

Example text_example(const std::string& where, const std::vector<std::string>& bbs,
                     const std::vector<std::string>& sdk, const std::string& raw) {
  size_t i = 0;
  while (i < bbs.size() && i < sdk.size() && bbs[i] == sdk[i]) {
    i++;
  }
  Example e{where, {}};
  e.detail.push_back(fmt::format("bbs has {} lines, sdk {}; first difference at line {}",
                                 bbs.size(), sdk.size(), i + 1));
  const auto from = i >= 2 ? i - 2 : 0;
  for (auto j = from; j < i + 3 && (j < bbs.size() || j < sdk.size()); j++) {
    const auto b = j < bbs.size() ? fmt::format("[{}]", escape(bbs[j])) : "(none)";
    const auto s = j < sdk.size() ? fmt::format("[{}]", escape(sdk[j])) : "(none)";
    e.detail.push_back(fmt::format("  {:>4} bbs: {}", j + 1, b));
    if (b != s) {
      e.detail.push_back(fmt::format("       sdk: {}", s));
    }
  }
  for (auto& l : raw_ends(raw)) {
    e.detail.push_back(std::move(l));
  }
  return e;
}

int run(const Options& o) {
  const auto bbsdir = fs::absolute(o.bbsdir);
  if (!fs::exists(bbsdir / "config.json")) {
    std::cerr << "No config.json in " << bbsdir << "\n";
    return 2;
  }

  // The work directory: a copy of what the readers need.
  fs::path work = o.work_dir;
  if (work.empty()) {
    std::random_device rd;
    work = fs::temp_directory_path() / fmt::format("msgcompare-{:08x}{:08x}", rd(), rd());
  }
  work = fs::absolute(work);
  if (fs::exists(work)) {
    std::cerr << "The work directory " << work << " already exists.\n";
    return 2;
  }
  if (is_under(work, bbsdir) || is_under(bbsdir, work)) {
    std::cerr << "The work directory must be outside " << bbsdir << ".\n";
    return 2;
  }
  fs::create_directories(work.parent_path());
  // create_directory fails if another process made it after the check above.
  if (!fs::create_directory(work)) {
    std::cerr << "The work directory " << work << " already exists.\n";
    return 2;
  }
  const WorkDirCleanup cleanup(work, o.keep_work_dir);
  fs::create_directories(work / "data");
  fs::create_directories(work / "msgs");
  for (const auto* d : {"gfiles", "menus", "dloads", "scripts"}) {
    fs::create_directories(work / d);
  }
  fs::copy_file(bbsdir / "config.json", work / "config.json");
  if (fs::exists(bbsdir / "config.dat")) {
    fs::copy_file(bbsdir / "config.dat", work / "config.dat");
  }

  Config config(work);
  if (!config.IsInitialized()) {
    std::cerr << "Unable to load the copy of config.json.\n";
    return 2;
  }
  // The source directories, as config.json names them.
  const auto& raw = config.to_config_t();
  const auto src_data = config_dir(bbsdir, raw.datadir);
  const auto src_msgs = config_dir(bbsdir, raw.msgsdir);
  // Point every directory at the copy.
  config.set_paths_for_test(work / "data", work / "msgs", work / "gfiles", work / "menus",
                            work / "dloads", work / "scripts");
  for (const auto& d : {config.datadir(), config.msgsdir(), config.gfilesdir(), config.menudir(),
                        config.dloadsdir(), config.scriptdir()}) {
    if (!is_under(d, work)) {
      std::cerr << "Refusing to run: " << d << " is outside the work directory.\n";
      return 2;
    }
  }

  std::cerr << "Copying " << src_data << " and " << src_msgs << " to " << work << "\n";
  copy_files(src_data, config.datadir(), [](const std::string&) { return true; });
  {
    Subs subs(config.datadir(), {});
    if (!subs.Load()) {
      std::cerr << "Unable to load the subs.\n";
      return 2;
    }
    copy_files(src_msgs, config.msgsdir(), [&](const std::string& name) {
      if (!ends_with(ToStringLowerCase(name), ".dat")) {
        return false;
      }
      const auto base = name.substr(0, name.size() - 4);
      return o.sub.empty() ? subs.exists(base) : iequals(base, o.sub);
    });
  }
  // Nothing below should touch BBSDIR; make relative paths land in the copy.
  fs::current_path(work);

  // A session for the BBS's reader (readfile uses a()).
  auto* local_io = new wwiv::local::io::NullLocalIO();
  auto* app = CreateSession(local_io);
  app->SetCommForTest(new wwiv::common::NullRemoteIO(local_io));
  app->set_config_for_test(config);
  app->init_status_and_instances_for_test();

  Subs subs(config.datadir(), {});
  if (!subs.Load()) {
    std::cerr << "Unable to load the subs.\n";
    return 2;
  }
  MessageApiOptions api_options;
  api_options.overflow_strategy = OverflowStrategy::delete_none;
  WWIVMessageApi api(api_options, config, {}, new NullLastReadImpl());

  Report report;
  std::cout << fmt::format("{:<20} {:>7} {:>9}\n", "sub", "posts", "differ");
  for (int n = 0; n < subs.size(); n++) {
    const auto& sub = subs.sub(n);
    if (!o.sub.empty() && !iequals(sub.filename, o.sub)) {
      continue;
    }
    if (sub.storage_type != 2 && sub.storage_type != 0) {
      std::cout << fmt::format("{:<20} skipped: storage type {}\n", sub.filename,
                               sub.storage_type);
      continue;
    }
    if (!api.Exist(sub)) {
      std::cout << fmt::format("{:<20} skipped: no message files\n", sub.filename);
      continue;
    }
    auto area = api.Open(sub, n);
    const auto count = area->number_of_messages();
    const auto sub_path = FilePath(config.datadir(), StrCat(sub.filename, ".sub"));
    int differ = 0;
    for (int i = 1; i <= count; i++) {
      report.messages++;
      postrec p{};
      bool read_ok;
      {
        // Closed again before the SDK opens the file: WWIV's File locks it
        // (flock), and a second open in this process would wait forever.
        DataFile<postrec> subfile(sub_path, File::modeBinary | File::modeReadOnly);
        read_ok = subfile && subfile.Read(i, &p);
      }
      if (!read_ok) {
        report.add("unreadable post header", o.examples,
                   {fmt::format("{} #{}", sub.filename, i), {}});
        differ++;
        continue;
      }
      const auto where = fmt::format("{} #{} \"{}\"", sub.filename, i, escape(p.title));
      const auto bbs_raw =
          p.msg.storage_type == 2 ? readfile(&p.msg, sub.filename) : std::nullopt;
      const auto sdk_msg = area->ReadMessage(i);
      if (!bbs_raw || !sdk_msg) {
        if (bbs_raw || sdk_msg) {
          report.add(bbs_raw ? "only the bbs can read it" : "only the sdk can read it",
                     o.examples, {where, {}});
          differ++;
        }
        continue;
      }
      Type2MessageData d{};
      parse_type2_message_text(bbs_raw.value(), d);
      auto bbs_lines = shown_lines(d.message_text);
      drop_re_by(bbs_lines);
      const auto sdk_lines = shown_lines(sdk_msg->text().string());
      bool same = true;
      if (d.from_user_name != sdk_msg->header().from()) {
        same = false;
        report.add("from", o.examples,
                   {where,
                    {fmt::format("  bbs: [{}]", escape(d.from_user_name)),
                     fmt::format("  sdk: [{}]", escape(sdk_msg->header().from()))}});
      }
      if (bbs_lines != sdk_lines) {
        same = false;
        report.add(classify(bbs_lines, sdk_lines), o.examples,
                   text_example(where, bbs_lines, sdk_lines, bbs_raw.value()));
      }
      if (!same) {
        differ++;
      }
    }
    std::cout << fmt::format("{:<20} {:>7} {:>9}\n", sub.filename, count, differ);
  }

  std::cout << fmt::format("\n{} posts read.\n", report.messages);
  if (report.counts.empty()) {
    std::cout << "No differences.\n";
  }
  for (const auto& [kind, n] : report.counts) {
    std::cout << fmt::format("\n== {} ({} posts)\n", kind, n);
    for (const auto& e : report.examples[kind]) {
      std::cout << e.where << "\n";
      for (const auto& l : e.detail) {
        std::cout << l << "\n";
      }
    }
  }

  return report.counts.empty() ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[]) {
  LoggerConfig logger_config{};
  // Never write log files (the BBS's would be in BBSDIR's log directory).
  logger_config.register_file_destinations = false;
  logger_config.log_startup = false;
  Logger::Init(argc, argv, logger_config);

  const auto o = parse_args(argc, argv);
  if (!o) {
    usage();
    return 2;
  }
  try {
    return run(o.value());
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
}
