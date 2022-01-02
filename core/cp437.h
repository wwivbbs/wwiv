/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2022, WWIV Software Services             */
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
#ifndef INCLUDED_LOCAL_IO_CP437_H
#define INCLUDED_LOCAL_IO_CP437_H

#include <cstdint>
#include <string>

namespace wwiv::core {

enum class wwiv_codepage_t { cp437, utf8 };

bool set_wwiv_codepage(wwiv_codepage_t);

int cp437_to_utf8(uint8_t ch, char* out);
wchar_t cp437_to_utf8(char ch);
wchar_t cp437_to_utf8(uint8_t ch);
std::wstring cp437_to_utf8w(const std::string& in);
std::string cp437_to_utf8(const std::string& in);

}

#endif