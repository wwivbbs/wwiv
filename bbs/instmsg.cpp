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
#include "bbs/instmsg.h"

#include <chrono>
#include <cstdarg>
#include <string>

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/multinst.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/os.h"
#include "core/wfndfile.h"
#include "core/wwivassert.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "sdk/filenames.h"

using std::chrono::seconds;
using std::string;
using namespace wwiv::os;

static bool chat_avail;
static bool chat_invis;

// Local functions
void send_inst_msg(inst_msg_header * ih, const char *msg);
int  handle_inst_msg(inst_msg_header * ih, const char *msg);
void send_inst_str1(int m, int whichinst, const char *send_string);
bool inst_available(instancerec * ir);
bool inst_available_chat(instancerec * ir);

using wwiv::bbs::TempDisablePause;

static int32_t last_iia = 0;

bool is_chat_invis() { return chat_invis; }
void send_inst_msg(inst_msg_header *ih, const char *msg) {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "%sTMSG%3.3u.%3.3d", syscfg.datadir, session()->instance_number(), ih->dest_inst);
  File file(szFileName);
  if (file.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
    file.Seek(0L, File::seekEnd);
    if (ih->msg_size > 0 && !msg) {
      ih->msg_size = 0;
    }
    file.Write(ih, sizeof(inst_msg_header));
    if (ih->msg_size > 0) {
      file.Write(msg, ih->msg_size);
    }
    file.Close();

    for (int i = 0; i < 1000; i++) {
      char szMsgFileName[MAX_PATH];
      sprintf(szMsgFileName, "%sMSG%5.5d.%3.3d", syscfg.datadir, i, ih->dest_inst);
      if (!File::Rename(szFileName, szMsgFileName) || (errno != EACCES)) {
        break;
      }
    }
  }
}

#define LAST(s) s[strlen(s)-1]

void send_inst_str1(int m, int whichinst, const char *send_string) {
  inst_msg_header ih;
  char szTempSendString[1024];

  sprintf(szTempSendString, "%s\r\n", send_string);
  ih.main = static_cast<uint16_t>(m);
  ih.minor = 0;
  ih.from_inst = static_cast<uint16_t>(session()->instance_number());
  ih.from_user = static_cast<uint16_t>(session()->usernum);
  ih.msg_size = strlen(szTempSendString) + 1;
  ih.dest_inst = static_cast<uint16_t>(whichinst);
  ih.daten = static_cast<uint32_t>(time(nullptr));

  send_inst_msg(&ih, szTempSendString);
}

void send_inst_str(int whichinst, const char *send_string) {
  send_inst_str1(INST_MSG_STRING, whichinst, send_string);
}

void send_inst_sysstr(int whichinst, const char *send_string) {
  send_inst_str1(INST_MSG_SYSMSG, whichinst, send_string);
}

void send_inst_shutdown(int whichinst) {
  inst_msg_header ih;

  ih.main = INST_MSG_SHUTDOWN;
  ih.minor = 0;
  ih.from_inst = static_cast<uint16_t>(session()->instance_number());
  ih.from_user = static_cast<uint16_t>(session()->usernum);
  ih.msg_size = 0;
  ih.dest_inst = static_cast<uint16_t>(whichinst);
  ih.daten = static_cast<uint32_t>(time(nullptr));

  send_inst_msg(&ih, nullptr);
}

void send_inst_cleannet() {
  inst_msg_header ih;
  instancerec ir;

  if (session()->instance_number() == 1) {
    return;
  }

  get_inst_info(1, &ir);
  if (ir.loc == INST_LOC_WFC) {
    ih.main = INST_MSG_CLEANNET;
    ih.minor = 0;
    ih.from_inst = static_cast<uint16_t>(session()->instance_number());
    ih.from_user = 1;
    ih.msg_size = 0;
    ih.dest_inst = 1;
    ih.daten = static_cast<uint32_t>(time(nullptr));

    send_inst_msg(&ih, nullptr);
  }
}


