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
#include "subxtr.h"

#include "wwiv.h"
#include "core/strings.h"

static xtrasubsnetrec *xsubsn;
static int nn;

static char *mallocin_file(const char *pszFileName, size_t *len) {
  *len = 0;
  char* ss = nullptr;

  File file(pszFileName);
  if (file.Open(File::modeReadOnly | File::modeBinary)) {
    *len = file.GetLength();
    ss = static_cast<char *>(malloc(*len + 20));
    if (ss) {
      file.Read(ss, *len);
      ss[*len] = 0;
    }
    file.Close();
    return ss;
  }
  return ss;
}

static char *skipspace(char *ss2) {
  while ((*ss2) && ((*ss2 != ' ') && (*ss2 != '\t'))) {
    ++ss2;
  }
  if (*ss2) {
    *ss2++ = 0;
  }
  while ((*ss2 == ' ') || (*ss2 == '\t')) {
    ++ss2;
  }
  return ss2;
}

static xtrasubsnetrec *fsub(int netnum, int type) {
  if (type != 0) {
    for (int i = 0; i < nn; i++) {
      if (xsubsn[i].net_num == netnum && xsubsn[i].type == static_cast<unsigned short>(type)) {
        return (&(xsubsn[i]));
      }
    }
  }
  return nullptr;
}


