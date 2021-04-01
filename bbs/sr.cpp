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
#include "bbs/sr.h"

#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/crc.h"
#include "bbs/execexternal.h"
#include "bbs/srrcv.h"
#include "bbs/srsend.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/numbers.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::stl;
using namespace wwiv::strings;

unsigned char checksum = 0;

void calc_CRC(unsigned char b) {
  checksum = checksum + b;

  crc ^= (static_cast<unsigned short>(b) << 8);
  for (auto i = 0; i < 8; i++) {
    if (crc & 0x8000) {
      crc = crc << 1;
      crc ^= 0x1021;
    } else {
      crc = crc << 1;
    }
  }
}


char gettimeout(long ds, bool *abort) {
  if (bin.bkbhitraw()) {
    return bin.bgetchraw();
  }

  const seconds d(ds);
  const auto d1 = steady_clock::now();
  while (steady_clock::now() - d1 < d && !bin.bkbhitraw() && !a()->sess().hangup() && !*abort) {
    if (bout.localIO()->KeyPressed()) {
      if (const auto ch = bout.localIO()->GetChar(); ch == 0) {
        bout.localIO()->GetChar();
      } else if (ch == ESC) {
        *abort = true;
      }
    }
    a()->CheckForHangup();
  }
  return bin.bkbhitraw() ? bin.bgetchraw() : 0;
}


int extern_prot(int num, const std::filesystem::path& path, bool bSending) {
  char s1[81];

  if (bSending) {
    bout.nl();
    bout << "-=> Beginning file transmission, Ctrl+X to abort.\r\n";
    if (num < 0) {
      strcpy(s1, a()->over_intern[(-num) - 1].sendfn);
    } else {
      strcpy(s1, a()->externs[num].sendfn);
    }
  } else {
    bout.nl();
    bout << "-=> Ready to receive, Ctrl+X to abort.\r\n";
    if (num < 0) {
      strcpy(s1, a()->over_intern[(-num) - 1].receivefn);
    } else {
      strcpy(s1, a()->externs[num].receivefn);
    }
  }
  // Use this since fdsz doesn't like 115200
  const auto xfer_speed = std::to_string(std::min<int>(a()->modem_speed_, 57600));
  const auto primary_port= fmt::format("{:d}", a()->primary_port());
  if (const auto command = stuff_in(s1, xfer_speed, primary_port, path.string(), xfer_speed, "");
      !command.empty()) {
    a()->ClearTopScreenProtection();
    ScopeExit at_exit([]{ a()->UpdateTopScreen(); });
    bout.localIO()->Puts(fmt::format("{} is currently online at {} bps\r\n\r\n",
                                     a()->user()->name_and_number(), a()->modem_speed_));
    bout.localIO()->Puts(command);
    bout.localIO()->Puts("\r\n");
    if (a()->sess().incom()) {
      return ExecuteExternalProgram(command, a()->spawn_option(SPAWNOPT_PROT_SINGLE));
    }
  }
  return -5;
}


bool ok_prot(int num, xfertype xt) {
  if (xt == xfertype::xf_none) {
    return false;
  }

  if (num > 0 && num < ssize(a()->externs) + WWIV_NUM_INTERNAL_PROTOCOLS) {
    switch (num) {
    case WWIV_INTERNAL_PROT_ASCII:
      if (xt == xfertype::xf_down || xt == xfertype::xf_down_temp) {
        return true;
      }
      break;
    // Stop using the legacy XModem
    case WWIV_INTERNAL_PROT_XMODEM:
      return false;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
    case WWIV_INTERNAL_PROT_YMODEM:
    case WWIV_INTERNAL_PROT_ZMODEM:
      if (xt != xfertype::xf_up_batch && xt != xfertype::xf_down_batch) {
        return true;
      }
      if (num == WWIV_INTERNAL_PROT_YMODEM && xt == xfertype::xf_down_batch) {
        return true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && xt == xfertype::xf_down_batch) {
        return true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && xt == xfertype::xf_up_temp) {
        return true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && !a()->IsUseInternalZmodem()) {
        // If AllowInternalZmodem is not true, don't allow it.
        return false;
      }
      break;
    case WWIV_INTERNAL_PROT_BATCH:
      if (xt == xfertype::xf_up) {
        for (const auto& e : a()->externs) {
          if (e.receivebatchfn[0]) {
            return true;
          }
        }
      } else if (xt == xfertype::xf_down) {
        for (const auto& e : a()->externs) {
          if (e.sendbatchfn[0]) {
            return true;
          }
        }
      }
      if (xt == xfertype::xf_up || xt == xfertype::xf_down) {
        return true;
      }
      break;
    default:
      switch (xt) {
      case xfertype::xf_up:
      case xfertype::xf_up_temp:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].receivefn[0]) {
          return true;
        }
        break;
      case xfertype::xf_down:
      case xfertype::xf_down_temp:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].sendfn[0]) {
          return true;
        }
        break;
      case xfertype::xf_up_batch:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].receivebatchfn[0]) {
          return true;
        }
        break;
      case xfertype::xf_down_batch:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn[0]) {
          return true;
        }
        break;
      case xfertype::xf_none:
        break;
      }
      break;
    }
  }
  return false;
}

