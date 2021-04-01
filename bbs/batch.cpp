/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
// ReSharper disable CppClangTidyHicppMultiwayPathsCovered
#include "bbs/batch.h"

#include "bbs/bbs.h"
#include "bbs/dsz.h"
#include "bbs/execexternal.h"
#include "bbs/instmsg.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/srsend.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
#include "sdk/files/files.h"
#include <algorithm>
#include <chrono>
#include <iterator>
#include <string>
#include <utility>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// trytoul.cpp
int try_to_ul(const std::string& file_name);

// normupld.cpp
void normalupload(int dn);

//////////////////////////////////////////////////////////////////////////////
// Implementation

// Shows listing of files currently in batch queue(s), both upload and
// download. Shows estimated time, by item, for files in the d/l queue.
static void listbatch() {
  bout.nl();
  if (a()->batch().empty()) {
    return;
  }
  bool abort = false;
  bout << "|#9Files - |#2" << a()->batch().size() << "  ";
  if (a()->batch().numbatchdl()) {
    bout << "|#9Time - |#2" << ctim(a()->batch().dl_time_in_secs());
  }
  bout.nl(2);
  int current_num = 0;
  for (const auto& b : a()->batch().entry) {
    if (abort || a()->sess().hangup()) {
      break;
    }
    std::string buffer;
    ++current_num;
    if (b.sending()) {
      const auto t = ctim(b.time(a()->modem_speed_));
      buffer = fmt::format("{}. (D) {}   {}  {}", current_num, b.aligned_filename(), t,
                           a()->dirs()[b.dir()].name);
    } else {
      buffer = fmt::format("{}. (U) {}             {}", current_num, b.aligned_filename(),
                           a()->dirs()[b.dir()].name);
    }
    bout.bpla(buffer, &abort);
  }
  bout.nl();
}

static void downloaded(const std::string& file_name, long lCharsPerSecond) {

  for (auto it = begin(a()->batch().entry); it != end(a()->batch().entry); ++it) {
    const auto& b = *it;
    if (file_name == b.aligned_filename() && b.sending()) {
      dliscan1(b.dir());
      auto* area = a()->current_file_area();
      auto nRecNum = recno(b.aligned_filename());
      if (nRecNum > 0) {
        auto f = area->ReadFile(nRecNum);
        a()->user()->increment_downloaded();
        a()->user()->set_dk(a()->user()->dk() + bytes_to_k(f.numbytes()));
        ++f.u().numdloads;
        if (area->UpdateFile(f, nRecNum)) {
          area->Save();
        }
        if (lCharsPerSecond) {
          sysoplog() << "Downloaded '" << f << "' (" << lCharsPerSecond << " cps).";
        } else {
          sysoplog() << "Downloaded '" << f << "'.";
        }
        if (a()->config()->sysconfig_flags() & sysconfig_log_dl) {
          if (const auto user = a()->users()->readuser(f.u().ownerusr, UserManager::mask::non_deleted);
              user.has_value() && !user->deleted()) {
            if (date_to_daten(user->firston()) < f.u().daten) {
              const auto user_name_number = a()->user()->name_and_number();
              ssm(f.u().ownerusr) << user_name_number << " downloaded|#1 \"" << f << "\" |#7on "
                                  << fulldate();
            }
          }
        }
      }
      it = a()->batch().delbatch(it);
      return;
    }
  }
  sysoplog() << "!!! Couldn't find \"" << file_name << "\" in DL batch queue.";
}

void didnt_upload(const BatchEntry& b) {
  if (b.sending()) {
    return;
  }

  dliscan1(b.dir());
  auto* area = a()->current_file_area();
  auto nRecNum = recno(b.aligned_filename());
  if (nRecNum <= 0) {
    sysoplog() << fmt::format("!!! Couldn't find \"{}\" in transfer area.", b.aligned_filename());
    return;
  }
  files::FileRecord f{};
  do {
    f = area->ReadFile(nRecNum);
    if (f.numbytes() != 0) {
      nRecNum = nrecno(b.aligned_filename(), nRecNum);
    }
  }
  while (nRecNum != -1 && f.numbytes() != 0);

  if (nRecNum == -1 || f.numbytes() != 0) {
    sysoplog() << fmt::format("!!! Couldn't find \"\" in transfer area.", b.aligned_filename());
    return;
  }
  if (area->DeleteFile(f, nRecNum)) {
    area->Save();
  }
}

