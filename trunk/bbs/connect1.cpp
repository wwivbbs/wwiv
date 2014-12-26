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

#if defined (NET)
#include "sdk/vardec.h"
#include "sdk/net.h"
#else
#include "bbs/wwiv.h"
#endif
#include "core/strings.h"
#include "core/wwivassert.h"


void read_bbs_list();
int system_index(int ts);

using wwiv::strings::StringPrintf;

void zap_call_out_list() {
  if (net_networks[session()->GetNetworkNumber()].con) {
    free(net_networks[session()->GetNetworkNumber()].con);
    net_networks[ session()->GetNetworkNumber() ].con = nullptr;
    net_networks[ session()->GetNetworkNumber() ].num_con = 0;
  }
}


void read_call_out_list() {
  net_call_out_rec *con;

  zap_call_out_list();

  File fileCallout(session()->GetNetworkDataDirectory(), CALLOUT_NET);
  if (fileCallout.Open(File::modeBinary | File::modeReadOnly)) {
    long lFileLength = fileCallout.GetLength();
    char *ss = nullptr;
    if ((ss = static_cast<char*>(BbsAllocA(lFileLength + 512))) == nullptr) {
      WWIV_ASSERT(ss != nullptr);
      application()->AbortBBS(true);
    }
    fileCallout.Read(ss, lFileLength);
    fileCallout.Close();
    for (long lPos = 0L; lPos < lFileLength; lPos++) {
      if (ss[lPos] == '@') {
        ++net_networks[session()->GetNetworkNumber()].num_con;
      }
    }
    free(ss);
    if ((net_networks[session()->GetNetworkNumber()].con = (net_call_out_rec *)
         BbsAllocA(static_cast<long>((net_networks[session()->GetNetworkNumber()].num_con + 2) *
                                     sizeof(net_call_out_rec)))) == nullptr) {
      WWIV_ASSERT(net_networks[session()->GetNetworkNumber()].con != nullptr);
      application()->AbortBBS(true);
    }
    con = net_networks[session()->GetNetworkNumber()].con;
    con--;
    fileCallout.Open(File::modeBinary | File::modeReadOnly);
    if ((ss = static_cast<char*>(BbsAllocA(lFileLength + 512))) == nullptr) {
      WWIV_ASSERT(ss != nullptr);
      application()->AbortBBS(true);
    }
    fileCallout.Read(ss, lFileLength);
    fileCallout.Close();
    long p = 0L;
    while (p < lFileLength) {
      while ((p < lFileLength) && (strchr("@%/\"&-=+~!();^|#$_*", ss[p]) == nullptr)) {
        ++p;
      }
      if (p < lFileLength) {
        switch (ss[p]) {
        case '@':
          ++p;
          con++;
          con->macnum     = 0;
          con->options    = 0;
          con->call_anyway  = 0;
          con->password[0]  = 0;
          con->sysnum     = wwiv::strings::StringToUnsignedShort(&(ss[p]));
          con->min_hr     = -1;
          con->max_hr     = -1;
          con->times_per_day  = 0;
          con->min_k      = 0;
          con->call_x_days  = 0;
          break;
        case '&':
          con->options |= options_sendback;
          ++p;
          break;
        case '-':
          con->options |= options_ATT_night;
          ++p;
          break;
        case '_':
          con->options |= options_ppp;
          ++p;
          break;
        case '+':
          con->options |= options_no_call;
          ++p;
          break;
        case '~':
          con->options |= options_receive_only;
          ++p;
          break;
        case '!':
          con->options |= options_once_per_day;
          ++p;
          con->times_per_day = wwiv::strings::StringToUnsignedChar(&(ss[p]));
          if (!con->times_per_day) {
            con->times_per_day = 1;
          }
          break;
        case '%':
          ++p;
          con->macnum = wwiv::strings::StringToUnsignedChar(&(ss[p]));
          break;
        case '/':
          ++p;
          con->call_anyway = wwiv::strings::StringToUnsignedChar(&(ss[p]));
          ++p;
          break;
        case '#':
          ++p;
          con->call_x_days = wwiv::strings::StringToUnsignedChar(&(ss[p]));
          break;
        case '(':
          ++p;
          con->min_hr = wwiv::strings::StringToChar(&(ss[p]));
          break;
        case ')':
          ++p;
          con->max_hr = wwiv::strings::StringToChar(&(ss[p]));
          break;
        case '|':
          ++p;
          con->min_k = wwiv::strings::StringToUnsignedShort(&(ss[p]));
          if (!con->min_k) {
            con->min_k = 0;
          }
          break;
        case ';':
          ++p;
          con->options |= options_compress;
          break;
        case '^':
          ++p;
          con->options |= options_hslink;
          break;
        case '$':
          ++p;
          con->options |= options_force_ac;
          break;
        case '=':
          ++p;
          con->options |= options_hide_pend;
          break;
        case '*':
          ++p;
          con->options |= options_dial_ten;
          break;
        case '\"': {
          ++p;
          int i = 0;
          while ((i < 19) && (ss[p + static_cast<long>(i)] != '\"')) {
            ++i;
          }
          for (int i1 = 0; i1 < i; i1++) {
            con->password[i1] = ss[ p + static_cast<long>(i1) ];
          }
          con->password[i] = 0;
          p += static_cast<long>(i + 1);
        }
        break;
        }
      }
    }
    free(ss);
  }
}



int bbs_list_net_no = -1;


