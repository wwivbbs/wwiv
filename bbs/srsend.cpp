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
#include "bbs/bbs.h"
#include "bbs/crc.h"
#include "bbs/sr.h"
#include "bbs/xfer.h"
#include "common/datetime.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/numbers.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "sdk/files/file_record.h"

#include <chrono>
#include <cmath>
#include <string>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;

// TODO(rushfan) Make zmwwiv.h?
bool NewZModemSendFile(const std::filesystem::path& path);

// from sr.cpp
extern unsigned char checksum;

void send_block(char *b, int block_type, bool use_crc, char byBlockNumber) {
  int nBlockSize = 0;

  a()->CheckForHangup();
  switch (block_type) {
  case 5:
    nBlockSize = 128;
    bout.rputch(1);
    break;
  case 4:
    bout.rputch('\x81');
    bout.rputch(byBlockNumber);
    bout.rputch(byBlockNumber ^ 0xff);
    break;
  case 3:
    bout.rputch(CX);
    break;
  case 2:
    bout.rputch(4);
    break;
  case 1:
    nBlockSize = 1024;
    bout.rputch(2);
    break;
  case 0:
    nBlockSize = 128;
    bout.rputch(1);
  }
  if (block_type > 1 && block_type < 5) {
    return;
  }

  bout.rputch(byBlockNumber);
  bout.rputch(byBlockNumber ^ 0xff);
  crc = 0;
  checksum = 0;
  for (int i = 0; i < nBlockSize; i++) {
    const char ch = b[i];
    bout.rputch(ch);
    calc_CRC(ch);
  }

  if (use_crc) {
    bout.rputch(static_cast<char>(crc >> 8));
    bout.rputch(static_cast<char>(crc & 0x00ff));
  } else {
    bout.rputch(checksum);
  }
  bout.dump();
}

char send_b(File &file, long pos, int block_type, char byBlockNumber, bool *use_crc, const wwiv::sdk::files::FileName& file_name,
            int *terr, bool *abort) {
  char b[1025];

  int nb = 0;
  if (block_type == 0) {
    nb = 128;
  }
  if (block_type == 1) {
    nb = 1024;
  }
  if (nb) {
    file.Seek(pos, File::Whence::begin);
    const int num_read = file.Read(b, nb);
    for (auto i = num_read; i < nb; i++) {
      b[i] = '\0';
    }
  } else if (block_type == 5) {
    memset(b, 0, 128);
    nb = 128;
    to_char_array(b, file_name.unaligned_filename());
    // We needed this cast to (long) to compile with XCode 1.5 on OS X
    const auto sb = fmt::sprintf("%ld %ld", pos, static_cast<long>(file.last_write_time()));

    strcpy(&b[strlen(b) + 1], sb.c_str());
    b[127] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) >> 8);
    b[126] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) & 0x00ff);
  }
  bool done = false;
  int nNumErrors = 0;
  char ch = 0;
  do {
    send_block(b, block_type, *use_crc, byBlockNumber);
    ch = gettimeout(5, abort);
    if (ch == 'C' && pos == 0) {
      *use_crc = true;
    }
    if (ch == 6 || ch == CX) {
      done = true;
    } else {
      ++nNumErrors;
      ++(*terr);
      if (nNumErrors >= 9) {
        done = true;
      }
      bout.localIO()->PutsXY(69, 4, std::to_string(nNumErrors));
      bout.localIO()->PutsXY(69, 5, std::to_string(*terr));
    }
  } while (!done && !a()->sess().hangup() && !*abort);

  if (ch == 6) {
    return 6;
  }
  if (ch == CX) {
    return CX;
  }
  return CU;
}

bool okstart(bool *use_crc, bool *abort) {
  auto d = steady_clock::now();
  bool ok = false;
  bool done = false;

  const seconds s90(90);
  while (steady_clock::now() - d < s90 && !done && !a()->sess().hangup() && !*abort) {
    const char ch = gettimeout(91, abort);
    if (ch == 'C') {
      *use_crc = true;
      ok = true;
      done = true;
    }
    if (ch == CU) {
      *use_crc = false;
      ok = true;
      done = true;
    }
    if (ch == CX) {
      ok = false;
      done = true;
    }
  }
  return ok;
}

static int GetXYModemBlockSize(bool bBlockSize1K) {
  return bBlockSize1K ? 1024 : 128;
}

