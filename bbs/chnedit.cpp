/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/finduser.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void modify_chain(int nCurrentChainum);
void insert_chain(int nCurrentChainum);
void delete_chain(int nCurrentChainum);

static string chaindata(int nCurrentChainum) {
  chainfilerec c = a()->chains[ nCurrentChainum ];
  char chAr = SPACE;

  if (c.ar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & c.ar) {
        chAr = static_cast<char>('A' + i);
      }
    }
  }
  char chAnsiReq = (c.ansir & ansir_ansi) ? 'Y' : 'N';
  return StringPrintf("|#2%2d |#1%-28.28s  |#2%-30.30s |#9%-3d    %1c  %1c",
      nCurrentChainum,
      stripcolors(c.description),
      c.filename,
      c.sl,
      chAnsiReq,
      chAr);
}

static void showchains() {
  bout.cls();
  bool abort = false;
  bout.bpla("|#2NN Description                   Path Name                      SL  ANSI AR", &abort);
  bout.bpla("|#7== ----------------------------  ============================== --- ==== --", &abort);
  for (size_t nChainNum = 0; nChainNum < a()->chains.size() && !abort; nChainNum++) {
    const string s = chaindata(nChainNum);
    bout.bpla(s, &abort);
  }
}

void ShowChainCommandLineHelp() {
  bout << "|#2  Macro   Value\r\n";
  bout << "|#7 ======== =======================================\r\n";
  bout << "|#1   %     |#9 A single \'%\' Character\r\n";
  bout << "|#1   %1    |#9 CHAIN.TXT full pathname (legacy parameter)\r\n";
  bout << "|#1   %A    |#9 CALLINFO.BBS full pathname \r\n";
  bout << "|#1   %C    |#9 CHAIN.TXT full pathname \r\n";
  bout << "|#1   %D    |#9 DORIFOx.DEF full pathname \r\n";
  bout << "|#1   %E    |#9 DOOR32.SYS full pathname \r\n";
  bout << "|#1   %H    |#9 Socket Handle \r\n";
  bout << "|#1   %K    |#9 GFiles Comment File For Archives\r\n";
  bout << "|#1   %M    |#9 Modem Baud Rate\r\n";
  bout << "|#1   %N    |#9 Node (Instance) number\r\n";
  bout << "|#1   %O    |#9 PCBOARD.SYS full pathname\r\n";
  bout << "|#1   %P    |#9 ComPort Number\r\n";
  bout << "|#1   %R    |#9 DOOR.SYS Full Pathname\r\n";
  bout << "|#1   %S    |#9 Com Port Baud Rate\r\n";
  bout << "|#1   %T    |#9 Minutes Remaining\r\n";
  bout << "|#1   %U    |#9 Users Handle (primary name)\r\n";
  bout.nl();
}

static std::string YesNoStringList(bool b, const std::string& yes, const std::string& no) {
  if (b) return yes;
  return no;
}