/*
 * "Broadcasts" a message to all online instances.
 */
void broadcast(const std::string& message) {
  instancerec ir;

  int ni = num_instances();
  for (int i = 1; i <= ni; i++) {
    if (i == session()->instance_number()) {
      continue;
    }
    if (get_inst_info(i, &ir)) {
      if (inst_available(&ir)) {
        send_inst_str(i, message.c_str());
      }
    }
  }
}


/*
 * Handles one inter-instance message, based on type, returns inter-instance
 * main type of the "packet".
 */
int handle_inst_msg(inst_msg_header * ih, const char *msg) {
  unsigned short i;
  char xl[81], cl[81], atr[81], cc;

  if (!ih || (ih->msg_size > 0 && msg == nullptr)) {
    return -1;
  }

  switch (ih->main) {
  case INST_MSG_STRING:
  case INST_MSG_SYSMSG:
    if (ih->msg_size > 0 && session()->IsUserOnline() && !hangup) {
      session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
      bout.nl(2);
      if (in_chatroom) {
        i = 0;
        while (i < ih->msg_size) {
          bout.bputch(msg[ i++ ]);
        }
        bout.nl();
        RestoreCurrentLine(cl, atr, xl, &cc);
        return (ih->main);
      }
      if (ih->main == INST_MSG_STRING) {
        const string from_user_name = session()->names()->UserName(ih->from_user);
        bout.bprintf("|#1%.12s (%d)|#0> |#2", 
          from_user_name.c_str(), ih->from_inst);
      } else {
        bout << "|#6[SYSTEM ANNOUNCEMENT] |#7> |#2";
      }
      i = 0;
      while (i < ih->msg_size) {
        bout.bputch(msg[i++]);
      }
      bout.nl(2);
      RestoreCurrentLine(cl, atr, xl, &cc);
    }
    break;
  case INST_MSG_CLEANNET:
    session()->SetCleanNetNeeded(true);
    break;
  // Handle this one in process_inst_msgs
  case INST_MSG_SHUTDOWN:
  default:
    break;
  }
  return ih->main;
}


void process_inst_msgs() {
  if (!inst_msg_waiting()) {
    return;
  }
  last_iia = timer1();

  int oiia = setiia(0);
  char* m = nullptr;
  char szFindFileName[MAX_PATH];
  inst_msg_header ih;
  WFindFile fnd;

  sprintf(szFindFileName, "%sMSG*.%3.3u", syscfg.datadir, session()->instance_number());
  bool bDone = fnd.open(szFindFileName, 0);
  while ((bDone) && (!hangup)) {
    File file(session()->config()->datadir(), fnd.GetFileName());
    if (!file.Open(File::modeBinary | File::modeReadOnly)) {
      continue;
    }
    long lFileSize = file.GetLength();
    long lFilePos = 0L;
    while (lFilePos < lFileSize) {
      m = nullptr;
      file.Read(&ih, sizeof(inst_msg_header));
      lFilePos += sizeof(inst_msg_header);
      if (ih.msg_size > 0) {
        m = static_cast<char*>(BbsAllocA(ih.msg_size));
        file.Read(m, ih.msg_size);
        lFilePos += ih.msg_size;
      }
      int hi = handle_inst_msg(&ih, m);
      if (m) {
        free(m);
        m = nullptr;
      }
      if (hi == INST_MSG_SHUTDOWN) {
        if (session()->IsUserOnline()) {
          TempDisablePause diable_pause;
          bout.nl(2);
          printfile(OFFLINE_NOEXT);
          if (session()->IsUserOnline()) {
            session()->WriteCurrentUser();
            write_qscn(session()->usernum, qsc, false);
          }
        }
        file.Close();
        file.Delete();
        session()->localIO()->SetTopLine(0);
        session()->localIO()->Cls();
        hangup = true;
        hang_it_up();
        sleep_for(seconds(1));
        session()->QuitBBS();
      }
    }
    file.Close();
    file.Delete();
    bDone = fnd.next();
  }
  setiia(oiia);
}

