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
#include "bbs/menus/menuspec.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/chains.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/multinst.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/menus/menusupp.h"
#include "common/input.h"
#include "common/output.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/files/files.h"

#include <string>

namespace wwiv::bbs::menus {

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

/* ---------------------------------------------------------------------- */
/* menuspec.cpp - Menu Specific support functions                           */
/*                                                                        */
/* Functions that don't have any direct WWIV function go in here           */
/* ie, functions to help emulate other BBS's.                             */
/* ---------------------------------------------------------------------- */


static int FindDN(const std::string& dl_fn) {
  for (auto i = 0; i < a()->dirs().size(); i++) {
    if (iequals(a()->dirs()[i].filename, dl_fn)) {
      return i;
    }
  }
  return -1;
}

/**
 *  Download a file
 *
 *  dir_fn:  fname of your directory record
 *  dl_fn:   Filename to download
 *  free_dl: true if this is a free download
 *  show_title:  true if title is to be shown with file info
 */
int MenuDownload(const std::string& dir_fn, const std::string& dl_fn, bool free_dl, bool show_title) {
  int bOkToDL;
  User ur;
  bool abort = false;

  int dn = FindDN(dir_fn);

  if (dn == -1) {
    sysoplog() << "Download: Dir Not Found";                  /* DL - DIR NOT FOUND */
    return 0;
  }
  const auto& dir = a()->dirs()[dn];
  dliscan1(dir);

  int nRecordNumber = recno(dl_fn);
  if (nRecordNumber <= 0) {
    bin.checka(&abort);
    if (abort) {
      return -1;
    }
    sysoplog() << "Download: File Not Found"; /* DL - FILE NOT FOUND */
    return 0;
  }
  bool ok = true;
  while (nRecordNumber > 0 && ok && !a()->sess().hangup()) {
    a()->tleft(true);
    auto f = a()->current_file_area()->ReadFile(nRecordNumber);
    bout.nl();

    if (show_title) {
      bout << "Directory  : " << dir.name << wwiv::endl;
    }
    bOkToDL = printfileinfo(&f.u(), dir);

    if (!ratio_ok()) {
      return -1;
    }
    if (bOkToDL || free_dl) {
      write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_NONE);
      auto s1 = FilePath(dir.path, f);
      if (dir.mask & mask_cdrom) {
        s1 = FilePath(a()->sess().dirs().temp_directory(), f);
        if (!File::Exists(s1)) {
          File::Copy(FilePath(dir.path, f), s1);
        }
      }
      bool sent = false;
      if (bOkToDL == -1) {
        send_file(s1.string(), &sent, &abort, f.aligned_filename(), dn, -2L);
      } else {
        send_file(s1.string(), &sent, &abort, f.aligned_filename(), dn, f.numbytes());
      }

      if (sent) {
        if (!free_dl) {
          a()->user()->increment_downloaded();
          a()->user()->set_dk(a()->user()->dk() +
                                    static_cast<int>(bytes_to_k(f.numbytes())));
        }
        ++f.u().numdloads;
        if (a()->current_file_area()->UpdateFile(f, nRecordNumber)) {
          a()->current_file_area()->Save();
        }

        sysoplog() << "Downloaded '" << f << "'.";

        if (a()->config()->sysconfig_flags() & sysconfig_log_dl) {
          a()->users()->readuser(&ur, f.u().ownerusr);
          if (!ur.deleted()) {
            if (date_to_daten(ur.firston()) < f.u().daten) {
              const auto username_num = a()->user()->name_and_number();
              ssm(f.u().ownerusr) << username_num << " downloaded '" << f.aligned_filename() << "' on " << date();
            }
          }
        }
      }

      bout.nl(2);
      bout.bprintf("Your ratio is now: %-6.3f\r\n", a()->user()->ratio());

      if (a()->sess().IsUserOnline()) {
        a()->UpdateTopScreen();
      }
    } else {
      bout << "\r\n\nNot enough time left to D/L.\r\n";
    }
    if (abort) {
      ok = false;
    } else {
      nRecordNumber = nrecno(dl_fn, nRecordNumber);
    }
  }
  return abort ? -1 : 1;
}

