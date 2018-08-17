/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include "bbs/batch.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsovl3.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/dirlist.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "local_io/keycodes.h"
#include "bbs/listplus.h"
#include "bbs/lpfunc.h"
#include "bbs/mmkey.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "local_io/wconstants.h"
#include "bbs/bbs.h"
#include "bbs/shortmsg.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "bbs/xfertmp.h"

#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"
#include "sdk/wwivcolors.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// How far from the bottom that the side menu will be on the screen
// This is used because of people who don't set there screenlines up correctly
// It defines how far up from the users screenlines to put the menu
static constexpr int STOP_LIST = 0;
static constexpr int MAX_EXTENDED_SIZE = 1000;

int bulk_move = 0;
int bulk_dir = -1;
bool ext_is_on = false;
listplus_config lp_config;

static char _on_[] = "ON!";
static char _off_[] = "OFF";

static bool lp_config_loaded = false;

// TODO remove this hack and fix the real problem of fake spaces in filenames everywhere
static bool ListPlusExist(const char *file_name) {
  char szRealFileName[MAX_PATH];
  strcpy(szRealFileName, file_name);
  StringRemoveWhitespace(szRealFileName);
  return (*szRealFileName) ? File::Exists(szRealFileName) : false;
}

static void colorize_foundtext(char *text, search_record* search_rec, int color) {
  int size;
  char *pszTempBuffer, found_color[10], normal_color[10], *tok;
  char find[101], word[101];

  sprintf(found_color, "|%02d|%02d", lp_config.found_fore_color, lp_config.found_back_color);
  sprintf(normal_color, "|16|%02d", color);

  if (lp_config.colorize_found_text) {
    to_char_array(find, search_rec->search);
    tok = strtok(find, "&|!()");

    while (tok) {
      pszTempBuffer = text;
      strcpy(word, tok);
      StringTrim(word);

      while (pszTempBuffer && word[0]) {
        if ((pszTempBuffer = strcasestr(pszTempBuffer, word)) != nullptr) {
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

static void colorize_foundtext(string *text, search_record * search_rec, int color) {
  std::unique_ptr<char[]> s(new char[text->size() * 2 + (12 * 10)]);  // extra padding for colorized
  strcpy(s.get(), text->c_str());
  colorize_foundtext(s.get(), search_rec, color);
  text->assign(s.get());
}

static void build_header() {
  int desc_pos = 30;
  string header(" Tag # ");
  if (a()->user()->data.lp_options & cfl_fname) {
    header += "FILENAME";
  }
  if (a()->user()->data.lp_options & cfl_extension) {
    header += ".EXT ";
  }
  if (a()->user()->data.lp_options & cfl_dloads) {
    header += " DL ";
  }
  if (a()->user()->data.lp_options & cfl_kbytes) {
    header += "Bytes ";
  }

  if (a()->user()->data.lp_options & cfl_days_old) {
    header += "Age ";
  }
  if (a()->user()->data.lp_options & cfl_description) {
    desc_pos = header.size();
    header += "Description";
  }
  StringJustify(&header, 79, ' ', JustificationType::LEFT);
  bout << "|23|01" << header << wwiv::endl;

  header.clear();
  if (a()->user()->data.lp_options & cfl_date_uploaded) {
    header += "Date Uploaded      ";
  }
  if (a()->user()->data.lp_options & cfl_upby) {
    header += "Who uploaded";
  }
  if (!header.empty()) {
    StringJustify(&header, desc_pos + header.size(), ' ', JustificationType::RIGHT);
    StringJustify(&header, 79, ' ', JustificationType::LEFT);
    bout << "|23|01" << header << wwiv::endl;
  }
}

static void printtitle_plus_old() {
  bout << "|16|15" << string(79, '\xDC') << wwiv::endl;

  const string buf = StringPrintf("Area %d : %-30.30s (%d files)", to_number<int>(a()->current_user_dir().keys),
          a()->directories[a()->current_user_dir().subnum].name, a()->numf);
  bout.bprintf("|23|01 \xF9 %-56s Space=Tag/?=Help \xF9 \r\n", buf.c_str());

  if (a()->user()->data.lp_options & cfl_header) {
    build_header();
  }

  bout << "|16|08" << string(79, '\xDF') << wwiv::endl;
  bout.Color(0);
}

void printtitle_plus() {
  if (a()->localIO()->WhereY() != 0 || a()->localIO()->WhereX() != 0) {
    bout.cls();
  }

  if (a()->user()->data.lp_options & cfl_header) {
    printtitle_plus_old();
  } else {
    const string buf = StringPrintf("Area %d : %-30.30s (%d files)", to_number<int>(a()->current_user_dir().keys),
            a()->directories[a()->current_user_dir().subnum].name, a()->numf);
    bout.litebarf("%-54s Space=Tag/?=Help", buf.c_str());
    bout.Color(0);
  }
}

static int lp_configured_lines() {
  return (a()->user()->data.lp_options & cfl_date_uploaded || 
          a()->user()->data.lp_options & cfl_upby) ? 3 : 2;
}

int first_file_pos() {
  int pos = FIRST_FILE_POS;
  if (a()->user()->data.lp_options & cfl_header) {
    pos += lp_configured_lines();
  }
  return pos;
}

void print_searching(search_record* search_rec) {
  if (a()->localIO()->WhereY() != 0 || a()->localIO()->WhereX() != 0) {
    bout.cls();
  }

  if (search_rec->search.size() > 3) {
    bout << "|#9Search keywords : |#2" << search_rec->search;
    bout.nl(2);
  }
  bout << "|#9<Space> aborts  : ";
  bout.cls();
  bout.bprintf(" |17|15%-40.40s|16|#0\r",
    a()->directories[a()->current_user_dir().subnum].name);
}

static void catch_divide_by_zero(int signum) {
  if (signum == SIGFPE) {
    sysoplog() << "Caught divide by 0";
  }
}

int listfiles_plus(int type) {
  int save_topdata = a()->topdata;
  auto save_dir = a()->current_user_dir_num();
  long save_status = a()->user()->GetStatus();

  ext_is_on = a()->user()->GetFullFileDescriptions();
  signal(SIGFPE, catch_divide_by_zero);

  a()->topdata = LocalIO::topdataNone;
  a()->UpdateTopScreen();
  bout.cls();

  int nReturn = listfiles_plus_function(type);
  bout.Color(0);
  bout.GotoXY(1, a()->user()->GetScreenLines() - 3);
  bout.nl(3);

  bout.clear_lines_listed();

  if (type != LP_NSCAN_NSCAN) {
    tmp_disable_conf(false);
  }

  a()->user()->SetStatus(save_status);

  if (type == LP_NSCAN_DIR || type == LP_SEARCH_ALL) {    // change Build3
    a()->set_current_user_dir_num(save_dir);
  }
  dliscan();

  a()->topdata = save_topdata;
  a()->UpdateTopScreen();

  return nReturn;
}

int lp_add_batch(const char *file_name, int dn, long fs) {
  double t;

  if (find_batch_queue(file_name) > -1) {
    return 0;
  }

  if (a()->batch().entry.size() >= a()->max_batch) {
    bout.GotoXY(1, a()->user()->GetScreenLines() - 1);
    bout << "No room left in batch queue.\r\n";
    pausescr();
  } else if (!ratio_ok()) {
    pausescr();
  } else {
    if (a()->modem_speed_ && fs) {
      t = (9.0) / ((double)(a()->modem_speed_)) * ((double)(fs));
    } else {
      t = 0.0;
    }
    if ((nsl() <= (a()->batch().dl_time_in_secs() + t)) && (!so())) {
      bout.GotoXY(1, a()->user()->GetScreenLines() - 1);
      bout << "Not enough time left in queue.\r\n";
      pausescr();
    } else {
      if (dn == -1) {
        bout.GotoXY(1, a()->user()->GetScreenLines() - 1);
        bout << "Can't add temporary file to batch queue.\r\n";
        pausescr();
      } else {
        batchrec b{};
        strcpy(b.filename, file_name);
        b.dir = static_cast<int16_t>(dn);
        b.time = static_cast<float>(t);
        b.sending = true;
        b.len = fs;
        a()->batch().entry.emplace_back(b);
        return 1;
      }
    }
  }
  return 0;
}


int printinfo_plus(uploadsrec * u, int filenum, int marked, int LinesLeft, search_record * search_rec) {
  char szFileName[MAX_PATH], szFileExt[MAX_PATH];
  char element[150];
  int chars_this_line = 0, numl = 0, will_fit = 78;
  int char_printed = 0, extdesc_pos;

  strcpy(szFileName, u->filename);
  if (!szFileName[0]) {
    // Make sure the filename isn't empty, if it is then lets bail!
    return numl;
  }

  char* str = strchr(szFileName, '.');
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

  string file_information = StringPrintf("|%02d %c |%02d%3d ", lp_config.tagged_color, marked ? '\xFE' : ' ', lp_config.file_num_color, filenum);
  int width = 7;
  bout.clear_lines_listed();

  string buffer;
  if (a()->user()->data.lp_options & cfl_fname) {
    buffer = szFileName;
    StringJustify(&buffer, 8, ' ', JustificationType::LEFT);
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[0]);
    }
    file_information += StringPrintf("|%02d%s", a()->user()->data.lp_colors[0], buffer.c_str());
    width += 8;
  }
  if (a()->user()->data.lp_options & cfl_extension) {
    buffer = szFileExt;
    StringJustify(&buffer, 3, ' ', JustificationType::LEFT);
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[1]);
    }
    sprintf(element, "|%02d.%s", a()->user()->data.lp_colors[1], buffer.c_str());
    file_information += element;
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_dloads) {
    file_information += StringPrintf(" |%02d%3d", a()->user()->data.lp_colors[2], u->numdloads);
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_kbytes) {
    buffer = StringPrintf("%4luk", bytes_to_k(u->numbytes));
    if (!(a()->directories[a()->current_user_dir().subnum].mask & mask_cdrom)) {
      char szTempFile[MAX_PATH];

      strcpy(szTempFile, a()->directories[a()->current_user_dir().subnum].path);
      strcat(szTempFile, u->filename);
      unalign(szTempFile);
      if (lp_config.check_exist) {
        if (!ListPlusExist(szTempFile)) {
          buffer = "OFFLN";
        }
      }
    }
    sprintf(element, " |%02d%s", a()->user()->data.lp_colors[3], buffer.c_str());
    file_information += element;
    width += 6;
  }

  if (a()->user()->data.lp_options & cfl_days_old) {
    sprintf(element, " |%02d%3d", a()->user()->data.lp_colors[6], nDaysOld);
    file_information += element;
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_description) {
    ++width;
    buffer = u->description;
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[10]);
    }
    sprintf(element, " |%02d%s", a()->user()->data.lp_colors[10], buffer.c_str());
    file_information += element;
    extdesc_pos = width;
  } else {
    extdesc_pos = -1;
  }

  string fi = trim_to_size_ignore_colors(file_information, will_fit);
  fi += "\r\n";
  bout.bputs(fi);
  numl++;

  if (extdesc_pos > 0) {
    int num_extended, lines_left;
    int lines_printed;

    lines_left = LinesLeft - numl;
    num_extended = a()->user()->GetNumExtended();
    if (num_extended < lp_config.show_at_least_extended) {
      num_extended = lp_config.show_at_least_extended;
    }
    if (num_extended > lines_left) {
      num_extended = lines_left;
    }
    if (ext_is_on && mask_extended & u->mask) {
      lines_printed = print_extended_plus(u->filename, num_extended, -extdesc_pos, 
        static_cast<Color>(a()->user()->data.lp_colors[10]), search_rec);
    } else {
      lines_printed = 0;
    }

    if (lines_printed) {
      numl += lines_printed;
      chars_this_line = 0;
      char_printed = 0;
    }
  }
  if (a()->localIO()->WhereX()) {
    if (char_printed) {
      bout.nl();
      ++numl;
    } else {
      bout.bputch('\r');
    }
  }
  file_information.clear();

  if (a()->user()->data.lp_options & cfl_date_uploaded) {
    if ((u->actualdate[2] == '/') && (u->actualdate[5] == '/')) {
      buffer = StringPrintf("UL: %s  New: %s", u->date, u->actualdate);
      StringJustify(&buffer, 27, ' ', JustificationType::LEFT);
    } else {
      buffer = StringPrintf("UL: %s", u->date);
      StringJustify(&buffer, 12, ' ', JustificationType::LEFT);
    }
    sprintf(element, "|%02d%s  ", a()->user()->data.lp_colors[4], buffer.c_str());
    file_information += element;
  }

  if (a()->user()->data.lp_options & cfl_upby) {
    if (a()->user()->data.lp_options & cfl_date_uploaded) {
      StringJustify(&file_information, file_information.size() + width, ' ', JustificationType::RIGHT);
      bout << file_information;
      bout.nl();
      ++numl;
    }
    string tmp = properize(string(u->upby));
    file_information = StringPrintf("|%02dUpby: %-15s", a()->user()->data.lp_colors[7], tmp.c_str());
  }

  if (!buffer.empty()) {
    StringJustify(&file_information, file_information.size() + width, ' ', JustificationType::RIGHT);
    bout << file_information;
    bout.nl();
    ++numl;
  }
  return numl;
}

