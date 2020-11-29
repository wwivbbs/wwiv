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
/*                                                                        */
/**************************************************************************/
#include "qwk_reply.h"
#include "bbs/bbs.h"
#include "bbs/qwk/qwk_struct.h"

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"

#include <algorithm>
#include <fcntl.h>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv::bbs::qwk {

std::string qwk_system_name(const qwk_config& c) {
  auto qwkname = !c.packet_name.empty() ? c.packet_name : a()->config()->system_name();

  if (qwkname.length() > 8) {
    qwkname.resize(8);
  }
  std::replace( qwkname.begin(), qwkname.end(), ' ', '-' );
  std::replace( qwkname.begin(), qwkname.end(), '.', '-' );
  return ToStringUpperCase(qwkname);
}

qwk_config read_qwk_cfg() {
  qwk_config_430 o{};

  const auto filename = FilePath(a()->config()->datadir(), QWK_CFG).string();
  int f = open(filename.c_str(), O_BINARY | O_RDONLY);
  if (f < 0) {
    return {};
  }
  read(f,  &o, sizeof(qwk_config_430));
  int x = 0;

  qwk_config c{};
  while (x < o.amount_blts) {
    auto pos = sizeof(qwk_config_430) + (x * BULL_SIZE);
    lseek(f, pos, SEEK_SET);
    char b[BULL_SIZE];
    memset(b, 0, sizeof(b));
    read(f, &b, BULL_SIZE);
    qwk_bulletin bltn;
    bltn.path = b;
    c.bulletins.emplace_back(bltn);
    ++x;
  }

  x = 0;
  for (auto& blt : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (ssize(c.bulletins) * BULL_SIZE) + (x * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);
    char b[BULL_SIZE];
    memset(b, 0, sizeof(b));
    read(f, &b, BULL_SIZE);
    blt.name = b;
    ++x;
  }
  close(f);

  c.fu = o.fu;
  c.timesd = o.timesd;
  c.timesu = o.timesu;
  c.max_msgs = o.max_msgs;
  c.hello = o.hello;
  c.news = o.news;
  c.bye = o.bye;
  c.packet_name = o.packet_name;
  c.amount_blts = o.amount_blts;
  return c;
}

void write_qwk_cfg(const qwk_config& c) {
  const auto filename = FilePath(a()->config()->datadir(), QWK_CFG).string();
  const auto f = open(filename.c_str(), O_BINARY | O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  if (f < 0) {
    return;
  }

  qwk_config_430 o{};
  o.fu = c.fu;
  o.timesd = c.timesd;
  o.timesu = c.timesu;
  o.max_msgs = c.max_msgs;
  to_char_array(o.hello, c.hello);
  to_char_array(o.news, c.news);
  to_char_array(o.bye, c.bye);
  to_char_array(o.packet_name, c.packet_name);
  o.amount_blts = size_int32(c.bulletins);

  write(f, &o, sizeof(qwk_config_430));

  int x = 0;
  for (const auto& b : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (x * BULL_SIZE);
    lseek(f, pos, SEEK_SET);

    if (!b.path.empty()) {
      char p[BULL_SIZE+1];
      memset(&p, 0, BULL_SIZE);
      to_char_array(p, b.path);
      write(f, p, BULL_SIZE);
      ++x;
    }
  }

  x = 0;
  for (const auto& b : c.bulletins) {
    const auto pos = sizeof(qwk_config_430) + (c.bulletins.size() * BULL_SIZE) + (x++ * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);

    if (!b.name.empty()) {
      char p[BNAME_SIZE+1];
      memset(&p, 0, BNAME_SIZE);
      to_char_array(p, b.name);
      write(f, p, BNAME_SIZE);
    }
  }

  close(f);
}

}
