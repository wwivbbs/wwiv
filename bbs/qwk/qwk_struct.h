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
#ifndef INCLUDED_BBS_QWK_QWK_STRUCT_H
#define INCLUDED_BBS_QWK_QWK_STRUCT_H

#include "core/file.h"
#include "core/datafile.h"
#include <cstdint>


namespace wwiv::bbs::qwk {
#pragma pack(push, 1)
struct qwk_record {
  char status;   // ' ' for public

  char msgnum[7]; // all strings are space padded
  char date[8];
  char time[5];

  char to[25]; // Uppercase, left justified
  char from[25]; // Uppercase left justified
  char subject[25];
  char password[12];
  char reference[8];

  char amount_blocks[6];
  char flag;

  uint16_t conf_num;
  uint16_t logical_num;
  char tagline;
};

struct qwk_index {
  float pos;
  char nouse;
};

struct qwk_state {
  uint16_t qwk_rec_num;
  uint16_t qwk_rec_pos;

  // File number for the MESSAGES.DAT file
  std::unique_ptr<wwiv::core::DataFile<qwk_record>>  file;

  // File number for the *.NDX files
  std::unique_ptr<wwiv::core::DataFile<qwk_index>> index;
  int cursub;
  std::unique_ptr<wwiv::core::DataFile<qwk_index>> personal;  // personal.ndx
  std::unique_ptr<wwiv::core::DataFile<qwk_index>> zero;      // 000.ndx for email

  qwk_record qwk_rec;
  qwk_index qwk_ndx;

  bool abort{false};
  bool in_email{false};
  // This should be 25 chars so we want 1 more for null.
  char email_title[25];
};


struct qwk_sub_conf {
  int import_num;
  char import_name[14];
  int to_num;
  char to_name[41];
};

enum CONFIG_QWK_RETURNS {
  QWK_SELECT,
  QWK_ARCHIVER,
  QWK_MAX_PACKET,
  QWK_MAX_SUB,
  QWK_SELECT_PROTOCOL,
  QWK_QUIT_SAVE,
  QWK_ABORT
};
#pragma pack(pop)

}

#endif