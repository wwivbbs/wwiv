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
#ifndef INCLUDED_COMMON_ANSI_KEYS_H
#define INCLUDED_COMMON_ANSI_KEYS_H

#include "local_io/keycodes.h"

#include <cctype>
#include <string_view>

namespace wwiv::common {

// Map a single-letter CSI/SS3 final byte (ESC[A, ESC[H, ESC[K, ...).
inline int command_for_ansi_letter(int key) {
  switch (key) {
  case 'A': return wwiv::local::io::COMMAND_UP;
  case 'B': return wwiv::local::io::COMMAND_DOWN;
  case 'C': return wwiv::local::io::COMMAND_RIGHT;
  case 'D': return wwiv::local::io::COMMAND_LEFT;
  case 'H': return wwiv::local::io::COMMAND_HOME;
  case 'F': // xterm / VT End
  case 'K': return wwiv::local::io::COMMAND_END;
  case 'V': return wwiv::local::io::COMMAND_PAGEUP;
  case 'U': return wwiv::local::io::COMMAND_PAGEDN;
  case '@': // ANSI-BBS Insert (SyncTERM)
  case 'r': return wwiv::local::io::COMMAND_INSERT;
  case 's': return wwiv::local::io::COMMAND_DELETE;
  default: return key;
  }
}

// Map a CSI parameter in ESC[<n>~ to a COMMAND_* code. Unmapped values
// (including F11/F12) return 0 so the sequence can be consumed silently.
inline int command_for_csi_tilde(int n) {
  switch (n) {
  case 1:
  case 7: return wwiv::local::io::COMMAND_HOME;
  case 2: return wwiv::local::io::COMMAND_INSERT;
  case 3: return wwiv::local::io::COMMAND_DELETE;
  case 4:
  case 8: return wwiv::local::io::COMMAND_END;
  case 5: return wwiv::local::io::COMMAND_PAGEUP;
  case 6: return wwiv::local::io::COMMAND_PAGEDN;
  case 11: return wwiv::local::io::COMMAND_F1;
  case 12: return wwiv::local::io::COMMAND_F2;
  case 13: return wwiv::local::io::COMMAND_F3;
  case 14: return wwiv::local::io::COMMAND_F4;
  case 15: return wwiv::local::io::COMMAND_F5;
  case 17: return wwiv::local::io::COMMAND_F6;
  case 18: return wwiv::local::io::COMMAND_F7;
  case 19: return wwiv::local::io::COMMAND_F8;
  case 20: return wwiv::local::io::COMMAND_F9;
  case 21: return wwiv::local::io::COMMAND_F10;
  default: return 0;
  }
}

// Translate the DEL byte (0x7F / CBACKSPACE).
// ANSI-BBS terminals such as SyncTERM (DECBKM set, the default) send 0x08
// for Backspace and 0x7F for Delete. Unix terminals (xterm, PuTTY, Linux
// console) send 0x7F for Backspace and ESC[3~ for Delete. After a remote
// 0x08 has been seen, treat 0x7F as Delete; otherwise keep it as Backspace.
inline int translate_cbackspace(bool remote_sent_bs) noexcept {
  return remote_sent_bs ? wwiv::local::io::COMMAND_DELETE : wwiv::local::io::BACKSPACE;
}

// True for CSI modifiers that include Ctrl: 5=Ctrl, 6=Ctrl+Shift,
// 7=Ctrl+Alt, 8=Ctrl+Alt+Shift.
inline bool csi_modifier_has_ctrl(int modifier) noexcept {
  return modifier == 5 || modifier == 6 || modifier == 7 || modifier == 8;
}

// Parse the bytes following ESC. `seq` is e.g. "[3~", "[A", "OA", "[[A",
// or "[1;5C". Returns a COMMAND_* code, GET_OUT for a missing/invalid
// introducer or an incomplete sequence, or 0 if the sequence is well
// formed but unmapped (e.g. ESC[23~).
//
// A Ctrl modifier (the second CSI parameter) is kept, so ESC[1;5D is
// COMMAND_CLEFT (word left) rather than COMMAND_LEFT.
inline int parse_ansi_escape_sequence(std::string_view seq) {
  if (seq.empty()) {
    return wwiv::local::io::GET_OUT;
  }
  size_t i = 0;
  if (seq[i] == '[' || seq[i] == 'O') {
    ++i;
    if (i < seq.size() && (seq[i] == '[' || seq[i] == 'O')) {
      ++i;
    }
  } else {
    return wwiv::local::io::GET_OUT;
  }
  if (i >= seq.size()) {
    return wwiv::local::io::GET_OUT;
  }

  int first_param = 0;
  int modifier = 0;
  bool have_param = false;
  int param_index = 0;
  while (i < seq.size() &&
         (std::isdigit(static_cast<unsigned char>(seq[i])) || seq[i] == ';')) {
    if (seq[i] == ';') {
      ++param_index;
    } else if (param_index == 0) {
      first_param = first_param * 10 + (seq[i] - '0');
      have_param = true;
    } else if (param_index == 1) {
      modifier = modifier * 10 + (seq[i] - '0');
    }
    ++i;
  }
  if (i >= seq.size()) {
    return wwiv::local::io::GET_OUT;
  }
  const auto final_byte = static_cast<unsigned char>(seq[i]);
  const bool ctrl = csi_modifier_has_ctrl(modifier);
  if (final_byte == '~') {
    if (ctrl) {
      switch (have_param ? first_param : 0) {
      case 1:
      case 7:
      case 5: // Ctrl-PgUp
        return wwiv::local::io::COMMAND_CHOME;
      case 3:
        return wwiv::local::io::COMMAND_CDELETE;
      case 4:
      case 8:
      case 6: // Ctrl-PgDn
        return wwiv::local::io::COMMAND_CEND;
      default:
        return 0;
      }
    }
    return command_for_csi_tilde(have_param ? first_param : 0);
  }
  if (ctrl) {
    switch (final_byte) {
    case 'D':
      return wwiv::local::io::COMMAND_CLEFT;
    case 'C':
      return wwiv::local::io::COMMAND_CRIGHT;
    case 'H':
      return wwiv::local::io::COMMAND_CHOME;
    case 'F':
    case 'K':
      return wwiv::local::io::COMMAND_CEND;
    default:
      break;
    }
  }
  return command_for_ansi_letter(final_byte);
}

}

#endif
