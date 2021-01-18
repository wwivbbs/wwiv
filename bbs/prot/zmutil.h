/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1999-2021, WWIV Software Services             */
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
#ifndef INCLUDED_WWIV_BBS_PROT_ZMUTIL_H
#define INCLUDED_WWIV_BBS_PROT_ZMUTIL_H

#include "bbs/prot/zmodem.h"
#include "fmt/format.h"
#include <cstdint>

void zmodemlog_impl(fmt::string_view format, fmt::format_args args);

template <typename S, typename... Args>
void zmodemlog(const S& format, Args&&... args) {
  zmodemlog_impl(format, fmt::make_args_checked<Args...>(format, args...));
}

struct ZModem;

uint32_t FileCrc(char* name);
std::string sname(ZModem*);
std::string sname2(ZMState);


#endif
