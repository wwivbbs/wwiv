/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2026, WWIV Software Services              */
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
#ifndef INCLUDED_BBS_WWIVFOSS_INBAND_H
#define INCLUDED_BBS_WWIVFOSS_INBAND_H

#include <cstdint>
#include <string>

namespace wwiv::bbs {

/*
 * In-band line control used by WWIVFOSS -E (and DOSBox-X's nullmodem in
 * non-transparent mode). See include/foss_protocol.h in
 * https://github.com/wwivbbs/wwivfoss
 *
 *   FF FF   a literal FF data byte
 *   FF nn   line state from the other end (nn != FF):
 *             bit 0 RTS, bit 1 DTR, bit 2 break
 *
 * Both directions start out as RTS|DTR (03h).
 */
constexpr uint8_t WWIVFOSS_INBAND_ESCAPE = 0xFF;
constexpr uint8_t WWIVFOSS_LINE_RTS = 0x01;
constexpr uint8_t WWIVFOSS_LINE_DTR = 0x02;
constexpr uint8_t WWIVFOSS_LINE_BREAK = 0x04;
constexpr uint8_t WWIVFOSS_LINE_UP = WWIVFOSS_LINE_RTS | WWIVFOSS_LINE_DTR;

/** Returns data with every FF byte doubled, ready to send to the door. */
std::string wwivfoss_escape(const char* data, size_t len);

/** Returns the two byte line state message (FF nn) for state. */
std::string wwivfoss_line_state(uint8_t state);

/**
 * Decodes the byte stream from the door. Keeps state between calls, so an
 * FF split across two reads is handled.
 */
class WWIVFossDecoder final {
public:
  /** Returns the data bytes in data, consuming any line state messages. */
  std::string decode(const char* data, size_t len);

  /** The door's most recent line state. */
  [[nodiscard]] uint8_t line_state() const noexcept { return line_state_; }

  /** true unless the door has lowered DTR (e.g. FOSSIL function 06h). */
  [[nodiscard]] bool dtr() const noexcept { return (line_state_ & WWIVFOSS_LINE_DTR) != 0; }

private:
  bool escape_{false};
  uint8_t line_state_{WWIVFOSS_LINE_UP};
};

} // namespace wwiv::bbs

#endif