int print_extended_plus(const char *file_name, int numlist, int indent, Color color,
                        search_record* search_rec) {
  int numl = 0;
  int cpos = 0;
  int chars_this_line = 0;

  int will_fit = 80 - std::abs(indent) - 2;

  string ss = read_extended_description(file_name);

  if (ss.empty()) {
    return 0;
  }
  StringTrimEnd(&ss);
  int nBufferSize = ss.size();
  if (nBufferSize > MAX_EXTENDED_SIZE) {
    nBufferSize = MAX_EXTENDED_SIZE;
    ss.resize(MAX_EXTENDED_SIZE);
  }
  auto new_ss = std::make_unique<char[]>((nBufferSize * 4) + 30);
  strcpy(new_ss.get(), ss.c_str());
  if (search_rec) {
    colorize_foundtext(new_ss.get(), search_rec, static_cast<uint8_t>(color));
  }
  if (indent > -1 && indent != 16) {
    bout << "  |#9Extended Description:\n\r";
  }
  char ch = SOFTRETURN;

  while (new_ss[cpos] && numl < numlist && !a()->hangup_) {
    if (ch == SOFTRETURN && indent) {
      bout.SystemColor(static_cast<uint8_t>(color));
      bout.bputch('\r');
      bout.Right(std::abs(indent));
    }
    do {
      ch = new_ss[cpos++];
    } while (ch == '\r' && !a()->hangup_);

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
      chars_this_line += bout.bputch(ch);
    }
  }

  if (a()->localIO()->WhereX()) {
    bout.nl();
    ++numl;
  }
  bout.Color(0);
  return numl;
}

