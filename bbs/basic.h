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
#ifndef __INCLUDED_BBS_BASIC_H__
#define __INCLUDED_BBS_BASIC_H__

#include <string>
#include <vector>

struct mb_interpreter_t;
class Output;

namespace wwiv::sdk {
  class Config;
  class User;
}

namespace wwiv::bbs {
  enum class script_data_type_t { STRING, INT, REAL };
  struct script_data_t {
    script_data_type_t type;
    ::std::string s;
    int i;
    float r;
  };

  class Basic {
  public:
    Basic(Output& bout, const wwiv::sdk::Config& config, const wwiv::sdk::User& user, bool mci_enabled)
      : bout_(bout), config_(config), user_(user), mci_enabled_(mci_enabled) {
    }

  bool RunScript(const std::string& script_name);

private:
  bool RegisterNamespaceData(mb_interpreter_t* bas);

private:
  Output& bout_;
  const wwiv::sdk::Config& config_;
  const wwiv::sdk::User& user_;
  const bool mci_enabled_;
  };

  bool RunBasicScript(const std::string& script_name);

}


#endif  // __INCLUDED_BBS_BASIC_H__