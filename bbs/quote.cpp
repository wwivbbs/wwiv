/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <memory>
#include <string>
#include <vector>

#include "bbs/bbsutl.h"
#include "bbs/input.h"
#include "local_io/keycodes.h"
#include "bbs/printfile.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/message_file.h"
#include "bbs/pause.h"

#include "bbs/utility.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/datetime.h"
#include "sdk/filenames.h"


#define LINELEN 79
#define PFXCOL 2
#define QUOTECOL 0

#define WRTPFX {file.WriteFormatted("\x3%c",PFXCOL+48);if (tf==1)\
                cp=file.WriteBinary(pfx.c_str(),pfx.size()-1);\
                else cp=file.WriteBinary(pfx.c_str(),pfx.size());\
                file.WriteFormatted("\x3%c",cc);}
#define NL {if (!cp) {file.WriteFormatted("\x3%c",PFXCOL+48);\
            file.WriteBinary(pfx.c_str(),pfx.size());} if (ctlc) file.WriteBinary("0",1);\
            file.WriteBinary("\r\n",2);cp=ns=ctlc=0;}
#define FLSH {if (ss1) {if (cp && (l3+cp>=linelen)) NL else if (ns)\
              cp+=file.WriteBinary(" ",1);if (!cp) {if (ctld)\
              file.WriteFormatted("\x4%c",ctld); WRTPFX; } file.WriteBinary(ss1,l2);\
              cp+=l3;ss1=nullptr;l2=l3=0;ns=1;}}

static int quotes_nrm_l = 0;
static int quotes_ind_l = 0;

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static string FirstLettersOfVectorAsString(const vector<string>& parts) {
  string result;
  for (const auto& part : parts) {
    result.push_back(part.front());
  }
  return result;
}

string GetQuoteInitials(const string& orig_name) {
  if (orig_name.empty()) {
    return {};
  }
  auto name = orig_name;
  if (starts_with(name, "``")) { 
    name = name.substr(2);
  }

  auto paren_start = name.find('(');
  if (paren_start != name.npos && !isdigit(name.at(paren_start + 1))) {
    auto inner = name.substr(paren_start + 1);
    return GetQuoteInitials(inner);
  }

  const auto last = name.find_first_of("#<>()[]`");
  vector<string> parts = (last != name.npos) ? 
      SplitString(name.substr(0, last), " ") : SplitString(name, " ");
  return FirstLettersOfVectorAsString(parts);
}


void clear_quotes() {
  auto quotes_txt_fn = FilePath(a()->temp_directory(), QUOTES_TXT);
  auto quotes_ind_fn = FilePath(a()->temp_directory(), QUOTES_IND);

  File::SetFilePermissions(quotes_txt_fn, File::permReadWrite);
  File::Remove(quotes_txt_fn);
  File::SetFilePermissions(quotes_ind_fn, File::permReadWrite);
  File::Remove(quotes_ind_fn);

  if (bout.quotes_ind) {
    free(bout.quotes_ind);
  }

  bout.quotes_ind = nullptr;
  quotes_nrm_l = quotes_ind_l = 0;
}

