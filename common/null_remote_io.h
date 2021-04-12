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

#ifndef INCLUDED_COMMON_NULL_REMOTE_IO_H
#define INCLUDED_COMMON_NULL_REMOTE_IO_H

#include "common/remote_io.h"

namespace wwiv::local::io {
class LocalIO;
}

namespace wwiv::common {

/** Null remote session. */
/**
 * Base Communication Class.
 */
class NullRemoteIO final : public RemoteIO {
public:
  NullRemoteIO(wwiv::local::io::LocalIO* local_io) : RemoteIO(), local_io_(local_io) {}
  virtual ~NullRemoteIO() {}

  // Nothing is always able to be open.
  bool open() override { return true; }
  void close(bool) override {}
  unsigned char getW() override { return 0; }
  bool disconnect() override { return true; }
  void purgeIn() override {}
  unsigned int put(unsigned char) override { return 0; }
  unsigned int read(char*, unsigned int) override { return 0; }
  unsigned int write(const char*, unsigned int, bool) override { return 0; }
  bool connected() override { return false; }
  bool incoming() override { return false; }

  unsigned int GetHandle() const override { return 0; }
  std::optional<ScreenPos> screen_position() override;

private:
  wwiv::local::io::LocalIO* local_io_;
};

} // namespace wwiv::comon 

#endif
