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
#include "bbs/lpfunc.h"

#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl.h"
#include "bbs/common.h"
#include "bbs/listplus.h"
#include "common/pause.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "common/output.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/files/files.h"
#include <string>
#include <vector>

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Local function prototypes
int  compare_criteria(search_record * sr, uploadsrec * ur);
bool lp_compare_strings(const char *raw, const char *formula);
bool lp_compare_strings_wh(const char *raw, const char *formula, unsigned *pos, int size);
int  lp_get_token(const char *formula, unsigned *pos);
int  lp_get_value(const char *raw, const char *formula, unsigned *pos);

// These are defined in listplus.cpp
extern int bulk_move;
extern bool ext_is_on;

static void drawfile(int filepos, int filenum) {
  bout.clear_lines_listed();
  bout.PutsXYSC(4, filepos + first_file_pos(), lp_config.current_file_color, fmt::format("{:>3}|#0", filenum));
  bout.GotoXY(4, filepos + first_file_pos());
}

static void undrawfile(int filepos, int filenum) {
  bout.clear_lines_listed();
  bout.PutsXYSC(4, filepos + first_file_pos(), lp_config.file_num_color, fmt::format("{:>3}|#0", filenum));
}

static void prep_menu_items(vector<string>* menu_items) {
  menu_items->push_back("Next");
  menu_items->push_back("Prev");
  menu_items->push_back("Tag");
  menu_items->push_back("Info");
  menu_items->push_back("ViewZip");

  if (a()->sess().using_modem()) {
    menu_items->push_back("Dload");
  } else {
    menu_items->push_back("Move");
  }

  menu_items->push_back("+Dir");
  menu_items->push_back("-Dir");
  menu_items->push_back("Full-Desc");
  menu_items->push_back("Quit");
  menu_items->push_back("?");
  if (so()) {
    menu_items->push_back("Sysop");
  }
}

static void load_listing() {
  a()->user()->data.lp_options |= cfl_fname;
  a()->user()->data.lp_options |= cfl_description;

  for (auto& lp_color : a()->user()->data.lp_colors) {
    if (lp_color == 0) {
      lp_color = 7;
    }
  }
}

