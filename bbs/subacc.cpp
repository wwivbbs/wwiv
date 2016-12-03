/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/subacc.h"

#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/message_file.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "bbs/vars.h"
#include "bbs/output.h"
#include "bbs/wsession.h"
#include "sdk/status.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

using std::string;
using namespace wwiv::strings;

/////////////////////////////////////////////////////////////////////////////
//
// NOTE: This file containts wwiv message base specific code and should 
// move to SDK.
//

static File fileSub;                       // File object for '.sub' file
static char subdat_fn[MAX_PATH];            // filename of .sub file

using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Needed by pack_all_subs() which should move out of here.
bool checka();


void close_sub() {
  if (fileSub.IsOpen()) {
    fileSub.Close();
  }
}

bool open_sub(bool wr) {
  close_sub();

  if (wr) {
    fileSub.SetName(subdat_fn);
    fileSub.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    if (fileSub.IsOpen()) {
      // re-read info from file, to be safe
      fileSub.Seek(0L, File::Whence::begin);
      postrec p;
      fileSub.Read(&p, sizeof(postrec));
      session()->SetNumMessagesInCurrentMessageArea(p.owneruser);
    }
  } else {
    fileSub.SetName(subdat_fn);
    fileSub.Open(File::modeReadOnly | File::modeBinary);
  }

  return fileSub.IsOpen();
}

uint32_t WWIVReadLastRead(int sub_number) {
  // open file, and create it if necessary
  postrec p{};

  File subFile(session()->config()->datadir(), 
    StrCat(session()->subs().sub(sub_number).filename, ".sub"));
  if (!subFile.Exists()) {
    bool created = subFile.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    if (!created) {
      return 0;
    }
    p.owneruser = 0;
    subFile.Write(&p, sizeof(postrec));
    return 1;
  }

  if (!subFile.Open(File::modeBinary | File::modeReadOnly)) {
    return 0;
  }
  // read in first rec, specifying # posts
  // p.owneruser contains # of posts.
  subFile.Read(&p, sizeof(postrec));

  if (p.owneruser == 0) {
    // Not sure why but iscan1 returned 1 for empty subs.
    return 1;
  }

  // read in sub date, if don't already know it
  subFile.Seek(p.owneruser * sizeof(postrec), File::Whence::begin);
  subFile.Read(&p, sizeof(postrec));
  return p.qscan;
}

// Initializes use of a sub value (session()->subs().subs()[], not session()->usub[]).  If quick, then
// don't worry about anything detailed, just grab qscan info.
bool iscan1(int sub_index) {
  postrec p{};

  // forget it if an invalid sub #
  if (sub_index < 0 || sub_index >= size_int(session()->subs().subs())) {
    return false;
  }

  // go to correct net #
  if (!session()->subs().sub(sub_index).nets.empty()) {
    set_net_num(session()->subs().sub(sub_index).nets[0].net_num);
  } else {
    set_net_num(0);
  }

  // set sub filename
  snprintf(subdat_fn, sizeof(subdat_fn), "%s%s.sub", 
      syscfg.datadir, session()->subs().sub(sub_index).filename.c_str());

  // open file, and create it if necessary
  if (!File::Exists(subdat_fn)) {
    if (!open_sub(true)) {
      return false;
    }
    p.owneruser = 0;
    fileSub.Write(&p, sizeof(postrec));
  } else if (!open_sub(false)) {
    return false;
  }

  // set sub
  session()->SetCurrentReadMessageArea(sub_index);
  session()->subchg = 0;

  // read in first rec, specifying # posts
  fileSub.Seek(0L, File::Whence::begin);
  fileSub.Read(&p, sizeof(postrec));
  session()->SetNumMessagesInCurrentMessageArea(p.owneruser);

  // We used to read in sub date, if don't already know it
  // Not callers should use WWIVReadLastRead to get it.

  // close file
  close_sub();

  // iscanned correctly
  return true;
}

