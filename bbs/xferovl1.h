/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_XFEROVL1_H__
#define __INCLUDED_BBS_XFEROVL1_H__

#include <string>
#include "sdk/vardec.h"

void modify_extended_description(std::string* sss, const std::string& dest);
bool valid_desc(const std::string& description);
bool get_file_idz(uploadsrec * upload_record, int dn);
int  read_idz_all();
int  read_idz(int mode, int tempdir);
void tag_it();
void tag_files(bool& need_title);
int  add_batch(char *description, const char *file_name, int dn, long fs);
int  try_to_download(const char *file_mask, int dn);
void download();
void endlist(int mode);
void SetNewFileScanDate();
void removefilesnotthere(int dn, int *autodel);
void removenotthere();
int  find_batch_queue(const char *file_name);
void remove_batch(const char *file_name);

#endif  // __INCLUDED_BBS_XFEROVL1_H__