void show_fileinfo(uploadsrec * u) {
  bout.cls();
  bout.Color(7);
  bout << string(78, '\xCD');
  bout.nl();
  bout << "  |#9Filename    : |#2" << u->filename << wwiv::endl;
  bout << "  |#9Uploaded on : |#2" << u->date << " by |#2" << u->upby << wwiv::endl;
  if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
    bout << "  |#9Newest file : |#2" << u->actualdate << wwiv::endl;
  }
  bout << "  |#9Size        : |#2" << bytes_to_k(u->numbytes) << wwiv::endl;
  bout << "  |#9Downloads   : |#2" << u->numdloads << "|#9" << wwiv::endl;
  bout << "  |#9Description : |#2" << u->description << wwiv::endl;
  print_extended_plus(u->filename, 255, 16, Color::YELLOW, nullptr);
  bout.Color(7);
  bout << string(78, '\xCD');
  bout.nl();
  pausescr();
}

int check_lines_needed(uploadsrec * u) {

  int max_lines = calc_max_lines();
  int num_extended = a()->user()->GetNumExtended();

  if (num_extended < lp_config.show_at_least_extended) {
    num_extended = lp_config.show_at_least_extended;
  }

  if (max_lines > num_extended) {
    max_lines = num_extended;
  }

  int elines = 0;
  if (a()->user()->data.lp_options & cfl_description) {
    int max_extended = a()->user()->GetNumExtended();

    if (max_extended < lp_config.show_at_least_extended) {
      max_extended = lp_config.show_at_least_extended;
    }

    string ss;
    if (ext_is_on && mask_extended & u->mask) {
      ss = read_extended_description(u->filename);
    }

    if (!ss.empty()) {
      const char* tmp = ss.c_str();
      while ((elines < max_extended) && ((tmp = strchr(tmp, '\r')) != nullptr)) {
        ++elines;
        ++tmp;
      }
    }
  }
  const int lc_lines_used = lp_configured_lines();
  if (lc_lines_used + elines > max_lines) {
    return max_lines - 1;
  }

  return lc_lines_used + elines;
}

