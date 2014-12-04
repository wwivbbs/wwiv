/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <string>

#include "wwiv.h"
#include "printfile.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "bbs/keycodes.h"

using std::string;

static char ShowBBSListMenuAndGetChoice() {
  bout.nl();
  if (so()) {
    bout <<
                       "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9]: (|#1R|#9)ead, (|#1A|#9)dd, (|#1D|#9)elete, (|#1N|#9)et : ";
    return onek("QRNAD");
  } else {
    bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2BBS list|#9] (|#1R|#9)ead, (|#1A|#9)dd, (|#1N|#9)et : ";
    return onek("QRNA");
  }
}

static bool IsBBSPhoneNumberUnique(const string& phoneNumber) {
  bool ok = true;
  File file(syscfg.gfilesdir, BBSLIST_MSG);
  if (file.Open(File::modeReadOnly | File::modeBinary)) {
    file.Seek(0L, File::seekBegin);
    long lBbsListLength = file.GetLength();
    char *ss = static_cast<char *>(BbsAllocA(lBbsListLength + 500L));
    if (ss == nullptr) {
      file.Close();
      return true;
    }
    file.Read(ss, lBbsListLength);
    long lBbsListPos = 0L;
    while (lBbsListPos < lBbsListLength && ok) {
      char szBbsListLine[255];
      int i = 0;
      char ch = '\0';
      do {
        ch = ss[ lBbsListPos++ ];
        szBbsListLine[i] = ch;
        if (ch == '\r') {
          szBbsListLine[i] = '\0';
        }
        ++i;
      } while (ch != '\n' && i < 120 && lBbsListPos < lBbsListLength);
      if (strstr(szBbsListLine, phoneNumber.c_str()) != nullptr) {
        ok = false;
      }
      if (strncmp(szBbsListLine, phoneNumber.c_str(), 12) == 0) {
        ok = false;
      }
    }
    free(ss);
    file.Close();
  }
  return ok;
}

static bool IsBBSPhoneNumberValid(const string& phoneNumber) {
  if (phoneNumber.empty()) {
    return false;
  }
  if (phoneNumber[3] != '-' || phoneNumber[7] != '-') {
    return false;
  }
  if (phoneNumber.length() != 12) {
    return false;
  }
  for (auto iter = phoneNumber.begin(); iter != phoneNumber.end(); iter++) {
    if (strchr("0123456789-", (*iter)) == 0) {
      return false;
    }
  }
  return true;
}

static void AddBBSListLine(const string bbsListLine) {
  File file(syscfg.gfilesdir, BBSLIST_MSG);
  bool bOpen = file.Open(File::modeReadWrite | File::modeCreateFile | File::modeBinary);
  if (bOpen && file.GetLength() > 0) {
    file.Seek(-1L, File::seekEnd);
    char chLastChar = 0;
    file.Read(&chLastChar, 1);
    if (chLastChar == CZ) {
      // If last char is a EOF, skip it.
      file.Seek(-1L, File::seekEnd);
    }
  }
  file.Write(bbsListLine.c_str(), bbsListLine.length());
  file.Close();
}

static void AddBBSListEntryImpl() {
  bout << "\r\nPlease enter phone number:\r\n ###-###-####\r\n:";
  string bbsPhoneNumber;
  input(&bbsPhoneNumber, 12, true);
  if (IsBBSPhoneNumberValid(bbsPhoneNumber.c_str())) {
    if (IsBBSPhoneNumberUnique(bbsPhoneNumber.c_str())) {
      string bbsName, bbsSpeed, bbsType;
      bout << "|#3This number can be added! It is not yet in BBS list.\r\n\n\n"
                         << "|#7Enter the BBS name and comments about it (incl. V.32/HST) :\r\n:";
      inputl(&bbsName, 50, true);
      bout << "\r\n|#7Enter maximum speed of the BBS:\r\n"
                         << "|#7(|#1example: 14.4,28.8, 33.6, 56k|#7)\r\n:";
      input(&bbsSpeed, 4, true);
      bout << "\r\n|#7Enter BBS type (ie, |#1WWIV|#7):\r\n:";
      input(&bbsType, 4, true);

      char szBbsListLine[ 255 ];
      snprintf(szBbsListLine, sizeof(szBbsListLine), "%12s  %-50s  [%4s] (%4s)\r\n",
               bbsPhoneNumber.c_str(), bbsName.c_str(), bbsSpeed.c_str(), bbsType.c_str());
      bout.nl(2);
      bout << szBbsListLine;
      bout.nl(2);
      bout << "|#5Is this information correct? ";
      if (yesno()) {
        AddBBSListLine(szBbsListLine);
        bout << "\r\n|#3This entry was added to BBS list.\r\n";
      }
      bout.nl();
    } else {
      bout << "|#6Sorry, It's already in the BBS list.\r\n\n\n";
    }
  } else {
    bout << "\r\n|#6 Error: Please enter number in correct format.\r\n\n";
  }
}

static void AddBBSListEntry() {
  if (session()->GetEffectiveSl() <= 10) {
    bout << "\r\n\nYou must be a validated user to add to the BBS list.\r\n\n";
  } else if (session()->user()->IsRestrictionAutomessage()) {
    bout << "\r\n\nYou can not add to the BBS list.\r\n\n\n";
  } else {
    AddBBSListEntryImpl();
  }

}

static void DeleteBBSListEntry() {
  bout << "\r\n|#7Please enter phone number in the following format:\r\n";
  bout << "|#1 ###-###-####\r\n:";
  string bbsPhoneNumber;
  input(&bbsPhoneNumber, 12, true);
  if (bbsPhoneNumber.length() != 12 || bbsPhoneNumber[3] != '-' || bbsPhoneNumber[7] != '-') {
    bout << "\r\n|#6Error: Please enter number in correct format.\r\n";
    return;
  }

  bool ok = false;
  TextFile fi(syscfg.gfilesdir, BBSLIST_MSG, "r");
  TextFile fo(syscfg.gfilesdir, BBSLIST_TMP, "w");
  if (fi.IsOpen()) {
    if (fo.IsOpen()) {
      string line;
      while (fi.ReadLine(&line)) {
        if (strstr(line.c_str(), bbsPhoneNumber.c_str())) {
          ok = true;
        } else {
          fo.WriteLine(line);
        }
      }
      fo.Close();
    }
    fi.Close();
  }
  bout.nl();
  if (ok) {
    File::Remove(fi.full_pathname());
    File::Rename(fo.full_pathname(), fi.full_pathname());
    bout << "|#7* |#1Number removed.\r\n";
  } else {
    File::Remove(fo.full_pathname());
    bout << "|#6Error: Couldn't find that in the bbslist file.\r\n";
  }
}

void LegacyBBSList() {
  bool done = false;
  do {
    char chInput = ShowBBSListMenuAndGetChoice();
    switch (chInput) {
    case 'Q':
      done = true;
      break;
    case 'R':
      printfile(BBSLIST_MSG, true, true);
      break;
    case 'N':
      print_net_listing(false);
      break;
    case 'A':
      AddBBSListEntry();
      break;
    case 'D':
      DeleteBBSListEntry();
      break;
    }
  } while (!done && !hangup);
}
