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
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_LISTPLUS_H__
#define __INCLUDED_LISTPLUS_H__


#include "common.h"

#define EXTRA_SPACE

extern int foundany;

// This is the place the first file will be printed, which defaults to line
// 3, if you modify the file listing header, this will need to be changed
// depending on where the file will need to start after your modification
#define FIRST_FILE_POS 3

// How far from the bottom that the side menu will be on the screen
// This is used because of people who don't set there screenlines up correctly
// It defines how far up from the users screenlines to put the menu
#define STOP_LIST 0
#define MAX_EXTENDED_SIZE (1000)


// Defines for searching
#define SR_MATCH      0
#define SR_DONTMATCH  1

#define SR_OR         1
#define SR_AND        0

#define SR_NEWER      0
#define SR_OLDER      1
#define SR_EQUAL      2



// Defines for the sysop commands
#define SYSOP_DELETE 1
#define SYSOP_RENAME 2
#define SYSOP_MOVE   3


//
// File options
//

#define cfl_status_inactive         0x00000001

#define cfl_fname                   0x00000001
#define cfl_extension               0x00000002
#define cfl_dloads                  0x00000004
#define cfl_kbytes                  0x00000008
#define cfl_date_uploaded           0x00000010
#define cfl_file_points             0x00000020
#define cfl_days_old                0x00000040
#define cfl_upby                    0x00000080
#define cfl_times_a_day_dloaded     0x00000100
#define cfl_days_between_dloads     0x00000200
#define cfl_description             0x00000400
#define cfl_header                  0x80000000L

#pragma pack( push,  1)

struct listplus_config {
  long fi, lssm, sent;

  // Side menu colors  (fore+(back<<4))
  short int normal_highlight, normal_menu_item, current_highlight, current_menu_item;

  // Foreground only
  short int tagged_color, file_num_color;

  // Color for 'found' text when searching
  short int found_fore_color, found_back_color;

  // What file you are on, its color (fore+(back<<4))
  short int current_file_color;

  // if set to 0, will use the users info, otherwise it will show up to
  // this variable, no more
  short int max_screen_lines_to_show;

  // If users extended description setting is lower than this, it will force
  // this amount to show (good for people who leave it at 0)
  short int show_at_least_extended;

  // Toggles
  unsigned int edit_enable         : 1;
  unsigned int request_file        : 1;
  unsigned int colorize_found_text : 1;
  unsigned int search_extended_on  : 1;

  unsigned int simple_search       : 1;
  unsigned int no_configuration    : 1;
  unsigned int check_exist         : 1;
  unsigned int xxxxxxxxxxxxxxxxx   : 9;

  short int forced_config;
};

#pragma pack(pop)

extern struct listplus_config lp_config;
extern user_config        list_config;

extern int            list_loaded;  // 1 through ? or -1 through ? for sysop defined choices
extern int            lp_config_loaded;


#define STR_AND         '&'
#define STR_SPC         ' '
#define STR_OR          '|'
#define STR_NOT         '!'
#define STR_OPEN_PAREN  '('
#define STR_CLOSE_PAREN ')'


void printtitle_plus();
int  first_file_pos();
void print_searching(struct search_record * search_rec);
int  listfiles_plus(int type);
int  lp_add_batch(const char *file_name, int dn, long fs);
int  printinfo_plus(uploadsrec *upload_record, int filenum, int marked, int LinesLeft,
struct search_record * search_rec);
int  print_extended_plus(const char *file_name, int numlist, int indent, int color,
struct search_record * search_rec);
void show_fileinfo(uploadsrec *upload_record);
int  check_lines_needed(uploadsrec * upload_record);
int  prep_search_rec(struct search_record * search_rec, int type);
int  calc_max_lines();
void load_lp_config();
void save_lp_config();
void sysop_configure();
short SelectColor(int which);
void config_file_list();
void update_user_config_screen(uploadsrec * upload_record, int which);
void do_batch_sysop_command(int mode, const char *file_name);
int  search_criteria(struct search_record * sr);
void load_listing();
void view_file(const char *file_name);
int  lp_try_to_download(const char *file_mask, int dn);
void download_plus(const char *file_name);
void request_file(const char *file_name);
bool ok_listplus();

#endif  // __INCLUDED_LISTPLUS_H__
