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
#include "bbs/listplus.h"
#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/dirlist.h"
#include "bbs/instmsg.h"
#include "bbs/lpfunc.h"
#include "bbs/mmkey.h"
#include "bbs/shortmsg.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "bbs/xferovl.h"
#include "bbs/xferovl1.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "common/pause.h"
#include "core/numbers.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/dirs.h"
#include "sdk/files/files.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/wwivcolors.h"
#include <algorithm>
#include <csignal>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// How far from the bottom that the side menu will be on the screen
// This is used because of people who don't set there screenlines up correctly
// It defines how far up from the users screenlines to put the menu
static constexpr int STOP_LIST = 0;

int bulk_move = 0;
int bulk_dir = -1;
bool ext_is_on = false;
listplus_config lp_config;

static char _on_[] = "ON!";
static char _off_[] = "OFF";

static bool lp_config_loaded = false;

// TODO(rushfan): Convert this to use strings.
static void colorize_foundtext(char* text, search_record* search_rec, int color) {
  char found_color[10], normal_color[10];
  char find[101], word[101];

  sprintf(found_color, "|%02d|%02d", lp_config.found_fore_color, lp_config.found_back_color);
  sprintf(normal_color, "|16|%02d", color);

  if (lp_config.colorize_found_text) {
    to_char_array(find, search_rec->search);
    char* tok = strtok(find, "&|!()");

    while (tok) {
      char* temp_buffer = text;
      strcpy(word, tok);
      StringTrim(word);

      while (temp_buffer && word[0]) {
        if ((temp_buffer = strcasestr(temp_buffer, word)) != nullptr) {
          int size = ssize(temp_buffer) + 1;
          memmove(&temp_buffer[6], &temp_buffer[0], size);
          strncpy(temp_buffer, found_color, 6);
          temp_buffer += strlen(word) + 6;
          size = ssize(temp_buffer) + 1;
          memmove(&temp_buffer[6], &temp_buffer[0], size);
          strncpy(temp_buffer, normal_color, 6);
          temp_buffer += 6;
          ++temp_buffer;
        }
      }
      tok = strtok(nullptr, "&|!()");
    }
  }
}

static void colorize_foundtext(std::string* text, search_record* search_rec, int color) {
  std::unique_ptr<char[]> s(new char[text->size() * 2 + (12 * 10)]); // extra padding for colorized
  strcpy(s.get(), text->c_str());
  colorize_foundtext(s.get(), search_rec, color);
  text->assign(s.get());
}

static void build_header() {
  int desc_pos = 30;
  std::string header(" Tag # ");
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
    desc_pos = size_int(header);
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
    StringJustify(&header, desc_pos + ssize(header), ' ', JustificationType::RIGHT);
    StringJustify(&header, 79, ' ', JustificationType::LEFT);
    bout << "|23|01" << header << wwiv::endl;
  }
}

static void printtitle_plus_old() {
  bout << "|16|15" << std::string(79, '\xDC') << wwiv::endl;

  const auto buf =
      fmt::sprintf("Area %d : %-30.30s (%d files)", to_number<int>(a()->current_user_dir().keys),
                   a()->dirs()[a()->current_user_dir().subnum].name,
                   a()->current_file_area()->number_of_files());
  bout.bprintf("|23|01 \xF9 %-56s Space=Tag/?=Help \xF9 \r\n", buf);

  if (a()->user()->data.lp_options & cfl_header) {
    build_header();
  }

  bout << "|16|08" << std::string(79, '\xDF') << wwiv::endl;
  bout.Color(0);
}

void printtitle_plus() {
  bout.cls();

  if (a()->user()->data.lp_options & cfl_header) {
    printtitle_plus_old();
  } else {
    const auto buf =
        fmt::format("Area {} : {: <30} ({} files)", to_number<int>(a()->current_user_dir().keys),
                    a()->dirs()[a()->current_user_dir().subnum].name,
                    a()->current_file_area()->number_of_files());
    bout.litebar(fmt::format("{:<54} Space=Tag/?=Help", buf));
    bout.Color(0);
  }
}

static int lp_configured_lines() {
  return (a()->user()->data.lp_options & cfl_date_uploaded ||
          a()->user()->data.lp_options & cfl_upby)
           ? 3
           : 2;
}

int first_file_pos() {
  int pos = FIRST_FILE_POS;
  if (a()->user()->data.lp_options & cfl_header) {
    pos += lp_configured_lines();
  }
  return pos;
}

void print_searching(search_record* search_rec) {
  bout.cls();

  if (search_rec->search.size() > 3) {
    bout << "|#9Search keywords : |#2" << search_rec->search;
    bout.nl(2);
  }
  bout << "|#9<Space> aborts  : ";
  bout.cls();
  bout.bprintf(" |17|15%-40.40s|16|#0\r",
                       a()->dirs()[a()->current_user_dir().subnum].name);
}

static void catch_divide_by_zero(int signum) {
  if (signum == SIGFPE) {
    sysoplog() << "Caught divide by 0";
  }
}