static char prot_key(int num) {
  const auto s = prot_name(num);
  return to_upper_case_char(s.front());
}

std::string prot_name(int num) {
  switch (num) {
  case WWIV_INTERNAL_PROT_ASCII:
    return "ASCII";
  case WWIV_INTERNAL_PROT_XMODEM:
    return "Xmodem";
  case WWIV_INTERNAL_PROT_XMODEMCRC:
    return "Xmodem-CRC";
  case WWIV_INTERNAL_PROT_YMODEM:
    return "Ymodem";
  case WWIV_INTERNAL_PROT_BATCH:
    return "Batch";
  case WWIV_INTERNAL_PROT_ZMODEM:
    return "Zmodem (Internal)";
  default:
    if (num >= WWIV_NUM_INTERNAL_PROTOCOLS &&
        num < (ssize(a()->externs) + WWIV_NUM_INTERNAL_PROTOCOLS)) {
      return a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].description;
    }
    return "-=>NONE<=-";
  }

}

struct AvailableProtocol {
  int protocol_number{-1};
  char key{0};
  std::string name;
};

static constexpr char BASE_CHAR = '!';

static char create_default_key(int i) {
  if (i < 10) {
    return static_cast<char>('0' + i);
  }
  return static_cast<char>(BASE_CHAR + i - 10);
}

class AvailableProtocols {
public:
  AvailableProtocols(xfertype t, const std::vector<newexternalrec>& e, wwiv::sdk::User& user)
      : xt(t), externs(e), u(user) {
    if (ok_prot(u.default_protocol(), xt)) {
      default_protocol = u.default_protocol();
    }

    const auto maxprot = WWIV_NUM_INTERNAL_PROTOCOLS - 1 + ssize(a()->externs);
    std::set<char> override_keys;
    for (auto i = 1; i <= maxprot; i++) {
      if (!ok_prot(i, xt)) {
        continue;
      }
      auto key = prot_key(i);
      if (auto [_, inserted] = override_keys.insert(key); !inserted) {
        key = create_default_key(i);
      }
      allowed_keys.push_back(key);
      AvailableProtocol a;
      a.key = key;
      a.name = prot_name(i);
      a.protocol_number = i;
      prots.emplace_back(a);
    }

    if (!default_protocol) {
      default_protocol = only().value_or(0);
    }
    if (default_protocol) {
      allowed_keys.push_back('\r');
    }
  }

  std::optional<int> only() {
    if (prots.size() != 1) {
      return std::nullopt;
    }
    return prots.front().protocol_number;
  }

  std::optional<int> protocol_for_key(char key) {
    if (key == '0' || key == 'Q') {
      return std::nullopt;
    }
    if (key == '\r') {
      return default_protocol;
    }
    for (const auto& p : prots) {
      if (key == p.key) {
        if (!u.default_protocol()) {
          u.default_protocol(p.protocol_number);
        }
        return {p.protocol_number};
      }
    }
    return std::nullopt;
  }

  int default_protocol{0};
  std::string allowed_keys{"Q?0"};
  std::vector<AvailableProtocol> prots;
  xfertype xt{xfertype::xf_none};
  const std::vector<newexternalrec> &externs;
  wwiv::sdk::User& u;
};