int listfiles_plus_function(int type) {
  int file_handle[51];
  char vert_pos[51];
  int file_pos = 0, save_file_pos = 0, menu_pos = 0;
  size_t save_dir = a()->current_user_dir_num();
  bool sysop_mode = false;
  side_menu_colors smc{};
  search_record search_rec = {};

  load_lp_config();

  smc.normal_highlight = lp_config.normal_highlight;
  smc.normal_menu_item = lp_config.normal_menu_item;
  smc.current_highlight = lp_config.current_highlight;
  smc.current_menu_item = lp_config.current_menu_item;

  load_listing();

  vector<string> menu_items;
  prep_menu_items(&menu_items);

  std::vector<uploadsrec> file_recs(a()->user()->GetScreenLines() + 20);
  if (!prep_search_rec(&search_rec, type)) {
    return 0;
  }

  auto max_lines = calc_max_lines();
  auto all_done = false;

  for (uint16_t this_dir = 0; this_dir < a()->udir.size() && !a()->sess().hangup() && !all_done;
       this_dir++) {
    int also_this_dir = a()->udir[this_dir].subnum;
    auto scan_dir = false;
    bin.checka(&all_done);

    if (search_rec.alldirs == THIS_DIR) {
      if (this_dir == save_dir) {
        scan_dir = true;
      }
    } else {
      if (a()->sess().qsc_n[also_this_dir / 32] & (1L << (also_this_dir % 32))) {
        scan_dir = true;
      }

      if (search_rec.alldirs == ALL_DIRS && (type != LP_NSCAN_NSCAN)) {
        scan_dir = true;
      }
    }

    int save_first_file = 0;
    if (scan_dir) {
      a()->set_current_user_dir_num(this_dir);
      dliscan();
      int first_file = save_first_file = 1;
      int amount = 0;
      bool done = false;
      int matches = 0;
      int lines = 0;
      int changedir = 0;

      while (!done && !a()->sess().hangup() && !all_done) {
        bin.checka(&all_done);
        if (!amount) {
          print_searching(&search_rec);
        }
        if (a()->current_file_area()->number_of_files()) {
          changedir = 0;
          bool force_menu = false;
          auto f = a()->current_file_area()->ReadFile(first_file + amount);
          file_recs[matches] = f.u();
          if (compare_criteria(&search_rec, &file_recs[matches])) {
            int lines_left = max_lines - lines;
            int needed = check_lines_needed(&file_recs[matches]);
            if (needed <= lines_left) {
              if (!matches) {
                printtitle_plus();
              }
              file_handle[matches] = first_file + amount;
              vert_pos[matches] = static_cast<char>(lines);
              auto marked = a()->batch().contains_file(file_recs[matches].filename);
              int lines_used = printinfo_plus(&file_recs[matches], file_handle[matches], marked,
                                              lines_left, &search_rec);
              if (lines_used > 1 && lines_used < lines_left) {
                bout.nl();
                ++lines_used;
              }
              lines += lines_used;
              ++matches;
            } else {
              force_menu = true;
            }
          }
          if (!force_menu) {
            ++amount;
          }

          if (lines >= max_lines ||
              a()->current_file_area()->number_of_files() < first_file + amount || force_menu) {
            if (matches) {
              file_pos = save_file_pos;
              drawfile(vert_pos[file_pos], file_handle[file_pos]);
              bool redraw = true;
              save_file_pos = 0;
              bool menu_done = false;
              while (!menu_done && !a()->sess().hangup()) {
                int command = side_menu(&menu_pos, redraw, menu_items, 2, max_lines + first_file_pos() + 1, &smc);
                redraw = true;
                bulk_move = 0;
                bout.Color(0);
                if (do_sysop_command(command)) {
                  menu_done = true;
                  amount = lines = matches = 0;
                  save_file_pos = file_pos;
                }
                if (command == COMMAND_PAGEUP) {
                  command = EXECUTE;
                  menu_pos = 1;
                }
                if (command == COMMAND_PAGEDN) {
                  command = EXECUTE;
                  menu_pos = 0;
                }
                switch (command) {
                case CX:
                case AX:
                  goto TOGGLE_EXTENDED;
                case '?':
                case CO:
                  bout.cls();
                  bout.print_help_file(LISTPLUS_HLP);
                  bout.pausescr();
                  menu_done = true;
                  amount = lines = matches = 0;
                  save_file_pos = file_pos;
                  break;
                case COMMAND_DOWN:
                  undrawfile(vert_pos[file_pos], file_handle[file_pos]);
                  ++file_pos;
                  if (file_pos >= matches) {
                    file_pos = 0;
                  }
                  drawfile(vert_pos[file_pos], file_handle[file_pos]);
                  redraw = false;
                  break;
                case COMMAND_UP:
                  undrawfile(vert_pos[file_pos], file_handle[file_pos]);
                  if (!file_pos) {
                    file_pos = matches - 1;
                  } else {
                    --file_pos;
                  }
                  drawfile(vert_pos[file_pos], file_handle[file_pos]);
                  redraw = false;
                  break;
                case SPACE:
                  goto ADD_OR_REMOVE_BATCH;
                case EXECUTE:
                  switch (menu_pos) {
                  case 0:
                    save_first_file = first_file;
                    first_file += amount;
                    if (first_file > a()->current_file_area()->number_of_files()) {
                      done = true;
                    }
                    menu_done = true;
                    amount = lines = matches = 0;
                    break;
                  case 1:
                    if (save_first_file >= first_file) {
                      if (first_file > 5) {
                        first_file -= 5;
                      } else {
                        first_file = 1;
                      }
                    } else {
                      first_file = save_first_file;
                    }
                    menu_done = true;
                    amount = lines = matches = 0;
                    break;
                  case 2:
                    if (sysop_mode) {
                      do_batch_sysop_command(SYSOP_DELETE, file_recs[file_pos].filename);
                      menu_done = true;
                      save_file_pos = file_pos = 0;
                      amount = lines = matches = 0;
                    } else {
ADD_OR_REMOVE_BATCH:
                      if (a()->batch().FindBatch(file_recs[file_pos].filename) > -1) {
                        a()->batch().RemoveBatch(file_recs[file_pos].filename);
                        redraw = false;
                      }
                      else if (!ratio_ok() && !sysop_mode) {
                        menu_done = true;
                        amount = lines = matches = 0;
                        save_file_pos = file_pos;
                        bout.pausescr();
                      } else {
                        redraw = false;
                        if (!(a()->dirs()[a()->current_user_dir().subnum].mask & mask_cdrom) && !sysop_mode) {
                          auto tf =
                              FilePath(a()->dirs()[a()->current_user_dir().subnum].path,
                                             wwiv::sdk::files::unalign(file_recs[file_pos].filename));
                          if (sysop_mode || !a()->sess().using_modem() || File::Exists(tf)) {
                            lp_add_batch(file_recs[file_pos].filename, a()->current_user_dir().subnum,
                                         file_recs[file_pos].numbytes);
                          } else if (lp_config.request_file) {
                            menu_done = true;
                            amount = lines = matches = 0;
                            request_file(file_recs[file_pos].filename);
                          }
                        } else {
                          lp_add_batch(file_recs[file_pos].filename, a()->current_user_dir().subnum,
                                       file_recs[file_pos].numbytes);
                        }
                      }
                      bout.GotoXY(1, first_file_pos() + vert_pos[file_pos]);
                      bout.bprintf(
                          "|%02d %c ", lp_config.tagged_color,
                          a()->batch().contains_file(file_recs[file_pos].filename) ? '\xFE' : ' ');
                      undrawfile(vert_pos[file_pos], file_handle[file_pos]);
                      ++file_pos;
                      if (file_pos >= matches) {
                        file_pos = 0;
                      }
                      drawfile(vert_pos[file_pos], file_handle[file_pos]);
                      redraw = false;
                    }
                    break;
                  case 3:
                    if (!sysop_mode) {
                      show_fileinfo(&file_recs[file_pos]);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    } else {
                      do_batch_sysop_command(SYSOP_RENAME, file_recs[file_pos].filename);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    }
                    menu_pos = 0;
                    break;
                  case 4:
                    if (!sysop_mode) {
                      view_file(file_recs[file_pos].filename);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    } else {
                      do_batch_sysop_command(SYSOP_MOVE, file_recs[file_pos].filename);
                      menu_done = true;
                      save_file_pos = file_pos = 0;
                      amount = lines = matches = 0;
                    }
                    menu_pos = 0;
                    break;
                  case 5:
                    if (!sysop_mode && a()->sess().using_modem()) {
                      bout.cls();
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                      if (!ratio_ok()) {
                        bout.pausescr();
                      } else {
                        if (!ratio_ok()  && !sysop_mode) {
                          menu_done = true;
                          amount = lines = matches = 0;
                          save_file_pos = file_pos;
                          bout.pausescr();
                        } else {
                          redraw = false;
                          if (!(a()->dirs()[a()->current_user_dir().subnum].mask & mask_cdrom) && !sysop_mode) {
                            auto tf =
                                FilePath(a()->dirs()[a()->current_user_dir().subnum].path,
                                             wwiv::sdk::files::unalign(file_recs[file_pos].filename));
                            if (sysop_mode || !a()->sess().using_modem() || File::Exists(tf)) {
                              lp_add_batch(file_recs[file_pos].filename, a()->current_user_dir().subnum,
                                           file_recs[file_pos].numbytes);
                            } else if (lp_config.request_file) {
                              menu_done = true;
                              amount = lines = matches = 0;
                              request_file(file_recs[file_pos].filename);
                            }
                          } else {
                            lp_add_batch(file_recs[file_pos].filename, a()->current_user_dir().subnum,
                                         file_recs[file_pos].numbytes);
                          }
                          download_plus(file_recs[file_pos].filename);
                        }
                      }
                      dliscan();
                    } else if (!sysop_mode) {
                      do_batch_sysop_command(SYSOP_MOVE, file_recs[file_pos].filename);
                      menu_done = true;
                      save_file_pos = file_pos = 0;
                      amount = lines = matches = 0;
                    } else {
                      sysop_configure();
                      smc.normal_highlight = lp_config.normal_highlight;
                      smc.normal_menu_item = lp_config.normal_menu_item;
                      smc.current_highlight = lp_config.current_highlight;
                      smc.current_menu_item = lp_config.current_menu_item;
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    }
                    menu_pos = 0;
                    break;
                  case 6:
                    menu_done = true;
                    amount = lines = matches = 0;
                    first_file = 1;
                    changedir = 1;
                    if (a()->current_user_dir_num() < a()->dirs().size() - 1
                        && a()->udir[a()->current_user_dir_num() + 1].subnum >= 0) {
                      a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
                      ++this_dir;
                    } else {
                      a()->set_current_user_dir_num(0);
                      this_dir = 0;
                    }
                    if (!type) {
                      save_dir = a()->current_user_dir_num();
                    }
                    dliscan();
                    menu_pos = 0;
                    break;
                  case 7:
                    menu_done = true;
                    amount = lines = matches = 0;
                    first_file = 1;
                    changedir = -1;
                    if (a()->current_user_dir_num() > 0) {
                      a()->set_current_user_dir_num(a()->current_user_dir_num() - 1);
                      --this_dir;
                    } else {
                      while ((a()->udir[a()->current_user_dir_num() + 1].subnum >= 0)
                             && (a()->current_user_dir_num() < a()->dirs().size() - 1)) {
                        a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
                      }
                      this_dir = a()->current_user_dir_num();
                    }
                    if (!type) {
                      save_dir = a()->current_user_dir_num();
                    }
                    dliscan();
                    menu_pos = 0;
                    break;
                  case 8:
TOGGLE_EXTENDED:
                    ext_is_on = !ext_is_on;
                    a()->user()->SetFullFileDescriptions(!a()->user()->GetFullFileDescriptions());
                    menu_done = true;
                    amount = lines = matches = 0;
                    file_pos = 0;
                    save_file_pos = file_pos;
                    menu_pos = 0;
                    break;
                  case 9:
                    menu_done = true;
                    done = true;
                    amount = lines = matches = 0;
                    all_done = true;
                    bout.clear_lines_listed();
                    break;
                  case 10:
                    bout.cls();
                    bout.print_help_file(LISTPLUS_HLP);
                    bout.pausescr();
                    menu_done = true;
                    amount = lines = matches = 0;
                    save_file_pos = file_pos;
                    break;
                  case 11:
                    if (so() && !sysop_mode) {
                      sysop_mode = true;
                      menu_items[2] =  "Delete";
                      menu_items[3] = "Rename";
                      menu_items[4] = "Move";
                      menu_items[5] = "Config";
                      menu_items[11] = "Back";
                    } else {
                      sysop_mode = false;
                      prep_menu_items(&menu_items);
                    }
                    bout.bputch('\r');
                    bout.clreol();
                    break;
                  }
                  break;
                case GET_OUT:
                  menu_done = true;
                  done = true;
                  all_done = true;
                  amount = lines = matches = 0;
                  break;
                }
              }
            }
            else {
              if (!changedir) {
                done = true;
              } else if (changedir == 1) {
                if ((a()->current_user_dir_num() < a()->dirs().size() - 1)
                    && (a()->udir[a()->current_user_dir_num() + 1].subnum >= 0)) {
                  a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
                } else {
                  a()->set_current_user_dir_num(0);
                }
                dliscan();
              } else {
                if (a()->current_user_dir_num() > 0) {
                  a()->set_current_user_dir_num(a()->current_user_dir_num() - 1);
                } else {
                  while ((a()->udir[a()->current_user_dir_num() + 1].subnum >= 0)
                         && (a()->current_user_dir_num() < a()->dirs().size() - 1)) {
                    a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
                  }
                }
                dliscan();
              }
            }
          }
        } else {
          if (!changedir) {
            done = true;
          } else if (changedir == 1) {
            if (a()->current_user_dir_num() < a()->dirs().size() - 1
                && a()->udir[a()->current_user_dir_num() + 1].subnum >= 0) {
              a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
            } else {
              a()->set_current_user_dir_num(0);
            }
            dliscan();
          } else {
            if (a()->current_user_dir_num() > 0) {
              a()->set_current_user_dir_num(a()->current_user_dir_num() - 1);
            } else {
              while ((a()->udir[a()->current_user_dir_num() + 1].subnum >= 0)
                     && (a()->current_user_dir_num() < a()->dirs().size() - 1)) {
                a()->set_current_user_dir_num(a()->current_user_dir_num() + 1);
              }
            }
            dliscan();
          }
        }
      }
    }
  }

  return all_done ? 1 : 0;
}