int listfiles_plus(int type) {
  const auto save_topdata = bout.localIO()->topdata();
  const auto save_dir = a()->current_user_dir_num();
  const long save_status = a()->user()->get_status();

  ext_is_on = a()->user()->full_descriptions();
  signal(SIGFPE, catch_divide_by_zero);

  bout.localIO()->topdata(wwiv::local::io::LocalIO::topdata_t::none);
  a()->UpdateTopScreen();
  bout.cls();

  const int r = listfiles_plus_function(type);
  // Stop trailing bits left on screen.
  bout.cls();
  bout.Color(0);
  bout.GotoXY(1, a()->user()->screen_lines() - 3);
  bout.nl(3);

  bout.clear_lines_listed();

  if (type != LP_NSCAN_NSCAN) {
    tmp_disable_conf(false);
  }

  a()->user()->SetStatus(save_status);

  if (type == LP_NSCAN_DIR || type == LP_SEARCH_ALL) {
    // change Build3
    a()->set_current_user_dir_num(save_dir);
  }
  dliscan();

  bout.localIO()->topdata(save_topdata);
  a()->UpdateTopScreen();

  return r;
}

int lp_add_batch(const std::string& file_name, int dn, int fs) {
  if (a()->batch().FindBatch(file_name) > -1) {
    return 0;
  }

  if (a()->batch().size() >= a()->max_batch) {
    bout.GotoXY(1, a()->user()->screen_lines() - 1);
    bout << "No room left in batch queue.\r\n";
    bout.pausescr();
  } else if (!ratio_ok()) {
    bout.pausescr();
  } else {
    if (nsl() <= a()->batch().dl_time_in_secs() + time_to_transfer(a()->modem_speed_, fs).count() &&
        !so()) {
      bout.GotoXY(1, a()->user()->screen_lines() - 1);
      bout << "Not enough time left in queue.\r\n";
      bout.pausescr();
    } else {
      if (dn == -1) {
        bout.GotoXY(1, a()->user()->screen_lines() - 1);
        bout << "Can't add temporary file to batch queue.\r\n";
        bout.pausescr();
      } else {
        a()->batch().AddBatch({file_name, dn, fs, true});
        return 1;
      }
    }
  }
  return 0;
}


int printinfo_plus(uploadsrec* u, int filenum, int marked, int LinesLeft,
                   search_record* search_rec) {
  int numl = 0, will_fit = 78;
  int char_printed = 0, extdesc_pos;

  files::FileName filename(u->filename);
  const auto& fn = filename.aligned_filename();
  if (fn.empty()) {
    // Make sure the filename isn't empty, if it is then lets bail!
    return 0; 
  }
  std::filesystem::path p{fn};
  const auto dot_idx = fn.find('.');
  const auto extension =
      fmt::format("{:<3}", dot_idx == std::string::npos ? "" : fn.substr(dot_idx + 1));
  // stem includes " around the filename.
  const auto basename =
      fmt::format("{:<8}", dot_idx == std::string::npos ? fn : fn.substr(0, dot_idx));

  auto now = DateTime::now().to_time_t();
  auto diff_time = static_cast<long>(difftime(now, u->daten));
  int days_old = diff_time / SECONDS_PER_DAY;

  auto file_information = fmt::sprintf("|%02d %c |%02d%3d ", lp_config.tagged_color,
                                       marked ? '\xFE' : ' ', lp_config.file_num_color, filenum);
  int width = 7;
  bout.clear_lines_listed();

  std::string buffer;
  if (a()->user()->data.lp_options & cfl_fname) {
    buffer = basename;
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[0]);
    }
    file_information += fmt::format("|{:02}{}", a()->user()->data.lp_colors[0], buffer);
    width += 8;
  }
  if (a()->user()->data.lp_options & cfl_extension) {
    buffer = extension;
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[1]);
    }
    file_information += fmt::sprintf("|%02d.%s", a()->user()->data.lp_colors[1], buffer);
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_dloads) {
    file_information += fmt::sprintf(" |%02d%3d", a()->user()->data.lp_colors[2], u->numdloads);
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_kbytes) {
    buffer = humanize(u->numbytes);
    if (!(a()->dirs()[a()->current_user_dir().subnum].mask & mask_cdrom)) {
      auto stf = FilePath(a()->dirs()[a()->current_user_dir().subnum].path,
                          files::FileName(u->filename));
      if (lp_config.check_exist) {
        if (!File::Exists(stf)) {
          buffer = "OFFLN";
        }
      }
    }
    file_information += fmt::format(" |{:02}{:>4} ", a()->user()->data.lp_colors[3], buffer);
    width += 6;
  }

  if (a()->user()->data.lp_options & cfl_days_old) {
    file_information += fmt::sprintf(" |%02d%3d", a()->user()->data.lp_colors[6], days_old);
    width += 4;
  }
  if (a()->user()->data.lp_options & cfl_description) {
    ++width;
    buffer = u->description;
    if (search_rec) {
      colorize_foundtext(&buffer, search_rec, a()->user()->data.lp_colors[10]);
    }
    file_information += fmt::sprintf(" |%02d%s", a()->user()->data.lp_colors[10], buffer);
    extdesc_pos = width;
  } else {
    extdesc_pos = -1;
  }

  auto fi = trim_to_size_ignore_colors(file_information, will_fit);
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
      lines_printed = print_extended(u->filename, num_extended, -extdesc_pos,
                                          static_cast<Color>(a()->user()->data.lp_colors[10]),
                                          search_rec);
    } else {
      lines_printed = 0;
    }

    if (lines_printed) {
      numl += lines_printed;
      char_printed = 0;
    }
  }
  if (bout.wherex()) {
    if (char_printed) {
      bout.nl();
      ++numl;
    } else {
      bout.bputch('\r');
    }
  }
  file_information.clear();

  if (a()->user()->data.lp_options & cfl_date_uploaded) {
    if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
      buffer = fmt::sprintf("UL: %s  New: %s", u->date, u->actualdate);
      StringJustify(&buffer, 27, ' ', JustificationType::LEFT);
    } else {
      buffer = fmt::sprintf("UL: %s", u->date);
      StringJustify(&buffer, 12, ' ', JustificationType::LEFT);
    }
    file_information += fmt::sprintf("|%02d%s  ", a()->user()->data.lp_colors[4], buffer);
  }

  if (a()->user()->data.lp_options & cfl_upby) {
    if (a()->user()->data.lp_options & cfl_date_uploaded) {
      StringJustify(&file_information, ssize(file_information) + width, ' ',
                    JustificationType::RIGHT);
      bout << file_information;
      bout.nl();
      ++numl;
    }
    auto tmp = properize(std::string(u->upby));
    file_information = fmt::sprintf("|%02dUpby: %-15s", a()->user()->data.lp_colors[7], tmp);
  }

  if (!buffer.empty()) {
    StringJustify(&file_information, ssize(file_information) + width, ' ',
                  JustificationType::RIGHT);
    bout << file_information;
    bout.nl();
    ++numl;
  }
  return numl;
}

