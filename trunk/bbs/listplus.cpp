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
#include <algorithm>
#include <csignal>
#include <string>
#include <vector>

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/keycodes.h"
#include "bbs/listplus.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/wconstants.h"
#include "bbs/wwiv.h"
#include "bbs/wwivcolors.h"

#include "core/strings.h"
#include "core/wwivassert.h"

using std::string;
using std::vector;
using wwiv::strings::StringPrintf;

user_config config_listing;
int list_loaded;

int bulk_move = 0;
int bulk_dir = -1;

int extended_desc_used, lc_lines_used;
bool ext_is_on;

struct listplus_config lp_config;

static char _on_[] = "ON!";
static char _off_[] = "OFF";

int lp_config_loaded;

#ifdef FILE_POINTS
long fpts;
#endif  // FILE_POINTS

// TODO remove this hack and fix the real problem of fake spaces in filenames everywhere
static bool ListPlusExist(const char *pszFileName) {
  char szRealFileName[MAX_PATH];
  strcpy(szRealFileName, pszFileName);
  StringRemoveWhitespace(szRealFileName);
  return (*szRealFileName) ? File::Exists(szRealFileName) : false;
}

static void colorize_foundtext(char *text, struct search_record * search_rec, int color) {
  int size;
  char *pszTempBuffer, found_color[10], normal_color[10], *tok;
  char find[101], word[101];

  sprintf(found_color, "|%02d|%02d", lp_config.found_fore_color, lp_config.found_back_color);
  sprintf(normal_color, "|16|%02d", color);

  if (lp_config.colorize_found_text) {
    strcpy(find, search_rec->search);
    tok = strtok(find, "&|!()");

    while (tok) {
      pszTempBuffer = text;
      strcpy(word, tok);
      StringTrim(word);

      while (pszTempBuffer && word[0]) {
        if ((pszTempBuffer = stristr(pszTempBuffer, word)) != nullptr) {
          size = strlen(pszTempBuffer) + 1;
          memmove(&pszTempBuffer[6], &pszTempBuffer[0], size);
          strncpy(pszTempBuffer, found_color, 6);
          pszTempBuffer += strlen(word) + 6;
          size = strlen(pszTempBuffer) + 1;
          memmove(&pszTempBuffer[6], &pszTempBuffer[0], size);
          strncpy(pszTempBuffer, normal_color, 6);
          pszTempBuffer += 6;
          ++pszTempBuffer;
        }
      }
      tok = strtok(nullptr, "&|!()");
    }
  }
}

static void build_header() {
  char szHeader[255];
  int desc_pos = 30;

  strcpy(szHeader, " Tag # ");

  if (config_listing.lp_options & cfl_fname) {
    strcat(szHeader, "FILENAME");
  }

  if (config_listing.lp_options & cfl_extension) {
    strcat(szHeader, ".EXT ");
  }

  if (config_listing.lp_options & cfl_dloads) {
    strcat(szHeader, " DL ");
  }

  if (config_listing.lp_options & cfl_kbytes) {
    strcat(szHeader, "Bytes ");
  }

#ifdef FILE_POINTS
  if (config_listing.lp_options & cfl_file_points) {
    strcat(szHeader, "Fpts ");
  }
#endif

  if (config_listing.lp_options & cfl_days_old) {
    strcat(szHeader, "Age ");
  }

  if (config_listing.lp_options & cfl_times_a_day_dloaded) {
    strcat(szHeader, "DL'PD ");
  }

  if (config_listing.lp_options & cfl_days_between_dloads) {
    strcat(szHeader, "DBDLS ");
  }

  if (config_listing.lp_options & cfl_description) {
    desc_pos = strlen(szHeader);
    strcat(szHeader, "Description");
  }
  StringJustify(szHeader, 79, ' ', JUSTIFY_LEFT);
  bout << "|23|01" << szHeader << wwiv::endl;

  szHeader[0] = '\0';

  if (config_listing.lp_options & cfl_date_uploaded) {
    strcat(szHeader, "Date Uploaded      ");
  }
  if (config_listing.lp_options & cfl_upby) {
    strcat(szHeader, "Who uploaded");
  }

  if (szHeader[0]) {
    StringJustify(szHeader, desc_pos + strlen(szHeader), ' ', JUSTIFY_RIGHT);
    StringJustify(szHeader, 79, ' ', JUSTIFY_LEFT);
    bout << "|23|01" << szHeader << wwiv::endl;
  }
}

static void printtitle_plus_old() {
  bout << "|16|15" << string(79, '\xDC') << wwiv::endl;

  const string buf = StringPrintf("Area %d : %-30.30s (%d files)", atoi(udir[session()->GetCurrentFileArea()].keys),
          directories[udir[session()->GetCurrentFileArea()].subnum].name, session()->numf);
  bout.bprintf("|23|01 \xF9 %-56s Space=Tag/?=Help \xF9 \r\n", buf.c_str());

  if (config_listing.lp_options & cfl_header) {
    build_header();
  }

  bout << "|16|08" << string(79, '\xDF') << wwiv::endl;
  bout.Color(0);
}

void printtitle_plus() {
  if (session()->localIO()->WhereY() != 0 || session()->localIO()->WhereX() != 0) {
    bout.cls();
  }

  if (config_listing.lp_options & cfl_header) {
    printtitle_plus_old();
  } else {
    const string buf = StringPrintf("Area %d : %-30.30s (%d files)", atoi(udir[session()->GetCurrentFileArea()].keys),
            directories[udir[session()->GetCurrentFileArea()].subnum].name, session()->numf);
    bout.litebar("%-54s Space=Tag/?=Help", buf.c_str());
    bout.Color(0);
  }
}

static int lp_configured_lines() {
  return (config_listing.lp_options & cfl_date_uploaded || 
          config_listing.lp_options & cfl_upby) ? 2 : 1;
}

int first_file_pos() {
  int pos = FIRST_FILE_POS;
  if (config_listing.lp_options & cfl_header) {
    pos += lp_configured_lines();
  }
  return pos;
}

void print_searching(struct search_record * search_rec) {
  if (session()->localIO()->WhereY() != 0 || session()->localIO()->WhereX() != 0) {
    bout.cls();
  }

  if (strlen(search_rec->search) > 3) {
    bout << "|#1Search keywords : |#2" << search_rec->search;
    bout.nl(2);
  }
  bout << "|#1<Space> aborts  : ";
  bout.bprintf(" |B1|15%-40.40s|B0|#0",
                                    directories[udir[session()->GetCurrentFileArea()].subnum].name);
}

static void catch_divide_by_zero(int signum) {
  if (signum == SIGFPE) {
    sysoplog("Caught divide by 0");
  }
}

int listfiles_plus(int type) {
  int save_topdata = session()->topdata;
  int save_dir = session()->GetCurrentFileArea();
  long save_status = session()->user()->GetStatus();

  session()->tagging = 0;

  ext_is_on = session()->user()->GetFullFileDescriptions();
  signal(SIGFPE, catch_divide_by_zero);

  session()->topdata = WLocalIO::topdataNone;
  application()->UpdateTopScreen();
  bout.cls();

  int nReturn = listfiles_plus_function(type);
  bout.Color(0);
  bout.GotoXY(1, session()->user()->GetScreenLines() - 3);
  bout.nl(3);

  lines_listed = 0;

  if (type != LP_NSCAN_NSCAN) {
    tmp_disable_conf(false);
  }

  session()->user()->SetStatus(save_status);

  if (type == LP_NSCAN_DIR || type == LP_SEARCH_ALL) {    // change Build3
    session()->SetCurrentFileArea(save_dir);
  }
  dliscan();

  session()->topdata = save_topdata;
  application()->UpdateTopScreen();

  return nReturn;
}


void undrawfile(int filepos, int filenum) {
  lines_listed = 0;
  bout.GotoXY(4, filepos + first_file_pos());
  bout.bprintf("|%2d%3d|#0", lp_config.file_num_color, filenum);
}


