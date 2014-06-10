/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#ifndef _IFCNS_H_
#define _IFCNS_H_

#include <string>
#include "ivardec.h"
#include "vardec.h"

/* File: compile.cpp */
void get_descriptions(char *pth, char ***descrs, int *n, autosel_data **ad, int *nn);
int compile(char *infn, char *outfn, char *notes, int notelen, char *conf, unsigned *pnModemBaud);
int select_strings(char **strs, int count, int cur, int yt, int yb, int xl, int xr);

/* File: convert.cpp */
void c_setup();
void c_old_to_new();
bool c_IsUserListInOldFormat();
int c_check_old_struct();

/* File: init.cpp */

void load_modems();
int set_modem_info(const char *mt, bool bPause);
char *mdm_name(int mdm_num);
void select_modem();
void edit_net(int nn);
void networks();
void tweak_dir(char *s);
void convcfg();
void printcfg();
int verify_dir(char *typeDir, char *dirName);

/* File: init1.cpp */

void init();
void *malloca(unsigned long nbytes);
void pausescr();
void backspace();
int upcase(int ch);
int getkey();
int input_number(int max_digits);
void input_password(char *pszOutText, int nMaxLength);
int yn();
char onek(const char *s);
void editline(std::string* s, int len, int status, int *returncode, const char *ss);
void editline(char *s, int len, int status, int *returncode, const char *ss);
int toggleitem(int value, const char **strings, int num, int *returncode);
int number_userrecs();
int open_qscn();
void close_qscn();
void read_qscn(unsigned int un, unsigned long *qscn, int stayopen);
void write_qscn(unsigned int un, unsigned long *qscn, int stayopen);
int exist(const char *s);
void save_status();
char *date();
bool read_status();
int save_config();
void Puts( const char *pszText );
void nlx( int numLines = 1 );
void Printf( const char *pszFormat, ... );
void create_text(const char *fn);
void cvtx(unsigned short sp, char *rc);
void convert_result_codes();
void init_files();
void convert_modem_info(const char *fn);
void init_modem_info();
void new_init();
int verify_inst_dirs(configoverrec *co, int inst);
void read_user(unsigned int un, userrec *u);
void write_user(unsigned int un, userrec *u);
int GetNextSelectionPosition( int nMin, int nMax, int nCurrentPos, int nReturnCode );

/* File: init2.cpp */

void trimstr(char *s);
void trimstrpath(char *s);
void print_time(unsigned short t, char *s);
unsigned short get_time(char *s);
void sysinfo1();
void setpaths();
void setupcom();
int read_subs();
void write_subs();
void del_net(int nn);
void insert_net(int nn);
void convert_to(int num_subs, int num_dirs);
void up_subs_dirs();
void edit_lang(int nn);
void up_langs();
void edit_editor(int n);
void extrn_editors();
const char *prot_name(int pn);
void edit_prot(int n);
void extrn_prots();

/* File: init3.cpp */

void edit_arc(int nn);
void externals();
void up_str(char *s, int cursl, int an);
void pr_st(int cursl, int ln, int an);
void up_sl(int cursl);
void ed_slx(int *sln);
void sec_levs();
void list_autoval();
void edit_autoval(int n);
void autoval_levs();
void create_arcs();

// ** OSD CODE **

/* File: filesupp.cpp */
void WWIV_ChangeDirTo(const char *s);

#ifndef _WIN32
/* File: stringstuff.cpp */
char *strupr(char *s);

/* File: utility2.cpp */
long filelength(int handle);
#endif  // _WIN32


#endif