bool show_protocols(const std::vector<AvailableProtocol>& prots) {
  bout << "|#1Available Protocols:\r\n\n";
  bout << "|#9[|#2Q|#9] |#9Abort Transfer(s)\r\n";
  bout << "|#9[|#20|#9] |#9Don't Transfer\r\n";

  for (const auto& p : prots) {
    bout.format("|#9[|#2{}|#9] |#9{}\r\n", p.key, p.name);
  }
  bout.nl(2);
  return true;
}

int get_protocol(xfertype xt) {
  AvailableProtocols avail(xt, a()->externs, *a()->user());
  show_protocols(avail.prots);

  while (true) {
    std::string prompt = "Protocol (?=list) : ";

    if (avail.default_protocol) {
      const auto dname = prot_name(avail.default_protocol);
      prompt = fmt::format("Protocol (?=list, <C/R>={}) : ", dname);
    }

    bout << "|#7" << prompt;
    if (const auto ch = onek(avail.allowed_keys); ch != '?') {
      return avail.protocol_for_key(ch).value_or(-1);
    }
    show_protocols(avail.prots);
  }
}

void ascii_send(const std::filesystem::path& path, bool* sent, double* percent) {

  if (File file(path); file.Open(File::modeBinary | File::modeReadOnly)) {
    auto file_size = file.length();
    file_size = std::max<File::size_type>(file_size, 1);
    char b[2048];
    auto num_read = file.Read(b, 1024);
    auto total_bytes = 0L;
    auto abort = false;
    while (num_read && !a()->sess().hangup() && !abort) {
      auto buffer_pos = 0;
      while (!a()->sess().hangup() && !abort && buffer_pos < num_read) {
        a()->CheckForHangup();
        bout.bputch(b[buffer_pos++]);
        bin.checka(&abort);
      }
      total_bytes += buffer_pos;
      bin.checka(&abort);
      num_read = file.Read(b, 1024);
    }
    file.Close();
    if (!abort) {
      *sent = true;
    } else {
      *sent = false;
      a()->user()->set_dk(a()->user()->dk() + bytes_to_k(total_bytes));
    }
    *percent = static_cast<double>(total_bytes) / static_cast<double>(file_size);
  } else {
    bout.nl();
    bout << "File not found.\r\n\n";
    *sent = false;
    *percent = 0.0;
  }
}

void maybe_internal(const std::filesystem::path& path, bool* xferred, double* percent, bool bSend,
                    int prot) {
  if (!a()->over_intern.empty() 
      && a()->over_intern[prot - 2].othr & othr_override_internal
      && ((bSend && a()->over_intern[prot - 2].sendfn[0]) ||
          (!bSend && a()->over_intern[prot - 2].receivefn[0]))) {
    if (extern_prot(-(prot - 1), path, bSend) == a()->over_intern[prot - 2].ok1) {
      *xferred = true;
    }
    return;
  }
  if (!a()->sess().incom()) {
    bout << "Would use internal " << prot_name(prot) << wwiv::endl;
    return;
  }

  if (bSend) {
    switch (prot) {
    case WWIV_INTERNAL_PROT_XMODEM:
      xymodem_send(path, xferred, percent, false, false, false);
      break;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
      xymodem_send(path, xferred, percent, true, false, false);
      break;
    case WWIV_INTERNAL_PROT_YMODEM:
      xymodem_send(path, xferred, percent, true, true, false);
      break;
    case WWIV_INTERNAL_PROT_ZMODEM:
      zmodem_send(path, xferred, percent);
      break;
    default: ;
    }
  } else {
    switch (prot) {
    case WWIV_INTERNAL_PROT_XMODEM:
      xymodem_receive(path.filename().string(), xferred, false);
      break;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
      [[fallthrough]];
    case WWIV_INTERNAL_PROT_YMODEM:
      xymodem_receive(path.filename().string(), xferred, true);
      break;
    case WWIV_INTERNAL_PROT_ZMODEM:
      *xferred = zmodem_receive(path);
      break;
    default:
      break;
    }
  }
}