void xymodem_send(const std::filesystem::path& path, bool *sent, double *percent, bool use_crc, bool use_ymodem,
                  bool use_ymodemBatch) {
  char ch;

  long cp = 0L;
  char byBlockNumber = 1;
  bool abort = false;
  int terr = 0;
  wwiv::sdk::files::FileName fn(wwiv::sdk::files::align(path.filename().string()));

  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    if (!use_ymodemBatch) {
      bout << "\r\nFile not found.\r\n\n";
    }
    *sent = false;
    *percent = 0.0;
    return;
  }
  auto file_size = file.length();
  if (!file_size) {
    file_size = 1;
  }

  const auto tpb = 12.656f / static_cast<double>(a()->modem_speed_);

  if (!use_ymodemBatch) {
    bout << "\r\n-=> Beginning file transmission, Ctrl+X to abort.\r\n";
  }
  int xx1 = bout.localIO()->WhereX();
  int yy1 = bout.localIO()->WhereY();
  bout.localIO()->PutsXY(52, 0, "\xB3 Filename :               ");
  bout.localIO()->PutsXY(52, 1, "\xB3 Xfer Time:               ");
  bout.localIO()->PutsXY(52, 2, "\xB3 File Size:               ");
  bout.localIO()->PutsXY(52, 3, "\xB3 Cur Block: 1 - 1k        ");
  bout.localIO()->PutsXY(52, 4, "\xB3 Consec Errors: 0         ");
  bout.localIO()->PutsXY(52, 5, "\xB3 Total Errors : 0         ");
  bout.localIO()->PutsXY(52, 6,
                                       "\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");
  bout.localIO()->PutsXY(65, 0, path.filename().string());
  bout.localIO()->PutsXY(65, 2,
                         fmt::format("{} - {}k", (file_size + 127) / 128, bytes_to_k(file_size)));

  if (!okstart(&use_crc, &abort)) {
    abort = true;
  }
  if (use_ymodem && !abort && !a()->sess().hangup()) {
    ch = send_b(file, file_size, 5, 0, &use_crc, fn, &terr, &abort);
    if (ch == CX) {
      abort = true;
    }
    if (ch == CU) {
      send_b(file, 0L, 3, 0, &use_crc, fn, &terr, &abort);
      abort = true;
    }
  }
  bool bUse1kBlocks = false;
  while (!a()->sess().hangup() && !abort && cp < file_size) {
    bUse1kBlocks = (use_ymodem) ? true : false;
    if ((file_size - cp) < 128L) {
      bUse1kBlocks = false;
    }
    bout.localIO()->PutsXY(65, 3, fmt::sprintf("%ld - %ldk", cp / 128 + 1, cp / 1024 + 1));
    const std::string t = ctim(std::lround((file_size - cp) * tpb));
    bout.localIO()->PutsXY(65, 1, t);
    bout.localIO()->PutsXY(69, 4, "0");

    ch = send_b(file, cp, (bUse1kBlocks) ? 1 : 0, byBlockNumber, &use_crc, fn, &terr,
                &abort);
    if (ch == CX) {
      abort = true;
    } else if (ch == CU) {
      sleep_for(seconds(1));
      bout.dump();
      send_b(file, 0L, 3, 0, &use_crc, fn, &terr, &abort);
      abort = true;
    } else {
      ++byBlockNumber;
      cp += GetXYModemBlockSize(bUse1kBlocks);
    }
  }
  if (!a()->sess().hangup() && !abort) {
    send_b(file, 0L, 2, 0, &use_crc, fn, &terr, &abort);
  }
  if (!abort) {
    *sent = true;
    *percent = 1.0;
  } else {
    *sent = false;
    cp += GetXYModemBlockSize(bUse1kBlocks);
    if (cp >= file_size) {
      *percent = 1.0;
    } else {
      cp -= GetXYModemBlockSize(bUse1kBlocks);
      *percent = static_cast<double>(cp) / static_cast<double>(file_size);
    }
  }
  file.Close();
  bout.localIO()->GotoXY(xx1, yy1);
  if (*sent && !use_ymodemBatch) {
    bout << "-=> File transmission complete.\r\n\n";
  }
}

void zmodem_send(const std::filesystem::path& path, bool *sent, double *percent) {
  *sent = false;
  *percent = 0.0;

  const auto old_binary_mode = bout.remoteIO()->binary_mode();
  bout.remoteIO()->set_binary_mode(true);
  const auto result = NewZModemSendFile(path);
  bout.remoteIO()->set_binary_mode(old_binary_mode);

  if (result) {
    *sent = true;
    *percent = 100.0;
  }
}