int lp_add_batch(const char *pszFileName, int dn, long fs) {
  double t;

  if (find_batch_queue(pszFileName) > -1) {
    return 0;
  }

  if (session()->numbatch >= session()->max_batch) {
    bout.GotoXY(1, session()->user()->GetScreenLines() - 1);
    bout << "No room left in batch queue.\r\n";
    pausescr();
  } else if (!ratio_ok()) {
    pausescr();
  } else {
    if (modem_speed && fs) {
      t = (9.0) / ((double)(modem_speed)) * ((double)(fs));
    } else {
      t = 0.0;
    }

#ifdef FILE_POINTS
    if ((session()->user()->GetFilePoints() < (batchfpts + fpts))
        && !session()->user()->IsExemptRatio()) {
      bout.GotoXY(1, session()->user()->GetScreenLines() - 1);
      bout << "Not enough file points to download this file\r\n";
      pausescr();
    } else
#endif

      if ((nsl() <= (batchtime + t)) && (!so())) {
        bout.GotoXY(1, session()->user()->GetScreenLines() - 1);
        bout << "Not enough time left in queue.\r\n";
        pausescr();
      } else {
        if (dn == -1) {
          bout.GotoXY(1, session()->user()->GetScreenLines() - 1);
          bout << "Can't add temporary file to batch queue.\r\n";
          pausescr();
        } else {
          batchtime += static_cast<float>(t);

#ifdef FILE_POINTS
          batchfpts += fpts;
#endif

          strcpy(batch[session()->numbatch].filename, pszFileName);
          batch[session()->numbatch].dir = static_cast<short>(dn);
          batch[session()->numbatch].time = static_cast<float>(t);
          batch[session()->numbatch].sending = 1;
          batch[session()->numbatch].len = fs;

#ifdef FILE_POINTS
          batch[session()->numbatch].filepoints = fpts;
#endif

          session()->numbatch++;

#ifdef KBPERDAY
          kbbatch += bytes_to_k(fs);
#endif
          ++session()->numbatchdl;
          return 1;
        }
      }
  }
  return 0;
}


int printinfo_plus(uploadsrec * u, int filenum, int marked, int LinesLeft, struct search_record * search_rec) {
  char szBuffer[MAX_PATH], szFileName[MAX_PATH], szFileExt[MAX_PATH];
  char szFileInformation[1024], element[150];
  int chars_this_line = 0, numl = 0, cpos = 0, will_fit = 78;
  char ch = 0;
  int char_printed = 0, extdesc_pos;

  strcpy(szFileName, u->filename);
  if (!szFileName[0]) {
    // Make sure the filename isn't empty, if it is then lets bail!
    return numl;
  }

  char * str = strchr(szFileName, '.');
  if (str && *str) {
    str[0] = 0;
    ++str;
    strcpy(szFileExt, str);
  } else {
    strcpy(szFileExt, "   ");
  }

  time_t tTimeNow = time(nullptr);
  long lDiffTime = static_cast<long>(difftime(tTimeNow, u->daten));
  int nDaysOld = lDiffTime / SECONDS_PER_DAY;

  sprintf(szFileInformation, "|%2d %c |%2d%3d ", lp_config.tagged_color, marked ? '\xFE' : ' ', lp_config.file_num_color,
          filenum);
  int width = 7;
  lines_listed = 0;

  if (config_listing.lp_options & cfl_fname) {
    strcpy(szBuffer, szFileName);
    StringJustify(szBuffer, 8, ' ', JUSTIFY_LEFT);
    if (search_rec) {
      colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[0]);
    }
    sprintf(element, "|%02d%s", config_listing.lp_colors[0], szBuffer);
    strcat(szFileInformation, element);
    width += 8;
  }
  if (config_listing.lp_options & cfl_extension) {
    strcpy(szBuffer, szFileExt);
    StringJustify(szBuffer, 3, ' ', JUSTIFY_LEFT);
    if (search_rec) {
      colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[1]);
    }
    sprintf(element, "|%02d.%s", config_listing.lp_colors[1], szBuffer);
    strcat(szFileInformation, element);
    width += 4;
  }
  if (config_listing.lp_options & cfl_dloads) {
    sprintf(szBuffer, "%3d", u->numdloads);
    szBuffer[3] = 0;
    sprintf(element, " |%02d%s", config_listing.lp_colors[2], szBuffer);
    strcat(szFileInformation, element);
    width += 4;
  }
  if (config_listing.lp_options & cfl_kbytes) {
    sprintf(szBuffer, "%4ldk", static_cast<long>(bytes_to_k(u->numbytes)));
    szBuffer[5] = 0;
    if (!(directories[udir[session()->GetCurrentFileArea()].subnum].mask & mask_cdrom)) {
      char szTempFile[MAX_PATH];

      strcpy(szTempFile, directories[udir[session()->GetCurrentFileArea()].subnum].path);
      strcat(szTempFile, u->filename);
      unalign(szTempFile);
      if (lp_config.check_exist) {
        if (!ListPlusExist(szTempFile)) {
          strcpy(szBuffer, "OFFLN");
        }
      }
    }
    sprintf(element, " |%02d%s", config_listing.lp_colors[3], szBuffer);
    strcat(szFileInformation, element);
    width += 6;
  }
#ifdef FILE_POINTS
  if (config_listing.lp_options & cfl_file_points) {
    if (u->mask & mask_validated) {
      if (u->filepoints) {
        sprintf(szBuffer, "%4u", u->filepoints);
        szBuffer[4] = 0;
      } else {
        sprintf(szBuffer, "Free");
      }
    } else {
      sprintf(szBuffer, "9e99");
    }
    sprintf(element, " |%02d%s", config_listing.lp_colors[5], szBuffer);
    strcat(szFileInformation, element);
    width += 5;
  }
#endif

  if (config_listing.lp_options & cfl_days_old) {
    sprintf(szBuffer, "%3d", nDaysOld);
    szBuffer[3] = 0;
    sprintf(element, " |%02d%s", config_listing.lp_colors[6], szBuffer);
    strcat(szFileInformation, element);
    width += 4;
  }
  if (config_listing.lp_options & cfl_times_a_day_dloaded) {
    float t;

    t = nDaysOld ? (float) u->numdloads / (float) nDaysOld : (float) 0.0;
    sprintf(szBuffer, "%2.2f", t);
    szBuffer[5] = 0;
    sprintf(element, " |%02d%s", config_listing.lp_colors[8], szBuffer);
    strcat(szFileInformation, element);
    width += 6;
  }
  if (config_listing.lp_options & cfl_days_between_dloads) {
    float t;

    t = nDaysOld ? (float) u->numdloads / (float) nDaysOld : (float) 0.0;
    t = t ? (float) 1 / (float) t : (float) 0.0;
    sprintf(szBuffer, "%3.1f", t);
    szBuffer[5] = 0;
    sprintf(element, " |%02d%s", config_listing.lp_colors[9], szBuffer);
    strcat(szFileInformation, element);
    width += 6;
  }
  if (config_listing.lp_options & cfl_description) {
    ++width;
    strcpy(szBuffer, u->description);
    if (search_rec) {
      colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[10]);
    }
    sprintf(element, " |%02d%s", config_listing.lp_colors[10], szBuffer);
    strcat(szFileInformation, element);
    extdesc_pos = width;
  } else {
    extdesc_pos = -1;
  }

  strcat(szFileInformation, "\r\n");
  cpos = 0;
  while (szFileInformation[cpos] && numl < LinesLeft) {
    do {
      ch = szFileInformation[cpos];
      if (!ch) {
        continue;
      }
      ++cpos;
    } while (ch == '\r' && ch);

    if (!ch) {
      break;
    }

    if (ch == SOFTRETURN) {
      bout.nl();
      chars_this_line = 0;
      char_printed = 0;
      ++numl;
    } else if (chars_this_line > will_fit && ch) {
      do {
        ch = szFileInformation[cpos++];
      } while (ch != '\n' && ch != 0);
      --cpos;
    } else if (ch) {
      chars_this_line += bputch(ch);
    }
  }

  if (extdesc_pos > 0) {
    int num_extended, lines_left;
    int lines_printed;

    lines_left = LinesLeft - numl;
    num_extended = session()->user()->GetNumExtended();
    if (num_extended < lp_config.show_at_least_extended) {
      num_extended = lp_config.show_at_least_extended;
    }
    if (num_extended > lines_left) {
      num_extended = lines_left;
    }
    if (ext_is_on && mask_extended & u->mask) {
      lines_printed = print_extended_plus(u->filename, num_extended, -extdesc_pos, config_listing.lp_colors[10], search_rec);
    } else {
      lines_printed = 0;
    }

    if (lines_printed) {
      numl += lines_printed;
      chars_this_line = 0;
      char_printed = 0;
    }
  }
  if (session()->localIO()->WhereX()) {
    if (char_printed) {
      bout.nl();
      ++numl;
    } else {
      bputch('\r');
    }
  }
  szBuffer[0] = 0;
  szFileInformation[0] = 0;

  if (config_listing.lp_options & cfl_date_uploaded) {
    if ((u->actualdate[2] == '/') && (u->actualdate[5] == '/')) {
      sprintf(szBuffer, "UL: %s  New: %s", u->date, u->actualdate);
      StringJustify(szBuffer, 27, ' ', JUSTIFY_LEFT);
    } else {
      sprintf(szBuffer, "UL: %s", u->date);
      StringJustify(szBuffer, 12, ' ', JUSTIFY_LEFT);
    }
    sprintf(element, "|%02d%s  ", config_listing.lp_colors[4], szBuffer);
    strcat(szFileInformation, element);
  }

  if (config_listing.lp_options & cfl_upby) {
    if (config_listing.lp_options & cfl_date_uploaded) {
      StringJustify(szFileInformation, strlen(szFileInformation) + width, ' ', JUSTIFY_RIGHT);
      bout << szFileInformation;
      bout.nl();
      ++numl;
    }
    string tmp = u->upby;
    tmp = properize(tmp).substr(0, 15);
    sprintf(element, "|%02dUpby: %s", config_listing.lp_colors[7], tmp.c_str());
    strcpy(szFileInformation, element);
  }

  if (szBuffer[0]) {
    StringJustify(szFileInformation, strlen(szFileInformation) + width, ' ', JUSTIFY_RIGHT);
    bout << szFileInformation;
    bout.nl();
    ++numl;
  }
  return numl;
}

