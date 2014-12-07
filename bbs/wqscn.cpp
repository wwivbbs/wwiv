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

#include "bbs/wwiv.h"


static File qscanFile;


bool open_qscn() {
  if (!qscanFile.IsOpen()) {
    qscanFile.SetName(syscfg.datadir, USER_QSC);
    if (!qscanFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      return false;
    }
  }
  return true;
}


void close_qscn() {
  if (qscanFile.IsOpen()) {
    qscanFile.Close();
  }
}


void read_qscn(int nUserNumber, uint32_t* qscn, bool bStayOpen, bool bForceRead) {
  if (!bForceRead) {
    if ((session()->IsUserOnline() && nUserNumber == session()->usernum) ||
        (application()->GetWfcStatus() && nUserNumber == 1)) {
      if (qscn != qsc) {
        for (int i = (syscfg.qscn_len / 4) - 1; i >= 0; i--) {
          qscn[i] = qsc[i];
        }
      }
      return;
    }
  }
  if (open_qscn()) {
    long lPos = static_cast<long>(syscfg.qscn_len) * static_cast<long>(nUserNumber);
    if (lPos + static_cast<long>(syscfg.qscn_len) <= qscanFile.GetLength()) {
      qscanFile.Seek(lPos, File::seekBegin);
      qscanFile.Read(qscn, syscfg.qscn_len);
      if (!bStayOpen) {
        close_qscn();
      }
      return;
    }
  }
  if (!bStayOpen) {
    close_qscn();
  }

  memset(qsc, 0, syscfg.qscn_len);
  *qsc = 999;
  memset(qsc + 1, 0xff, ((syscfg.max_dirs + 31) / 32) * 4);
  memset(qsc + 1 + (syscfg.max_subs + 31) / 32, 0xff, ((syscfg.max_subs + 31) / 32) * 4);
}


void write_qscn(int nUserNumber, uint32_t *qscn, bool bStayOpen) {
  if ((nUserNumber < 1) || (nUserNumber > syscfg.maxusers) || guest_user) {
    return;
  }

  if ((session()->IsUserOnline() && (nUserNumber == session()->usernum)) ||
      (application()->GetWfcStatus() && nUserNumber == 1)) {
    if (qsc != qscn) {
      for (int i = (syscfg.qscn_len / 4) - 1; i >= 0; i--) {
        qsc[ i ] = qscn[ i ];
      }
    }
  }
  if (open_qscn()) {
    long lPos = static_cast<long>(syscfg.qscn_len) * static_cast<long>(nUserNumber);
    qscanFile.Seek(lPos, File::seekBegin);
    qscanFile.Write(qscn, syscfg.qscn_len);
    if (!bStayOpen) {
      close_qscn();
    }
  }
}