void send_file(const std::filesystem::path& path, bool* sent, bool* abort, const std::string& sfn,
               int dn, long fs) {
  *sent = false;
  *abort = false;
  int nProtocol;
  if (fs < 0) {
    nProtocol = get_protocol(xfertype::xf_none);
  } else {
    nProtocol = get_protocol(dn == -1 ? xfertype::xf_down_temp : xfertype::xf_down);
  }
  auto ok = false;
  auto percent = 0.0;
  auto opt = wwiv::sdk::files::FileName::FromUnaligned(sfn);
  if (!opt) {
    LOG(ERROR) << "Failed to align filename: " << sfn;
    *abort = true;
    bout.nl();
    bout << "Internal Error: " << "Failed to align filename: " << sfn;
    return;
  }
  if (a()->batch().contains_file(opt.value())) {
    *sent = false;
    if (nProtocol > 0) {
      bout.nl();
      bout << "That file is already in the batch queue.\r\n\n";
    } else if (nProtocol == -1) {
      *abort = true;
    }
  } else {
    switch (nProtocol) {
    case -1:
      *abort = true;
      ok = true;
      break;
    case 0:
      ok = true;
      break;
    case WWIV_INTERNAL_PROT_ASCII:
      ascii_send(path, sent, &percent);
      break;
    case WWIV_INTERNAL_PROT_XMODEM:
    case WWIV_INTERNAL_PROT_XMODEMCRC:
    case WWIV_INTERNAL_PROT_YMODEM:
    case WWIV_INTERNAL_PROT_ZMODEM:
      maybe_internal(path, sent, &percent, true, nProtocol);
      break;
    case WWIV_INTERNAL_PROT_BATCH:
      ok = true;
      if (a()->batch().size() >= a()->max_batch) {
        bout.nl();
        bout << "No room left in batch queue.\r\n\n";
        *sent = false;
        *abort = false;
      } else {
        if (nsl() <=
            a()->batch().dl_time_in_secs() + time_to_transfer(fs, a()->modem_speed_).count()) {
          bout.nl();
          bout << "Not enough time left in queue.\r\n\n";
          *sent = false;
          *abort = false;
        } else {
          if (dn == -1) {
            bout.nl();
            bout << "Can't add temporary file to batch queue.\r\n\n";
            *sent = false;
            *abort = false;
          } else {
            const BatchEntry b(sfn, dn, fs, true);
            a()->batch().AddBatch(b);
            bout.nl(2);
            bout << "File added to batch queue.\r\n";
            bout << "Batch: Files - " << a()->batch().size()
                 << "  Time - " << ctim(a()->batch().dl_time_in_secs()) << "\r\n\n";
            *sent = false;
            *abort = false;
          }
        }
      }
      break;
    default:
      const int temp_prot = extern_prot(nProtocol - WWIV_NUM_INTERNAL_PROTOCOLS, path, true);
      *abort = false;
      if (temp_prot == a()->externs[nProtocol - WWIV_NUM_INTERNAL_PROTOCOLS].ok1) {
        *sent = true;
      }
      break;
    }
  }
  if (!*sent && !ok) {
    if (percent == 1.0) {
      *sent = true;
    } else {
      const auto fn = path.filename().string();
      sysoplog() << fmt::sprintf("Tried D/L \"%s\" %3.2f%%", fn, percent * 100.0);
    }
  }
}

void receive_file(const std::filesystem::path& path, int* received, const std::string& sfn, int dn) {
  switch (const auto prot = get_protocol(dn == -1 ? xfertype::xf_up_temp : xfertype::xf_up); prot) {
  case 0:
  case -1:
    *received = 0;
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM: {
    bool xferred;
    maybe_internal(path, &xferred, nullptr, false, prot);
    *received = xferred ? 1 : 0;
  }
  break;
  case WWIV_INTERNAL_PROT_BATCH:
    if (dn != -1) {
      if (a()->batch().size() >= a()->max_batch) {
        bout.nl();
        bout << "No room left in batch queue.\r\n\n";
        *received = 0;
      } else {
        *received = 2;
        const BatchEntry b(sfn, dn, 0, false);
        a()->batch().AddBatch(b);
        bout.nl();
        bout << "File added to batch queue.\r\n\n";
        bout << "Batch upload: files - " << a()->batch().numbatchul() << "\r\n\n";
      }
    } else {
      bout.nl();
      bout << "Can't batch upload that.\r\n\n";
    }
    break;
  default:
    if (prot > WWIV_NUM_INTERNAL_PROTOCOLS - 1 && a()->sess().incom()) {
      extern_prot(prot - WWIV_NUM_INTERNAL_PROTOCOLS, path, false);
      *received = File::Exists(path);
    }
    break;
  }
}
