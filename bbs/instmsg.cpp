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
#include "bbs/instmsg.h"

#include "bbs/bbs.h"
#include "common/datetime.h"
#include "common/output.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/instance.h"
#include "sdk/instance_message.h"
#include "sdk/names.h"

#include <chrono>
#include <string>

using std::string;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static bool chat_avail;
static bool chat_invis;

static steady_clock::time_point last_iia;
static std::chrono::milliseconds iia;

bool is_chat_invis() { 
  return chat_invis; 
}

void send_inst_str(int whichinst, const std::string& s) {
  send_instance_string(*a()->config(), instance_message_type_t::user, whichinst,
                       a()->sess().user_num(), a()->instance_number(), s);
}

void send_inst_sysstr(int whichinst, const std::string& s) {
  send_instance_string(*a()->config(), instance_message_type_t::system, whichinst,
                       a()->sess().user_num(), a()->instance_number(), s);
}

/*
 * "Broadcasts" a message to all online instances.
 */
void broadcast(const std::string& message) {
  for (auto i = 1; i <= num_instances(); i++) {
    if (i != a()->instance_number() && a()->instances().at(i).available()) {
      send_inst_str(i, message);
    }
  }
}

/*
 * Handles one inter-instance message, based on type, returns inter-instance
 * main type of the "packet".
 */
static void handle_inst_msg(const instance_message_t& ih) {
  if (a()->sess().hangup() || !a()->sess().IsUserOnline()) {
    return;
  }
  const auto line = bout.SaveCurrentLine();
  bout.nl(2);
  if (a()->sess().in_chatroom()) {
    bout << ih.message << "\r\n";
    bout.RestoreCurrentLine(line);
    return;
  }
  if (ih.message_type == instance_message_type_t::system) {
    const auto from_user_name = a()->names()->UserName(ih.from_user);
    bout.bprintf("|#1%.12s (%d)|#0> |#2", from_user_name, ih.from_instance);
  } else {
    bout << "|#6[SYSTEM ANNOUNCEMENT] |#7> |#2";
  }
  bout << ih.message << "\r\n\r\n";
  bout.RestoreCurrentLine(line);
}

void process_inst_msgs() {
  if (!inst_msg_waiting()) {
    return;
  }
  last_iia = steady_clock::now();
  const auto oiia = setiia(std::chrono::milliseconds(0));

  // Handle semaphores
  const auto sem_fnd = FilePath(a()->sess().dirs().scratch_directory(), "*.wwiv");
  FindFiles ffs(sem_fnd, FindFiles::FindFilesType::files, FindFiles::WinNameType::long_name);
  for (const auto& f : ffs) {
   if (f.name == "readuser.wwiv") {
     LOG(INFO) << "Reread current user";
     File::Remove(FilePath(a()->sess().dirs().scratch_directory(), f.name));
     a()->ReadCurrentUser();
   }
  }

  auto messages = read_all_instance_messages(*a()->config(), a()->instance_number(), 1000);
  for (const auto& m : messages) {
    handle_inst_msg(m);
  }
  setiia(oiia);
}

/*
 * Returns max instance number.
 */
int num_instances() {
  return a()->instances().size();
}


/*
 * Returns true if  user_number is online, and returns instance user is on in
 * wi, else returns false.
 */
std::optional<int> user_online(int user_number) {
  for (auto i = 1; i <= wwiv::stl::size_int(a()->instances()); i++) {
    if (i == a()->instance_number()) {
      continue;
    }
    if (const auto ir = a()->instances().at(i); ir.user_number() == user_number && ir.online()) {
      return i;
    }
  }
  return std::nullopt;
}