static void uploaded(const std::string& file_name, long lCharsPerSecond) {
  for (auto it = begin(a()->batch().entry); it != end(a()->batch().entry); ++it) {
    const auto& b = *it;
    if (file_name == b.aligned_filename() && !b.sending()) {
      dliscan1(b.dir());
      auto* area = a()->current_file_area();
      auto nRecNum = recno(b.aligned_filename());
      if (nRecNum > 0) {
        files::FileRecord f{};
        do {
          f = area->ReadFile(nRecNum);
          if (f.numbytes() != 0) {
            nRecNum = nrecno(b.aligned_filename(), nRecNum);
          }
        }
        while (nRecNum != -1 && f.numbytes() != 0);
        if (nRecNum != -1 && f.numbytes() == 0) {
          const auto source_filename = FilePath(a()->sess().dirs().batch_directory(), file_name);
          const auto dest_filename = FilePath(a()->dirs()[b.dir()].path, file_name);
          if (source_filename != dest_filename && File::Exists(source_filename)) {
            File::Rename(source_filename, dest_filename);
            File::Remove(source_filename);
          }
          File file(dest_filename);
          if (file.Open(File::modeBinary | File::modeReadOnly)) {
            if (!a()->upload_cmd.empty()) {
              file.Close();
              if (!check_ul_event(b.dir(), &f.u())) {
                didnt_upload(b);
              } else {
                file.Open(File::modeBinary | File::modeReadOnly);
              }
            }
            if (file.IsOpen()) {
              f.set_numbytes(file.length());
              file.Close();
              get_file_idz(f, a()->dirs()[b.dir()]);
              a()->user()->increment_uploaded();
              add_to_file_database(f);
              a()->user()->set_uk(a()->user()->uk() + static_cast<int>(bytes_to_k(f.numbytes())));
              a()->status_manager()->Run([](Status& s)
              {
                s.increment_uploads_today();
                s.increment_filechanged(Status::file_change_upload);
              });
              if (area->UpdateFile(f, nRecNum)) {
                area->Save();
              }
              sysoplog() << fmt::format("+ \"{}\" uploaded on {} ({} cps)", f,
                                        a()->dirs()[b.dir()].name, lCharsPerSecond);
              bout << "Uploaded '" << f << "' to " << a()->dirs()[b.dir()].name << " ("
                   << lCharsPerSecond << " cps)" << wwiv::endl;
            }
          }
          it = a()->batch().delbatch(it);
          return;
        }
      }
      it = a()->batch().delbatch(it);
      if (try_to_ul(file_name)) {
        sysoplog() << fmt::sprintf("!!! Couldn't find file \"%s\" in directory.", file_name);
        bout << "Deleting - couldn't find data for file " << file_name << wwiv::endl;
      }
      return;
    }
  }
  if (try_to_ul(file_name)) {
    sysoplog() << fmt::sprintf("!!! Couldn't find \"%s\" in UL batch queue.", file_name);
    bout << "Deleting - don't know what to do with file " << file_name << wwiv::endl;

    File::Remove(FilePath(a()->sess().dirs().batch_directory(), file_name));
  }
}

static void ProcessDSZLogFile(const std::string& path) {
  ProcessDSZLogFile(path, [](dsz_logline_t t, std::string fn, int cps)
  {
    switch (t) {
    case dsz_logline_t::download:
      downloaded(fn, cps);
      break;
    case dsz_logline_t::upload:
      uploaded(fn, cps);
      break;
    case dsz_logline_t::error:
    default:
      sysoplog() << fmt::format("Error transferring \"{}\"", fn);
      break;
    }
  });
}

static int hangup_color(int left) {
  if (left < 3) {
    return 6;
  }
  if (left < 6) {
    return 2;
  }
  return 5;
}

