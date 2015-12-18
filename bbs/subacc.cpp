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
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/message_file.h"
#include "bbs/msgbase.h"
#include "bbs/subxtr.h"
#include "sdk/vardec.h"
#include "bbs/vars.h"
#include "bbs/woutstreambuffer.h"
#include "bbs/wsession.h"
#include "bbs/wstatus.h"
#include "core/file.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

/////////////////////////////////////////////////////////////////////////////
//
// NOTE: This file containts wwiv message base specific code and should 
// move to SDK.
//

static File fileSub;                       // File object for '.sub' file
static char subdat_fn[MAX_PATH];            // filename of .sub file

using std::unique_ptr;
using wwiv::strings::StrCat;
using wwiv::core::ScopeExit;

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
      fileSub.Seek(0L, File::seekBegin);
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
  postrec p;
  memset(&p, 0, sizeof(postrec));

  File subFile(syscfg.datadir, StrCat(session()->subboards[sub_number].filename, ".sub"));
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
  subFile.Seek(p.owneruser * sizeof(postrec), File::seekBegin);
  subFile.Read(&p, sizeof(postrec));
  return p.qscan;
}

// Initializes use of a sub value (session()->subboards[], not usub[]).  If quick, then
// don't worry about anything detailed, just grab qscan info.
bool iscan1(int sub_index) {
  postrec p;

  // forget it if an invalid sub #
  if (sub_index < 0 || sub_index >= session()->num_subs) {
    return false;
  }

  // go to correct net #
  if (!session()->xsubs[sub_index].nets.empty()) {
    set_net_num(session()->xsubs[sub_index].nets[0].net_num);
  } else {
    set_net_num(0);
  }

  // set sub filename
  snprintf(subdat_fn, sizeof(subdat_fn), "%s%s.sub", 
      syscfg.datadir, session()->subboards[sub_index].filename);

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
  fileSub.Seek(0L, File::seekBegin);
  fileSub.Read(&p, sizeof(postrec));
  session()->SetNumMessagesInCurrentMessageArea(p.owneruser);

  // We used to read in sub date, if don't already know it
  // Not callers should use WWIVReadLastRead to get it.

  // close file
  close_sub();

  // iscanned correctly
  return true;
}

// Initializes use of a sub (usub[] value, not session()->subboards[] value).
int iscan(int b) {
  return iscan1(usub[b].subnum);
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
  fileSub.Seek(mn * sizeof(postrec), File::seekBegin);
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
  fileSub.Seek(mn * sizeof(postrec), File::seekBegin);
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
    session()->GetStatusManager()->RefreshStatusCache();
    fileSub.Seek(0L, File::seekBegin);
    postrec p;
    fileSub.Read(&p, sizeof(postrec));

    // one more post
    p.owneruser++;
    session()->SetNumMessagesInCurrentMessageArea(p.owneruser);
    fileSub.Seek(0L, File::seekBegin);
    fileSub.Write(&p, sizeof(postrec));

    // add the new post
    fileSub.Seek(session()->GetNumMessagesInCurrentMessageArea() * sizeof(postrec), File::seekBegin);
    fileSub.Write(pp, sizeof(postrec));

    // we've modified the sub
    session()->subchg = 0;
  }
  if (need_close) {
    close_sub();
  }
}

#define BUFSIZE 32000

void delete_message(int mn) {
  bool need_close = false;

  // open file, if needed
  if (!fileSub.IsOpen()) {
    open_sub(true);
    need_close = true;
  }
  // see if anything changed
  session()->GetStatusManager()->RefreshStatusCache();

  if (fileSub.IsOpen()) {
    if (mn > 0 && mn <= session()->GetNumMessagesInCurrentMessageArea()) {
      char *pBuffer = static_cast<char *>(malloc(BUFSIZE));
      if (pBuffer) {
        postrec *p1 = get_post(mn);
        remove_link(&(p1->msg), session()->subboards[session()->GetCurrentReadMessageArea()].filename);

        long cp = static_cast<long>(mn + 1) * sizeof(postrec);
        long len = static_cast<long>(session()->GetNumMessagesInCurrentMessageArea() + 1) * sizeof(postrec);

        unsigned int nb = 0;
        do {
          long l = len - cp;
          nb = (l < BUFSIZE) ? static_cast<int>(l) : BUFSIZE;
          if (nb) {
            fileSub.Seek(cp, File::seekBegin);
            fileSub.Read(pBuffer, nb);
            fileSub.Seek(cp - sizeof(postrec), File::seekBegin);
            fileSub.Write(pBuffer, nb);
            cp += nb;
          }
        } while (nb == BUFSIZE);

        // update # msgs
        postrec p;
        fileSub.Seek(0L, File::seekBegin);
        fileSub.Read(&p, sizeof(postrec));
        p.owneruser--;
        session()->SetNumMessagesInCurrentMessageArea(p.owneruser);
        fileSub.Seek(0L, File::seekBegin);
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

  session()->GetStatusManager()->RefreshStatusCache();

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
  if (open_sub(true) && session()->subboards[si].storage_type == 2) {
    const char *sfn = session()->subboards[si].filename;
    const char *nfn = "PACKTMP$";

    char fn1[MAX_PATH], fn2[MAX_PATH];
    sprintf(fn1, "%s%s.dat", syscfg.msgsdir, sfn);
    sprintf(fn2, "%s%s.dat", syscfg.msgsdir, nfn);

    bout << "\r\n|#7\xFE |#1Packing Message Area: |#5" 
         << session()->subboards[si].name << wwiv::endl;

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
  for (int i=0; i < session()->num_subs && !hangup; i++) {
    pack_sub(i);
    if (!checka()) {
      return false;
    }
  }
  return true;
}
