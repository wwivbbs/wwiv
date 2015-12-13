/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/inmsg.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsovl3.h"
#include "bbs/crc.h"
#include "bbs/input.h"
#include "bbs/subxtr.h"
#include "bbs/printfile.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/message_file.h"
#include "bbs/vars.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "bbs/external_edit.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::bbs::InputMode;
using namespace wwiv::strings;

static const int LEN = 161;
static const char crlf[] = "\r\n";

static bool GetMessageToName(const char *aux) {
  // If session()->GetCurrentReadMessageArea() is -1, then it hasn't been set by reading a sub,
  // also, if we are using e-mail, this is definately NOT a FidoNet
  // post so there's no reason in wasting everyone's time in the loop...
  WWIV_ASSERT(aux);
  if (session()->GetCurrentReadMessageArea() == -1 ||
      IsEqualsIgnoreCase(aux, "email")) {
    return 0;
  }

  bool bHasAddress = false;
  bool newlsave = newline;

  if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
    for (int i = 0; i < xsubs[session()->GetCurrentReadMessageArea()].num_nets; i++) {
      xtrasubsnetrec *xnp = &xsubs[session()->GetCurrentReadMessageArea()].nets[i];
      if (net_networks[xnp->net_num].type == net_type_fidonet &&
          !IsEqualsIgnoreCase(aux, "email")) {
        bHasAddress = true;
        bout << "|#1Fidonet addressee, |#7[|#2Enter|#7]|#1 for ALL |#0: ";
        newline = false;
        string to_name = Input1("", 40, true, InputMode::MIXED);
        newline = newlsave;
        if (to_name.empty()) {
          strcpy(irt_name, "ALL");
          bout << "|#4All\r\n";
          bout.Color(0);
        } else {
          strcpy(irt_name, to_name.c_str());
        }
        strcpy(irt, "\xAB");
      }
    }
  }
  return bHasAddress;
}

static void GetMessageTitle(MessageEditorData& data) {
  bool force_title = !data.title.empty();
  if (okansi()) {
    if (!data.silent_mode) {
      bout << "|#2Title: ";
      bout.mpl(60);
    }
    if (irt[0] != '\xAB' && irt[0]) {
      string s1;
      char ch = '\0';
      StringTrim(irt);
      if (strncasecmp(stripcolors(irt), "re:", 3) != 0) {
        if (data.silent_mode) {
          s1 = irt;
          irt[0] = '\0';
        } else {
          s1 = StrCat("Re: ", irt);
        }
      } else {
        s1 = irt;
      }
      if (s1.length() > 60) {
        s1.resize(60);
      }
      if (!data.silent_mode && !force_title) {
        bout << s1;
        ch = getkey();
        if (ch == 10) {
          ch = getkey();
        }
      } else {
        data.title.assign(s1);
        ch = RETURN;
      }
      force_title = false;
      if (ch != RETURN) {
        bout << "\r";
        if (ch == SPACE || ch == ESC) {
          ch = '\0';
        }
        bout << "|#2Title: ";
        bout.mpl(60);
        string rollover_line = StringPrintf("%c", ch);
        inli(&s1, &rollover_line, 60, true, false);
        data.title.assign(s1);
      } else {
        bout.nl();
        data.title.assign(s1);
      }
    } else {
      inputl(&data.title, 60);
    }
  } else {
    if (data.silent_mode || force_title) {
      data.title.assign(irt);
    } else {
      bout << "       (---=----=----=----=----=----=----=----=----=----=----=----)\r\n";
      bout << "Title: ";
      inputl(&data.title, 60);
    }
  }
}

