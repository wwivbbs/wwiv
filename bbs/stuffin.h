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
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_BBS_STUFFIN_H
#define INCLUDED_BBS_STUFFIN_H

#include <filesystem>
#include <map>
#include <string>

namespace wwiv::bbs {

/**
 * \verbatim
 * Creates a commandline by expanding the replaceable parameters offered by
 * WWIV.
 *
 * If you add one, please update these docs:
 * - http://docs.wwivbbs.org/en/latest/chains/parameters
 *
 * Replaceable parameters:
 * ~~~~~~~~~~~~~~~~~~~~~~
 *  Param    Description                       Example
 *  ----------------------------------------------------------------------------
 *  %%       A single '%'                          "%"
 *  %1-%5    Specified passed-in parameter
 *  %A       callinfo full pathname                "c:\wwiv\temp\callinfo.bbs"
 *  %B       Full path to BATCH instance directory "C:\wwiv\e\1\batch"
 *  %C       chain.txt full pathname               "c:\wwiv\temp\chain.txt"
 *  %D       doorinfo full pathname                "c:\wwiv\temp\dorinfo1.def"
 *  %E       door32.sys full pathname              "C:\wwiv\temp\door32.sys"
 *  %H       Socket Handle                         "1234"
 *  %I       Full Path to TEMP instance directory  "C:\wwiv\e\1\temp"
 *  %K       gfiles comment file for archives      "c:\wwiv\gfiles\comment.txt"
 *  %M       Modem BPS rate                         "38400"
 *  %N       Instance number                       "1"
 *  %O       pcboard full pathname                 "c:\wwiv\temp\pcboard.sys"
 *  %P       Com port number                       "1"
 *  %R       door full pathname                    "c:\wwiv\temp\door.sys"
 *  %S       Com port BPS rate                     "38400"
 *  %T       Time remaining (min)                  "30"
 *  %U       Username                              "Rushfan #1"
 *  %Z       Socket port or path                   "12345" or "/wwiv/bbs/e/1/scratch/wwiv.sock"
 * \endverbatim
 */
class CommandLine {
public:
  enum class cmdtype_t { relative, absolute };
  explicit CommandLine(const std::string& cmd) : cmd_(cmd), cmdtype_(cmdtype_t::relative) {}
  CommandLine(const std::string& cmd, const std::filesystem::path& bbs_root)
      : cmd_(cmd), bbs_root_(bbs_root), cmdtype_(cmdtype_t::absolute) {}
  ~CommandLine() = default;

  std::string cmdline() const;
  bool empty() const noexcept { 
    return cmd_.empty();
  }

  template <typename... Args> void args(Args... args) {
    return argss_(1, args...);
  }

  void add(char key, std::string value);

private:
  std::string stuff_in() const;
  template <typename T> void argss_(int num, T a) {
    std::ostringstream ss;
    ss << a;
    add(static_cast<char>(num + '0'), ss.str());
  }

  template <typename T, typename... Args> void argss_(int num, T arg, Args... args) {
    argss_(num, arg);
    argss_(num + 1, args...);
  }

  std::string cmd_;
  const std::filesystem::path bbs_root_;
  cmdtype_t cmdtype_{cmdtype_t::relative};
  std::map<char, const std::string> args_;
};

} // namespace wwiv::bbs 

#endif