static void CheckLPColors() {
  for (int i = 0; i < 32; i++) {
    if (config_listing.lp_colors[i] == 0) {
      config_listing.lp_colors[i] = 7;
    }
  }
}

static void unload_config_listing() {
  list_loaded = 0;
  memset(&config_listing, 0, sizeof(user_config));
}

static int load_config_listing(int config) {
  int len;

  unload_config_listing();
  memset(&config_listing, 0, sizeof(user_config));

  for (int fh = 0; fh < 32; fh++) {
    config_listing.lp_colors[fh] = 7;
  }

  config_listing.lp_options = cfl_fname | cfl_extension | cfl_dloads | cfl_kbytes | cfl_description;

  if (!config) {
    return 0;
  }

  File fileConfig(syscfg.datadir, CONFIG_USR);

  if (fileConfig.Exists()) {
    WUser user;
    application()->users()->ReadUser(&user, config);
    if (fileConfig.Open(File::modeBinary | File::modeReadOnly)) {
      fileConfig.Seek(config * sizeof(user_config), File::seekBegin);
      len = fileConfig.Read(&config_listing, sizeof(user_config));
      fileConfig.Close();
      if (len != sizeof(user_config) ||
          !wwiv::strings::IsEqualsIgnoreCase(config_listing.name, user.GetName())) {
        memset(&config_listing, 0, sizeof(config_listing));
        strcpy(config_listing.name, user.GetName());
        CheckLPColors();
        return 0;
      }
      list_loaded = config;
      extended_desc_used = config_listing.lp_options & cfl_description;
      config_listing.lp_options |= cfl_fname;
      config_listing.lp_options |= cfl_description;
      CheckLPColors();
      return 1;
    }
  }
  CheckLPColors();
  return 0;
}

static void write_config_listing(int config) {
  if (!config) {
    return;
  }

  WUser user;
  application()->users()->ReadUser(&user, config);
  strcpy(config_listing.name, user.GetName());

  File fileUserConfig(syscfg.datadir, CONFIG_USR);
  if (!fileUserConfig.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
    return;
  }

  config_listing.lp_options |= cfl_fname;
  config_listing.lp_options |= cfl_description;

  fileUserConfig.Seek(config * sizeof(user_config), File::seekBegin);
  fileUserConfig.Write(&config_listing, sizeof(user_config));
  fileUserConfig.Close();
}

int print_extended_plus(const char *pszFileName, int numlist, int indent, int color,
                        struct search_record * search_rec) {
  int numl = 0;
  int cpos = 0;
  int chars_this_line = 0;

  int will_fit = 80 - abs(indent) - 2;

  char * ss = read_extended_description(pszFileName);

  if (!ss) {
    return 0;
  }
  StringTrimEnd(ss);
  if (ss) {
    int nBufferSize = strlen(ss);
    if (nBufferSize > MAX_EXTENDED_SIZE) {
      nBufferSize = MAX_EXTENDED_SIZE;
      ss[nBufferSize] = '\0';
    }
    char* new_ss = static_cast<char *>(BbsAllocA((nBufferSize * 4) + 30));
    WWIV_ASSERT(new_ss);
    if (new_ss) {
      strcpy(new_ss, ss);
      if (search_rec) {
        colorize_foundtext(new_ss, search_rec, color);
      }
      if (indent > -1 && indent != 16) {
        bout << "  |#1Extended Description:\n\r";
      }
      char ch = SOFTRETURN;

      while (new_ss[cpos] && numl < numlist && !hangup) {
        if (ch == SOFTRETURN && indent) {
          bout.SystemColor(color);
          bputch('\r');
          bout << "\x1b[" << abs(indent) << "C";
        }
        do {
          ch = new_ss[cpos++];
        } while (ch == '\r' && !hangup);

        if (ch == SOFTRETURN) {
          bout.nl();
          chars_this_line = 0;
          ++numl;
        } else if (chars_this_line > will_fit) {
          do {
            ch = new_ss[cpos++];
          } while (ch != '\n' && ch != 0);
          --cpos;
        } else {
          chars_this_line += bputch(ch);
        }
      }

      if (session()->localIO()->WhereX()) {
        bout.nl();
        ++numl;
      }
      free(new_ss);
      free(ss);   // frank's gpf is here.
    }
  }
  bout.Color(0);
  return numl;
}

void show_fileinfo(uploadsrec * u) {
  bout.cls();
  repeat_char('\xCD', 78);
  bout << "  |#1Filename    : |#2" << u->filename << wwiv::endl;
  bout << "  |#1Uploaded on : |#2" << u->date << " by |#2" << u->upby << wwiv::endl;
  if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
    bout << "  |#1Newest file : |#2" << u->actualdate << wwiv::endl;
  }
  bout << "  |#1Size        : |#2" << bytes_to_k(u->numbytes) << wwiv::endl;
  bout << "  |#1Downloads   : |#2" << u->numdloads << "|#1" << wwiv::endl;
  bout << "  |#1Description : |#2" << u->description << wwiv::endl;
  print_extended_plus(u->filename, 255, 16, YELLOW, nullptr);
  repeat_char('\xCD', 78);
  pausescr();
}

int check_lines_needed(uploadsrec * u) {
  char *tmp;
  int elines = 0;
  int max_extended;

  lc_lines_used = lp_configured_lines();
  int max_lines = calc_max_lines();
  int num_extended = session()->user()->GetNumExtended();

  if (num_extended < lp_config.show_at_least_extended) {
    num_extended = lp_config.show_at_least_extended;
  }

  if (max_lines > num_extended) {
    max_lines = num_extended;
  }

  if (extended_desc_used) {
    max_extended = session()->user()->GetNumExtended();

    if (max_extended < lp_config.show_at_least_extended) {
      max_extended = lp_config.show_at_least_extended;
    }

    char* ss = nullptr;
    if (ext_is_on && mask_extended & u->mask) {
      ss = read_extended_description(u->filename);
    }

    if (ss) {
      tmp = ss;
      while ((elines < max_extended) && ((tmp = strchr(tmp, '\r')) != nullptr)) {
        ++elines;
        ++tmp;
      }
      free(ss);
    }
  }
  if (lc_lines_used + elines > max_lines) {
    return max_lines - 1;
  }

  return lc_lines_used + elines;
}