// This function returns one character from either the local keyboard or
// remote com port (if applicable).  Every second of inactivity, a
// beep is sounded.  After 10 seconds of inactivity, the user is hung up.
static void bihangup() {
  bout.dump();
  const auto batch_lastchar = steady_clock::now();
  auto nextbeep = seconds(1);
  bout << "\r\n|#2Automatic disconnect in progress.\r\n";
  bout << "|#2Press 'H' to Hangup, or any other key to return to system.\r\n";

  while (!bin.bkbhit() && !a()->sess().hangup()) {
    const auto dd = steady_clock::now();
    const auto elapsed = dd - batch_lastchar;
    if (elapsed > nextbeep) {
      nextbeep += seconds(1);
      const auto left = 10 - static_cast<int>(duration_cast<seconds>(elapsed).count());
      bout.Color(hangup_color(left));
      bout << "\r" << left;
    }
    if (dd - batch_lastchar > seconds(10)) {
      bout.nl();
      bout << "Thank you for calling.";
      bout.nl();
      bout.remoteIO()->disconnect();
      a()->Hangup();
      return;
    }
    a()->CheckForHangup();
  }
  if (const auto ch = bin.getkey(); ch == 'h' || ch == 'H') {
    a()->Hangup();
  }
}

void zmbatchdl(bool bHangupAfterDl) {
  int cur = 0;

  if (!a()->sess().incom()) {
    return;
  }

  auto message = StrCat("ZModem Download: Files - ", a()->batch().size(),
                        " Time - ", ctim(a()->batch().dl_time_in_secs()));
  if (bHangupAfterDl) {
    message += ", HAD";
  }
  sysoplog() << message;
  bout.nl();
  bout << message;
  bout.nl(2);

  bool bRatioBad = false;
  bool ok = true;
  // TODO(rushfan): Rewrite this to use iterators;
  do {
    a()->tleft(true);
    if (a()->config()->req_ratio() > 0.0001 && a()->user()->ratio() < a()->config()->req_ratio()) {
      bRatioBad = true;
    }
    if (a()->user()->exempt_ratio()) {
      bRatioBad = false;
    }
    if (!a()->batch().entry[cur].sending()) {
      bRatioBad = false;
      ++cur;
    }
    if (nsl() >= a()->batch().entry[cur].time(a()->modem_speed_) && !bRatioBad) {
      const auto dir_num = a()->batch().entry[cur].dir();
      dliscan1(dir_num);
      const int record_number = recno(a()->batch().entry[cur].aligned_filename());
      if (record_number <= 0) {
        a()->batch().delbatch(cur);
      } else {
        bout.localIO()->Puts(StrCat("Files left - ", a()->batch().size(), ", Time left - ",
                                    ctim(a()->batch().dl_time_in_secs()), "\r\n"));
        auto* area = a()->current_file_area();
        auto f = area->ReadFile(record_number);
        auto send_filename = FilePath(a()->dirs()[dir_num].path, f);
        if (a()->dirs()[dir_num].mask & mask_cdrom) {
          auto orig_filename = FilePath(a()->dirs()[dir_num].path, f);
          // update the send filename and copy it from the CD-ROM
          send_filename = FilePath(a()->sess().dirs().temp_directory(), f);
          if (!File::Exists(send_filename)) {
            File::Copy(orig_filename, send_filename);
          }
        }
        write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_NONE);
        const auto send_fn = send_filename.string();
        double percent;
        zmodem_send(send_fn, &ok, &percent);
        if (ok) {
          downloaded(f.aligned_filename(), 0);
        }
      }
    } else {
      a()->batch().delbatch(cur);
    }
  }
  while (ok && !a()->sess().hangup() && ssize(a()->batch().entry) > cur && !bRatioBad);

  if (bRatioBad) {
    bout << "\r\nYour ratio is too low to continue the transfer.\r\n\n\n";
  }
  if (bHangupAfterDl) {
    bihangup();
  }
}

char end_ymodem_batch1() {
  char b[128];

  memset(b, 0, 128);

  bool done = false;
  int nerr = 0;
  bool bAbort = false;
  char ch;
  do {
    send_block(b, 5, true, 0);
    ch = gettimeout(5, &bAbort);
    if (ch == CF || ch == CX) {
      done = true;
    } else {
      ++nerr;
      if (nerr >= 9) {
        done = true;
      }
    }
  }
  while (!done && !a()->sess().hangup() && !bAbort);
  if (ch == CF) {
    return CF;
  }
  if (ch == CX) {
    return CX;
  }
  return CU;
}

