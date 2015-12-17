/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_WCONSTANTS_H__
#define __INCLUDED_WCONSTANTS_H__

#include <cstdint>

// For get_kb_event, decides if the number pad turns '8' into an arrow etc.. or not
constexpr int NOTNUMBERS = 1;
constexpr int NUMBERS = 0;

// Defines for listplus
constexpr int LP_LIST_DIR = 0;
constexpr int LP_SEARCH_ALL = 1;
constexpr int LP_NSCAN_DIR = 2;
constexpr int LP_NSCAN_NSCAN = 3;

constexpr int ALL_DIRS = 0;
constexpr int THIS_DIR = 1;
constexpr int NSCAN_DIRS = 2;

// Defines for Q/Nscan Plus
constexpr int QSCAN = 0;
constexpr int NSCAN = 1;

constexpr int INST_FORMAT_OLD = 0;
constexpr int INST_FORMAT_WFC = 1;
constexpr int INST_FORMAT_LIST = 2;

// Defines for UEDIT
constexpr int UEDIT_NONE = 0;
constexpr int UEDIT_FULLINFO = 1;
constexpr int UEDIT_CLEARSCREEN = 2;


constexpr int MAX_NUM_ACT = 100;

constexpr int NUM_DOTS = 5;
constexpr int FRAME_COLOR = 7;

//
// Protocols
//
constexpr int WWIV_INTERNAL_PROT_ASCII = 1;
constexpr int WWIV_INTERNAL_PROT_XMODEM = 2;
constexpr int WWIV_INTERNAL_PROT_XMODEMCRC = 3;
constexpr int WWIV_INTERNAL_PROT_YMODEM = 4;
constexpr int WWIV_INTERNAL_PROT_BATCH = 5;
constexpr int WWIV_INTERNAL_PROT_ZMODEM = 6;
constexpr int WWIV_NUM_INTERNAL_PROTOCOLS = 7; // last protocol +1

constexpr int MAX_ARCS = 15;
constexpr int MAXMAIL = 255;

constexpr int LIST_USERS_MESSAGE_AREA = 0;
constexpr int LIST_USERS_FILE_AREA = 1;

// Time
constexpr int SECONDS_PER_HOUR = 3600L;
constexpr float SECONDS_PER_HOUR_FLOAT = 3600.0;
constexpr int SECONDS_PER_DAY = 86400L;
constexpr float SECONDS_PER_DAY_FLOAT = 86400.0;
constexpr int HOURS_PER_DAY = 24L;
constexpr float HOURS_PER_DAY_FLOAT = 24.0;
constexpr int MINUTES_PER_HOUR = 60L;
constexpr float MINUTES_PER_HOUR_FLOAT = 60.0;
constexpr int SECONDS_PER_MINUTE = 60L;
constexpr float SECONDS_PER_MINUTE_FLOAT = 60.0;


#endif // __INCLUDED_WCONSTANTS_H__
