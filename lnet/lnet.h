/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*          Copyright (C)2022, WWIV Software Services                     */
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
/**************************************************************************/
#ifndef INCLUDED_NET_LNET_H
#define INCLUDED_NET_LNET_H

#include "local_io/local_io.h"
#include "net_core/net_cmdline.h"
#include "sdk/net/legacy_net.h"

class LNet final {
public:
  LNet(const wwiv::net::NetworkCommandLine& cmdline);
  ~LNet();

  int Run();

private:
  void pausescr();
  void dump_char(char ch);
  void show_help();
  void show_header(const net_header_rec& nh, int current, double percent);


  const wwiv::net::NetworkCommandLine& net_cmdline_;
  wwiv::local::io::LocalIO* io{nullptr};
  int curli_{0};
};

#endif // INCLUDED_NET_LNET_H