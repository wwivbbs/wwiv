/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/message_file.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "sdk/config.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"

#include <memory>
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

/////////////////////////////////////////////////////////////////////////////
//
// NOTE: This file containts wwiv message base specific code and should
// move to SDK.
//

static std::unique_ptr<File> fileSub; // File object for '.sub' file
static char subdat_fn[MAX_PATH];      // filename of .sub file

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

void close_sub() {
  if (fileSub) {
    fileSub.reset();
  }
}

bool open_sub(bool wr) {
  fileSub.reset(new File(subdat_fn));
  if (wr) {
    fileSub->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    if (fileSub->IsOpen()) {
      // re-read info from file, to be safe
      fileSub->Seek(0L, File::Whence::begin);
      postrec p{};
      fileSub->Read(&p, sizeof(postrec));
      a()->SetNumMessagesInCurrentMessageArea(p.owneruser);
    }
  } else {
    fileSub->Open(File::modeReadOnly | File::modeBinary);
  }

  return fileSub->IsOpen();
}

uint32_t WWIVReadLastRead(int sub_number) {
  // open file, and create it if necessary
  postrec p{};

  const auto fn =
      FilePath(a()->config()->datadir(), StrCat(a()->subs().sub(sub_number).filename, ".sub"));
  if (!File::Exists(fn)) {
    File subFile(fn);
    if (!subFile.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      return 0;
    }
    p.owneruser = 0;
    subFile.Write(&p, sizeof(postrec));
    return 1;
  }

  File subFile(fn);
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

// Initializes use of a sub value (a()->subs().subs()[], not a()->usub[]).  If quick, then
// don't worry about anything detailed, just grab qscan info.
bool iscan1(int sub_index) {
  postrec p{};

  // forget it if an invalid sub #
  if (sub_index < 0 || sub_index >= size_int(a()->subs().subs())) {
    return false;
  }

  // go to correct net #
  if (!a()->subs().sub(sub_index).nets.empty()) {
    set_net_num(a()->subs().sub(sub_index).nets[0].net_num);
  } else {
    set_net_num(0);
  }

  // set sub filename
  const auto subfn = StrCat(a()->subs().sub(sub_index).filename, ".sub");
  to_char_array(subdat_fn, FilePath(a()->config()->datadir(), subfn).string());

  // open file, and create it if necessary
  if (!File::Exists(subdat_fn)) {
    if (!open_sub(true)) {
      return false;
    }
    p.owneruser = 0;
    fileSub->Write(&p, sizeof(postrec));
  } else if (!open_sub(false)) {
    return false;
  }

  // set sub
  a()->sess().SetCurrentReadMessageArea(sub_index);
  a()->subchg = 0;

  // read in first rec, specifying # posts
  fileSub->Seek(0L, File::Whence::begin);
  fileSub->Read(&p, sizeof(postrec));
  a()->SetNumMessagesInCurrentMessageArea(p.owneruser);

  // We used to read in sub date, if don't already know it
  // Not callers should use WWIVReadLastRead to get it.

  // close file
  close_sub();

  // iscanned correctly
  return true;
}

// Initializes use of a sub (a()->usub[] value, not a()->subs().subs()[] value).
int iscan(int b) { return iscan1(a()->usub[b].subnum); }

// Returns info for a post.
postrec* get_post(int mn) {
  if (mn < 1) {
    return nullptr;
  }
  if (mn > a()->GetNumMessagesInCurrentMessageArea()) {
    mn = a()->GetNumMessagesInCurrentMessageArea();
  }
  auto need_close = false;
  if (!fileSub) {
    if (!open_sub(false)) {
      return nullptr;
    }
    need_close = true;
  }
  // read in post
  static postrec p;
  fileSub->Seek(mn * sizeof(postrec), File::Whence::begin);
  fileSub->Read(&p, sizeof(postrec));

  if (need_close) {
    close_sub();
  }
  return &p;
}

void write_post(int mn, postrec* pp) {
  if (!fileSub || !fileSub->IsOpen()) {
    return;
  }
  fileSub->Seek(mn * sizeof(postrec), File::Whence::begin);
  fileSub->Write(pp, sizeof(postrec));
}

void add_post(postrec* pp) {
  bool need_close = false;

  // open the sub, if necessary
  if (!fileSub) {
    open_sub(true);
    need_close = true;
  }
  if (!fileSub || !fileSub->IsOpen()) {
    return;
  }
  // get updated info
  a()->status_manager()->reload_status();
  fileSub->Seek(0L, File::Whence::begin);
  subfile_header_t p{};
  fileSub->Read(&p, sizeof(subfile_header_t));

  if (strncmp(p.signature, "WWIV\x1A", 5) != 0) {
    const auto saved_count = p.active_message_count;
    memset(&p, 0, sizeof(subfile_header_t));
    // We don't have a modern header.
    strcpy(p.signature, "WWIV\x1A");
    p.active_message_count = saved_count;
    p.revision = 1;
    p.wwiv_version = wwiv_config_version();
    p.daten_created = DateTime::now().to_daten_t();
  }

  // one more post
  p.active_message_count++;
  p.mod_count++;
  a()->SetNumMessagesInCurrentMessageArea(p.active_message_count);
  fileSub->Seek(0L, File::Whence::begin);
  fileSub->Write(&p, sizeof(postrec));

  // add the new post
  fileSub->Seek(a()->GetNumMessagesInCurrentMessageArea() * sizeof(postrec), File::Whence::begin);
  fileSub->Write(pp, sizeof(postrec));

  // we've modified the sub
  a()->subchg = 0;

  if (need_close) {
    close_sub();
  }
}

static constexpr size_t BUFSIZE = 32000;

void delete_message(int mn) {
  bool need_close = false;

  // open file, if needed
  if (!fileSub || !fileSub->IsOpen()) {
    open_sub(true);
    need_close = true;
  }
  // see if anything changed
  a()->status_manager()->reload_status();

  if (fileSub) {
    if (mn > 0 && mn <= a()->GetNumMessagesInCurrentMessageArea()) {
      if (auto* buffer = static_cast<char*>(malloc(BUFSIZE))) {
        const auto* p1 = get_post(mn);
        remove_link(&(p1->msg), a()->current_sub().filename);

        auto cp = static_cast<long>(mn + 1) * sizeof(postrec);
        const auto len = static_cast<long>(a()->GetNumMessagesInCurrentMessageArea() + 1) * sizeof(postrec);

        unsigned int nb;
        do {
          const auto l = len - cp;
          nb = l < BUFSIZE ? static_cast<int>(l) : BUFSIZE;
          if (nb) {
            fileSub->Seek(cp, File::Whence::begin);
            fileSub->Read(buffer, nb);
            fileSub->Seek(cp - sizeof(postrec), File::Whence::begin);
            fileSub->Write(buffer, nb);
            cp += nb;
          }
        } while (nb == BUFSIZE);

        // update # msgs
        postrec p{};
        fileSub->Seek(0L, File::Whence::begin);
        fileSub->Read(&p, sizeof(postrec));
        p.owneruser--;
        a()->SetNumMessagesInCurrentMessageArea(p.owneruser);
        fileSub->Seek(0L, File::Whence::begin);
        fileSub->Write(&p, sizeof(postrec));
        free(buffer);
      }
    }
  }
  // close file, if needed
  if (need_close) {
    close_sub();
  }
}

static bool IsSamePost(postrec* p1, postrec* p2) {
  return p1 && p2 && p1->daten == p2->daten && p1->qscan == p2->qscan &&
         p1->ownersys == p2->ownersys && p1->owneruser == p2->owneruser &&
         IsEquals(p1->title, p2->title);
}

void resynch(int* msgnum, postrec* pp) {
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

  a()->status_manager()->reload_status();

  if (a()->subchg || pp) {
    pp1 = get_post(*msgnum);
    if (IsSamePost(pp1, &p)) {
      return;
    } else if (!pp1 || (p.qscan < pp1->qscan)) {
      if (*msgnum > a()->GetNumMessagesInCurrentMessageArea()) {
        *msgnum = a()->GetNumMessagesInCurrentMessageArea() + 1;
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
      for (int i = *msgnum + 1; i <= a()->GetNumMessagesInCurrentMessageArea(); i++) {
        pp1 = get_post(i);
        if (IsSamePost(&p, pp1) || (p.qscan <= pp1->qscan)) {
          *msgnum = i;
          return;
        }
      }
      *msgnum = a()->GetNumMessagesInCurrentMessageArea();
    }
  }
}