int prep_search_rec(struct search_record * search_rec, int type) {
  memset(search_rec, 0, sizeof(struct search_record));
  search_rec->search_extended = lp_config.search_extended_on;

  if (type == LP_LIST_DIR) {
    file_mask(search_rec->filemask);
    search_rec->alldirs = THIS_DIR;
  } else if (type == LP_SEARCH_ALL) {
    search_rec->alldirs = ALL_DIRS;
    if (!search_criteria(search_rec)) {
      return 0;
    }
  } else if (type == LP_NSCAN_DIR) {
    search_rec->alldirs = THIS_DIR;
    search_rec->nscandate = nscandate;
  } else if (type == LP_NSCAN_NSCAN) {
    g_flags |= g_flag_scanned_files;
    search_rec->nscandate = nscandate;
    search_rec->alldirs = ALL_DIRS;
  } else {
    sysoplog("Undef LP type");
    return 0;
  }

  if (!search_rec->filemask[0] && !search_rec->nscandate && !search_rec->search[0]) {
    return 0;
  }

  if (search_rec->filemask[0]) {
    if (strchr(search_rec->filemask, '.') == nullptr) {
      strcat(search_rec->filemask, ".*");
    }
  }
  align(search_rec->filemask);
  return 1;
}

int calc_max_lines() {
  int max_lines;

  if (lp_config.max_screen_lines_to_show) {
    max_lines = std::min<int>(session()->user()->GetScreenLines(),
                              lp_config.max_screen_lines_to_show) -
                (first_file_pos() + 1 + STOP_LIST);
  } else {
    max_lines = session()->user()->GetScreenLines() - (first_file_pos() + 1 + STOP_LIST);
  }

  return max_lines;
}

static void check_lp_colors() {
  for (int i = 0; i < 32; i++) {
    if (!config_listing.lp_colors[i]) {
      config_listing.lp_colors[i] = 1;
    }
  }
}

void load_lp_config() {
  if (!lp_config_loaded) {
    File fileConfig(syscfg.datadir, LISTPLUS_CFG);
    if (!fileConfig.Open(File::modeBinary | File::modeReadOnly)) {
      memset(&lp_config, 0, sizeof(struct listplus_config));
      lp_config.fi = lp_config.lssm = static_cast<long>(time(nullptr));

      lp_config.normal_highlight  = (YELLOW + (BLACK << 4));
      lp_config.normal_menu_item  = (CYAN + (BLACK << 4));
      lp_config.current_highlight = (BLUE + (LIGHTGRAY << 4));
      lp_config.current_menu_item = (BLACK + (LIGHTGRAY << 4));

      lp_config.tagged_color      = LIGHTGRAY;
      lp_config.file_num_color    = GREEN;

      lp_config.found_fore_color  = RED;
      lp_config.found_back_color  = (LIGHTGRAY) + 16;

      lp_config.current_file_color = BLACK + (LIGHTGRAY << 4);

      lp_config.max_screen_lines_to_show = 24;
      lp_config.show_at_least_extended = 5;

      lp_config.edit_enable = 1;            // Do or don't let users edit their config
      lp_config.request_file = 1;           // Do or don't use file request
      lp_config.colorize_found_text = 1;    // Do or don't colorize found text
      lp_config.search_extended_on =
        0;     // Defaults to either on or off in adv search, or is either on or off in simple search
      lp_config.simple_search = 1;          // Which one is entered when searching, can switch to other still
      lp_config.no_configuration = 0;       // Toggles configurable menus on and off
      lp_config.check_exist = 1;            // Will check to see if the file exists on hardrive and put N/A if not
      lp_config_loaded = 1;
      lp_config.forced_config = 0;

      save_lp_config();
    } else {
      fileConfig.Read(&lp_config, sizeof(struct listplus_config));
      lp_config_loaded = 1;
      fileConfig.Close();
    }
  }
  check_lp_colors();
}

void save_lp_config() {
  if (lp_config_loaded) {
    File fileConfig(syscfg.datadir, LISTPLUS_CFG);
    if (fileConfig.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite)) {
      fileConfig.Write(&lp_config, sizeof(struct listplus_config));
      fileConfig.Close();
    }
  }
}

void sysop_configure() {
  short color = 0;
  bool done = false;
  char s[201];

  if (!so()) {
    return;
  }

  load_lp_config();

  while (!done && !hangup) {
    bout.cls();
    printfile(LPSYSOP_NOEXT);
    bout.GotoXY(38, 2);
    bout.SystemColor(lp_config.normal_highlight);
    bout.bprintf("%3d", lp_config.normal_highlight);
    bout.GotoXY(77, 2);
    bout.SystemColor(lp_config.normal_menu_item);
    bout.bprintf("%3d", lp_config.normal_menu_item);
    bout.GotoXY(38, 3);
    bout.SystemColor(lp_config.current_highlight);
    bout.bprintf("%3d", lp_config.current_highlight);
    bout.GotoXY(77, 3);
    bout.SystemColor(lp_config.current_menu_item);
    bout.bprintf("%3d", lp_config.current_menu_item);
    bout.Color(0);
    bout.GotoXY(38, 6);
    bout.bprintf("|%2d%2d", lp_config.tagged_color, lp_config.tagged_color);
    bout.GotoXY(77, 6);
    bout.bprintf("|%2d%2d", lp_config.file_num_color, lp_config.file_num_color);
    bout.GotoXY(38, 7);
    bout.bprintf("|%2d%2d", lp_config.found_fore_color, lp_config.found_fore_color);
    bout.GotoXY(77, 7);
    bout.bprintf("|%2d%2d", lp_config.found_back_color, lp_config.found_back_color);
    bout.GotoXY(38, 8);
    bout.SystemColor(lp_config.current_file_color);
    bout.bprintf("%3d", lp_config.current_file_color);
    bout.GotoXY(38, 11);
    bout.bprintf("|#4%2d", lp_config.max_screen_lines_to_show);
    bout.GotoXY(77, 11);
    bout.bprintf("|#4%2d", lp_config.show_at_least_extended);
    bout.GotoXY(74, 14);
    bout.bprintf("|#4%s", lp_config.request_file ? _on_ : _off_);
    bout.GotoXY(74, 15);
    bout.bprintf("|#4%s", lp_config.search_extended_on ? _on_ : _off_);
    bout.GotoXY(74, 16);
    bout.bprintf("|#4%s", lp_config.edit_enable ? _on_ : _off_);
    bout.Color(0);
    bout.GotoXY(29, 14);
    bout.bprintf("|#4%s", lp_config.no_configuration ? _on_ : _off_);
    bout.GotoXY(29, 15);
    bout.bprintf("|#4%s", lp_config.colorize_found_text ? _on_ : _off_);
    bout.GotoXY(29, 16);
    bout.bprintf("|#4%s", lp_config.simple_search ? _on_ : _off_);
    bout.GotoXY(29, 17);
    bout.bprintf("|#4%s", lp_config.check_exist ? _on_ : _off_);
    bout.GotoXY(1, 19);
    bout << "|#1Q-Quit ";
    char key = onek("Q\rABCDEFGHIJKLMNOPRS", true);
    switch (key) {
    case 'Q':
    case '\r':
      done = true;
      break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'I':
      color = SelectColor(2);
      if (color >= 0) {
        switch (key) {
        case 'A':
          lp_config.normal_highlight = color;
          break;
        case 'B':
          lp_config.normal_menu_item = color;
          break;
        case 'C':
          lp_config.current_highlight = color;
          break;
        case 'D':
          lp_config.current_menu_item = color;
          break;
        case 'I':
          lp_config.current_file_color = color;
          break;
        }
      }
      break;
    case 'E':
    case 'F':
    case 'G':
    case 'H':
      color = SelectColor(1);
      if (color >= 0) {
        switch (key) {
        case 'E':
          lp_config.tagged_color = color;
          break;
        case 'F':
          lp_config.file_num_color = color;
          break;
        case 'G':
          lp_config.found_fore_color = color;
          break;
        case 'H':
          lp_config.found_back_color = color + 16;
          break;
        }
      }
      break;
    case 'J':
      bout << "Enter max amount of lines to show (0=disabled) ";
      input(s, 2, true);
      lp_config.max_screen_lines_to_show = wwiv::strings::StringToShort(s);
      break;
    case 'K':
      bout << "Enter minimum extended description lines to show ";
      input(s, 2, true);
      lp_config.show_at_least_extended = wwiv::strings::StringToShort(s);
      break;
    case 'L':
      lp_config.no_configuration = !lp_config.no_configuration;
      break;
    case 'M':
      lp_config.request_file = !lp_config.request_file;
      break;
    case 'N':
      lp_config.colorize_found_text = !lp_config.colorize_found_text;
      break;
    case 'O':
      lp_config.search_extended_on = !lp_config.search_extended_on;
      break;
    case 'P':
      lp_config.simple_search = !lp_config.simple_search;
      break;
    case 'R':
      lp_config.edit_enable = !lp_config.edit_enable;
      break;
    case 'S':
      lp_config.check_exist = !lp_config.check_exist;
      break;
    }
  }
  save_lp_config();
  load_lp_config();
}

