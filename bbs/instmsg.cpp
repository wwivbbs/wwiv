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
#include "instmsg.h"

#include <cstdarg>

#include "bbs/wwiv.h"
#include "core/wfndfile.h"
#include "core/wwivassert.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"

// Local functions
void send_inst_msg(inst_msg_header * ih, const char *msg);
int  handle_inst_msg(inst_msg_header * ih, const char *msg);
void send_inst_str1(int m, int whichinst, const char *pszSendString);
bool inst_available(instancerec * ir);
bool inst_available_chat(instancerec * ir);

using wwiv::bbs::TempDisablePause;

void send_inst_msg(inst_msg_header *ih, const char *msg) {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "%sTMSG%3.3u.%3.3d", syscfg.datadir, GetApplication()->GetInstanceNumber(), ih->dest_inst);
  WFile file(szFileName);
  if (file.Open(WFile::modeBinary | WFile::modeReadWrite | WFile::modeCreateFile)) {
    file.Seek(0L, WFile::seekEnd);
    if (ih->msg_size > 0 && !msg) {
      ih->msg_size = 0;
    }
    file.Write(ih, sizeof(inst_msg_header));
    if (ih->msg_size > 0) {
      file.Write(msg, ih->msg_size);
    }
    file.Close();

    for (int i = 0; i < 1000; i++) {
      char szMsgFileName[ MAX_PATH ];
      sprintf(szMsgFileName, "%sMSG%5.5d.%3.3d", syscfg.datadir, i, ih->dest_inst);
      if (!WFile::Rename(szFileName, szMsgFileName) || (errno != EACCES)) {
        break;
      }
    }
  }
}

#define LAST(s) s[strlen(s)-1]

void send_inst_str1(int m, int whichinst, const char *pszSendString) {
  inst_msg_header ih;
  char szTempSendString[ 1024 ];

  sprintf(szTempSendString, "%s\r\n", pszSendString);
  ih.main = static_cast<unsigned short>(m);
  ih.minor = 0;
  ih.from_inst = static_cast<unsigned short>(GetApplication()->GetInstanceNumber());
  ih.from_user = static_cast<unsigned short>(GetSession()->usernum);
  ih.msg_size = strlen(szTempSendString) + 1;
  ih.dest_inst = static_cast<unsigned short>(whichinst);
  ih.daten = static_cast<unsigned long>(time(nullptr));

  send_inst_msg(&ih, szTempSendString);
}

void send_inst_str(int whichinst, const char *pszSendString) {
  send_inst_str1(INST_MSG_STRING, whichinst, pszSendString);
}

void send_inst_sysstr(int whichinst, const char *pszSendString) {
  send_inst_str1(INST_MSG_SYSMSG, whichinst, pszSendString);
}

void send_inst_shutdown(int whichinst) {
  inst_msg_header ih;

  ih.main = INST_MSG_SHUTDOWN;
  ih.minor = 0;
  ih.from_inst = static_cast<unsigned short>(GetApplication()->GetInstanceNumber());
  ih.from_user = static_cast<unsigned short>(GetSession()->usernum);
  ih.msg_size = 0;
  ih.dest_inst = static_cast<unsigned short>(whichinst);
  ih.daten = static_cast<unsigned long>(time(nullptr));

  send_inst_msg(&ih, nullptr);
}

void send_inst_cleannet() {
  inst_msg_header ih;
  instancerec ir;

  if (GetApplication()->GetInstanceNumber() == 1) {
    return;
  }

  get_inst_info(1, &ir);
  if (ir.loc == INST_LOC_WFC) {
    ih.main = INST_MSG_CLEANNET;
    ih.minor = 0;
    ih.from_inst = static_cast<unsigned short>(GetApplication()->GetInstanceNumber());
    ih.from_user = 1;
    ih.msg_size = 0;
    ih.dest_inst = 1;
    ih.daten = static_cast<unsigned long>(time(nullptr));

    send_inst_msg(&ih, nullptr);
  }
}


/*
 * "Broadcasts" a message to all online instances.
 */
void _broadcast(char *pszSendString) {
  instancerec ir;

  int ni = num_instances();
  for (int i = 1; i <= ni; i++) {
    if (i == GetApplication()->GetInstanceNumber()) {
      continue;
    }
    if (get_inst_info(i, &ir)) {
      if (inst_available(&ir)) {
        send_inst_str(i, pszSendString);
      }
    }
  }
}

