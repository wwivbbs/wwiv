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


const char *interpret(char chKey) {
  static char s[255];

  memset(s, 0, sizeof(s));
  if (g_flags & g_flag_disable_mci) {
    return "";
  }

  switch (chKey) {
  case '@':                               // Dir name
    strcpy(s, directories[udir[GetSession()->GetCurrentFileArea()].subnum].name);
    break;
  case '~':                               // Total mails/feedbacks sent
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumEmailSent() +
             GetSession()->GetCurrentUser()->GetNumFeedbackSent() + GetSession()->GetCurrentUser()->GetNumNetEmailSent());
    break;
  case '/':                               // Today's date
    strcpy(s, fulldate());
    break;
  case '%':                               // Time left today
    snprintf(s, sizeof(s), "%d", static_cast<int>(nsl() / 60));
    break;
  case '#':                               // User's number
    snprintf(s, sizeof(s), "%d", GetSession()->usernum);
    break;
  case '$':                               // File points
    snprintf(s, sizeof(s), "%lu", GetSession()->GetCurrentUser()->GetFilePoints());
    break;
  case '*':                               // User reg num
    snprintf(s, sizeof(s), "%lu", GetSession()->GetCurrentUser()->GetWWIVRegNumber());
    break;
  case '-':                               // Aggravation points
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetAssPoints());
    break;
  case ':':                               // Sub number
    strcpy(s, usub[GetSession()->GetCurrentMessageArea()].keys);
    break;
  case ';':                               // Directory number
    strcpy(s, udir[GetSession()->GetCurrentFileArea()].keys);
    break;
  case '!':                               // Built-in pause
    pausescr();
    break;
  case '&': {
    strcpy(s, GetSession()->GetCurrentUser()->HasAnsi() ? "ANSI" : "ASCII");
  }
  break;
  case 'A':                               // User's age
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetAge());
    break;
  case 'a':                               // User's language
    strcpy(s, cur_lang_name);
    break;
  case 'B':                               // User's birthday
    snprintf(s, sizeof(s), "%d/%d/%d", GetSession()->GetCurrentUser()->GetBirthdayMonth(),
             GetSession()->GetCurrentUser()->GetBirthdayDay(), GetSession()->GetCurrentUser()->GetBirthdayYear());
    break;
  case 'b':                               // Minutes in bank
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetTimeBankMinutes());
    break;
  case 'C':                               // User's city
    strcpy(s, GetSession()->GetCurrentUser()->GetCity());
    break;
  case 'c':                               // User's country
    strcpy(s, GetSession()->GetCurrentUser()->GetCountry());
    break;
  case 'D':                               // Files downloaded
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetFilesDownloaded());
    break;
  case 'd':                               // User's DSL
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetDsl());
    break;
  case 'E':                               // E-mails sent
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumEmailSent());
    break;
  case 'e':                               // Net E-mails sent
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumNetEmailSent());
    break;
  case 'F':
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumFeedbackSent());
    break;
  case 'f':                               // First time user called
    strcpy(s, GetSession()->GetCurrentUser()->GetFirstOn());
    break;
  case 'G':                               // MessaGes read
    snprintf(s, sizeof(s), "%lu", GetSession()->GetCurrentUser()->GetNumMessagesRead());
    break;
  case 'g':                               // Gold
    snprintf(s, sizeof(s), "%f", GetSession()->GetCurrentUser()->GetGold());
    break;
  case 'I':                               // User's call sIgn
    strcpy(s, GetSession()->GetCurrentUser()->GetCallsign());
    break;
  case 'i':                               // Illegal log-ons
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumIllegalLogons());
    break;
  case 'J':                               // Message conference
    strcpy(s, reinterpret_cast<char*>(subconfs[uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum].name));
    break;
  case 'j':                               // Transfer conference
    strcpy(s, reinterpret_cast<char*>(dirconfs[uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum].name));
    break;
  case 'K':                               // Kb uploaded
    snprintf(s, sizeof(s), "%lu", GetSession()->GetCurrentUser()->GetUploadK());
    break;
  case 'k':                               // Kb downloaded
    snprintf(s, sizeof(s), "%lu", GetSession()->GetCurrentUser()->GetDownloadK());
    break;
  case 'L':                               // Last call
    strcpy(s, GetSession()->GetCurrentUser()->GetLastOn());
    break;
  case 'l':                               // Number of logons
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumLogons());
    break;
  case 'M':                               // Mail waiting
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetNumMailWaiting());
    break;
  case 'm':                               // Messages posted
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetNumMessagesPosted());
    break;
  case 'N':                               // User's name
    strcpy(s, GetSession()->GetCurrentUser()->GetName());
    break;
  case 'n':                               // Sysop's note
    strcpy(s, GetSession()->GetCurrentUser()->GetNote());
    break;
  case 'O':                               // Times on today
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetTimesOnToday());
    break;
  case 'o':                               // Time on today
    snprintf(s, sizeof(s), "%ld", static_cast<long>((GetSession()->GetCurrentUser()->GetTimeOn() +
             timer() - timeon) /
             SECONDS_PER_MINUTE_FLOAT));
    break;
  case 'P':                               // BBS phone
    strcpy(s, reinterpret_cast<char*>(syscfg.systemphone));
    break;
  case 'p':                               // User's phone
    strcpy(s, GetSession()->GetCurrentUser()->GetDataPhoneNumber());
    break;
  case 'R':                               // User's real name
    strcpy(s, GetSession()->GetCurrentUser()->GetRealName());
    break;
  case 'r':                               // Last baud rate
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetLastBaudRate());
    break;
  case 'S':                               // User's SL
    snprintf(s, sizeof(s), "%d", GetSession()->GetCurrentUser()->GetSl());
    break;
  case 's':                               // User's street address
    strcpy(s, GetSession()->GetCurrentUser()->GetStreet());
    break;
  case 'T':                               // User's sTate
    strcpy(s, GetSession()->GetCurrentUser()->GetState());
    break;
  case 't':                               // Current time
    strcpy(s, times());
    break;
  case 'U':                               // Files uploaded
    snprintf(s, sizeof(s), "%u", GetSession()->GetCurrentUser()->GetFilesUploaded());
    break;
  case 'u':                               // Current sub
    strcpy(s, subboards[usub[GetSession()->GetCurrentMessageArea()].subnum].name);
    break;
  case 'W':                               // Total # of messages in sub
    snprintf(s, sizeof(s), "%d", GetSession()->GetNumMessagesInCurrentMessageArea());
    break;
  case 'X':                               // User's sex
    snprintf(s, sizeof(s), "%c", GetSession()->GetCurrentUser()->GetGender());
    break;
  case 'Y':                               // Your BBS name
    strcpy(s, syscfg.systemname);
    break;
  case 'y':                               // Computer type
    strcpy(s, ctypes(GetSession()->GetCurrentUser()->GetComputerType()));
    break;
  case 'Z':                               // User's zip code
    strcpy(s, GetSession()->GetCurrentUser()->GetZipcode());
    break;
  default:
    return "";
  }

  return s;
}