static void end_ymodem_batch() {
  bool abort = false;
  int oldx = bout.localIO()->WhereX();
  int oldy = bout.localIO()->WhereY();
  bool ucrc = false;
  if (!okstart(&ucrc, &abort)) {
    abort = true;
  }
  if (!abort && !a()->sess().hangup()) {
    const char ch = end_ymodem_batch1();
    if (ch == CX) {
      abort = true;
    }
    if (ch == CU) {
      const auto fn = FilePath(a()->sess().dirs().temp_directory(),
                                   StrCat(".does-not-exist-", a()->sess().instance_number(), ".$$$"));
      File::Remove(fn);
      File nullFile(fn);
      int terr = 0;
      send_b(nullFile, 0L, 3, 0, &ucrc, files::FileName(""), &terr, &abort);
      abort = true;
      File::Remove(fn);
    }
  }
  bout.localIO()->GotoXY(oldx, oldy);
}

void ymbatchdl(bool bHangupAfterDl) {
  int cur = 0;

  if (!a()->sess().incom()) {
    return;
  }
  auto message = StrCat("Ymodem Download: Files - ", a()->batch().size(),
                        ", Time - ", ctim(a()->batch().dl_time_in_secs()));
  if (bHangupAfterDl) {
    message += ", HAD";
  }
  sysoplog() << message;
  bout.nl();
  bout << message;
  bout.nl(2);

  bool bRatioBad = false;
  bool ok = true;
  //TODO(rushfan): rewrite to use iterators.
  do {
    a()->tleft(true);
    if ((a()->config()->req_ratio() > 0.0001) && (a()->user()->ratio() < a()->config()->req_ratio())) {
      bRatioBad = true;
    }
    if (a()->user()->exempt_ratio()) {
      bRatioBad = false;
    }
    if (!a()->batch().entry[cur].sending()) {
      bRatioBad = false;
      ++cur;
    }
    if (nsl() >= a()->batch().entry[cur].time(a()->modem_speed_) && !bRatioBad) {
      const auto dir_num = a()->batch().entry[cur].dir();
      dliscan1(dir_num);
      if (const auto record_number = recno(a()->batch().entry[cur].aligned_filename());
          record_number <= 0) {
        a()->batch().delbatch(cur);
      } else {
        bout.localIO()->Puts(StrCat("Files left - ", a()->batch().size(), ", Time left - ",
                                    ctim(a()->batch().dl_time_in_secs()), "\r\n"));
        auto* area = a()->current_file_area();
        auto f = area->ReadFile(record_number);
        auto send_filename = FilePath(a()->dirs()[dir_num].path, f);
        if (a()->dirs()[dir_num].mask & mask_cdrom) {
          auto orig_filename = FilePath(a()->dirs()[dir_num].path, f);
          send_filename = FilePath(a()->sess().dirs().temp_directory(), f);
          if (!File::Exists(send_filename)) {
            File::Copy(orig_filename, send_filename);
          }
        }
        write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_NONE);
        double percent;
        xymodem_send(send_filename.string(), &ok, &percent, true, true, true);
        if (ok) {
          downloaded(f.aligned_filename(), 0);
        }
      }
    } else {
      a()->batch().delbatch(cur);
    }
  }
  while (ok && !a()->sess().hangup() && ssize(a()->batch().entry) > cur && !bRatioBad);

  if (ok && !a()->sess().hangup()) {
    end_ymodem_batch();
  }
  if (bRatioBad) {
    bout << "\r\nYour ratio is too low to continue the transfer.\r\n\n";
  }
  if (bHangupAfterDl) {
    bihangup();
  }
}

static double ratio1(unsigned long xa) {
  if (a()->user()->dk() == 0 && xa == 0) {
    return 99.999;
  }
  const auto r =
      static_cast<double>(a()->user()->uk()) / static_cast<double>(a()->user()->dk() + xa);
  return std::min<double>(r, 99.998);
}

static std::string make_ul_batch_list() {
  const auto fn = fmt::sprintf("%s.%3.3u", FILESUL_NOEXT, a()->sess().instance_number());
  // TODO(rushfan): This should move to a temp directory.
  const auto list_filename = FilePath(a()->bbspath(), fn);

  File::Remove(list_filename, true);

  TextFile tf(list_filename, "wt");
  for (const auto& b : a()->batch().entry) {
    if (b.sending()) {
      continue;
    }
    auto line = FilePath(a()->dirs()[b.dir()].path, files::FileName(b.aligned_filename()));
    tf.WriteLine(line.string());
  }
  return list_filename.string();
}

