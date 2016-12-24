/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/subreq.h"

#include <string>

#include "bbs/input.h"
#include "sdk/subxtr.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/mmkey.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/pause.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

bool display_sub_categories();
int find_hostfor(const std::string& type, short *ui, char *description, short *opt);


static void maybe_netmail(subboard_network_data_t* ni, bool bAdd) {
  bout << "|#5Send email request to the host now? ";
  if (yesno()) {
    strcpy(irt, "Sub type ");
    to_char_array(irt, ni->stype);
    if (bAdd) {
      strcat(irt, " - ADD request");
    } else {
      strcat(irt, " - DROP request");
    }
    set_net_num(ni->net_num);
    email(irt, 1, ni->host, false, 0);
  }
}

static void sub_req(uint16_t main_type, int tosys, const string& stype) {
  net_header_rec nh;

  nh.tosys = static_cast<uint16_t>(tosys);
  nh.touser = 1;
  nh.fromsys = a()->current_net().sysnum;
  nh.fromuser = 1;
  nh.main_type = main_type;
  // always use 0 since we use the stype
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = static_cast<uint32_t>(time(nullptr));
  nh.method = 0;
  // This is an alphanumeric sub type.
  nh.length = stype.size() + 1;
  send_net(&nh, {}, stype, "");

  bout.nl();
  if (main_type == main_type_sub_add_req) {
    bout <<  "Automated add request sent to @" << tosys << wwiv::endl;
  } else {
    bout << "Automated drop request sent to @" << tosys << wwiv::endl;
  }
  pausescr();
}


#define OPTION_AUTO   0x0001
#define OPTION_NO_TAG 0x0002
#define OPTION_GATED  0x0004
#define OPTION_NETVAL 0x0008
#define OPTION_ANSI   0x0010


