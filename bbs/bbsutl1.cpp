/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/finduser.h"
#include "bbs/input.h"
#include "bbs/mmkey.h"
#include "bbs/remote_io.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using std::chrono::milliseconds;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;

/**
 * Finds a()->usernum and system number from emailAddress, sets network number as
 * appropriate.
 * @param emailAddress The text of the email address.
 * @param un OUT The User Number
 * @param sy OUT The System Number
 */
std::tuple<uint16_t, uint16_t> parse_email_info(const std::string& email_address) {
  char *ss1, onx[20];
  unsigned user_number;
  std::set<char> odc;

  char szEmailAddress[255];
  to_char_array(szEmailAddress, email_address);

  uint16_t un = 0;
  uint16_t sy = 0;
  a()->net_email_name.clear();
  char* ss = strrchr(szEmailAddress, '@');
  if (ss == nullptr) {
    user_number = finduser1(szEmailAddress);
    if (user_number > 0) {
      un = static_cast<uint16_t>(user_number);
    } else if (wwiv::strings::IsEquals(szEmailAddress, "SYSOP")) { // Add 4.31 Build3
      un = 1;
    } else {
      bout << "Unknown user.\r\n";
    }
  } else if (to_number<int>(ss + 1) == 0) {
    int i;
    for (i = 0; i < wwiv::stl::ssize(a()->nets()); i++) {
      set_net_num(i);
      if (a()->current_net().type == network_type_t::internet) {
        for (ss1 = szEmailAddress; *ss1; ss1++) {
          if ((*ss1 >= 'A') && (*ss1 <= 'Z')) {
            *ss1 += 'a' - 'A';
          }
          a()->net_email_name = szEmailAddress;
        }
        sy = 1;
        break;
      }
    }
    if (i >= wwiv::stl::ssize(a()->nets())) {
      bout << "Unknown user.\r\n";
    }
  } else {
    ss[0] = '\0';
    ss = &ss[1];
    StringTrimEnd(szEmailAddress);
    user_number = to_number<unsigned int>(szEmailAddress);
    if (user_number == 0 && szEmailAddress[0] == '#') {
      user_number = to_number<unsigned int>(szEmailAddress + 1);
    }
    if (strchr(szEmailAddress, '@')) {
      user_number = 0;
    }
    const auto system_number = to_number<unsigned int>(ss);
    ss1 = strchr(ss, '.');
    if (ss1) {
      ss1++;
    }
    if (user_number == 0) {
      a()->net_email_name = szEmailAddress;
      StringTrimEnd(&a()->net_email_name);
      if (!a()->net_email_name.empty()) {
        sy = static_cast<uint16_t>(system_number);
      } else {
        bout << "Unknown user.\r\n";
      }
    } else {
      un = static_cast<uint16_t>(user_number);
      sy = static_cast<uint16_t>(system_number);
    }
    if (sy && ss1) {
      auto i = 0;
      for (i = 0; i < wwiv::stl::ssize(a()->nets()); i++) {
        set_net_num(i);
        if (iequals(ss1, a()->network_name())) {
          if (!valid_system(sy)) {
            bout.nl();
            bout << "There is no " << ss1 << " @" << sy << ".\r\n\n";
            sy = un = 0;
          } else {
            if (sy == a()->current_net().sysnum) {
              sy = 0;
              if (un == 0) {
                un = static_cast<uint16_t>(finduser(a()->net_email_name));
              }
              if (un == 0 || un > 32767) {
                un = 0;
                bout << "Unknown user.\r\n";
              }
            }
          }
          break;
        }
      }
      if (i >= wwiv::stl::ssize(a()->nets())) {
        bout.nl();
        bout << "This system isn't connected to " << ss1 << "\r\n";
        sy = un = 0;
      }
    } else if (sy && wwiv::stl::ssize(a()->nets()) > 1) {
      onx[0] = 'Q';
      onx[1] = '\0';
      auto onxi = 1;
      auto nv = 0;
      const auto on = a()->net_num();
      ss = static_cast<char*>(calloc(wwiv::stl::ssize(a()->nets()) + 1, 1));
      CHECK_NOTNULL(ss);
      int xx = -1;
      for (int i = 0; i < wwiv::stl::ssize(a()->nets()); i++) {
        set_net_num(i);
        if (a()->current_net().sysnum == sy) {
          xx = i;
        } else if (valid_system(sy)) {
          ss[nv++] = static_cast<char>(i);
        }
      }
      set_net_num(on);
      if (nv == 0) {
        if (xx != -1) {
          set_net_num(xx);
          sy = 0;
          if (un == 0) {
            un = static_cast<uint16_t>(finduser(a()->net_email_name));
            if (un == 0 || un > 32767) {
              un = 0;
              bout << "Unknown user.\r\n";
            }
          }
        } else {
          bout.nl();
          bout << "Unknown system\r\n";
          sy = un = 0;
        }
      } else if (nv == 1) {
        set_net_num(ss[0]);
      } else {
        bout.nl();
        for (int i = 0; i < nv; i++) {
          set_net_num(ss[i]);
          const auto* csne = next_system(sy);
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
        int i;
        if (nv < 9) {
          const char ch = onek(onx);
          i = ch == 'Q' ? -1 : ch - '1';
        } else {
          const auto mmk = mmkey(odc);
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
          un = sy = 0;
        }
      }
      free(ss);
    } else {
      if (sy == a()->current_net().sysnum) {
        sy = 0;
        if (un == 0) {
          un = static_cast<uint16_t>(finduser(a()->net_email_name));
        }
        if (un == 0 || un > 32767) {
          un = 0;
          bout << "Unknown user.\r\n";
        }
      } else if (!valid_system(sy)) {
        bout << "\r\nUnknown user.\r\n";
        sy = un = 0;
      }
    }
  }
  return std::make_tuple(un, sy);
}

std::string username_system_net_as_string(uint16_t un, const std::string& user_name, uint16_t sn,
                                          const std::string& network_name) {
  std::ostringstream ss;
  if (un) {
    ss << "#" << un;
  } else {
    ss << user_name;
  }
  if (sn > 0) {
    ss << " @" << sn;
  }
  if (!network_name.empty()) {
    ss << "." << network_name;
  }
  return ss.str();
}

std::string username_system_net_as_string(uint16_t un, const std::string& user_name, uint16_t sn) {
  return username_system_net_as_string(un, user_name, sn, "");
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
  if (!a()->context().incom()) {
    return true;
  }
  const auto password = input_password("|#7SY: ", 20);
  return password == a()->config()->system_password();
}

/**
 * Hangs up the modem if user online. Whether using modem or not, sets
 * a()->hangup_ to 1.
 */
void hang_it_up() {
  if (!a()->context().ok_modem_stuff()) {
    return;
  }
  a()->remoteIO()->disconnect();
  Hangup();
}

/**
 * Plays a sound definition file (*.sdf) through PC speaker. SDF files
 * should reside in the gfiles dir. The only params passed to function are
 * filename and false if playback is not abortable, true if it is abortable. If no
 * extension then .SDF is appended. A full path to file may be specified to
 * override gfiles dir. Format of file is:
 *
 * <freq> <duration in ms> [pause_delay in ms]
 * 1000 1000 50
 *
 * Returns true if successful, else returns false. The pause_delay is optional and
 * is used to insert silences between tones.
 */
bool play_sdf(const string& sound_filename, bool abortable) {
  if (sound_filename.empty()) {
    return false;
  }

  std::filesystem::path full_pathname;
  // append gfiles directory if no path specified
  if (sound_filename.find(File::pathSeparatorChar) == string::npos) {
    full_pathname = FilePath(a()->config()->gfilesdir(), sound_filename);
  } else {
    full_pathname = sound_filename;
  }

  // append .SDF if no extension specified
  if (!full_pathname.has_extension()) {
    full_pathname.replace_extension(".sdf");
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
    const auto nw = wordcount(soundLine, DELIMS_WHITE);
    if (nw >= 2) {
      const auto freq = to_number<int>(extractword(1, soundLine, DELIMS_WHITE));
      auto dur = to_number<int>(extractword(2, soundLine, DELIMS_WHITE));

      // only play if freq and duration > 0
      if (freq > 0 && dur > 0) {
        auto pause_delay = 0;
        if (nw > 2) {
          pause_delay = to_number<int>(extractword(3, soundLine, DELIMS_WHITE));
        }
        sound(freq, milliseconds(dur));
        if (pause_delay > 0) {
          sleep_for(milliseconds(pause_delay));
        }
      }
    }
  }

  return true;
}

/**
 * Describes the area code as listed in regions.dat
 * @param nAreaCode The area code to describe
 */
string describe_area_code(int nAreaCode) {
  TextFile file(FilePath(a()->config()->datadir(), REGIONS_DAT), "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  string previous;
  string current;
  while (file.ReadLine(&current)) {
    const auto current_town = to_number<int>(current);
    if (current_town == nAreaCode) {
      return previous;
    }
    if (current_town == 0) {
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
  const auto regions_dir = FilePath(a()->config()->datadir(), REGIONS_DIR);
  const auto filename = fmt::sprintf("%s.%-3d", REGIONS_DIR, nAreaCode);
  TextFile file(FilePath(regions_dir, filename), "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  string previous;
  string current;
  while (file.ReadLine(&current)) {
    const auto current_town = to_number<int>(current);
    if (current_town == nTargetTown) {
      return previous;
    }
    if (current_town == 0) {
      // Only set this on values that are town names and not area codes.
      previous = current;
    }
  }
  return "";
}
