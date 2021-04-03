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
#include "bbs/bbsutl1.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/connect1.h"
#include "bbs/finduser.h"
#include "bbs/mmkey.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "common/remote_io.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <chrono>
#include <filesystem>
#include <string>

// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/fido/fido_util.h"
#include "sdk/net/networks.h"

using std::chrono::milliseconds;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static const size_t MAX_CONF_LINE = 4096;


/*
 * Returns number of "words" in a specified string, using a specified set
 * of characters as delimiters.
 */
static int wordcount(const std::string& instr, const char* delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  auto i = 0;

  to_char_array(szTempBuffer, instr);
  for (auto* s = strtok(szTempBuffer, delimstr); s; s = strtok(nullptr, delimstr)) {
    i++;
  }
  return i;
}

/*
 * Returns pointer to string representing the nth "word" of a string, using
 * a specified set of characters as delimiters.
 */
static std::string extractword(int ww, const std::string& instr, const char* delimstr) {
  char buffer[MAX_CONF_LINE];
  auto i = 0;

  if (!ww) {
    return {};
  }

  to_char_array(buffer, instr);
  for (auto* s = strtok(buffer, delimstr); s && i++ < ww; s = strtok(nullptr, delimstr)) {
    if (i == ww) {
      return std::string(s);
    }
  }
  return {};
}

/**
 * Filters networks by viability.
 *
 * A viable network is:
 *   (1) has system_num reachable.
 *   (2) If the network is FTN, then tries to match zone from the network.
 *
 * Returns a vector of networks and the system names identified by the email address
 * either from system_num for WWIVnet networks, or from parsing the FidoNet Address
 * out of the email address for FTN networks.
 */
std::vector<NetworkAndName> filter_networks(std::vector<Network>& nets,
                                            const std::string& email, int system_num) {
  if (nets.empty()) {
    return {};
  }

  std::vector<NetworkAndName> viable;
  const auto ftnadr = fido::get_address_from_single_line(email);
  for (auto& net : nets) {
    set_net_num(net.network_number());
    if (const auto csne = next_system(system_num)) {
      if (net.type == network_type_t::ftn) {
        if (ftnadr.zone() != -1 && net.try_load_nodelist()) {
          if (!net.nodelist->has_zone(ftnadr.zone())) {
            VLOG(1) << "Skipping net known to not have zone: " << net.name << "; zone: " << ftnadr.zone();
            // This nodelist is loaded and doesn't have the zone, skip it.
            continue;
          }
        }
      }
      viable.emplace_back(net, csne->name);
    }
  }
  return viable;
}

static std::optional<int> query_network(const std::vector<NetworkAndName>& nets) {
  if (nets.empty()) {
    return std::nullopt;    
  }
  if (nets.size() == 1) {
    return { nets.front().net.network_number() };
  }
  std::string onx{'Q'};
  std::set<char> odc;
  bout.nl();
  auto nn = 1;
  for (const auto& v : nets) {
    set_net_num(v.net.network_number());
    if (nn < 9) {
      onx.push_back(static_cast<char>(nn + '0'));
    } else {
      odc.insert(static_cast<char>(nn / 10));
    }
    bout << "|#2" << nn << "|#9) " << v.net.name << " (|#1" << v.system_name << "|#9)\r\n";
    ++nn;
  }
  bout << "|#2Q|#9) Quit\r\n\n";
  bout << "|#5(|#2Q|#5=|#1Quit|#5) Which network (number): ";
  if (nets.size() < 9) {
    const char ch = onek(onx);
    if (ch == 'Q') {
      return std::nullopt;
    }
    const int n = ch - '1';
    return { nets.at(n).net.network_number() };
  }
  const auto mmk = mmkey(odc); 
  if (mmk == "Q") {
    return std::nullopt;
  }
  const auto selected =  to_number<int>(mmk) - 1;
  if (selected < 0 || selected >= wwiv::stl::size_int(nets)) {
    return std::nullopt;
  }
  return { nets.at(selected).net.network_number() };
}

static std::tuple<uint16_t, uint16_t> parse_internet_email_info(const std::string& email) {
  for (auto i = 0; i < wwiv::stl::size_int(a()->nets()); i++) {
    set_net_num(i);
    if (a()->current_net().type == network_type_t::internet) {
      a()->net_email_name = ToStringLowerCase(email);
      // 0 for the user, and we'll use the network.
      return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(INTERNET_EMAIL_FAKE_OUTBOUND_NODE));
    }
  }
  bout << "Unknown user.\r\n";
  return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
}

std::tuple<uint16_t, uint16_t> parse_ftn_email_info(const std::string& email) {
  a()->net_email_name = ToStringLowerCase(email);
  // We don't have a network name, so need to loop through them all.
  std::vector<Network> nets;
  for (const auto& net : a()->nets().networks()) {
    if (net.type == network_type_t::ftn) {
      nets.push_back(net);
    }
  }

  if (const auto o = query_network(filter_networks(nets, email, FTN_FAKE_OUTBOUND_NODE))) {
    set_net_num(o.value());
    return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(FTN_FAKE_OUTBOUND_NODE));
  }
  return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
}

