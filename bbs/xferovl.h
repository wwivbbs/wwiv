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
#ifndef __INCLUDED_BBS_XFEROVL_H__
#define __INCLUDED_BBS_XFEROVL_H__

void move_file();
void sortdir(int directory_num, int type);
void sort_all(int type);
void rename_file();
bool maybe_upload(const char *file_name, uint16_t directory_num, const char *description);
void upload_files(const char *file_name, uint16_t directory_num, int type);
bool uploadall(uint16_t directory_num);
void relist();
void edit_database();
void modify_database(const char *file_name, bool add);
bool is_uploadable(const char *file_name);
void xfer_defaults();
void finddescription();
void arc_l();

#endif  // __INCLUDED_BBS_XFEROVL_H__