static std::filesystem::path make_dl_batch_list() {
  const auto fn = fmt::sprintf("%s.%3.3u", FILESDL_NOEXT, a()->sess().instance_number());
  auto list_filename = FilePath(a()->bbspath(), fn);

  File::Remove(list_filename, true);

  TextFile tf(list_filename, "wt");

  int32_t at = 0;
  unsigned long addk = 0;
  for (const auto& b : a()->batch().entry) {
    if (!b.sending()) {
      continue;
    }
    std::string filename_to_send;
    if (a()->dirs()[b.dir()].mask & mask_cdrom) {
      const auto fileToSend =
          FilePath(a()->sess().dirs().temp_directory(), files::FileName(b.aligned_filename()));
      if (!File::Exists(fileToSend)) {
        auto sourceFile = FilePath(a()->dirs()[b.dir()].path, files::FileName(b.aligned_filename()));
        File::Copy(sourceFile, fileToSend);
      }
      filename_to_send = fileToSend.string();
    } else {
      filename_to_send = FilePath(a()->dirs()[b.dir()].path, files::FileName(b.aligned_filename())).string();
    }
    bool ok = true;
    if (nsl() < b.time(a()->modem_speed_) + at) {
      ok = false;
      bout << "Cannot download " << b.aligned_filename() << ": Not enough time" << wwiv::endl;
    }
    const auto thisk = bytes_to_k(b.len());
    if (a()->config()->req_ratio() > 0.0001f &&
        ratio1(addk + thisk) < a()->config()->req_ratio() &&
        !a()->user()->exempt_ratio()) {
      ok = false;
      bout << "Cannot download " << b.aligned_filename() << ": Ratio too low" << wwiv::endl;
    }
    if (ok) {
      tf.WriteLine(filename_to_send);
      at += b.time(a()->modem_speed_);
      addk += thisk;
    }
  }
  return list_filename;
}

static void run_cmd(const std::string& orig_commandline, const std::string& downlist, const std::string& uplist,
                    const std::string& dl, bool bHangupAfterDl) {
  std::string commandLine = stuff_in(orig_commandline,
                                std::to_string(std::min<int>(a()->modem_speed_, 57600)),
                                std::to_string(a()->primary_port()),
                                downlist,
                                std::to_string(std::min<int>(a()->modem_speed_, 57600)),
                                uplist);

  if (!commandLine.empty()) {
    make_abs_cmd(a()->bbspath(), &commandLine);
    a()->Cls();
    const auto user_name_number = a()->user()->name_and_number();
    const auto message = fmt::sprintf("%s is currently online at %u bps\r\n\r\n%s\r\n%s\r\n",
                                      user_name_number, a()->modem_speed_, dl, commandLine);
    bout.localIO()->Puts(message);
    if (a()->sess().incom()) {
      File::Remove(a()->dsz_logfile_name_, true);
      ExecuteExternalProgram(commandLine, a()->spawn_option(SPAWNOPT_PROT_BATCH) | EFLAG_BATCH_DIR);
      if (bHangupAfterDl) {
        bihangup();
      } else {
        bout << "\r\n|#9Please wait...\r\n\n";
      }
      ProcessDSZLogFile(a()->dsz_logfile_name_);
      a()->UpdateTopScreen();
    }
  }
  if (!downlist.empty()) {
    File::Remove(downlist, true);
  }
  if (!uplist.empty()) {
    File::Remove(uplist, true);
  }
}


void dszbatchdl(bool bHangupAfterDl, const std::string& command_line, const std::string& description) {
  auto download_log_entry = fmt::format("{} BATCH Download: Files - {}, Time - ()", 
        description, a()->batch().size(), ctim(a()->batch().dl_time_in_secs()));
  if (bHangupAfterDl) {
    download_log_entry += ", HAD";
  }
  sysoplog() << download_log_entry;
  bout.nl();
  bout << download_log_entry;
  bout.nl(2);

  write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_NONE);
  const auto list_filename = make_dl_batch_list();
  run_cmd(command_line, list_filename.string(), "", download_log_entry, bHangupAfterDl);
}

