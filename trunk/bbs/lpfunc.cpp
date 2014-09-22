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
#include "common.h"
#include "listplus.h"
#include "printfile.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"

//
// Local function prototypes
//
int  compare_criteria(struct search_record * sr, uploadsrec * ur);
bool lp_compare_strings(char *raw, char *formula);
bool lp_compare_strings_wh(char *raw, char *formula, unsigned *pos, int size);
int  lp_get_token(char *formula, unsigned *pos);
int  lp_get_value(char *raw, char *formula, unsigned *pos);


//
// These are defined in listplus.cpp
//
extern int bulk_move;
extern bool ext_is_on;


int listfiles_plus_function(int type) {
  uploadsrec(*file_recs)[1];
  int file_handle[51];
  char vert_pos[51];
  int file_pos = 0, save_file_pos = 0, menu_pos = 0;
  int save_dir = GetSession()->GetCurrentFileArea();
  bool sysop_mode = false;
  struct side_menu_colors smc;
  struct search_record search_rec;

  load_lp_config();

  smc.normal_highlight = lp_config.normal_highlight;
  smc.normal_menu_item = lp_config.normal_menu_item;
  smc.current_highlight = lp_config.current_highlight;
  smc.current_menu_item = lp_config.current_menu_item;

  load_listing();

  char **menu_items = static_cast<char **>(BbsAlloc2D(20, 15, sizeof(char)));
  if (!menu_items) {
    return 0;
  }

  prep_menu_items(menu_items);

  file_recs = (uploadsrec(*)[1])(BbsAllocA((GetSession()->GetCurrentUser()->GetScreenLines() + 20) * sizeof(uploadsrec)));
  WWIV_ASSERT(file_recs);
  if (!file_recs) {
    BbsFree2D(menu_items);
    return 0;
  }
  if (!prep_search_rec(&search_rec, type)) {
    free(file_recs);
    BbsFree2D(menu_items);
    return 0;
  }
  int max_lines = calc_max_lines();

  g_num_listed = 0;
  bool all_done = false;
  for (int this_dir = 0; (this_dir < GetSession()->num_dirs) && (!hangup) && (udir[this_dir].subnum != -1)
       && !all_done; this_dir++) {
    int also_this_dir = udir[this_dir].subnum;
    bool scan_dir = false;
    checka(&all_done);

    if (search_rec.alldirs == THIS_DIR) {
      if (this_dir == save_dir) {
        scan_dir = true;
      }
    } else {
      if (qsc_n[also_this_dir / 32] & (1L << (also_this_dir % 32))) {
        scan_dir = true;
      }

      if ((search_rec.alldirs == ALL_DIRS) && (type != LP_NSCAN_NSCAN)) {
        scan_dir = true;
      }
    }

    int save_first_file = 0;
    if (scan_dir) {
      GetSession()->SetCurrentFileArea(this_dir);
      dliscan();
      g_num_listed = 0;
      int first_file = save_first_file = 1;
      int amount = 0;
      bool done = false;
      int matches = 0;
      int lines = 0;
      int changedir = 0;

      WFile fileDownload(g_szDownloadFileName);
      while (!done && !hangup && !all_done) {
        checka(&all_done);
        if (!amount) {
          if (!fileDownload.Open(WFile::modeBinary | WFile::modeReadOnly)) {
            done = true;
            continue;
          }
          print_searching(&search_rec);
        }
        if (GetSession()->numf) {
          changedir = 0;
          bool force_menu = false;
          FileAreaSetRecord(fileDownload, first_file + amount);
          fileDownload.Read(file_recs[matches], sizeof(uploadsrec));
          if (compare_criteria(&search_rec, file_recs[matches])) {
            int lines_left = max_lines - lines;
            int needed = check_lines_needed(file_recs[matches]);
            if (needed <= lines_left) {
              if (!matches) {
                printtitle_plus();
              }
              file_handle[matches] = first_file + amount;
              vert_pos[matches] = static_cast<char>(lines);
              int lines_used = printinfo_plus(file_recs[matches], file_handle[matches],
                                              check_batch_queue(file_recs[matches]->filename), lines_left, &search_rec);
#ifdef EXTRA_SPACE
              if (lines_used > 1 && lines_used < lines_left) {
                GetSession()->bout.NewLine();
                ++lines_used;
              }
#endif
              lines += lines_used;
              ++matches;
            } else {
              force_menu = true;
            }
          }
          if (!force_menu) {
            ++amount;
          }

          if (lines >= max_lines || GetSession()->numf < first_file + amount || force_menu) {
            fileDownload.Close();
            if (matches) {
              file_pos = save_file_pos;
              drawfile(vert_pos[file_pos], file_handle[file_pos]);
              bool redraw = true;
              save_file_pos = 0;
              bool menu_done = false;
              while (!menu_done && !hangup) {
                int command = side_menu(&menu_pos, redraw, menu_items, 2, max_lines + first_file_pos() + 1, &smc);
                redraw = true;
                bulk_move = 0;
                GetSession()->bout.Color(0);
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
                  GetSession()->bout.ClearScreen();
                  printfile(LISTPLUS_HLP);
                  pausescr();
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
                    if (first_file > GetSession()->numf) {
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
                      do_batch_sysop_command(SYSOP_DELETE, file_recs[file_pos]->filename);
                      menu_done = true;
                      save_file_pos = file_pos = 0;
                      amount = lines = matches = 0;
                    } else {
ADD_OR_REMOVE_BATCH:
#ifdef KBPERDAY
                      kbbatch += bytes_to_k(file_recs[file_pos]->numbytes);
#endif
                      if (find_batch_queue(file_recs[file_pos]->filename) > -1) {
                        remove_batch(file_recs[file_pos]->filename);
                        redraw = false;
                      }
#ifdef FILE_POINTS
                      else if (((!(file_recs[file_pos]->mask & mask_validated)) ||
                                ((file_recs[file_pos]->filepoints > GetSession()->GetCurrentUser()->GetFilePoints())) &&
                                !GetSession()->GetCurrentUser()->IsExemptRatio()) &&
                               !sysop_mode) {
                        GetSession()->bout.ClearScreen();
                        GetSession()->bout << "You don't have enough file points to download this file\r\n";
                        GetSession()->bout << "Or this file is not validated yet.\r\n";
#else
                      else if (!ratio_ok() && !sysop_mode) {
#endif
                        menu_done = true;
                        amount = lines = matches = 0;
                        save_file_pos = file_pos;
                        pausescr();
                      } else {
                        char szTempFile[MAX_PATH];
                        redraw = false;
                        if (!(directories[udir[GetSession()->GetCurrentFileArea()].subnum].mask & mask_cdrom) && !sysop_mode) {
                          strcpy(szTempFile, directories[udir[GetSession()->GetCurrentFileArea()].subnum].path);
                          strcat(szTempFile, file_recs[file_pos]->filename);
                          unalign(szTempFile);
                          if (sysop_mode || !GetSession()->using_modem || WFile::Exists(szTempFile)) {
#ifdef FILE_POINTS
                            fpts = 0;
                            fpts = (file_recs[file_pos]->filepoints);
#endif
                            lp_add_batch(file_recs[file_pos]->filename, udir[GetSession()->GetCurrentFileArea()].subnum,
                                         file_recs[file_pos]->numbytes);
                          } else if (lp_config.request_file) {
                            menu_done = true;
                            amount = lines = matches = 0;
                            request_file(file_recs[file_pos]->filename);
                          }
                        } else {
                          lp_add_batch(file_recs[file_pos]->filename, udir[GetSession()->GetCurrentFileArea()].subnum,
                                       file_recs[file_pos]->numbytes);
                        }
                      }
#ifdef KBPERDAY
                      kbbatch -= bytes_to_k(file_recs[file_pos]->numbytes);
#endif
                      GetSession()->bout.GotoXY(1, first_file_pos() + vert_pos[file_pos]);
                      GetSession()->bout.WriteFormatted("|%2d %c ", lp_config.tagged_color,
                                                        check_batch_queue(file_recs[file_pos]->filename) ? '\xFE' : ' ');
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
                      show_fileinfo(file_recs[file_pos]);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    } else {
                      do_batch_sysop_command(SYSOP_RENAME, file_recs[file_pos]->filename);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    }
                    menu_pos = 0;
                    break;
                  case 4:
                    if (!sysop_mode) {
                      view_file(file_recs[file_pos]->filename);
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
                    } else {
                      do_batch_sysop_command(SYSOP_MOVE, file_recs[file_pos]->filename);
                      menu_done = true;
                      save_file_pos = file_pos = 0;
                      amount = lines = matches = 0;
                    }
                    menu_pos = 0;
                    break;
                  case 5:
                    if (!sysop_mode && GetSession()->using_modem) {
                      GetSession()->bout.ClearScreen();
                      menu_done = true;
                      save_file_pos = file_pos;
                      amount = lines = matches = 0;
#ifdef FILE_POINTS
                      if (((!(file_recs[file_pos]->mask & mask_validated))
                           || ((file_recs[file_pos]->filepoints > GetSession()->GetCurrentUser()->GetFilePoints())) &&
                           !GetSession()->GetCurrentUser()->IsExemptRatio()) && !sysop_mode) {
                        GetSession()->bout.ClearScreen();
                        GetSession()->bout << "You don't have enough file points to download this file\r\n";
                        GetSession()->bout << "Or this file is not validated yet.\r\n";
#else
                      if (!ratio_ok()) {
#endif
                        pausescr();
                      } else {
                        if (!ratio_ok()  && !sysop_mode) {
                          menu_done = true;
                          amount = lines = matches = 0;
                          save_file_pos = file_pos;
                          pausescr();
                        } else {
                          char szTempFile[MAX_PATH];
                          redraw = false;
                          if (!(directories[udir[GetSession()->GetCurrentFileArea()].subnum].mask & mask_cdrom) && !sysop_mode) {
                            strcpy(szTempFile, directories[udir[GetSession()->GetCurrentFileArea()].subnum].path);
                            strcat(szTempFile, file_recs[file_pos]->filename);
                            unalign(szTempFile);
                            if (sysop_mode || !GetSession()->using_modem || WFile::Exists(szTempFile)) {
#ifdef FILE_POINTS
                              fpts = 0;
                              fpts = (file_recs[file_pos]->filepoints);
#endif
                              lp_add_batch(file_recs[file_pos]->filename, udir[GetSession()->GetCurrentFileArea()].subnum,
                                           file_recs[file_pos]->numbytes);
                            } else if (lp_config.request_file) {
                              menu_done = true;
                              amount = lines = matches = 0;
                              request_file(file_recs[file_pos]->filename);
                            }
                          } else {
                            lp_add_batch(file_recs[file_pos]->filename, udir[GetSession()->GetCurrentFileArea()].subnum,
                                         file_recs[file_pos]->numbytes);
                          }
                          download_plus(file_recs[file_pos]->filename);
                        }
                      }
                      dliscan();
                    } else if (!sysop_mode) {
                      do_batch_sysop_command(SYSOP_MOVE, file_recs[file_pos]->filename);
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
                    if ((GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)
                        && (udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)) {
                      GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
                      ++this_dir;
                    } else {
                      GetSession()->SetCurrentFileArea(0);
                      this_dir = 0;
                    }
                    if (!type) {
                      save_dir = GetSession()->GetCurrentFileArea();
                    }
                    dliscan();
                    menu_pos = 0;
                    break;
                  case 7:
                    menu_done = true;
                    amount = lines = matches = 0;
                    first_file = 1;
                    changedir = -1;
                    if (GetSession()->GetCurrentFileArea() > 0) {
                      GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() - 1);
                      --this_dir;
                    } else {
                      while ((udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)
                             && (GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)) {
                        GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
                      }
                      this_dir = GetSession()->GetCurrentFileArea();
                    }
                    if (!type) {
                      save_dir = GetSession()->GetCurrentFileArea();
                    }
                    dliscan();
                    menu_pos = 0;
                    break;
                  case 8:
TOGGLE_EXTENDED:
                    ext_is_on = !ext_is_on;
                    GetSession()->GetCurrentUser()->SetFullFileDescriptions(!GetSession()->GetCurrentUser()->GetFullFileDescriptions());
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
                    lines_listed = 0;
                    break;
                  case 10:
                    GetSession()->bout.ClearScreen();
                    printfile(LISTPLUS_HLP);
                    pausescr();
                    menu_done = true;
                    amount = lines = matches = 0;
                    save_file_pos = file_pos;
                    break;
                  case 11:
                    if (so() && !sysop_mode) {
                      sysop_mode = true;
                      strcpy(menu_items[2], "Delete");
                      strcpy(menu_items[3], "Rename");
                      strcpy(menu_items[4], "Move");
                      strcpy(menu_items[5], "Config");
                      strcpy(menu_items[11], "Back");
                    } else {
                      sysop_mode = false;
                      prep_menu_items(menu_items);
                    }
                    bputch('\r');
                    GetSession()->bout.ClearEOL();
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
                if ((GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)
                    && (udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)) {
                  GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
                } else {
                  GetSession()->SetCurrentFileArea(0);
                }
                dliscan();
              } else {
                if (GetSession()->GetCurrentFileArea() > 0) {
                  GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() - 1);
                } else {
                  while ((udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)
                         && (GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)) {
                    GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
                  }
                }
                dliscan();
              }
            }
          }
        } else {
          fileDownload.Close();
          if (!changedir) {
            done = true;
          } else if (changedir == 1) {
            if ((GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)
                && (udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)) {
              GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
            } else {
              GetSession()->SetCurrentFileArea(0);
            }
            dliscan();
          } else {
            if (GetSession()->GetCurrentFileArea() > 0) {
              GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() - 1);
            } else {
              while ((udir[GetSession()->GetCurrentFileArea() + 1].subnum >= 0)
                     && (GetSession()->GetCurrentFileArea() < GetSession()->num_dirs - 1)) {
                GetSession()->SetCurrentFileArea(GetSession()->GetCurrentFileArea() + 1);
              }
            }
            dliscan();
          }
        }
      }
    }
  }

  free(file_recs);
  BbsFree2D(menu_items);

  return (all_done) ? 1 : 0;
}


