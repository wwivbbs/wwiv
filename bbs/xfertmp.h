/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_XFERTMP_H__
#define __INCLUDED_BBS_XFERTMP_H__

bool bad_filename(const char *file_name);
void add_arc(const char *arc, const char *file_name, int dos);
void add_temp_arc();
void del_temp();
void list_temp_dir();
void temp_extract();
void list_temp_text();
void list_temp_arc();
void temporary_stuff();
void move_file_t();
void removefile();

#endif  // __INCLUDED_BBS_XFERTMP_H__