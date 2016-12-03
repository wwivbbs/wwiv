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
/**************************************************************************/
#ifndef __INCLUDED_FCNS_H__
#define __INCLUDED_FCNS_H__

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

#include "sdk/user.h"
#include "sdk/vardec.h"

// File: bbsutl.cpp
#include "bbs/bbsutl.h"

// File: memory.cpp
void *BbsAllocA(size_t lNumBytes);

// File: menuedit.cpp
void EditMenus();
void ListMenuDirs();

// File: multmail.cpp
void multimail(int *user_number, int numu);
void slash_e();

// File: pause.cpp

// File: readmail.cpp
void readmail(int mode);
int  check_new_mail(int user_number);

// File: shortmsg.cpp
#include "bbs/shortmsg.h"

// File: showfiles.cpp
void show_files(const char *file_name, const char *pszDirectoryName);

// File: sr.cpp
#include "bbs/sr.h"

// File: srrcv.cpp
char modemkey(int *tout);
int  receive_block(char *b, unsigned char *bln, bool use_crc);
void xymodem_receive(const char *file_name, bool *received, bool use_crc);
void zmodem_receive(const std::string& filename, bool *received);

// File: subacc.cpp
#include "bbs/subacc.h"

// File: sublist.cpp
void old_sublist();
void SubList();

// File: subreq.cpp
void sub_xtr_del(int n, int nn, int f);
void sub_xtr_add(int n, int nn);

// File: syschat.cpp
void RequestChat();
void select_chat_name(char *sysop_name);
void two_way_chat(char *rollover, int max_length, bool crend, char *sysop_name);
void chat1(const char *chat_line, bool two_way);


// File: sysopf.cpp
#include "bbs/sysopf.h"

// File: user.cpp
bool okconf(wwiv::sdk::User *pUser);

// File: utility.cpp
#include "utility.h"

// File: wqscn.cpp
#include "bbs/wqscn.h"

// File: xfer.cpp
#include "bbs/xfer.h"

// File: xferovl.cpp
#include "bbs/xferovl.h"

// File: xferovl1.cpp
#include "bbs/xferovl1.h"

// File: xfertmp.cpp
#include "bbs/xfertmp.h"

#endif // __INCLUDED_FCNS_H__