void broadcast(const char *fmt, ...) {
  va_list ap;
  char szBuffer[2048];

  va_start(ap, fmt);
  vsnprintf(szBuffer, sizeof(szBuffer), fmt, ap);
  va_end(ap);
  _broadcast(szBuffer);
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
    if (ih->msg_size > 0 && GetSession()->IsUserOnline() && !hangup) {
      GetSession()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
      bout.nl(2);
      if (in_chatroom) {
        i = 0;
        while (i < ih->msg_size) {
          bputch(msg[ i++ ]);
        }
        bout.nl();
        RestoreCurrentLine(cl, atr, xl, &cc);
        return (ih->main);
      }
      if (ih->main == INST_MSG_STRING) {
        WUser user;
        GetApplication()->GetUserManager()->ReadUser(&user, ih->from_user);
        bout.bprintf("|#1%.12s (%d)|#0> |#2", user.GetUserNameAndNumber(ih->from_user), ih->from_inst);
      } else {
        bout << "|#6[SYSTEM ANNOUNCEMENT] |#7> |#2";
      }
      i = 0;
      while (i < ih->msg_size) {
        bputch(msg[i++]);
      }
      bout.nl(2);
      RestoreCurrentLine(cl, atr, xl, &cc);
    }
    break;
  case INST_MSG_CLEANNET:
    GetApplication()->SetCleanNetNeeded(true);
    break;
  // Handle this one in process_inst_msgs
  case INST_MSG_SHUTDOWN:
  default:
    break;
  }
  return ih->main;
}


