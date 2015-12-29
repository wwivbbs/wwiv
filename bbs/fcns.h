/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
/**************************************************************************/
#ifndef __INCLUDED_FCNS_H__
#define __INCLUDED_FCNS_H__

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

#include "core/file.h"
#include "sdk/net.h"
#include "sdk/user.h"
#include "sdk/vardec.h"

// File: bbsutl.cpp
#include "bbs/bbsutl.h"

// File: bbsutl1.cpp
#include "bbs/bbsutl1.h"

// File: bbsutl2.cpp
#include "bbs/bbsutl2.h"

// File: bgetch.cpp
#include "bbs/bgetch.h"

// File: bputch.cpp
#include "bbs/bputch.h"

// File: com.cpp
#include "bbs/com.h"

// File: connect1.cpp
#include "bbs/connect1.h"

// File: diredit.cpp
void dlboardedit();


// File: dirlist.cpp
void dirlist(int mode);


// File: dupphone.cpp
void add_phone_number(int usernum, const char *phone);
void delete_phone_number(int usernum, const char *phone);
int  find_phone_number(const char *phone);

// File: execexternal.cpp
int ExecuteExternalProgram(const std::string& commandLine, int nFlags);

// File: finduser.cpp
int finduser(const std::string& searchString);
int finduser1(const std::string& searchString);

// File: gfiles.cpp
gfilerec *read_sec(int sn, int *nf);
void gfiles();

// File: gfledit.cpp
void modify_sec(int n);
void gfileedit();
bool fill_sec(int sn);

// File: hop.cpp
void HopSub();
void HopDir();

// File: inetmsg.cpp
void get_user_ppp_addr();
void send_inet_email();
bool check_inet_addr(const char *inetaddr);
char *read_inet_addr(char *internet_address, int user_number);
void write_inet_addr(const char *internet_address, int user_number);
void send_inst_sysstr(int whichinst, const char *send_string);

// File: lilo.cpp
bool IsPhoneNumberUSAFormat(wwiv::sdk::User* pUser);
void getuser();
void logon();
void logoff();
void logon_guest();

// File: memory.cpp
void *BbsAllocA(size_t lNumBytes);

// File: menuedit.cpp
void EditMenus();
void ListMenuDirs();

// File: misccmd.cpp
void kill_old_email();
void list_users(int mode);
void time_bank();
int  getnetnum(const char *network_name);
void uudecode(const char *input_filename, const char *output_filename);
void Packers();

// File: multmail.cpp
void multimail(int *user_number, int numu);
void slash_e();

// File: pause.cpp
void pausescr();

// File: quote.cpp
#include "bbs/quote.h"

// File: readmail.cpp
void readmail(int mode);
int  check_new_mail(int user_number);

// File: shortmsg.cpp
#include "bbs/shortmsg.h"

// File: showfiles.cpp
void show_files(const char *file_name, const char *pszDirectoryName);

// File: SmallRecord.cpp
void InsertSmallRecord(int user_number, const char *name);
void DeleteSmallRecord(const char *name);

// File: sr.cpp
#include "bbs/sr.h"

// File: srrcv.cpp
char modemkey(int *tout);
int  receive_block(char *b, unsigned char *bln, bool use_crc);
void xymodem_receive(const char *file_name, bool *received, bool use_crc);
void zmodem_receive(const std::string& filename, bool *received);

// File: srsend.cpp
void send_block(char *b, int block_type, bool use_crc, char byBlockNumber);
char send_b(File &file, long pos, int block_type, char byBlockNumber, bool *use_crc, const char *file_name,
            int *terr, bool *abort);
bool okstart(bool *use_crc, bool *abort);
void xymodem_send(const char *file_name, bool *sent, double *percent, bool use_crc, bool use_ymodem,
                  bool use_ymodemBatch);
void zmodem_send(const char *file_name, bool *sent, double *percent);

// File: subacc.cpp
#include "bbs/subacc.h"

// File: subedit.cpp
void boardedit();

// File: sublist.cpp
void old_sublist();
void SubList();

// File: subreq.cpp
void sub_xtr_del(int n, int nn, int f);
void sub_xtr_add(int n, int nn);
int  amount_of_subscribers(const char *pszNetworkFileName);

// File: syschat.cpp
void RequestChat();
void select_chat_name(char *sysop_name);
void two_way_chat(char *rollover, int max_length, bool crend, char *sysop_name);
void chat1(char *chat_line, bool two_way);


// File: sysoplog.cpp
#include "sysoplog.h"
// File: sysopf.cpp
#include "bbs/sysopf.h"

// File: user.cpp
bool okconf(wwiv::sdk::User *pUser);
void add_ass(int num_points, const char *reason);

// File: utility.cpp
#include "utility.h"

// File: wqscn.cpp
bool open_qscn();
void close_qscn();
void read_qscn(int user_number, uint32_t* qscn, bool stay_open, bool bForceRead = false);
void write_qscn(int user_number, uint32_t* qscn, bool stay_open);

// File: xfer.cpp
#include "bbs/xfer.h"

// File: xferovl.cpp
void move_file();
void sortdir(int directory_num, int type);
void sort_all(int type);
void rename_file();
bool maybe_upload(const char *file_name, int directory_num, const char *description);
void upload_files(const char *file_name, int directory_num, int type);
bool uploadall(int directory_num);
void relist();
void edit_database();
void modify_database(const char *file_name, bool add);
bool is_uploadable(const char *file_name);
void xfer_defaults();
void finddescription();
void arc_l();

// File: xferovl1.cpp
void modify_extended_description(char **sss, const char *dest);
bool valid_desc(const char *description);
bool get_file_idz(uploadsrec * upload_record, int dn);
int  read_idz_all();
int  read_idz(int mode, int tempdir);
void tag_it();
void tag_files();
int  add_batch(char *description, const char *file_name, int dn, long fs);
int  try_to_download(const char *file_mask, int dn);
void download();
char fancy_prompt(const char *pszPrompt, const char *pszAcceptChars);
void endlist(int mode);
void SetNewFileScanDate();
void removefilesnotthere(int dn, int *autodel);
void removenotthere();
int  find_batch_queue(const char *file_name);
void remove_batch(const char *file_name);

// File: xfertmp.cpp
bool bad_filename(const char *file_name);
// returns true if the file is downloaded.
bool download_temp_arc(const char *file_name, bool count_against_xfer_ratio);
void add_arc(const char *arc, const char *file_name, int dos);
void add_temp_arc();
void del_temp();
void list_temp_dir();
void temp_extract();
void list_temp_text();
void list_temp_arc();
void temporary_stuff();
void move_file_t();
void removefile();

#endif // __INCLUDED_FCNS_H__
