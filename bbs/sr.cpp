/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/crc.h"
#include "bbs/datetime.h"
#include "bbs/execexternal.h"
#include "bbs/srrcv.h"
#include "bbs/srsend.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using std::string;
using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

unsigned char checksum = 0;

void calc_CRC(unsigned char b) {
  checksum = checksum + b;

  crc ^= (((unsigned short)(b)) << 8);
  for (int i = 0; i < 8; i++) {
    if (crc & 0x8000) {
      crc = (crc << 1);
      crc ^= 0x1021;
    } else {
      crc = (crc << 1);
    }
  }
}


char gettimeout(long ds, bool *abort) {
  if (bkbhitraw()) {
    return bgetchraw();
  }

  seconds d(ds);
  auto d1 = steady_clock::now();
  while (steady_clock::now() - d1 < d && !bkbhitraw() && !a()->hangup_ && !*abort) {
    if (a()->localIO()->KeyPressed()) {
      char ch = a()->localIO()->GetChar();
      if (ch == 0) {
        a()->localIO()->GetChar();
      } else if (ch == ESC) {
        *abort = true;
      }
    }
    CheckForHangup();
  }
  return (bkbhitraw()) ? bgetchraw() : 0;
}


int extern_prot(int num, const std::string& send_filename, bool bSending) {
  char s1[81], s2[81], sx1[21], sx2[21], sx3[21];

  if (bSending) {
    bout.nl();
    bout << "-=> Beginning file transmission, Ctrl+X to abort.\r\n";
    if (num < 0) {
      strcpy(s1, a()->over_intern[(-num) - 1].sendfn);
    } else {
      strcpy(s1, (a()->externs[num].sendfn));
    }
  } else {
    bout.nl();
    bout << "-=> Ready to receive, Ctrl+X to abort.\r\n";
    if (num < 0) {
      strcpy(s1, a()->over_intern[(-num) - 1].receivefn);
    } else {
      strcpy(s1, (a()->externs[num].receivefn));
    }
  }
  // Use this since fdsz doesn't like 115200
  auto xfer_speed = std::min<int>(a()->modem_speed_, 57600);
  sprintf(sx1, "%d", xfer_speed);
  sprintf(sx3, "%d", xfer_speed);
  sx2[0] = '0' + a()->primary_port();
  sx2[1] = '\0';
  const auto command = stuff_in(s1, sx1, sx2, send_filename, sx3, "");
  if (!command.empty()) {
    a()->ClearTopScreenProtection();
    const string unn = a()->names()->UserName(a()->usernum);
    sprintf(s2, "%s is currently online at %u bps", unn.c_str(), a()->modem_speed_);
    a()->localIO()->Puts(s2);
    a()->localIO()->Puts("\r\n\r\n");
    a()->localIO()->Puts(command);
    a()->localIO()->Puts("\r\n");
    if (a()->context().incom()) {
      int nRetCode = ExecuteExternalProgram(command, a()->spawn_option(SPAWNOPT_PROT_SINGLE));
      a()->UpdateTopScreen();
      return nRetCode;
    } else {
      a()->UpdateTopScreen();
      return -5;
    }
  }
  return -5;
}


