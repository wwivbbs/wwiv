/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/wqscn.h"

#include <memory>
#include "bbs/bbs.h"
#include "sdk/config.h"
#include "sdk/filenames.h"

using namespace wwiv::core;

static std::unique_ptr<File> qscanFile;

static bool open_qscn() {
  if (!qscanFile) {
    qscanFile.reset(new File(FilePath(a()->config()->datadir(), USER_QSC)));
    if (!qscanFile->Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return false;
    }
  }
  return true;
}


void close_qscn() {
  if (qscanFile->IsOpen()) {
    qscanFile.reset();
  }
}


void read_qscn(int user_number, uint32_t* qscn, bool stay_open, bool bForceRead) {
  if (!bForceRead) {
    if ((a()->sess().IsUserOnline() && user_number == a()->usernum) ||
        (a()->at_wfc() && user_number == 1)) {
      if (qscn != a()->sess().qsc) {
        for (int i = (a()->config()->qscn_len() / 4) - 1; i >= 0; i--) {
          qscn[i] = a()->sess().qsc[i];
        }
      }
      return;
    }
  }
  if (open_qscn()) {
    long lPos = static_cast<long>(a()->config()->qscn_len()) * static_cast<long>(user_number);
    if (lPos + static_cast<long>(a()->config()->qscn_len()) <= qscanFile->length()) {
      qscanFile->Seek(lPos, File::Whence::begin);
      qscanFile->Read(qscn, a()->config()->qscn_len());
      if (!stay_open) {
        close_qscn();
      }
      return;
    }
  }
  if (!stay_open) {
    close_qscn();
  }

  a()->sess().ResetQScanPointers(*a()->config());
}


void write_qscn(int user_number, uint32_t *qscn, bool stay_open) {
  if ((user_number < 1) || (user_number > a()->config()->max_users()) ||
      a()->sess().guest_user()) {
    return;
  }

  if ((a()->sess().IsUserOnline() && (user_number == a()->usernum)) ||
      (a()->at_wfc() && user_number == 1)) {
    if (a()->sess().qsc != qscn) {
      for (int i = (a()->config()->qscn_len() / 4) - 1; i >= 0; i--) {
        a()->sess().qsc[i] = qscn[i];
      }
    }
  }
  if (open_qscn()) {
    auto pos = static_cast<long>(a()->config()->qscn_len()) * static_cast<long>(user_number);
    qscanFile->Seek(pos, File::Whence::begin);
    qscanFile->Write(qscn, a()->config()->qscn_len());
    if (!stay_open) {
      close_qscn();
    }
  }
}

