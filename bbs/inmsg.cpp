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
#include <algorithm>
#include <string>

#include "wwiv.h"
#include "subxtr.h"
#include "printfile.h"
#include "core/strings.h"
#include "core/wtextfile.h"
#include "core/wwivassert.h"
#include "bbs/external_edit.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"

using std::string;
using wwiv::bbs::InputMode;
using wwiv::strings::GetStringLength;
using wwiv::strings::IsEqualsIgnoreCase;
using wwiv::strings::StringPrintf;

//
// Local function prototypes
//
void AddLineToMessageBuffer(char *pszMessageBuffer, const char *pszLineToAdd, long *plBufferLength);

bool InternalMessageEditor(char* lin, int maxli, int* curli, int* setanon, string *title);
void GetMessageTitle(string *title, bool force_title);
void UpdateMessageBufferTheadsInfo(char *pszMessageBuffer, long *plBufferLength, const char *aux);
void UpdateMessageBufferInReplyToInfo(char *pszMessageBuffer, long *plBufferLength, const char *aux);
void UpdateMessageBufferTagLine(char *pszMessageBuffer, long *plBufferLength, const char *aux);
void UpdateMessageBufferQuotesCtrlLines(char *pszMessageBuffer, long *plBufferLength);
void GetMessageAnonStatus(bool *real_name, int *anony, int setanon);

static const int LEN = 161;

static bool GetMessageToName(const char *aux) {
  // If GetSession()->GetCurrentReadMessageArea() is -1, then it hasn't been set by reading a sub,
  // also, if we are using e-mail, this is definately NOT a FidoNet
  // post so there's no reason in wasting everyone's time in the loop...
  WWIV_ASSERT(aux);
  if (GetSession()->GetCurrentReadMessageArea() == -1 ||
      IsEqualsIgnoreCase(aux, "email")) {
    return 0;
  }

  bool bHasAddress = false;
  bool newlsave = newline;

  if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) {
    for (int i = 0; i < xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets; i++) {
      xtrasubsnetrec *xnp = &xsubs[GetSession()->GetCurrentReadMessageArea()].nets[i];
      if (net_networks[xnp->net_num].type == net_type_fidonet &&
          !IsEqualsIgnoreCase(aux, "email")) {
        bHasAddress = true;
        GetSession()->bout << "|#1Fidonet addressee, |#7[|#2Enter|#7]|#1 for ALL |#0: ";
        newline = false;
        std::string to_name;
        input1(&to_name, 40, InputMode::MIXED, false, true);
        newline = newlsave;
        if (to_name.empty()) {
          strcpy(irt_name, "ALL");
          GetSession()->bout << "|#4All\r\n";
          GetSession()->bout.Color(0);
        } else {
          strcpy(irt_name, to_name.c_str());
        }
        strcpy(irt, "\xAB");
      }
    }
  }
  return bHasAddress;
}

