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

#include "wwiv.h"
#include "subxtr.h"
#include "core/strings.h"
#include "core/wtextfile.h"

bool display_sub_categories();
int find_hostfor(char *type, short *ui, char *pszDescription, short *opt);


static void maybe_netmail(xtrasubsnetrec * ni, bool bAdd) {
  GetSession()->bout << "|#5Send email request to the host now? ";
  if (yesno()) {
    strcpy(irt, "Sub type ");
    strcat(irt, ni->stype);
    if (bAdd) {
      strcat(irt, " - ADD request");
    } else {
      strcat(irt, " - DROP request");
    }
    set_net_num(ni->net_num);
    email(1, ni->host, false, 0);
  }
}

static void sub_req(int main_type, int minor_type, int tosys, char *extra) {
  net_header_rec nh;

  nh.tosys = static_cast<unsigned short>(tosys);
  nh.touser = 1;
  nh.fromsys = net_sysnum;
  nh.fromuser = 1;
  nh.main_type = static_cast<unsigned short>(main_type);
  nh.minor_type = static_cast<unsigned short>(minor_type) ;
  nh.list_len = 0;
  nh.daten = static_cast<unsigned long>(time(nullptr));
  if (!minor_type) {
    nh.length = strlen(extra) + 1;
  } else {
    nh.length = 1;
    extra[0] = 0;
  }
  nh.method = 0;

  send_net(&nh, nullptr, extra, nullptr);

  GetSession()->bout.NewLine();
  if (main_type == main_type_sub_add_req) {
    GetSession()->bout <<  "Automated add request sent to @" << tosys << wwiv::endl;
  } else {
    GetSession()->bout << "Automated drop request sent to @" << tosys << wwiv::endl;
  }
  pausescr();
}


#define OPTION_AUTO   0x0001
#define OPTION_NO_TAG 0x0002
#define OPTION_GATED  0x0004
#define OPTION_NETVAL 0x0008
#define OPTION_ANSI   0x0010


