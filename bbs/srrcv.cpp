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
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/crc.h"
#include "bbs/sr.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/numbers.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "sdk/files/file_record.h"
#include <chrono>
#include <string>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::strings;

// From zmwwiv.cpp
bool NewZModemReceiveFile(const std::filesystem::path& path);

// from sr.cpp
extern unsigned char checksum;

char modemkey(int* tout) {
  if (bin.bkbhitraw()) {
    char ch = bin.bgetchraw();
    calc_CRC(ch);
    return ch;
  }
  if (*tout) {
    return 0;
  }
  auto d1 = steady_clock::now();
  while (steady_clock::now() - d1 < milliseconds(500) && !bin.bkbhitraw() && !a()->sess().hangup()) {
    a()->CheckForHangup();
  }
  if (bin.bkbhitraw()) {
    char ch = bin.bgetchraw();
    calc_CRC(ch);
    return ch;
  }
  *tout = 1;
  return 0;
}

int receive_block(char* b, unsigned char* bln, bool use_crc) {
  bool abort = false;
  unsigned char ch = gettimeout(5, &abort);
  int err = 0;

  if (abort) {
    return CF;
  }
  int tout = 0;
  if (ch == 0x81) {
    unsigned char bn = modemkey(&tout);
    unsigned char bn1 = modemkey(&tout);
    if ((bn ^ bn1) == 0xff) {
      b[0] = bn;
      *bln = bn;
      return 8;
    } else {
      return 3;
    }
  } else if (ch == 1) {
    unsigned char bn = modemkey(&tout);
    unsigned char bn1 = modemkey(&tout);
    if ((bn ^ bn1) != 0xff) {
      err = 3;
    }
    *bln = bn;
    crc = 0;
    checksum = 0;
    for (int i = 0; (i < 128) && (!a()->sess().hangup()); i++) {
      b[i] = modemkey(&tout);
    }
    if (!use_crc && !a()->sess().hangup()) {
      unsigned char cs1 = checksum;
      bn1 = modemkey(&tout);
      if (bn1 != cs1) {
        err = 2;
      }
    } else if (!a()->sess().hangup()) {
      int cc1 = crc;
      bn = modemkey(&tout);
      bn1 = modemkey(&tout);
      if ((bn != (unsigned char)(cc1 >> 8)) || (bn1 != (unsigned char)(cc1 & 0x00ff))) {
        err = 2;
      }
    }
    if (tout) {
      return 7;
    }
    return err;
  } else if (ch == 2) {
    unsigned char bn = modemkey(&tout);
    unsigned char bn1 = modemkey(&tout);
    crc = 0;
    checksum = 0;
    if ((bn ^ bn1) != 0xff) {
      err = 3;
    }
    *bln = bn;
    for (int i = 0; (i < 1024) && (!a()->sess().hangup()); i++) {
      b[i] = modemkey(&tout);
    }
    if (!use_crc && !a()->sess().hangup()) {
      unsigned char cs1 = checksum;
      bn1 = modemkey(&tout);
      if (bn1 != cs1) {
        err = 2;
      }
    } else if (!a()->sess().hangup()) {
      int cc1 = crc;
      bn = modemkey(&tout);
      bn1 = modemkey(&tout);
      if ((bn != (unsigned char)(cc1 >> 8)) || (bn1 != (unsigned char)(cc1 & 0x00ff))) {
        err = 2;
      }
    }
    if (tout) {
      return 7;
    }
    return (err == 0) ? 1 : err;
  } else if (ch == CX) {
    return 4;
  } else if (ch == 4) {
    return 5;
  } else if (ch == 0) {
    return 7;
  } else {
    return 9;
  }
}