void inmsg(messagerec * pMessageRecord, std::string* title, int *anony, bool needtitle, const char *aux, int fsed,
           const char *pszDestination, int flags, bool force_title) {
  char *lin = nullptr, *b = nullptr;

  int oiia = setiia(0);

  if (fsed != INMSG_NOFSED && !okfsed()) {
    fsed = INMSG_NOFSED;
  }

  const string exted_filename = StringPrintf("%s%s", syscfgovr.tempdir, INPUT_MSG);
  if (fsed) {
    fsed = INMSG_FSED;
  }
  if (use_workspace) {
    if (!WFile::Exists(exted_filename)) {
      use_workspace = false;
    } else {
      fsed = INMSG_FSED_WORKSPACE;
    }
  }

  int maxli = GetMaxMessageLinesAllowed();
  if (!fsed) {
    if ((lin = static_cast<char *>(BbsAllocA((maxli + 10) * LEN))) == nullptr) {
      pMessageRecord->stored_as = 0xffffffff;
      setiia(oiia);
      return;
    }
    for (int i = 0; i < maxli; i++) {
      lin[i * LEN] = '\0';
    }
  }
  GetSession()->bout.NewLine();

  if (irt_name[0] == '\0') {
    if (GetMessageToName(aux)) {
      GetSession()->bout.NewLine();
    }
  }

  GetMessageTitle(title, force_title);
  if (title->empty() && needtitle) {
    GetSession()->bout << "|#6Aborted.\r\n";
    pMessageRecord->stored_as = 0xffffffff;
    if (!fsed) {
      free(lin);
    }
    setiia(oiia);
    return;
  }

  int setanon = 0;
  int curli = 0;
  bool bSaveMessage = false;
  if (fsed == INMSG_NOFSED) {   // Use Internal Message Editor
    bSaveMessage = InternalMessageEditor(lin, maxli, &curli, &setanon, title);
  } else if (fsed == INMSG_FSED) {   // Use Full Screen Editor
    bSaveMessage = ExternalMessageEditor(maxli, &setanon, title, pszDestination, flags, aux);
  } else if (fsed == INMSG_FSED_WORKSPACE) {   // "auto-send mail message"
    bSaveMessage = WFile::Exists(exted_filename);
    if (bSaveMessage && !GetSession()->IsNewMailWatiting()) {
      GetSession()->bout << "Reading in file...\r\n";
    }
    use_workspace = false;
  }

  if (bSaveMessage) {
    long lMaxMessageSize = 0;
    bool real_name = false;
    GetMessageAnonStatus(&real_name, anony, setanon);
    GetSession()->bout.BackLine();
    if (!GetSession()->IsNewMailWatiting()) {
      SpinPuts("Saving...", 2);
    }
    WFile fileExtEd(exted_filename);
    if (fsed) {
      fileExtEd.Open(WFile::modeBinary | WFile::modeReadOnly);
      long lExternalEditorFileSize = fileExtEd.GetLength();
      lMaxMessageSize  = std::max<long>(lExternalEditorFileSize, 30000);
    } else {
      for (int i5 = 0; i5 < curli; i5++) {
        lMaxMessageSize  += (strlen(&(lin[i5 * LEN])) + 2);
      }
    }
    lMaxMessageSize  += 1024;
    if ((b = static_cast<char *>(BbsAllocA(lMaxMessageSize))) == nullptr) {
      free(lin);
      GetSession()->bout << "Out of memory.\r\n";
      pMessageRecord->stored_as = 0xffffffff;
      setiia(oiia);
      return;
    }
    long lCurrentMessageSize = 0;

    // Add author name
    if (real_name) {
      AddLineToMessageBuffer(b, GetSession()->GetCurrentUser()->GetRealName(), &lCurrentMessageSize);
    } else {
      if (GetSession()->IsNewMailWatiting()) {
        const string sysop_name = StringPrintf("%s #1", syscfg.sysopname);
        AddLineToMessageBuffer(b, sysop_name.c_str(), &lCurrentMessageSize);
      } else {
        AddLineToMessageBuffer(b, GetSession()->GetCurrentUser()->GetUserNameNumberAndSystem(GetSession()->usernum, net_sysnum),
                               &lCurrentMessageSize);
      }
    }

    // Add date to message body
    time_t lTime = time(nullptr);
    string time_string(asctime(localtime(&lTime)));
    // asctime appends a \n to the end of the string.
    StringTrimEnd(&time_string);
    AddLineToMessageBuffer(b, time_string.c_str(), &lCurrentMessageSize);
    UpdateMessageBufferQuotesCtrlLines(b, &lCurrentMessageSize);

    if (GetSession()->IsMessageThreadingEnabled()) {
      UpdateMessageBufferTheadsInfo(b, &lCurrentMessageSize, aux);
    }
    if (irt[0]) {
      UpdateMessageBufferInReplyToInfo(b, &lCurrentMessageSize, aux);
    }
    if (fsed) {
      // Read the file produced by the external editor and add it to 'b'
      long lFileSize = fileExtEd.GetLength();
      fileExtEd.Read((&(b[lCurrentMessageSize])), lFileSize);
      lCurrentMessageSize += lFileSize;
      fileExtEd.Close();
    } else {
      // iterate through the lines in "char *lin" and append them to 'b'
      for (int i5 = 0; i5 < curli; i5++) {
        AddLineToMessageBuffer(b, &(lin[i5 * LEN]), &lCurrentMessageSize);
      }
    }

    if (GetApplication()->HasConfigFlag(OP_FLAGS_MSG_TAG)) {
      UpdateMessageBufferTagLine(b, &lCurrentMessageSize, aux);
    }
    if (b[lCurrentMessageSize - 1] != CZ) {
      b[lCurrentMessageSize++] = CZ;
    }
    savefile(b, lCurrentMessageSize, pMessageRecord, aux);
  } else {
    GetSession()->bout << "|#6Aborted.\r\n";
    pMessageRecord->stored_as = 0xffffffff;
  }
  if (fsed) {
    WFile::Remove(exted_filename);
  }
  if (!fsed) {
    free(lin);
  }
  charbufferpointer = 0;
  charbuffer[0] = '\0';
  setiia(oiia);
  grab_quotes(nullptr, nullptr);
}