short SelectColor(int which) {
  char ch, nc;

  bout.nl();

  if (session()->user()->HasColor()) {
    color_list();
    bout.Color(0);
    bout.nl();
    bout << "|#2Foreground? ";
    ch = onek("01234567");
    nc = ch - '0';

    if (which == 2) {
      bout << "|#2Background? ";
      ch = onek("01234567");
      nc = nc | ((ch - '0') << 4);
    }
  } else {
    bout.nl();
    bout << "|#5Inversed? ";
    if (yesno()) {
      if ((session()->user()->GetBWColor(1) & 0x70) == 0) {
        nc = 0 | ((session()->user()->GetBWColor(1) & 0x07) << 4);
      } else {
        nc = (session()->user()->GetBWColor(1) & 0x70);
      }
    } else {
      if ((session()->user()->GetBWColor(1) & 0x70) == 0) {
        nc = 0 | (session()->user()->GetBWColor(1) & 0x07);
      } else {
        nc = ((session()->user()->GetBWColor(1) & 0x70) >> 4);
      }
    }
  }

  bout << "|#5Intensified? ";
  if (yesno()) {
    nc |= 0x08;
  }

  if (which == 2) {
    bout << "|#5Blinking? ";
    if (yesno()) {
      nc |= 0x80;
    }
  }
  bout.nl();
  bout.SystemColor(nc);
  bout << DescribeColorCode(nc);
  bout.Color(0);
  bout.nl();

  bout << "|#5Is this OK? ";
  if (yesno()) {
    return nc;
  }
  return -1;
}

void check_listplus() {
  bout << "|#5Use listplus file tagging? ";

  if (noyes()) {
    if (session()->user()->IsUseListPlus()) {
      session()->user()->ClearStatusFlag(WUser::listPlus);
    }
  } else {
    if (!session()->user()->IsUseListPlus()) {
      session()->user()->SetStatusFlag(WUser::listPlus);
    }
  }
}

void config_file_list() {
  int key, which = -1;
  unsigned long bit = 0L;
  char action[51];
  uploadsrec u;

  strcpy(u.filename, "WWIV50.ZIP");
  strcpy(u.description, "This is a sample description!");
  strcpy(u.date, date());
  strcpy(reinterpret_cast<char*>(u.upby), session()->user()->GetUserNameAndNumber(session()->usernum));
  u.numdloads = 50;
  u.numbytes = 655535L;
  u.daten = time(nullptr) - 10000;

  load_lp_config();

  if (session()->usernum != list_loaded) {
    if (!load_config_listing(session()->usernum)) {
      load_config_listing(1);
    }
  }

  bout.cls();
  printfile(LPCONFIG_NOEXT);
  if (!config_listing.lp_options & cfl_fname) {
    config_listing.lp_options |= cfl_fname;
  }

  if (!(config_listing.lp_options & cfl_description)) {
    config_listing.lp_options |= cfl_description;
  }

  action[0] = '\0';
  bool done = false;
  while (!done && !hangup) {
    update_user_config_screen(&u, which);
    key = onek("Q2346789H!@#$%^&*(");
    switch (key) {
    case '2':
    case '3':
    case '4':
    case '6':
    case '7':
    case '8':
    case '9':
    case 'H':
      switch (key) {
      case '2':
        bit = cfl_extension;
        which = 2;
        break;
      case '3':
        bit = cfl_dloads;
        which = 3;
        break;
      case '4':
        bit = cfl_kbytes;
        which = 4;
        break;
      case '6':
        bit = cfl_date_uploaded;
        which = 6;
        break;
      case '7':
        bit = cfl_file_points;
        which = 7;
        break;
      case '8':
        bit = cfl_days_old;
        which = 8;
        break;
      case '9':
        bit = cfl_upby;
        which = 9;
        break;
      case 'H':
        bit = cfl_header;
        which = 10;
        break;
      }

      if (config_listing.lp_options & bit) {
        config_listing.lp_options &= ~bit;
      } else {
        config_listing.lp_options |= bit;
      }
      break;

    case '!':
    case '@':
    case '#':
    case '$':
    case '%':
    case '^':
    case '&':
    case '*':
    case '(':
      switch (key) {
      case '!':
        bit = 0;
        which = 1;
        break;
      case '@':
        bit = 1;
        which = 2;
        break;
      case '#':
        bit = 2;
        which = 3;
        break;
      case '$':
        bit = 3;
        which = 4;
        break;
      case '%':
        bit = 10;
        which = 5;
        break;
      case '^':
        bit = 4;
        which = 6;
        break;
      case '&':
        bit = 5;
        which = 7;
        break;
      case '*':
        bit = 6;
        which = 8;
        break;
      case '(':
        bit = 7;
        which = 9;
        break;
      }

      ++config_listing.lp_colors[bit];
      if (config_listing.lp_colors[bit] > 15) {
        config_listing.lp_colors[bit] = 1;
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  }
  list_loaded = session()->usernum;
  write_config_listing(session()->usernum);
  bout.nl(4);
}

void update_user_config_screen(uploadsrec * u, int which) {
  struct search_record sr;
  static const vector<string> lp_color_list{
    "Black   ",
    "Blue    ",
    "Green   ",
    "Cyan    ",
    "Red     ",
    "Purple  ",
    "Brown   ",
    "L-Gray  ",
    "D-Gray  ",
    "L-Blue  ",
    "L-Green ",
    "L-Cyan  ",
    "L-Red   ",
    "L-Purple",
    "Yellow  ",
    "White   "
  };

  memset(&sr, 0, sizeof(struct search_record));

  if (which < 1 || which == 1) {
    bout.GotoXY(37, 4);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_fname ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 0 ] ];
  }
  if (which < 1 || which == 2) {
    bout.GotoXY(37, 5);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_extension ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 1 ] ];
  }
  if (which < 1 || which == 3) {
    bout.GotoXY(37, 6);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_dloads ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 2 ] ];
  }
  if (which < 1 || which == 4) {
    bout.GotoXY(37, 7);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_kbytes ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 3 ] ];
  }
  if (which < 1 || which == 5) {
    bout.GotoXY(37, 8);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_description ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 10 ] ];
  }
  if (which < 1 || which == 6) {
    bout.GotoXY(37, 9);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_date_uploaded ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[ config_listing.lp_colors[ 4 ] ];
  }
  if (which < 1 || which == 7) {
    bout.GotoXY(37, 10);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_file_points ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[config_listing.lp_colors[5]];
  }
  if (which < 1 || which == 8) {
    bout.GotoXY(37, 11);
    bout.SystemColor((config_listing.lp_options & cfl_days_old ? RED + (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[config_listing.lp_colors[6]];
  }
  if (which < 1 || which == 9) {
    bout.GotoXY(37, 12);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_upby ? RED + (BLUE << 4) : BLACK +
                                   (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
    bout << lp_color_list[config_listing.lp_colors[7]];
  }
  if (which < 1 || which == 10) {
    bout.GotoXY(37, 13);
    bout.SystemColor(static_cast<uint8_t>(config_listing.lp_options & cfl_header ? RED +
                                   (BLUE << 4) : BLACK + (BLUE << 4)));
    bout << "\xFE ";
    bout.SystemColor(BLACK + (BLUE << 4));
  }
  bout.SystemColor(YELLOW);
  bout.GotoXY(1, 21);
  bout.clreol();
  bout.nl();
  bout.clreol();
  bout.GotoXY(1, 21);
  printinfo_plus(u, 1, 1, 30, &sr);
  bout.GotoXY(30, 17);
  bout.SystemColor(YELLOW);
  bout.bs();
}

static int rename_filename(const char *pszFileName, int dn) {
  char s[81], s1[81], s2[81], *ss, s3[81], ch;
  int i, cp, ret = 1;
  uploadsrec u;

  dliscan1(dn);
  strcpy(s, pszFileName);

  if (s[0] == '\0') {
    return ret;
  }

  if (strchr(s, '.') == nullptr) {
    strcat(s, ".*");
  }
  align(s);

  strcpy(s3, s);
  i = recno(s);
  while (i > 0) {
    File fileDownload(g_szDownloadFileName);
    if (!fileDownload.Open(File::modeBinary | File::modeReadOnly)) {
      break;
    }
    cp = i;
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();
    printfileinfo(&u, dn);
    bout.nl();
    bout << "|#5Change info for this file (Y/N/Q)? ";
    ch = ynq();
    if (ch == 'Q') {
      ret = 0;
      break;
    } else if (ch == 'N') {
      i = nrecno(s3, cp);
      continue;
    }
    bout.nl();
    bout << "|#2New filename? ";
    input(s, 12);
    if (!okfn(s)) {
      s[0] = '\0';
    }
    if (s[0]) {
      align(s);
      if (!wwiv::strings::IsEquals(s, "        .   ")) {
        strcpy(s1, directories[dn].path);
        strcpy(s2, s1);
        strcat(s1, s);
        if (ListPlusExist(s1)) {
          bout << "Filename already in use; not changed.\r\n";
        } else {
          strcat(s2, u.filename);
          File::Rename(s2, s1);
          if (ListPlusExist(s1)) {
            ss = read_extended_description(u.filename);
            if (ss) {
              delete_extended_description(u.filename);
              add_extended_description(s, ss);
              free(ss);
            }
            strcpy(u.filename, s);
          } else {
            bout << "Bad filename.\r\n";
          }
        }
      }
    }
    bout.nl();
    bout << "New description:\r\n|#2: ";
    Input1(s, u.description, 58, false, wwiv::bbs::InputMode::MIXED);
    if (s[0]) {
      strcpy(u.description, s);
    }
    ss = read_extended_description(u.filename);
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (yesno()) {
      bout.nl();
      if (ss) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          free(ss);
          delete_extended_description(u.filename);
          u.mask &= ~mask_extended;
        } else {
          u.mask |= mask_extended;
          modify_extended_description(&ss, directories[dn].name, u.filename);
          if (ss) {
            delete_extended_description(u.filename);
            add_extended_description(u.filename, ss);
            free(ss);
          }
        }
      } else {
        modify_extended_description(&ss, directories[dn].name, u.filename);
        if (ss) {
          add_extended_description(u.filename, ss);
          free(ss);
          u.mask |= mask_extended;
        } else {
          u.mask &= ~mask_extended;
        }
      }
    } else if (ss) {
      free(ss);
      u.mask |= mask_extended;
    } else {
      u.mask &= ~mask_extended;
    }
    if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      FileAreaSetRecord(fileDownload, i);
      fileDownload.Write(&u, sizeof(uploadsrec));
      fileDownload.Close();
    }
    i = nrecno(s3, cp);
  }
  return ret;
}