int print_extended(const std::string& file_name, int numlist, int indent, Color color,
                        search_record* search_rec) {

  const int will_fit = 80 - std::abs(indent) - 2;

  auto ss = a()->current_file_area()->ReadExtendedDescriptionAsString(file_name).value_or("");

  if (ss.empty()) {
    return 0;
  }
  StringTrimEnd(&ss);
  if (search_rec && search_rec->filemask != "????????.???") {
    colorize_foundtext(&ss, search_rec, static_cast<uint8_t>(color));
  }
  if (indent > -1 && indent != 16) {
    bout << "  |#9Extended Description:\n\r";
  }
  const auto lines = SplitString(ss, "\n", false);
  auto numl = std::min<int>(size_int(lines), numlist);
  for (auto i = 0; i < numl; i++) {
    bout.Right(std::abs(indent));
    auto l = lines.at(i);
    if (ssize(l) > will_fit) {
      l.resize(will_fit);
    }
    bout.SystemColor(static_cast<uint8_t>(color));
    bout.bputs(lines.at(i));
    bout.nl();
  }
  bout.Color(0);
  if (bout.wherex()) {
    bout.nl();
    ++numl;
  }
  return numl;
}

void show_fileinfo(uploadsrec* u) {
  bout.cls();
  bout.Color(7);
  bout << std::string(78, '\xCD');
  bout.nl();
  bout << "  |#9Filename    : |#2" << u->filename << wwiv::endl;
  bout << "  |#9Uploaded on : |#2" << u->date << " by |#2" << u->upby << wwiv::endl;
  if (u->actualdate[2] == '/' && u->actualdate[5] == '/') {
    bout << "  |#9Newest file : |#2" << u->actualdate << wwiv::endl;
  }
  bout << "  |#9Size        : |#2" << humanize(u->numbytes) << wwiv::endl;
  bout << "  |#9Downloads   : |#2" << u->numdloads << "|#9" << wwiv::endl;
  bout << "  |#9Description : |#2" << u->description << wwiv::endl;
  print_extended(u->filename, 255, 16, Color::YELLOW, nullptr);
  bout.Color(7);
  bout << std::string(78, '\xCD');
  bout.nl();
  bout.pausescr();
}

int check_lines_needed(uploadsrec* u) {
  const auto num_extended = std::max<int>(a()->user()->GetNumExtended(),
                                          lp_config.show_at_least_extended);
  const auto max_lines = std::min<int>(num_extended, calc_max_lines());

  auto elines = 0;
  if (a()->user()->data.lp_options & cfl_description) {
    if (ext_is_on && mask_extended & u->mask) {
      const auto ss = a()->current_file_area()->ReadExtendedDescriptionAsString(u->filename).
                           value_or("");
      const auto lines = SplitString(ss, "\r");
      elines = size_int(lines);
    }
  }
  const auto lc_lines_used = lp_configured_lines();
  return std::min<int>(lc_lines_used + elines, max_lines - 1);
}

int prep_search_rec(search_record* r, int type) {
  r->search_extended = lp_config.search_extended_on ? true : false;

  if (type == LP_LIST_DIR) {
    r->filemask = file_mask();
    r->alldirs = THIS_DIR;
  } else if (type == LP_SEARCH_ALL) {
    r->alldirs = ALL_DIRS;
    if (!search_criteria(r)) {
      return 0;
    }
  } else if (type == LP_NSCAN_DIR) {
    r->alldirs = THIS_DIR;
    r->nscandate = a()->sess().nscandate();
  } else if (type == LP_NSCAN_NSCAN) {
    a()->sess().scanned_files(true);
    r->nscandate = a()->sess().nscandate();
    r->alldirs = ALL_DIRS;
  } else {
    sysoplog() << "Undef LP type";
    return 0;
  }

  if (r->filemask.empty() && !r->nscandate && !r->search[0]) {
    return 0;
  }

  if (!r->filemask.empty()) {
    if (!contains(r->filemask, '.')) {
      r->filemask += ".*";
    }
  }
  r->filemask = aligns(r->filemask);
  return 1;
}

