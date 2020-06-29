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
#include "bbs/instmsg.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/datetime.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/utility.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include <chrono>
#include <string>

using std::string;
using std::chrono::seconds;
using std::chrono::steady_clock;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static bool chat_avail;
static bool chat_invis;

// Local functions
bool inst_available(instancerec * ir);
bool inst_available_chat(instancerec * ir);

using wwiv::bbs::TempDisablePause;

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

    for (int i = 0; i < 1000; i++) {
      const auto dest =
          FilePath(a()->config()->datadir(), fmt::sprintf("msg%5.5d.%3.3d", i, ih->dest_inst));
      if (!File::Rename(file.path(), dest) || (errno != EACCES)) {
        break;
      }
    }
  }
}

static void send_inst_str1(int m, int whichinst, const std::string& send_string) {
  inst_msg_header ih;

  string tempsendstring = StrCat(send_string, "\r\n");
  ih.main = static_cast<uint16_t>(m);
  ih.minor = 0;
  ih.from_inst = static_cast<uint16_t>(a()->instance_number());
  ih.from_user = static_cast<uint16_t>(a()->usernum);
  ih.msg_size = tempsendstring.size() + 1;
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
  instancerec ir;

  const auto ni = num_instances();
  for (auto i = 1; i <= ni; i++) {
    if (i == a()->instance_number()) {
      continue;
    }
    if (get_inst_info(i, &ir)) {
      if (inst_available(&ir)) {
        send_inst_str(i, message);
      }
    }
  }
}


/*
 * Handles one inter-instance message, based on type, returns inter-instance
 * main type of the "packet".
 */
