/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2020, WWIV Software Services            */
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
#ifndef __INCLUDED_WWIVFSED_FSEDCONFIG_H__
#define __INCLUDED_WWIVFSED_FSEDCONFIG_H__

#include "common/context.h"
#include "common/macro_context.h"
#include "common/message_editor_data.h"
#include "common/remote_io.h"
#include "core/command_line.h"
#include "fsed/line.h"
#include "fsed/model.h"
#include "local_io/local_io.h"
#include <filesystem>
#include <string>
#include <vector>

namespace wwiv::wwivfsed {

class FsedConfig final  {
public:
  explicit FsedConfig(const wwiv::core::CommandLine& cmdline);
  ~FsedConfig() = default;

  enum class bbs_type { wwiv, qbbs };

  bbs_type bbs_type() const noexcept { return bbs_type_; }
  int socket_handle() const noexcept { return socket_handle_; }
  bool local() const noexcept { return local_; }
  bool pause() const noexcept { return pause_; }
  const std::filesystem::path& root() const noexcept { return root_; }
  std::filesystem::path help_path() const;  
  std::filesystem::path file_path() const;

  LocalIO* CreateLocalIO();
  wwiv::common::RemoteIO* CreateRemoteIO();

private:
  const std::filesystem::path root_;
  std::filesystem::path help_path_;
  enum bbs_type bbs_type_{bbs_type::wwiv};
  int socket_handle_{-1};
  bool local_{false};
  bool pause_{false};
  std::filesystem::path file_path_;
};

} // namespace wwiv::wwivfsed

#endif