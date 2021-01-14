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
#include "bbs/inetmsg.h"

#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "common/input.h"
#include "bbs/instmsg.h"
#include "bbs/misccmd.h"
#include "common/quote.h"
#include "bbs/utility.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


static unsigned char translate_table[] = {
    "................................_!.#$%&.{}*+.-.{0123456789..{=}?"
    "_abcdefghijklmnopqrstuvwxyz{}}-_.abcdefghijklmnopqrstuvwxyz{|}~."
    "cueaaaaceeeiiiaaelaooouuyouclypfaiounnao?__..!{}................"
    "................................abfneouyo0od.0en=+}{fj*=oo..n2.."
};

bool check_inet_addr(const std::string& a) {
  return !a.empty() && (contains(a, '@') && contains(a, '.'));
}

void read_inet_addr(std::string& internet_address, int user_number) {
  internet_address.clear();
  if (!user_number) {
    return ;
  }

  if (user_number == a()->sess().user_num() && check_inet_addr(a()->user()->email_address())) {
    internet_address = a()->user()->email_address();
  } else {
    const auto fn = FilePath(a()->config()->datadir(), INETADDR_DAT);
    if (!File::Exists(fn)) {
      File file(fn);
      file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
      auto size = a()->config()->max_users() * 80;
      auto zero = std::make_unique<char[]>(size);
      file.Write(zero.get(), size);
    } else {
      char szUserName[255];
      File file(fn);
      file.Open(File::modeReadOnly | File::modeBinary);
      file.Seek(80 * user_number, File::Whence::begin);
      file.Read(szUserName, 80L);
      if (check_inet_addr(szUserName)) {
        internet_address = szUserName;
      } else {
        internet_address = StrCat("User #", user_number);
        User user;
        a()->users()->readuser(&user, user_number);
        user.email_address("");
        a()->users()->writeuser(&user, user_number);
      }
    }
  }
}

void write_inet_addr(const std::string& internet_address, int user_number) {
  if (!user_number) {
    return; /*nullptr;*/
  }

  File inetAddrFile(FilePath(a()->config()->datadir(), INETADDR_DAT));
  inetAddrFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  long lCurPos = 80L * static_cast<long>(user_number);
  inetAddrFile.Seek(lCurPos, File::Whence::begin);
  inetAddrFile.Write(internet_address.c_str(), 80L);
  inetAddrFile.Close();
  const auto default_addr = StrCat("USER", user_number);
  auto inet_net_num = getnetnum_by_type(network_type_t::internet);
  if (inet_net_num < 0) {
    return;
  }
  const auto& net = a()->nets().at(inet_net_num);
  TextFile in(FilePath(net.dir, ACCT_INI), "rt");
  TextFile out(FilePath(a()->sess().dirs().temp_directory(), ACCT_INI), "wt+");
  if (in.IsOpen() && out.IsOpen()) {
    char szLine[260];
    while (in.ReadLine(szLine, 255)) {
      char szSavedLine[260];
      bool match = false;
      strcpy(szSavedLine, szLine);
      char* ss = strtok(szLine, "=");
      if (ss) {
        StringTrim(ss);
        // TODO(rushfan): This is probably broken, maybe should be ss?
        if (iequals(szLine, default_addr)) {
          match = true;
        }
      }
      if (!match) {
        out.Write(szSavedLine);
      }
    }
    out.Write(fmt::sprintf("\nUSER%d = %s", user_number, internet_address));
    in.Close();
    out.Close();
  }
  File::Remove(in.path());
  File::Copy(out.path(), in.path());
  File::Remove(out.path());
}