bool ok_prot(int num, xfertype xt) {
  bool ok = false;

  if (xt == xf_none) {
    return false;
  }

  if (num > 0 && num < (size_int(a()->externs) + WWIV_NUM_INTERNAL_PROTOCOLS)) {
    switch (num) {
    case WWIV_INTERNAL_PROT_ASCII:
      if (xt == xf_down || xt == xf_down_temp) {
        ok = true;
      }
      break;
    // Stop using the legacy XModem
    case WWIV_INTERNAL_PROT_XMODEM:
      ok = false;
      break;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
    case WWIV_INTERNAL_PROT_YMODEM:
    case WWIV_INTERNAL_PROT_ZMODEM:
      if (xt != xf_up_batch && xt != xf_down_batch) {
        ok = true;
      }
      if (num == WWIV_INTERNAL_PROT_YMODEM && xt == xf_down_batch) {
        ok = true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && xt == xf_down_batch) {
        ok = true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && xt == xf_up_temp) {
        ok = true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && xt == xf_up_batch) {
        // TODO(rushfan): Internal ZModem doesn't support internal batch uploads
        // ok = true;
      }
      if (num == WWIV_INTERNAL_PROT_ZMODEM && !a()->IsUseInternalZmodem()) {
        // If AllowInternalZmodem is not true, don't allow it.
        ok = false;
      }
      break;
    case WWIV_INTERNAL_PROT_BATCH:
      if (xt == xf_up) {
        for (const auto& e : a()->externs) {
          if (e.receivebatchfn[0]) {
            ok = true;
          }
        }
      } else if (xt == xf_down) {
        for (const auto& e : a()->externs) {
          if (e.sendbatchfn[0]) {
            ok = true;
          }
        }
      }
      if (xt == xf_up || xt == xf_down) {
        ok = true;
      }
      break;
    default:
      switch (xt) {
      case xf_up:
      case xf_up_temp:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].receivefn[0]) {
          ok = true;
        }
        break;
      case xf_down:
      case xf_down_temp:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].sendfn[0]) {
          ok = true;
        }
        break;
      case xf_up_batch:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].receivebatchfn[0]) {
          ok = true;
        }
        break;
      case xf_down_batch:
        if (a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].sendbatchfn[0]) {
          ok = true;
        }
        break;
      case xf_none:
        break;
      }
      break;
    }
  }
  return ok;
}

static char prot_key(int num) {
  const auto s = prot_name(num);
  return upcase(s[0]);
}

std::string prot_name(int num) {
  switch (num) {
  case WWIV_INTERNAL_PROT_ASCII:
    return "ASCII";
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
    return "Xmodem";
    break;
  case WWIV_INTERNAL_PROT_XMODEMCRC:
    return "Xmodem-CRC";
    break;
  case WWIV_INTERNAL_PROT_YMODEM:
    return "Ymodem";
    break;
  case WWIV_INTERNAL_PROT_BATCH:
    return "Batch";
    break;
  case WWIV_INTERNAL_PROT_ZMODEM:
    return "Zmodem (Internal)";
  default:
    if (num >= WWIV_NUM_INTERNAL_PROTOCOLS &&
        num < (size_int(a()->externs) + WWIV_NUM_INTERNAL_PROTOCOLS)) {
      return a()->externs[num - WWIV_NUM_INTERNAL_PROTOCOLS].description;
    }
    return "-=>NONE<=-";
  }

}


#define BASE_CHAR '!'