static constexpr char STR_AND = '&';
static constexpr char STR_SPC = ' ';
static constexpr char STR_OR = '|';
static constexpr char STR_NOT = '!';
static constexpr char STR_OPEN_PAREN = '(';
static constexpr char STR_CLOSE_PAREN = ')';

int compare_criteria(search_record * sr, uploadsrec * ur) {
  // "        .   "
  if (sr->filemask != "        .   ") {
    if (!wwiv::sdk::files::aligned_wildcard_match(sr->filemask, ur->filename)) {
      return 0;
    }
  }
  // the above test was passed if it got here


  if (sr->nscandate) {
    if (ur->daten < sr->nscandate) {
      return 0;
    }
  }

  // the above test was passed if it got here
  if (!sr->search[0]) {
    // this is return 1 because top two tests were passed
    // and we are not searching on text, so it is assume to
    // have passed the test
    return 1;                                 
  }
  // we want to search the filename, description and ext description
  // as one unit, that way, if you specify something like 'one & two
  // and one is located in the description and two is in the
  // extended description, then it will properly find the search
  string buff;
  if (sr->search_extended && ur->mask & mask_extended) {
    buff = a()->current_file_area()->ReadExtendedDescriptionAsString(ur->filename).value_or("");
  }

  if (buff.empty()) {
    // Something had something.
    return 0;
  }

  // tag the file name and description on to the end of the extended
  // description (if there is one)
  buff += StrCat(" ", ur->filename, " ", ur->description);

  if (lp_compare_strings(buff.c_str(), sr->search.c_str())) {
    return 1;
  }

  // failed search test, so exit with 0
  return 0;
}

