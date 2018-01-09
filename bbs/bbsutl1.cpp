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
#include "bbs/bbsutl1.h"

#include <chrono>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/finduser.h"
#include "bbs/input.h"
#include "bbs/mmkey.h"
#include "bbs/remote_io.h"
#include "bbs/vars.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

using std::chrono::milliseconds;
using std::string;
using std::unique_ptr;
using namespace wwiv::os;
using namespace wwiv::strings;

/**
 * Finds a()->usernum and system number from emailAddress, sets network number as
 * appropriate.
 * @param emailAddress The text of the email address.
 * @param pUserNumber OUT The User Number
 * @param pSystemmNumber OUT The System Number
 */
void parse_email_info(const string& emailAddress, uint16_t *pUserNumber, uint16_t *pSystemNumber) {
  char *ss1, onx[20], ch;
  unsigned user_number, system_number;
  int nv, on, xx, onxi, odci;
  std::set<char> odc;

  char szEmailAddress[255];
  strcpy(szEmailAddress, emailAddress.c_str());

  *pUserNumber = 0;
  *pSystemNumber = 0;
  a()->net_email_name.clear();
  char *ss = strrchr(szEmailAddress, '@');
  if (ss == nullptr) {
    user_number = finduser1(szEmailAddress);
    if (user_number > 0) {
      *pUserNumber = static_cast<uint16_t>(user_number);
    } else if (wwiv::strings::IsEquals(szEmailAddress, "SYSOP")) {     // Add 4.31 Build3
      *pUserNumber = 1;
    } else {
      bout << "Unknown user.\r\n";
    }
  } else if (to_number<int>(ss + 1) == 0) {
    int i = 0;
    for (i = 0; i < a()->max_net_num(); i++) {
      set_net_num(i);
      if (a()->current_net().type == network_type_t::internet) {
        for (ss1 = szEmailAddress; *ss1; ss1++) {
          if ((*ss1 >= 'A') && (*ss1 <= 'Z')) {
            *ss1 += 'a' - 'A';
          }
          a()->net_email_name = szEmailAddress;
        }
        *pSystemNumber = 1;
        break;
      }
    }
    if (i >= a()->max_net_num()) {
      bout << "Unknown user.\r\n";
    }
  } else {
    ss[0] = '\0';
    ss = &(ss[1]);
    StringTrimEnd(szEmailAddress);
    user_number = to_number<unsigned int>(szEmailAddress);
    if (user_number == 0 && szEmailAddress[0] == '#') {
      user_number = to_number<unsigned int>(szEmailAddress + 1);
    }
    if (strchr(szEmailAddress, '@')) {
      user_number = 0;
    }
    system_number = to_number<unsigned int>(ss);
    ss1 = strchr(ss, '.');
    if (ss1) {
      ss1++;
    }
    if (user_number == 0) {
      a()->net_email_name = szEmailAddress;
      StringTrimEnd(&a()->net_email_name);
      if (!a()->net_email_name.empty()) {
        *pSystemNumber = static_cast<uint16_t>(system_number);
      } else {
        bout << "Unknown user.\r\n";
      }
    } else {
      *pUserNumber = static_cast<uint16_t>(user_number);
      *pSystemNumber = static_cast<uint16_t>(system_number);
    }
    if (*pSystemNumber && ss1) {
      int i = 0;
      for (i = 0; i < a()->max_net_num(); i++) {
        set_net_num(i);
        if (IsEqualsIgnoreCase(ss1, a()->network_name())) {
          if (!valid_system(*pSystemNumber)) {
            bout.nl();
            bout << "There is no " << ss1 << " @" << *pSystemNumber << ".\r\n\n";
            *pSystemNumber = *pUserNumber = 0;
          } else {
            if (*pSystemNumber == a()->current_net().sysnum) {
              *pSystemNumber = 0;
              if (*pUserNumber == 0) {
                *pUserNumber = static_cast<uint16_t>(finduser(a()->net_email_name));
              }
              if (*pUserNumber == 0 || *pUserNumber > 32767) {
                *pUserNumber = 0;
                bout << "Unknown user.\r\n";
              }
            }
          }
          break;
        }
      }
      if (i >= a()->max_net_num()) {
        bout.nl();
        bout << "This system isn't connected to " << ss1 << "\r\n";
        *pSystemNumber = *pUserNumber = 0;
      }
    } else if (*pSystemNumber && a()->max_net_num() > 1) {
      odci = 0;
      onx[0] = 'Q';
      onx[1] = '\0';
      onxi = 1;
      nv = 0;
      on = a()->net_num();
      ss = static_cast<char *>(calloc(a()->max_net_num() + 1, 1));
      WWIV_ASSERT(ss != nullptr);
      xx = -1;
      for (int i = 0; i < a()->max_net_num(); i++) {
        set_net_num(i);
        if (a()->current_net().sysnum == *pSystemNumber) {
          xx = i;
        } else if (valid_system(*pSystemNumber)) {
          ss[nv++] = static_cast<char>(i);
        }
      }
      set_net_num(on);
      if (nv == 0) {
        if (xx != -1) {
          set_net_num(xx);
          *pSystemNumber = 0;
          if (*pUserNumber == 0) {
            *pUserNumber = static_cast<uint16_t>(finduser(a()->net_email_name));
            if (*pUserNumber == 0 || *pUserNumber > 32767) {
              *pUserNumber = 0;
              bout << "Unknown user.\r\n";
            }
          }
        } else {
          bout.nl();
          bout << "Unknown system\r\n";
          *pSystemNumber = *pUserNumber = 0;
        }
      } else if (nv == 1) {
        set_net_num(ss[0]);
      } else {
        bout.nl();
        for (int i = 0; i < nv; i++) {
          set_net_num(ss[i]);
          net_system_list_rec *csne = next_system(*pSystemNumber);
          if (csne) {
            if (i < 9) {
              onx[onxi++] = static_cast<char>(i + '1');
              onx[onxi] = 0;
            } else {
              odc.insert(static_cast<char>((i + 1) / 10));
            }
            bout << i + 1 << ". " << a()->network_name() << " (" << csne->name << ")\r\n";
          }
        }
        bout << "Q. Quit\r\n\n";
        bout << "|#2Which network (number): ";
        int i = -1;
        if (nv < 9) {
          ch = onek(onx);
          if (ch == 'Q') {
            i = -1;
          } else {
            i = ch - '1';
          }
        } else {
          string mmk = mmkey(odc);
          if (mmk == "Q") {
            i = -1;
          } else {
            i = to_number<decltype(i)>(mmk) - 1;
          }
        }
        if (i >= 0 && i < nv) {
          set_net_num(ss[i]);
        } else {
          bout << "\r\n|#6Aborted.\r\n\n";
          *pUserNumber = *pSystemNumber = 0;
        }
      }
      free(ss);
    } else {
      if (*pSystemNumber == a()->current_net().sysnum) {
        *pSystemNumber = 0;
        if (*pUserNumber == 0) {
          *pUserNumber = static_cast<uint16_t>(finduser(a()->net_email_name));
        }
        if (*pUserNumber == 0 || *pUserNumber > 32767) {
          *pUserNumber = 0;
          bout << "Unknown user.\r\n";
        }
      } else if (!valid_system(*pSystemNumber)) {
        bout << "\r\nUnknown user.\r\n";
        *pSystemNumber = *pUserNumber = 0;
      }
    }
  }
}