int get_protocol(xfertype xt) {
  char oks[81], ch, oks1[81], ch1, fl[80];
  int prot;

  if (ok_prot(a()->user()->GetDefaultProtocol(), xt)) {
    prot = a()->user()->GetDefaultProtocol();
  } else {
    prot = 0;
  }

  int cyColorSave = a()->user()->color(8);
  a()->user()->SetColor(8, 1);
  int oks1p = 0;
  oks1[0] = '\0';
  strcpy(oks, "Q?0");
  int i1 = strlen(oks);
  int only = 0;
  int maxprot = (WWIV_NUM_INTERNAL_PROTOCOLS - 1) + a()->externs.size();
  for (int i = 1; i <= maxprot; i++) {
    fl[i] = '\0';
    if (ok_prot(i, xt)) {
      if (i < 10) {
        oks[i1++] = static_cast<char>('0' + i);
      } else {
        oks[i1++] = static_cast<char>(BASE_CHAR + i - 10);
      }
      if (only == 0) {
        only = i;
      } else {
        only = -1;
      }
      if (i >= 3) {
        ch1 = prot_key(i);
        if (strchr(oks1, ch1) == 0) {
          oks1[oks1p++] = ch1;
          oks1[oks1p] = 0;
          fl[i] = 1;
        }
      }
    }
  }
  oks[i1] = 0;
  strcat(oks, oks1);
  if (only > 0) {
    prot = only;
  }

  if (only == 0 && xt != xf_none) {
    bout.nl();
    bout << "No protocols available for that.\r\n\n";
    return -1;
  }
  std::string allowable{oks};
  std::string prompt = "Protocol (?=list) : ";
  
  if (prot) {
    const auto ss = prot_name(prot);
    prompt = fmt::format("Protocol (?=list, <C/R>={}}) : ", ss);
    allowable.push_back('\r');
  }
  if (!prot) {
    bout.nl();
    bout << "|#3Available Protocols :|#1\r\n\n";
    bout << "|#8[|#7Q|#8] |#1Abort Transfer(s)\r\n";
    bout << "|#8[|#70|#8] |#1Don't Transfer\r\n";
    for (int j = 1; j <= maxprot; j++) {
      if (ok_prot(j, xt)) {
        ch1 = prot_key(j);
        if (fl[j] == 0) {
          bout.bprintf("|#8[|#7%c|#8] |#1%s\r\n", (j < 10) ? (j + '0') : (j + BASE_CHAR - 10),
                                            prot_name(j));
        } else {
          bout.bprintf("|#8[|#7%c|#8] |#1%s\r\n", prot_key(j), prot_name(j));
        }
      }
    }
    bout.nl();
  }
  bool done = false;
  do {
    bout.nl();
    bout << "|#7" << prompt;
    ch = onek(allowable);
    if (ch == '?') {
      bout.nl();
      bout << "|#3Available Protocols :|#1\r\n\n";
      bout << "|#8[|#7Q|#8] |#1Abort Transfer(s)\r\n";
      bout << "|#8[|#70|#8] |#1Don't Transfer\r\n";
      for (int j = 1; j <= maxprot; j++) {
        if (ok_prot(j, xt)) {
          ch1 = prot_key(j);
          if (fl[ j ] == 0) {
            bout.bprintf("|#8[|#7%c|#8] |#1%s\r\n", (j < 10) ? (j + '0') : (j + BASE_CHAR - 10),
                                              prot_name(j));
          } else {
            bout.bprintf("|#8[|#7%c|#8] |#1%s\r\n", prot_key(j), prot_name(j));
          }
        }
      }
      bout.nl();
    } else {
      done = true;
    }
  } while (!done && !a()->hangup_);
  a()->user()->SetColor(8, cyColorSave);
  if (ch == RETURN) {
    return prot;
  }
  if (ch >= '0' && ch <= '9') {
    if (ch != '0') {
      a()->user()->SetDefaultProtocol(ch - '0');
    }
    return ch - '0';
  } else {
    if (ch == 'Q') {
      return -1;
    } else {
      i1 = ch - BASE_CHAR + 10;
      a()->user()->SetDefaultProtocol(i1);
      if (i1 < size_int(a()->externs) + WWIV_NUM_INTERNAL_PROTOCOLS) {
        return ch - BASE_CHAR + 10;
      }
      for (size_t j = 3; j < a()->externs.size() + WWIV_NUM_INTERNAL_PROTOCOLS; j++) {
        if (prot_key(j) == ch) {
          return j;
        }
      }
    }
  }
  return -1;
}

