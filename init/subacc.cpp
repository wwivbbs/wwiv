/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include <cstdlib>
#include "init/wwivinit.h"
#include "core/wfile.h"

#ifndef MAX_TO_CACHE
#define MAX_TO_CACHE 15                     // max postrecs to hold in cache
#endif

#ifdef _WIN32
#define snprintf _snprintf
#endif  // _WIN32

static postrec *cache;                      // points to sub cache memory
static bool believe_cache;                  // true if cache is valid
static int cache_start;                     // starting msgnum of cache
static int last_msgnum;                     // last msgnum read
static WFile fileSub;           // WFile object for '.sub' file
static char subdat_fn[MAX_PATH];            // filename of .sub file


// locals
static int m_nCurrentReadMessageArea, m_nNumMsgsInCurrentSub, subchg;
int  GetCurrentReadMessageArea() {
  return m_nCurrentReadMessageArea;
}
void SetCurrentReadMessageArea(int n) {
  m_nCurrentReadMessageArea = n;
}

int  GetNumMessagesInCurrentMessageArea() {
  return m_nNumMsgsInCurrentSub;
}
void SetNumMessagesInCurrentMessageArea(int n) {
  m_nNumMsgsInCurrentSub = n;
}

void close_sub() {
  if (fileSub.IsOpen()) {
    fileSub.Close();
  }
}

bool open_sub(bool wr) {
  postrec p;

  close_sub();

  if (wr) {
    fileSub.SetName(subdat_fn);
    fileSub.Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite);

    if (fileSub.IsOpen()) {
      // re-read info from file, to be safe
      believe_cache = false;
      fileSub.Seek(0L, WFile::seekBegin);
      fileSub.Read(&p, sizeof(postrec));
      SetNumMessagesInCurrentMessageArea(p.owneruser);
    }
  } else {
    fileSub.SetName(subdat_fn);
    fileSub.Open(WFile::modeReadOnly | WFile::modeBinary);
  }

  return fileSub.IsOpen();
}

bool iscan1(int si)
// Initializes use of a sub value (subboards[], not usub[]).  If quick, then
// don't worry about anything detailed, just grab qscan info.
{
  postrec p;

  // make sure we have cache space
  if (!cache) {
    cache = static_cast<postrec *>(malloc(MAX_TO_CACHE * sizeof(postrec)));
    if (!cache) {
      return false;
    }
  }
  // forget it if an invalid sub #
  if (si < 0 || si >= initinfo.num_subs) {
    return false;
  }

  // see if a sub has changed
  if (subchg) {
    SetCurrentReadMessageArea(-1);
  }

  // if already have this one set, nothing more to do
  if (si == GetCurrentReadMessageArea()) {
    return true;
  }
  // sub cache no longer valid
  believe_cache = false;

  // set sub filename
  snprintf(subdat_fn, sizeof(subdat_fn), "%s%s.sub", syscfg.datadir, subboards[si].filename);

  // open file, and create it if necessary
  if (!WFile::Exists(subdat_fn)) {
    if (!open_sub(true)) {
      return false;
    }
    p.owneruser = 0;
    fileSub.Write(&p, sizeof(postrec));
  } else if (!open_sub(false)) {
    return false;
  }

  // set sub
  SetCurrentReadMessageArea(si);
  subchg = 0;
  last_msgnum = 0;

  // read in first rec, specifying # posts
  fileSub.Seek(0L, WFile::seekBegin);
  fileSub.Read(&p, sizeof(postrec));
  SetNumMessagesInCurrentMessageArea(p.owneruser);

  // close file
  close_sub();

  // iscanned correctly
  return true;
}

postrec *get_post(int mn)
// Returns info for a post.  Maintains a cache.  Does not correct anything
// if the sub has changed.
{
  postrec p;
  bool bCloseSubFile = false;

  if (mn == 0) {
    return nullptr;
  }

  if (subchg == 1) {
    // sub has changed (detected in GetApplication()->GetStatusManager()->Read); invalidate cache
    believe_cache = false;

    // kludge: subch==2 leaves subch indicating change, but the '2' value
    // indicates, to this routine, that it has been handled at this level
    subchg = 2;
  }
  // see if we need new cache info
  if (!believe_cache ||
      mn < cache_start ||
      mn >= (cache_start + MAX_TO_CACHE)) {
    if (!fileSub.IsOpen()) {
      // open the sub data file, if necessary
      if (!open_sub(false)) {
        return nullptr;
      }
      bCloseSubFile = true;
    }

    // re-read # msgs, if needed
    if (subchg == 2) {
      fileSub.Seek(0L, WFile::seekBegin);
      fileSub.Read(&p, sizeof(postrec));
      SetNumMessagesInCurrentMessageArea(p.owneruser);

      // another kludge: subch==3 indicates we have re-read # msgs also
      // only used so we don't do this every time through
      subchg = 3;

      // adjust msgnum, if it is no longer valid
      if (mn > GetNumMessagesInCurrentMessageArea()) {
        mn = GetNumMessagesInCurrentMessageArea();
      }
    }
    // select new starting point of cache
    if (mn >= last_msgnum) {
      // going forward
      if (GetNumMessagesInCurrentMessageArea() <= MAX_TO_CACHE) {
        cache_start = 1;
      } else if (mn > (GetNumMessagesInCurrentMessageArea() - MAX_TO_CACHE)) {
        cache_start = GetNumMessagesInCurrentMessageArea() - MAX_TO_CACHE + 1;
      } else {
        cache_start = mn;
      }
    } else {
      // going backward
      if (mn > MAX_TO_CACHE) {
        cache_start = mn - MAX_TO_CACHE + 1;
      } else {
        cache_start = 1;
      }
    }

    if (cache_start < 1) {
      cache_start = 1;
    }

    // read in some sub info
    fileSub.Seek(cache_start * sizeof(postrec), WFile::seekBegin);
    fileSub.Read(cache, MAX_TO_CACHE * sizeof(postrec));

    // now, close the file, if necessary
    if (bCloseSubFile) {
      close_sub();
    }
    // cache is now valid
    believe_cache = true;
  }
  // error if msg # invalid
  if (mn < 1 || mn > GetNumMessagesInCurrentMessageArea()) {
    return nullptr;
  }
  last_msgnum = mn;
  return (cache + (mn - cache_start));
}

void write_post(int mn, postrec * pp) {
  postrec *p1;

  if (fileSub.IsOpen()) {
    fileSub.Seek(mn * sizeof(postrec), WFile::seekBegin);
    fileSub.Write(pp, sizeof(postrec));
    if (believe_cache) {
      if (mn >= cache_start && mn < (cache_start + MAX_TO_CACHE)) {
        p1 = cache + (mn - cache_start);
        if (p1 != pp) {
          *p1 = *pp;
        }
      }
    }
  }
}