bool lp_compare_strings(const char *raw, const char *formula) {
  unsigned i = 0;

  return lp_compare_strings_wh(raw, formula, &i, ssize(formula));
}

bool lp_compare_strings_wh(const char *raw, const  char *formula, unsigned *pos, int size) {
  bool rvalue;

  auto lvalue = lp_get_value(raw, formula, pos) ? true : false;
  while (*pos < static_cast<unsigned>(size)) {
    const auto token = lp_get_token(formula, pos);

    switch (token) {
    case STR_SPC:                         // Added
    case STR_AND:
      rvalue = lp_compare_strings_wh(raw, formula, pos, size);
      lvalue = lvalue && rvalue;
      break;

    case STR_OR:
      rvalue = lp_compare_strings_wh(raw, formula, pos, size);
      lvalue = lvalue || rvalue;
      break;

    case STR_CLOSE_PAREN:
    case 0:
      return lvalue;

    default:
      return lvalue;
    }
  }
  return lvalue;
}

int lp_get_token(const char *formula, unsigned *pos) {
  auto tpos = 0;

  while (formula[*pos] && isspace(formula[*pos])) {
    ++* pos;
  }

  if (isalpha(formula[*pos])) {
    // remove isspace to split by word basis
    while (isalnum(formula[*pos]) || isspace(formula[*pos])) {
      ++tpos;
      ++*pos;
    }
  }
  ++*pos;
  return formula[*pos - 1];
}