void process_inst_msgs() {
  if (x_only || !inst_msg_waiting()) {
    return;
  }
  last_iia = timer1();

  int oiia = setiia(0);
  char* m = nullptr;
  char szFindFileName[MAX_PATH];
  inst_msg_header ih;
  WFindFile fnd;

  sprintf(szFindFileName, "%sMSG*.%3.3u", syscfg.datadir, GetApplication()->GetInstanceNumber());
  bool bDone = fnd.open(szFindFileName, 0);
  while ((bDone) && (!hangup)) {
    WFile file(syscfg.datadir, fnd.GetFileName());
    if (!file.Open(WFile::modeBinary | WFile::modeReadOnly)) {
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
        WWIV_ASSERT(m);
        if (m == nullptr) {
          break;
        }
        file.Read(m, ih.msg_size);
        lFilePos += ih.msg_size;
      }
      int hi = handle_inst_msg(&ih, m);
      if (m) {
        free(m);
        m = nullptr;
      }
      if (hi == INST_MSG_SHUTDOWN) {
        if (GetSession()->IsUserOnline()) {
          TempDisablePause diable_pause;
          bout.nl(2);
          printfile(OFFLINE_NOEXT);
          if (GetSession()->IsUserOnline()) {
            if (GetSession()->usernum == 1) {
              fwaiting = GetSession()->GetCurrentUser()->GetNumMailWaiting();
            }
            GetSession()->WriteCurrentUser();
            write_qscn(GetSession()->usernum, qsc, false);
          }
        }
        file.Close();
        file.Delete();
        GetSession()->localIO()->SetTopLine(0);
        GetSession()->localIO()->LocalCls();
        hangup = true;
        hang_it_up();
        holdphone(false);
        Wait(1);
        GetApplication()->QuitBBS();
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

  WFile instFile(syscfg.datadir, INSTANCE_DAT);
  if (!instFile.Open(WFile::modeBinary | WFile::modeReadOnly)) {
    return false;
  }
  int i = static_cast<int>(instFile.GetLength() / sizeof(instancerec));
  if (i < (nInstanceNum + 1)) {
    instFile.Close();
    return false;
  }
  instFile.Seek(static_cast<long>(nInstanceNum * sizeof(instancerec)), WFile::seekBegin);
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
  WFile instFile(syscfg.datadir, INSTANCE_DAT);
  if (!instFile.Open(WFile::modeReadOnly | WFile::modeBinary)) {
    return 0;
  }
  int nNumInstances = static_cast<int>(instFile.GetLength() / sizeof(instancerec)) - 1;
  instFile.Close();
  return nNumInstances;
}


/*
 * Returns 1 if GetSession()->usernum nUserNumber is online, and returns instance user is on in
 * wi, else returns 0.
 */
bool user_online(int nUserNumber, int *wi) {
  int ni = num_instances();
  for (int i = 1; i <= ni; i++) {
    if (i == GetApplication()->GetInstanceNumber()) {
      continue;
    }
    instancerec ir;
    get_inst_info(i, &ir);
    if (ir.user == nUserNumber && (ir.flags & INST_FLAGS_ONLINE)) {
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
      if (i == GetApplication()->GetInstanceNumber()) {
        GetSession()->localIO()->SetTopLine(0);
        GetSession()->localIO()->LocalCls();
        hangup = true;
        hang_it_up();
        holdphone(false);
        Wait(1);
        GetApplication()->QuitBBS();
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
          if (i1 != GetApplication()->GetInstanceNumber()) {
            if (get_inst_info(i1, &ir) && ir.loc != INST_LOC_DOWN) {
              send_inst_shutdown(i1);
            }
          }
        }
        GetSession()->localIO()->SetTopLine(0);
        GetSession()->localIO()->LocalCls();
        hangup = true;
        hang_it_up();
        holdphone(false);
        Wait(1);
        GetApplication()->QuitBBS();
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
    if (get_inst_info(GetApplication()->GetInstanceNumber(), &ir)) {
      ti.user = ir.user;
    } else {
      ti.user = 1;
    }
    ti.inst_started = static_cast<unsigned long>(time(nullptr));
    re_write = true;
  }

  unsigned short cf = ti.flags & (~(INST_FLAGS_ONLINE | INST_FLAGS_MSG_AVAIL));
  if (GetSession()->IsUserOnline()) {
    cf |= INST_FLAGS_ONLINE;
    if (invis) {
      cf |= INST_FLAGS_INVIS;
    }
    if (!GetSession()->GetCurrentUser()->IsIgnoreNodeMessages()) {
      switch (loc) {
      case INST_LOC_MAIN:
      case INST_LOC_XFER:
      case INST_LOC_SUBS:
      case INST_LOC_EMAIL:
      case INST_LOC_CHATROOM:
      case INST_LOC_RMAIL:
        if (avail) {
          cf |= INST_FLAGS_MSG_AVAIL;
        }
        break;
      default:
        if ((loc >= INST_LOC_CH1) && (loc <= INST_LOC_CH10) && avail) {
          cf |= INST_FLAGS_MSG_AVAIL;
        }
        break;
      }
    }
    unsigned short ms = (GetSession()->using_modem) ? modem_speed : 0;
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
  if ((ti.flags & INST_FLAGS_INVIS) && (!invis)) {
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
  if (ti.number != GetApplication()->GetInstanceNumber()) {
    re_write = true;
    ti.number = static_cast<short>(GetApplication()->GetInstanceNumber());
  }
  if (loc == INST_LOC_DOWN) {
    re_write = true;
  } else {
    if (GetSession()->IsUserOnline()) {
      if (ti.user != GetSession()->usernum) {
        re_write = true;
        if ((GetSession()->usernum > 0) && (GetSession()->usernum <= syscfg.maxusers)) {
          ti.user = static_cast<short>(GetSession()->usernum);
        }
      }
    }
  }

  if (ti.subloc != static_cast<unsigned short>(subloc)) {
    re_write = true;
    ti.subloc = static_cast<unsigned short>(subloc);
  }
  if (ti.loc != static_cast<unsigned short>(loc)) {
    re_write = true;
    ti.loc = static_cast<unsigned short>(loc);
  }
  if ((((ti.flags & INST_FLAGS_INVIS) && (!invis)) ||
       ((!(ti.flags & INST_FLAGS_INVIS)) && (invis))) ||
      (((ti.flags & INST_FLAGS_MSG_AVAIL) && (!avail)) ||
       ((!(ti.flags & INST_FLAGS_MSG_AVAIL)) && (avail))) &&
      (ti.loc != INST_LOC_WFC)) {
    re_write = true;
  }
  if (re_write && syscfg.datadir != nullptr) {
    ti.last_update = static_cast<unsigned long>(time(nullptr));
    WFile instFile(syscfg.datadir, INSTANCE_DAT);
    if (instFile.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile)) {
      instFile.Seek(static_cast<long>(GetApplication()->GetInstanceNumber() * sizeof(instancerec)), WFile::seekBegin);
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
  if (labs(l - last_iia) < iia) {
    return false;
  }

  char szFileName[81];
  sprintf(szFileName, "%sMSG*.%3.3u", syscfg.datadir, GetApplication()->GetInstanceNumber());
  bool bExist = WFile::ExistsWildcard(szFileName);
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