int find_hostfor(char *type, short *ui, char *pszDescription, short *opt) {
  char s[255], *ss;
  int rc = 0;

  if (pszDescription) {
    *pszDescription = 0;
  }
  *opt = 0;

  bool done = false;
  for (int i = 0; i < 256 && !done; i++) {
    if (i) {
      sprintf(s, "%s%s.%d", GetSession()->GetNetworkDataDirectory(), SUBS_NOEXT, i);
    } else {
      sprintf(s, "%s%s", GetSession()->GetNetworkDataDirectory(), SUBS_LST);
    }
    WTextFile file(s, "r");
    if (file.IsOpen()) {
      while (!done && file.ReadLine(s, 160)) {
        if (s[0] > ' ') {
          ss = strtok(s, " \r\n\t");
          if (ss) {
            if (wwiv::strings::IsEqualsIgnoreCase(ss, type)) {
              ss = strtok(nullptr, " \r\n\t");
              if (ss) {
                short h = static_cast<short>(atol(ss));
                short o = 0;
                ss = strtok(nullptr, "\r\n");
                if (ss) {
                  int i1 = 0;
                  while (*ss && ((*ss == ' ') || (*ss == '\t'))) {
                    ++ss;
                    ++i1;
                  }
                  if (i1 < 4) {
                    while (*ss && (*ss != ' ') && (*ss != '\t')) {
                      switch (*ss) {
                      case 'T':
                        o |= OPTION_NO_TAG;
                        break;
                      case 'R':
                        o |= OPTION_AUTO;
                        break;
                      case 'G':
                        o |= OPTION_GATED;
                        break;
                      case 'N':
                        o |= OPTION_NETVAL;
                        break;
                      case 'A':
                        o |= OPTION_ANSI;
                        break;
                      }
                      ++ss;
                    }
                    while (*ss && ((*ss == ' ') || (*ss == '\t'))) {
                      ++ss;
                    }
                  }
                  if (*ui) {
                    if (*ui == h) {
                      done = true;
                      *opt = o;
                      rc = h;
                      if (pszDescription) {
                        strcpy(pszDescription, ss);
                      }
                    }
                  } else {
                    GetSession()->bout.NewLine();
                    GetSession()->bout << "Type: " << type << wwiv::endl;
                    GetSession()->bout << "Host: " << h << wwiv::endl;
                    GetSession()->bout << "Sub : " << ss << wwiv::endl;
                    GetSession()->bout.NewLine();
                    GetSession()->bout << "|#5Is this the sub you want? ";
                    if (yesno()) {
                      done = true;
                      *ui = h;
                      *opt = o;
                      rc = h;
                      if (pszDescription) {
                        strcpy(pszDescription, ss);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      file.Close();
    } else {
      done = true;
    }
  }

  return rc;
}


void sub_xtr_del(int n, int nn, int f) {
  int i;
  short opt;

  xtrasubsnetrec xn = xsubs[n].nets[nn];

  if (f) {
    xsubs[n].num_nets--;

    for (i = nn; i < xsubs[n].num_nets; i++) {
      xsubs[n].nets[i] = xsubs[n].nets[i + 1];
    }
  }
  set_net_num(xn.net_num);

  if ((xn.host) && (valid_system(xn.host))) {
    int ok = find_hostfor(xn.stype, &xn.host, nullptr, &opt);
    if (ok) {
      if (opt & OPTION_AUTO) {
        GetSession()->bout << "|#5Attempt automated drop request? ";
        if (yesno()) {
          sub_req(main_type_sub_drop_req, xn.type, xn.host, xn.stype);
        }
      } else {
        maybe_netmail(&xn, false);
      }
    } else {
      GetSession()->bout << "|#5Attempt automated drop request? ";
      if (yesno()) {
        sub_req(main_type_sub_drop_req, xn.type, xn.host, xn.stype);
      } else {
        maybe_netmail(&xn, false);
      }
    }
  }
}

void sub_xtr_add(int n, int nn) {
  unsigned short i, i1;
  short opt;
  char szDescription[100], s[100], onx[20], *mmk, ch;
  int onxi, odci, ii, gc;
  xtrasubsnetrec *xnp;

  if (nn < 0 || nn >= xsubs[n].num_nets) {
    if (xsubs[n].num_nets_max <= xsubs[n].num_nets) {
      i1 = xsubs[n].num_nets + 4;
      xnp = static_cast<xtrasubsnetrec *>(BbsAllocA(i1 * sizeof(xtrasubsnetrec)));
      if (!xnp) {
        return;
      }
      for (i = 0; i < xsubs[n].num_nets; i++) {
        xnp[i] = xsubs[n].nets[i];
      }

      if (xsubs[n].flags & XTRA_MALLOCED) {
        free(xsubs[n].nets);
      }

      xsubs[n].nets = xnp;
      xsubs[n].num_nets_max = i1;
      xsubs[n].flags |= XTRA_MALLOCED;
    }
    xnp = &(xsubs[n].nets[xsubs[n].num_nets]);
  } else {
    xnp = &(xsubs[n].nets[nn]);
  }

  memset(xnp, 0, sizeof(xtrasubsnetrec));

  if (GetSession()->GetMaxNetworkNumber() > 1) {
    odc[0] = 0;
    odci = 0;
    onx[0] = 'Q';
    onx[1] = 0;
    onxi = 1;
    GetSession()->bout.NewLine();
    for (ii = 0; ii < GetSession()->GetMaxNetworkNumber(); ii++) {
      if (ii < 9) {
        onx[onxi++] = static_cast<char>(ii + '1');
        onx[onxi] = 0;
      } else {
        odci = (ii + 1) / 10;
        odc[odci - 1] = static_cast<char>(odci + '0');
        odc[odci] = 0;
      }
      GetSession()->bout << "(" << ii + 1 << ") " << net_networks[ii].name << wwiv::endl;
    }
    GetSession()->bout << "Q. Quit\r\n\n";
    GetSession()->bout << "|#2Which network (number): ";
    if (GetSession()->GetMaxNetworkNumber() < 9) {
      ch = onek(onx);
      if (ch == 'Q') {
        ii = -1;
      } else {
        ii = ch - '1';
      }
    } else {
      mmk = mmkey(2);
      if (*mmk == 'Q') {
        ii = -1;
      } else {
        ii = atoi(mmk) - 1;
      }
    }
    if (ii >= 0 && ii < GetSession()->GetMaxNetworkNumber()) {
      set_net_num(ii);
    } else {
      return;
    }
  }
  xnp->net_num = static_cast<short>(GetSession()->GetNetworkNumber());

  GetSession()->bout.NewLine();
  GetSession()->bout << "|#2What sub type? ";
  input(xnp->stype, 7);
  if (xnp->stype[0] == 0) {
    return;
  }

  xnp->type = wwiv::strings::StringToUnsignedShort(xnp->stype);

  if (xnp->type) {
    sprintf(xnp->stype, "%u", xnp->type);
  }

  GetSession()->bout << "|#5Will you be hosting the sub? ";
  if (yesno()) {
    char szFileName[MAX_PATH];
    sprintf(szFileName, "%sn%s.net", GetSession()->GetNetworkDataDirectory(), xnp->stype);
    WFile file(szFileName);
    if (file.Open(WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite)) {
      file.Close();
    }

    GetSession()->bout << "|#5Allow auto add/drop requests? ";
    if (noyes()) {
      xnp->flags |= XTRA_NET_AUTO_ADDDROP;
    }

    GetSession()->bout << "|#5Make this sub public (in subs.lst)?";
    if (noyes()) {
      xnp->flags |= XTRA_NET_AUTO_INFO;
      if (display_sub_categories()) {
        gc = 0;
        while (!gc) {
          GetSession()->bout.NewLine();
          GetSession()->bout << "|#2Which category is this sub in (0 for unknown/misc)? ";
          input(s, 3);
          i = wwiv::strings::StringToUnsignedShort(s);
          if (i || wwiv::strings::IsEquals(s, "0")) {
            WTextFile ff(GetSession()->GetNetworkDataDirectory(), CATEG_NET, "rt");
            while (ff.ReadLine(s, 100)) {
              i1 = wwiv::strings::StringToUnsignedShort(s);
              if (i1 == i) {
                gc = 1;
                xnp->category = i;
                break;
              }
            }
            file.Close();
            if (wwiv::strings::IsEquals(s, "0")) {
              gc = 1;
            } else if (!xnp->category) {
              GetSession()->bout << "Illegal/invalid category.\r\n\n";
            }
          } else {
            if (strlen(s) == 1 && s[0] == '?') {
              display_sub_categories();
              continue;
            }
          }
        }
      }
    }
  } else {
    int ok = find_hostfor(xnp->stype, &(xnp->host), szDescription, &opt);

    if (!ok) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Which system (number) is the host? ";
      input(szDescription, 6);
      xnp->host = static_cast<unsigned short>(atol(szDescription));
      szDescription[0] = '\0';
    }
    if (!xsubs[n].desc[0]) {
      strcpy(xsubs[n].desc, szDescription);
    }

    if (xnp->host == net_sysnum) {
      xnp->host = 0;
    }

    if (xnp->host) {
      if (valid_system(xnp->host)) {
        if (ok) {
          if (opt & OPTION_NO_TAG) {
            subboards[n].anony |= anony_no_tag;
          }
          GetSession()->bout.NewLine();
          if (opt & OPTION_AUTO) {
            GetSession()->bout << "|#5Attempt automated add request? ";
            if (yesno()) {
              sub_req(main_type_sub_add_req, xnp->type, xnp->host, xnp->stype);
            }
          } else {
            maybe_netmail(xnp, true);
          }
        } else {
          GetSession()->bout.NewLine();
          GetSession()->bout << "|#5Attempt automated add request? ";
          bool bTryAutoAddReq = yesno();
          if (bTryAutoAddReq) {
            sub_req(main_type_sub_add_req, xnp->type, xnp->host, xnp->stype);
          } else {
            maybe_netmail(xnp, true);
          }
        }
      } else {
        GetSession()->bout.NewLine();
        GetSession()->bout << "The host is not listed in the network.\r\n";
        pausescr();
      }
    }
  }
  if ((nn < 0) || (nn >= xsubs[n].num_nets)) {
    xsubs[n].num_nets++;
  }
}


bool display_sub_categories() {
  if (!net_sysnum) {
    return false;
  }

  WTextFile ff(GetSession()->GetNetworkDataDirectory(), CATEG_NET, "rt");
  if (ff.IsOpen()) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "Available sub categories are:\r\n";
    bool abort = false;
    char szLine[255];
    while (!abort && ff.ReadLine(szLine, 100)) {
      char* ss = strchr(szLine, '\n');
      if (ss) {
        *ss = 0;
      }
      pla(szLine, &abort);
    }
    ff.Close();
    return true;
  }
  return false;
}

int amount_of_subscribers(const char *pszNetworkFileName) {
  int numnodes = 0;

  WFile file(pszNetworkFileName);
  if (!file.Open(WFile::modeReadOnly | WFile::modeBinary)) {
    return 0;
  } else {
    long len1 = file.GetLength();
    if (len1 == 0) {
      return 0;
    }
    char *b = static_cast<char *>(BbsAllocA(len1));
    file.Seek(0L, WFile::seekBegin);
    file.Read(b, len1);
    b[len1] = 0;
    file.Close();
    long len2 = 0;
    while (len2 < len1) {
      while ((len2 < len1) && ((b[len2] < '0') || (b[len2] > '9'))) {
        ++len2;
      }
      if ((b[len2] >= '0') && (b[len2] <= '9') && (len2 < len1)) {
        numnodes++;
        while ((len2 < len1) && (b[len2] >= '0') && (b[len2] <= '9')) {
          ++len2;
        }
      }
    }
    free(b);
  }
  return numnodes;
}
