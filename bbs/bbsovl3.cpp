/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include <string>
#include "bbs/bbsovl3.h"

#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/bgetch.h"
#include "bbs/utility.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"

using std::string;
using namespace wwiv::core;

bool do_sysop_command(int nCommandID) {
  unsigned int nKeyStroke = 0;
  bool bNeedToRedraw = false;

  switch (nCommandID) {
  // Commands that cause screen to need to be redrawn go here
  case COMMAND_F1:
  case COMMAND_CF1:
  case COMMAND_CF9:
  case COMMAND_F10:
  case COMMAND_CF10:
    bNeedToRedraw = true;
    nKeyStroke = nCommandID - 256;
    break;

  // Commands that don't change the screen around
  case COMMAND_SF1:
  case COMMAND_F2:
  case COMMAND_SF2:
  case COMMAND_CF2:
  case COMMAND_F3:
  case COMMAND_SF3:
  case COMMAND_CF3:
  case COMMAND_F4:
  case COMMAND_SF4:
  case COMMAND_CF4:
  case COMMAND_F5:
  case COMMAND_SF5:
  case COMMAND_CF5:
  case COMMAND_F6:
  case COMMAND_SF6:
  case COMMAND_CF6:
  case COMMAND_F7:
  case COMMAND_SF7:
  case COMMAND_CF7:
  case COMMAND_F8:
  case COMMAND_SF8:
  case COMMAND_CF8:
  case COMMAND_F9:
  case COMMAND_SF9:
  case COMMAND_SF10:
    bNeedToRedraw = false;
    nKeyStroke = nCommandID - 256;
    break;

  default:
    nKeyStroke = 0;
    break;
  }

  if (nKeyStroke) {
    if (bNeedToRedraw) {
      bout.cls();
    }

    a()->handle_sysop_key(static_cast<uint8_t>(nKeyStroke));

    if (bNeedToRedraw) {
      bout.cls();
    }
  }
  return bNeedToRedraw;
}

/**
 * copyfile - Copies a file from one location to another
 *
 * @param input - fully-qualified name of the source file
 * @param output - fully-qualified name of the destination file
 * @param stats - if true, print stuff to the screen
 *
 * @return - false on failure, true on success
 *
 */
bool copyfile(const string& sourceFileName, const string& destFileName, bool stats) {
  if (stats) {
    bout << "|#7File movement ";
  }

  if (sourceFileName != destFileName &&
      File::Exists(sourceFileName) &&
      !File::Exists(destFileName)) {
    std::error_code ec;
    return wwiv::fs::copy_file(sourceFileName, destFileName, ec);
  }
  return false;
}

/**
 * movefile - Moves a file from one location to another
 *
 * @param src - fully-qualified name of the source file
 * @param dst - fully-qualified name of the destination file
 * @param stats - if true, print stuff to the screen (not used)
 *
 * @return - false on failure, true on success
 *
 */
bool movefile(const string& sourceFileName, const string& destFileName, bool stats) {
  if (sourceFileName != destFileName && File::Exists(sourceFileName)) {
    bool bCanUseRename = false;

    if (sourceFileName[1] != ':' && destFileName[1] != ':') {
      bCanUseRename = true;
    }
    if (sourceFileName[1] == ':' && destFileName[1] == ':' && sourceFileName[0] == destFileName[0]) {
      bCanUseRename = true;
    }

    if (bCanUseRename) {
      File::Rename(sourceFileName, destFileName);
      if (File::Exists(destFileName)) {
        return false;
      }
    }
  }
  bool bCopyFileResult = copyfile(sourceFileName, destFileName, stats);
  File::Remove(sourceFileName);

  return bCopyFileResult;
}

void ListAllColors() {
  bout.nl();
  for (int i = 0; i < 128; i++) {
    if ((i % 26) == 0) {
      bout.nl();
    }
    bout.SystemColor(i);
    bout.bprintf("%3d", i);
  }
  bout.Color(0);
  bout.nl();
}
