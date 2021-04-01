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
#include "bbs/subreq.h"

#include "bbs/bbs.h"
#include "bbs/connect1.h"
#include "bbs/email.h"
#include "bbs/mmkey.h"
#include "bbs/utility.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/net/networks.h"
#include "sdk/net/subscribers.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void maybe_netmail(subboard_network_data_t* ni, bool bAdd) {
  bout << "|#5Send email request to the host now? ";
  if (bin.yesno()) {
    auto title = StrCat("Sub type ", ni->stype);
    if (bAdd) {
      title += " - ADD request";
    } else {
      title += " - DROP request";
    }
    set_net_num(ni->net_num);
    email(title, 1, ni->host, false, 0);
  }
}

static bool display_sub_categories(const Network& net) {
  if (!net.sysnum) {
    return false;
  }

  TextFile ff(FilePath(net.dir, CATEG_NET), "rt");
  if (!ff.IsOpen()) {
    return false;
  }
  bout.nl();
  bout << "Available sub categories are:\r\n";
  auto abort = false;
  std::string s;
  while (!abort && ff.ReadLine(&s)) {
    StringTrim(&s);
    bout.bpla(s, &abort);
  }
  ff.Close();
  return true;
}

static void sub_req(uint16_t main_type, int tosys, const std::string& stype,
                    const Network& net) {
  net_header_rec nh{};

  nh.tosys = static_cast<uint16_t>(tosys);
  nh.touser = 1;
  nh.fromsys = net.sysnum;
  nh.fromuser = 1;
  nh.main_type = main_type;
  // always use 0 since we use the "stype" field
  nh.minor_type = 0;
  nh.list_len = 0;
  nh.daten = daten_t_now();
  nh.method = 0;
  // This is an alphanumeric sub type.
  auto text = stype;
  text.push_back(0);
  nh.length = size_uint32(text);
  send_net(&nh, {}, text, "");

  bout.nl();
  if (main_type == main_type_sub_add_req) {
    bout << "Automated add request sent to @" << tosys << wwiv::endl;
  } else {
    bout << "Automated drop request sent to @" << tosys << wwiv::endl;
  }
  bout.pausescr();
}

static constexpr short OPTION_AUTO = 0x0001;
static constexpr short OPTION_NO_TAG = 0x0002;
static constexpr short OPTION_GATED = 0x0004;
static constexpr short OPTION_NETVAL = 0x0008;
static constexpr short OPTION_ANSI = 0x0010;