void modify_chain(int nCurrentChainum) {
  chainregrec r;
  char s[255], s1[255], ch, ch2;
  memset(&r, 0, sizeof(chainregrec));

  chainfilerec c = a()->chains[ nCurrentChainum ];
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    r = a()->chains_reg[ nCurrentChainum ];
  }
  bool done = false;
  do {
    bout.cls();
    bout.litebarf("Editing Chain # %d", nCurrentChainum);

    bout << "|#9A) Description  : |#2" << c.description << wwiv::endl;
    bout << "|#9B) Filename     : |#2" << c.filename << wwiv::endl;
    bout << "|#9C) SL           : |#2" << static_cast<int>(c.sl) << wwiv::endl;
    strcpy(s, "None.");
    if (c.ar != 0) {
      for (int i = 0; i < 16; i++) {
        if ((1 << i) & c.ar) {
          s[0] = static_cast<char>('A' + i);
        }
      }
      s[1] = 0;
    }
    bout << "|#9D) AR           : |#2" << s << wwiv::endl;
    bout << "|#9E) ANSI         : |#2" << ((c.ansir & ansir_ansi) ? "|#6Required" : "|#1Optional") << wwiv::endl;
    bout << "|#9F) DOS Interrupt: |#2" << ((c.ansir & ansir_no_DOS) ? "NOT Used" : "Used") << wwiv::endl;
    bout << "|#9G) Win32 FOSSIL : |#2" << YesNoString((c.ansir & ansir_emulate_fossil) ? true : false) << wwiv::endl;
    bout << "|#9H) Native STDIO : |#2" << YesNoString((c.ansir & ansir_stdio) ? true : false) << wwiv::endl;
    bout << "|#9I) Launch From  : |#2" << YesNoStringList(c.ansir & ansir_temp_dir, "Temp/Node Directory", "BBS Root Directory") << wwiv::endl;
    bout << "|#9J) Local only   : |#2" << YesNoString((c.ansir & ansir_local_only) ? true : false) << wwiv::endl;
    bout << "|#9K) Multi user   : |#2" << YesNoString((c.ansir & ansir_multi_user) ? true : false) << wwiv::endl;
    if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
      User regUser;
      if (r.regby[0]) {
        a()->users()->readuser(&regUser, r.regby[0]);
      }
      bout << "|#9L) Registered by: |#2" << ((r.regby[0]) ? regUser.GetName() : "AVAILABLE") << wwiv::endl;
      for (int i = 1; i < 5; i++) {
        if (r.regby[i] != 0) {
          a()->users()->readuser(&regUser, r.regby[i]);
          bout << string(18, ' ') << regUser.GetName() << wwiv::endl;
        }
      }
      bout << "|#9M) Usage        : |#2" << r.usage << wwiv::endl;
      if (r.maxage == 0 && r.minage == 0) {
        r.maxage = 255;
      }
      bout << "|#9N) Age limit    : |#2" << static_cast<int>(r.minage) << " - " 
        << static_cast<int>(r.maxage) << wwiv::endl;
      bout.nl();
      bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1N|#7,|#1R|#7,|#1[|#7,|#1]|#7) : ";
      ch = onek("QABCDEFGHIJKLMN[]", true);     // removed i
    } else {
      bout.nl();
      bout << "|#9Which (A-K,R,[,],Q) ? ";
      ch = onek("QABCDEFGHIJK[]", true);   // removed i
    }
    switch (ch) {
    case 'Q': {
      done = true;
    } break;
    case '[':
      a()->chains[ nCurrentChainum ] = c;
      if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        a()->chains_reg[ nCurrentChainum ] = r;
      }
      if (--nCurrentChainum < 0) {
        nCurrentChainum = size_int(a()->chains) - 1;
      }
      c = a()->chains[ nCurrentChainum ];
      if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        r = a()->chains_reg[ nCurrentChainum ];
      }
      break;
    case ']':
      a()->chains[ nCurrentChainum ] = c;
      if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        a()->chains_reg[ nCurrentChainum ] = r;
      }
      if (++nCurrentChainum >= size_int(a()->chains)) {
        nCurrentChainum = 0;
      }
      c = a()->chains[ nCurrentChainum ];
      if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        r = a()->chains_reg[ nCurrentChainum ];
      }
      break;
    case 'A':
      bout.nl();
      bout << "|#7New Description? ";
      Input1(s, c.description, 40, true, InputMode::MIXED);
      if (s[0]) {
        strcpy(c.description, s);
      }
      break;
    case 'B':
      bout.cls();
      ShowChainCommandLineHelp();
      bout << "\r\n|#9Enter Command Line.\r\n|#7:";
      Input1(s, c.filename, 79, true, InputMode::MIXED);
      if (s[0] != 0) {
        strcpy(c.filename, s);
      }
      break;
    case 'C': {
      bout.nl();
      bout << "|#7New SL? ";
      input(s, 3, true);
      int sl = to_number<int>(s);
      if ((sl >= 0) && (sl < 256) && (s[0])) {
        c.sl = static_cast<unsigned char>(sl);
      }
    } break;
    case 'D':
      bout.nl();
      bout << "|#7New AR (<SPC>=None) ? ";
      ch2 = onek(" ABCDEFGHIJKLMNOP");
      if (ch2 == SPACE) {
        c.ar = 0;
      } else {
        c.ar = static_cast<uint16_t>(1 << (ch2 - 'A'));
      }
      break;
    case 'E': c.ansir ^= ansir_ansi; break;
    case 'F': c.ansir ^= ansir_no_DOS; break;
    case 'G': c.ansir ^= ansir_emulate_fossil; break;
    case 'H': c.ansir ^= ansir_stdio; break;
    case 'I': c.ansir ^= ansir_temp_dir; break;
    case 'J': c.ansir ^= ansir_local_only; break;
    case 'K':c.ansir ^= ansir_multi_user; break;
    case 'L':
      if (!a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
        break;
      }
      for (int i = 0; i < 5; i++) {
        bout.nl();
        bout << "|#9(Q=Quit, 0=None) User name/number: : ";
        input(s1, 30, true);
        if (s1[0] != 'Q' && s1[0] != 'q') {
          if (s1[0] == '0') {
            r.regby[i] = 0;
          } else {
            int user_number = finduser1(s1);
            if (user_number > 0) {
              User regUser;
              a()->users()->readuser(&regUser, user_number);
              r.regby[i] = static_cast<int16_t>(user_number);
              bout.nl();
              bout << "|#1Registered by       |#2" << user_number << " " 
                   << ((r.regby[i]) ? regUser.GetName() : "AVAILABLE");
            }
          }
        } else {
          break;
        }
      }
      break;
    case 'M':
      r.usage = 0;
      bout.nl();
      bout << "|#5Times Run : ";
      input(s, 3);
      if (s[0] != 0) {
        r.usage = to_number<int16_t>(s);
      }
      break;
    case 'N':
      bout.nl();
      bout << "|#5New minimum age? ";
      input(s, 3);
      if (s[0]) {
        if ((to_number<int>(s) > 255) || (to_number<int>(s) < 0)) {
          s[0] = 0;
        } else {
          r.minage = to_number<uint8_t>(s);
        }
        bout << "|#5New maximum age? ";
        input(s, 3);
        if (s[0]) {
          if (to_number<uint8_t>(s) < r.minage) {
            break;
          }
          if (to_number<uint8_t>(s) > 255) {
            r.maxage = 255;
          } else {
            r.maxage = to_number<uint8_t>(s);
          }
        }
      }
      break;
    }
  } while (!done && !a()->hangup_);
  a()->chains[ nCurrentChainum ] = c;
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    a()->chains_reg[ nCurrentChainum ] = r;
  }
}

