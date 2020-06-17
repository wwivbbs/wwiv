/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FIDO_FIDO_DIRECTORIES_H__
#define __INCLUDED_SDK_FIDO_FIDO_DIRECTORIES_H__

#include "sdk/fido/fido_callout.h"
#include <filesystem>
#include <string>

namespace wwiv::sdk::fido {

class FtnDirectories {
public:
  FtnDirectories(const std::string& bbsdir, const net_networks_rec& net, std::string receive_dir);
  FtnDirectories(const std::string& bbsdir, const net_networks_rec& net);
  virtual ~FtnDirectories();

  [[nodiscard]] const std::string& net_dir() const;
  [[nodiscard]] const std::string& inbound_dir() const;
  [[nodiscard]] const std::string& temp_inbound_dir() const;
  [[nodiscard]] const std::string& temp_outbound_dir() const;
  [[nodiscard]] const std::string& outbound_dir() const;
  [[nodiscard]] const std::string& netmail_dir() const;
  [[nodiscard]] const std::string& bad_packets_dir() const;
  [[nodiscard]] const std::string& receive_dir() const;
  [[nodiscard]] const std::string& tic_dir() const;
  [[nodiscard]] const std::string& unknown_dir() const;

private:
  const std::string bbsdir_;
  const net_networks_rec net_;
  const std::string net_dir_;
  const std::string inbound_dir_;
  const std::string temp_inbound_dir_;
  const std::string temp_outbound_dir_;
  const std::string outbound_dir_;
  const std::string netmail_dir_;
  const std::string bad_packets_dir_;
  const std::string receive_dir_;
  const std::string tic_dir_;
  const std::string unknown_dir_;
};

}

#endif