static bool InternalMessageEditor(vector<string>& lin, int maxli, int* curli, int* setanon, string *title) {
  bool abort = false, next = false;

  bout.nl(2);
  bout << "|#9Enter message now, you can use |#2" << maxli << "|#9 lines.\r\n";
  bout << "|#9Colors: ^P-0\003""11\003""22\003""33\003""44\003""55\003""66\003""77\003""88\003""99\003""AA\003""BB\003""CC\003""DD\003""EE\003""FF\003""GG\003""HH\003""II\003""JJ\003""KK\003""LL\003""MM\003""NN\003""OO\003""PP\003""QQ\003""RR\003""SS\003""""\003""0";
  bout << "\003""TT\003""UU\003""VV\003""WW\003""XX\003""YY\003""ZZ\003""aa\003""bb\003""cc\003""dd\003""ee\003""ff\003""gg\003""hh\003""ii\003""jj\003""kk\003""ll\003""mm\003""nn\003""oo\003""pp\003""qq\003""rr\003""ss\003""tt\003""uu\003""vv\003""ww\003""xx\003""yy\003""zz\r\n";
  bout.nl();
  bout << "|#1Enter |#2/Q|#1 to quote previous message, |#2/HELP|#1 for other editor commands.\r\n";

  bout.Color(7);
  string header = "[---=----=----=----=----=----=----=----]----=----=----=----=----=----=----=----]";
  if (session()->user()->GetScreenChars() < 80) {
    header.resize(session()->user()->GetScreenChars());
  }
  bout << header;
  bout.nl();

  string current_line;
  bool bCheckMessageSize = true;
  bool bSaveMessage = false;
  bool done = false;
  string rollover_line;
  while (!done && !hangup) {
    bool bAllowPrevious = (*curli > 0) ? true : false;
    while (inli(&current_line, &rollover_line, 160, true, bAllowPrevious)) {
      (*curli)--;
      rollover_line = lin.at(*curli);
      if (rollover_line.length() > session()->user()->GetScreenChars() - 1) {
        rollover_line.resize(session()->user()->GetScreenChars() - 2);
      }
    }
    if (hangup) {
      done = true;
    }
    bCheckMessageSize = true;
    if (current_line[0] == '/') {
      string cmd = current_line;
      StringUpperCase(&cmd);
      if (((cmd == "/HELP")) ||
          ((cmd == "/H")) ||
          ((cmd == "/?"))) {
        bCheckMessageSize = false;
        printfile(EDITOR_NOEXT);
      } else if ((cmd == "/QUOTE") ||
                 (cmd == "/Q")) {
        bCheckMessageSize = false;
        if (quotes_ind != nullptr) {
          get_quote(0);
        }
      } else if ((cmd == "/LI")) {
        bCheckMessageSize = false;
        abort = false;
        for (int i = 0; (i < *curli) && !abort; i++) {
          string line = lin.at(i);
          if (line.back() == CA) {
            line.pop_back();
          }
          if (line.front() == CB) {
            line = line.substr(1);
            int i5 = 0;
            int i4 = 0;
            // TODO(rusnfan): Make a utility function to do this in strings.h
            for (i4 = 0; i4 < line.size(); i4++) {
              if ((line[i4] == 8) || (line[i4] == 3)) {
                --i5;
              } else {
                ++i5;
              }
            }
            for (i4 = 0; (i4 < (session()->user()->GetScreenChars() - i5) / 2) && (!abort); i4++) {
              osan(" ", &abort, &next);
            }
          }
          pla(line, &abort);
        }
        if (!okansi() || next) {
          bout.nl();
          bout << "Continue...\r\n";
        }
      } else if ((cmd == "/ES") ||
                 (cmd == "/S")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
      } else if ((cmd == "/ESY") ||
                 (cmd == "/SY")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
        *setanon = 1;
      } else if ((cmd == "/ESN") ||
                 (cmd == "/SN")) {
        bSaveMessage = true;
        done = true;
        bCheckMessageSize = false;
        *setanon = -1;
      } else if ((cmd == "/ABT") ||
                 (cmd == "/A")) {
        done = true;
        bCheckMessageSize = false;
      } else if ((cmd == "/CLR")) {
        bCheckMessageSize = false;
        *curli = 0;
        bout << "Message cleared... Start over...\r\n\n";
      } else if ((cmd == "/RL")) {
        bCheckMessageSize = false;
        if (*curli) {
          (*curli)--;
          bout << "Replace:\r\n";
        } else {
          bout << "Nothing to replace.\r\n";
        }
      } else if ((cmd == "/TI")) {
        bCheckMessageSize = false;
        bout << "|#1Subj|#7: |#2" ;
        inputl(title, 60, true);
        bout << "Continue...\r\n\n";
      }
      if (cmd.length() > 3) {
        cmd.resize(3);
      }
      if (cmd == "/C:") {
        const string centered_text = current_line.substr(3);
        current_line = StringPrintf("%c%s", CB, centered_text.c_str());
      } else if (cmd == "/SU" && current_line[3] == '/' && *curli > 0) {
        string old_string = current_line.substr(4);
        string::size_type slash = old_string.find('/');
        if (slash != string::npos) {
          string new_string = old_string.substr(slash + 1);
          old_string.resize(slash);
          // We've already advanced *curli to point to the new row, so we need
          // to back up one.
          int last_line = *curli - 1;
          string temp = lin.at(last_line);
          wwiv::strings::StringReplace(&temp, old_string, new_string);
          lin[last_line] = temp;
          bout << "Last line:\r\n" << lin.at(last_line) << "\r\nContinue...\r\n";
        }
        bCheckMessageSize = false;
      }
    }

    if (bCheckMessageSize) {
      if (*curli == lin.size()) {
        // we're inserting a new line at the end.
        lin.emplace_back(current_line);
        // Since we added the line, let's move forward.
        (*curli)++;
      } else if (*curli < lin.size()) {
        // replacing an older line.
        lin.at(*curli).assign(current_line);
      }
      if (*curli == (maxli + 1)) {
        bout << "\r\n-= No more lines, last line lost =-\r\n/S to save\r\n\n";
        (*curli)--;
      } else if (*curli == maxli) {
        bout << "-= Message limit reached, /S to save =-\r\n";
      } else if ((*curli + 5) == maxli) {
        bout << "-= 5 lines left =-\r\n";
      }
    }
  }
  if (*curli == 0) {
    bSaveMessage = false;
  }
  return bSaveMessage;
}


void UpdateMessageBufferTheadsInfo(std::ostringstream& ss, const char *aux) {
  if (!IsEqualsIgnoreCase(aux, "email")) {
    time_t message_time = time(nullptr);
    const string s = ss.str();
    unsigned long targcrc = crc32buf(s.c_str(), s.length());
    const string msgid_buf = StringPrintf("%c0P %lX-%lX", CD, targcrc, message_time);
    ss << msgid_buf << crlf;
    if (thread) {
      thread [session()->GetNumMessagesInCurrentMessageArea() + 1 ].msg_num = static_cast< unsigned short>
          (session()->GetNumMessagesInCurrentMessageArea() + 1);
      strcpy(thread[session()->GetNumMessagesInCurrentMessageArea() + 1].message_code, &msgid_buf.c_str()[4]);
    }
    if (session()->threadID.length() > 0) {
      const string threadid_buf = StringPrintf("%c0W %s", CD, session()->threadID.c_str());
      ss << threadid_buf << crlf;
      if (thread) {
        strcpy(thread[session()->GetNumMessagesInCurrentMessageArea() + 1].parent_code, &threadid_buf.c_str()[4]);
        thread[ session()->GetNumMessagesInCurrentMessageArea() + 1 ].used = 1;
      }
    }
  }
}

static void UpdateMessageBufferInReplyToInfo(std::ostringstream& ss, const char *aux) {
  if (irt_name[0] &&
      !IsEqualsIgnoreCase(aux, "email") &&
      xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
    for (int i = 0; i < xsubs[session()->GetCurrentReadMessageArea()].num_nets; i++) {
      xtrasubsnetrec *xnp = &xsubs[session()->GetCurrentReadMessageArea()].nets[i];
      if (net_networks[xnp->net_num].type == net_type_fidonet) {
        const string buf = StringPrintf("0FidoAddr: %s", irt_name);
        ss << buf << crlf;
        break;
      }
    }
  }
  if ((strncasecmp("internet", session()->GetNetworkName(), 8) == 0) ||
      (strncasecmp("filenet", session()->GetNetworkName(), 7) == 0)) {
    if (session()->usenetReferencesLine.length() > 0) {
      const string buf = StringPrintf("%c0RReferences: %s", CD, session()->usenetReferencesLine.c_str());
      ss << buf << crlf;
      session()->usenetReferencesLine = "";
    }
  }
  if (irt[0] != '"') {
    const string irt_buf = StringPrintf("RE: %s", irt);
    ss << irt_buf << crlf;
    if (irt_sub[0]) {
      const string irt_sub_buf = StringPrintf("ON: %s", irt_sub);
      ss << irt_sub_buf << crlf;
    }
  } else {
    irt_sub[0] = '\0';
  }

  if (irt_name[0] &&
      !IsEqualsIgnoreCase(aux, "email")) {
    const string irt_name_buf = StringPrintf("BY: %s", irt_name);
    ss << irt_name_buf << crlf;
  }
  ss << crlf;
}

static string FindTagFileName() {
  for (int i = 0; i < xsubs[session()->GetCurrentReadMessageArea()].num_nets; i++) {
    xtrasubsnetrec *xnp = &xsubs[session()->GetCurrentReadMessageArea()].nets[i];
    const char *nd = net_networks[xnp->net_num].dir;
    string filename = StringPrintf("%s%s.tag", nd, xnp->stype);
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StrCat(nd, GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StringPrintf("%s%s.tag", syscfg.datadir, xnp->stype);
    if (File::Exists(filename)) {
      return filename;
    }
    filename = StringPrintf("%s%s", syscfg.datadir, GENERAL_TAG);
    if (File::Exists(filename)) {
      return filename;
    }
  }
  return "";
}

static void UpdateMessageBufferTagLine(std::ostringstream& ss, const char *aux) {
  if (session()->num_subs <= 0 && session()->GetCurrentReadMessageArea() <= 0) {
    return;
  }
  const char szMultiMail[] = "Multi-Mail";
  if (xsubs[session()->GetCurrentReadMessageArea()].num_nets &&
      !IsEqualsIgnoreCase(aux, "email") &&
      (!(subboards[session()->GetCurrentReadMessageArea()].anony & anony_no_tag)) &&
      !IsEqualsIgnoreCase(irt, szMultiMail)) {
		// tag is ok
  } else {
		return;
  }

  const string filename = FindTagFileName();
  if (filename.empty()) {
    // FindTagFileName returns an empty string if no tagname exists, so
    // just exit here since there is no tag.
  }
  TextFile file(filename, "rb");
  if (file.IsOpen()) {
    int j = 0;
    string s;
    while (!file.IsEndOfFile()) {
      s.clear();
      file.ReadLine(&s);
      if (s.length() > 1 && s[s.length() - 2] == RETURN) {
        // remove last 2 characters.
        s.pop_back();
        s.pop_back();
      }
      string s1 = s;
      if (s[0] != CD) {
        s1 = StringPrintf("%c%c%s", CD, j + '2', s.c_str());
      }
      if (!j) {
        ss << StringPrintf("%c1", CD) << crlf;
      }
      ss << s1 << crlf;
      if (j < 7) {
        j++;
      }
    }
    file.Close();
  }
}

static void UpdateMessageBufferQuotesCtrlLines(std::ostringstream& ss) {
  const string quotes_filename = StrCat(syscfgovr.tempdir, QUOTES_TXT);
  TextFile file(quotes_filename, "rt");
  if (file.IsOpen()) {
    string quote_text;
    while (file.ReadLine(&quote_text)) {
      string::size_type slash_n = quote_text.find('\n');
      if (slash_n != string::npos) {
        quote_text.resize(slash_n);
      }
      if (starts_with(quote_text, "\004""0U")) {
        ss << quote_text << crlf;
      }
    }
    file.Close();
  }

  const string msginf_filename = StrCat(syscfgovr.tempdir, "msginf");
  copyfile(quotes_filename, msginf_filename, false);
}

static void GetMessageAnonStatus(bool *real_name, int *anony, int setanon) {
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
      bout << "|#5Anonymous? ";
      if (yesno()) {
        *anony = anony_sender;
      } else {
        *anony = 0;
      }
    }
    break;
  case anony_enable_dear_abby: {
    bout.nl();
    bout << "1. " << session()->user()->GetUserNameAndNumber(
                         session()->usernum) << wwiv::endl;
    bout << "2. Abby\r\n";
    bout << "3. Problemed Person\r\n\n";
    bout << "|#5Which? ";
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

bool inmsg(MessageEditorData& data) {
  vector<string> lin;

  int oiia = setiia(0);
  if (data.fsed_flags != INMSG_NOFSED && !okfsed()) {
    data.fsed_flags = INMSG_NOFSED;
  }

  const string exted_filename = StringPrintf("%s%s", syscfgovr.tempdir, INPUT_MSG);
  if (data.fsed_flags) {
    data.fsed_flags = INMSG_FSED;
  }
  if (use_workspace) {
    if (!File::Exists(exted_filename)) {
      use_workspace = false;
    } else {
      data.fsed_flags = INMSG_FSED_WORKSPACE;
    }
  }

  wwiv::core::ScopeExit at_exit([=]() {
    setiia(oiia);
    charbufferpointer = 0;
    charbuffer[0] = '\0';
    grab_quotes(nullptr, nullptr);
    if (data.fsed_flags) {
      File::Remove(exted_filename);
    }
  });

  int maxli = GetMaxMessageLinesAllowed();
  bout.nl();

  if (irt_name[0] == '\0') {
    if (GetMessageToName(data.aux.c_str())) {
      bout.nl();
    }
  }

  GetMessageTitle(data);
  if (data.title.empty() && data.need_title) {
    bout << "|#6Aborted.\r\n";
    return false;
  }

  int setanon = 0;
  int curli = 0;
  bool bSaveMessage = false;
  if (data.fsed_flags == INMSG_NOFSED) {   // Use Internal Message Editor
    bSaveMessage = InternalMessageEditor(lin, maxli, &curli, &setanon, &data.title);
  } else if (data.fsed_flags == INMSG_FSED) {   // Use Full Screen Editor
    bSaveMessage = ExternalMessageEditor(maxli, &setanon, &data.title, data.to_name, data.msged_flags, data.aux);
  } else if (data.fsed_flags == INMSG_FSED_WORKSPACE) {   // "auto-send mail message"
    bSaveMessage = File::Exists(exted_filename);
    if (bSaveMessage && !data.silent_mode) {
      bout << "Reading in file...\r\n";
    }
    use_workspace = false;
  }

  if (!bSaveMessage) {
    bout << "|#6Aborted.\r\n";
    return false;
  }
  int max_message_size = 0;
  bool real_name = false;
  GetMessageAnonStatus(&real_name, &data.anonymous_flag, setanon);
  bout.backline();
  if (!data.silent_mode) {
    SpinPuts("Saving...", 2);
  }
  if (data.fsed_flags) {
    File fileExtEd(exted_filename);
    max_message_size = std::max<int>(fileExtEd.GetLength(), 30000);
  } else {
    for (int i5 = 0; i5 < curli; i5++) {
      max_message_size += lin.at(i5).length() + 2;
    }
  }
  std::ostringstream b;

  // Add author name
  if (real_name) {
    b << session()->user()->GetRealName() << crlf;
  } else if (data.silent_mode) {
    b << syscfg.sysopname << " #1" << crlf;
  } else {
    b << session()->user()->GetUserNameNumberAndSystem(session()->usernum, net_sysnum) << crlf;
  }

  // Add date to message body
  time_t lTime = time(nullptr);
  string time_string(asctime(localtime(&lTime)));
  // asctime appends a \n to the end of the string.
  StringTrimEnd(&time_string);
  b << time_string << crlf;
  UpdateMessageBufferQuotesCtrlLines(b);

  if (session()->IsMessageThreadingEnabled()) {
    UpdateMessageBufferTheadsInfo(b, data.aux.c_str());
  }
  if (irt[0]) {
    UpdateMessageBufferInReplyToInfo(b, data.aux.c_str());
  }
  if (data.fsed_flags) {
    // Read the file produced by the external editor and add it to the message.
    TextFile editor_file(exted_filename, "r");
    string line;
    while (editor_file.ReadLine(&line)) {
      b << line << crlf;
    }
  } else {
    // iterate through the lines in "lin" and append them to 'b'
    for (int i5 = 0; i5 < curli; i5++) {
      b << lin.at(i5) << crlf;
    }
  }

  if (session()->HasConfigFlag(OP_FLAGS_MSG_TAG)) {
    UpdateMessageBufferTagLine(b, data.aux.c_str());
  }

  string text = b.str();
  if (text.back() != CZ) {
    text.push_back(CZ);
  }
  data.text = std::move(text);
  return true;
}