void ascii_send(const std::string& file_name, bool* sent, double* percent) {
  char b[2048];

  File file(file_name);
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    auto file_size = file.length();
    file_size = std::max<off_t>(file_size, 1);
    auto num_read = file.Read(b, 1024);
    long lTotalBytes = 0L;
    bool abort = false;
    while (num_read && !a()->hangup_ && !abort) {
      int nBufferPos = 0;
      while (!a()->hangup_ && !abort && nBufferPos < num_read) {
        CheckForHangup();
        bout.bputch(b[nBufferPos++]);
        checka(&abort);
      }
      lTotalBytes += nBufferPos;
      checka(&abort);
      num_read = file.Read(b, 1024);
    }
    file.Close();
    if (!abort) {
      *sent = true;
    } else {
      *sent = false;
      a()->user()->SetDownloadK(a()->user()->GetDownloadK() + bytes_to_k(lTotalBytes));
    }
    *percent = static_cast<double>(lTotalBytes) / static_cast<double>(file_size);
  } else {
    bout.nl();
    bout << "File not found.\r\n\n";
    *sent = false;
    *percent = 0.0;
  }
}

void maybe_internal(const std::string& file_name, bool* xferred, double* percent, bool bSend,
                    int prot) {
  if (a()->over_intern.size() > 0 
      && (a()->over_intern[prot - 2].othr & othr_override_internal)
      && ((bSend && a()->over_intern[prot - 2].sendfn[0]) ||
          (!bSend && a()->over_intern[prot - 2].receivefn[0]))) {
    if (extern_prot(-(prot - 1), file_name, bSend) == a()->over_intern[prot - 2].ok1) {
      *xferred = true;
    }
    return;
  }
  if (!a()->context().incom()) {
    bout << "Would use internal " << prot_name(prot) << wwiv::endl;
    return;
  }

  if (bSend) {
    switch (prot) {
    case WWIV_INTERNAL_PROT_XMODEM:
      xymodem_send(file_name, xferred, percent, false, false, false);
      break;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
      xymodem_send(file_name, xferred, percent, true, false, false);
      break;
    case WWIV_INTERNAL_PROT_YMODEM:
      xymodem_send(file_name, xferred, percent, true, true, false);
      break;
    case WWIV_INTERNAL_PROT_ZMODEM:
      zmodem_send(file_name, xferred, percent);
      break;
    }
  } else {
    switch (prot) {
    case WWIV_INTERNAL_PROT_XMODEM:
      xymodem_receive(file_name, xferred, false);
      break;
    case WWIV_INTERNAL_PROT_XMODEMCRC:
    case WWIV_INTERNAL_PROT_YMODEM:
      xymodem_receive(file_name, xferred, true);
      break;
    case WWIV_INTERNAL_PROT_ZMODEM:
      zmodem_receive(file_name, xferred);
      break;
    }
  }
}

void send_file(const std::string& file_name, bool* sent, bool* abort, const std::string& sfn,
               int dn,
               long fs) {
  *sent = false;
  *abort = false;
  int nProtocol = 0;
  if (fs < 0) {
    nProtocol = get_protocol(xf_none);
  } else {
    nProtocol = (dn == -1) ? get_protocol(xf_down_temp) : get_protocol(xf_down);
  }
  bool ok = false;
  double percent = 0.0;
  if (check_batch_queue(sfn.c_str())) {
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
      ascii_send(file_name, sent, &percent);
      break;
    case WWIV_INTERNAL_PROT_XMODEM:
    case WWIV_INTERNAL_PROT_XMODEMCRC:
    case WWIV_INTERNAL_PROT_YMODEM:
    case WWIV_INTERNAL_PROT_ZMODEM:
      maybe_internal(file_name, sent, &percent, true, nProtocol);
      break;
    case WWIV_INTERNAL_PROT_BATCH:
      ok = true;
      if (a()->batch().entry.size() >= a()->max_batch) {
        bout.nl();
        bout << "No room left in batch queue.\r\n\n";
        *sent = false;
        *abort = false;
      } else {
        double t = (a()->modem_speed_) ? (12.656) / ((double)(a()->modem_speed_)) * ((double)(fs)) : 0;
        if (nsl() <= (a()->batch().dl_time_in_secs() + t)) {
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
            batchrec b{};
            to_char_array(b.filename, sfn);
            b.dir = static_cast<int16_t>(dn);
            b.time = static_cast<float>(t);
            b.sending = true;
            b.len = fs;
            a()->batch().entry.emplace_back(b);
            bout.nl(2);
            bout << "File added to batch queue.\r\n";
            bout << "Batch: Files - " << a()->batch().entry.size()
                 << "  Time - " << ctim(a()->batch().dl_time_in_secs()) << "\r\n\n";
            *sent = false;
            *abort = false;
          }
        }
      }
      break;
    default:
      int nTempProt = extern_prot(nProtocol - WWIV_NUM_INTERNAL_PROTOCOLS, file_name, true);
      *abort = false;
      if (nTempProt == a()->externs[nProtocol - WWIV_NUM_INTERNAL_PROTOCOLS].ok1) {
        *sent = true;
      }
      break;
    }
  }
  if (!*sent && !ok) {
    if (percent == 1.0) {
      *sent = true;
    } else {
      sysoplog() << fmt::sprintf("Tried D/L \"%s\" %3.2f%%", stripfn(file_name), percent * 100.0);
    }
  }
}

