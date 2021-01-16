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
#include "common/pause.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/instance.h"
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

static void send_inst_msg(inst_msg_header *ih, const std::string& msg) {
  const auto fn = fmt::sprintf("tmsg%3.3u.%3.3d", a()->instance_number(), ih->dest_inst);
  File file(FilePath(a()->config()->datadir(), fn));
  if (file.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile, File::shareDenyReadWrite)) {
    file.Seek(0L, File::Whence::end);
    if (ih->msg_size > 0 && msg.empty()) {
      ih->msg_size = 0;
    }
    file.Write(ih, sizeof(inst_msg_header));
    if (ih->msg_size > 0) {
      file.Write(msg.c_str(), ih->msg_size);
    }
    file.Close();

    for (auto i = 0; i < 1000; i++) {
      const auto dest =
          FilePath(a()->config()->datadir(), fmt::sprintf("msg%5.5d.%3.3d", i, ih->dest_inst));
      if (!File::Rename(file.path(), dest) || (errno != EACCES)) {
        break;
      }
    }
  }
}

static void send_inst_str1(int m, int whichinst, const std::string& send_string) {
  const auto tempsendstring = StrCat(send_string, "\r\n");
  inst_msg_header ih{};
  ih.main = static_cast<uint16_t>(m);
  ih.minor = 0;
  ih.from_inst = static_cast<uint16_t>(a()->instance_number());
  ih.from_user = static_cast<uint16_t>(a()->sess().user_num());
  ih.msg_size = wwiv::stl::size_int(tempsendstring) + 1;
  ih.dest_inst = static_cast<uint16_t>(whichinst);
  ih.daten = daten_t_now();

  send_inst_msg(&ih, tempsendstring);
}

void send_inst_str(int whichinst, const std::string& send_string) {
  send_inst_str1(INST_MSG_STRING, whichinst, send_string);
}

void send_inst_sysstr(int whichinst, const std::string& send_string) {
  send_inst_str1(INST_MSG_SYSMSG, whichinst, send_string);
}

/*
 * "Broadcasts" a message to all online instances.
 */
void broadcast(const std::string& message) {
  const auto ni = num_instances();
  for (auto i = 1; i <= ni; i++) {
    if (i != a()->instance_number() && a()->instances().at(i).available()) {
      send_inst_str(i, message);
    }
  }
}


/*
 * Handles one inter-instance message, based on type, returns inter-instance
 * main type of the "packet".
 */
static int handle_inst_msg(inst_msg_header * ih, const std::string& msg) {
  if (!ih || (ih->msg_size > 0 && msg.empty())) {
    return -1;
  }

  switch (ih->main) {
  case INST_MSG_STRING:
  case INST_MSG_SYSMSG:
    if (ih->msg_size > 0 && a()->sess().IsUserOnline() && !a()->sess().hangup()) {
      const auto line = bout.SaveCurrentLine();
      bout.nl(2);
      if (a()->sess().in_chatroom()) {
        bout << msg << "\r\n";
        bout.RestoreCurrentLine(line);
        return ih->main;
      }
      if (ih->main == INST_MSG_STRING) {
        const auto from_user_name = a()->names()->UserName(ih->from_user);
        bout.bprintf("|#1%.12s (%d)|#0> |#2", from_user_name, ih->from_inst);
      } else {
        bout << "|#6[SYSTEM ANNOUNCEMENT] |#7> |#2";
      }
      bout << msg << "\r\n\r\n";
      bout.RestoreCurrentLine(line);
    }
    break;
  default:
    break;
  }
  return ih->main;
}


void process_inst_msgs() {
  if (!inst_msg_waiting()) {
    return;
  }
  last_iia = steady_clock::now();
  const auto oiia = setiia(std::chrono::milliseconds(0));

  const auto fndspec = fmt::sprintf("%smsg*.%3.3u", a()->config()->datadir(), a()->instance_number());
  FindFiles ff(fndspec, FindFiles::FindFilesType::files);
  for (const auto& f : ff) {
    if (a()->sess().hangup()) { break; }
    File file(FilePath(a()->config()->datadir(), f.name));
    if (!file.Open(File::modeBinary | File::modeReadOnly, File::shareDenyReadWrite)) {
      LOG(ERROR) << "Unable to open file: " << file;
      continue;
    }
    while (true) {
      inst_msg_header ih = {};
      const auto num_read = file.Read(&ih, sizeof(inst_msg_header));
      if (num_read == 0) {
        // End of file.
        break;
      }
      string m;
      if (ih.msg_size > 0) {
        m.resize(ih.msg_size + 1);
        file.Read(&m[0], ih.msg_size);
        m.resize(ih.msg_size);
      }
      handle_inst_msg(&ih, m);
    }
    file.Close();
    File::Remove(file.path());
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
bool user_online(int user_number, int *wi) {
  const auto ni = a()->instances().size();
  for (auto i = 1; i <= ni; i++) {
    if (i == a()->instance_number()) {
      continue;
    }
    const auto ir = a()->instances().at(i);
    if (ir.user_number() == user_number && ir.online()) {
      if (wi) {
        *wi = i;
      }
      return true;
    }
  }
  if (wi) {
    *wi = -1;
  }
  return false;
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

  const auto filename = fmt::sprintf("msg*.%3.3u", a()->instance_number());
  if (!File::ExistsWildcard(FilePath(a()->config()->datadir(), filename))) {
    last_iia = l;
    return false;
  }

  return true;
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