bool read_subs_xtr(int nMaxSubs, int nNumSubs, subboardrec * subboards) {
  char *ss1, *ss2;
  int n, curn;
  short i = 0;
  char s[81];
  xtrasubsnetrec *xnp;

  if (xsubs) {
    for (i = 0; i < nNumSubs; i++) {
      if ((xsubs[i].flags & XTRA_MALLOCED) && xsubs[i].nets) {
        free(xsubs[i].nets);
      }
    }
    free(xsubs);
    if (xsubsn) {
      free(xsubsn);
    }
    xsubsn = nullptr;
    xsubs = nullptr;
  }
  size_t l = nMaxSubs * sizeof(xtrasubsrec);
  xsubs = static_cast<xtrasubsrec *>(malloc(l + 1));
  if (!xsubs) {
    std::cout << "Insufficient memory (" << l << "d bytes) for SUBS.XTR" << std::endl;
    return false;
  }
  memset(xsubs, 0, l);

  sprintf(s, "%s%s", syscfg.datadir, SUBS_XTR);
  char* ss = mallocin_file(s, &l);
  nn = 0;
  if (ss) {
    for (ss1 = strtok(ss, "\r\n"); ss1; ss1 = strtok(nullptr, "\r\n")) {
      if (*ss1 == '$') {
        ++nn;
      }
    }
    free(ss);
  } else {
    for (i = 0; i < nNumSubs; i++) {
      if (subboards[i].type) {
        ++nn;
      }
    }
  }
  if (nn) {
    l = static_cast<long>(nn) * sizeof(xtrasubsnetrec);
    xsubsn = static_cast<xtrasubsnetrec *>(malloc(l));
    if (!xsubsn) {
      std::cout << "Insufficient memory (" << l << " bytes) for net subs info" << std::endl;
      return false;
    }
    memset(xsubsn, 0, l);

    nn = 0;
    ss = mallocin_file(s, &l);
    if (ss) {
      curn = -1;
      for (ss1 = strtok(ss, "\r\n"); ss1; ss1 = strtok(nullptr, "\r\n")) {
        switch (*ss1) {
        case '!':                         /* sub idx */
          curn = atoi(ss1 + 1);
          if ((curn < 0) || (curn >= nNumSubs)) {
            curn = -1;
          }
          break;
        case '@':                         /* desc */
          if (curn >= 0) {
            strncpy(xsubs[curn].desc, ss1 + 1, 60);
          }
          break;
        case '#':                         /* flags */
          if (curn >= 0) {
            xsubs[curn].flags = atol(ss1 + 1);
          }
          break;
        case '$':                         /* net info */
          if (curn >= 0) {
            if (!xsubs[curn].num_nets) {
              xsubs[curn].nets = &(xsubsn[nn]);
            }
            ss2 = skipspace(++ss1);
            for (i = 0; i < session()->GetMaxNetworkNumber(); i++) {
              if (wwiv::strings::IsEqualsIgnoreCase(net_networks[i].name, ss1)) {
                break;
              }
            }
            if ((i < session()->GetMaxNetworkNumber()) && (*ss2)) {
              xsubsn[nn].net_num = i;
              ss1 = ss2;
              ss2 = skipspace(ss2);
              strncpy(xsubsn[nn].stype, ss1, 7);
              xsubsn[nn].type = wwiv::strings::StringToUnsignedShort(xsubsn[nn].stype);
              if (xsubsn[nn].type) {
                sprintf(xsubsn[nn].stype, "%u", xsubsn[nn].type);
              }
              ss1 = ss2;
              ss2 = skipspace(ss2);
              xsubsn[nn].flags = atol(ss1);
              ss1 = ss2;
              ss2 = skipspace(ss2);
              xsubsn[nn].host = wwiv::strings::StringToShort(ss1);
              ss1 = ss2;
              ss2 = skipspace(ss2);
              xsubsn[nn].category = wwiv::strings::StringToShort(ss1);
              nn++;
              xsubs[curn].num_nets++;
              xsubs[curn].num_nets_max++;
            } else {
              std::cout << "Unknown network '" << ss1 << "' in SUBS.XTR" << std::endl;
            }
          }
          break;
        case 0:
          break;
        default:
          break;
        }
      }
      free(ss);
    } else {
      for (curn = 0; curn < nNumSubs; curn++) {
        if (subboards[curn].type) {
          if (subboards[curn].age & 0x80) {
            xsubsn[nn].net_num = subboards[curn].name[40];
          } else {
            xsubsn[nn].net_num = 0;
          }
          if ((xsubsn[nn].net_num >= 0) && (xsubsn[nn].net_num < session()->GetMaxNetworkNumber())) {
            xsubs[curn].nets = &(xsubsn[nn]);
            xsubsn[nn].type = subboards[curn].type;
            sprintf(xsubsn[nn].stype, "%u", xsubsn[nn].type);
            nn++;
            xsubs[curn].num_nets = 1;
            xsubs[curn].num_nets_max = 1;
          }
        }
      }
      for (n = 0; n < session()->GetMaxNetworkNumber(); n++) {
        sprintf(s, "%s%s", net_networks[n].dir, ALLOW_NET);
        ss = mallocin_file(s, &l);
        if (ss) {
          for (ss1 = strtok(ss, " \t\r\n"); ss1; ss1 = strtok(nullptr, " \t\r\n")) {
            xnp = fsub(n, atoi(ss1));
            if (xnp) {
              xnp->flags |= XTRA_NET_AUTO_ADDDROP;
            }
          }
          free(ss);
        }
        sprintf(s, "%sSUBS.PUB", net_networks[n].dir);
        ss = mallocin_file(s, &l);
        if (ss) {
          for (ss1 = strtok(ss, " \t\r\n"); ss1; ss1 = strtok(nullptr, " \t\r\n")) {
            xnp = fsub(n, atoi(ss1));
            if (xnp) {
              xnp->flags |= XTRA_NET_AUTO_INFO;
            }
          }
          free(ss);
        }
        sprintf(s, "%sNNALL.NET", net_networks[n].dir);
        ss = mallocin_file(s, &l);
        if (ss) {
          for (ss1 = strtok(ss, "\r\n"); ss1; ss1 = strtok(nullptr, "\r\n")) {
            while ((*ss1 == ' ') || (*ss1 == '\t')) {
              ++ss1;
            }
            ss2 = skipspace(ss1);
            xnp = fsub(n, atoi(ss1));
            if (xnp) {
              xnp->host = wwiv::strings::StringToShort(ss2);
            }
          }
          free(ss);
        }
      }
      for (i = 0; i < nn; i++) {
        if ((xsubsn[i].type) && (!xsubsn[i].host)) {
          sprintf(s, "%sNN%u.NET",
                  net_networks[xsubsn[i].net_num].dir,
                  xsubsn[i].type);
          ss = mallocin_file(s, &l);
          if (ss) {
            for (ss1 = ss; (*ss1) && ((*ss1 < '0') || (*ss1 > '9')); ss1++)
              ;
            xsubsn[i].host = wwiv::strings::StringToShort(ss1);
            free(ss);
          }
        }
      }
    }
  }
  return true;
}
