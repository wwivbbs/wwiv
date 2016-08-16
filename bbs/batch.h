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
/**************************************************************************/
#ifndef __INCLUDED_BBS_BATCH_H__
#define __INCLUDED_BBS_BATCH_H__

#include <iterator>
#include <vector>
#include "sdk/vardec.h"

class Batch {
public:
  bool clear() { entry.clear(); return true; }
  // deletes an entry by position;
  bool delbatch(size_t pos);
  long dl_time_in_secs() const;
  size_t numbatchdl() const { size_t r = 0;  for (const auto& e : entry) { if (e.sending) r++; } return r; }
  size_t numbatchul() const { size_t r = 0;  for (const auto& e : entry) { if (!e.sending) r++; } return r; }
  std::vector<batchrec> entry;
};

void upload(int dn);
std::vector<batchrec>::iterator delbatch(std::vector<batchrec>::iterator it);
void delbatch(int nBatchEntryNum);
char *unalign(char *file_name);
void dszbatchdl(bool bHangupAfterDl, char *command_line, char *description);
int  batchdl(int mode);
void didnt_upload(const batchrec& b);
void ymbatchdl(bool bHangupAfterDl);
void zmbatchdl(bool bHangupAfterDl);

#endif  // __INCLUDED_BBS_BATCH_H__