static int remove_filename(const char *pszFileName, int dn) {
  int ret = 1;
  char szTempFileName[MAX_PATH];
  uploadsrec u;
  memset(&u, 0, sizeof(uploadsrec));

  dliscan1(dn);
  strcpy(szTempFileName, pszFileName);

  if (szTempFileName[0] == '\0') {
    return ret;
  }
  if (strchr(szTempFileName, '.') == nullptr) {
    strcat(szTempFileName, ".*");
  }
  align(szTempFileName);
  int i = recno(szTempFileName);
  bool abort = false;
  bool rdlp = false;
  while (!hangup && i > 0 && !abort) {
    File fileDownload(g_szDownloadFileName);
    if (fileDownload.Open(File::modeReadOnly | File::modeBinary)) {
      FileAreaSetRecord(fileDownload, i);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
    }
    if (dcs() || (u.ownersys == 0 && u.ownerusr == session()->usernum)) {
      bout.nl();
      printfileinfo(&u, dn);
      bout << "|#9Remove (|#2Y/N/Q|#9) |#0: |#2";
      char ch = ynq();
      if (ch == 'Q') {
        ret = 0;
        abort = true;
      } else if (ch == 'Y') {
        rdlp = true;
        bool rm = false;
        if (dcs()) {
          bout << "|#5Delete file too? ";
          rm = yesno();
          if (rm && (u.ownersys == 0)) {
            bout << "|#5Remove DL points? ";
            rdlp = yesno();
          }
          if (application()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
            bout << "|#5Remove from ALLOW.DAT? ";
            if (yesno()) {
              modify_database(szTempFileName, false);
            }
          }
        } else {
          rm = true;
          modify_database(szTempFileName, false);
        }
        if (rm) {
          File::Remove(directories[dn].path, u.filename);
          if (rdlp && u.ownersys == 0) {
            WUser user;
            application()->users()->ReadUser(&user, u.ownerusr);
            if (!user.IsUserDeleted()) {
              if (date_to_daten(user.GetFirstOn()) < static_cast<time_t>(u.daten)) {
                user.SetFilesUploaded(user.GetFilesUploaded() - 1);
                user.SetUploadK(user.GetUploadK() - bytes_to_k(u.numbytes));

#ifdef FILE_POINTS
                if (u.mask & mask_validated) {
                  if ((u.filepoints * 2) > user.GetFilePoints()) {
                    user.SetFilePoints(0);
                  } else {
                    user.SetFilePoints(user.GetFilePoints() - (u.filepoints * 2));
                  }
                }
                bout << "Removed " << (u.filepoints * 2) << " file points\r\n";
#endif
                application()->users()->WriteUser(&user, u.ownerusr);
              }
            }
          }
        }
        if (u.mask & mask_extended) {
          delete_extended_description(u.filename);
        }
        sysoplogf("- \"%s\" removed off of %s", u.filename, directories[dn].name);

        if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          for (int i1 = i; i1 < session()->numf; i1++) {
            FileAreaSetRecord(fileDownload, i1 + 1);
            fileDownload.Read(&u, sizeof(uploadsrec));
            FileAreaSetRecord(fileDownload, i1);
            fileDownload.Write(&u, sizeof(uploadsrec));
          }
          --i;
          --session()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Read(&u, sizeof(uploadsrec));
          u.numbytes = session()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Write(&u, sizeof(uploadsrec));
          fileDownload.Close();
        }
      }
    }
    i = nrecno(szTempFileName, i);
  }
  return ret;
}

