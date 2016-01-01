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
#ifndef __INCLUDED_INSTMSG_H__
#define __INCLUDED_INSTMSG_H__

#include "sdk/vardec.h"

constexpr int INST_MSG_STRING = 1;  // A string to print out to the user
constexpr int INST_MSG_SHUTDOWN = 2;  // Hangs up, ends BBS execution
constexpr int INST_MSG_SYSMSG = 3;  // Message from the system, not a user
constexpr int INST_MSG_CLEANNET = 4;  // should call cleanup_net
constexpr int INST_MSG_CHAT = 6;  // External chat request

/****************************************************************************/


// (moved from vardec.h)

/* Instance status flags */
constexpr int INST_FLAGS_NONE = 0x0000;  // No flags at all
constexpr int INST_FLAGS_ONLINE = 0x0001;  // User online
constexpr int INST_FLAGS_MSG_AVAIL = 0x0002;  // Available for inst msgs
constexpr int INST_FLAGS_INVIS = 0x0004;  // For invisibility

/* Instance primary location points */
constexpr int INST_LOC_DOWN = 0;
constexpr int INST_LOC_INIT = 1;
constexpr int INST_LOC_EMAIL = 2;
constexpr int INST_LOC_MAIN = 3;
constexpr int INST_LOC_XFER = 4;
constexpr int INST_LOC_CHAINS = 5;
constexpr int INST_LOC_NET = 6;
constexpr int INST_LOC_GFILES = 7;
constexpr int INST_LOC_BEGINDAY = 8;
constexpr int INST_LOC_EVENT = 9;
constexpr int INST_LOC_CHAT = 10;
constexpr int INST_LOC_CHAT2 = 11;
constexpr int INST_LOC_CHATROOM = 12;
constexpr int INST_LOC_LOGON = 13;
constexpr int INST_LOC_LOGOFF = 14;
constexpr int INST_LOC_FSED = 15;
constexpr int INST_LOC_UEDIT = 16;
constexpr int INST_LOC_CHAINEDIT = 17;
constexpr int INST_LOC_BOARDEDIT = 18;
constexpr int INST_LOC_DIREDIT = 19;
constexpr int INST_LOC_GFILEEDIT = 20;
constexpr int INST_LOC_CONFEDIT = 21;
constexpr int INST_LOC_DOS = 22;
constexpr int INST_LOC_DEFAULTS = 23;
constexpr int INST_LOC_REBOOT = 24;
constexpr int INST_LOC_RELOAD = 25;
constexpr int INST_LOC_VOTE = 26;
constexpr int INST_LOC_BANK = 27;
constexpr int INST_LOC_AMSG = 28;
constexpr int INST_LOC_SUBS = 29;
constexpr int INST_LOC_CHUSER = 30;
constexpr int INST_LOC_TEDIT = 31;
constexpr int INST_LOC_MAILR = 32;
constexpr int INST_LOC_RESETQSCAN = 33;
constexpr int INST_LOC_VOTEEDIT = 34;
constexpr int INST_LOC_VOTEPRINT = 35;
constexpr int INST_LOC_RESETF = 36;
constexpr int INST_LOC_FEEDBACK = 37;
constexpr int INST_LOC_KILLEMAIL = 38;
constexpr int INST_LOC_POST = 39;
constexpr int INST_LOC_NEWUSER = 40;
constexpr int INST_LOC_RMAIL = 41;
constexpr int INST_LOC_DOWNLOAD = 42;
constexpr int INST_LOC_UPLOAD = 43;
constexpr int INST_LOC_BIXFER = 44;
constexpr int INST_LOC_NETLIST = 45;
constexpr int INST_LOC_TERM = 46;
constexpr int INST_LOC_EVENTEDIT = 47;
constexpr int INST_LOC_GETUSER = 48;
constexpr int INST_LOC_QWK = 49;
constexpr int INST_LOC_CH1 = 5000;
constexpr int INST_LOC_CH2 = 5001;
constexpr int INST_LOC_CH3 = 5002;
constexpr int INST_LOC_CH5 = 5004;
constexpr int INST_LOC_CH6 = 5005;
constexpr int INST_LOC_CH7 = 5006;
constexpr int INST_LOC_CH8 = 5007;
constexpr int INST_LOC_CH9 = 5008;
constexpr int INST_LOC_CH10 = 5009;
constexpr int INST_LOC_WFC = 65535;


/****************************************************************************/


/*
 * Structure for inter-instance messages. File would be comprised of headers
 * using the following structure, followed by the "message" (if any).
 */
struct inst_msg_header {
  unsigned short
  main,                    // Message main type
  minor,                   // Message minor type
  from_inst,               // Originating instance
  from_user,               // Originating sess->usernum
  dest_inst;               // Destination instance
  unsigned long
  daten,                   // Secs-since-1970 Unix datetime
  msg_size,                // Length of the "message"
  flags;                   // Bit-mapped flags
};

void send_inst_str(int whichinst, const char *send_string);
void send_inst_shutdown(int whichinst);
void send_inst_cleannet();
void broadcast(const char *fmt, ...);
void process_inst_msgs();
bool get_inst_info(int nInstanceNum, instancerec * ir);
int  num_instances();
bool user_online(int user_number, int *wi);
void instance_edit();
void write_inst(int loc, int subloc, int flags);
bool inst_msg_waiting();
int  setiia(int poll_ticks);
void toggle_invis();
void toggle_avail();
bool is_chat_invis();

#endif // __INCLUDED_INSTMSG_H__

