/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2014-2019, WWIV Software Services          */
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
#ifndef __INCLUDED_DROPFILE_H__
#define __INCLUDED_DROPFILE_H__

#include <string>

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
const std::string create_dropfile_filename(drop_file_t dropfile_type);
/**
 * Creates a dropfile of type chain.txt, and returns the string form
 * of the filename.
 */
const std::string create_chain_file();


#endif  // __INCLUDED_DROPFILE_H__