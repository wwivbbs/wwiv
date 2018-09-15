/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_SRSEND_H__
#define __INCLUDED_BBS_SRSEND_H__

#include "core/file.h"

void send_block(char *b, int block_type, bool use_crc, char byBlockNumber);
char send_b(wwiv::core::File& file, long pos, int block_type, char byBlockNumber, bool* use_crc,
            const std::string& file_name, int* terr, bool* abort);
bool okstart(bool *use_crc, bool *abort);
void xymodem_send(const std::string& file_name, bool* sent, double* percent, bool use_crc,
                  bool use_ymodem, bool use_ymodemBatch);
void zmodem_send(const std::string& file_name, bool *sent, double *percent);

#endif  // __INCLUDED_BBS_SRSEND_H__