static int move_filename(const char *pszFileName, int dn) {
  char szTempMoveFileName[81], szSourceFileName[MAX_PATH], szDestFileName[MAX_PATH], *ss;
  int nDestDirNum = -1, ret = 1;
  uploadsrec u, u1;

  strcpy(szTempMoveFileName, pszFileName);
  dliscan1(dn);
  align(szTempMoveFileName);
  int nRecNum = recno(szTempMoveFileName);
  if (nRecNum < 0) {
    bout << "\r\nFile not found.\r\n";
    return ret;
  }
  bool done = false;
  bool ok = false;

  tmp_disable_conf(true);
  wwiv::bbs::TempDisablePause diable_pause;

  while (!hangup && nRecNum > 0 && !done) {
    int cp = nRecNum;
    File fileDownload(g_szDownloadFileName);
    if (fileDownload.Open(File::modeBinary | File::modeReadOnly)) {
      FileAreaSetRecord(fileDownload, nRecNum);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
    }
    bout.nl();
    printfileinfo(&u, dn);
    bout.nl();
    bout << "|#5Move this (Y/N/Q)? ";
    char ch = 0;
    if (bulk_move) {
      bout.Color(1);
      bout << YesNoString(true);
      bout.nl();
      ch = 'Y';
    } else {
      ch = ynq();
    }

    if (ch == 'Q') {
      done = true;
      ret = 0;
    } else if (ch == 'Y') {
      sprintf(szSourceFileName, "%s%s", directories[dn].path, u.filename);
      if (!bulk_move) {
        do {
          bout.nl(2);
          bout << "|#2To which directory? ";
          ss = mmkey(1);
          if (ss[0] == '?') {
            dirlist(1);
            dliscan1(dn);
          }
        } while (!hangup && ss[0] == '?');

        nDestDirNum = -1;
        if (ss[0]) {
          for (int i1 = 0; (i1 < session()->num_dirs) && (udir[i1].subnum != -1); i1++) {
            if (wwiv::strings::IsEquals(udir[i1].keys, ss)) {
              nDestDirNum = i1;
            }
          }
        }

        if (session()->numbatch > 1) {
          bout << "|#5Move all tagged files? ";
          if (yesno()) {
            bulk_move = 1;
            bulk_dir = nDestDirNum;
          }
        }
      } else {
        nDestDirNum = bulk_dir;
      }

      if (nDestDirNum != -1) {
        ok = true;
        nDestDirNum = udir[nDestDirNum].subnum;
        dliscan1(nDestDirNum);
        if (recno(u.filename) > 0) {
          ok = false;
          bout.nl();
          bout << "Filename already in use in that directory.\r\n";
        }
        if (session()->numf >= directories[nDestDirNum].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (freek1(directories[nDestDirNum].path) < static_cast<long>(u.numbytes / 1024L) + 3) {
          ok = false;
          bout << "\r\nNot enough disk space to move it.\r\n";
        }
        dliscan();
      } else {
        ok = false;
      }
    } else {
      ok = false;
    }
    if (ok && !done) {
      if (!bulk_move) {
        bout << "|#5Reset upload time for file? ";
        if (yesno()) {
          u.daten = static_cast<unsigned long>(time(nullptr));
        }
      } else {
        u.daten = static_cast<unsigned long>(time(nullptr));
      }
      --cp;
      if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        for (int i2 = nRecNum; i2 < session()->numf; i2++) {
          FileAreaSetRecord(fileDownload, i2 + 1);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i2);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        --session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
      }
      ss = read_extended_description(u.filename);
      if (ss) {
        delete_extended_description(u.filename);
      }
      sprintf(szDestFileName, "%s%s", directories[nDestDirNum].path, u.filename);
      dliscan1(nDestDirNum);
      if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        for (int i = session()->numf; i >= 1; i--) {
          FileAreaSetRecord(fileDownload, i);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i + 1);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        FileAreaSetRecord(fileDownload, 1);
        fileDownload.Write(&u, sizeof(uploadsrec));
        ++session()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = session()->numf;
        if (u.daten > u1.daten) {
          u1.daten = u.daten;
          session()->m_DirectoryDateCache[nDestDirNum] = u.daten;
        }
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
      }
      if (ss) {
        add_extended_description(u.filename, ss);
        free(ss);
      }
      if (!wwiv::strings::IsEquals(szSourceFileName, szDestFileName) &&
          ListPlusExist(szSourceFileName)) {
        StringRemoveWhitespace(szSourceFileName);
        StringRemoveWhitespace(szDestFileName);
        bool bSameDrive = false;
        if ((szSourceFileName[1] != ':') && (szDestFileName[1] != ':')) {
          bSameDrive = true;
        }
        if ((szSourceFileName[1] == ':') && (szDestFileName[1] == ':') && (szSourceFileName[0] == szDestFileName[0])) {
          bSameDrive = true;
        }
        if (bSameDrive) {
          File::Rename(szSourceFileName, szDestFileName);
          if (ListPlusExist(szDestFileName)) {
            File::Remove(szSourceFileName);
          } else {
            copyfile(szSourceFileName, szDestFileName, false);
            File::Remove(szSourceFileName);
          }
        } else {
          copyfile(szSourceFileName, szDestFileName, false);
          File::Remove(szSourceFileName);
        }
      }
      bout << "\r\nFile moved.\r\n";
    }
    dliscan();
    nRecNum = nrecno(szTempMoveFileName, cp);
  }
  tmp_disable_conf(false);
  return ret;
}

void do_batch_sysop_command(int mode, const char *pszFileName) {
  int save_curdir = session()->GetCurrentFileArea();
  int pos = 0;

  bout.cls();

  if (session()->numbatchdl) {
    bool done = false;
    while (pos < session()->numbatch && !done) {
      if (batch[pos].sending) {
        switch (mode) {
        case SYSOP_DELETE:
          if (!remove_filename(batch[pos].filename, batch[pos].dir)) {
            done = true;
          } else {
            delbatch(pos);
          }
          break;
        case SYSOP_RENAME:
          if (!rename_filename(batch[pos].filename, batch[pos].dir)) {
            done = true;
          } else {
            delbatch(pos);
          }
          break;
        case SYSOP_MOVE:
          if (!move_filename(batch[pos].filename, batch[pos].dir)) {
            done = true;
          } else {
            delbatch(pos);
          }
          break;
        }
      } else {
        ++pos;
      }
    }
  } else {
    // Just act on the single file
    switch (mode) {
    case SYSOP_DELETE:
      remove_filename(pszFileName, udir[session()->GetCurrentFileArea()].subnum);
      break;
    case SYSOP_RENAME:
      rename_filename(pszFileName, udir[session()->GetCurrentFileArea()].subnum);
      break;
    case SYSOP_MOVE:
      move_filename(pszFileName, udir[session()->GetCurrentFileArea()].subnum);
      break;
    }
  }
  session()->SetCurrentFileArea(save_curdir);
  dliscan();
}