// Initializes use of a sub (session()->usub[] value, not session()->subs().subs()[] value).
int iscan(int b) {
  return iscan1(session()->usub[b].subnum);
}

// Returns info for a post.
postrec *get_post(int mn) {
  if (mn < 1) {
    return nullptr;
  }
  if (mn > session()->GetNumMessagesInCurrentMessageArea()) {
    mn = session()->GetNumMessagesInCurrentMessageArea();
  }
  bool need_close = false;
  if (!fileSub.IsOpen()) {
    if (!open_sub(false)) {
      return nullptr;
    }
    need_close = true;
  }
  // read in post
  static postrec p;
  fileSub.Seek(mn * sizeof(postrec), File::Whence::begin);
  fileSub.Read(&p, sizeof(postrec));

  if (need_close) {
    close_sub();
  }
  return &p;
}

void write_post(int mn, postrec * pp) {
  if (!fileSub.IsOpen()) {
    return;
  }
  fileSub.Seek(mn * sizeof(postrec), File::Whence::begin);
  fileSub.Write(pp, sizeof(postrec));
}

void add_post(postrec * pp) {
  bool need_close = false;

  // open the sub, if necessary
  if (!fileSub.IsOpen()) {
    open_sub(true);
    need_close = true;
  }
  if (fileSub.IsOpen()) {
    // get updated info
    session()->status_manager()->RefreshStatusCache();
    fileSub.Seek(0L, File::Whence::begin);
    subfile_header_t p{};
    fileSub.Read(&p, sizeof(subfile_header_t));

    if (strncmp(p.signature, "WWIV\x1A", 5) != 0) {
      auto saved_count = p.active_message_count;
      memset(&p, 0, sizeof(subfile_header_t));
      // We don't have a modern header.
      strcpy(p.signature, "WWIV\x1A");
      p.active_message_count = saved_count;
      p.revision = 1;
      p.wwiv_version = wwiv_num_version;
      p.daten_created = static_cast<uint32_t>(time(nullptr));
    }

    // one more post
    p.active_message_count++;
    p.mod_count++;
    session()->SetNumMessagesInCurrentMessageArea(p.active_message_count);
    fileSub.Seek(0L, File::Whence::begin);
    fileSub.Write(&p, sizeof(postrec));

    // add the new post
    fileSub.Seek(session()->GetNumMessagesInCurrentMessageArea() * sizeof(postrec), File::Whence::begin);
    fileSub.Write(pp, sizeof(postrec));

    // we've modified the sub
    session()->subchg = 0;
  }
  if (need_close) {
    close_sub();
  }
}

static constexpr size_t BUFSIZE = 32000;

void delete_message(int mn) {
  bool need_close = false;

  // open file, if needed
  if (!fileSub.IsOpen()) {
    open_sub(true);
    need_close = true;
  }
  // see if anything changed
  session()->status_manager()->RefreshStatusCache();

  if (fileSub.IsOpen()) {
    if (mn > 0 && mn <= session()->GetNumMessagesInCurrentMessageArea()) {
      char *pBuffer = static_cast<char *>(malloc(BUFSIZE));
      if (pBuffer) {
        postrec *p1 = get_post(mn);
        remove_link(&(p1->msg), session()->current_sub().filename);

        long cp = static_cast<long>(mn + 1) * sizeof(postrec);
        long len = static_cast<long>(session()->GetNumMessagesInCurrentMessageArea() + 1) * sizeof(postrec);

        unsigned int nb = 0;
        do {
          long l = len - cp;
          nb = (l < BUFSIZE) ? static_cast<int>(l) : BUFSIZE;
          if (nb) {
            fileSub.Seek(cp, File::Whence::begin);
            fileSub.Read(pBuffer, nb);
            fileSub.Seek(cp - sizeof(postrec), File::Whence::begin);
            fileSub.Write(pBuffer, nb);
            cp += nb;
          }
        } while (nb == BUFSIZE);

        // update # msgs
        postrec p;
        fileSub.Seek(0L, File::Whence::begin);
        fileSub.Read(&p, sizeof(postrec));
        p.owneruser--;
        session()->SetNumMessagesInCurrentMessageArea(p.owneruser);
        fileSub.Seek(0L, File::Whence::begin);
        fileSub.Write(&p, sizeof(postrec));
        free(pBuffer);
      }
    }
  }
  // close file, if needed
  if (need_close) {
    close_sub();
  }
}