// Gets instancerec for specified instance, returns in ir.
bool get_inst_info(int nInstanceNum, instancerec * ir) {
  if (!ir || syscfg.datadir == nullptr) {
    return false;
  }

  memset(ir, 0, sizeof(instancerec));

  File instFile(session()->config()->datadir(), INSTANCE_DAT);
  if (!instFile.Open(File::modeBinary | File::modeReadOnly)) {
    return false;
  }
  int i = static_cast<int>(instFile.GetLength() / sizeof(instancerec));
  if (i < (nInstanceNum + 1)) {
    instFile.Close();
    return false;
  }
  instFile.Seek(static_cast<long>(nInstanceNum * sizeof(instancerec)), File::seekBegin);
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
  File instFile(session()->config()->datadir(), INSTANCE_DAT);
  if (!instFile.Open(File::modeReadOnly | File::modeBinary)) {
    return 0;
  }
  int nNumInstances = static_cast<int>(instFile.GetLength() / sizeof(instancerec)) - 1;
  instFile.Close();
  return nNumInstances;
}


/*
 * Returns 1 if session()->usernum user_number is online, and returns instance user is on in
 * wi, else returns 0.
 */
bool user_online(int user_number, int *wi) {
  int ni = num_instances();
  for (int i = 1; i <= ni; i++) {
    if (i == session()->instance_number()) {
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
 * Allows sending some types of messages to other instances, or simply
 * viewing the status of the other instances.
 */
void instance_edit() {
  instancerec ir;

  if (!ValidateSysopPassword()) {
    return;
  }

  int ni = num_instances();

  bool done = false;
  while (!done && !hangup) {
    CheckForHangup();
    bout.nl();
    bout << "|#21|#7)|#1 Multi-Instance Status\r\n";
    bout << "|#22|#7)|#1 Shut Down One Instance\r\n";
    bout << "|#23|#7)|#1 Shut Down ALL Instances\r\n";
    bout << "|#2Q|#7)|#1 Quit\r\n";
    bout.nl();
    bout << "|#1Select: ";
    char ch = onek("Q123");
    switch (ch) {
    case '1':
      bout.nl();
      bout << "|#1Instance Status:\r\n";
      multi_instance();
      break;
    case '2': {
      bout.nl();
      bout << "|#2Which Instance: ";
      char szInst[ 10 ];
      input(szInst, 3, true);
      if (!szInst[0]) {
        break;
      }
      int i = atoi(szInst);
      if (!i || i > ni) {
        bout.nl();
        bout << "|#6Instance unavailable.\r\n";
        break;
      }
      if (i == session()->instance_number()) {
        session()->localIO()->SetTopLine(0);
        session()->localIO()->Cls();
        hangup = true;
        hang_it_up();
        sleep_for(seconds(1));
        session()->QuitBBS();
        break;
      }
      if (get_inst_info(i, &ir)) {
        if (ir.loc != INST_LOC_DOWN) {
          bout.nl();
          bout << "|#2Shutting down instance " << i << wwiv::endl;
          send_inst_shutdown(i);
        } else {
          bout << "\r\n|#6Instance already shut down.\r\n";
        }
      } else {
        bout << "\r\n|#6Instance unavailable.\r\n";
      }
    }
    break;
    case '3':
      bout.nl();
      bout << "|#5Are you sure? ";
      if (yesno()) {
        bout << "\r\n|#2Shutting down all instances.\r\n";
        for (int i1 = 1; i1 <= ni; i1++) {
          if (i1 != session()->instance_number()) {
            if (get_inst_info(i1, &ir) && ir.loc != INST_LOC_DOWN) {
              send_inst_shutdown(i1);
            }
          }
        }
        session()->localIO()->SetTopLine(0);
        session()->localIO()->Cls();
        hangup = true;
        hang_it_up();
        sleep_for(seconds(1));
        session()->QuitBBS();
      }
      break;
    case 'Q':
    default:
      done = true;
      break;
    }
  }
}

/*
* Writes BBS location data to instance.dat, so other instances can see
* some info about this instance.
*/
void write_inst(int loc, int subloc, int flags) {
  static instancerec ti;
  instancerec ir;

  bool re_write = false;
  if (ti.user == 0) {
    if (get_inst_info(session()->instance_number(), &ir)) {
      ti.user = ir.user;
    } else {
      ti.user = 1;
    }
    ti.inst_started = static_cast<uint32_t>(time(nullptr));
    re_write = true;
  }

  unsigned short cf = ti.flags & (~(INST_FLAGS_ONLINE | INST_FLAGS_MSG_AVAIL));
  if (session()->IsUserOnline()) {
    cf |= INST_FLAGS_ONLINE;
    if (chat_invis) {
      cf |= INST_FLAGS_INVIS;
    }
    if (!session()->user()->IsIgnoreNodeMessages()) {
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
    uint16_t ms = static_cast<uint16_t>(session()->using_modem ? modem_speed : 0);
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
  if (ti.number != session()->instance_number()) {
    re_write = true;
    ti.number = static_cast<int16_t>(session()->instance_number());
  }
  if (loc == INST_LOC_DOWN) {
    re_write = true;
  } else {
    if (session()->IsUserOnline()) {
      if (ti.user != session()->usernum) {
        re_write = true;
        if (session()->usernum > 0 && session()->usernum <= syscfg.maxusers) {
          ti.user = static_cast<int16_t>(session()->usernum);
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
    ti.last_update = static_cast<uint32_t>(time(nullptr));
    File instFile(session()->config()->datadir(), INSTANCE_DAT);
    if (instFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      instFile.Seek(static_cast<long>(session()->instance_number() * sizeof(instancerec)), File::seekBegin);
      instFile.Write(&ti, sizeof(instancerec));
      instFile.Close();
    }
  }
}

/*
* Returns 1 if a message waiting for this instance, 0 otherwise.
*/
bool inst_msg_waiting() {
  if (!iia || !local_echo) {
    return false;
  }

  long l = timer1();
  if (std::abs(l - last_iia) < iia) {
    return false;
  }

  char szFileName[81];
  sprintf(szFileName, "%sMSG*.%3.3u", syscfg.datadir, session()->instance_number());
  bool bExist = File::ExistsWildcard(szFileName);
  if (!bExist) {
    last_iia = l;
  }

  return bExist;
}

// Sets inter-instance availability on/off, for inter-instance messaging.
// retruns the old iia value.
int setiia(int poll_ticks) {
  int oiia = iia;
  iia = poll_ticks;
  return oiia;
}

// Toggles user availability, called when ctrl-N is hit

void toggle_avail() {
  instancerec ir;
  char xl[81], cl[81], atr[81], cc;

  session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
  get_inst_info(session()->instance_number(), &ir);
  chat_avail = !chat_avail;

  bout << "\n\rYou are now ";
  bout << (chat_avail ? "available for chat.\n\r\n" : "not available for chat.\n\r\n");
  write_inst(ir.loc, session()->current_user_sub().subnum, INST_FLAGS_NONE);
  RestoreCurrentLine(cl, atr, xl, &cc);
}

// Toggles invisibility, called when ctrl-L is hit by a sysop

void toggle_invis() {
  instancerec ir;
  char xl[81], cl[81], atr[81], cc;

  session()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
  get_inst_info(session()->instance_number(), &ir);
  chat_invis = !chat_invis;

  bout << "\r\n|#1You are now ";
  bout << (chat_invis ? "invisible.\n\r\n" : "visible.\n\r\n");
  write_inst(ir.loc, session()->current_user_sub().subnum, INST_FLAGS_NONE);
  RestoreCurrentLine(cl, atr, xl, &cc);
}