int calc_max_lines() {
  if (lp_config.max_screen_lines_to_show) {
    return std::min<int>(a()->user()->screen_lines(), lp_config.max_screen_lines_to_show) -
           (first_file_pos() + 1 + STOP_LIST);
  }
  return a()->user()->screen_lines() - (first_file_pos() + 1 + STOP_LIST);
}

static void check_lp_colors() {
  bool needs_defaults = false;
  auto* u = a()->user();
  for (auto& c : u->data.lp_colors) {
    if (!c) {
      needs_defaults = true;
      c = static_cast<uint8_t>(Color::CYAN);
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
    if (fileConfig.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate |
                        File::modeReadWrite)) {
      fileConfig.Write(&lp_config, sizeof(listplus_config));
      fileConfig.Close();
    }
  }
}

void load_lp_config() {
  if (!lp_config_loaded) {
    File fileConfig(FilePath(a()->config()->datadir(), LISTPLUS_CFG));
    if (!fileConfig.Open(File::modeBinary | File::modeReadOnly)) {
      memset(&lp_config, 0, sizeof(listplus_config));

      lp_config.normal_highlight =
          static_cast<uint8_t>(Color::YELLOW) + (static_cast<uint8_t>(Color::BLACK) << 4);
      lp_config.normal_menu_item =
          static_cast<uint8_t>(Color::CYAN) + (static_cast<uint8_t>(Color::BLACK) << 4);
      lp_config.current_highlight =
          static_cast<uint8_t>(Color::BLUE) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4);
      lp_config.current_menu_item =
          static_cast<uint8_t>(Color::BLACK) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4);

      lp_config.tagged_color = static_cast<uint8_t>(Color::LIGHTGRAY);
      lp_config.file_num_color = static_cast<uint8_t>(Color::GREEN);

      lp_config.found_fore_color = static_cast<uint8_t>(Color::RED);
      lp_config.found_back_color = static_cast<uint8_t>(Color::LIGHTGRAY) + 16;

      lp_config.current_file_color =
          static_cast<uint8_t>(Color::BLACK) + (static_cast<uint8_t>(Color::LIGHTGRAY) << 4);

      lp_config.max_screen_lines_to_show = 24;
      lp_config.show_at_least_extended = 5;

      lp_config.edit_enable = 1;         // Do or don't let users edit their config
      lp_config.request_file = 1;        // Do or don't use file request
      lp_config.colorize_found_text = 1; // Do or don't colorize found text
      lp_config.search_extended_on = 0;
      // Defaults to either on or off in adv search, or is either on or off in simple search
      lp_config.simple_search = 1; // Which one is entered when searching, can switch to other still
      lp_config.no_configuration = 0; // Toggles configurable menus on and off
      lp_config.check_exist = 1;
      // Will check to see if the file exists on hardrive and put N/A if not
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
  bool done = false;

  if (!so()) {
    return;
  }

  load_lp_config();

  while (!done && !a()->sess().hangup()) {
    bout.cls();
    bout.printfile(LPSYSOP_NOEXT);
    bout.PutsXYSC(38, 2, lp_config.normal_highlight, fmt::sprintf("%3d", lp_config.normal_highlight));
    bout.PutsXYSC(77, 2, lp_config.normal_menu_item, fmt::sprintf("%3d", lp_config.normal_menu_item));
    bout.PutsXYSC(38, 3, lp_config.current_highlight, fmt::sprintf("%3d", lp_config.current_highlight));
    bout.PutsXYSC(77, 3, lp_config.current_menu_item, fmt::sprintf("%3d", lp_config.current_menu_item));
    bout.Color(0);
    bout.PutsXY(38, 6, fmt::sprintf("|%02d%2d", lp_config.tagged_color, lp_config.tagged_color));
    bout.PutsXY(77, 6, fmt::sprintf("|%02d%2d", lp_config.file_num_color, lp_config.file_num_color));
    bout.PutsXY(38, 7, fmt::sprintf("|%02d%2d", lp_config.found_fore_color, lp_config.found_fore_color));
    bout.PutsXY(77, 7, fmt::sprintf("|%02d%2d", lp_config.found_back_color, lp_config.found_back_color));
    bout.PutsXYSC(38, 8, lp_config.current_file_color, fmt::sprintf("%3d", lp_config.current_file_color));
    bout.PutsXY(38, 11, fmt::sprintf("|#4%2d", lp_config.max_screen_lines_to_show));
    bout.PutsXY(77, 11,fmt::sprintf("|#4%2d", lp_config.show_at_least_extended));
    bout.PutsXY(74, 14, fmt::sprintf("|#4%s", lp_config.request_file ? _on_ : _off_));
    bout.PutsXY(74, 15, fmt::sprintf("|#4%s", lp_config.search_extended_on ? _on_ : _off_));
    bout.PutsXY(74, 16, fmt::sprintf("|#4%s", lp_config.edit_enable ? _on_ : _off_));
    bout.Color(0);
    bout.PutsXY(29, 14, fmt::sprintf("|#4%s", lp_config.no_configuration ? _on_ : _off_));
    bout.PutsXY(29, 15, fmt::sprintf("|#4%s", lp_config.colorize_found_text ? _on_ : _off_));
    bout.PutsXY(29, 16, fmt::sprintf("|#4%s", lp_config.simple_search ? _on_ : _off_));
    bout.PutsXY(29, 17, fmt::sprintf("|#4%s", lp_config.check_exist ? _on_ : _off_));
    bout.PutsXY(1, 19, "|#9Q-Quit ");
    const auto key = onek("Q\rABCDEFGHIJKLMNOPRS", true);
    switch (key) {
    case 'Q':
    case '\r':
      done = true;
      break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'I': {
      auto color = SelectColor(2);
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
      } break;
    case 'E':
    case 'F':
    case 'G':
    case 'H': {
      const auto color = SelectColor(1);
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
      } break;
    case 'J':
      bout << "Enter max amount of lines to show (0=disabled) ";
      lp_config.max_screen_lines_to_show = bin.input_number(lp_config.max_screen_lines_to_show, 0, 20, true);
      break;
    case 'K':
      bout << "Enter minimum extended description lines to show ";
      lp_config.show_at_least_extended = bin.input_number(lp_config.show_at_least_extended, 0, 20, true);
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
  unsigned char nc;

  bout.nl();

  if (a()->user()->color()) {
    color_list();
    bout.Color(0);
    bout.nl();
    bout << "|#2Foreground? ";
    unsigned char ch = onek("01234567");
    nc = ch - '0';

    if (which == 2) {
      bout << "|#2Background? ";
      ch = onek("01234567");
      nc |= (ch - '0') << 4;
    }
  } else {
    bout.nl();
    bout << "|#5Inversed? ";
    if (bin.yesno()) {
      if ((a()->user()->bwcolor(1) & 0x70) == 0) {
        nc = (a()->user()->bwcolor(1) & 0x07) << 4;
      } else {
        nc = a()->user()->bwcolor(1) & 0x70;
      }
    } else {
      if ((a()->user()->bwcolor(1) & 0x70) == 0) {
        nc = a()->user()->bwcolor(1) & 0x07;
      } else {
        nc = (a()->user()->bwcolor(1) & 0x70) >> 4;
      }
    }
  }

  bout << "|#5Intensified? ";
  if (bin.yesno()) {
    nc |= 0x08;
  }

  if (which == 2) {
    bout << "|#5Blinking? ";
    if (bin.yesno()) {
      nc |= 0x80;
    }
  }
  bout.nl();
  bout.SystemColor(nc);
  bout << DescribeColorCode(nc);
  bout.Color(0);
  bout.nl();

  bout << "|#5Is this OK? ";
  if (bin.yesno()) {
    return nc;
  }
  return -1;
}

static void update_user_config_screen(uploadsrec* u, int which) {
  const std::vector<std::string> lp_color_list{
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

  const auto color_background = static_cast<uint8_t>(Color::BLUE) << 4;
  auto color_selected = static_cast<uint8_t>(Color::LIGHTRED) | color_background;
  auto color_notselected = static_cast<uint8_t>(Color::BLACK) | color_background;
  const auto color_colortext = static_cast<uint8_t>(Color::LIGHTCYAN) | color_background;
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
  int which = -1;
  unsigned long bit = 0L;
  uploadsrec u = {};

  to_char_array(u.filename, "WWIV55.ZIP");
  to_char_array(u.description, "This is a sample description!");
  to_char_array(u.date, date());
  const std::string username_num = a()->user()->name_and_number();
  to_char_array(u.upby, username_num);
  u.numdloads = 50;
  u.numbytes = 655535L;
  u.daten = daten_t_now() - 10000;

  load_lp_config();

  bout.cls();
  bout.printfile(LPCONFIG_NOEXT);
  if (!(a()->user()->data.lp_options & cfl_fname)) {
    a()->user()->data.lp_options |= cfl_fname;
  }

  if (!(a()->user()->data.lp_options & cfl_description)) {
    a()->user()->data.lp_options |= cfl_description;
  }

  auto done = false;
  while (!done && !a()->sess().hangup()) {
    update_user_config_screen(&u, which);
    switch (const int key = onek("Q2346789H!@#$%^&*("); key) {
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

static int rename_filename(const std::string& file_name, int dn) {
  int ret = 1;

  dliscan1(dn);
  std::string s = file_name;
  if (s.empty()) {
    return 1;
  }
  if (!contains(s, '.')) {
    s += ".*";
  }
  const auto orig_aligned_filename = aligns(s);

  int i = recno(orig_aligned_filename);
  while (i > 0) {
    const int current_file_position = i;
    auto* area = a()->current_file_area();
    auto& dir = a()->dirs()[dn];
    auto f = area->ReadFile(current_file_position);
    bout.nl();
    printfileinfo(&f.u(), dir);
    bout.nl();
    bout << "|#5Change info for this file (Y/N/Q)? ";
    char ch = bin.ynq();
    if (ch == 'Q') {
      ret = 0;
      break;
    }
    if (ch == 'N') {
      i = nrecno(orig_aligned_filename, current_file_position);
      continue;
    }
    bout.nl();
    bout << "|#2New filename? ";
    auto new_filename = bin.input(12);
    if (!okfn(new_filename)) {
      new_filename.clear();
    }
    if (!new_filename.empty()) {
      new_filename = aligns(new_filename);
      if (new_filename != "        .   ") {
        files::FileName nfn(new_filename);
        auto new_fn = FilePath(a()->dirs()[dn].path, nfn);
        if (File::Exists(new_fn)) {
          bout << "Filename already in use; not changed.\r\n";
        } else {
          auto old_fn = FilePath(a()->dirs()[dn].path, f);
          File::Rename(old_fn, new_fn);
          if (File::Exists(new_fn)) {
            auto ss = area->ReadExtendedDescriptionAsString(f).value_or(std::string());
            if (!ss.empty()) {
              // TODO(rushfan): Display error if these fail?
              area->DeleteExtendedDescription(f, current_file_position);
              area->AddExtendedDescription(new_filename, ss);
            }
            f.set_filename(new_filename);
            f.set_extended_description(!ss.empty());
          } else {
            bout << "Bad filename.\r\n";
          }
        }
      }
    }
    bout.nl();
    bout << "New description:\r\n|#2: ";
    auto desc = bin.input_text(f.u().description, 58);
    if (!desc.empty()) {
      f.set_description(desc);
    }
    auto ss = area->ReadExtendedDescriptionAsString(f).value_or(std::string());
    bout.nl(2);
    bout << "|#5Modify extended description? ";
    if (bin.yesno()) {
      bout.nl();
      if (!ss.empty()) {
        bout << "|#5Delete it? ";
        if (bin.yesno()) {
          area->DeleteExtendedDescription(f, current_file_position);
          f.set_extended_description(false);
        } else {
          f.set_extended_description(true);
          modify_extended_description(&ss, a()->dirs()[dn].name);
          if (!ss.empty()) {
            area->DeleteExtendedDescription(f, current_file_position);
            area->AddExtendedDescription(f, current_file_position, ss);
          }
        }
      } else {
        modify_extended_description(&ss, a()->dirs()[dn].name);
        if (!ss.empty()) {
          area->AddExtendedDescription(f, current_file_position, ss);
          f.set_extended_description(true);
        } else {
          f.set_extended_description(false);
        }
      }
    } else if (!ss.empty()) {
      f.set_extended_description(true);
    } else {
      f.set_extended_description(false);
    }
    if (a()->current_file_area()->UpdateFile(f, i)) {
      a()->current_file_area()->Save();
    }
    i = nrecno(orig_aligned_filename, current_file_position);
  }
  return ret;
}

static int remove_filename(const std::string& file_name, int dn) {
  if (file_name.empty()) {
    return 1;
  }

  const auto& dir = a()->dirs()[dn];
  dliscan1(dir);

  auto fn{file_name};
  if (fn.find('.') == std::string::npos) {
    fn += ".*";
  }
  fn = aligns(fn);
  int i = recno(fn);
  bool abort = false;
  bool rdlp = false;
  int ret = 1;
  while (!a()->sess().hangup() && i > 0 && !abort) {
    auto f = a()->current_file_area()->ReadFile(i);
    if (dcs() || f.u().ownersys == 0 && f.u().ownerusr == a()->sess().user_num()) {
      bout.nl();
      printfileinfo(&f.u(), dir);
      bout << "|#9Remove (|#2Y/N/Q|#9) |#0: |#2";
      char ch = bin.ynq();
      if (ch == 'Q') {
        ret = 0;
        abort = true;
      } else if (ch == 'Y') {
        rdlp = true;
        bool rm = false;
        if (dcs()) {
          bout << "|#5Delete file too? ";
          rm = bin.yesno();
          if (rm && f.u().ownersys == 0) {
            bout << "|#5Remove DL points? ";
            rdlp = bin.yesno();
          }
          bout << "|#5Remove from ALLOW.DAT? ";
          if (bin.yesno()) {
            remove_from_file_database(fn);
          }
        } else {
          rm = true;
          remove_from_file_database(fn);
        }
        if (rm) {
          File::Remove(FilePath(dir.path, f));
          if (rdlp && f.u().ownersys == 0) {
            if (auto user = a()->users()->readuser(f.u().ownerusr, UserManager::mask::non_deleted)) {
              if (date_to_daten(user->firston()) < f.u().daten) {
                user->decrement_uploaded();
                user->decrement_uploaded();
                user->set_uk(user->uk() - bytes_to_k(f.numbytes()));
                a()->users()->writeuser(&user.value(), f.u().ownerusr);
              }
            }
          }
        }
        sysoplog() << "- '" << f << "' removed off of " << a()->dirs()[dn].name;
        if (a()->current_file_area()->DeleteFile(f, i)) {
          a()->current_file_area()->Save();
          --i;
        }
      }
    }
    i = nrecno(fn, i);
  }
  return ret;
}

static int move_filename(const std::string& file_name, int dn) {
  int nDestDirNum = -1, ret = 1;
  const auto move_fn = aligns(file_name);
  dliscan1(dn);
  int nRecNum = recno(move_fn);
  if (nRecNum < 0) {
    bout << "\r\nFile not found.\r\n";
    return ret;
  }
  bool done = false;
  bool ok = false;

  tmp_disable_conf(true);
  wwiv::common::TempDisablePause diable_pause(bout);
  while (!a()->sess().hangup() && nRecNum > 0 && !done) {
    int cp = nRecNum;
    const auto& dir = a()->dirs()[dn];
    auto source_filearea = a()->fileapi()->Open(dir);
    auto f = source_filearea->ReadFile(nRecNum);
    auto src_fn = FilePath(dir.path, f);
    bout.nl();
    printfileinfo(&f.u(), dir);
    bout.nl();
    bout << "|#5Move this (Y/N/Q)? ";
    char ch = 'Y';
    if (bulk_move) {
      bout.Color(1);
      bout << YesNoString(true);
      bout.nl();
    } else {
      ch = bin.ynq();
    }

    if (ch == 'Q') {
      done = true;
      ret = 0;
    } else if (ch == 'Y') {
      if (!bulk_move) {
        std::string ss;
        do {
          bout.nl(2);
          bout << "|#2To which directory? ";
          ss = mmkey(MMKeyAreaType::dirs);
          if (ss[0] == '?') {
            dirlist(1);
            dliscan1(dn);
          }
        }
        while (!a()->sess().hangup() && ss[0] == '?');

        nDestDirNum = -1;
        if (ss[0]) {
          for (auto i1 = 0; i1 < a()->dirs().size(); i1++) {
            if (ss == a()->udir[i1].keys) {
              nDestDirNum = i1;
            }
          }
        }

        if (a()->batch().size() > 1) {
          bout << "|#5Move all tagged files? ";
          if (bin.yesno()) {
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
        if (recno(f.aligned_filename()) > 0) {
          ok = false;
          bout.nl();
          bout << "Filename already in use in that directory.\r\n";
        }
        if (a()->current_file_area()->number_of_files() >= a()->dirs()[nDestDirNum].maxfiles) {
          ok = false;
          bout << "\r\nToo many files in that directory.\r\n";
        }
        if (File::freespace_for_path(a()->dirs()[nDestDirNum].path) <
            static_cast<long>(f.numbytes() / 1024L) + 3) {
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
        if (bin.yesno()) {
          f.set_date(DateTime::now());
        }
      } else {
          f.set_date(DateTime::now());
      }
      --cp;
      auto ss = source_filearea->ReadExtendedDescriptionAsString(f).value_or("");
      if (source_filearea->DeleteFile(f, nRecNum)) {
        source_filearea->Save();
      }
      auto dest_fn = FilePath(a()->dirs()[nDestDirNum].path, f);
      dliscan1(nDestDirNum);
      auto* area = a()->current_file_area();
      if (area->AddFile(f)) {
        area->Save();
      }
      auto current_file_position = area->FindFile(f);
      if (!ss.empty()) {
        f.set_extended_description(true);
        if (current_file_position.has_value()) {
          area->AddExtendedDescription(f, current_file_position.value(), ss);
        } else {
          area->AddExtendedDescription(f.filename(), ss);
        }
      }
      if (src_fn != dest_fn && File::Exists(src_fn)) {
        File::Rename(src_fn, dest_fn);
      }
      bout << "\r\nFile moved.\r\n";
    }
    dliscan();
    nRecNum = nrecno(move_fn, cp);
  }
  tmp_disable_conf(false);
  return ret;
}

void do_batch_sysop_command(int mode, const std::string& file_name) {
  const auto save_curdir = a()->current_user_dir_num();
  bout.cls();

  if (a()->batch().numbatchdl() > 0) {
    auto done = false;
    for (auto it = begin(a()->batch().entry); it != end(a()->batch().entry) && !done;) {
      const auto& b = *it;
      if (b.sending()) {
        switch (mode) {
        case SYSOP_DELETE:
          if (!remove_filename(b.aligned_filename(), b.dir())) {
            done = true;
          } else {
            it = a()->batch().delbatch(it);
            continue;
          }
          break;
        case SYSOP_RENAME:
          if (!rename_filename(b.aligned_filename(), b.dir())) {
            done = true;
          } else {
            it = a()->batch().delbatch(it);
            continue;
          }
          break;
        case SYSOP_MOVE:
          if (!move_filename(b.aligned_filename(), b.dir())) {
            done = true;
          } else {
            it = a()->batch().delbatch(it);
            continue;
          }
          break;
        }
      }
      ++it;
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

int search_criteria(search_record* sr) {
  auto all_conf = true;

  const auto useconf = ok_multiple_conf(a()->user(), a()->uconfdir);

LP_SEARCH_HELP:
  sr->search_extended = lp_config.search_extended_on ? true : false;

  bout.cls();
  bout.printfile(LPSEARCH_NOEXT);

  auto done = false;
  char x{0};
  while (!done) {
    bout.GotoXY(1, 15);
    for (auto i = 0; i < 9; i++) {
      bout.GotoXY(1, 15 + i);
      bout.clreol();
    }
    bout.GotoXY(1, 15);

    bout << "|#9A)|#2 Filename (wildcards) :|#2 " << sr->filemask << wwiv::endl;
    bout << "|#9B)|#2 Text (no wildcards)  :|#2 " << sr->search << wwiv::endl;
    const auto dir_name = stripcolors(a()->dirs()[a()->current_user_dir().subnum].name);
    bout << "|#9C)|#2 Which Directories    :|#2 "
         << (sr->alldirs == THIS_DIR ? dir_name : sr->alldirs == ALL_DIRS ? "All dirs" : "Dirs in NSCAN")
         << wwiv::endl;
    const auto conf_name =
        stripcolors(a()->uconfdir[a()->sess().current_user_dir_conf_num()].conf_name);
    bout << "|#9D)|#2 Which Conferences    :|#2 " << (all_conf ? "All Conferences" : conf_name) <<
        wwiv::endl;
    bout << "|#9E)|#2 Extended Description :|#2 " << (sr->search_extended ? "Yes" : "No ") <<
        wwiv::endl;
    bout.nl();
    bout << "|15Select item to change |#2<CR>|15 to start search |#2Q=Quit|15:|#0 ";

    x = onek("QABCDE\r?");

    switch (x) {
    case 'A':
      bout << "Filename (wildcards okay) : ";
      sr->filemask = bin.input(12, true);
      if (sr->filemask[0]) {
        if (okfn(sr->filemask)) {
          if (sr->filemask.size() < 8) {
            sysoplog() << "Filespec: " << sr->filemask;
          } else {
            if (contains(sr->filemask, '.')) {
              sysoplog() << "Filespec: " << sr->filemask;
            } else {
              bout << "|#6Invalid filename: " << sr->filemask << wwiv::endl;
              bout.pausescr();
              sr->filemask[0] = '\0';
            }
          }
        } else {
          bout << "|#6Invalid filespec: " << sr->filemask << wwiv::endl;
          bout.pausescr();
          sr->filemask[0] = 0;
        }
      }
      break;

    case 'B':
      bout << "Keyword(s) : ";
      sr->search = bin.input_upper(60);
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

  if (useconf && all_conf) {
    // toggle conferences off
    tmp_disable_conf(true);
  }

  return x;
}

void view_file(const std::string& aligned_file_name) {
  bout.cls();
  dliscan();
  auto abort = false;
  auto i = recno(aligned_file_name);
  do {
    if (i > 0) {
      auto f = a()->current_file_area()->ReadFile(i);
      const auto i1 = list_arc_out(stripfn(f.unaligned_filename()),
                                   a()->dirs()[a()->current_user_dir().subnum].path);
      if (i1) {
        abort = true;
      }
      bin.checka(&abort);
      i = nrecno(aligned_file_name, i);
    }
  }
  while (i > 0 && !a()->sess().hangup() && !abort);
  bout.nl();
  bout.pausescr();
}

int lp_try_to_download(const std::string& file_mask, int dn) {
  int rtn;
  auto abort = false;

  dliscan1(dn);
  auto i = recno(file_mask);
  if (i <= 0) {
    bin.checka(&abort);
    return abort ? -1 : 0;
  }
  bool ok = true;

  foundany = 1;
  do {
    a()->tleft(true);
    auto f = a()->current_file_area()->ReadFile(i);

    if (!(f.u().mask & mask_no_ratio)) {
      if (!ratio_ok()) {
        return -2;
      }
    }

    write_inst(INST_LOC_DOWNLOAD, a()->current_user_dir().subnum, INST_FLAGS_ONLINE);
    auto d = fmt::sprintf("%-40.40s", f.u().description);
    abort = false;
    rtn = add_batch(d, f.aligned_filename(), dn, f.numbytes());

    if (abort || rtn == -3) {
      ok = false;
    } else {
      i = nrecno(file_mask, i);
    }
  }
  while (i > 0 && ok && !a()->sess().hangup());
  if (rtn == -2) {
    return -2;
  }
  return abort || rtn == -3 ? -1 : 1;
}

void download_plus(const std::string& file_name) {
  if (a()->batch().numbatchdl() != 0) {
    bout.nl();
    bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
    if (bin.noyes()) {
      batchdl(1);
      return;
    }
  }
  if (file_name.empty()) {
    return;
  }
  auto fn{file_name};

  if (strchr(fn.c_str(), '.') == nullptr) {
    fn += ".*";
  }
  fn = aligns(fn);
  if (lp_try_to_download(fn, a()->current_user_dir().subnum) == 0) {
    bout << "\r\nSearching all directories.\r\n\n";
    auto dn = 0;
    auto count = 0;
    auto color = 3;
    foundany = 0;
    bout << "\r|#2Searching ";
    while (dn < a()->dirs().size()) {
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
      if (lp_try_to_download(fn, a()->udir[dn].subnum) < 0) {
        break;
      }
      dn++;
    }
    if (!foundany) {
      bout << "File not found.\r\n\n";
    }
  }
}

void request_file(const std::string& file_name) {
  bout.cls();
  bout.nl();

  bout.printfile(LPFREQ_NOEXT);
  bout << "|#2File missing.  Request it? ";

  if (bin.noyes()) {
    ssm(1) << a()->user()->name() << " is requesting file " << file_name;
    bout << "File request sent\r\n";
  } else {
    bout << "File request NOT sent\r\n";
  }
  bout.pausescr();
}
