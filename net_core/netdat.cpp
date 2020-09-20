/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2020, WWIV Software Services                */
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
#include <utility>

#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "net_core/netdat.h"
#include "core/numbers.h"
#include "sdk/net/net.h"

using namespace wwiv::core;

namespace wwiv::net {

// static
char NetDat::NetDatMsgType(netdat_msgtype_t t) {
  switch (t) {
  case netdat_msgtype_t::banner:
    return '\xFE';
  case netdat_msgtype_t::post:
    return '+';
  case netdat_msgtype_t::normal:
    return '-';
  case netdat_msgtype_t::error:
    return '!';
  default:
    LOG(FATAL) << "Missing case for " << static_cast<int>(t);
    return 0;
  }
}

NetDat::NetDat(std::filesystem::path gfiles, const net_networks_rec& net, char net_cmd, Clock& clock)
  : gfiles_(std::move(gfiles)), net_(net), net_cmd_(net_cmd), clock_(clock) {
  rollover();
  file_ = open("ad");
  if (file_->position() == 0) {
    // Write Header.
    const auto ndate = clock_.Now().to_string("%m/%d/%y");
    const auto header = fmt::format("\r\n={}\r\n", ndate);
    file_->Write(header);
  }
}

NetDat::~NetDat() {
  WriteStats();
  WriteLines();
  file_->Close();
}

void NetDat::WriteHeader() {
  const auto now_hms = clock_.Now().to_string("%H:%M:%S");
  const auto header = fmt::format("\xFE {} - net{} network{} [{}] ({}) [{}]",
    now_hms,
    wwiv_network_compatible_version(),
    net_cmd_,
    full_version(),
    wwiv_compile_datetime(), net_.name);
  file_->WriteLine("");
  WriteLine(header);
  VLOG(2) << header;
}

void NetDat::WriteLine(const std::string& s) {
  const auto now_hms = clock_.Now().to_string("%H:%M:%S");
  const auto prefix = fmt::format("\xFE {} - ", now_hms);

  file_->WriteLine(strings::StrCat(prefix, s));
}

bool NetDat::WriteStats() {
  const auto now_hms = clock_.Now().to_string("%H:%M:%S");
  if (stats_.empty()) {
    return false;
  }

  WriteHeader();

  auto total_files = 0;
  auto total_bytes = 0;
  for (const auto& [_, s] : stats_) {
    total_files += s.files;
    total_bytes += s.bytes;
  }

  const auto total = fmt::format("  -  Total :  {} msgs, {}k", total_files, bytes_to_k(total_bytes));
  VLOG(2) << total;
  WriteLine(total);

  if (stl::contains(stats_, 65535)) {
    const auto& s = stats_.at(65535);
    const auto line = fmt::format("  -   Dead :  {} msgs, {}k", s.files, bytes_to_k(s.bytes));
    VLOG(2) << line;
    WriteLine(line);
  }

  for (const auto& [n, s] : stats_) {
    if (n == 65535) {
      // Skip dead.net
      continue;
    }
    const auto line = fmt::format("  - {:<6} :  {} msgs, {}k", strings::StrCat("@", n), s.files,
                                  bytes_to_k(s.bytes));
    VLOG(2) << line;
    WriteLine(line);
  }
  stats_.clear();
  return true;
}

bool NetDat::WriteLines() {
  if (lines_.empty()) {
    return false;
  }

  WriteHeader();
  for (const auto& l : lines_) {
    VLOG(2) << l;
    file_->WriteLine(l);
  }
  return true;
}

void NetDat::add_file_bytes(int node, int bytes) {
  ++stats_[node].files;
  stats_[node].bytes += bytes;
}

void NetDat::add_message(netdat_msgtype_t t, const std::string& msg) {
  const auto start =
      t == netdat_msgtype_t::banner ? "\xFE " : strings::StrCat(" ", NetDatMsgType(t), " ");
  lines_.emplace_back(strings::StrCat(start, msg));
}

bool NetDat::empty() const {
  return stats_.empty();
}

std::string NetDat::ToDebugString() const {
  std::ostringstream ss;

  if (!stats_.empty()) {
    ss << "Stats: " << std::endl;
    for (const auto& [k, v] : stats_) {
      ss << "k: " << k << ":  files: " << v.files << "; bytes: " << v.bytes << std::endl;
    }
  }

  if (!lines_.empty()) {
    ss << "Lines: " << std::endl;
    for (const auto& l : lines_) {
      ss << l << std::endl;
    }
  }

  return ss.str();
}


bool NetDat::rollover() {

  auto r = open("rd");
  auto lines = r->ReadFileIntoVector(2);
  if (lines.size() != 2 || lines.at(1).size() < 9) {
    return false;
  }
  const auto& l = lines.at(1);
  // We have a proper line.
  const auto ddate = l.substr(1);
  const auto ndate = clock_.Now().to_string("%m/%d/%y");
  if (ddate == ndate) {
    return false;
  }

  // Finally, we need to rollover.
  r->Close();

  const auto n0 = FilePath(gfiles_, "netdat0.log");
  const auto n1 = FilePath(gfiles_, "netdat1.log");
  const auto n2 = FilePath(gfiles_, "netdat2.log");
  File::Remove(n2);
  File::Move(n1, n2);
  File::Move(n0, n1);

  return true;
}

std::unique_ptr<TextFile> NetDat::open(const std::string& mode) const {
 return std::make_unique<TextFile>(FilePath(gfiles_, "netdat0.log"), mode);

}
}