void zap_bbs_list() {
  if (csn) {
    free(csn);
    csn = nullptr;
  }
  if (csn_index) {
    free(csn_index);
    csn_index = nullptr;
  }
  session()->num_sys_list  = 0;
  bbs_list_net_no     = -1;
}


void read_bbs_list() {
  zap_bbs_list();

  if (net_sysnum == 0) {
    return;
  }
  File fileBbsData(session()->GetNetworkDataDirectory(), BBSDATA_NET);
  if (fileBbsData.Open(File::modeBinary | File::modeReadOnly)) {
    long lFileLength = fileBbsData.GetLength();
    session()->num_sys_list = static_cast<int>(lFileLength / sizeof(net_system_list_rec));
    if ((csn = static_cast<net_system_list_rec *>(BbsAllocA(lFileLength + 512L))) == nullptr) {
      WWIV_ASSERT(csn != nullptr);
      application()->AbortBBS(true);
    }
    for (int i = 0; i < session()->num_sys_list; i += 256) {
      fileBbsData.Read(&(csn[i]), 256 * sizeof(net_system_list_rec));
    }
    fileBbsData.Close();
  }
  bbs_list_net_no = session()->GetNetworkNumber();
}


void read_bbs_list_index() {
  zap_bbs_list();

  if (net_sysnum == 0) {
    return;
  }
  File fileBbsData(session()->GetNetworkDataDirectory(), BBSDATA_IND);
  if (fileBbsData.Open(File::modeBinary | File::modeReadOnly)) {
    long lFileLength = fileBbsData.GetLength();
    session()->num_sys_list = static_cast<int>(lFileLength / 2);
    if ((csn_index = static_cast<unsigned short *>(BbsAllocA(lFileLength))) == nullptr) {
      WWIV_ASSERT(csn_index != nullptr);
      application()->AbortBBS(true);
    }
    fileBbsData.Read(csn_index, lFileLength);
    fileBbsData.Close();
  } else {
    read_bbs_list();
  }
  bbs_list_net_no = session()->GetNetworkNumber();
}


int system_index(int ts) {
  if (bbs_list_net_no != session()->GetNetworkNumber()) {
    read_bbs_list_index();
  }

  if (csn) {
    for (int i = 0; i < session()->num_sys_list; i++) {
      if (csn[i].sysnum == ts && csn[i].forsys != 65535) {
        return i;
      }
    }
  } else {
    for (int i = 0; i < session()->num_sys_list; i++) {
      if (csn_index[i] == ts) {
        return i;
      }
    }
  }
  return -1;
}


bool valid_system(int ts) {
  return (system_index(ts) == -1) ? false : true;
}


net_system_list_rec *next_system(int ts) {
  static net_system_list_rec csne;
  int nIndex = system_index(ts);

  if (nIndex == -1) {
    return nullptr;
  } else if (csn) {
    return ((net_system_list_rec *) & (csn[nIndex]));
  } else {
    File fileBbsData(session()->GetNetworkDataDirectory(), BBSDATA_NET);
    fileBbsData.Open(File::modeBinary | File::modeReadOnly);
    fileBbsData.Seek(sizeof(net_system_list_rec) * static_cast<long>(nIndex), File::seekBegin);
    fileBbsData.Read(&csne, sizeof(net_system_list_rec));
    fileBbsData.Close();
    return (csne.forsys == 65535) ? nullptr : (&csne);
  }
}



void zap_contacts() {
  if (net_networks[session()->GetNetworkNumber()].ncn) {
    free(net_networks[session()->GetNetworkNumber()].ncn);
    net_networks[session()->GetNetworkNumber()].ncn = nullptr;
    net_networks[session()->GetNetworkNumber()].num_ncn = 0;
  }
}

void read_contacts() {
  zap_contacts();

  File fileContact(session()->GetNetworkDataDirectory(), CONTACT_NET);
  if (fileContact.Open(File::modeBinary | File::modeReadOnly)) {
    long lFileLength = fileContact.GetLength();
    net_networks[session()->GetNetworkNumber()].num_ncn = static_cast<short>(lFileLength / sizeof(net_contact_rec));
    if ((net_networks[session()->GetNetworkNumber()].ncn =
           static_cast<net_contact_rec *>(BbsAllocA((net_networks[session()->GetNetworkNumber()].num_ncn + 2) *
           sizeof(net_contact_rec)))) == nullptr) {
      WWIV_ASSERT(net_networks[session()->GetNetworkNumber()].ncn != nullptr);
      application()->AbortBBS(true);
    }
    fileContact.Seek(0L, File::seekBegin);
    fileContact.Read(net_networks[session()->GetNetworkNumber()].ncn,
                     net_networks[session()->GetNetworkNumber()].num_ncn * sizeof(net_contact_rec));
    fileContact.Close();
  }
}


void set_net_num(int nNetworkNumber) {
  if (nNetworkNumber >= 0 && nNetworkNumber < session()->GetMaxNetworkNumber()) {
    session()->SetNetworkNumber(nNetworkNumber);
    //session()->pszNetworkName = net_networks[session()->GetNetworkNumber()].name;
    //session()->pszNetworkDataDir = net_networks[session()->GetNetworkNumber()].dir;
    net_sysnum = net_networks[session()->GetNetworkNumber()].sysnum;
    session()->SetCurrentNetworkType(net_networks[ session()->GetNetworkNumber() ].type);

    application()->networkNumEnvVar = StringPrintf("WWIV_NET=%d", session()->GetNetworkNumber());
  }
}

