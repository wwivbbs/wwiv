/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_ARWORD_H
#define INCLUDED_BBS_ARWORD_H

#include <cstdint>
#include <string>

/**
 * Returns a proper AR string "AB             P", into the integer form.
 */
uint16_t str_to_arword(const std::string& arstr);

/**
 * Returns the int form of ar as proper AR string, Example: "AB             P"
 */
std::string word_to_arstr(int ar, const std::string& empty_ar_str);

/**
 * Returns the int form of ar as a string, no padding. Example: "ABP"
 */
std::string word_to_arstr_nopadding(int ar, const std::string& empty_ar_str);

#endif  // INCLUDED_BBS_ARWORD_H