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
#include "bbs/readmail.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/extract.h"
#include "bbs/instmsg.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/read_message.h"
#include "bbs/shortmsg.h"
#include "bbs/showfiles.h"
#include "bbs/sr.h"
#include "bbs/subacc.h"
#include "bbs/sublist.h"
#include "bbs/sysopf.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/quote.h"
#include "common/workspace.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/net/networks.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Implementation
static bool same_email(tmpmailrec& tm, const mailrec& m) {
  if (tm.fromsys != m.fromsys || tm.fromuser != m.fromuser || m.tosys != 0 ||
      m.touser != a()->sess().user_num() || tm.daten != m.daten || tm.index == -1 ||
      memcmp(&tm.msg, &m.msg, sizeof(messagerec)) != 0) {
    return false;
  }
  return true;
}

static void purgemail(std::vector<tmpmailrec>& mloc, int mw, int* curmail, mailrec* m1, slrec* sl) {
  mailrec m{};

  if (m1->anony & anony_sender && (sl->ability & ability_read_email_anony) == 0) {
    bout << "|#5Delete all mail to you from this user? ";
  } else {
    bout << "|#5Delete all mail to you from ";
    if (m1->fromsys) {
      bout << "#" << m1->fromuser << " @" << m1->fromsys << "? ";
    } else {
      if (m1->fromuser == 65535) {
        bout << "Networks? ";
      } else {
        bout << "#" << m1->fromuser << "? ";
      }
    }
  }
  if (bin.yesno()) {
    auto pFileEmail(OpenEmailFile(true));
    if (!pFileEmail->IsOpen()) {
      return;
    }
    for (int i = 0; i < mw; i++) {
      if (mloc[i].index >= 0) {
        pFileEmail->Seek(mloc[i].index * sizeof(mailrec), File::Whence::begin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if (same_email(mloc[i], m)) {
          if (m.fromuser == m1->fromuser && m.fromsys == m1->fromsys) {
            bout << "Deleting mail msg #" << i + 1 << wwiv::endl;
            delmail(*pFileEmail, mloc[i].index);
            mloc[i].index = -1;
            if (*curmail == i) {
              ++(*curmail);
            }
          }
        }
      } else {
        if (*curmail == i) {
          ++(*curmail);
        }
      }
    }
    pFileEmail->Close();
  }
}

static void resynch_email(std::vector<tmpmailrec>& mloc, int mw, int rec, mailrec* m, bool del,
                          unsigned short stat) {
  int i;
  mailrec m1{};

  auto pFileEmail(OpenEmailFile(del || stat));
  if (pFileEmail->IsOpen()) {
    const auto mfl = static_cast<File::size_type>(pFileEmail->length() / sizeof(mailrec));

    for (i = 0; i < mw; i++) {
      if (mloc[i].index >= 0) {
        mloc[i].index = -2;
      }
    }

    int mp = 0;

    for (i = 0; i < mfl; i++) {
      pFileEmail->Seek(i * sizeof(mailrec), File::Whence::begin);
      pFileEmail->Read(&m1, sizeof(mailrec));

      if (m1.tosys == 0 && m1.touser == a()->sess().user_num()) {
        for (int i1 = mp; i1 < mw; i1++) {
          if (same_email(mloc[i1], m1)) {
            mloc[i1].index = static_cast<int16_t>(i);
            mp = i1 + 1;
            if (i1 == rec) {
              *m = m1;
            }
            break;
          }
        }
      }
    }

    for (i = 0; i < mw; i++) {
      if (mloc[i].index == -2) {
        mloc[i].index = -1;
      }
    }

    if (stat && !del && (mloc[rec].index >= 0)) {
      m->status |= stat;
      pFileEmail->Seek(mloc[rec].index * sizeof(mailrec), File::Whence::begin);
      pFileEmail->Write(m, sizeof(mailrec));
    }
    if (del && (mloc[rec].index >= 0)) {
      delmail(*pFileEmail, mloc[rec].index);
      mloc[rec].index = -1;
    }
    pFileEmail->Close();
  } else {
    mloc[rec].index = -1;
  }
}

// used in qwk1.cpp
bool read_same_email(std::vector<tmpmailrec>& mloc, int mw, int rec, mailrec& m, bool del,
                     uint16_t stat) {
  if (at(mloc, rec).index < 0) {
    return false;
  }

  auto file = OpenEmailFile(del || stat);
  if (!file->IsOpen()) {
    LOG(ERROR) << "read_same_email: Failed to open email file.";
    return false;
  }
  file->Seek(mloc[rec].index * sizeof(mailrec), File::Whence::begin);
  file->Read(&m, sizeof(mailrec));

  if (!same_email(mloc[rec], m)) {
    file->Close();
    a()->status_manager()->reload_status();
    if (a()->emchg_) {
      resynch_email(mloc, mw, rec, &m, del, stat);
    } else {
      mloc[rec].index = -1;
    }
  } else {
    if (stat && !del && mloc[rec].index >= 0) {
      m.status |= stat;
      file->Seek(mloc[rec].index * sizeof(mailrec), File::Whence::begin);
      file->Write(&m, sizeof(mailrec));
    }
    if (del) {
      delmail(*file, mloc[rec].index);
      mloc[rec].index = -1;
    }
    file->Close();
  }
  return mloc[rec].index != -1;
}

static void add_netsubscriber(const Network& net, int network_number, int system_number) {
  if (!valid_system(system_number)) {
    system_number = 0;
  }

  bout.nl();
  bout << "|#1Adding subscriber to subscriber list...\r\n\n";
  bout << "|#2SubType: ";
  const auto subtype = bin.input(7, true);
  if (subtype.empty()) {
    return;
  }
  const auto fn = StrCat(net.dir, "n", subtype, ".net");
  if (!File::Exists(fn)) {
    bout.nl();
    bout << "|#6Subscriber file not found: " << fn << wwiv::endl;
    return;
  }
  bout.nl();
  if (system_number) {
    bout << "Add @" << system_number << "." << net.name << " to subtype " << subtype << "? ";
  }
  if (!system_number || !bin.noyes()) {
    bout << "|#2System Number: ";
    const auto s = bin.input(5, true);
    if (s.empty()) {
      return;
    }
    system_number = to_number<int>(s);
    if (!valid_system(system_number)) {
      bout << "@" << system_number << " is not a valid system in " << net.name << ".\r\n\n";
      return;
    }
  }
  TextFile host_file(fn, "a+t");
  if (host_file.IsOpen()) {
    host_file.Write(fmt::sprintf("%u\n", system_number));
    host_file.Close();
    // TODO find replacement for autosend.exe
    if (File::Exists("autosend.exe")) {
      bout << "AutoSend starter messages? ";
      if (bin.yesno()) {
        const auto autosend = FilePath(a()->bindir(), "autosend");
        const auto cmd = StrCat(autosend.string(), " ", subtype, " ", system_number, " .", network_number);
        ExecuteExternalProgram(cmd, EFLAG_NONE);
      }
    }
  }
}

void delete_attachment(unsigned long daten, int forceit) {
  filestatusrec fsr{};

  auto found = false;
  File fileAttach(FilePath(a()->config()->datadir(), ATTACH_DAT));
  if (fileAttach.Open(File::modeBinary | File::modeReadWrite)) {
    auto l = fileAttach.Read(&fsr, sizeof(fsr));
    while (l > 0 && !found) {
      if (daten == static_cast<unsigned long>(fsr.id)) {
        found = true;
        fsr.id = 0;
        fileAttach.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
        fileAttach.Write(&fsr, sizeof(filestatusrec));
        auto delfile = true;
        if (!forceit) {
          if (so()) {
            bout << "|#5Delete attached file? ";
            delfile = bin.yesno();
          }
        }
        if (delfile) {
          File::Remove(FilePath(a()->GetAttachmentDirectory(), fsr.filename));
        } else {
          bout << "\r\nOrphaned attach " << fsr.filename << " remains in "
               << a()->GetAttachmentDirectory() << wwiv::endl;
          bout.pausescr();
        }
      } else {
        l = fileAttach.Read(&fsr, sizeof(filestatusrec));
      }
    }
    fileAttach.Close();
  }
}

static std::string from_name(const mailrec& m, const Network& net, const slrec& sl, int nn) {
  if (m.anony & anony_sender && (sl.ability & ability_read_email_anony) == 0) {
    return ">UNKNOWN<";
  }
  auto csne = next_system(m.fromsys);
  const std::string system_name = csne ? csne->name : "Unknown System";
  if (m.fromsys == 0) {
    if (m.fromuser == 65535) {
      if (nn != 255) {
        return net.name;
      }
    } else {
      return a()->names()->UserName(m.fromuser);
    }
  } else {
    if (nn == 255) {
      return fmt::format("#{} @{}.", m.fromuser, m.fromsys, net.name);
    }
    if (auto o = readfile(&m.msg, "email")) {
      if (const auto idx = o.value().find('\r'); idx != std::string::npos) {
        const std::string from = o.value().substr(0, idx);
        if (m.fromsys == INTERNET_EMAIL_FAKE_OUTBOUND_NODE || m.fromsys == FTN_FAKE_OUTBOUND_NODE) {
          return stripcolors(from);
        }
        return fmt::format("{} {}@{}.{} ({})", stripcolors(wwiv::common::strip_to_node(from)),
                           m.fromuser, m.fromsys, net.name, system_name);
      }
    } else {
      if (ssize(a()->nets()) > 1) {
        return fmt::format("#{} @{}.{} ({})", m.fromuser, m.fromsys, net.name, system_name);
      }
    }
  }
  return fmt::format("#{} @{} ({})", m.fromuser, m.fromsys, system_name);
}

static std::tuple<Network, int> network_and_num(const mailrec& m) {
  Network net{};
  auto nn = network_number_from(&m);
  if (nn <= a()->nets().size()) {
    net = a()->nets()[nn];
  } else {
    net.sysnum = static_cast<uint16_t>(-1);
    net.type = network_type_t::unknown;
    net.name = fmt::format("<deleted network #{}>", nn);
    nn = 255;
  }
  return std::make_tuple(net, nn);
}

void readmail(bool newmail_only) {
  constexpr auto mail_who_field_len = 45;
  int i1, curmail = 0;
  bool done;
  mailrec m{};
  mailrec m1{};
  char ch;
  filestatusrec fsr{};

  a()->emchg_ = false;

  bool next = false, abort = false;
  std::vector<tmpmailrec> mloc;
  write_inst(INST_LOC_RMAIL, 0, INST_FLAGS_NONE);
  auto sl = a()->config()->sl(a()->sess().effective_sl());
  auto mw = 0;
  {
    auto file(OpenEmailFile(false));
    if (!file->IsOpen()) {
      bout << "\r\n\nNo mail file exists!\r\n\n";
      return;
    }
    auto mfl = static_cast<File::size_type>(file->length() / sizeof(mailrec));
    for (int i = 0; i < mfl && mw < MAXMAIL; i++) {
      file->Seek(i * sizeof(mailrec), File::Whence::begin);
      file->Read(&m, sizeof(mailrec));
      if (m.tosys == 0 && m.touser == a()->sess().user_num()) {
        tmpmailrec r = {};
        r.index = static_cast<int16_t>(i);
        r.fromsys = m.fromsys;
        r.fromuser = m.fromuser;
        r.daten = m.daten;
        r.msg = m.msg;
        mloc.emplace_back(r);
        mw++;
      }
    }
    file->Close();
  }
  a()->user()->email_waiting(mw);
  if (mloc.empty()) {
    bout << "\r\n\n|#3You have no mail.\r\n\n";
    return;
  }
  if (mloc.size() == 1) {
    curmail = 0;
  } else {
    bout << "\r\n\n|#2You have mail from:\r\n";
    bout << "|#9" << std::string(a()->user()->screen_width() - 1, '-') << wwiv::endl;
    for (auto i = 0; i < mw && !abort; i++) {
      if (!read_same_email(mloc, mw, i, m, false, 0)) {
        continue;
      }
      if (newmail_only && (m.status & status_seen)) {
        ++curmail;
        continue;
      }

      auto [net, nn] = network_and_num(m);
      set_net_num(nn);
      const auto current_line =
          fmt::format("|#2{:>3}{}|#1{:<45.45}|#7| |#1{:<25.25}", i + 1, (m.status & status_seen ? " " : "|#3*"),
                      from_name(m, net, sl, nn), stripcolors(m.title));
      bout.bpla(current_line, &abort);
    }
    bout << "|#9" << std::string(a()->user()->screen_width() - 1, '-') << wwiv::endl;
    bout.bputs("|#9(|#2Q|#9=|#2Quit|#9, |#2Enter|#9=|#2First Message|#9) \r\n|#9Enter message number: ");
    // TODO: use input numberor hotkey for number 1-mw, or Q
    auto res = bin.input_number_hotkey(1, {'Q'}, 1, mw);
    if (res.key == 'Q') { 
      return;
    }
    if (res.num > 0 && !newmail_only) {
      if (res.num <= mw) {
        curmail = res.num - 1;
      } else {
        curmail = 0;
      }
    }
  }
  done = false;

  do {
    bool okmail;
    auto attach_exists = false;
    auto found = false;
    abort = false;
    bout.nl(2);
    next = false;

    if (std::string title = m.title; !read_same_email(mloc, mw, curmail, m, false, 0)) {
      title += ">>> MAIL DELETED <<<";
      okmail = false;
      bout.nl(3);
    } else {
      strcpy(a()->sess().irt_, m.title);
      abort = false;
      auto readit  = ((ability_read_email_anony & sl.ability) != 0);
      okmail = true;
      if (m.fromsys && !m.fromuser) {
        grab_user_name(&(m.msg), "email", network_number_from(&m));
      } else {
        a()->net_email_name.clear();
      }
      if (m.status & status_source_verified) {
        if (int sv_type = source_verfied_type(&m); sv_type > 0) {
          title += StrCat("-=> Source Verified Type ", sv_type);
          if (sv_type == 1) {
            title += " (From NC)";
          } else if (sv_type > 256 && sv_type < 512) {
            title += StrCat(" (From GC-", sv_type - 256, ")");
          }
        } else {
          title += "-=> Source Verified (unknown type)";
        }
      }
      auto [net, nn] = network_and_num(m);
      set_net_num(nn);
      int nFromSystem = 0;
      int nFromUser = 0;
      if (nn != 255) {
        nFromSystem = m.fromsys;
        nFromUser = m.fromuser;
      }
      if (!abort) {
        // read_type2_message will parse out the title and other fields of the
        // message, including sender name (which is all we have for FTN messages).
        // We need to get the full header before that and pass it into this
        // method to display it.
        auto msg = read_type2_message(&m.msg, m.anony & 0x0f, readit ? true : false, "email",
                                      nFromSystem, nFromUser);
        msg.message_area = "Personal E-Mail";
        msg.title = m.title;
        msg.message_number = curmail + 1;
        msg.total_messages = mw;
        // We set this to false since we do *not* want to use the
        // command handling from the full screen reader.
        msg.use_msg_command_handler = false;
        if (a()->current_net().type == network_type_t::ftn) {
          // Set email name to be the to address.
          // This is also done above in grab_user_name, but we should stop using
          // a()->net_email_name
          a()->net_email_name = msg.from_user_name;
        }
        int fake_msgno = -1;
        display_type2_message(fake_msgno, msg, &next);
        if (!(m.status & status_seen)) {
          read_same_email(mloc, mw, curmail, m, false, status_seen);
        }
      }
      found = false;
      attach_exists = false;
      if (m.status & status_file) {
        File fileAttach(FilePath(a()->config()->datadir(), ATTACH_DAT));
        if (fileAttach.Open(File::modeBinary | File::modeReadOnly)) {
          auto l1 = fileAttach.Read(&fsr, sizeof(fsr));
          while (l1 > 0 && !found) {
            if (m.daten == static_cast<uint32_t>(fsr.id)) {
              found = true;
              if (File::Exists(FilePath(a()->GetAttachmentDirectory(), fsr.filename))) {
                bout << "'T' to download attached file \"" << fsr.filename << "\" (" << fsr.numbytes
                     << " bytes).\r\n";
                attach_exists = true;
              } else {
                bout << "Attached file \"" << fsr.filename << "\" (" << fsr.numbytes
                     << " bytes) is missing!\r\n";
              }
            }
            if (!found) {
              l1 = fileAttach.Read(&fsr, sizeof(fsr));
            }
          }
          if (!found) {
            bout << "File attached but attachment data missing.  Alert sysop!\r\n";
          }
        } else {
          bout << "File attached but attachment data missing.  Alert sysop!\r\n";
        }
        fileAttach.Close();
      }
    }

    do {
      char mnu[81];
      int delme;
      std::string allowable;
      write_inst(INST_LOC_RMAIL, 0, INST_FLAGS_NONE);
      auto [net, nn] = network_and_num(m);
      set_net_num(nn);
      i1 = 1;
      if (!a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
        strcpy(mnu, EMAIL_NOEXT);
        bout << "|#2Mail {?} : ";
      }
      if (so()) {
        strcpy(mnu, SY_EMAIL_NOEXT);
        if (a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
          bout << "|#2Mail |#7{|#1QSRIDAF?-+GEMZPVUOLCNY@|#7} |#2: ";
        }
        allowable = "QSRIDAF?-+GEMZPVUOLCNY@";
      } else {
        if (cs()) {
          strcpy(mnu, CS_EMAIL_NOEXT);
          if (a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
            bout << "|#2Mail |#7{|#1QSRIDAF?-+GZPVUOCY@|#7} |#2: ";
          }
          allowable = "QSRIDAF?-+GZPVUOCY@";
        } else {
          if (!okmail) {
            strcpy(mnu, RS_EMAIL_NOEXT);
            if (a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
              bout << "|#2Mail |#7{|#1QI?-+GY|#7} |#2: ";
            }
            allowable = "QI?-+G";
          } else {
            strcpy(mnu, EMAIL_NOEXT);
            if (a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
              bout << "|#2Mail |#7{|#1QSRIDAF?+-GY@|#7} |#2: ";
            }
            allowable = "QSRIDAF?-+GY@";
          }
        }
      }
      if ((m.status & status_file) && found && attach_exists) {
        if (a()->HasConfigFlag(OP_FLAGS_MAIL_PROMPT)) {
          bout << "\b\b|#7{|#1T|#7} |#2: |#0";
        }
        allowable += "T";
      }
      ch = onek(allowable);
      if (okmail && !read_same_email(mloc, mw, curmail, m, false, 0)) {
        bout << "\r\nMail got deleted.\r\n\n";
        ch = 'R';
      }
      delme = 0;
      switch (ch) {
        int num_mail1;
        int num_mail;
      case 'T': {
        bout.nl();
        auto fn = FilePath(a()->GetAttachmentDirectory(), fsr.filename);
        bool sentt;
        bool abortt;
        send_file(fn.string(), &sentt, &abortt, fsr.filename, -1, fsr.numbytes);
        if (sentt) {
          bout << "\r\nAttached file sent.\r\n";
          sysoplog() << fmt::sprintf("Downloaded %ldk of attached file %s.",
                                     (fsr.numbytes + 1023) / 1024, fsr.filename);
        } else {
          bout << "\r\nAttached file not completely sent.\r\n";
          sysoplog() << fmt::sprintf("Tried to download attached file %s.", fsr.filename);
        }
        bout.nl();
      } break;
      case 'N':
        if (m.fromuser == 1) {
          add_netsubscriber(net, nn, m.fromsys);
        } else {
          add_netsubscriber(net, nn, 0);
        }
        break;
      case 'E':
        if (so() && okmail) {
          if (auto o = readfile(&(m.msg), "email")) {
            auto b = o.value();
            extract_out(b, m.title);
          }
        }
        i1 = 0;
        break;
      case 'Q':
        done = true;
        break;
      case 'O': {
        if (cs() && okmail && m.fromuser != 65535 && nn != 255) {
          show_files("*.frm", a()->config()->gfilesdir().c_str());
          bout << "|#2Which form letter: ";
          auto user_input = bin.input(8, true);
          if (user_input.empty()) {
            break;
          }
          auto fn = FilePath(a()->config()->gfilesdir(), StrCat(user_input, ".frm"));
          if (!File::Exists(fn)) {
            fn = FilePath(a()->config()->gfilesdir(), StrCat("form", user_input, ".msg"));
          }
          if (File::Exists(fn)) {
            LoadFileIntoWorkspace(a()->context(), fn, true);
            num_mail = a()->user()->feedback_sent() + a()->user()->email_sent() +
                       a()->user()->email_net();
            clear_quotes(a()->sess());
            if (m.fromuser != 65535) {
              email(m.title, m.fromuser, m.fromsys, false, m.anony);
            }
            num_mail1 = static_cast<long>(a()->user()->feedback_sent()) +
                        static_cast<long>(a()->user()->email_sent()) +
                        static_cast<long>(a()->user()->email_net());
            if (num_mail != num_mail1) {
              const auto userandnet =
                  a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
              std::string msg;
              if (m.fromsys != 0) {
                msg = StrCat(a()->network_name(), ": ", userandnet);
              } else {
                msg = userandnet;
              }
              if (m.anony & anony_receiver) {
                msg += ">UNKNOWN<";
              }
              msg += " read your mail on ";
              msg += fulldate();
              if (!(m.status & status_source_verified)) {
                ssm(m.fromuser, m.fromsys, &net) << msg;
              }
              read_same_email(mloc, mw, curmail, m, true, 0);
              ++curmail;
              if (curmail >= mw) {
                done = true;
              }
            } else {
              // need instance
              File::Remove(FilePath(a()->sess().dirs().temp_directory(), INPUT_MSG));
            }
          } else {
            bout << "\r\nFile not found.\r\n\n";
            i1 = 0;
          }
        }
      } break;
      case 'G': {
        bout << "|#2Go to which (1-" << mw << ") ? |#0";
        auto user_input = bin.input(3);
        i1 = to_number<int>(user_input);
        if (i1 > 0 && i1 <= mw) {
          curmail = i1 - 1;
          i1 = 1;
        } else {
          i1 = 0;
        }
      } break;
      case 'I':
      case '+':
        ++curmail;
        if (curmail >= mw) {
          done = true;
        }
        break;
      case '-':
        if (curmail) {
          --curmail;
        }
        break;
      case 'R':
        break;
      case '?':
        bout.printfile(mnu);
        i1 = 0;
        break;
      case 'M':
        if (!okmail) {
          break;
        }
        if (so()) {
          if (!a()->sess().IsUserOnline()) {
            a()->set_current_user_sub_num(0);
            a()->sess().SetCurrentReadMessageArea(0);
            a()->sess().set_current_user_sub_conf_num(0);
          }
          tmp_disable_conf(true);
          bout.nl();
          std::string ss1;
          do {
            bout << "|#2Move to which sub? ";
            ss1 = mmkey(MMKeyAreaType::subs);
            if (ss1[0] == '?') {
              old_sublist();
            }
          } while ((!a()->sess().hangup()) && (ss1[0] == '?'));
          auto i = -1;
          if ((ss1[0] == 0) || a()->sess().hangup()) {
            i1 = 0;
            bout.nl();
            tmp_disable_conf(false);
            break;
          }
          for (i1 = 0; i1 < ssize(a()->usub); i1++) {
            if (ss1 == a()->usub[i1].keys) {
              i = i1;
            }
          }
          if (i != -1) {
            const auto& sub = a()->subs().sub(a()->usub[i].subnum);
            if (!wwiv::bbs::check_acs(sub.post_acs)) {
              bout << "\r\nSorry, you don't have post access on that sub.\r\n\n";
              i = -1;
            }
          }
          if (i != -1) {
            auto o = readfile(&(m.msg), "email");
            if (!o) {
              break;
            }
            const auto& b = o.value();

            postrec p{};
            strcpy(p.title, m.title);
            p.anony = m.anony;
            p.ownersys = m.fromsys;
            a()->SetNumMessagesInCurrentMessageArea(p.owneruser);
            p.owneruser = static_cast<uint16_t>(a()->sess().user_num());
            p.msg = m.msg;
            p.daten = m.daten;
            p.status = 0;

            iscan(i);
            open_sub(true);
            if (!a()->current_sub().nets.empty()) {
              p.status |= status_pending_net;
            }
            p.msg.storage_type = static_cast<uint8_t>(a()->current_sub().storage_type);
            savefile(b, &(p.msg), a()->current_sub().filename);
            a()->status_manager()->Run([&](Status& status) {
              p.qscan = status.next_qscanptr();
            });
            if (a()->GetNumMessagesInCurrentMessageArea() >= a()->current_sub().maxmsgs) {
              int i2;
              i1 = 1;
              i2 = 0;
              while (i2 == 0 && i1 <= a()->GetNumMessagesInCurrentMessageArea()) {
                if ((get_post(i1)->status & status_no_delete) == 0) {
                  i2 = i1;
                }
                ++i1;
              }
              if (i2 == 0) {
                i2 = 1;
              }
              delete_message(i2);
            }
            add_post(&p);
            a()->status_manager()->Run([&](Status& status) {
              status.increment_msgs_today();
              status.IncrementNumLocalPosts();
            });
            close_sub();
            tmp_disable_conf(false);
            iscan(a()->current_user_sub_num());
            bout << "\r\n\n|#9Message moved.\r\n\n";
            auto temp_num_msgs = a()->GetNumMessagesInCurrentMessageArea();
            resynch(&temp_num_msgs, &p);
            a()->SetNumMessagesInCurrentMessageArea(temp_num_msgs);
          } else {
            tmp_disable_conf(false);
          }
        } break;
      case 'D': {
        std::string message;
        if (!okmail) {
          break;
        }
        bout << "|#5Delete this message? ";
        if (!bin.noyes()) {
          break;
        }
        if (m.fromsys != 0) {
          message = StrCat(a()->network_name(), ": ",
                           a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum));
        } else {
          message = a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
        }

        if (m.anony & anony_receiver) {
          message = ">UNKNOWN<";
        }

        message += " read your mail on ";
        message += fulldate();
        if (!(m.status & status_source_verified) && nn != 255) {
          ssm(m.fromuser, m.fromsys, &net) << message;
        }
      } 
      [[fallthrough]];
      case 'Z':
        if (!okmail) {
          break;
        }
        read_same_email(mloc, mw, curmail, m, true, 0);
        ++curmail;
        if (curmail >= mw) {
          done = true;
        }
        found = false;
        if (m.status & status_file) {
          delete_attachment(m.daten, 1);
        }
        break;
      case 'P':
        if (!okmail) {
          break;
        }
        purgemail(mloc, mw, &curmail, &m, &sl);
        if (curmail >= mw) {
          done = true;
        }
        break;
      case 'F': {
        if (!okmail) {
          break;
        }
        if (m.status & status_multimail) {
          bout << "\r\nCan't forward multimail.\r\n\n";
          break;
        }
        bout.nl(2);
        if (okfsed() && a()->user()->auto_quote()) {
          // TODO: optimize this since we also call readfile in grab_user_name
          auto reply_to_name = grab_user_name(&(m.msg), "email", network_number_from(&m));
          if (auto o = readfile(&(m.msg), "email")) {
            auto_quote(o.value(), reply_to_name, quote_date_format_t::forward, m.daten,
                       a()->context());
            send_email();
          }
          break;
        }

        bout << "|#2Forward to: ";
        auto user_input = fixup_user_entered_email(bin.input(75));
        auto [un, sn] = parse_email_info(user_input);
        if (un || sn) {
          if (ForwardMessage(&un, &sn)) {
            bout << "Mail forwarded.\r\n";
          }
          if (un == a()->sess().user_num() && sn == 0 && !cs()) {
            bout << "Can't forward to yourself.\r\n";
            un = 0;
          }
          if (un || sn) {
            std::string fwd_email_name;
            if (sn) {
              if (sn == 1 && un == 0 &&
                  a()->current_net().type == network_type_t::internet) {
                fwd_email_name = a()->net_email_name;
              } else {
                auto netname = ssize(a()->nets()) > 1 ? a()->network_name() : "";
                fwd_email_name = username_system_net_as_string(un, a()->net_email_name,
                                                                sn, netname);
              }
            } else {
              set_net_num(nn);
              fwd_email_name = a()->names()->UserName(un, a()->current_net().sysnum);
            }
            if (ok_to_mail(un, sn, false)) {
              bout << "|#5Forward to " << fwd_email_name << "? ";
              if (bin.yesno()) {
                auto file = OpenEmailFile(true);
                if (!file->IsOpen()) {
                  break;
                }
                file->Seek(mloc[curmail].index * sizeof(mailrec), File::Whence::begin);
                file->Read(&m, sizeof(mailrec));
                if (!same_email(mloc[curmail], m)) {
                  bout << "Error, mail moved.\r\n";
                  break;
                }
                bout << "|#5Delete this message? ";
                if (bin.yesno()) {
                  if (m.status & status_file) {
                    delete_attachment(m.daten, 0);
                  }
                  delme = 1;
                  m1.touser = 0;
                  m1.tosys = 0;
                  m1.daten = 0xffffffff;
                  m1.msg.storage_type = 0;
                  m1.msg.stored_as = 0xffffffff;
                  file->Seek(mloc[curmail].index * sizeof(mailrec), File::Whence::begin);
                  file->Write(&m1, sizeof(mailrec));
                } else {
                  std::string b;
                  if (auto o = readfile(&(m.msg), "email")) {
                    savefile(o.value(), &(m.msg), "email");
                  }
                }
                m.status |= status_forwarded;
                m.status |= status_seen;
                file->Close();

                auto net_num = a()->net_num();
                const auto fwd_user_name =
                    a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
                auto s = fmt::sprintf("\r\nForwarded to %s from %s.", fwd_email_name, fwd_user_name);

                set_net_num(nn);
                net = a()->nets()[nn];
                lineadd(&m.msg, s, "email");
                s = StrCat(fwd_user_name, " forwarded your mail to ", fwd_email_name);
                if (!(m.status & status_source_verified)) {
                  ssm(m.fromuser, m.fromsys, &net) << s;
                }
                set_net_num(net_num);
                s = StrCat("Forwarded mail to ", fwd_email_name);
                if (delme) {
                  a()->user()->email_waiting(a()->user()->email_waiting() - 1);
                }
                bout << "Forwarding: ";
                ::EmailData email;
                email.title = m.title;
                email.msg = &m.msg;
                email.anony = m.anony;
                email.user_number = un;
                email.system_number = sn;
                email.an = true;
                email.forwarded_code = delme; // this looks totally wrong to me...
                email.silent_mode = false;

                if (nn != 255 && nn == a()->net_num()) {
                  email.from_user = m.fromuser;
                  email.from_system = m.fromsys ? m.fromsys : a()->nets()[nn].sysnum;
                  email.from_network_number = nn;
                  sendout_email(email);
                } else {
                  email.set_from_user(a()->sess().user_num());
                  email.from_system = a()->current_net().sysnum;
                  email.from_network_number = a()->net_num();
                  sendout_email(email);
                }
                ++curmail;
                if (curmail >= mw) {
                  done = true;
                }
              }
            }
          }
        }
        delme = 0;
      } break;
      case 'A':
      case 'S':
      case '@': {
        if (!okmail) {
          break;
        }
        num_mail = static_cast<long>(a()->user()->feedback_sent()) +
                   static_cast<long>(a()->user()->email_sent()) +
                   static_cast<long>(a()->user()->email_net());
        if (nn == 255) {
          bout << "|#6Deleted network.\r\n";
          i1 = 0;
          break;
        }
        if (m.fromuser != 65535) {
          std::string reply_to_name;
          // TODO: optimize this since we also call readfile in grab_user_name
          reply_to_name = grab_user_name(&(m.msg), "email", network_number_from(&m));
          if (auto o = readfile(&(m.msg), "email")) {
            if (okfsed() && a()->user()->auto_quote()) {
              // used to be 1 or 2 depending on s[0] == '@', but
              // that's allowable now and @ was never in the beginning.
              auto_quote(o.value(), reply_to_name, quote_date_format_t::email, m.daten,
                         a()->context());
            }
            grab_quotes(o.value(), reply_to_name, a()->context());
          }

          if (ch == '@') {
            bout << "\r\n|#9Enter user name or number:\r\n:";
            auto user_email = fixup_user_entered_email(bin.input(75, true));
            auto [un, sy] = parse_email_info(user_email);
            if (un || sy) {
              email("", un, sy, false, 0);
            }
          } else {
            email("", m.fromuser, m.fromsys, false, m.anony);
          }
          clear_quotes(a()->sess());
        }
        num_mail1 = static_cast<long>(a()->user()->feedback_sent()) +
                    static_cast<long>(a()->user()->email_sent()) +
                    static_cast<long>(a()->user()->email_net());
        if (ch == 'A' || ch == '@') {
          if (num_mail != num_mail1) {
            std::string message;
            const auto name = a()->names()->UserName(a()->sess().user_num(), a()->current_net().sysnum);
            if (m.fromsys != 0) {
              message = a()->network_name();
              message += ": ";
              message += name;
            } else {
              message = name;
            }
            if (m.anony & anony_receiver) {
              message = ">UNKNOWN<";
            }
            message += " read your mail on ";
            message += fulldate();
            if (!(m.status & status_source_verified)) {
              ssm(m.fromuser, m.fromsys, &net) << message;
            }
            read_same_email(mloc, mw, curmail, m, true, 0);
            ++curmail;
            if (curmail >= mw) {
              done = true;
            }
            found = false;
            if (m.status & status_file) {
              delete_attachment(m.daten, 0);
            }
          } else {
            bout << "\r\nNo mail sent.\r\n\n";
            i1 = 0;
          }
        } else {
          if (num_mail != num_mail1) {
            if (!(m.status & status_replied)) {
              read_same_email(mloc, mw, curmail, m, false, status_replied);
            }
            ++curmail;
            if (curmail >= mw) {
              done = true;
            }
          }
        }
      } break;
      case 'U':
      case 'V':
      case 'C':
        if (!okmail) {
          break;
        }
        if (m.fromsys == 0 && cs() && m.fromuser != 65535) {
          if (ch == 'V') {
            valuser(m.fromuser);
          }
        } else if (cs()) {
          bout << "\r\nMail from another system.\r\n\n";
        }
        i1 = 0;
        break;
      case 'L': {
        if (!so()) {
          break;
        }
        bout << "\r\n|#2Filename: ";
        auto fileName = bin.input_path(50);
        if (!fileName.empty()) {
          bout.nl();
          bout << "|#5Allow editing? ";
          if (bin.yesno()) {
            bout.nl();
            LoadFileIntoWorkspace(a()->context(), fileName, false);
          } else {
            bout.nl();
            LoadFileIntoWorkspace(a()->context(), fileName, true);
          }
        }
      } break;
      case 'Y': // Add from here
        if (curmail >= 0) {
          auto o = readfile(&(m.msg), "email");
          if (!o) {
            break;
          }
          auto b = o.value();
          bout << "E-mail download -\r\n\n|#2Filename: ";
          auto downloadFileName = bin.input(12);
          if (!okfn(downloadFileName)) {
            break;
          }
          const auto fn = FilePath(a()->sess().dirs().temp_directory(), downloadFileName);
          File::Remove(fn);
          TextFile tf(fn, "w");
          tf.Write(b);
          tf.Close();
          bool bSent;
          bool bAbort;
          send_file(fn.string(), &bSent, &bAbort, fn.string(), -1, ssize(b));
          if (bSent) {
            bout << "E-mail download successful.\r\n";
            sysoplog() << "Downloaded E-mail";
          } else {
            bout << "E-mail download aborted.\r\n";
          }
        }
        break;
      }
    } while (!i1 && !a()->sess().hangup());
  } while (!a()->sess().hangup() && !done);
}

int check_new_mail(int user_number) {
  auto new_messages = 0; // number of new mail

  if (auto file(OpenEmailFile(false)); file->Exists() && file->IsOpen()) {
    const auto mfLength = static_cast<int>(file->length() / sizeof(mailrec));
    for (auto i = 0; i < mfLength; i++) {
      mailrec m{};
      file->Seek(i * sizeof(mailrec), File::Whence::begin);
      file->Read(&m, sizeof(mailrec));
      if (m.tosys == 0 && m.touser == user_number) {
        if (!(m.status & status_seen)) {
          ++new_messages;
        }
      }
    }
    file->Close();
  }
  return new_messages;
}