std::tuple<uint16_t, uint16_t> parse_local_email_info(const std::string& email) {
  if (const auto user_number = finduser1(email); user_number > 0) {
    return std::make_tuple(static_cast<uint16_t>(user_number), static_cast<uint16_t>(0));
  }
  if (iequals(email, "SYSOP")) {
    return std::make_tuple(static_cast<uint16_t>(1), static_cast<uint16_t>(0));
  }
  bout << "Unknown user.\r\n";
  return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
}


std::tuple<unsigned short, unsigned short>
parse_wwivnet_email_info(const std::string& email, int system_number,
                         const std::string& network_name) {

  auto user_number = to_number<int>(email);
  if (user_number == 0 && email.front() == '#') {
    user_number = to_number<int>(email.substr(1));
  }

  a()->net_email_name = ToStringLowerCase(email);
  if (!network_name.empty()) {
    for (auto i = 0; i < wwiv::stl::size_int(a()->nets()); i++) {
      set_net_num(i);
      if (iequals(network_name, a()->network_name())) {
        if (valid_system(system_number)) {
          if (system_number == a()->current_net().sysnum) {
            return parse_local_email_info(email);
          }
          // return 0,0 since we set the net_email_name already.
          return std::make_tuple(static_cast<uint16_t>(user_number), static_cast<uint16_t>(system_number));
        }
      }
    }
    return std::make_tuple(static_cast<uint16_t>(user_number), static_cast<uint16_t>(system_number));
  }
  // We don't have a network name, so need to loop through them all.
  std::vector<Network> nets;
  for (const auto& net : a()->nets().networks()) {
    if (net.type == network_type_t::wwivnet) {
      nets.push_back(net);
    }
  }

  if (const auto o = query_network(filter_networks(nets, email, system_number))) {
    set_net_num(o.value());
    return std::make_tuple(static_cast<uint16_t>(user_number),
                           static_cast<uint16_t>(system_number));
  }
  return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
}

std::tuple<uint16_t, uint16_t> parse_email_info(const std::string& email_address) {
  if (email_address.empty()) {
    return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
  }
  
  if (const auto atidx = email_address.rfind('@');
    atidx != std::string::npos) {
    if (atidx == email_address.size() - 1) {
      // malformed email address of 1@
      return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
    }
    const auto email = email_address.substr(0, atidx);
    if (isdigit(email_address.at(atidx + 1))) {
      // We have something like "xx @N" where N is a digit.
      const auto suffix = email_address.substr(atidx + 1);
      auto system_number = to_number<int>(suffix);
      std::string network_name;
      if (const auto dotidx = suffix.find('.'); dotidx != std::string::npos) {
        network_name = suffix.substr(dotidx + 1);
        system_number = to_number<int>(suffix.substr(0, dotidx));
      }

      if (system_number == FTN_FAKE_OUTBOUND_NODE) {
        return parse_ftn_email_info(email);
      }
      if (system_number == INTERNET_EMAIL_FAKE_OUTBOUND_NODE) {
        return parse_internet_email_info(email);
      }
      if (system_number == INTERNET_NEWS_FAKE_OUTBOUND_NODE) {
        bout << "|#6NNTP is not supported yet." << wwiv::endl;
        return std::make_tuple(static_cast<uint16_t>(0), static_cast<uint16_t>(0));
      }
      return parse_wwivnet_email_info(email, system_number, network_name);
    }
    return parse_internet_email_info(email);
  }
  // No @, so just a local address.
  return parse_local_email_info(email_address);
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
  if (!a()->sess().incom()) {
    return true;
  }
  const auto password = bin.input_password("|#7SY: ", 20);
  return password == a()->config()->system_password();
}

/**
 * Hangs up the modem if user online. Whether using modem or not.
 */
void hang_it_up() {
  if (!a()->sess().ok_modem_stuff()) {
    return;
  }
  bout.remoteIO()->disconnect();
  a()->Hangup();
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
bool play_sdf(const std::string& sound_filename, bool abortable) {
  if (sound_filename.empty()) {
    return false;
  }

  std::filesystem::path full_pathname;
  // append gfiles directory if no path specified
  if (sound_filename.find(File::pathSeparatorChar) == std::string::npos) {
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
  std::string soundLine;
  while (soundFile.ReadLine(&soundLine)) {
    if (abortable && bin.bkbhit()) {
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
std::string describe_area_code(int nAreaCode) {
  TextFile file(FilePath(a()->config()->datadir(), REGIONS_DAT), "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  std::string previous;
  std::string current;
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
std::string describe_area_code_prefix(int nAreaCode, int nTargetTown) {
  const auto regions_dir = FilePath(a()->config()->datadir(), REGIONS_DIR);
  const auto filename = fmt::sprintf("%s.%-3d", REGIONS_DIR, nAreaCode);
  TextFile file(FilePath(regions_dir, filename), "rt");
  if (!file.IsOpen()) {
    // Failed to open regions area code file
    return "";
  }

  std::string previous;
  std::string current;
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
