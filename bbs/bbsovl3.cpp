/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"


// The final character of an ansi sequence
#define OB ('[')
#define O ('O')
#define A_HOME ('H')
#define A_LEFT ('D')
#define A_END ('K')
#define A_UP ('A')
#define A_DOWN ('B')
#define A_RIGHT ('C')
#define A_INSERT ('r')
#define A_DELETE ('s')

static int pd_getkey() {
  g_flags |= g_flag_allow_extended;
  int x = getkey();
  g_flags &= ~g_flag_allow_extended;
  return x;
}

int get_kb_event(int nNumLockMode) {
  int key = 0;
  GetSession()->localIO()->tleft(true);
  time_t time1 = time(NULL);

  do {
    time_t time2 = time(NULL);
    if (difftime(time2, time1) > 180) {
      // greater than 3 minutes
      hangup = true;
      return 0;
    }
    if (hangup) {
      return 0;
    }

    if (bkbhitraw() || GetSession()->localIO()->LocalKeyPressed()) {
      if (!incom || GetSession()->localIO()->LocalKeyPressed()) {
        // Check for local keys
        key = GetSession()->localIO()->LocalGetChar();
        if (key == CBACKSPACE) {
          return COMMAND_DELETE;
        }
        if (key == CV) {
          return COMMAND_INSERT;
        }
        if (key == RETURN || key == CL) {
          return EXECUTE;
        }
        if ((key == 0 || key == 224) && GetSession()->localIO()->LocalKeyPressed()) {
          // again, the 224 is an artifact of Win32, I dunno why.
          key = GetSession()->localIO()->LocalGetChar();
          return key + 256;
        } else {
          if (nNumLockMode == NOTNUMBERS) {
            switch (key) {
            case '8':
              return COMMAND_UP;
            case '4':
              return COMMAND_LEFT;
            case '2':
              return COMMAND_DOWN;
            case '6':
              return COMMAND_RIGHT;
            case '0':
              return COMMAND_INSERT;
            case '.':
              return COMMAND_DELETE;
            case '9':
              return COMMAND_PAGEUP;
            case '3':
              return COMMAND_PAGEDN;
            case '7':
              return COMMAND_HOME;
            case '1':
              return COMMAND_END;
            }
          }
          switch (key) {
          case TAB:
            return TAB;
          case ESC:
            return GET_OUT;
          default:
            return key;
          }
        }
      } else if (bkbhitraw()) {
        key = pd_getkey();

        if (key == CBACKSPACE) {
          return COMMAND_DELETE;
        }
        if (key == CV) {
          return COMMAND_INSERT;
        }
        if (key == RETURN || key == CL) {
          return EXECUTE;
        } else if (key == ESC) {
          time_t time1 = time(NULL);
          time_t time2 = time(NULL);
          do {
            time2 = time(NULL);
            if (bkbhitraw()) {
              key = pd_getkey();
              if (key == OB || key == O) {
                key = pd_getkey();

                // Check for a second set of brackets
                if (key == OB || key == O) {
                  key = pd_getkey();
                }

                switch (key) {
                case A_UP:
                  return COMMAND_UP;
                case A_LEFT:
                  return COMMAND_LEFT;
                case A_DOWN:
                  return COMMAND_DOWN;
                case A_RIGHT:
                  return COMMAND_RIGHT;
                case A_INSERT:
                  return COMMAND_INSERT;
                case A_DELETE:
                  return COMMAND_DELETE;
                case A_HOME:
                  return COMMAND_HOME;
                case A_END:
                  return COMMAND_END;
                default:
                  return key;
                }
              } else {
                return GET_OUT;
              }
            }
          } while (difftime(time2, time1) < 1 && !hangup);

          if (difftime(time2, time1) >= 1) {     // if no keys followed ESC
            return GET_OUT;
          }
        } else {
          if (!key) {
            if (GetSession()->localIO()->LocalKeyPressed()) {
              key = GetSession()->localIO()->LocalGetChar();
              return (key + 256);
            }
          }
          if (nNumLockMode == NOTNUMBERS) {
            switch (key) {
            case '8':
              return COMMAND_UP;
            case '4':
              return COMMAND_LEFT;
            case '2':
              return COMMAND_DOWN;
            case '6':
              return COMMAND_RIGHT;
            case '0':
              return COMMAND_INSERT;
            case '.':
              return COMMAND_DELETE;
            case '9':
              return COMMAND_PAGEUP;
            case '3':
              return COMMAND_PAGEDN;
            case '7':
              return COMMAND_HOME;
            case '1':
              return COMMAND_END;
            }
          }
          return key;
        }
      }
      time1 = time(NULL);                           // reset timer
    } else {
      giveup_timeslice();
    }

  } while (!hangup);
  return 0;                                 // must have hung up
}

// Like onek but does not put cursor down a line
// One key, no carriage return
char onek_ncr(const char *pszAllowableChars) {
  WWIV_ASSERT(pszAllowableChars);

  char ch = '\0';
  while (!strchr(pszAllowableChars, ch = wwiv::UpperCase<char>(getkey())) && !hangup)
    ;
  if (hangup) {
    ch = pszAllowableChars[0];
  }
  bputch(ch);
  return ch;
}

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
      GetSession()->bout.ClearScreen();
    }

    GetSession()->localIO()->skey(static_cast<char>(nKeyStroke));

    if (bNeedToRedraw) {
      GetSession()->bout.ClearScreen();
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
bool copyfile(const std::string& sourceFileName, const std::string& destFileName, bool stats) {
  if (stats) {
    GetSession()->bout << "|#7File movement ";
  }

  if ((sourceFileName != destFileName) &&
      WFile::Exists(sourceFileName) &&
      !WFile::Exists(destFileName)) {
    if (WFile::CopyFile(sourceFileName, destFileName)) {
      return true;
    }
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
bool movefile(const std::string& sourceFileName, const std::string& destFileName, bool stats) {
  if (sourceFileName != destFileName && WFile::Exists(sourceFileName)) {
    bool bCanUseRename = false;

    if (sourceFileName[1] != ':' && destFileName[1] != ':') {
      bCanUseRename = true;
    }
    if (sourceFileName[1] == ':' && destFileName[1] == ':' && sourceFileName[0] == destFileName[0]) {
      bCanUseRename = true;
    }

    if (bCanUseRename) {
      WFile::Rename(sourceFileName, destFileName);
      if (WFile::Exists(destFileName)) {
        return false;
      }
    }
  }
  bool bCopyFileResult = copyfile(sourceFileName, destFileName, stats);
  WFile::Remove(sourceFileName);

  return bCopyFileResult;
}

void ListAllColors() {
  GetSession()->bout.NewLine();
  for (int i = 0; i < 128; i++) {
    if ((i % 26) == 0) {
      GetSession()->bout.NewLine();
    }
    GetSession()->bout.SystemColor(i);
    GetSession()->bout.WriteFormatted("%3d", i);
  }
  GetSession()->bout.Color(0);
  GetSession()->bout.NewLine();
}