void grab_quotes(messagerec* m, const std::string& message_filename, const std::string& to_name) {
  WWIV_ASSERT(m);

  char temp[255];
  long l2, l3;
  int cp = 0, ctla = 0, ctlc = 0, ns = 0, ctld = 0;
  char cc = QUOTECOL + 48;
  int linelen = LINELEN, tf = 0;

  clear_quotes();

  auto quotes_txt_fn = FilePath(a()->temp_directory(), QUOTES_TXT);
  auto quotes_ind_fn = FilePath(a()->temp_directory(), QUOTES_IND);

  auto pfx = StrCat(GetQuoteInitials(to_name), "> ");

  string ss;
  if (!readfile(m, message_filename, &ss)) {
    return;
  }
  quotes_nrm_l = ss.length();
  if (ss.back() == CZ) {
    // Since CZ isn't special on Win32/Linux. Don't write it out
    // to the quotea file.
    ss.pop_back();
  }

  File quotesTextFile(quotes_txt_fn);
  if (quotesTextFile.Open(File::modeDefault | File::modeCreateFile | File::modeTruncate, File::shareDenyNone)) {
    quotesTextFile.Write(ss);
    quotesTextFile.Close();
  }
  TextFile file(quotes_ind_fn, "wb");
  if (!file.IsOpen()) {
    return;
  }

  l3 = l2 = 0;
  char* ss1 = nullptr;
  a()->internetFullEmailAddress = "";
  if (a()->current_net().type == network_type_t::internet ||
      a()->current_net().type == network_type_t::news) {
    for (size_t l1 = 0; l1 < ss.length(); l1++) {
      if ((ss[l1] == 4) && (ss[l1 + 1] == '0') && (ss[l1 + 2] == 'R') &&
          (ss[l1 + 3] == 'M')) {
        l1 += 3;
        while ((ss[l1] != '\r') && (l1 < ss.length())) {
          temp[l3++] = ss[l1];
          l1++;
        }
        temp[l3] = 0;
        if (strncasecmp(temp, "Message-ID", 10) == 0) {
          if (temp[0] != 0) {
            ss1 = strtok(temp, ":");
            if (ss1) {
              ss1 = strtok(nullptr, "\r\n");
            }
            if (ss1) {
              a()->usenetReferencesLine = ss1;
            }
          }
        }
        l1 = ss.length();
      }
    }
  }
  l3 = l2 = 0;
  ss1 = nullptr;
  for (size_t l1 = 0; l1 < ss.length(); l1++) {
    if (ctld == -1) {
      ctld = ss[l1];
    } else switch (ss[l1]) {
      case 1:
        ctla = 1;
        break;
      case 2:
        break;
      case 3:
        if (!ss1) {
          ss1 = &ss[0] + l1;
        }
        l2++;
        ctlc = 1;
        break;
      case 4:
        ctld = -1;
        break;
      case '\n':
        tf = 0;
        if (ctla) {
          ctla = 0;
        } else {
          cc = QUOTECOL + 48;
          FLSH;
          ctld = 0;
          NL;
        }
        break;
      case ' ':
      case '\r':
        if (ss1) {
          FLSH;
        } else {
          if (ss[l1] == ' ') {
            if (cp + 1 >= linelen) {
              NL;
            }
            if (!cp) {
              if (ctld) {
                file.WriteFormatted("\x04%c", ctld);
              }
              WRTPFX;
            }
            cp++;
            file.WriteBinary(" ", 1);
          }
        }
        break;
      default:
        if (!ss1) {
          ss1 = &ss[0] + l1;
        }
        l2++;
        if (ctlc) {
          if (ss[l1] == 48) {
            ss[l1] = QUOTECOL + 48;
          }
          cc = ss[l1];
          ctlc = 0;
        } else {
          l3++;
          if (!tf) {
            if (ss[l1] == '>') {
              tf = 1;
              linelen = LINELEN;
            } else {
              tf = 2;
              linelen = LINELEN - 5;
            }
          }
        }
        break;
      }
  }
  FLSH;
  if (cp) {
    file.WriteBinary("\r\n", 2);
  }
  file.Close();

  File ff(quotes_ind_fn);
  if (ff.Open(File::modeBinary | File::modeReadOnly)) {
    quotes_ind_l = ff.length();
    bout.quotes_ind = static_cast<char*>(BbsAllocA(quotes_ind_l));
    if (bout.quotes_ind) {
      ff.Read(bout.quotes_ind, quotes_ind_l);
    } else {
      quotes_ind_l = 0;
    }
    ff.Close();
  }
}

static string CreateDateString(time_t t) {
  auto dt = DateTime::from_time_t(t);
  std::ostringstream ss;
  ss << dt.to_string("%A,%B %d, %Y") << " at ";
  if (a()->user()->IsUse24HourClock()) {
    ss << dt.to_string("%H:%M");
  }
  else {
    ss << dt.to_string("%I:%M %p");
  }
  return ss.str();
}