/**
 * Queries user and verifies system password.
 * @return true if the password entered is valid.
 */
bool ValidateSysopPassword() {
  bout.nl();
  if (!so()) {
    return false;
  }
  if (!incom) {
    return true;
  }
  string password = input_password("|#7SY: ", 20);
  return (password == a()->config()->config()->systempw);
}

/**
 * Hangs up the modem if user online. Whether using modem or not, sets
 * hangup to 1.
 */
void hang_it_up() {
  if (!ok_modem_stuff) {
    return;
  }
  a()->remoteIO()->disconnect();
  Hangup();
}

/**
 * Plays a sound definition file (*.sdf) through PC speaker. SDF files
 * should reside in the gfiles dir. The only params passed to function are
 * filename and false if playback is unabortable, true if it is abortable. If no
 * extension then .SDF is appended. A full path to file may be specified to
 * override gfiles dir. Format of file is:
 *
 * <freq> <duration in ms> [pause_delay in ms]
 * 1000 1000 50
 *
 * Returns 1 if sucessful, else returns 0. The pause_delay is optional and
 * is used to insert silences between tones.
 */
bool play_sdf(const string& sound_filename, bool abortable) {
  WWIV_ASSERT(!sound_filename.empty());

  string full_pathname;
  // append gfilesdir if no path specified
  if (sound_filename.find(File::pathSeparatorChar) == string::npos) {
    full_pathname = StrCat(a()->config()->gfilesdir(), sound_filename);
  } else {
    full_pathname = sound_filename;
  }

  // append .SDF if no extension specified
  if (full_pathname.find('.') == string::npos) {
    full_pathname += ".sdf";
  }

  // Must Exist
  if (!File::Exists(full_pathname)) {
    return false;
  }

  // must be able to open read-only
  TextFile soundFile(full_pathname, "rt");
  if (!soundFile.IsOpen()) {
    return false;
  }

  // scan each line, ignore lines with words<2
  string soundLine;
  while (soundFile.ReadLine(&soundLine)) {
    if (abortable && bkbhit()) {
      break;
    }
    int nw = wordcount(soundLine.c_str(), DELIMS_WHITE);
    if (nw >= 2) {
      auto freq = to_number<int>(extractword(1, soundLine, DELIMS_WHITE));
      auto dur = to_number<int>(extractword(2, soundLine, DELIMS_WHITE));

      // only play if freq and duration > 0
      if (freq > 0 && dur > 0) {
        int nPauseDelay = 0;
        if (nw > 2) {
          nPauseDelay = to_number<int>(extractword(3, soundLine, DELIMS_WHITE));
        }
        sound(freq, milliseconds(dur));
        if (nPauseDelay > 0) {
          sleep_for(milliseconds(nPauseDelay));
        }
      }
    }
  }

  soundFile.Close();
  return true;
}