void receive_file(const std::string& file_name, int* received, const std::string& sfn, int dn) {
  bool bReceived;
  int nProtocol = (dn == -1) ? get_protocol(xf_up_temp) : get_protocol(xf_up);

  switch (nProtocol) {
  case -1:
    *received = 0;
    break;
  case 0:
    *received = 0;
    break;
  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM: {
    maybe_internal(file_name, &bReceived, nullptr, false, nProtocol);
    *received = (bReceived) ? 1 : 0;
  }
  break;
  case WWIV_INTERNAL_PROT_BATCH:
    if (dn != -1) {
      if (a()->batch().entry.size() >= a()->max_batch) {
        bout.nl();
        bout << "No room left in batch queue.\r\n\n";
        *received = 0;
      } else {
        *received = 2;
        batchrec b{};
        to_char_array(b.filename, sfn);
        b.dir = static_cast<int16_t>(dn);
        b.time = 0;
        b.sending = false;
        b.len = 0;
        a()->batch().entry.emplace_back(b);
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
    if (nProtocol > (WWIV_NUM_INTERNAL_PROTOCOLS - 1) && a()->context().incom()) {
      extern_prot(nProtocol - WWIV_NUM_INTERNAL_PROTOCOLS, file_name, false);
      *received = File::Exists(file_name);
    }
    break;
  }
}

char end_batch1() {
  char b[128];

  memset(b, 0, 128);

  bool done = false;
  int  nerr = 0;
  bool bAbort = false;
  char ch = 0;
  do {
    send_block(b, 5, 1, 0);
    ch = gettimeout(5, &bAbort);
    if (ch == CF || ch == CX) {
      done = true;
    } else {
      ++nerr;
      if (nerr >= 9) {
        done = true;
      }
    }
  } while (!done && !a()->hangup_ && !bAbort);
  if (ch == CF) {
    return CF;
  }
  if (ch == CX) {
    return CX;
  }
  return CU;
}


void endbatch() {
  bool abort = false;
  int terr = 0;
  int oldx = a()->localIO()->WhereX();
  int oldy = a()->localIO()->WhereY();
  bool ucrc = false;
  if (!okstart(&ucrc, &abort)) {
    abort = true;
  }
  if (!abort && !a()->hangup_) {
    char ch = end_batch1();
    if (ch == CX) {
      abort = true;
    }
    if (ch == CU) {
      const auto fn = PathFilePath(a()->temp_directory(),
                               StrCat(".does-not-exist-", a()->instance_number(), ".$$$"));
      File::Remove(fn);
      File nullFile(fn);
      send_b(nullFile, 0L, 3, 0, &ucrc, "", &terr, &abort);
      abort = true;
      File::Remove(fn);
    }
  }
  a()->localIO()->GotoXY(oldx, oldy);
}
