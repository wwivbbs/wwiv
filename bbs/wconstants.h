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
#ifndef __INCLUDED_WCONSTANTS_H__
#define __INCLUDED_WCONSTANTS_H__

#ifndef JUSTIFY_LEFT
#define JUSTIFY_LEFT   0
#define JUSTIFY_RIGHT  1
#define JUSTIFY_CENTER 2
#endif  // JUSTIFY_LEFT

// For get_kb_event, decides if the number pad turns '8' into an arrow etc.. or not
#define NOTNUMBERS 1
#define NUMBERS    0

// Defines for listplus
#define LP_LIST_DIR    0
#define LP_SEARCH_ALL  1
#define LP_NSCAN_DIR   2
#define LP_NSCAN_NSCAN 3

#define ALL_DIRS    0
#define THIS_DIR    1
#define NSCAN_DIRS  2

// Defines for Q/Nscan Plus
#define QSCAN       0
#define NSCAN       1

#define INST_FORMAT_OLD     0
#define INST_FORMAT_WFC     1
#define INST_FORMAT_LIST    2

// Defines for UEDIT
#define UEDIT_NONE          0
#define UEDIT_FULLINFO      1
#define UEDIT_CLEARSCREEN   2


#define MAX_NUM_ACT 100

#define NUM_DOTS 5
#define FRAME_COLOR 7

//
// inmsg/external_edit flags
//
#define MSGED_FLAG_NONE             0
#define MSGED_FLAG_NO_TAGLINE       1
#define MSGED_FLAG_HAS_REPLY_NAME   2
#define MSGED_FLAG_HAS_REPLY_TITLE  4

#define INMSG_NOFSED            0
#define INMSG_FSED              1
#define INMSG_FSED_WORKSPACE    2

//
// Protocols
//
#define WWIV_INTERNAL_PROT_ASCII        1
#define WWIV_INTERNAL_PROT_XMODEM       2
#define WWIV_INTERNAL_PROT_XMODEMCRC    3
#define WWIV_INTERNAL_PROT_YMODEM       4
#define WWIV_INTERNAL_PROT_BATCH        5
#define WWIV_INTERNAL_PROT_ZMODEM       6
#define WWIV_NUM_INTERNAL_PROTOCOLS     7 // last protocol +1

#define MAX_ARCS 15
#define MAXMAIL 255

#define LIST_USERS_MESSAGE_AREA     0
#define LIST_USERS_FILE_AREA        1

// Time
#define SECONDS_PER_HOUR        3600L
#define SECONDS_PER_HOUR_FLOAT      3600.0
#define SECONDS_PER_DAY         86400L
#define SECONDS_PER_DAY_FLOAT       86400.0
#define HOURS_PER_DAY               24L
#define HOURS_PER_DAY_FLOAT         24.0
#define MINUTES_PER_HOUR            60L
#define MINUTES_PER_HOUR_FLOAT      60.0
#define SECONDS_PER_MINUTE          60L
#define SECONDS_PER_MINUTE_FLOAT  60.0


#endif // __INCLUDED_WCONSTANTS_H__