int lp_get_value(const char *raw, const char *formula, unsigned *pos) {
  char szBuffer[255];
  int tpos = 0;
  int sign = 1, started_number = 0;

  while (formula[*pos] && isspace(formula[*pos])) {
    ++* pos;
  }
  int x = formula[*pos];

OPERATOR_CHECK_1:
  switch (x) {
  case STR_NOT:
    sign = !sign;
    ++*pos;
    if (formula[*pos]) {
      x = formula[*pos];
      goto OPERATOR_CHECK_1;
    }
    return 0;

  case STR_AND:
  case STR_SPC:
  case STR_OR:
    return 0;
  }

  switch (x) {
  case STR_OPEN_PAREN:
    ++*pos;
    if (lp_compare_strings_wh(raw, formula, pos, ssize(formula))) {
      return (sign) ? 1 : 0;
    } else {
      return (sign) ? 0 : 1;
    }
  }

  bool done = false;
  while (!done && formula[*pos]) {
    x = formula[*pos];
    switch (x) {
    case STR_NOT:
      if (started_number) {
        done = true;
        break;
      }
      sign = !sign;
      ++*pos;
      continue;
    case STR_AND:
    case STR_SPC:
    case STR_OR:
    case STR_OPEN_PAREN:
    case STR_CLOSE_PAREN: {
      done = true;
      break;
    }
    default:
      started_number = 1;
      szBuffer[tpos] = static_cast<char>(x);
      ++tpos;
      ++*pos;
      break;
    }
  }
  szBuffer[tpos] = 0;
  StringTrim(szBuffer);

  if (ifind_first(raw, szBuffer)) {
    return (sign ? 1 : 0);
  }
  return (sign ? 0 : 1);
}