static void dszbatchul(bool bHangupAfterDl, char* command_line, const std::string& description) {
  auto download_log_entry =
      fmt::format("{} BATCH Upload: Files - {}", description, a()->batch().size());
  if (bHangupAfterDl) {
    download_log_entry += ", HAD";
  }
  sysoplog() << download_log_entry;
  bout.nl();
  bout << download_log_entry;
  bout.nl(2);

  write_inst(INST_LOC_UPLOAD, a()->current_user_dir().subnum, INST_FLAGS_NONE);
  const auto list_filename = make_ul_batch_list();

  const auto ti = std::chrono::system_clock::now();
  run_cmd(command_line, "", list_filename, download_log_entry, bHangupAfterDl);
  const auto time_used = std::chrono::system_clock::now() - ti;
  a()->user()->add_extratime(time_used);
}

int batchdl(int mode) {
  bool done = false;
  do {
    char ch = 0;
    switch (mode) {  // NOLINT(hicpp-multiway-paths-covered)
    case 0:
    case 3:
      bout.nl();
      if (mode == 3) {
        bout <<
            "|#7[|#2L|#7]|#1ist Files, |#7[|#2C|#7]|#1lear Queue, |#7[|#2R|#7]|#1emove File, |#7[|#2Q|#7]|#1uit or |#7[|#2D|#7]|#1ownload : |#0";
        ch = onek("QLRDC\r");
      } else {
        bout << "|#9Batch: L,R,Q,C,D,U,? : ";
        ch = onek("Q?CLRDU");
      }
      break;
    case 1:
      listbatch();
      ch = 'D';
      break;
    case 2:
      listbatch();
      ch = 'U';
      break;
    }
    switch (ch) {
    case '?':
      bout.printfile(TBATCH_NOEXT);
      break;
    case 'Q':
      if (mode == 3) {
        return 1;
      }
      done = true;
      break;
    case 'L':
      listbatch();
      break;
    case 'R': {
      bout.nl();
      bout << "|#9Remove which? ";
      std::string s = bin.input(4);
      auto i = to_number<int>(s);
      if (i > 0 && i <= ssize(a()->batch().entry)) {
        didnt_upload(a()->batch().entry[i - 1]);
        a()->batch().delbatch(i - 1);
      }
      if (a()->batch().empty()) {
        bout << "\r\nBatch queue empty.\r\n\n";
        done = true;
      }
    }
    break;
    case 'C':
      bout << "|#5Clear queue? ";
      if (bin.yesno()) {
        for (const auto& b : a()->batch().entry) { didnt_upload(b); }
        a()->batch().entry.clear();
        bout << "Queue cleared.\r\n";
        if (mode == 3) {
          return 1;
        }
      }
      done = true;
      break;
    case 'U': {
      if (mode != 3) {
        bout.nl();
        bout << "|#5Hang up after transfer? ";
        const auto hangup_after_dl = bin.yesno();
        bout.nl(2);
        int i = get_protocol(xfertype::xf_up_batch);
        if (i > 0) {
          dszbatchul(hangup_after_dl, a()->externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].receivebatchfn,
                     a()->externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].description);
          if (!hangup_after_dl) {
            bout.bprintf("Your ratio is now: %-6.3f\r\n", a()->user()->ratio());
          }
        }
        done = true;
      }
    }
    break;
    case 'D':
    case 13:
      if (mode != 3) {
        if (a()->batch().numbatchdl() == 0) {
          bout << "\r\nNothing in batch download queue.\r\n\n";
          done = true;
          break;
        }
        bout.nl();
        if (!ratio_ok()) {
          bout << "\r\nSorry, your ratio is too low.\r\n\n";
          done = true;
          break;
        }
        bout << "|#5Hang up after transfer? ";
        const auto hangup_after_dl = bin.yesno();
        bout.nl();
        int i = get_protocol(xfertype::xf_down_batch);
        if (i > 0) {
          if (i == WWIV_INTERNAL_PROT_YMODEM) {
            if (!a()->over_intern.empty() && a()->over_intern[2].othr & othr_override_internal &&
                a()->over_intern[2].sendbatchfn[0]) {
              dszbatchdl(hangup_after_dl, a()->over_intern[2].sendbatchfn, prot_name(4));
            } else {
              ymbatchdl(hangup_after_dl);
            }
          } else if (i == WWIV_INTERNAL_PROT_ZMODEM) {
            zmbatchdl(hangup_after_dl);
          } else {
            dszbatchdl(hangup_after_dl, a()->externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn,
                       a()->externs[i - WWIV_NUM_INTERNAL_PROTOCOLS].description);
          }
          if (!hangup_after_dl) {
            bout.nl();
            bout.bprintf("Your ratio is now: %-6.3f\r\n", a()->user()->ratio());
          }
        }
      }
      done = true;
      break;
    }
  }
  while (!done && !a()->sess().hangup());
  return 0;
}

