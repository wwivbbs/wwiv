/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2014-2022, WWIV Software Services          */
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
/**************************************************************************/
#ifndef INCLUDED_BBS_DROPFILE_H
#define INCLUDED_BBS_DROPFILE_H

#include <cstdint>
#include <optional>
#include <string>

#pragma pack(push, 1)

// See http://wiki.bbs.nz/doors:dropfiles:pcboardsys
// Note, this doesn't seem to match perfectly what that states, but this
// is the same pcboard.sys format wwiv has done for years.
// The character fields are fixed width and are not null terminated.
struct pcboard_sys_rec {
  char display[2], printer[2], page_bell[2], alarm[2], sysop_next, errcheck[2], graphics, nodechat,
      openbps[5], connectbps[5];

  int16_t usernum;

  char firstname[15], password[12];

  int16_t time_on, prev_used;

  char time_logged[5];

  int16_t time_limit, down_limit;

  char curconf, bitmap1[5], bitmap2[5];

  int16_t time_added, time_credit;

  char slanguage[4], name[25];

  int16_t sminsleft;

  char snodenum, seventtime[5], seventactive[2], sslide[2], smemmsg[4], scomport, packflag, bpsflag;

  // PCB 14.5 extra stuff
  char ansi, lastevent[8];

  int16_t lasteventmin;

  char exittodos, eventupcoming;

  int16_t lastconfarea;

  char hconfbitmap;
  // end PCB 14.5 additions
};
#pragma pack(pop)

enum class drop_file_t {
  CHAIN_TXT,
  DORINFO_DEF,
  PCBOARD_SYS,
  CALLINFO_BBS,
  DOOR_SYS,
  DOOR32_SYS
};

/** 
 * Creates a dropfile of type dropfile_type, and returns the string form
 * of the filename.
 */
std::string create_dropfile_filename(drop_file_t dropfile_type);

/** Writes pcboard.sys (see pcboard_sys_rec) for the current user. */
void CreatePCBoardSysDropFile();
/**
 * Creates a dropfile of type chain.txt, and returns the string form
 * of the filename.
 */
std::string create_chain_file();

std::optional<std::string> create_netfoss_bat();

#endif
