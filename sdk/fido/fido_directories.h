/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_FIDO_FIDO_DIRECTORIES_H
#define INCLUDED_SDK_FIDO_FIDO_DIRECTORIES_H

#include "sdk/fido/fido_callout.h"
#include <filesystem>
#include <string>

namespace wwiv::sdk::fido {

using namespace wwiv::sdk::net;

class FtnDirectories {
public:
  FtnDirectories(std::filesystem::path bbsdir, const Network& net,
                 std::filesystem::path receive_dir);
  FtnDirectories(const std::filesystem::path& bbsdir, const Network& net);
  virtual ~FtnDirectories();

  [[nodiscard]] std::string net_dir() const;
  [[nodiscard]] std::string inbound_dir() const;
  [[nodiscard]] std::string temp_inbound_dir() const;
  [[nodiscard]] std::string temp_outbound_dir() const;
  [[nodiscard]] std::string outbound_dir() const;
  [[nodiscard]] std::string netmail_dir() const;
  [[nodiscard]] std::string bad_packets_dir() const;
  [[nodiscard]] std::string receive_dir() const;
  [[nodiscard]] std::string tic_dir() const;
  [[nodiscard]] std::string unknown_dir() const;

private:
  const std::filesystem::path bbsdir_;
  const Network net_;
  const std::filesystem::path net_dir_;
  const std::filesystem::path inbound_dir_;
  const std::filesystem::path temp_inbound_dir_;
  const std::filesystem::path temp_outbound_dir_;
  const std::filesystem::path outbound_dir_;
  const std::filesystem::path netmail_dir_;
  const std::filesystem::path bad_packets_dir_;
  const std::filesystem::path receive_dir_;
  const std::filesystem::path tic_dir_;
  const std::filesystem::path unknown_dir_;
};

}

#endif