void upload(int dn) {
  const auto& d = a()->dirs()[dn];
  dliscan1(d);
  const long free_space = File::freespace_for_path(d.path);
  if (free_space < 100) {
    bout << "\r\nNot enough disk space to upload here.\r\n\n";
    return;
  }
  listbatch();
  bout << "Upload - " << free_space << "k free.";
  bout.nl();
  bout.printfile(TRY2UL_NOEXT);

  bool done = false;
  do {
    bout << "|#2B|#7) |#1Blind batch upload\r\n";
    bout << "|#2N|#7) |#1Normal upload\r\n";
    bout << "|#2Q|#7) |#1Quit\r\n\n";
    bout << "|#2Which |#7(|#2B,n,q,?|#7)|#1: ";

    const char key = onek("QB\rN?");
    switch (key) {
    case 'B':
    case '\r':
      batchdl(2);
      done = true;
      break;
    case 'Q':
      done = true;
      break;
    case 'N':
      normalupload(dn);
      done = true;
      break;
    case '?':
      bout << "This is help?";
      bout.nl();
      bout.pausescr();
      done = false;
      break;
    }
  }
  while (!done && !a()->sess().hangup());
}

std::chrono::seconds time_to_transfer(int32_t file_size, int32_t modem_speed) {
  if (modem_speed == 0) {
    return std::chrono::seconds(0);
  }
  const double ms = modem_speed;
  const double fs = file_size;
  return std::chrono::seconds(std::lround(12.656 * fs / ms));
}

BatchEntry::BatchEntry(std::string fn, int d, int l, bool s)
: filename_(std::move(fn)), dir_(static_cast<int16_t>(d)), len_(l), sending_(s) {}
BatchEntry::BatchEntry(const files::FileName& fn, int d, int l, bool s)
: BatchEntry(fn.aligned_filename(), d, l, s) {}

BatchEntry::BatchEntry() = default;

int32_t BatchEntry::time(int modem_speed) const {
  const auto d = time_to_transfer(modem_speed, len());
  return static_cast<int32_t>(d.count());
}

int Batch::FindBatch(const std::string& file_name) {
  for (int i = 0; i < size_int(entry); i++) {
    if (iequals(file_name, entry[i].aligned_filename())) {
      return i;
    }
  }

  return -1;
}

bool Batch::RemoveBatch(const std::string& file_name) {
  const auto n = FindBatch(file_name);
  if (n < 0) {
    return false;
  }
  return delbatch(n);
}

bool Batch::delbatch(size_t pos) {
  if (pos >= entry.size()) {
    return false;
  }
  return erase_at(entry, pos);
}

std::vector<BatchEntry>::iterator Batch::delbatch(std::vector<BatchEntry>::iterator& it) {
  return entry.erase(it);
}

long Batch::dl_time_in_secs() const {

  if (a()->modem_speed_ == 0) {
    return 0;
  }

  size_t r = 0;
  for (const auto& e : entry) {
    if (e.sending()) {
      r += e.len();
    }
  }

  const auto t = 12.656 * r / a()->modem_speed_;
  return std::lround(t);
}

bool Batch::contains_file(const std::string& file_name) const {
  for (const auto& b : entry) {
    if (iequals(file_name, b.aligned_filename())) {
      return true;
    }
  }
  return false;

}

bool Batch::contains_file(const files::FileName& fn) const {
  return contains_file(fn.aligned_filename());
}
