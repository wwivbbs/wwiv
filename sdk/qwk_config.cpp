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
#include "sdk/qwk_config.h"

//#include "bbs/qwk/qwk_reply.h"
#include "config.h"
#include "core/cereal_utils.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include <algorithm>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::sdk{

template <class Archive> void serialize(Archive& ar, qwk_bulletin& s) { 
  SERIALIZE(s, name);
  SERIALIZE(s, path);
}

template <class Archive> void serialize(Archive& ar, qwk_config& s) { 
  SERIALIZE(s, fu);
  SERIALIZE(s, timesd);
  SERIALIZE(s, timesu);
  SERIALIZE(s, max_msgs);
  SERIALIZE(s, hello);
  SERIALIZE(s, news);
  SERIALIZE(s, bye);
  SERIALIZE(s, packet_name);
  SERIALIZE(s, bulletins);
}

std::string qwk_system_name(const qwk_config& c, const std::string& system_name) {
  auto qwkname = !c.packet_name.empty() ? c.packet_name : system_name;

  if (qwkname.length() > 8) {
    qwkname.resize(8);
  }
  std::replace( qwkname.begin(), qwkname.end(), ' ', '-' );
  std::replace( qwkname.begin(), qwkname.end(), '.', '-' );
  return ToStringLowerCase(qwkname);
}

#define BULL_SIZE     81
#define BNAME_SIZE    13

static qwk_config read_qwk_cfg_430(const Config& config) {
  const auto filename = FilePath(config.datadir(), QWK_CFG);
  File f(filename);

  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    return {};
  }

  qwk_config_430 o{};
  if (f.Read(&o, sizeof(qwk_config_430)) != sizeof(qwk_config_430)) {
    LOG(ERROR) << "Read failed on: " << f;
    return {};
  }

  auto x = 0;
  qwk_config c{};
  while (x < o.amount_blts) {
    auto pos = sizeof(qwk_config_430) + (x * BULL_SIZE);
    f.Seek(pos, File::Whence::begin);
    char b[BULL_SIZE]{};
    f.Read(&b, BULL_SIZE);
    c.bulletins.emplace_back(qwk_bulletin{ "", b });
    ++x;
  }

  x = 0;
  for (auto& blt : c.bulletins) {
    const auto pos = static_cast<int>(sizeof(qwk_config_430)) + (size_int(c.bulletins) * BULL_SIZE) + (x * BNAME_SIZE);
    f.Seek(pos, File::Whence::begin);
    char b[BULL_SIZE]{};
    f.Read(&b, BULL_SIZE);
    blt.name = b;
    ++x;
  }
  f.Close();

  c.fu = o.fu;
  c.timesd = o.timesd;
  c.timesu = o.timesu;
  c.max_msgs = o.max_msgs;
  c.hello = o.hello;
  c.news = o.news;
  c.bye = o.bye;
  c.packet_name = o.packet_name;
  return c;
}

qwk_config read_qwk_cfg(const Config& config) {
  const auto path = FilePath(config.datadir(), "qwk.json");
  qwk_config c{};
  JsonFile f(path, "qwk", c, 1);
  if (f.Load()) {
    return c;
  }
  return read_qwk_cfg_430(config);
}

void write_qwk_cfg(const Config& config, const qwk_config& c) {
  const auto path = FilePath(config.datadir(), "qwk.json");
  JsonFile f(path, "qwk", c, 1);
  if (!f.Save()) {
    LOG(ERROR) << "Failed to save qwk.json";
  }
}

}
