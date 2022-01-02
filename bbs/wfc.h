/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_WFC_H
#define INCLUDED_BBS_WFC_H

#include "bbs/application.h"

void wfc_cls(Application* a);

namespace wwiv::bbs {

enum class local_logon_t { exit, prompt, fast };
enum class wfc_events_t { exit, login, login_fast };

class WFC {
public:
  explicit WFC(Application* a);
  virtual ~WFC();

  std::tuple<wfc_events_t, int> doWFCEvents();

private:
  std::tuple<local_logon_t, int> LocalLogon();
  void DrawScreen();
  void Clear();

  Application* a_{nullptr};
  int status_{0};
};

}  // namespace wwiv::bbs

#endif
