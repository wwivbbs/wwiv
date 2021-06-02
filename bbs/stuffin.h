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
#ifndef INCLUDED_BBS_STUFFIN_H
#define INCLUDED_BBS_STUFFIN_H

#include <filesystem>
#include <map>
#include <string>

namespace wwiv::bbs {

class CommandLine {
public:
  enum class cmdtype_t { relative, absolute };
  explicit CommandLine(const std::string& cmd) : cmd_(cmd), cmdtype_(cmdtype_t::relative) {}
  CommandLine(const std::string& cmd, const std::filesystem::path& bbs_root)
      : cmd_(cmd), bbs_root_(bbs_root), cmdtype_(cmdtype_t::absolute) {}
  ~CommandLine() {}

  std::string cmdline() const;

  void args(const std::string arg1 = "", const std::string arg2 = "", const std::string arg3 = "",
            const std::string arg4 = "", const std::string arg5 = "");
  void arg(int n, const std::string& arg);
  void add(char key, std::string value);
  std::string stuff_in(const std::string& commandline, const std::string arg1 = "",
                       const std::string arg2 = "", const std::string arg3 = "",
                       const std::string arg4 = "", const std::string arg5 = "") const;
  bool empty() const noexcept { return cmd_.empty(); }

private:
  std::string cmd_;
  const std::filesystem::path bbs_root_;
  cmdtype_t cmdtype_{cmdtype_t::relative};
  std::string arg1_;
  std::string arg2_;
  std::string arg3_;
  std::string arg4_;
  std::string arg5_;
  std::map<char, const std::string> args_;
};

} // namespace wwiv::bbs 

#endif