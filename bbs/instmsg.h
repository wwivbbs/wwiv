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
#ifndef __INCLUDED_INSTMSG_H__
#define __INCLUDED_INSTMSG_H__

#define INST_MSG_STRING      1  // A string to print out to the user
#define INST_MSG_SHUTDOWN    2  // Hangs up, ends BBS execution
#define INST_MSG_SYSMSG      3  // Message from the system, not a user
#define INST_MSG_CLEANNET    4  // should call cleanup_net
#define INST_MSG_CHAT        6  // External chat request

/****************************************************************************/


// (moved from vardec.h)

/* Instance status flags */
#define INST_FLAGS_NONE      0x0000         // No flags at all
#define INST_FLAGS_ONLINE    0x0001         // User online
#define INST_FLAGS_MSG_AVAIL 0x0002         // Available for inst msgs
#define INST_FLAGS_INVIS     0x0004         // For invisibility

/* Instance primary location points */
#define INST_LOC_DOWN          0
#define INST_LOC_INIT          1
#define INST_LOC_EMAIL         2
#define INST_LOC_MAIN          3
#define INST_LOC_XFER          4
#define INST_LOC_CHAINS        5
#define INST_LOC_NET           6
#define INST_LOC_GFILES        7
#define INST_LOC_BEGINDAY      8
#define INST_LOC_EVENT         9
#define INST_LOC_CHAT         10
#define INST_LOC_CHAT2        11
#define INST_LOC_CHATROOM     12
#define INST_LOC_LOGON        13
#define INST_LOC_LOGOFF       14
#define INST_LOC_FSED         15
#define INST_LOC_UEDIT        16
#define INST_LOC_CHAINEDIT    17
#define INST_LOC_BOARDEDIT    18
#define INST_LOC_DIREDIT      19
#define INST_LOC_GFILEEDIT    20
#define INST_LOC_CONFEDIT     21
#define INST_LOC_DOS          22
#define INST_LOC_DEFAULTS     23
#define INST_LOC_REBOOT       24
#define INST_LOC_RELOAD       25
#define INST_LOC_VOTE         26
#define INST_LOC_BANK         27
#define INST_LOC_AMSG         28
#define INST_LOC_SUBS         29
#define INST_LOC_CHUSER       30
#define INST_LOC_TEDIT        31
#define INST_LOC_MAILR        32
#define INST_LOC_RESETQSCAN   33
#define INST_LOC_VOTEEDIT     34
#define INST_LOC_VOTEPRINT    35
#define INST_LOC_RESETF       36
#define INST_LOC_FEEDBACK     37
#define INST_LOC_KILLEMAIL    38
#define INST_LOC_POST         39
#define INST_LOC_NEWUSER      40
#define INST_LOC_RMAIL        41
#define INST_LOC_DOWNLOAD     42
#define INST_LOC_UPLOAD       43
#define INST_LOC_BIXFER       44
#define INST_LOC_NETLIST      45
#define INST_LOC_TERM         46
#define INST_LOC_EVENTEDIT    47
#define INST_LOC_GETUSER      48
#define INST_LOC_CH1        5000
#define INST_LOC_CH2        5001
#define INST_LOC_CH3        5002
#define INST_LOC_CH4        5003
#define INST_LOC_CH5        5004
#define INST_LOC_CH6        5005
#define INST_LOC_CH7        5006
#define INST_LOC_CH8        5007
#define INST_LOC_CH9        5008
#define INST_LOC_CH10       5009
#define INST_LOC_WFC       65535


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


#endif // __INCLUDED_INSTMSG_H__