int find_hostfor(const std::string& type, short *ui, char *description, short *opt) {
  char s[255], *ss;
  int rc = 0;

  if (description) {
    *description = 0;
  }
  *opt = 0;

  bool done = false;
  for (int i = 0; i < 256 && !done; i++) {
    if (i) {
      sprintf(s, "%s%s.%d", a()->network_directory().c_str(), SUBS_NOEXT, i);
    } else {
      sprintf(s, "%s%s", a()->network_directory().c_str(), SUBS_LST);
    }
    TextFile file(s, "r");
    if (file.IsOpen()) {
      while (!done && file.ReadLine(s, 160)) {
        if (s[0] > ' ') {
          ss = strtok(s, " \r\n\t");
          if (ss) {
            if (IsEqualsIgnoreCase(ss, type.c_str())) {
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
                      if (description) {
                        strcpy(description, ss);
                      }
                    }
                  } else {
                    bout.nl();
                    bout << "Type: " << type << wwiv::endl;
                    bout << "Host: " << h << wwiv::endl;
                    bout << "Sub : " << ss << wwiv::endl;
                    bout.nl();
                    bout << "|#5Is this the sub you want? ";
                    if (yesno()) {
                      done = true;
                      *ui = h;
                      *opt = o;
                      rc = h;
                      if (description) {
                        strcpy(description, ss);
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
  // make a copy of the old network info.
  auto xn = a()->subs().sub(n).nets[nn];

  if (f) {
    erase_at(a()->subs().sub(n).nets, nn);
  }
  set_net_num(xn.net_num);

  if (xn.host != 0 && valid_system(xn.host)) {
    short opt;
    int ok = find_hostfor(xn.stype, &xn.host, nullptr, &opt);
    if (ok) {
      if (opt & OPTION_AUTO) {
        bout << "|#5Attempt automated drop request? ";
        if (yesno()) {
          sub_req(main_type_sub_drop_req, xn.host, xn.stype);
        }
      } else {
        maybe_netmail(&xn, false);
      }
    } else {
      bout << "|#5Attempt automated drop request? ";
      if (yesno()) {
        sub_req(main_type_sub_drop_req, xn.host, xn.stype);
      } else {
        maybe_netmail(&xn, false);
      }
    }
  }
}

void sub_xtr_add(int n, int nn) {
  unsigned short i;
  short opt;
  char szDescription[100], s[100], onx[20], ch;
  int onxi, ii, gc;

  // nn may be -1
  while (nn >= size_int(a()->subs().sub(n).nets)) {
    a()->subs().sub(n).nets.push_back({});
  }
  subboard_network_data_t xnp = {};

  if (a()->max_net_num() > 1) {
    std::set<char> odc;
    onx[0] = 'Q';
    onx[1] = 0;
    onxi = 1;
    bout.nl();
    for (ii = 0; ii < a()->max_net_num(); ii++) {
      if (ii < 9) {
        onx[onxi++] = static_cast<char>(ii + '1');
        onx[onxi] = 0;
      } else {
        int odci = (ii + 1) / 10;
        odc.insert(static_cast<char>(odci + '0'));
      }
      bout << "(" << ii + 1 << ") " << a()->net_networks[ii].name << wwiv::endl;
    }
    bout << "Q. Quit\r\n\n";
    bout << "|#2Which network (number): ";
    if (a()->max_net_num() < 9) {
      ch = onek(onx);
      if (ch == 'Q') {
        ii = -1;
      } else {
        ii = ch - '1';
      }
    } else {
      string mmk = mmkey(odc);
      if (mmk == "Q") {
        ii = -1;
      } else {
        ii = StringToInt(mmk) - 1;
      }
    }
    if (ii >= 0 && ii < a()->max_net_num()) {
      set_net_num(ii);
    } else {
      return;
    }
  }
  xnp.net_num = static_cast<short>(a()->net_num());

  bout.nl();
  int stype_len = 7;
  if (a()->current_net().type == network_type_t::ftn) {
    bout << "|#2What echomail area: ";
    stype_len = 40;
  } else {
    bout << "|#2What sub type? ";
  }
  xnp.stype = input(stype_len, true);
  if (xnp.stype.empty()) {
    return;
  }

  bool is_hosting = false;
  if (a()->current_net().type == network_type_t::wwivnet || a()->current_net().type == network_type_t::internet) {
    bout << "|#5Will you be hosting the sub? ";
    is_hosting = yesno();
  }

  if (is_hosting) {
    string file_name = StrCat(a()->network_directory(), "n", xnp.stype, ".net");
    File file(file_name);
    if (file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      file.Close();
    }

    bout << "|#5Allow auto add/drop requests? ";
    if (noyes()) {
      xnp.flags |= XTRA_NET_AUTO_ADDDROP;
    }

    bout << "|#5Make this sub public (in subs.lst)?";
    if (noyes()) {
      xnp.flags |= XTRA_NET_AUTO_INFO;
      if (display_sub_categories()) {
        gc = 0;
        while (!gc) {
          bout.nl();
          bout << "|#2Which category is this sub in (0 for unknown/misc)? ";
          input(s, 3);
          i = StringToUnsignedShort(s);
          if (i || IsEquals(s, "0")) {
            TextFile ff(a()->network_directory(), CATEG_NET, "rt");
            while (ff.ReadLine(s, 100)) {
              int i1 = StringToUnsignedShort(s);
              if (i1 == i) {
                gc = 1;
                xnp.category = i;
                break;
              }
            }
            file.Close();
            if (IsEquals(s, "0")) {
              gc = 1;
            } else if (!xnp.category) {
              bout << "Illegal/invalid category.\r\n\n";
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
  } else if (a()->current_net().type == network_type_t::ftn) {
    // Set the fake fido node up as the host.
    xnp.host = FTN_FAKE_OUTBOUND_NODE;
  } else {
    int ok = find_hostfor(xnp.stype, &(xnp.host), szDescription, &opt);

    if (!ok) {
      bout.nl();
      bout << "|#2Which system (number) is the host? ";
      input(szDescription, 6);
      xnp.host = static_cast<uint16_t>(atol(szDescription));
      szDescription[0] = '\0';
    }
    if (!a()->subs().sub(n).desc[0]) {
      a()->subs().sub(n).desc = szDescription;
    }

    if (xnp.host == a()->current_net().sysnum) {
      xnp.host = 0;
    }

    if (xnp.host) {
      if (valid_system(xnp.host)) {
        if (ok) {
          if (opt & OPTION_NO_TAG) {
            a()->subs().sub(n).anony |= anony_no_tag;
          }
          bout.nl();
          if (opt & OPTION_AUTO) {
            bout << "|#5Attempt automated add request? ";
            if (yesno()) {
              sub_req(main_type_sub_add_req, xnp.host, xnp.stype);
            }
          } else {
            maybe_netmail(&xnp, true);
          }
        } else {
          bout.nl();
          bout << "|#5Attempt automated add request? ";
          bool bTryAutoAddReq = yesno();
          if (bTryAutoAddReq) {
            sub_req(main_type_sub_add_req, xnp.host, xnp.stype);
          } else {
            maybe_netmail(&xnp, true);
          }
        }
      } else {
        bout.nl();
        bout << "The host is not listed in the network.\r\n";
        pausescr();
      }
    }
  }
  if (nn == -1 || nn >= size_int(a()->subs().sub(n).nets)) {
    // nn will be -1 when adding a new sub.
    a()->subs().sub(n).nets.push_back(xnp);
  } else {
    a()->subs().sub(n).nets[nn] = xnp;
  }
}

bool display_sub_categories() {
  if (!a()->current_net().sysnum) {
    return false;
  }

  TextFile ff(a()->network_directory(), CATEG_NET, "rt");
  if (!ff.IsOpen()) {
    return false;
  }
  bout.nl();
  bout << "Available sub categories are:\r\n";
  bool abort = false;
  string s;
  while (!abort && ff.ReadLine(&s)) {
    StringTrim(&s); 
    bout.bpla(s, &abort);
  }
  ff.Close();
  return true;
}