int search_criteria(struct search_record * sr) {
  int x = 0;
  int all_conf = 1, useconf;
  char s1[81];

  useconf = (uconfdir[1].confnum != -1 && okconf(session()->user()));


LP_SEARCH_HELP:
  sr->search_extended = lp_config.search_extended_on;

  bout.cls();
  printfile(LPSEARCH_NOEXT);

  bool done = false;
  while (!done) {
    bout.GotoXY(1, 15);
    for (int i = 0; i < 9; i++) {
      bout.GotoXY(1, 15 + i);
      bout.clreol();
    }
    bout.GotoXY(1, 15);

    bout << "|#1A)|#2 Filename (wildcards) :|02 " << sr->filemask << wwiv::endl;
    bout << "|#1B)|#2 Text (no wildcards)  :|02 " << sr->search << wwiv::endl;
    if (okconf(session()->user())) {
      sprintf(s1, "%s", stripcolors(directories[udir[session()->GetCurrentFileArea()].subnum].name));
    } else {
      sprintf(s1, "%s", stripcolors(directories[udir[session()->GetCurrentFileArea()].subnum].name));
    }
    bout << "|#1C)|#2 Which Directories    :|02 " << (sr->alldirs == THIS_DIR ? s1 : sr->alldirs == ALL_DIRS ?
                       "All dirs" : "Dirs in NSCAN") << wwiv::endl;
    sprintf(s1, "%s", stripcolors(reinterpret_cast<char*>
                                  (dirconfs[uconfdir[session()->GetCurrentConferenceFileArea()].confnum].name)));
    bout << "|#1D)|#2 Which Conferences    :|02 " << (all_conf ? "All Conferences" : s1) << wwiv::endl;
    bout << "|#1E)|#2 Extended Description :|02 " << (sr->search_extended ? "Yes" : "No ") << wwiv::endl;
    bout.nl();
    bout << "|15Select item to change |#2<CR>|15 to start search |#2Q=Quit|15:|#0 ";

    x = onek("QABCDE\r?");

    switch (x) {
    case 'A':
      bout << "Filename (wildcards okay) : ";
      input(sr->filemask, 12, true);
      if (sr->filemask[0]) {
        if (okfn(sr->filemask)) {
          if (strlen(sr->filemask) < 8) {
            sysoplogf("Filespec: %s", sr->filemask);
          } else {
            if (strstr(sr->filemask, ".")) {
              sysoplogf("Filespec: %s", sr->filemask);
            } else {
              bout << "|#6Invalid filename: " << sr->filemask << wwiv::endl;
              pausescr();
              sr->filemask[0] = '\0';
            }
          }
        } else {
          bout << "|#6Invalid filespec: " << sr->filemask << wwiv::endl;
          pausescr();
          sr->filemask[0] = 0;
        }
      }
      break;

    case 'B':
      bout << "Keyword(s) : ";
      input(sr->search, 60, true);
      if (sr->search[0]) {
        sysoplogf("Keyword: %s", sr->search);
      }
      break;

    case 'C':
      ++sr->alldirs;
      if (sr->alldirs >= 3) {
        sr->alldirs = 0;
      }
      break;

    case 'D':
      all_conf = !all_conf;
      break;

    case 'E':
      sr->search_extended = !sr->search_extended;
      break;

    case 'Q':
      return 0;

    case '\r':
      done = true;
      break;

    case '?':
      goto LP_SEARCH_HELP;

    }
  }

  if (sr->search[0] == ' ' && sr->search[1] == 0) {
    sr->search[0] = 0;
  }


  if (useconf && all_conf) {                // toggle conferences off
    tmp_disable_conf(true);
  }

  return x;
}


void load_listing() {

  if (session()->usernum != list_loaded) {
    load_config_listing(session()->usernum);
  }

  if (!list_loaded) {
    load_config_listing(1);                   // default to what the sysop has
  }                                         // config it for themselves
  // force filename to be shown
  if (!config_listing.lp_options & cfl_fname) {
    config_listing.lp_options |= cfl_fname;
  }
}


void view_file(const char *pszFileName) {
  char szCommandLine[MAX_PATH];
  char szBuffer[30];
  long osysstatus;
  int i, i1;
  uploadsrec u;

  bout.cls();

  strcpy(szBuffer, pszFileName);
  unalign(szBuffer);

  // TODO: AVIEWCOM.EXE will not work on x64 platforms, find replacement
  if (File::Exists("AVIEWCOM.EXE")) {
    if (incom) {
      sprintf(szCommandLine, "AVIEWCOM.EXE %s%s -p%s -a1 -d",
              directories[udir[session()->GetCurrentFileArea()].subnum].path, szBuffer, syscfgovr.tempdir);
      osysstatus = session()->user()->GetStatus();
      if (session()->user()->HasPause()) {
        session()->user()->ToggleStatusFlag(WUser::pauseOnPage);
      }
      ExecuteExternalProgram(szCommandLine, EFLAG_COMIO);
      session()->user()->SetStatus(osysstatus);
    } else {
      sprintf(szCommandLine, "AVIEWCOM.EXE %s%s com0 -o%u -p%s -a1 -d",
              directories[udir[session()->GetCurrentFileArea()].subnum].path, szBuffer,
              application()->GetInstanceNumber(), syscfgovr.tempdir);
      ExecuteExternalProgram(szCommandLine, EFLAG_NONE);
    }
  } else {
    dliscan();
    bool abort = false;
    i = recno(pszFileName);
    do {
      if (i > 0) {
        File fileDownload(g_szDownloadFileName);
        if (fileDownload.Open(File::modeBinary | File::modeReadOnly)) {
          FileAreaSetRecord(fileDownload, i);
          fileDownload.Read(&u, sizeof(uploadsrec));
          fileDownload.Close();
        }
        i1 = list_arc_out(stripfn(u.filename), directories[udir[session()->GetCurrentFileArea()].subnum].path);
        if (i1) {
          abort = true;
        }
        checka(&abort);
        i = nrecno(pszFileName, i);
      }
    } while (i > 0 && !hangup && !abort);
    bout.nl();
    pausescr();
  }
}

int lp_try_to_download(const char *pszFileMask, int dn) {
  int i, rtn, ok2;
  bool abort = false;
  uploadsrec u;
  char s1[81], s3[81];

  dliscan1(dn);
  i = recno(pszFileMask);
  if (i <= 0) {
    checka(&abort);
    return (abort) ? -1 : 0;
  }
  bool ok = true;

  foundany = 1;
  do {
    session()->localIO()->tleft(true);
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();

    ok2 = 0;
    if (strncmp(u.filename, "WWIV4", 5) == 0 &&
        !application()->HasConfigFlag(OP_FLAGS_NO_EASY_DL)) {
      ok2 = 1;
    }

    if (!ok2 && (!(u.mask & mask_no_ratio))) {
      if (!ratio_ok()) {
        return -2;
      }
    }

    write_inst(INST_LOC_DOWNLOAD, udir[session()->GetCurrentFileArea()].subnum, INST_FLAGS_ONLINE);
    sprintf(s1, "%s%s", directories[dn].path, u.filename);
    sprintf(s3, "%-40.40s", u.description);
    abort = 0;
    rtn = add_batch(s3, u.filename, dn, u.numbytes);
    s3[0] = 0;

    if (abort || (rtn == -3)) {
      ok = false;
    } else {
      i = nrecno(pszFileMask, i);
    }
  } while ((i > 0) && ok && !hangup);
  returning = true;
  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  } else {
    return 1;
  }
}

void download_plus(const char *pszFileName) {
  char szFileName[MAX_PATH];

  if (session()->numbatchdl != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
      return;
    }
  }
  strcpy(szFileName, pszFileName);
  if (szFileName[0] == '\0') {
    return;
  }
  if (strchr(szFileName, '.') == nullptr) {
    strcat(szFileName, ".*");
  }
  align(szFileName);
  if (lp_try_to_download(szFileName, udir[session()->GetCurrentFileArea()].subnum) == 0) {
    bout << "\r\nSearching all directories.\r\n\n";
    int dn = 0;
    int count = 0;
    int color = 3;
    foundany = 0;
    if (!x_only) {
      bout << "\r|#2Searching ";
    }
    while ((dn < session()->num_dirs) && (udir[dn].subnum != -1)) {
      count++;
      if (!x_only) {
        bout << "|#" << color << ".";
        if (count == NUM_DOTS) {
          bout << "\r";
          bout << "|#2Searching ";
          color++;
          count = 0;
          if (color == 4) {
            color++;
          }
          if (color == 10) {
            color = 0;
          }
        }
      }
      if (lp_try_to_download(szFileName, udir[dn].subnum) < 0) {
        break;
      } else {
        dn++;
      }
    }
    if (!foundany) {
      bout << "File not found.\r\n\n";
    }
  }
}

void request_file(const char *pszFileName) {
  bout.cls();
  bout.nl();

  printfile(LPFREQ_NOEXT);
  bout << "|#2File missing.  Request it? ";

  if (noyes()) {
    ssm(1, 0, "%s is requesting file %s", session()->user()->GetName(), pszFileName);
    bout << "File request sent\r\n";
  } else {
    bout << "File request NOT sent\r\n";
  }
  pausescr();
}

bool ok_listplus() {
  if (!okansi()) {
    return false;
  }

#ifndef FORCE_LP
  if (session()->user()->IsUseNoTagging()) {
    return false;
  }
  if (session()->user()->IsUseListPlus()) {
    return false;
  }
#endif

  if (x_only) {
    return false;
  }
  return true;
}
