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
#include "common/ansi_keys.h"

#include "gtest/gtest.h"

using namespace wwiv::common;
using namespace wwiv::local::io;

TEST(AnsiKeys, SingleLetterCsi) {
  EXPECT_EQ(COMMAND_UP, parse_ansi_escape_sequence("[A"));
  EXPECT_EQ(COMMAND_DOWN, parse_ansi_escape_sequence("[B"));
  EXPECT_EQ(COMMAND_RIGHT, parse_ansi_escape_sequence("[C"));
  EXPECT_EQ(COMMAND_LEFT, parse_ansi_escape_sequence("[D"));
  EXPECT_EQ(COMMAND_HOME, parse_ansi_escape_sequence("[H"));
  EXPECT_EQ(COMMAND_END, parse_ansi_escape_sequence("[K"));
  EXPECT_EQ(COMMAND_END, parse_ansi_escape_sequence("[F"));
  EXPECT_EQ(COMMAND_PAGEUP, parse_ansi_escape_sequence("[V"));
  EXPECT_EQ(COMMAND_PAGEDN, parse_ansi_escape_sequence("[U"));
  EXPECT_EQ(COMMAND_INSERT, parse_ansi_escape_sequence("[r"));
  EXPECT_EQ(COMMAND_INSERT, parse_ansi_escape_sequence("[@"));
  EXPECT_EQ(COMMAND_DELETE, parse_ansi_escape_sequence("[s"));
}

TEST(AnsiKeys, Ss3AndDoubleIntroducer) {
  EXPECT_EQ(COMMAND_UP, parse_ansi_escape_sequence("OA"));
  EXPECT_EQ(COMMAND_LEFT, parse_ansi_escape_sequence("OD"));
  EXPECT_EQ(COMMAND_UP, parse_ansi_escape_sequence("[[A"));
  EXPECT_EQ(COMMAND_HOME, parse_ansi_escape_sequence("OOH"));
}

TEST(AnsiKeys, Vt220TildeKeys) {
  EXPECT_EQ(COMMAND_HOME, parse_ansi_escape_sequence("[1~"));
  EXPECT_EQ(COMMAND_INSERT, parse_ansi_escape_sequence("[2~"));
  EXPECT_EQ(COMMAND_DELETE, parse_ansi_escape_sequence("[3~"));
  EXPECT_EQ(COMMAND_END, parse_ansi_escape_sequence("[4~"));
  EXPECT_EQ(COMMAND_PAGEUP, parse_ansi_escape_sequence("[5~"));
  EXPECT_EQ(COMMAND_PAGEDN, parse_ansi_escape_sequence("[6~"));
  // rxvt Home/End
  EXPECT_EQ(COMMAND_HOME, parse_ansi_escape_sequence("[7~"));
  EXPECT_EQ(COMMAND_END, parse_ansi_escape_sequence("[8~"));
}

TEST(AnsiKeys, FunctionKeysConsumed) {
  EXPECT_EQ(COMMAND_F1, parse_ansi_escape_sequence("[11~"));
  EXPECT_EQ(COMMAND_F5, parse_ansi_escape_sequence("[15~"));
  EXPECT_EQ(COMMAND_F6, parse_ansi_escape_sequence("[17~"));
  EXPECT_EQ(COMMAND_F10, parse_ansi_escape_sequence("[21~"));
  // F11/F12 have no COMMAND_* constants; consume the '~' and return 0.
  EXPECT_EQ(0, parse_ansi_escape_sequence("[23~"));
  EXPECT_EQ(0, parse_ansi_escape_sequence("[24~"));
}

TEST(AnsiKeys, ShiftModifiersStillPlainCommands) {
  EXPECT_EQ(COMMAND_DELETE, parse_ansi_escape_sequence("[3;2~"));
  EXPECT_EQ(COMMAND_F1, parse_ansi_escape_sequence("[11;2~"));
}

TEST(AnsiKeys, CtrlModifiersAreDistinct) {
  // xterm / PuTTY
  EXPECT_EQ(COMMAND_CLEFT, parse_ansi_escape_sequence("[1;5D"));
  EXPECT_EQ(COMMAND_CRIGHT, parse_ansi_escape_sequence("[1;5C"));
  EXPECT_EQ(COMMAND_CHOME, parse_ansi_escape_sequence("[1;5H"));
  EXPECT_EQ(COMMAND_CEND, parse_ansi_escape_sequence("[1;5F"));
  EXPECT_EQ(COMMAND_CEND, parse_ansi_escape_sequence("[1;5K"));
  EXPECT_EQ(COMMAND_CDELETE, parse_ansi_escape_sequence("[3;5~"));
  EXPECT_EQ(COMMAND_CHOME, parse_ansi_escape_sequence("[1;5~"));
  EXPECT_EQ(COMMAND_CEND, parse_ansi_escape_sequence("[4;5~"));
  EXPECT_EQ(COMMAND_CHOME, parse_ansi_escape_sequence("[5;5~")); // Ctrl-PgUp
  EXPECT_EQ(COMMAND_CEND, parse_ansi_escape_sequence("[6;5~")); // Ctrl-PgDn
  // Unmodified keys stay unmodified
  EXPECT_EQ(COMMAND_RIGHT, parse_ansi_escape_sequence("[C"));
  EXPECT_EQ(COMMAND_LEFT, parse_ansi_escape_sequence("[D"));
  EXPECT_EQ(COMMAND_DELETE, parse_ansi_escape_sequence("[3~"));
  EXPECT_EQ(COMMAND_HOME, parse_ansi_escape_sequence("[H"));
  EXPECT_EQ(COMMAND_END, parse_ansi_escape_sequence("[F"));
}

TEST(AnsiKeys, IncompleteOrInvalid) {
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence(""));
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence("["));
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence("[3"));
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence("[3;"));
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence("X"));
  EXPECT_EQ(GET_OUT, parse_ansi_escape_sequence("3~"));
}

TEST(AnsiKeys, TranslateCbackspace) {
  EXPECT_EQ(BACKSPACE, translate_cbackspace(false));
  EXPECT_EQ(COMMAND_DELETE, translate_cbackspace(true));
}

TEST(AnsiKeys, CommandForCsiTildeDirect) {
  EXPECT_EQ(COMMAND_DELETE, command_for_csi_tilde(3));
  EXPECT_EQ(COMMAND_HOME, command_for_csi_tilde(1));
  EXPECT_EQ(0, command_for_csi_tilde(0));
  EXPECT_EQ(0, command_for_csi_tilde(99));
}