void AddLineToMessageBuffer(char *pszMessageBuffer, const char *pszLineToAdd, long *plBufferLength) {
  strcpy(&(pszMessageBuffer[ *plBufferLength ]), pszLineToAdd);
  *plBufferLength += strlen(pszLineToAdd);
  strcpy(&(pszMessageBuffer[ *plBufferLength ]), "\r\n");
  *plBufferLength += 2;
}

static void ReplaceString(char *pszResult, char *pszOld, char *pszNew) {
  if (strlen(pszResult) - strlen(pszOld) + strlen(pszNew) >= LEN) {
    return;
  }
  char* ss = strstr(pszResult, pszOld);
  if (ss == nullptr) {
    return;
  }
  ss[0] = '\0';

  const string result = StringPrintf("%s%s%s", pszResult, pszNew, ss + strlen(pszOld));
  strcpy(pszResult, result.c_str());
}

bool InternalMessageEditor(char* lin, int maxli, int* curli, int* setanon, string *title) {
  bool abort, next;
  char s[ 255 ];
  char s1[ 255 ];

  GetSession()->bout.NewLine(2);
  GetSession()->bout << "|#9Enter message now, you can use |#2" << maxli << "|#9 lines.\r\n";
  GetSession()->bout <<
                     "|#9Colors: ^P-0\003""11\003""22\003""33\003""44\003""55\003""66\003""77\003""88\003""99\003""AA\003""BB\003""CC\003""DD\003""EE\003""FF\003""GG\003""HH\003""II\003""JJ\003""KK\003""LL\003""MM\003""NN\003""OO\003""PP\003""QQ\003""RR\003""SS\003""""\003""0";
  GetSession()->bout <<
                     "\003""TT\003""UU\003""VV\003""WW\003""XX\003""YY\003""ZZ\003""aa\003""bb\003""cc\003""dd\003""ee\003""ff\003""gg\003""hh\003""ii\003""jj\003""kk\003""ll\003""mm\003""nn\003""oo\003""pp\003""qq\003""rr\003""ss\003""tt\003""uu\003""vv\003""ww\003""xx\003""yy\003""zz\r\n";
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#1Enter |#2/Q|#1 to quote previous message, |#2/HELP|#1 for other editor commands.\r\n";
  strcpy(s, "[---=----=----=----=----=----=----=----]----=----=----=----=----=----=----=----]");
  if (GetSession()->GetCurrentUser()->GetScreenChars() < 80) {
    s[ GetSession()->GetCurrentUser()->GetScreenChars() ] = '\0';
  }
  GetSession()->bout.Color(7);
  GetSession()->bout << s;
  GetSession()->bout.NewLine();

  bool bCheckMessageSize = true;
  bool bSaveMessage = false;
  bool done = false;
  char szRollOverLine[ 81 ];
  szRollOverLine[ 0 ] = '\0';
  while (!done && !hangup) {
    bool bAllowPrevious = (*curli > 0) ? true : false;
    while (inli(s, szRollOverLine, 160, true, bAllowPrevious)) {
      (*curli)--;
      strcpy(szRollOverLine, &(lin[(*curli) * LEN]));
      if (GetStringLength(szRollOverLine) > GetSession()->GetCurrentUser()->GetScreenChars() - 1) {
        szRollOverLine[ GetSession()->GetCurrentUser()->GetScreenChars() - 2 ] = '\0';
      }
    }
    if (hangup) {
      done = true;
    }
    bCheckMessageSize = true;
    if (s[0] == '/') {
      if ((IsEqualsIgnoreCase(s, "/HELP")) ||
          (IsEqualsIgnoreCase(s, "/H")) ||
          (IsEqualsIgnoreCase(s, "/?"))) {
        bCheckMessageSize = false;
        printfile(EDITOR_NOEXT);
      } else if (IsEqualsIgnoreCase(s, "/QUOTE") ||
                 IsEqualsIgnoreCase(s, "/Q")) {
        bCheckMessageSize = false;
        if (quotes_ind != nullptr) {
          get_quote(0);
        }
      } else if (IsEqualsIgnoreCase(s, "/LI")) {
        bCheckMessageSize = false;
        if (okansi()) {
          next = false;
        } else {
          GetSession()->bout << "|#5With line numbers? ";
          next = yesno();
        }
        abort = false;
        for (int i = 0; (i < *curli) && !abort; i++) {
          if (next) {
            GetSession()->bout << i + 1 << ":" << wwiv::endl;
          }
          strcpy(s1, &(lin[i * LEN]));
          int i3 = strlen(s1);
          if (s1[i3 - 1] == 1) {
            s1[i3 - 1] = '\0';
          }
          if (s1[0] == 2) {
            strcpy(s1, &(s1[1]));
            int i5 = 0;
            int i4 = 0;
            for (i4 = 0; i4 < GetStringLength(s1); i4++) {
              if ((s1[i4] == 8) || (s1[i4] == 3)) {
                --i5;
              } else {
                ++i5;
              }
            }
            for (i4 = 0; (i4 < (GetSession()->GetCurrentUser()->GetScreenChars() - i5) / 2) && (!abort); i4++) {
              osan(" ", &abort, &next);
            }
          }
          pla(s1, &abort);
        }
        if (!okansi() || next) {
          GetSession()->bout.NewLine();
          GetSession()->bout << "Continue...\r\n";
        }
      } else if (IsEqualsIgnoreCase(s, "/ES") ||
                 IsEqualsIgnoreCase(s, "/S")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
      } else if (IsEqualsIgnoreCase(s, "/ESY") ||
                 IsEqualsIgnoreCase(s, "/SY")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
        *setanon = 1;
      } else if (IsEqualsIgnoreCase(s, "/ESN") ||
                 IsEqualsIgnoreCase(s, "/SN")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
        *setanon = -1;
      } else if (IsEqualsIgnoreCase(s, "/ABT") ||
                 IsEqualsIgnoreCase(s, "/A")) {
        done = true;
        bCheckMessageSize = false;
      } else if (IsEqualsIgnoreCase(s, "/CLR")) {
        bCheckMessageSize = false;
        *curli = 0;
        GetSession()->bout << "Message cleared... Start over...\r\n\n";
      } else if (IsEqualsIgnoreCase(s, "/RL")) {
        bCheckMessageSize = false;
        if (*curli) {
          (*curli)--;
          GetSession()->bout << "Replace:\r\n";
        } else {
          GetSession()->bout << "Nothing to replace.\r\n";
        }
      } else if (IsEqualsIgnoreCase(s, "/TI")) {
        bCheckMessageSize = false;
        if (okansi()) {
          GetSession()->bout << "|#1Subj|#7: |#2" ;
          inputl(title, 60, true);
        } else {
          GetSession()->bout << "       (---=----=----=----=----=----=----=----=----=----=----=----)\r\n";
          GetSession()->bout << "|#1Subj|#7: |#2";
          inputl(title, 60);
        }
        GetSession()->bout << "Continue...\r\n\n";
      }
      strcpy(s1, s);
      s1[3] = '\0';
      if (IsEqualsIgnoreCase(s1, "/C:")) {
        s1[0] = 2;
        strcpy((&s1[1]), &(s[3]));
        strcpy(s, s1);
      } else if (IsEqualsIgnoreCase(s1, "/SU") &&
                 s[3] == '/' && *curli > 0) {
        strcpy(s1, &(s[4]));
        char *ss = strstr(s1, "/");
        if (ss) {
          char *ss1 = &(ss[1]);
          ss[0] = '\0';
          ReplaceString(&(lin[(*curli - 1) * LEN]), s1, ss1);
          GetSession()->bout << "Last line:\r\n" << &(lin[(*curli - 1) * LEN]) << "\r\nContinue...\r\n";
        }
        bCheckMessageSize = false;
      }
    }

    if (bCheckMessageSize) {
      strcpy(&(lin[((*curli)++) * LEN]), s);
      if (*curli == (maxli + 1)) {
        GetSession()->bout << "\r\n-= No more lines, last line lost =-\r\n/S to save\r\n\n";
        (*curli)--;
      } else if (*curli == maxli) {
        GetSession()->bout << "-= Message limit reached, /S to save =-\r\n";
      } else if ((*curli + 5) == maxli) {
        GetSession()->bout << "-= 5 lines left =-\r\n";
      }
    }
  }
  if (*curli == 0) {
    bSaveMessage = false;
  }
  return bSaveMessage;
}

void GetMessageTitle(string *title, bool force_title) {
  if (okansi()) {
    if (!GetSession()->IsNewMailWatiting()) {
      GetSession()->bout << "|#2Title: ";
      GetSession()->bout.ColorizedInputField(60);
    }
    if (irt[0] != '\xAB' && irt[0]) {
      char s1[ 255 ];
      char ch = '\0';
      StringTrim(irt);
      if (strncasecmp(stripcolors(irt), "re:", 3) != 0) {
        if (GetSession()->IsNewMailWatiting()) {
          sprintf(s1, "%s", irt);
          irt[0] = '\0';
        } else {
          sprintf(s1, "Re: %s", irt);
        }
      } else {
        sprintf(s1, "%s", irt);
      }
      s1[60] = '\0';
      if (!GetSession()->IsNewMailWatiting() && !force_title) {
        GetSession()->bout << s1;
        ch = getkey();
        if (ch == 10) {
          ch = getkey();
        }
      } else {
        title->assign(s1);
        ch = RETURN;
      }
      force_title = false;
      if (ch != RETURN) {
        GetSession()->bout << "\r";
        if (ch == SPACE || ch == ESC) {
          ch = '\0';
        }
        GetSession()->bout << "|#2Title: ";
        GetSession()->bout.ColorizedInputField(60);
        char szRollOverLine[ 81 ];
        sprintf(szRollOverLine, "%c", ch);
        inli(s1, szRollOverLine, 60, true, false);
        title->assign(s1);
      } else {
        GetSession()->bout.NewLine();
        title->assign(s1);
      }
    } else {
      inputl(title, 60);
    }
  } else {
    if (GetSession()->IsNewMailWatiting() || force_title) {
      title->assign(irt);
    } else {
      GetSession()->bout << "       (---=----=----=----=----=----=----=----=----=----=----=----)\r\n";
      GetSession()->bout << "Title: ";
      inputl(title, 60);
    }
  }
}

void UpdateMessageBufferTheadsInfo(char *pszMessageBuffer, long *plBufferLength, const char *aux) {
  if (!IsEqualsIgnoreCase(aux, "email")) {
    long msgid = time(nullptr);
    long targcrc = crc32buf(pszMessageBuffer, strlen(pszMessageBuffer));
    const string buf = StringPrintf("0P %lX-%lX", targcrc, msgid);
    AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
    if (thread) {
      thread [GetSession()->GetNumMessagesInCurrentMessageArea() + 1 ].msg_num = static_cast< unsigned short>
          (GetSession()->GetNumMessagesInCurrentMessageArea() + 1);
      strcpy(thread[GetSession()->GetNumMessagesInCurrentMessageArea() + 1].message_code, &buf.c_str()[4]);
    }
    if (GetSession()->threadID.length() > 0) {
      const string buf = StringPrintf("0W %s", GetSession()->threadID.c_str());
      AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
      if (thread) {
        strcpy(thread[GetSession()->GetNumMessagesInCurrentMessageArea() + 1].parent_code, &buf.c_str()[4]);
        thread[ GetSession()->GetNumMessagesInCurrentMessageArea() + 1 ].used = 1;
      }
    }
  }
}

void UpdateMessageBufferInReplyToInfo(char *pszMessageBuffer, long *plBufferLength, const char *aux) {
  if (irt_name[0] &&
      !IsEqualsIgnoreCase(aux, "email") &&
      xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) {
    for (int i = 0; i < xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets; i++) {
      xtrasubsnetrec *xnp = &xsubs[GetSession()->GetCurrentReadMessageArea()].nets[i];
      if (net_networks[xnp->net_num].type == net_type_fidonet) {
        const string buf = StringPrintf("0FidoAddr: %s", irt_name);
        AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
        break;
      }
    }
  }
  if ((strncasecmp("internet", GetSession()->GetNetworkName(), 8) == 0) ||
      (strncasecmp("filenet", GetSession()->GetNetworkName(), 7) == 0)) {
    if (GetSession()->usenetReferencesLine.length() > 0) {
      const string buf = StringPrintf("%c0RReferences: %s", CD, GetSession()->usenetReferencesLine.c_str());
      AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
      GetSession()->usenetReferencesLine = "";
    }
  }
  if (irt[0] != '"') {
    const string buf = StringPrintf("RE: %s", irt);
    AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
    if (irt_sub[0]) {
      const string buf = StringPrintf("ON: %s", irt_sub);
      AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
    }
  } else {
    irt_sub[0] = '\0';
  }

  if (irt_name[0] &&
      !IsEqualsIgnoreCase(aux, "email")) {
    const string buf = StringPrintf("BY: %s", irt_name);
    AddLineToMessageBuffer(pszMessageBuffer, buf.c_str(), plBufferLength);
  }
  AddLineToMessageBuffer(pszMessageBuffer, "", plBufferLength);
}


void UpdateMessageBufferTagLine(char *pszMessageBuffer, long *plBufferLength, const char *aux) {
  char szMultiMail[] = "Multi-Mail";
  if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets &&
      !IsEqualsIgnoreCase(aux, "email") &&
      (!(subboards[GetSession()->GetCurrentReadMessageArea()].anony & anony_no_tag)) &&
      !IsEqualsIgnoreCase(irt, szMultiMail)) {
    char szFileName[ MAX_PATH ];
    for (int i = 0; i < xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets; i++) {
      xtrasubsnetrec *xnp = &xsubs[GetSession()->GetCurrentReadMessageArea()].nets[i];
      char *nd = net_networks[xnp->net_num].dir;
      sprintf(szFileName, "%s%s.tag", nd, xnp->stype);
      if (WFile::Exists(szFileName)) {
        break;
      }
      sprintf(szFileName, "%s%s", nd, GENERAL_TAG);
      if (WFile::Exists(szFileName)) {
        break;
      }
      sprintf(szFileName, "%s%s.tag", syscfg.datadir, xnp->stype);
      if (WFile::Exists(szFileName)) {
        break;
      }
      sprintf(szFileName, "%s%s", syscfg.datadir, GENERAL_TAG);
      if (WFile::Exists(szFileName)) {
        break;
      }
    }
    WTextFile file(szFileName, "rb");
    if (file.IsOpen()) {
      int j = 0;
      while (!file.IsEndOfFile()) {
        char s[ 181 ];
        s[0] = '\0';
        char s1[ 181 ];
        s1[0] = '\0';
        file.ReadLine(s, 180);
        if (strlen(s) > 1) {
          if (s[strlen(s) - 2] == RETURN) {
            s[strlen(s) - 2] = '\0';
          }
        }
        if (s[0] != CD) {
          sprintf(s1, "%c%c%s", CD, j + '2', s);
        } else {
          strncpy(s1, s, sizeof(s1));
        }
        if (!j) {
          AddLineToMessageBuffer(pszMessageBuffer, "1", plBufferLength);
        }
        AddLineToMessageBuffer(pszMessageBuffer, s1, plBufferLength);
        if (j < 7) {
          j++;
        }
      }
      file.Close();
    }
  }
}

void UpdateMessageBufferQuotesCtrlLines(char *pszMessageBuffer, long *plBufferLength) {
  char szQuotesFileName[ MAX_PATH ];
  sprintf(szQuotesFileName, "%s%s", syscfgovr.tempdir, QUOTES_TXT);
  WTextFile file(szQuotesFileName, "rt");
  if (file.IsOpen()) {
    char szQuoteText[ 255 ];
    while (file.ReadLine(szQuoteText, sizeof(szQuoteText) - 1)) {
      char *ss1 = strchr(szQuoteText, '\n');
      if (ss1) {
        *ss1 = '\0';
      }
      if (strncmp(szQuoteText, "\004""0U", 3) == 0) {
        AddLineToMessageBuffer(pszMessageBuffer, szQuoteText, plBufferLength);
      }
    }
    file.Close();
  }

  char szMsgInfFileName[ MAX_PATH ];
  sprintf(szMsgInfFileName, "%smsginf", syscfgovr.tempdir);
  copyfile(szQuotesFileName, szMsgInfFileName, false);

}

void GetMessageAnonStatus(bool *real_name, int *anony, int setanon) {
  // Changed *anony to anony
  switch (*anony) {
  case 0:
    *anony = 0;
    break;
  case anony_enable_anony:
    if (setanon) {
      if (setanon == 1) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    } else {
      GetSession()->bout << "|#5Anonymous? ";
      if (yesno()) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    }
    break;
  case anony_enable_dear_abby: {
    GetSession()->bout.NewLine();
    GetSession()->bout << "1. " << GetSession()->GetCurrentUser()->GetUserNameAndNumber(
                         GetSession()->usernum) << wwiv::endl;
    GetSession()->bout << "2. Abby\r\n";
    GetSession()->bout << "3. Problemed Person\r\n\n";
    GetSession()->bout << "|#5Which? ";
    char chx = onek("\r123");
    switch (chx) {
    case '\r':
    case '1':
      *anony = 0;
      break;
    case '2':
      *anony = anony_sender_da;
      break;
    case '3':
      *anony = anony_sender_pp;
    }
  }
  break;
  case anony_force_anony:
    *anony = anony_sender;
    break;
  case anony_real_name:
    *real_name = true;
    *anony = 0;
    break;
  }
}