static int find_hostfor(const Network& net, const std::string& type, short* ui,
                        char* description, short* opt) {
  int rc = 0;

  if (description) {
    *description = 0;
  }
  *opt = 0;

  bool done = false;
  for (int i = 0; i < 256 && !done; i++) {
    std::filesystem::path fn;
    if (i) {
      fn = FilePath(net.dir, StrCat(SUBS_NOEXT, ".", i));
    } else {
      fn = FilePath(net.dir, StrCat(SUBS_LST));
    }
    TextFile file(fn, "r");
    if (!file) {
      return rc;
    }
    char s[255];
    while (!done && file.ReadLine(s, 160)) {
      if (s[0] <= ' ') {
        continue;
      }
      auto* ss = strtok(s, " \r\n\t");
      if (!ss) {
        continue;
      }
      if (!iequals(ss, type)) {
        continue;
      }
      ss = strtok(nullptr, " \r\n\t");
      if (!ss) {
        continue;
      }
      auto h = to_number<short>(ss);
      short o = 0;
      ss = strtok(nullptr, "\r\n");
      if (!ss) {
        continue;
      }
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
        if (bin.yesno()) {
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

  return rc;
}

void sub_xtr_del(int n, int nn, int f) {
  // make a copy of the old network info.
  auto xn = a()->subs().sub(n).nets[nn];

  if (f) {
    erase_at(a()->subs().sub(n).nets, nn);
  }
  set_net_num(xn.net_num);
  const auto& net = a()->nets().at(xn.net_num);

  if (xn.host != 0 && valid_system(xn.host)) {
    short opt;
    const auto ok = find_hostfor(net, xn.stype, &xn.host, nullptr, &opt);
    if (ok) {
      if (opt & OPTION_AUTO) {
        bout << "|#5Attempt automated drop request? ";
        if (bin.yesno()) {
          sub_req(main_type_sub_drop_req, xn.host, xn.stype, net);
        }
      } else {
        maybe_netmail(&xn, false);
      }
    } else {
      bout << "|#5Attempt automated drop request? ";
      if (bin.yesno()) {
        sub_req(main_type_sub_drop_req, xn.host, xn.stype, net);
      } else {
        maybe_netmail(&xn, false);
      }
    }
  }
}

bool sub_xtr_add(int n, int nn) {
  // nn may be -1
  while (nn >= ssize(a()->subs().sub(n).nets)) {
    a()->subs().sub(n).nets.push_back({});
  }
  subboard_network_data_t xnp = {};
  // Start with the first network number, only ask if
  // we have more than 1 network to choose from.
  int network_number = 0;
  const auto num_nets = wwiv::stl::ssize(a()->nets());
  if (num_nets == 0) {
    LOG(ERROR) << "Called sub_xtr_add when no networks defined.";
    return false;
  }

  if (num_nets > 1) {
    std::set<char> odc;
    char onx[20];
    onx[0] = 'Q';
    onx[1] = 0;
    auto onxi = 1;
    bout.nl();
    for (auto ii = 0; ii < wwiv::stl::ssize(a()->nets()); ii++) {
      if (ii < 9) {
        onx[onxi++] = static_cast<char>(ii + '1');
        onx[onxi] = 0;
      } else {
        auto odci = (ii + 1) / 10;
        odc.insert(static_cast<char>(odci + '0'));
      }
      bout << "(" << ii + 1 << ") " << a()->nets()[ii].name << wwiv::endl;
    }
    bout << "Q. Quit\r\n\n";
    bout << "|#2Which network (number): ";
    if (wwiv::stl::ssize(a()->nets()) < 9) {
      auto ch = onek(onx);
      if (ch == 'Q') {
        return false;
      }
      network_number = ch - '1';
    } else {
      auto mmk = mmkey(odc);
      if (mmk == "Q") {
        return false;
      }
      network_number = to_number<int>(mmk) - 1;
    }
    if (network_number >= 0 && network_number < wwiv::stl::ssize(a()->nets())) {
      set_net_num(network_number);
    } else {
      return false;
    }
  }
  xnp.net_num = static_cast<int16_t>(network_number);
  const auto& net = a()->nets()[network_number];

  bout.nl();
  auto stype_len = 7;
  if (net.type == network_type_t::ftn) {
    bout << "|#2What echomail area: ";
    stype_len = 40;
  } else {
    bout << "|#2What sub type? ";
  }
  xnp.stype = bin.input(stype_len, true);
  if (xnp.stype.empty()) {
    return false;
  }

  bool is_hosting = false;
  if (net.type == network_type_t::wwivnet || net.type == network_type_t::internet ||
      net.type == network_type_t::news) {
    bout << "|#5Will you be hosting the sub? ";
    is_hosting = bin.yesno();
  }

  if (is_hosting) {
    auto file_name = StrCat(net.dir, "n", xnp.stype, ".net");
    File file(file_name);
    if (file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      file.Close();
    }

    bout << "|#5Allow auto add/drop requests? ";
    if (bin.noyes()) {
      xnp.flags |= XTRA_NET_AUTO_ADDDROP;
    }

    bout << "|#5Make this sub public (in subs.lst)?";
    if (bin.noyes()) {
      xnp.flags |= XTRA_NET_AUTO_INFO;
      if (display_sub_categories(net)) {
        auto gc = 0;
        while (!gc) {
          bout.nl();
          bout << "|#2Which category is this sub in (0 for unknown/misc)? ";
          auto s = bin.input(3);
          auto i = to_number<uint16_t>(s);
          if (i || s == "0") {
            TextFile ff(FilePath(net.dir, CATEG_NET), "rt");
            while (ff.ReadLine(&s)) {
              int i1 = to_number<uint16_t>(s);
              if (i1 == i) {
                gc = 1;
                xnp.category = i;
                break;
              }
            }
            file.Close();
            if (s == "0") {
              gc = 1;
            } else if (!xnp.category) {
              bout << "Illegal/invalid category.\r\n\n";
            }
          } else {
            if (s.size() == 1 && s.front() == '?') {
              display_sub_categories(net);
              continue;
            }
          } 
        }
      }
    }
  } else if (net.type == network_type_t::ftn) {
    // Set the fake fido node up as the host.
    xnp.host = FTN_FAKE_OUTBOUND_NODE;
    const auto sub_file_name = FilePath(net.dir, StrCat("n", xnp.stype, ".net"));

    auto addresses = ReadFidoSubcriberFile(sub_file_name);
    bool done = false;
    do {
      a()->CheckForHangup();
      bout.nl();
      bout << "|#2Which FTN system (address) is the host? ";
      const auto host = bin.input_text(20);
      try {
        fido::FidoAddress a(host);
        addresses.insert(a);
        if (!WriteFidoSubcriberFile(sub_file_name, addresses)) {
          bout << "ERROR: Unable to add host to subscriber file";
        }
        done = true;
      } catch (const fido::bad_fidonet_address& e) {
        bout << "Bad Address: " << e.what();
      }
    } while (!done && !a()->sess().hangup());

  } else {
    char description[100];
    short opt;
    auto ok = find_hostfor(net, xnp.stype, &(xnp.host), description, &opt);

    if (!ok) {
      bout.nl();
      bout << "|#2Which system (number) is the host? ";
      bin.input(description, 6);
      xnp.host = to_number<uint16_t>(description);
      description[0] = '\0';
    }
    if (!a()->subs().sub(n).desc[0]) {
      a()->subs().sub(n).desc = description;
    }

    if (xnp.host == net.sysnum) {
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
            if (bin.yesno()) {
              sub_req(main_type_sub_add_req, xnp.host, xnp.stype, net);
            }
          } else {
            maybe_netmail(&xnp, true);
          }
        } else {
          bout.nl();
          bout << "|#5Attempt automated add request? ";
          auto bTryAutoAddReq = bin.yesno();
          if (bTryAutoAddReq) {
            sub_req(main_type_sub_add_req, xnp.host, xnp.stype, net);
          } else {
            maybe_netmail(&xnp, true);
          }
        }
      } else {
        bout.nl();
        bout << "The host is not listed in the network.\r\n";
        bout.pausescr();
      }
    }
  }
  if (nn == -1 || nn >= ssize(a()->subs().sub(n).nets)) {
    // nn will be -1 when adding a new sub.
    a()->subs().sub(n).nets.push_back(xnp);
  } else {
    a()->subs().sub(n).nets[nn] = xnp;
  }
  return true;
}