static int handle_inst_msg(inst_msg_header * ih, const std::string& msg) {
  unsigned short i;

  if (!ih || (ih->msg_size > 0 && msg.empty())) {
    return -1;
  }

  switch (ih->main) {
  case INST_MSG_STRING:
  case INST_MSG_SYSMSG:
    if (ih->msg_size > 0 && a()->IsUserOnline() && !a()->hangup_) {
      const auto line = bout.SaveCurrentLine();
      bout.nl(2);
      if (a()->in_chatroom_) {
        i = 0;
        bout << msg << "\r\n";
        bout.RestoreCurrentLine(line);
        return ih->main;
      }
      if (ih->main == INST_MSG_STRING) {
        const auto from_user_name = a()->names()->UserName(ih->from_user);
        bout << fmt::sprintf("|#1%.12s (%d)|#0> |#2", from_user_name, ih->from_inst);
      } else {
        bout << "|#6[SYSTEM ANNOUNCEMENT] |#7> |#2";
      }
      i = 0;
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
  auto oiia = setiia(std::chrono::milliseconds(0));

  string fndspec = fmt::sprintf("%smsg*.%3.3u", a()->config()->datadir(), a()->instance_number());
  FindFiles ff(fndspec, FindFilesType::files);
  for (const auto& f : ff) {
    if (a()->hangup_) { break; }
    File file(FilePath(a()->config()->datadir(), f.name));
    if (!file.Open(File::modeBinary | File::modeReadOnly, File::shareDenyReadWrite)) {
      LOG(ERROR) << "Unable to open file: " << file;
      continue;
    }
    while (true) {
      inst_msg_header ih = {};
      auto num_read = file.Read(&ih, sizeof(inst_msg_header));
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
      handle_inst_msg(&ih, m.c_str());
    }
    file.Close();
    File::Remove(file.path());
  }
  setiia(oiia);
}

// Gets instancerec for specified instance, returns in ir.
bool get_inst_info(int nInstanceNum, instancerec * ir) {
  if (!ir || a()->config()->datadir().empty()) {
    return false;
  }

  memset(ir, 0, sizeof(instancerec));

  File instFile(FilePath(a()->config()->datadir(), INSTANCE_DAT));
  if (!instFile.Open(File::modeBinary | File::modeReadOnly)) {
    return false;
  }
  auto i = instFile.length() / sizeof(instancerec);
  if (i < static_cast<size_t>(nInstanceNum + 1)) {
    instFile.Close();
    return false;
  }
  instFile.Seek(static_cast<long>(nInstanceNum * sizeof(instancerec)), File::Whence::begin);
  instFile.Read(ir, sizeof(instancerec));
  instFile.Close();
  return true;
}


/*
 * Returns 1 if inst_no has user online that can receive msgs, else 0
 */
bool inst_available(instancerec * ir) {
  if (!ir) {
    return false;
  }

  return ((ir->flags & INST_FLAGS_ONLINE) && (ir->flags & INST_FLAGS_MSG_AVAIL)) ? true : false;
}


/*
 * Returns 1 if inst_no has user online in chat, else 0
 */
bool inst_available_chat(instancerec * ir) {
  if (!ir) {
    return false;
  }

  return ((ir->flags & INST_FLAGS_ONLINE) &&
          (ir->flags & INST_FLAGS_MSG_AVAIL) &&
          (ir->loc == INST_LOC_CHATROOM)) ? true : false;
}


/*
 * Returns max instance number.
 */
int num_instances() {
  File instFile(FilePath(a()->config()->datadir(), INSTANCE_DAT));
  if (!instFile.Open(File::modeReadOnly | File::modeBinary)) {
    return 0;
  }
  auto nNumInstances = static_cast<int>(instFile.length() / sizeof(instancerec)) - 1;
  instFile.Close();
  return nNumInstances;
}


/*
 * Returns 1 if a()->usernum user_number is online, and returns instance user is on in
 * wi, else returns 0.
 */
bool user_online(int user_number, int *wi) {
  int ni = num_instances();
  for (int i = 1; i <= ni; i++) {
    if (i == a()->instance_number()) {
      continue;
    }
    instancerec ir;
    get_inst_info(i, &ir);
    if (ir.user == user_number && (ir.flags & INST_FLAGS_ONLINE)) {
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
  static instancerec ti{};
  instancerec ir{};

  bool re_write = false;
  if (ti.user == 0) {
    if (get_inst_info(a()->instance_number(), &ir)) {
      ti.user = ir.user;
    } else {
      ti.user = 1;
    }
    ti.inst_started = daten_t_now();
    re_write = true;
  }

  unsigned short cf = ti.flags & (~(INST_FLAGS_ONLINE | INST_FLAGS_MSG_AVAIL));
  if (a()->IsUserOnline()) {
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
    uint16_t ms = static_cast<uint16_t>(a()->using_modem ? a()->modem_speed_ : 0);
    if (ti.modem_speed != ms) {
      ti.modem_speed = ms;
      re_write = true;
    }
  }
  if (flags != INST_FLAGS_NONE) {
    if (flags & 0x8000) {
      // reset an option
      ti.flags &= flags;
    } else {
      // set an option
      ti.flags |= flags;
    }
  }
  if ((ti.flags & INST_FLAGS_INVIS) && (!chat_invis)) {
    cf = 0;
    if (ti.flags & INST_FLAGS_ONLINE) {
      cf |= INST_FLAGS_ONLINE;
    }
    if (ti.flags & INST_FLAGS_MSG_AVAIL) {
      cf |= INST_FLAGS_MSG_AVAIL;
    }
    re_write = true;
  }
  if (cf != ti.flags) {
    re_write = true;
    ti.flags = cf;
  }
  if (ti.number != a()->instance_number()) {
    re_write = true;
    ti.number = static_cast<int16_t>(a()->instance_number());
  }
  if (loc == INST_LOC_DOWN) {
    re_write = true;
  } else {
    if (a()->IsUserOnline()) {
      if (ti.user != a()->usernum) {
        re_write = true;
        if (a()->usernum > 0 && a()->usernum <= a()->config()->max_users()) {
          ti.user = static_cast<int16_t>(a()->usernum);
        }
      }
    }
  }

  if (ti.subloc != static_cast<uint16_t>(subloc)) {
    re_write = true;
    ti.subloc = static_cast<uint16_t>(subloc);
  }
  if (ti.loc != static_cast<uint16_t>(loc)) {
    re_write = true;
    ti.loc = static_cast<uint16_t>(loc);
  }
  if ((((ti.flags & INST_FLAGS_INVIS) && (!chat_invis)) ||
       ((!(ti.flags & INST_FLAGS_INVIS)) && (chat_invis))) ||
      (((ti.flags & INST_FLAGS_MSG_AVAIL) && (!chat_avail)) ||
       ((!(ti.flags & INST_FLAGS_MSG_AVAIL)) && (chat_avail))) &&
      (ti.loc != INST_LOC_WFC)) {
    re_write = true;
  }
  if (re_write) {
    ti.last_update = daten_t_now();
    File instFile(FilePath(a()->config()->datadir(), INSTANCE_DAT));
    if (instFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      instFile.Seek(static_cast<long>(a()->instance_number() * sizeof(instancerec)), File::Whence::begin);
      instFile.Write(&ti, sizeof(instancerec));
      instFile.Close();
    }
  }
}

/*
* Returns 1 if a message waiting for this instance, 0 otherwise.
*/
bool inst_msg_waiting() {
  if (iia.count() == 0) return false;

  auto l = steady_clock::now();
  if ((l - last_iia) < iia) {
    return false;
  }

  const string filename = fmt::sprintf("msg*.%3.3u", a()->instance_number());
  if (!File::ExistsWildcard(FilePath(a()->config()->datadir(), filename))) {
    last_iia = l;
    return false;
  }

  return true;
}

// Sets inter-instance availability on/off, for inter-instance messaging.
// retruns the old iia value.
std::chrono::milliseconds setiia(std::chrono::milliseconds poll_time) {
  auto oiia = iia;
  iia = poll_time;
  return oiia;
}

// Toggles user availability, called when ctrl-N is hit

void toggle_avail() {
  instancerec ir = {};

  SavedLine line = bout.SaveCurrentLine();
  get_inst_info(a()->instance_number(), &ir);
  chat_avail = !chat_avail;

  bout << "\n\rYou are now ";
  bout << (chat_avail ? "available for chat.\n\r\n" : "not available for chat.\n\r\n");
  write_inst(ir.loc, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  bout.RestoreCurrentLine(line);
}

// Toggles invisibility, called when ctrl-L is hit by a sysop

void toggle_invis() {
  instancerec ir;

  SavedLine line = bout.SaveCurrentLine();
  get_inst_info(a()->instance_number(), &ir);
  chat_invis = !chat_invis;

  bout << "\r\n|#1You are now ";
  bout << (chat_invis ? "invisible.\n\r\n" : "visible.\n\r\n");
  write_inst(ir.loc, a()->current_user_sub().subnum, INST_FLAGS_NONE);
  bout.RestoreCurrentLine(line);
}