int prep_search_rec(search_record* search_rec, int type) {
  search_rec->search_extended = lp_config.search_extended_on ? true : false;

  if (type == LP_LIST_DIR) {
    search_rec->filemask = file_mask();
    search_rec->alldirs = THIS_DIR;
  } else if (type == LP_SEARCH_ALL) {
    search_rec->alldirs = ALL_DIRS;
    if (!search_criteria(search_rec)) {
      return 0;
    }
  } else if (type == LP_NSCAN_DIR) {
    search_rec->alldirs = THIS_DIR;
    search_rec->nscandate = a()->context().nscandate();
  } else if (type == LP_NSCAN_NSCAN) {
    a()->context().scanned_files(true);
    search_rec->nscandate = a()->context().nscandate();
    search_rec->alldirs = ALL_DIRS;
  } else {
    sysoplog() << "Undef LP type";
    return 0;
  }

  if (search_rec->filemask.empty() 
      && !search_rec->nscandate
      && !search_rec->search[0]) {
    return 0;
  }

  if (!search_rec->filemask.empty()) {
    if (!contains(search_rec->filemask, '.')) {
      search_rec->filemask += ".*";
    }
  }
  align(&search_rec->filemask);
  return 1;
}

int calc_max_lines() {
  if (lp_config.max_screen_lines_to_show) {
    return std::min<int>(a()->user()->GetScreenLines(),
      lp_config.max_screen_lines_to_show) -
      (first_file_pos() + 1 + STOP_LIST);
  }
  return a()->user()->GetScreenLines() - (first_file_pos() + 1 + STOP_LIST);
}

static void check_lp_colors() {
  bool needs_defaults = false;
  auto u = a()->user();
  for (int i = 0; i < 32; i++) {
    if (!u->data.lp_colors[i]) {
      needs_defaults = true;
      u->data.lp_colors[i] = static_cast<uint8_t>(Color::CYAN);
    }
  }

  if (needs_defaults) {
    u->data.lp_colors[0] = static_cast<uint8_t>(Color::LIGHTGREEN);
    u->data.lp_colors[1] = static_cast<uint8_t>(Color::LIGHTGREEN);
    u->data.lp_colors[4] = static_cast<uint8_t>(Color::LIGHTCYAN);
    u->data.lp_colors[5] = static_cast<uint8_t>(Color::LIGHTCYAN);
    u->data.lp_colors[10] = static_cast<uint8_t>(Color::LIGHTCYAN);
  }
}

void save_lp_config() {
  if (lp_config_loaded) {
    File fileConfig(FilePath(a()->config()->datadir(), LISTPLUS_CFG));
    if (fileConfig.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite)) {
      fileConfig.Write(&lp_config, sizeof(struct listplus_config));
      fileConfig.Close();
    }
  }
}

void load_lp_config() {
  if (!lp_config_loaded) {
    File fileConfig(FilePath(a()->config()->datadir(), LISTPLUS_CFG));
    if (!fileConfig.Open(File::modeBinary | File::modeReadOnly)) {
      memset(&lp_config, 0, sizeof(listplus_config));

      lp_config.normal_highlight  = (static_cast<uint8_t>(Color::YELLOW) + (static_cast<uint8_t>(Color::BLACK) << 4));
      lp_config.normal_menu_item  = (static_cast<uint8_t>(Color::CYAN) + (static_cast<uint8_t>(Color::BLACK) << 4));
      lp_config.current_highlight = (static_cast<uint8_t>(Color::BLUE) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4));
      lp_config.current_menu_item = (static_cast<uint8_t>(Color::BLACK) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4));

      lp_config.tagged_color      = static_cast<uint8_t>(Color::LIGHTGRAY);
      lp_config.file_num_color    = static_cast<uint8_t>(Color::GREEN);

      lp_config.found_fore_color  = static_cast<uint8_t>(Color::RED);
      lp_config.found_back_color  = static_cast<uint8_t>(Color::LIGHTGRAY) + 16;

      lp_config.current_file_color = static_cast<uint8_t>(Color::BLACK) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4);

      lp_config.max_screen_lines_to_show = 24;
      lp_config.show_at_least_extended = 5;

      lp_config.edit_enable = 1;            // Do or don't let users edit their config
      lp_config.request_file = 1;           // Do or don't use file request
      lp_config.colorize_found_text = 1;    // Do or don't colorize found text
      lp_config.search_extended_on = 0;     // Defaults to either on or off in adv search, or is either on or off in simple search
      lp_config.simple_search = 1;          // Which one is entered when searching, can switch to other still
      lp_config.no_configuration = 0;       // Toggles configurable menus on and off
      lp_config.check_exist = 1;            // Will check to see if the file exists on hardrive and put N/A if not
      lp_config_loaded = true;
      lp_config.forced_config = 0;

      save_lp_config();
    } else {
      fileConfig.Read(&lp_config, sizeof(listplus_config));
      lp_config_loaded = true;
      fileConfig.Close();
    }
  }
  check_lp_colors();
}