int MenuDownload(const std::string& dir_and_fname, bool bFreeDL, bool bTitle) {
  auto v = SplitString(dir_and_fname, " ", true);
  if (v.size() != 2) {
    return -1;
  }
  return MenuDownload(v.at(0), aligns(v.at(1)), bFreeDL, bTitle);
}

static int FindDoorNo(const std::string& name) {
  for (auto i = 0; i < size_int(a()->chains->chains()); i++) {
    if (iequals(a()->chains->at(i).description, name)) {
      return i;
    }
  }

  return -1;
}

static bool ValidateDoorAccess(int door_number) {
  const auto& c = a()->chains->at(door_number);
  if (c.ansi && !okansi()) {
    return false;
  }
  if (c.local_only && a()->sess().using_modem()) {
    return false;
  }
  if (!check_acs(c.acs)) {
    return false;
  }
  // Check multi-instance doors.
  if (auto inst = find_instance_by_loc(INST_LOC_CHAINS, door_number + 1); inst != 0) {
    const auto inuse_msg = fmt::format("|#2Chain '{}' is in use on instance {}.  ", c.description, inst);
    if (!c.multi_user) {
      bout << inuse_msg << " Try again later.\r\n";
      return false;
    }
    bout << inuse_msg << " Care to join in? ";
    if (!bin.yesno()) {
      return false;
    }
  }
  // passed all the checks, return true
  return true;
}

/**
 * Run a Door (chain)
 *
 * name = Door description to run
 * bFree  = If true, security on door will not back checked
 */
bool MenuRunDoorName(const std::string& name, bool bFree) {
  const auto door_number = FindDoorNo(name);
  return door_number >= 0 ? MenuRunDoorNumber(door_number, bFree) : false;
}

bool MenuRunDoorNumber(int nDoorNumber, bool bFree) {
  if (!bFree && !ValidateDoorAccess(nDoorNumber)) {
    return false;
  }

  run_chain(nDoorNumber);
  return true;
}

/* ----------------------- */
/* End of run door section */
/* ----------------------- */
void ChangeSubNumber() {
  bout << "|#7Select Sub number : |#0";

  const auto s = mmkey(MMKeyAreaType::subs);
  for (auto i = 0; i < size_int(a()->usub); i++) {
    if (s == a()->usub[i].keys) {
      a()->set_current_user_sub_num(i);
    }
  }
}

void ChangeDirNumber() {
  auto done = false;
  while (!done && !a()->sess().hangup()) {
    bout << "|#7Select Dir number : |#0";

    const auto s = mmkey(MMKeyAreaType::dirs);

    if (s[0] == '?') {
      DirList();
      bout.nl();
      continue;
    }
    for (auto i = 0; i < size_int(a()->udir); i++) {
      if (s == a()->udir[i].keys) {
        a()->set_current_user_dir_num(i);
        done = true;
      }
    }
  }
}


static void SetConf(ConferenceType t, char key) {
  auto info = get_conf_info(t);

  for (auto i = 0; i < size_int(info.uc); i++) {
    if (key == info.uc[i].key.key()) {
      setuconf(t, i, -1);
      break;
    }
  }
}

// have a little conference ability...
void SetMsgConf(char conf_designator) {
  SetConf(ConferenceType::CONF_SUBS, conf_designator);
}

void SetDirConf(char conf_designator) {
  SetConf(ConferenceType::CONF_DIRS, conf_designator);
}

void EnableConf() {
  tmp_disable_conf(false);
}

void DisableConf() {
  tmp_disable_conf(true);
}

void SetNewScanMsg() {
  sysoplog() << "Select Subs";
  config_qscan();
}

}
