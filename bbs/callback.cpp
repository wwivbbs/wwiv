/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include "bbs/callback.h"

#include <string>

#include "bbs/confutil.h"
#include "bbs/input.h"
#include "bbs/newuser.h"
#include "bbs/wwiv.h"
#include "bbs/printfile.h"
#include "bbs/uedit.h"
#include "bbs/wcomm.h"
#include "core/strings.h"

using std::string;

void wwivnode(WUser *pUser, int mode) {
  char sysnum[6], s[81];
  int nUserNumber, nSystemNumber;

  if (!mode) {
    bout.nl();
    bout << "|#7Are you an active WWIV SysOp (y/N): ";
    if (!yesno()) {
      return;
    }
  }
  bout.nl();
  bout << "|#7Node:|#0 ";
  input(sysnum, 5);
  if (sysnum[0] == 'L' && mode) {
    print_net_listing(false);
    bout << "|#7Node:|#0 ";
    input(sysnum, 5);
  }
  if (sysnum[0] == '0' && mode) {
    pUser->SetForwardNetNumber(0);
    pUser->SetHomeUserNumber(0);
    pUser->SetHomeSystemNumber(0);
    return;
  }
  sprintf(s, "1@%s", sysnum);
  parse_email_info(s, &nUserNumber, &nSystemNumber);
  if (nSystemNumber == 0) {
    bout << "|#2No match for " << sysnum << "." << wwiv::endl;
    pausescr();
    return;
  }
  net_system_list_rec *csne = next_system(nSystemNumber);
  sprintf(s, "Sysop @%u %s %s", nSystemNumber, csne->name, session()->GetNetworkName());
  string ph, ph1;
  if (!mode) {
    ph1 = pUser->GetDataPhoneNumber();
    input_dataphone();
    ph = pUser->GetDataPhoneNumber();
    pUser->SetDataPhoneNumber(ph1.c_str());
    enter_regnum();
  } else {
    ph = csne->phone;
  }
  if (ph != csne->phone) {
    bout.nl();
    if (printfile(ASV0_NOEXT)) {
      // failed
      bout.nl();
      pausescr();
    }
    sprintf(s, "Attempted WWIV SysOp autovalidation.");
    pUser->SetNote(s);
    if (pUser->GetSl() < syscfg.newusersl) {
      pUser->SetSl(syscfg.newusersl);
    }
    if (pUser->GetDsl() < syscfg.newuserdsl) {
      pUser->SetDsl(syscfg.newuserdsl);
    }
    return;
  }
  sysoplog("-+ WWIV SysOp");
  sysoplog(s);
  pUser->SetRestriction(session()->asv.restrict);
  pUser->SetExempt(session()->asv.exempt);
  pUser->SetAr(session()->asv.ar);
  pUser->SetDar(session()->asv.dar);
  if (!mode) {
    bout.nl();
    if (printfile(ASV1_NOEXT)) {
      // passed
      bout.nl();
      pausescr();
    }
  }
  if (wwiv::strings::IsEquals(pUser->GetDataPhoneNumber(),
                              reinterpret_cast<char*>(csne->phone))) {
    if (pUser->GetSl() < session()->asv.sl) {
      pUser->SetSl(session()->asv.sl);
    }
    if (pUser->GetDsl() < session()->asv.dsl) {
      pUser->SetDsl(session()->asv.dsl);
    }
  } else {
    if (!mode) {
      bout.nl();
      if (printfile(ASV2_NOEXT)) {
        // data phone not bbs
        bout.nl();
        pausescr();
      }
    }
  }
  pUser->SetForwardNetNumber(session()->GetNetworkNumber());
  pUser->SetHomeUserNumber(1);
  pUser->SetHomeSystemNumber(nSystemNumber);
  if (!mode) {
    print_affil(pUser);
    pausescr();
  }
  pUser->SetForwardUserNumber(pUser->GetHomeUserNumber());
  pUser->SetForwardSystemNumber(pUser->GetHomeSystemNumber());
  if (!mode) {
    bout.nl();
    if (printfile(ASV3_NOEXT)) {
      \
      // mail forwarded
      bout.nl();
      pausescr();
    }
  }
  if (!pUser->GetWWIVRegNumber()) {
    enter_regnum();
  }
  changedsl();
}