void sysop_configure() {
  short color = 0;
  bool done = false;
  char s[201];

  if (!so()) {
    return;
  }

  load_lp_config();

  while (!done && !a()->hangup_) {
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
    bout << "|#9Q-Quit ";
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
      lp_config.max_screen_lines_to_show = to_number<int16_t>(s);
      break;
    case 'K':
      bout << "Enter minimum extended description lines to show ";
      input(s, 2, true);
      lp_config.show_at_least_extended = to_number<int16_t>(s);
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

  if (a()->user()->HasColor()) {
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
      if ((a()->user()->GetBWColor(1) & 0x70) == 0) {
        nc = 0 | ((a()->user()->GetBWColor(1) & 0x07) << 4);
      } else {
        nc = (a()->user()->GetBWColor(1) & 0x70);
      }
    } else {
      if ((a()->user()->GetBWColor(1) & 0x70) == 0) {
        nc = 0 | (a()->user()->GetBWColor(1) & 0x07);
      } else {
        nc = ((a()->user()->GetBWColor(1) & 0x70) >> 4);
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

static void update_user_config_screen(uploadsrec* u, int which) {
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

  uint8_t color_background = static_cast<uint8_t>(Color::BLUE) << 4;
  uint8_t color_selected = static_cast<uint8_t>(Color::LIGHTRED) | color_background;
  uint8_t color_notselected = static_cast<uint8_t>(Color::BLACK) | color_background;
  uint8_t color_colortext = static_cast<uint8_t>(Color::LIGHTCYAN) | color_background;
  auto& lpo = a()->user()->data.lp_options;
  auto& lpc = a()->user()->data.lp_colors;
  
  if (which < 1 || which == 1) {
    bout.GotoXY(37, 4);
    bout.SystemColor(lpo & cfl_fname ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[0]];
  }
  if (which < 1 || which == 2) {
    bout.GotoXY(37, 5);
    bout.SystemColor(lpo & cfl_extension ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[1]];
  }
  if (which < 1 || which == 3) {
    bout.GotoXY(37, 6);
    bout.SystemColor(lpo & cfl_dloads ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[2]];
  }
  if (which < 1 || which == 4) {
    bout.GotoXY(37, 7);
    bout.SystemColor(lpo & cfl_kbytes ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[3]];
  }
  if (which < 1 || which == 5) {
    bout.GotoXY(37, 8);
    bout.SystemColor(lpo & cfl_description ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[10]];
  }
  if (which < 1 || which == 6) {
    bout.GotoXY(37, 9);
    bout.SystemColor(lpo & cfl_date_uploaded ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[4]];
  }
  if (which < 1 || which == 7) {
    bout.GotoXY(37, 10);
    bout.SystemColor(lpo & cfl_file_points ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[5]];
  }
  if (which < 1 || which == 8) {
    bout.GotoXY(37, 11);
    bout.SystemColor(lpo & cfl_days_old ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[6]];
  }
  if (which < 1 || which == 9) {
    bout.GotoXY(37, 12);
    bout.SystemColor(lpo & cfl_upby ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
    bout << lp_color_list[lpc[7]];
  }
  if (which < 1 || which == 10) {
    bout.GotoXY(37, 13);
    bout.SystemColor(lpo & cfl_header ? color_selected : color_notselected);
    bout << "\xFE ";
    bout.SystemColor(color_colortext);
  }
  bout.SystemColor(Color::YELLOW);
  bout.GotoXY(1, 21);
  bout.clreol();
  bout.nl();
  bout.clreol();
  bout.GotoXY(1, 21);

  search_record sr{};
  printinfo_plus(u, 1, 1, 30, &sr);
  bout.GotoXY(30, 17);
  bout.SystemColor(Color::YELLOW);
  bout.bs();
}

void config_file_list() {
  int key, which = -1;
  unsigned long bit = 0L;
  char action[51];
  uploadsrec u = {};

  strcpy(u.filename, "WWIV52.ZIP");
  strcpy(u.description, "This is a sample description!");
  to_char_array(u.date, date());
  const string username_num = a()->names()->UserName(a()->usernum);
  to_char_array(u.upby, username_num);
  u.numdloads = 50;
  u.numbytes = 655535L;
  u.daten = daten_t_now() - 10000;

  load_lp_config();

  bout.cls();
  printfile(LPCONFIG_NOEXT);
  if (!a()->user()->data.lp_options & cfl_fname) {
    a()->user()->data.lp_options |= cfl_fname;
  }

  if (!(a()->user()->data.lp_options & cfl_description)) {
    a()->user()->data.lp_options |= cfl_description;
  }

  action[0] = '\0';
  bool done = false;
  while (!done && !a()->hangup_) {
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

      if (a()->user()->data.lp_options & bit) {
        a()->user()->data.lp_options &= ~bit;
      } else {
        a()->user()->data.lp_options |= bit;
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

      ++a()->user()->data.lp_colors[bit];
      if (a()->user()->data.lp_colors[bit] > 15) {
        a()->user()->data.lp_colors[bit] = 1;
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  }
  bout.nl(4);
}

static int rename_filename(const char *file_name, int dn) {
  char s1[81], s2[81], ch;
  int cp, ret = 1;
  uploadsrec u;

  dliscan1(dn);
  string orig_aligned_filename;
  {
    string s = file_name;
    if (s.empty()) {
      return 1;
    }
    if (!contains(s, '.')) s += ".*";
    align(&s);
    orig_aligned_filename = s;
  }

  int i = recno(orig_aligned_filename);
  while (i > 0) {
    File fileDownload(a()->download_filename_);
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
      i = nrecno(orig_aligned_filename, cp);
      continue;
    }
    bout.nl();
    bout << "|#2New filename? ";
    string new_filename = input(12);
    if (!okfn(new_filename)) {
      new_filename.clear();
    }
    if (!new_filename.empty()) {
      align(&new_filename);
      if (new_filename != "        .   ") {
        strcpy(s1, a()->directories[dn].path);
        strcpy(s2, s1);
        strcat(s1, new_filename.c_str());
        if (ListPlusExist(s1)) {
          bout << "Filename already in use; not changed.\r\n";
        } else {
          strcat(s2, u.filename);
          File::Rename(s2, s1);
          if (ListPlusExist(s1)) {
            string ss = read_extended_description(u.filename);
            if (!ss.empty()) {
              delete_extended_description(u.filename);
              add_extended_description(new_filename.c_str(), ss);
            }
            strcpy(u.filename, new_filename.c_str());
          } else {
            bout << "Bad filename.\r\n";
          }
        }
      }
    }
    bout.nl();
    bout << "New description:\r\n|#2: ";
    string desc = Input1(u.description, 58, false, wwiv::bbs::InputMode::MIXED);
    if (!desc.empty()) {
      strcpy(u.description, desc.c_str());
    }
    string ss = read_extended_description(u.filename);
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (yesno()) {
      bout.nl();
      if (!ss.empty()) {
        bout << "|#5Delete it? ";
        if (yesno()) {
          delete_extended_description(u.filename);
          u.mask &= ~mask_extended;
        } else {
          u.mask |= mask_extended;
          modify_extended_description(&ss, a()->directories[dn].name);
          if (!ss.empty()) {
            delete_extended_description(u.filename);
            add_extended_description(u.filename, ss);
          }
        }
      } else {
        modify_extended_description(&ss, a()->directories[dn].name);
        if (!ss.empty()) {
          add_extended_description(u.filename, ss);
          u.mask |= mask_extended;
        } else {
          u.mask &= ~mask_extended;
        }
      }
    } else if (!ss.empty()) {
      u.mask |= mask_extended;
    } else {
      u.mask &= ~mask_extended;
    }
    if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      FileAreaSetRecord(fileDownload, i);
      fileDownload.Write(&u, sizeof(uploadsrec));
      fileDownload.Close();
    }
    i = nrecno(orig_aligned_filename, cp);
  }
  return ret;
}

static int remove_filename(const char *file_name, int dn) {
  int ret = 1;
  char szTempFileName[MAX_PATH];
  uploadsrec u;
  memset(&u, 0, sizeof(uploadsrec));

  dliscan1(dn);
  strcpy(szTempFileName, file_name);

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
  while (!a()->hangup_ && i > 0 && !abort) {
    File fileDownload(a()->download_filename_);
    if (fileDownload.Open(File::modeReadOnly | File::modeBinary)) {
      FileAreaSetRecord(fileDownload, i);
      fileDownload.Read(&u, sizeof(uploadsrec));
      fileDownload.Close();
    }
    if (dcs() || (u.ownersys == 0 && u.ownerusr == a()->usernum)) {
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
          if (a()->HasConfigFlag(OP_FLAGS_FAST_SEARCH)) {
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
          File::Remove(a()->directories[dn].path, u.filename);
          if (rdlp && u.ownersys == 0) {
            User user;
            a()->users()->readuser(&user, u.ownerusr);
            if (!user.IsUserDeleted()) {
              if (date_to_daten(user.GetFirstOn()) < u.daten) {
                user.SetFilesUploaded(user.GetFilesUploaded() - 1);
                user.SetUploadK(user.GetUploadK() - bytes_to_k(u.numbytes));
                a()->users()->writeuser(&user, u.ownerusr);
              }
            }
          }
        }
        if (u.mask & mask_extended) {
          delete_extended_description(u.filename);
        }
        sysoplog() << "- '" << u.filename << "' removed off of " << a()->directories[dn].name;

        if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          for (int i1 = i; i1 < a()->numf; i1++) {
            FileAreaSetRecord(fileDownload, i1 + 1);
            fileDownload.Read(&u, sizeof(uploadsrec));
            FileAreaSetRecord(fileDownload, i1);
            fileDownload.Write(&u, sizeof(uploadsrec));
          }
          --i;
          --a()->numf;
          FileAreaSetRecord(fileDownload, 0);
          fileDownload.Read(&u, sizeof(uploadsrec));
          u.numbytes = a()->numf;
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

static int move_filename(const char *file_name, int dn) {
  char szTempMoveFileName[81], szSourceFileName[MAX_PATH], szDestFileName[MAX_PATH];
  int nDestDirNum = -1, ret = 1;
  uploadsrec u, u1;

  strcpy(szTempMoveFileName, file_name);
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

  while (!a()->hangup_ && nRecNum > 0 && !done) {
    int cp = nRecNum;
    File fileDownload(a()->download_filename_);
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
      sprintf(szSourceFileName, "%s%s", a()->directories[dn].path, u.filename);
      if (!bulk_move) {
        string ss;
        do {
          bout.nl(2);
          bout << "|#2To which directory? ";
          ss = mmkey(MMKeyAreaType::dirs);
          if (ss[0] == '?') {
            dirlist(1);
            dliscan1(dn);
          }
        } while (!a()->hangup_ && ss[0] == '?');

        nDestDirNum = -1;
        if (ss[0]) {
          for (size_t i1 = 0; (i1 < a()->directories.size()) && (a()->udir[i1].subnum != -1); i1++) {
            if (ss == a()->udir[i1].keys) {
              nDestDirNum = i1;
            }
          }
        }

        if (a()->batch().entry.size() > 1) {
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
        nDestDirNum = a()->udir[nDestDirNum].subnum;
        dliscan1(nDestDirNum);
        if (recno(u.filename) > 0) {
          ok = false;
          bout.nl();
          bout << "Filename already in use in that directory.\r\n";
        }
        if (a()->numf >= a()->directories[nDestDirNum].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (File::freespace_for_path(a()->directories[nDestDirNum].path) < static_cast<long>(u.numbytes / 1024L) + 3) {
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
          u.daten = daten_t_now();
        }
      } else {
        u.daten = daten_t_now();
      }
      --cp;
      if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        for (int i2 = nRecNum; i2 < a()->numf; i2++) {
          FileAreaSetRecord(fileDownload, i2 + 1);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i2);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        --a()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = a()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
      }
      string ss = read_extended_description(u.filename);
      if (!ss.empty()) {
        delete_extended_description(u.filename);
      }
      sprintf(szDestFileName, "%s%s", a()->directories[nDestDirNum].path, u.filename);
      dliscan1(nDestDirNum);
      if (fileDownload.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
        for (int i = a()->numf; i >= 1; i--) {
          FileAreaSetRecord(fileDownload, i);
          fileDownload.Read(&u1, sizeof(uploadsrec));
          FileAreaSetRecord(fileDownload, i + 1);
          fileDownload.Write(&u1, sizeof(uploadsrec));
        }
        FileAreaSetRecord(fileDownload, 1);
        fileDownload.Write(&u, sizeof(uploadsrec));
        ++a()->numf;
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Read(&u1, sizeof(uploadsrec));
        u1.numbytes = a()->numf;
        if (u.daten > u1.daten) {
          u1.daten = u.daten;
        }
        FileAreaSetRecord(fileDownload, 0);
        fileDownload.Write(&u1, sizeof(uploadsrec));
        fileDownload.Close();
      }
      if (!ss.empty()) {
        add_extended_description(u.filename, ss);
      }
      if (!IsEquals(szSourceFileName, szDestFileName) &&
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

void do_batch_sysop_command(int mode, const char *file_name) {
  auto save_curdir = a()->current_user_dir_num();
  bout.cls();

  if (a()->batch().numbatchdl() > 0) {
    bool done = false;
    for (auto it = begin(a()->batch().entry); it != end(a()->batch().entry) && !done; it++) {
      const auto& b = *it;
      if (b.sending) {
        switch (mode) {
        case SYSOP_DELETE:
          if (!remove_filename(b.filename, b.dir)) {
            done = true;
          } else {
            it = delbatch(it);
          }
          break;
        case SYSOP_RENAME:
          if (!rename_filename(b.filename, b.dir)) {
            done = true;
          } else {
            it = delbatch(it);
          }
          break;
        case SYSOP_MOVE:
          if (!move_filename(b.filename, b.dir)) {
            done = true;
          } else {
            it = delbatch(it);
          }
          break;
        }
      }
    }
  } else {
    // Just act on the single file
    switch (mode) {
    case SYSOP_DELETE:
      remove_filename(file_name, a()->current_user_dir().subnum);
      break;
    case SYSOP_RENAME:
      rename_filename(file_name, a()->current_user_dir().subnum);
      break;
    case SYSOP_MOVE:
      move_filename(file_name, a()->current_user_dir().subnum);
      break;
    }
  }
  a()->set_current_user_dir_num(save_curdir);
  dliscan();
}

int search_criteria(search_record * sr) {
  int x = 0;
  int all_conf = 1, useconf;
  char s1[81];

  useconf = (a()->uconfdir[1].confnum != -1 && okconf(a()->user()));


LP_SEARCH_HELP:
  sr->search_extended = lp_config.search_extended_on ? true : false;

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

    bout << "|#9A)|#2 Filename (wildcards) :|#2 " << sr->filemask << wwiv::endl;
    bout << "|#9B)|#2 Text (no wildcards)  :|#2 " << sr->search << wwiv::endl;
    if (okconf(a()->user())) {
      sprintf(s1, "%s", stripcolors(a()->directories[a()->current_user_dir().subnum].name));
    } else {
      sprintf(s1, "%s", stripcolors(a()->directories[a()->current_user_dir().subnum].name));
    }
    bout << "|#9C)|#2 Which Directories    :|#2 " << (sr->alldirs == THIS_DIR ? s1 : sr->alldirs == ALL_DIRS ?
                       "All dirs" : "Dirs in NSCAN") << wwiv::endl;
    to_char_array(s1, stripcolors(a()->dirconfs[a()->uconfdir[a()->GetCurrentConferenceFileArea()].confnum].conf_name));
    bout << "|#9D)|#2 Which Conferences    :|#2 " << (all_conf ? "All Conferences" : s1) << wwiv::endl;
    bout << "|#9E)|#2 Extended Description :|#2 " << (sr->search_extended ? "Yes" : "No ") << wwiv::endl;
    bout.nl();
    bout << "|15Select item to change |#2<CR>|15 to start search |#2Q=Quit|15:|#0 ";

    x = onek("QABCDE\r?");

    switch (x) {
    case 'A':
      bout << "Filename (wildcards okay) : ";
      sr->filemask = input(12, true);
      if (sr->filemask[0]) {
        if (okfn(sr->filemask)) {
          if (sr->filemask.size() < 8) {
            sysoplog() << "Filespec: " << sr->filemask;
          } else {
            if (contains(sr->filemask, '.')) {
              sysoplog() << "Filespec: " << sr->filemask;
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
      sr->search = input(60, true);
      if (sr->search[0]) {
        sysoplog() << "Keyword: " << sr->search;
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

void view_file(const char *file_name) {
  char szBuffer[30];
  int i, i1;
  uploadsrec u;

  bout.cls();

  strcpy(szBuffer, file_name);
  unalign(szBuffer);

  dliscan();
  bool abort = false;
  i = recno(file_name);
  do {
    if (i > 0) {
      File fileDownload(a()->download_filename_);
      if (fileDownload.Open(File::modeBinary | File::modeReadOnly)) {
        FileAreaSetRecord(fileDownload, i);
        fileDownload.Read(&u, sizeof(uploadsrec));
        fileDownload.Close();
      }
      i1 = list_arc_out(stripfn(u.filename), a()->directories[a()->current_user_dir().subnum].path);
      if (i1) {
        abort = true;
      }
      checka(&abort);
      i = nrecno(file_name, i);
    }
  } while (i > 0 && !a()->hangup_ && !abort);
  bout.nl();
  pausescr();
}

int lp_try_to_download(const char *file_mask, int dn) {
  int i, rtn, ok2;
  bool abort = false;
  uploadsrec u;
  char s1[81], s3[81];

  dliscan1(dn);
  i = recno(file_mask);
  if (i <= 0) {
    checka(&abort);
    return (abort) ? -1 : 0;
  }
  bool ok = true;

  foundany = 1;
  do {
    a()->tleft(true);
    File fileDownload(a()->download_filename_);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, i);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();

    ok2 = 0;
    if (!ok2 && (!(u.mask & mask_no_ratio))) {
      if (!ratio_ok()) {
        return -2;
      }
    }

    write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
    sprintf(s1, "%s%s", a()->directories[dn].path, u.filename);
    sprintf(s3, "%-40.40s", u.description);
    abort = 0;
    rtn = add_batch(s3, u.filename, dn, u.numbytes);
    s3[0] = 0;

    if (abort || (rtn == -3)) {
      ok = false;
    } else {
      i = nrecno(file_mask, i);
    }
  } while ((i > 0) && ok && !a()->hangup_);
  if (rtn == -2) {
    return -2;
  }
  if (abort || rtn == -3) {
    return -1;
  } else {
    return 1;
  }
}

void download_plus(const char *file_name) {
  char szFileName[MAX_PATH];

  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (noyes()) {
      batchdl(1);
      return;
    }
  }
  strcpy(szFileName, file_name);
  if (szFileName[0] == '\0') {
    return;
  }
  if (strchr(szFileName, '.') == nullptr) {
    strcat(szFileName, ".*");
  }
  align(szFileName);
  if (lp_try_to_download(szFileName, a()->current_user_dir().subnum) == 0) {
    bout << "\r\nSearching all a()->directories.\r\n\n";
    size_t dn = 0;
    int count = 0;
    int color = 3;
    foundany = 0;
    bout << "\r|#2Searching ";
    while ((dn < a()->directories.size()) && (a()->udir[dn].subnum != -1)) {
      count++;
      bout.Color(color);
      bout << ".";
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
      if (lp_try_to_download(szFileName, a()->udir[dn].subnum) < 0) {
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

void request_file(const char *file_name) {
  bout.cls();
  bout.nl();

  printfile(LPFREQ_NOEXT);
  bout << "|#2File missing.  Request it? ";

  if (noyes()) {
    ssm(1) << a()->user()->GetName() << " is requesting file " << file_name;
    bout << "File request sent\r\n";
  } else {
    bout << "File request NOT sent\r\n";
  }
  pausescr();
}
