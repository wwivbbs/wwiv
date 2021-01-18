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
#ifndef INCLUDED_SDK_QWK_CONFIG_H
#define INCLUDED_SDK_QWK_CONFIG_H

#include "core/wwivport.h" // daten_t
#include "sdk/config.h"

#include <string>
#include <vector>

namespace wwiv::sdk {

struct qwk_bulletin {
  std::string name;
  std::string path;
};

struct qwk_config {
  daten_t fu;
  long timesd;
  long timesu;
  uint16_t max_msgs;

  std::string hello;
  std::string news;
  std::string bye;
  std::string packet_name;

  std::vector<qwk_bulletin> bulletins;
};

#pragma pack(push, 1)

struct qwk_config_430 {
  daten_t fu;
  int32_t timesd;
  int32_t timesu;
  uint16_t max_msgs;

  char hello[13];
  char news[13];
  char bye[13];
  char packet_name[9];
  char res[190];

  int32_t amount_blts;
  char unused_blt_res[8 * 50];
};
#pragma pack(pop)


static_assert(sizeof(qwk_config_430) == 656u, "qwk_config should be 656 bytes");

std::string qwk_system_name(const qwk_config& c, const std::string& system_name);
qwk_config read_qwk_cfg(const Config& config);
void write_qwk_cfg(const Config& config, const qwk_config& c);


}

#endif