/**
 * Describes the area code as listed in regions.dat
 * @param nAreaCode The area code to describe
 * @param description point to return the description for the specified
 *        area code.
 */
string describe_area_code(int nAreaCode) {
  TextFile file(a()->config()->datadir(), REGIONS_DAT, "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  string previous;
  string current;
  while (file.ReadLine(&current)) {
    auto nCurrentTown = to_number<int>(current.c_str());
    if (nCurrentTown == nAreaCode) {
      return previous;
    } else if (nCurrentTown == 0) {
      // Only set this on values that are town names and not area codes.
      previous = current;
    }
  }
  return "";
}

/**
 * Describes the Town (area code + prefix) as listed in the regions file.
 * @param nAreaCode The area code to describe
 * @param nTargetTown The phone number prefix to describe
 * @return the description for the specified area code.
 */
string describe_area_code_prefix(int nAreaCode, int nTargetTown) {
  const auto regions_dir = wwiv::core::FilePath(a()->config()->datadir(), REGIONS_DIR);
  const auto filename = StringPrintf("%s.%-3d", REGIONS_DIR, nAreaCode);
  TextFile file(regions_dir, filename, "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  string previous;
  string current;
  while (file.ReadLine(&current)) {
    int nCurrentTown = to_number<int>(current);
    if (nCurrentTown == nTargetTown) {
      return previous;
    } else if (nCurrentTown == 0) {
      // Only set this on values that are town names and not area codes.
      previous = current;
    }
  }
  return "";
}
