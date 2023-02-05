/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2022, WWIV Software Services                  */
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

// WWIV5 LNet
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "lnet/lnet.h"
#include "local_io/local_io.h"
#include "local_io/local_io_curses.h"
#include "local_io/local_io_win32.h"
#include "localui/curses_io.h"
#include "net_core/net_cmdline.h"
#include "sdk/config.h"
#include "sdk/fido/fido_util.h"
#include "sdk/net/packets.h"
#include "sdk/status.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;


static LocalIO* CreateLocalIO() {
#if defined(_WIN32) && !defined(WWIV_WIN32_CURSES_IO)
  return new Win32ConsoleIO();
#else
  wwiv::local::ui::CursesIO::Init(fmt::sprintf("WWIV BBS %s", full_version()));
  return new CursesLocalIO(wwiv::local::ui::curses_out->GetMaxY(),
                                            wwiv::local::ui::curses_out->GetMaxX());
#endif
}

LNet::LNet(const wwiv::net::NetworkCommandLine& cmdline)
    : net_cmdline_(cmdline), io(CreateLocalIO()) {}

LNet::~LNet() {
  delete io;
  io = nullptr;

  // Cleanup CursesIO.
  using namespace wwiv::local::ui;
  if (curses_out != nullptr) {
    delete curses_out;
    curses_out = nullptr;
  }
}

void LNet::dump_char(char ch) {

  switch (ch) {
  case 0:
    io->Putch(' ');
    break;
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 11:
  case 12:
  case 14:
  case 15:
  case 16:
  case 17:
  case 18:
  case 19:
  case 20:
  case 21:
  case 22:
  case 23:
  case 24:
  case 25:
  case 26:
    io->Format("[^{:c}]", static_cast<char>('@' + ch));
    break;
  case 27:
    io->Puts("[ESC]");
    break;
  case 28:
  case 29:
  case 30:
  case 31:
    io->Format("[#{:d}]", ch);
    break;
  case 10:
    ++curli_;
    io->Putch(ch);
    if (curli_ > 24) {
      pausescr();
    }
    break;
  default:
    io->Putch(ch);
    break;
  }
}

static std::string minor_type_to_string(uint16_t main_type, uint16_t minor_type) {
  if (main_type == main_type_net_info) {
    return net_info_minor_type_name(minor_type);
  } else if (main_type > 0) {
    return std::to_string(minor_type);
  }
  return "0";
}

void LNet::show_help() {
  const auto help_text = R"(

R - Read message.
D - Delete message.
N - Next message.
T - Re-Read message.
W - Write message to new file.
] - 10 messages forward.
} - 50 messages forward.
' - 500 messages forward
X - Truncate file (used for bad messages)
Q - Quit.


)";
  io->Puts(help_text);
}

void LNet::show_header(const net_header_rec& nh, int current, double percent) {
  io->Format("Entry {:3} ({:03}%): ", current, percent);

  if (nh.main_type == 65535) {
    io->Puts(">DELETED<\n\n");
  } else {
    io->Format("{} ({})", main_type_name(nh.main_type),
               minor_type_to_string(nh.main_type, nh.minor_type));
    if (nh.fromuser) {
      io->Format(", From #{} @{}", nh.fromuser, nh.fromsys);
    } else {
      io->Format(", From @{}", nh.fromsys);
    }
    io->Format(", {} bytes", nh.length);
    const auto now = wwiv::core::daten_t_now();
    const auto days_old = (int)((now - nh.daten) / 24 / 3600);
    io->Format(", {} day{} old", days_old, days_old > 1 ? "s" : "");

    if (nh.touser) {
      io->Format("\n                  To #{} @{}\r\n\n", nh.touser, nh.tosys);
    } else {
      io->Format("\n                  To @{}\r\n\n", nh.tosys);
    }
  }
}

static int skip_messages(NetMailFile& file, int num_to_skip) {
  int skipped = 0;
  while (--num_to_skip >= 0) {
    auto [packet, resp] = file.Read();
    if (resp != ReadNetPacketResponse::OK) {
      return skipped;
    }
    ++skipped;
  }
  return skipped;
}

void LNet::pausescr() { 
  auto saved = io->curatr(); 
  io->curatr(13);
  io->Puts("[PAUSE]");
  io->GetChar();
  io->Cr();
  io->Lf();
  io->curatr(saved);
  curli_ = 0;
}

int LNet::Run() {
  const std::filesystem::path filename = net_cmdline_.cmdline().remaining().front();
  NetMailFile file(filename, true, true);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << filename << "; error: " << file.file().last_error();
    return 1;
  }

  auto current{0};
  bool packet_modified{false};
  const auto file_length = file.file().length();
  for (;;) {
    io->Cls();
    auto [packet, response] = file.Read();
    if (response == ReadNetPacketResponse::END_OF_FILE) {
      return 0;
    } else if (response != ReadNetPacketResponse::OK) {
      return 1;
    }
    ++current;
    const auto percent = static_cast<double>(packet.offset() / file_length);
    show_header(packet.nh, current, std::floor(percent * 100));

    auto prompt_done = false;
    while (!prompt_done) {
      io->Format("Action (R,D,N,T,Q,?) ? ");

      char key = upcase(io->GetChar());

      switch (key) {
      case '?': {
        show_help();
      } break;
      case ']':
        current += skip_messages(file, 9);
        prompt_done = true;
        break;
      case '}':
        current += skip_messages(file, 49);
        prompt_done = true;
        break;
      case '\'':
        current += skip_messages(file, 499);
        prompt_done = true;
        break;
      case 'D': {
        // TODO: Mark message 65536
        auto saved_position = packet.offset();
        file.file().Seek(saved_position, wwiv::core::File::Whence::begin);
        packet.nh.main_type = 65535; // Mark this packet deleted.
        const auto num = file.file().Write(&packet.nh, sizeof(net_header_rec));
        if (num != sizeof(net_header_rec)) {
          // Let's fail now since we didn't write this right.
          LOG(ERROR) << "Error deleting packet; num written (" << num
                     << ") != net_header_rec size.";
          io->GetChar();
          prompt_done = true;
          break;
        }
        file.file().Seek(saved_position, wwiv::core::File::Whence::begin);
        --current;
        prompt_done = true;
        packet_modified = true;
      } break;
      case 'N':
        prompt_done = true;
        break;
      case 'Q': {
        if (packet_modified) {
          file.Close();
          // Move the pending file into the same directory as P1-L-###.NET
          const auto dir = filename.parent_path();
          rename_pend(dir, filename.filename().string(), 'L');
        }
        return 0;
      } break;
      case 'R': {
        if (packet.nh.length) {
          io->Cr();
          io->Lf();
          ++curli_;
          for (const auto ch : packet.text()) {
            dump_char(ch);
          }
          io->Cr();
          io->Lf();
        }
      } break;
      case 'T': {
        // Reread message
        file.file().Seek(packet.offset(), wwiv::core::File::Whence::begin);
        --current;
        prompt_done = true;
      } break;
      case 'W': {
        // Write message to new file.

        // Back up.
        file.file().Seek(packet.offset(), wwiv::core::File::Whence::begin);
        --current;
        prompt_done = true;
        std::string fn;
        io->Puts("Enter Filename: ");
        const auto r = io->EditLine(fn, 12, AllowedKeys::ALL);
        io->Cr();
        io->Lf();
        if (r == EditlineResult::ABORTED) {
          io->Puts("Aborted");
          break;
        }
        const auto dir = filename.parent_path();
        write_wwivnet_packet(FilePath(dir, fn), packet);
 
      } break;
      }
    } // !prompt_done
  }
}
