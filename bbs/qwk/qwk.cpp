/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/qwk/qwk.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/instmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/qwk/qwk_mail_packet.h"
#include "bbs/qwk/qwk_reply.h"
#include "bbs/qwk/qwk_ui.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/qwk_config.h"
#include "sdk/vardec.h"

#include <string>

using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::stl;

namespace wwiv::bbs::qwk {

void qwk_download() {
  sysoplog("Download QWK packet");
  build_qwk_packet();
}

void qwk_upload() {
  sysoplog("Upload REP packet");
  upload_reply_packet();
}

void qwk_menu() {
  const auto qwk_cfg = read_qwk_cfg(*a()->config());

  while (!a()->sess().hangup()) {
    bout.cls();
    bout.printfile("QWK");
    std::string allowed = "QCDU?";
    if (so()) {
      allowed.push_back('*');
    }
    bout.nl();
    if (so()) {
      bout.outstr("|#7(|#1*|#7=|#2Sysop Menu|#7,|#1Q|#7=|#2Quit|#7) |#1C|#7, |#1D|#7, |#1U|#7: ");
    } else {
      bout.outstr("|#7(|#1Q|#7=|#2Quit|#7) |#1C|#7, |#1D|#7, |#1U|#7: ");
    }

    switch (const auto key = onek(allowed, true); key) {
    case 'C':
      qwk_config_user();
      break;
    case 'D':
      qwk_download();
    break;
    case 'Q':
      return;
    case 'U':
      qwk_upload();
      break;
    case '*':
      qwk_config_sysop();
      break;
    case '?':
    default:
      break;
    }
  }
}

void qwk_nscan() {
#ifdef NEVER // Not ported yet
#define SETREC(f, i) lseek(f, ((long)(i)) * ((long)sizeof(uploadsrec)), SEEK_SET);
  static constexpr int DOTS = 5;
  uploadsrec u;
  bool abort = false;
  int od, newfile, i, i1, i5, f, count, color = 3;
  char s[201];
  ;

  bout.ansic(3);
  bout.outstr("Building NEWFILES.DAT");

  sprintf(s, "%s%s", a()->sess().dirs().qwk_directory().c_str(), "NEWFILES.DAT");
  newfile = open(s, O_BINARY | O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE;
  if (newfile < 1) {
    bout.outstr("Open Error");
    return;
  }

  for (i = 0; i < num_dirs && !abort; i++) {
    bin.checka(&abort);
    count++;

    bout.printf("%d.", color);
    if (count >= DOTS) {
      bout.outstr("\r");
      bout.outstr("Searching");
      color++;
      count = 0;
      if (color == 4) {
        color++;
      }
      if (color == 10) {
        color = 0;
      }
    }

    i1 = a()->udir[i].subnum;
    if (a()->sess().qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      if ((dir_dates[a()->udir[i].subnum]) &&
          (dir_dates[a()->udir[i].subnum] < a()->sess().nscandate())) {
        continue;
      }

      od = a()->current_user_dir_num();
      a()->set_current_user_dir_num(i);
      dliscan();
      if (this_date >= a()->sess().nscandate()) {
        sprintf(s, "\r\n\r\n%s - #%s, %d %s.\r\n\r\n",
                a()->dirs()[a()->current_user_dir().subnum].name, a()->current_user_dir().keys,
                numf, "files");
        write(newfile, s, strlen(s));

        f = open(dlfn, O_RDONLY | O_BINARY);
        for (i5 = 1; (i5 <= numf) && (!(abort)) && (!a()->sess().hangup()); i5++) {
          SETREC(f, i5);
          read(f, &u, sizeof(uploadsrec));
          if (u.daten >= a()->sess().nscandate()) {
            sprintf(s, "%s %5ldk  %s\r\n", u.filename, (long)bytes_to_k(u.numbytes), u.description);
            write(newfile, s, strlen(s));

#ifndef HUGE_TRAN
            if (u.mask & mask_extended) {
              int pos = 0;
              string ext =
                  a()->current_file_area()->ReadExtentedDescription(u.filename).value_or("");

              if (!ext.empty(0)) {
                int spos = 21, x;

                strcpy(s, "                     ");
                while (ext[pos]) {
                  x = ext[pos];

                  if (x != '\r' && x != '\n' && x > 2) {
                    s[spos] = x;
                  }

                  if (x == '\n' || x == 0) {
                    s[spos] = 0;
                    write(newfile, s, strlen(s));
                    write(newfile, "\r\n", 2);
                    strcpy(s, "                     ");
                    spos = 21;
                  }

                  if (x != '\r' && x != '\n' && x > 2) {
                    ++spos;
                  }

                  ++pos;
                }
              }
            }
#endif

          } else if (!empty()) {
            bin.checka(&abort);
          }
        }
        f = close(f);
      }
      a()->set_current_user_dir_num(od);
    }
  }
  newfile = close(newfile);
#endif // NEVER
}

static std::string qwk_current_text(int pos) {
  static const char* yesorno[] = {"Yes", "No"};

  switch (pos) {
  case 0:
    return a()->user()->data.qwk_dont_scan_mail ? yesorno[1] : yesorno[0];
  case 1:
    return a()->user()->data.qwk_delete_mail ? yesorno[0] : yesorno[1];
  case 2:
    return a()->user()->data.qwk_dontsetnscan ? yesorno[1] : yesorno[0];
  case 3:
    return a()->user()->data.qwk_remove_color ? yesorno[0] : yesorno[1];
  case 4:
    return a()->user()->data.qwk_convert_color ? yesorno[0] : yesorno[1];
  case 5:
    return a()->user()->data.qwk_leave_bulletin ? yesorno[1] : yesorno[0];
  case 6:
    return a()->user()->data.qwk_dontscanfiles ? yesorno[1] : yesorno[0];
  case 7:
    return a()->user()->data.qwk_keep_routing ? yesorno[1] : yesorno[0];
  case 8:
    return qwk_which_zip();
  case 9:
    return qwk_which_protocol();
  case 10: {
    if (!a()->user()->data.qwk_max_msgs_per_sub && !a()->user()->data.qwk_max_msgs) {
      return "Unlimited/Unlimited";
    }
    if (!a()->user()->data.qwk_max_msgs_per_sub) {
      return fmt::format("Unlimited/{}", a()->user()->data.qwk_max_msgs);
    }
    if (!a()->user()->data.qwk_max_msgs) {
      return fmt::format("{}/Unlimited", a()->user()->data.qwk_max_msgs_per_sub);
    }
    return fmt::format("{}/{}", a()->user()->data.qwk_max_msgs,
                       a()->user()->data.qwk_max_msgs_per_sub);
  }
  case 11:
  default:
    return "DONE";
  }
}

void qwk_config_user() {
  sysoplog("Config Options");
  bool done = false;

  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.litebar("QWK Preferences");
    bout.print("|#1A|#9) Include E-Mail            : |#2{}\r\n", qwk_current_text(0));
    bout.print("|#1B|#9) Delete Included E-Mail    : |#2{}\r\n", qwk_current_text(1));
    bout.print("|#1C|#9) Update Last Read Pointer  : |#2{}\r\n", qwk_current_text(2));
    bout.print("|#1D|#9) Remove WWIV color codes   : |#2{}\r\n", qwk_current_text(3));
    bout.print("|#1E|#9) Convert WWIV color to ANSI: |#2{}\r\n", qwk_current_text(4));
    bout.print("|#1F|#9) Include Bulletins         : |#2{}\r\n", qwk_current_text(5));
    // bout.print("|#1G|#9) Scan New Files           : |#2{}\r\n", qwk_current_text(6));
    bout.print("|#1H|#9) Remove Routing Information: |#2{}\r\n", qwk_current_text(7));
    bout.print("|#1I|#9) Default Compression Type  : |#2{}\r\n", qwk_current_text(8));
    bout.print("|#1J|#9) Default Transfer Protocol : |#2{}\r\n", qwk_current_text(9));
    bout.print("|#1K|#9) Max Messages To Include   : |#2{}\r\n", qwk_current_text(10));
    bout.pl("|#1Q|#9) Done");
    bout.nl(2);
    bout.outstr("|#9Select: ");
    int key = onek("QABCDEFGHIJK", true);

    if (key == 'Q') {
      done = true;
    }
    key = key - 'A';

    switch (key) {
    case 0:
      a()->user()->data.qwk_dont_scan_mail = !a()->user()->data.qwk_dont_scan_mail;
      break;
    case 1:
      a()->user()->data.qwk_delete_mail = !a()->user()->data.qwk_delete_mail;
      break;
    case 2:
      a()->user()->data.qwk_dontsetnscan = !a()->user()->data.qwk_dontsetnscan;
      break;
    case 3:
      a()->user()->data.qwk_remove_color = !a()->user()->data.qwk_remove_color;
      break;
    case 4:
      a()->user()->data.qwk_convert_color = !a()->user()->data.qwk_convert_color;
      break;
    case 5:
      a()->user()->data.qwk_leave_bulletin = !a()->user()->data.qwk_leave_bulletin;
      break;
    case 6:
      a()->user()->data.qwk_dontscanfiles = !a()->user()->data.qwk_dontscanfiles;
      break;
    case 7:
      a()->user()->data.qwk_keep_routing = !a()->user()->data.qwk_keep_routing;
      break;
    case 8: {
      qwk_state qj{};
      memset(&qj, 0, sizeof(qwk_state));
      bout.cls();

      const auto arcno = static_cast<unsigned short>(select_qwk_archiver(&qj, 1));
      if (!qj.abort) {
        a()->user()->data.qwk_archive = arcno;
      }
      break;
    }
    case 9: {
      qwk_state qj{};
      bout.cls();

      const auto arcno = select_qwk_protocol(&qj);
      if (!qj.abort) {
        a()->user()->data.qwk_protocol = arcno;
      }
    } break;
    case 10: {
      if (auto max_msgs = a()->user()->data.qwk_max_msgs,
          max_per_sub = a()->user()->data.qwk_max_msgs_per_sub;
          get_qwk_max_msgs(&max_msgs, &max_per_sub)) {
        a()->user()->data.qwk_max_msgs = max_msgs;
        a()->user()->data.qwk_max_msgs_per_sub = max_per_sub;
      }
    } break;
    }
  }
}

void qwk_config_sysop() {
  if (!so()) {
    return;
  }
  sysoplog("Ran Sysop Config");

  auto c = read_qwk_cfg(*a()->config());

  auto done = false;
  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.print("|#21|#9) Hello file     : |#5{}\r\n", c.hello.empty() ? "|#3<None>" : c.hello);
    bout.print("|#22|#9) News file      : |#5{}\r\n", c.news.empty() ? "|#3<None>" : c.news);
    bout.print("|#23|#9) Goodbye file   : |#5{}\r\n", c.bye.empty() ? "|#3<None>" : c.bye);
    auto sn = qwk_system_name(c, a()->config()->system_name());
    bout.print("|#24|#9) Packet name    : |#5{}\r\n", sn);
    const auto max_msgs = c.max_msgs == 0 ? "(Unlimited)" : std::to_string(c.max_msgs);
    bout.print("|#25|#9) Max Msgs/Packet: |#5{}\r\n", max_msgs);
    bout.print("|#26|#9) Modify Bulletins ({})\r\n", c.bulletins.size());
    bout.print("|#2Q|#9) Quit\r\n");
    bout.nl();
    bout.outstr("|#9Selection? ");

    const int x = onek("Q123456\r\n");
    if (x == '1' || x == '2' || x == '3') {
      bout.nl();
      bout.outstr("|#9Enter New Filename:");
      bout.mpl(12);
    }

    switch (x) {
    case '1':
      c.hello = bin.input(12);
      break;
    case '2':
      c.news = bin.input(12);
      break;
    case '3':
      c.bye = bin.input(12);
      break;
    case '4': {
      sn = qwk_system_name(c, a()->config()->system_name());
      write_qwk_cfg(*a()->config(), c);
      bout.nl();
      bout.print("|#9 Current Packet Name:  |#1{}\r\n", sn);
      bout.outstr("|#9Enter new packet name: ");
      bout.mpl(8);
      sn = bin.input_text(sn, 8);
      if (!sn.empty()) {
        c.packet_name = ToStringLowerCase(sn);
      }
      write_qwk_cfg(*a()->config(), c);
    } break;

    case '5': {
      bout.outstr("|#9(|#10=Unlimited|#9) Enter max messages per packet: ");
      bout.mpl(5);
      c.max_msgs = bin.input_number(c.max_msgs, 0, 65535, true);
    } break;
    case '6':
      modify_bulletins(c);
      break;
    default:
      done = true;
    }
  }

  write_qwk_cfg(*a()->config(), c);
}

} // namespace wwiv::bbs::qwk