void xymodem_receive(const std::string& file_name, bool* received, bool use_crc) {
  char b[1025], x[81], ch;
  unsigned char bln;

  File::Remove(file_name);
  bool ok = true;
  bool lastcan = false;
  bool lasteot = false;
  int nTotalErrors = 0;
  int nConsecErrors = 0;

  File file(file_name);
  if (!file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    bout << "\r\n\nDOS error - Can't create file.\r\n\n";
    *received = false;
    return;
  }
  long pos = 0L;
  long reallen = 0L;
  time_t filedatetime = 0L;
  unsigned int bn = 1;
  bool done = false;
  bout << "\r\n-=> Ready to receive, Ctrl+X to abort.\r\n";
  int nOldXPos = bout.localIO()->WhereX();
  int nOldYPos = bout.localIO()->WhereY();
  bout.localIO()->PutsXY(52, 0, "\xB3 Filename :               ");
  bout.localIO()->PutsXY(52, 1, "\xB3 Xfer Time:               ");
  bout.localIO()->PutsXY(52, 2, "\xB3 File Size:               ");
  bout.localIO()->PutsXY(52, 3, "\xB3 Cur Block: 1 - 1k        ");
  bout.localIO()->PutsXY(52, 4, "\xB3 Consec Errors: 0         ");
  bout.localIO()->PutsXY(52, 5, "\xB3 Total Errors : 0         ");
  bout.localIO()->PutsXY(52, 6,
                         "\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
                         "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");
  bout.localIO()->PutsXY(65, 0, stripfn(file_name));
  int nNumStartTries = 0;
  do {
    if (nNumStartTries++ > 9) {
      *received = false;
      file.Close();
      File::Remove(file_name);
      return;
    }
    if (use_crc) {
      bout.rputch('C');
    } else {
      bout.rputch(CU);
    }

    auto d1 = steady_clock::now();
    while (steady_clock::now() - d1 < seconds(10) && !bin.bkbhitraw() && !a()->sess().hangup()) {
      a()->CheckForHangup();
      if (bout.localIO()->KeyPressed()) {
        ch = bout.localIO()->GetChar();
        if (ch == 0) {
          bout.localIO()->GetChar();
        } else if (ch == ESC) {
          done = true;
          ok = false;
        }
      }
    }
  } while (!bin.bkbhitraw() && !a()->sess().hangup());

  int i = 0;
  do {
    bln = 255;
    bout.localIO()->PutsXY(69, 4, fmt::sprintf("%d  ", nConsecErrors));
    bout.localIO()->PutsXY(69, 5, std::to_string(nTotalErrors));
    bout.localIO()->PutsXY(65, 3, fmt::sprintf("%ld - %ldk", pos / 128 + 1, pos / 1024 + 1));
    const auto tpb = time_to_transfer(reallen-pos, a()->modem_speed_);
    const auto t = ctim(tpb);
    if (reallen) {
      bout.localIO()->PutsXY(65, 1, t);
    }
    i = receive_block(b, &bln, use_crc);
    if (i == 0 || i == 1) {
      if (bln == 0 && pos == 0L) {
        int i1 = ssize(b) + 1;
        int i3 = i1;
        while (b[i3] >= '0' && b[i3] <= '9' && (i3 - i1) < 15) {
          x[i3 - i1] = b[i3];
          i3++;
        }
        x[i3 - i1] = '\0';
        reallen = to_number<long>(x);
        bout.localIO()->PutsXY(
            65, 2, fmt::sprintf("%ld - %ldk", (reallen + 127) / 128, bytes_to_k(reallen)));
        while ((b[i1] != SPACE) && (i1 < 64)) {
          ++i1;
        }
        if (b[i1] == SPACE) {
          ++i1;
          while ((b[i1] >= '0') && (b[i1] <= '8')) {
            filedatetime = (filedatetime * 8) + static_cast<long>(b[i1] - '0');
            ++i1;
          }
        }
        bout.rputch(CF);
      } else if ((bn & 0x00ff) == static_cast<unsigned int>(bln)) {
        file.Seek(pos, File::Whence::begin);
        long lx = reallen - pos;
        int i2 = (i == 0) ? 128 : 1024;
        if ((static_cast<long>(i2) > lx) && reallen) {
          i2 = static_cast<int>(lx);
        }
        file.Write(b, i2);
        pos += static_cast<long>(i2);
        ++bn;
        bout.rputch(CF);
      } else if (((bn - 1) & 0x00ff) == static_cast<unsigned int>(bln)) {
        bout.rputch(CF);
      } else {
        bout.rputch(CX);
        ok = false;
        done = true;
      }
      nConsecErrors = 0;
    } else if (i == 2 || i == 7 || i == 3) {
      if (pos == 0L && reallen == 0L && use_crc) {
        bout.rputch('C');
      } else {
        bout.rputch(CU);
      }
      ++nConsecErrors;
      ++nTotalErrors;
      if (nConsecErrors > 9) {
        bout.rputch(CX);
        ok = false;
        done = true;
      }
    } else if (i == CF) {
      ok = false;
      done = true;
      bout.rputch(CX);
    } else if (i == 4) {
      if (lastcan) {
        ok = false;
        done = true;
        bout.rputch(CF);
      } else {
        lastcan = true;
        bout.rputch(CU);
      }
    } else if (i == 5) {
      lasteot = true;
      if (lasteot) {
        done = true;
        bout.rputch(CF);
      } else {
        lasteot = true;
        bout.rputch(CU);
      }
    } else if (i == 8) {
      // This used to be where the filetype was set.
      //*ft = bln;
      bout.rputch(CF);
      nConsecErrors = 0;
    } else if (i == 9) {
      bout.dump();
    }
    if (i != 4) {
      lastcan = false;
    }
    if (i != 5) {
      lasteot = false;
    }
  } while (!a()->sess().hangup() && !done);
  bout.localIO()->GotoXY(nOldXPos, nOldYPos);
  if (ok) {
    file.Close();
    if (filedatetime) {
      file.set_last_write_time(filedatetime);
    }
    *received = true;
  } else {
    file.Close();
    File::Remove(file_name);
    *received = false;
  }
}

bool zmodem_receive(const std::filesystem::path& path) {

  const auto saved_mode = bout.remoteIO()->binary_mode();
  bout.remoteIO()->set_binary_mode(true);
  ScopeExit at_exit([=]{bout.remoteIO()->set_binary_mode(saved_mode);});

  auto newpath = path;
  const auto local_filename(wwiv::sdk::files::unalign(path.filename().string()));
  newpath.replace_filename(local_filename);
  return NewZModemReceiveFile(newpath);
}
