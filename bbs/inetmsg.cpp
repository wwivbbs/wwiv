/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/inetmsg.h"

#include "bbs/bbsovl3.h"
#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/fcns.h"
#include "bbs/input.h"
#include "bbs/misccmd.h"
#include "bbs/instmsg.h"
#include "bbs/quote.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


static unsigned char translate_table[] = {
    "................................_!.#$%&.{}*+.-.{0123456789..{=}?"
    "_abcdefghijklmnopqrstuvwxyz{}}-_.abcdefghijklmnopqrstuvwxyz{|}~."
    "cueaaaaceeeiiiaaelaooouuyouclypfaiounnao?__..!{}................"
    "................................abfneouyo0od.0en=+}{fj*=oo..n2.."
};

void get_user_ppp_addr() {
  session()->internetFullEmailAddress = "";
  int network_number = getnetnum("FILEnet");
  if (network_number == -1) {
    return;
  }
  set_net_num(network_number);
  session()->internetFullEmailAddress = StringPrintf("%s@%s",
      session()->internetEmailName.c_str(),
      session()->internetEmailDomain.c_str());
  TextFile acctFile(session()->network_directory(), ACCT_INI, "rt");
  char szLine[260];
  bool found = false;
  if (acctFile.IsOpen()) {
    while (acctFile.ReadLine(szLine, 255) && !found) {
      if (strncasecmp(szLine, "USER", 4) == 0) {
        int nUserNum = atoi(&szLine[4]);
        if (nUserNum == session()->usernum) {
          char* ss = strtok(szLine, "=");
          ss = strtok(nullptr, "\r\n");
          if (ss) {
            while (ss[0] == ' ') {
              strcpy(ss, &ss[1]);
            }
            StringTrimEnd(ss);
            if (ss) {
              session()->internetFullEmailAddress = ss;
              found = true;
            }
          }
        }
      }
    }
    acctFile.Close();
  }
  if (!found && !session()->internetPopDomain.empty()) {
    int j = 0;
    char szLocalUserName[255];
    strcpy(szLocalUserName, session()->user()->GetName());
    for (int i = 0; (i < GetStringLength(szLocalUserName)) && (i < 61); i++) {
      if (szLocalUserName[i] != '.') {
        szLine[ j++ ] = translate_table[(int)szLocalUserName[i] ];
      }
    }
    szLine[ j ] = '\0';
    session()->internetFullEmailAddress = StringPrintf("%s@%s", szLine,
        session()->internetPopDomain.c_str());
  }
}

void send_inet_email() {
  if (session()->user()->GetNumEmailSentToday() > getslrec(session()->GetEffectiveSl()).emails) {
    bout.nl();
    bout << "|#6Too much mail sent today.\r\n";
    return;
  }
  write_inst(INST_LOC_EMAIL, 0, INST_FLAGS_NONE);
  int network_number = getnetnum("FILEnet");
  session()->set_net_num(network_number);
  if (network_number == -1) {
    return;
  }
  set_net_num(session()->net_num());
  bout.nl();
  bout << "|#9Your Internet Address:|#1 "
       << (session()->IsInternetUseRealNames() ? session()->user()->GetRealName() : session()->user()->GetName())
       << " <" << session()->internetFullEmailAddress << ">";
  bout.nl(2);
  bout << "|#9Enter the Internet mail destination address.\r\n|#7:";
  session()->net_email_name = inputl(75, true);
  if (check_inet_addr(session()->net_email_name)) {
    unsigned short user_number = 0;
    unsigned short system_number = 32767;
    irt[0] = 0;
    irt_name[0] = 0;
    grab_quotes(nullptr, nullptr);
    if (user_number || system_number) {
      email("", user_number, system_number, false, 0);
    }
  } else {
    bout.nl();
    if (!session()->net_email_name.empty()) {
      bout << "|#6Invalid address format!\r\n";
    } else {
      bout << "|#6Aborted.\r\n";
    }
  }
}

bool check_inet_addr(const std::string& a) {
  return !a.empty() && (contains(a, '@') && contains(a, '.'));
}

void read_inet_addr(std::string& internet_address, int user_number) {
  internet_address.clear();
  if (!user_number) {
    return ;
  }

  if (user_number == session()->usernum && check_inet_addr(session()->user()->GetEmailAddress())) {
    internet_address = session()->user()->GetEmailAddress();
  } else {
    File file(session()->config()->datadir(), INETADDR_DAT);
    if (!file.Exists()) {
      file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      auto size = syscfg.maxusers * 80;
      auto zero = std::make_unique<char[]>(size);
      file.Write(zero.get(), size);
    } else {
      char szUserName[255];
      file.Open(File::modeReadOnly | File::modeBinary);
      file.Seek(80 * user_number, File::Whence::begin);
      file.Read(szUserName, 80L);
      if (check_inet_addr(szUserName)) {
        internet_address = szUserName;
      } else {
        internet_address = StrCat("User #", user_number);
        User user;
        session()->users()->ReadUser(&user, user_number);
        user.SetEmailAddress("");
        session()->users()->WriteUser(&user, user_number);
      }
    }
    file.Close();
  }
}

void write_inet_addr(const std::string& internet_address, int user_number) {
  if (!user_number) {
    return; /*nullptr;*/
  }

  File inetAddrFile(session()->config()->datadir(), INETADDR_DAT);
  inetAddrFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  long lCurPos = 80L * static_cast<long>(user_number);
  inetAddrFile.Seek(lCurPos, File::Whence::begin);
  inetAddrFile.Write(internet_address.c_str(), 80L);
  inetAddrFile.Close();
  char szDefaultUserAddr[255];
  sprintf(szDefaultUserAddr, "USER%d", user_number);
  session()->set_net_num(getnetnum("FILEnet"));
  if (session()->net_num() != -1) {
    set_net_num(session()->net_num());
    TextFile in(session()->network_directory(), ACCT_INI, "rt");
    TextFile out(session()->temp_directory(), ACCT_INI, "wt+");
    if (in.IsOpen() && out.IsOpen()) {
      char szLine[260];
      while (in.ReadLine(szLine, 255)) {
        char szSavedLine[260];
        bool match = false;
        strcpy(szSavedLine, szLine);
        char* ss = strtok(szLine, "=");
        if (ss) {
          StringTrim(ss);
          if (IsEqualsIgnoreCase(szLine, szDefaultUserAddr)) {
            match = true;
          }
        }
        if (!match) {
          out.WriteFormatted(szSavedLine);
        }
      }
      out.WriteFormatted("\nUSER%d = %s", user_number, internet_address.c_str());
      in.Close();
      out.Close();
    }
    File::Remove(in.full_pathname());
    copyfile(out.full_pathname(), in.full_pathname(), false);
    File::Remove(out.full_pathname());
  }
}