void drawfile(int filepos, int filenum) {
  lines_listed = 0;
  GetSession()->bout.GotoXY(4, filepos + first_file_pos());
  GetSession()->bout.SystemColor(lp_config.current_file_color);
  GetSession()->bout.WriteFormatted("%3d|#0", filenum);
  GetSession()->bout.GotoXY(4, filepos + first_file_pos());
}


int compare_criteria(struct search_record * sr, uploadsrec * ur) {
  // "        .   "
  if (!wwiv::strings::IsEquals(sr->filemask, "        .   ")) {
    if (!compare(sr->filemask, ur->filename)) {
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
  if (sr->search[0]) {
    char *buff = nullptr;
    int desc_len = 0, fname_len = 0, ext_len = 0;

    // we want to seach the filename, description and ext description
    // as one unit, that way, if you specify something like 'one & two
    // and one is located in the description and two is in the
    // extended description, then it will properly find the search

    if (sr->search_extended && ur->mask & mask_extended) {
      buff = READ_EXTENDED_DESCRIPTION(ur->filename);
    }

    desc_len = strlen(ur->description);

    if (buff) {
      ext_len = strlen(buff);
    }
    fname_len = strlen(ur->filename);

    buff = reinterpret_cast<char*>(realloc(buff, desc_len + ext_len + fname_len + 10));
    if (!buff) {
      return 0;
    }

    buff[ext_len] = '\0';

    // tag the file name and description on to the end of the extended
    // description (if there is one)
    strcat(buff, " ");
    strcat(buff, ur->filename);
    strcat(buff, " ");
    strcat(buff, ur->description);

    if (lp_compare_strings(buff, sr->search)) {
      free(buff);
      return 1;
    }
    free(buff);

    return 0;                               // if we get here, we failed search test, so exit with 0 */

  }
  return 1;                                 // this is return 1 becuase top two tests were passed */
  // and we are not searching on text, so it is assume to
  // have passed the test
}


bool lp_compare_strings(char *raw, char *formula) {
  unsigned i = 0;

  return lp_compare_strings_wh(raw, formula, &i, strlen(formula));
}


bool lp_compare_strings_wh(char *raw, char *formula, unsigned *pos, int size) {
  bool rvalue;
  int token;

  bool lvalue = lp_get_value(raw, formula, pos) ? true : false;
  while (*pos < (unsigned int) size) {
    token = lp_get_token(formula, pos);

    switch (token) {
    case STR_SPC:                         // Added
    case STR_AND:
      rvalue = lp_compare_strings_wh(raw, formula, pos, size);
      lvalue = lvalue & rvalue;
      break;

    case STR_OR:
      rvalue = lp_compare_strings_wh(raw, formula, pos, size);
      lvalue = lvalue | rvalue;
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


int lp_get_token(char *formula, unsigned *pos) {
  char szBuffer[255];
  int tpos = 0;

  while (formula[*pos] && isspace(formula[*pos])) {
    ++* pos;
  }

  if (isalpha(formula[*pos])) {
    // remove isspace to delemit on a by word basis
    while (isalnum(formula[*pos]) || isspace(formula[*pos])) {
      szBuffer[tpos] = formula[*pos];
      ++tpos;
      ++*pos;
    }
    szBuffer[tpos] = 0;
  }
  ++*pos;
  return formula[*pos - 1];
}


int lp_get_value(char *raw, char *formula, unsigned *pos) {
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
    if (lp_compare_strings_wh(raw, formula, pos, strlen(formula))) {
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

  if (stristr(raw, szBuffer)) {
    return (sign ? 1 : 0);
  } else {
    return (sign ? 0 : 1);
  }
}