/*
* Writes BBS location data to instance.dat, so other instances can see
* some info about this instance.
*/
void write_inst(int loc, int subloc, int flags) {
  static Instance ti{};

  auto re_write = false;
  if (ti.user_number() == 0) {
    const auto ir = a()->instances().at(a()->instance_number());
    ti.ir().user = static_cast<int16_t>(ir.user_number());
    ti.ir().inst_started = daten_t_now();
    re_write = true;
  }

  uint16_t cf = ti.ir().flags & (~(INST_FLAGS_ONLINE | INST_FLAGS_MSG_AVAIL));
  if (a()->sess().IsUserOnline()) {
    cf |= INST_FLAGS_ONLINE;
    if (chat_invis) {
      cf |= INST_FLAGS_INVIS;
    }
    if (!a()->user()->IsIgnoreNodeMessages()) {
      switch (loc) {
      case INST_LOC_MAIN:
      case INST_LOC_XFER:
      case INST_LOC_SUBS:
      case INST_LOC_EMAIL:
      case INST_LOC_CHATROOM:
      case INST_LOC_RMAIL:
        if (chat_avail) {
          cf |= INST_FLAGS_MSG_AVAIL;
        }
        break;
      default:
        if (loc >= INST_LOC_CH1 && loc <= INST_LOC_CH10 && chat_avail) {
          cf |= INST_FLAGS_MSG_AVAIL;
        }
        break;
      }
    }
    const auto ms = static_cast<uint16_t>(a()->sess().using_modem() ? a()->modem_speed_ : 0);
    if (ti.modem_speed() != ms) {
      ti.ir().modem_speed = ms;
      re_write = true;
    }
  }
  if (flags != INST_FLAGS_NONE) {
    // set an option
    ti.ir().flags |= flags;
  }
  if (ti.invisible() && !chat_invis) {
    cf = 0;
    if (ti.ir().flags & INST_FLAGS_ONLINE) {
      cf |= INST_FLAGS_ONLINE;
    }
    if (ti.ir().flags & INST_FLAGS_MSG_AVAIL) {
      cf |= INST_FLAGS_MSG_AVAIL;
    }
    re_write = true;
  }
  if (cf != ti.ir().flags) {
    re_write = true;
    ti.ir().flags = cf;
  }
  if (ti.ir().number != a()->instance_number()) {
    re_write = true;
    ti.ir().number = static_cast<int16_t>(a()->instance_number());
  }
  if (loc == INST_LOC_DOWN) {
    re_write = true;
  } else {
    if (a()->sess().IsUserOnline()) {
      if (ti.ir().user != a()->sess().user_num()) {
        re_write = true;
        if (a()->sess().user_num() > 0 && a()->sess().user_num() <= a()->config()->max_users()) {
          ti.ir().user = static_cast<int16_t>(a()->sess().user_num());
        }
      }
    }
  }

  if (ti.ir().subloc != static_cast<uint16_t>(subloc)) {
    re_write = true;
    ti.ir().subloc = static_cast<uint16_t>(subloc);
  }
  if (ti.ir().loc != static_cast<uint16_t>(loc)) {
    re_write = true;
    ti.ir().loc = static_cast<uint16_t>(loc);
  }
  const auto ti_chat_invis = ti.invisible();
  const auto ti_msg_avail = (ti.ir().flags & INST_FLAGS_MSG_AVAIL) != 0;
  if ((ti_chat_invis != chat_invis || ti_msg_avail != chat_avail) && ti.loc_code() != INST_LOC_WFC) {
    re_write = true;
  }
  if (!re_write) {
    return;
  }

  ti.ir().last_update = daten_t_now();
  a()->instances().upsert(a()->instance_number(), ti);
}

/*
* Returns 1 if a message waiting for this instance, 0 otherwise.
*/
bool inst_msg_waiting() {
  if (iia.count() == 0) return false;

  const auto l = steady_clock::now();
  if ((l - last_iia) < iia) {
    return false;
  }

  if (File::ExistsWildcard(FilePath(a()->sess().dirs().scratch_directory(), "msg*.json"))) {
    return true;
  }

  if (File::ExistsWildcard(FilePath(a()->sess().dirs().scratch_directory(), "*.wwiv"))) {
    return true;
  }

  last_iia = l;
  return false;
}

// Sets inter-instance availability on/off, for inter-instance messaging.
// returns the old iia value.
std::chrono::milliseconds setiia(std::chrono::milliseconds poll_time) {
  const auto oiia = iia;
  iia = poll_time;
  return oiia;
}

// Toggles user availability, called when ctrl-N is hit

void toggle_avail() {

  const auto line = bout.SaveCurrentLine();
  chat_avail = !chat_avail;

  bout.format("\r\nYou are now {}\r\n\r\n", chat_avail ? "available for chat." : "not available for chat.");
  const auto loc = a()->instances().at(a()->instance_number()).loc_code();
  write_inst(loc, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  bout.RestoreCurrentLine(line);
}

// Toggles invisibility, called when ctrl-L is hit by a sysop

void toggle_invis() {
  const auto line = bout.SaveCurrentLine();
  chat_invis = !chat_invis;

  bout.format("\r\nYou are now {}\r\n\r\n", chat_invis ? "invisible." : "visible.");
  const auto loc = a()->instances().at(a()->instance_number()).loc_code();
  write_inst(loc, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  bout.RestoreCurrentLine(line);
}