static bool IsSamePost(postrec * p1, postrec * p2) {
  return p1 &&
    p2 &&
    p1->daten == p2->daten &&
    p1->qscan == p2->qscan &&
    p1->ownersys == p2->ownersys &&
    p1->owneruser == p2->owneruser &&
    wwiv::strings::IsEquals(p1->title, p2->title);
}

void resynch(int *msgnum, postrec * pp) {
  postrec p, *pp1;
  if (pp) {
    p = *pp;
  } else {
    pp1 = get_post(*msgnum);
    if (!pp1) {
      return;
    }
    p = *pp1;
  }

  session()->status_manager()->RefreshStatusCache();

  if (session()->subchg || pp) {
    pp1 = get_post(*msgnum);
    if (IsSamePost(pp1, &p)) {
      return;
    } else if (!pp1 || (p.qscan < pp1->qscan)) {
      if (*msgnum > session()->GetNumMessagesInCurrentMessageArea()) {
        *msgnum = session()->GetNumMessagesInCurrentMessageArea() + 1;
      }
      for (int i = *msgnum - 1; i > 0; i--) {
        pp1 = get_post(i);
        if (IsSamePost(&p, pp1) || (p.qscan >= pp1->qscan)) {
          *msgnum = i;
          return;
        }
      }
      *msgnum = 0;
    } else {
      for (int i = *msgnum + 1; i <= session()->GetNumMessagesInCurrentMessageArea(); i++) {
        pp1 = get_post(i);
        if (IsSamePost(&p, pp1) || (p.qscan <= pp1->qscan)) {
          *msgnum = i;
          return;
        }
      }
      *msgnum = session()->GetNumMessagesInCurrentMessageArea();
    }
  }
}

void pack_sub(int si) {
  if (!iscan1(si)) {
    return;
  }
  if (open_sub(true) && session()->subs().sub(si).storage_type == 2) {
    string sfn = session()->subs().sub(si).filename;
    string nfn = "PACKTMP$";

    const string fn1 = StrCat(session()->config()->msgsdir(), sfn, ".dat");
    const string fn2 = StrCat(session()->config()->msgsdir(), nfn, ".dat");

    bout << "\r\n|#7\xFE |#1Packing Message Subboard: |#5" 
         << session()->subs().sub(si).name << wwiv::endl;

    for (int i = 1; i <= session()->GetNumMessagesInCurrentMessageArea(); i++) {
      if (i % 10 == 0) {
        bout << i << "/" 
             << session()->GetNumMessagesInCurrentMessageArea()
             << "\r";
      }
      postrec *p = get_post(i);
      if (p) {
        std::string text;
        if (!readfile(&(p->msg), sfn, &text)) {
          text = "??";
        }
        savefile(text, &(p->msg), nfn);
        write_post(i, p);
      }
      bout << i << "/" 
           << session()->GetNumMessagesInCurrentMessageArea()
           << "\r";
    }

    File::Remove(fn1);
    File::Rename(fn2, fn1);

    close_sub();
    bout << "|#7\xFE |#1Done Packing " 
         << session()->GetNumMessagesInCurrentMessageArea() 
         << " messages.                              \r\n";
  }
}

bool pack_all_subs() {
  for (size_t i=0; i < session()->subs().subs().size() && !hangup; i++) {
    pack_sub(i);
    if (checka() == true) {
      // checka checks to see if abort is set.
      return false;
    }
  }
  return true;
}
