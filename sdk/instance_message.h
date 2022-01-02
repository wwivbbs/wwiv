/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_INSTANCE_MESSAGE_H
#define INCLUDED_SDK_INSTANCE_MESSAGE_H

#include "core/datetime.h"

#include <filesystem>
#include <string>
#include <vector>
#include "sdk/config.h"

namespace wwiv::sdk {

enum class instance_message_type_t {
  user,
  system
};

/*
 * Structure for inter-instance messages.
 */
struct instance_message_t {
  // Message main type
  instance_message_type_t message_type;
  // Originating instance
  int from_instance;
  // Originating sess->usernum
  int from_user;
  // Destination instance
  int dest_inst;

  // Timestamp the message was sent.
  daten_t daten;
  // Message text
  std::string message;
};

/**
 * Sends an instance message to the instance pointed to by msg.
 */
bool send_instance_message(const Config& config, const instance_message_t& msg);

std::filesystem::path instance_message_filespec(const Config& config, int instance_num);


bool send_instance_string(const Config& config, instance_message_type_t t, int dest_instance,
                          int from_user, int from_instance, const std::string& text);

std::vector<instance_message_t> read_all_instance_messages(const Config& config, int instance_num, int limit = 1000);

}


#endif 