void auto_quote(char *org, const std::string& to_name, long len, int type, time_t tDateTime) {
  char s1[81], s2[81], buf[255], *p = org, *b = org, b1[81];

  File fileInputMsg(FilePath(a()->temp_directory(), INPUT_MSG));
  fileInputMsg.Delete();
  if (!a()->hangup_) {
    fileInputMsg.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    fileInputMsg.Seek(0L, File::Whence::end);
    while (*p != '\r') {
      ++p;
    }
    *p = '\0';
    strcpy(s1, b);
    p += 2;
    len = len - (p - b);
    b = p;
    while (*p != '\r') {
      ++p;
    }
    p += 2;
    len = len - (p - b);
    b = p;
    const auto datetime = CreateDateString(tDateTime);
    to_char_array(s2, datetime);

    //    s2[strlen(s2)-1]='\0';
    auto tb = properize(strip_to_node(s1));
    auto tb1 = GetQuoteInitials(to_name);
    switch (type) {
    case 1:
      sprintf(buf, "\003""3On \003""1%s, \003""2%s\003""3 wrote:\003""0", s2, tb.c_str());
      break;
    case 2:
      sprintf(buf, "\003""3In your e-mail of \003""2%s\003""3, you wrote:\003""0", s2);
      break;
    case 3:
      sprintf(buf, "\003""3In a message posted \003""2%s\003""3, you wrote:\003""0", s2);
      break;
    case 4:
      sprintf(buf, "\003""3Message forwarded from \003""2%s\003""3, sent on %s.\003""0",
              tb.c_str(), s2);
      break;
    }
    strcat(buf, "\r\n");
    fileInputMsg.Writeln(buf, strlen(buf));
    while (len > 0) {
      while ((strchr("\r\001", *p) == nullptr) && ((p - b) < (len < 253 ? len : 253))) {
        ++p;
      }
      if (*p == '\001') {
        *(p++) = '\0';
      }
      *p = '\0';
      if (*b != '\004' && strchr(b, '\033') == nullptr) {
        int jj = 0;
        for (int j = 0; j < static_cast<int>(77 - tb1.length()); j++) {
          if (((b[j] == '0') && (b[j - 1] != '\003')) || (b[j] != '0')) {
            b1[jj] = b[j];
          } else {
            b1[jj] = '5';
          }
          b1[jj + 1] = 0;
          jj++;
        }
        sprintf(buf, "\003""1%s\003""7>\003""5%s\003""0", tb1.c_str(), b1);
        fileInputMsg.Writeln(buf, strlen(buf));
      }
      p += 2;
      len = len - (p - b);
      b = p;
    }
    fileInputMsg.Close();
    if (a()->user()->GetNumMessagesPosted() < 10) {
      printfile(QUOTE_NOEXT);
    }
  }
}

void get_quote(const string& reply_to_name) {
  static char s[141], s1[10];
  static int i, i1, i2, i3, rl;
  static int l1, l2;

  if (bout.quotes_ind == nullptr) {
    bout.nl();
    bout << "Not replying to a message!  Nothing to quote!\r\n\n";
    return;
  }
  rl = 1;
  do {
    if (rl) {
      i = 1;
      l1 = l2 = 0;
      i1 = i2 = 0;
      bool abort = false;
      bool next = false;
      do {
        if (bout.quotes_ind[l2++] == 10) {
          l1++;
        }
      } while ((l2 < quotes_ind_l) && (l1 < 2));
      do {
        if (bout.quotes_ind[l2] == 0x04) {
          while ((bout.quotes_ind[l2++] != 10) && (l2 < quotes_ind_l)) {
          }
        } else {
          if (!reply_to_name.empty()) {
            s[0] = 32;
            i3 = 1;
          } else {
            i3 = 0;
          }
          if (abort) {
            do {
              l2++;
            } while (bout.quotes_ind[l2] != RETURN && l2 < quotes_ind_l);
          } else {
            do {
              s[i3++] = bout.quotes_ind[l2++];
            } while (bout.quotes_ind[l2] != RETURN && l2 < quotes_ind_l);
          }
          if (bout.quotes_ind[l2]) {
            l2 += 2;
            s[i3] = 0;
          }
          sprintf(s1, "%3d", i++);
          bout.bputs(s1, &abort, &next);
          bout.bpla(s, &abort);
        }
      } while (l2 < quotes_ind_l);
      --i;
    }
    bout.nl();

    if (!i1 && !a()->hangup_) {
      do {
        sprintf(s, "Quote from line 1-%d? (?=relist, Q=quit) ", i);
        bout << "|#2" << s;
        input(s, 3);
      } while (!s[0] && !a()->hangup_);
      if (s[0] == 'Q') {
        rl = 0;
      } else if (s[0] != '?') {
        i1 = to_number<int>(s);
        if (i1 >= i) {
          i2 = i1 = i;
        }
        if (i1 < 1) {
          i1 = 1;
        }
      }
    }

    if (i1 && !i2 && !a()->hangup_) {
      do {
        bout << "|#2through line " << i1 << "-" << i << "? (Q=quit) ";
        input(s, 3);
      } while (!s[0] && !a()->hangup_);
      if (s[0] == 'Q') {
        rl = 0;
      } else if (s[0] != '?') {
        i2 = to_number<int>(s);
        if (i2 > i) {
          i2 = i;
        }
        if (i2 < i1) {
          i2 = i1;
        }
      }
    }
    if (i2 && rl && !a()->hangup_) {
      if (i1 == i2) {
        bout << "|#5Quote line " << i1 << "? ";
      } else {
        bout << "|#5Quote lines " << i1 << "-" << i2 << "? ";
      }
      if (!noyes()) {
        i1 = 0;
        i2 = 0;
      }
    }
  } while (!a()->hangup_ && rl && !i2);
  bout.charbufferpointer_ = 0;
  if (i1 > 0 && i2 >= i1 && i2 <= i && rl && !a()->hangup_) {
    a()->bquote_ = i1;
    a()->equote_ = i2;
  }
}
