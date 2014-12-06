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

#include "bbs/wwiv.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "core/strings.h"
#include "core/textfile.h"

//////////////////////////////////////////////////////////////////////////////
//
//
// module private functions
//
//


unsigned char *valid_name(unsigned char *s);


static unsigned char translate_table[] = { \
                                           "................................_!.#$%&.{}*+.-.{0123456789..{=}?" \
                                           "_abcdefghijklmnopqrstuvwxyz{}}-_.abcdefghijklmnopqrstuvwxyz{|}~." \
                                           "cueaaaaceeeiiiaaelaooouuyouclypfaiounnao?__..!{}................" \
                                           "................................abfneouyo0od.0en=+}{fj*=oo..n2.."
                                         };



unsigned char *valid_name(unsigned char *s) {
  static unsigned char szName[60];

  unsigned int j = 0;
  for (unsigned int i = 0; (i < strlen(reinterpret_cast<char*>(s))) && (i < 81); i++) {
    if (s[i] != '.') {
      szName[j++] = translate_table[s[i]];
    }
  }
  szName[j] = 0;
  return szName;
}


void get_user_ppp_addr() {
  session()->internetFullEmailAddress = "";
  bool found = false;
  int nNetworkNumber = getnetnum("FILEnet");
  session()->SetNetworkNumber(nNetworkNumber);
  if (nNetworkNumber == -1) {
    return;
  }
  set_net_num(session()->GetNetworkNumber());
  session()->internetFullEmailAddress = wwiv::strings::StringPrintf("%s@%s",
      session()->internetEmailName.c_str(),
      session()->internetEmailDomain.c_str());
  TextFile acctFile(session()->GetNetworkDataDirectory(), ACCT_INI, "rt");
  char szLine[ 260 ];
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
    char szLocalUserName[ 255 ];
    strcpy(szLocalUserName, session()->user()->GetName());
    for (int i = 0; (i < wwiv::strings::GetStringLength(szLocalUserName)) && (i < 61); i++) {
      if (szLocalUserName[ i ] != '.') {
        szLine[ j++ ] = translate_table[(int)szLocalUserName[ i ] ];
      }
    }
    szLine[ j ] = '\0';
    session()->internetFullEmailAddress = wwiv::strings::StringPrintf("%s@%s", szLine,
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
  int nNetworkNumber = getnetnum("FILEnet");
  session()->SetNetworkNumber(nNetworkNumber);
  if (nNetworkNumber == -1) {
    return;
  }
  set_net_num(session()->GetNetworkNumber());
  bout.nl();
  bout << "|#9Your Internet Address:|#1 " <<
                     (session()->IsInternetUseRealNames() ? session()->user()->GetRealName() :
                      session()->user()->GetName()) <<
                     " <" << session()->internetFullEmailAddress << ">";
  bout.nl(2);
  bout << "|#9Enter the Internet mail destination address.\r\n|#7:";
  inputl(net_email_name, 75, true);
  if (check_inet_addr(net_email_name)) {
    unsigned short nUserNumber = 0;
    unsigned short nSystemNumber = 32767;
    irt[0] = 0;
    irt_name[0] = 0;
    grab_quotes(nullptr, nullptr);
    if (nUserNumber || nSystemNumber) {
      email(nUserNumber, nSystemNumber, false, 0);
    }
  } else {
    bout.nl();
    if (net_email_name[0]) {
      bout << "|#6Invalid address format!\r\n";
    } else {
      bout << "|#6Aborted.\r\n";
    }
  }
}


bool check_inet_addr(const char *inetaddr) {
  if (!inetaddr || !*inetaddr) {
    return false;
  }

  char szBuffer[80];

  strcpy(szBuffer, inetaddr);
  char *p = strchr(szBuffer, '@');
  if (p == nullptr || strchr(p, '.') == nullptr) {
    return false;
  }
  return true;
}


char *read_inet_addr(char *pszInternetEmailAddress, int nUserNumber) {
  if (!nUserNumber) {
    return nullptr;
  }

  if (nUserNumber == session()->usernum && check_inet_addr(session()->user()->GetEmailAddress())) {
    strcpy(pszInternetEmailAddress, session()->user()->GetEmailAddress());
  } else {
    //pszInternetEmailAddress = nullptr;
    *pszInternetEmailAddress = 0;
    File inetAddrFile(syscfg.datadir, INETADDR_DAT);
    if (!inetAddrFile.Exists()) {
      inetAddrFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      for (int i = 0; i <= syscfg.maxusers; i++) {
        long lCurPos = 80L * static_cast<long>(i);
        inetAddrFile.Seek(lCurPos, File::seekBegin);
        inetAddrFile.Write(pszInternetEmailAddress, 80L);
      }
    } else {
      char szUserName[ 255 ];
      inetAddrFile.Open(File::modeReadOnly | File::modeBinary);
      long lCurPos = 80L * static_cast<long>(nUserNumber);
      inetAddrFile.Seek(lCurPos, File::seekBegin);
      inetAddrFile.Read(szUserName, 80L);
      if (check_inet_addr(szUserName)) {
        strcpy(pszInternetEmailAddress, szUserName);
      } else {
        sprintf(pszInternetEmailAddress, "User #%d", nUserNumber);
        WUser user;
        application()->users()->ReadUser(&user, nUserNumber);
        user.SetEmailAddress("");
        application()->users()->WriteUser(&user, nUserNumber);
      }
    }
    inetAddrFile.Close();
  }
  return pszInternetEmailAddress;
}


void write_inet_addr(const char *pszInternetEmailAddress, int nUserNumber) {
  if (!nUserNumber) {
    return; /*nullptr;*/
  }

  File inetAddrFile(syscfg.datadir, INETADDR_DAT);
  inetAddrFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  long lCurPos = 80L * static_cast<long>(nUserNumber);
  inetAddrFile.Seek(lCurPos, File::seekBegin);
  inetAddrFile.Write(pszInternetEmailAddress, 80L);
  inetAddrFile.Close();
  char szDefaultUserAddr[ 255 ];
  sprintf(szDefaultUserAddr, "USER%d", nUserNumber);
  session()->SetNetworkNumber(getnetnum("FILEnet"));
  if (session()->GetNetworkNumber() != -1) {
    set_net_num(session()->GetNetworkNumber());
    TextFile in(session()->GetNetworkDataDirectory(), ACCT_INI, "rt");
    TextFile out(syscfgovr.tempdir, ACCT_INI, "wt+");
    if (in.IsOpen() && out.IsOpen()) {
      char szLine[ 260 ];
      while (in.ReadLine(szLine, 255)) {
        char szSavedLine[ 260 ];
        bool match = false;
        strcpy(szSavedLine, szLine);
        char* ss = strtok(szLine, "=");
        if (ss) {
          StringTrim(ss);
          if (wwiv::strings::IsEqualsIgnoreCase(szLine, szDefaultUserAddr)) {
            match = true;
          }
        }
        if (!match) {
          out.WriteFormatted(szSavedLine);
        }
      }
      out.WriteFormatted("\nUSER%d = %s", nUserNumber, pszInternetEmailAddress);
      in.Close();
      out.Close();
    }
    File::Remove(in.full_pathname());
    copyfile(out.full_pathname(), in.full_pathname(), false);
    File::Remove(out.full_pathname());
  }
}
