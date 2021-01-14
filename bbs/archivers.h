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
#ifndef _ARCHIVERS_H_
#define _ARCHIVERS_H_

#include "sdk/vardec.h"
#include <optional>
#include <string>
#include <vector>

std::optional<arcrec> match_archiver(const std::vector<arcrec>& arcs, const std::string& filename);

struct arc_command_t {
  bool internal{false};
  std::string cmd;
};

enum class arc_command_type_t { list, extract, add, comment, remove, test };
std::optional<arc_command_t> get_arc_cmd(const std::string& arc_fn, arc_command_type_t cmd,
                                         const std::string& data);

#endif  // _ARCHIVERS_H_