void insert_chain(int pos) {
  {
    chainfilerec c{};
    strcpy(c.description, "** NEW CHAIN **");
    strcpy(c.filename, "REM");
    c.sl = 10;
    c.ar = 0;
    c.ansir = 0;
    c.ansir |= ansir_no_DOS;

    insert_at(a()->chains, pos, c);
  }
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    chainregrec r{};
    memset(&r, 0, sizeof(r));
    r.maxage = 255;

    insert_at(a()->chains_reg, pos, r);
  }
  modify_chain(pos);
}

void delete_chain(int nCurrentChainum) {
  erase_at(a()->chains, nCurrentChainum);
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    erase_at(a()->chains_reg, nCurrentChainum);
  }
}

void chainedit() {
  if (!ValidateSysopPassword()) {
    return;
  }
  showchains();
  bool done = false;
  do {
    bout.nl();
    bout << "|#7Chains: (D)elete, (I)nsert, (M)odify, (Q)uit, ? : ";
    char ch = onek("QDIM?");
    switch (ch) {
    case '?':
      showchains();
      break;
    case 'Q':
      done = true;
      break;
    case 'M': {
      bout.nl();
      bout << "|#2Chain number? ";
      string s = input(2);
      int i = to_number<int>(s);
      if (s[0] != '\0' && i >= 0 && i < size_int(a()->chains)) {
        modify_chain(i);
      }
    } break;
    case 'I': {
      if (a()->chains.size() < a()->max_chains) {
        bout.nl();
        bout << "|#2Insert before which chain ('$' for end) : ";
        int chain = 0;
        string s = input(2);
        if (s[0] == '$') {
          chain =  a()->chains.size();
        } else {
          chain = to_number<int>(s);
        }
        if (s[0] != '\0' && chain >= 0 && chain <= size_int(a()->chains)) {
          insert_chain(chain);
        }
      }
    } break;
    case 'D': {
      bout.nl();
      bout << "|#2Delete which chain? ";
      auto s = input(2);
      auto i = to_number<int>(s);
      if (s[0] != '\0' && i >= 0 && i < size_int(a()->chains)) {
        bout.nl();
        bout << "|#5Delete " << a()->chains[i].description << "? ";
        if (yesno()) {
          delete_chain(i);
        }
      }
    } break;
    }
  } while (!done && !a()->hangup_);

  DataFile<chainfilerec> file(FilePath(a()->config()->datadir(), CHAINS_DAT),
    File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
  file.WriteVector(a()->chains);
  if (a()->HasConfigFlag(OP_FLAGS_CHAIN_REG)) {
    DataFile<chainregrec> regFile(FilePath(a()->config()->datadir(), CHAINS_REG),
        File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    if (regFile) {
      regFile.WriteVector(a()->chains_reg);
    }
  }
}
