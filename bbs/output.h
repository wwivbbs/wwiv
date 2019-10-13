/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#ifndef __INCLUDED_BBS_OUTPUT_H__
#define __INCLUDED_BBS_OUTPUT_H__

#include "fmt/printf.h"
#include "local_io/curatr_provider.h"
#include "local_io/local_io.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/localio_screen.h"
#include "sdk/wwivcolors.h"
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

typedef std::basic_ostream<char>&(ENDL_TYPE_O)(std::basic_ostream<char>&);

class RemoteIO;

class SavedLine {
public:
  SavedLine(std::vector<std::pair<char, uint8_t>> l, int c) : line(std::move(l)), color(c) {}
  std::vector<std::pair<char, uint8_t>> line;
  int color;
};

class Output final : public wwiv::local_io::curatr_provider {
protected:
  LocalIO* local_io_{nullptr};
  RemoteIO* comm_{nullptr};

public:
  Output();
  ~Output();

  void SetLocalIO(LocalIO* local_io);
  [[nodiscard]] LocalIO* localIO() const noexcept { return local_io_; }

  void SetComm(RemoteIO* comm) { comm_ = comm; }
  [[nodiscard]] RemoteIO* remoteIO() const noexcept { return comm_; }

  void Color(int wwiv_color);
  void ResetColors();
  void GotoXY(int x, int y);
  void Left(int num);
  void Right(int num);
  void SavePosition();
  void RestorePosition();
  void nl(int nNumLines = 1);
  void bs();
  /* This sets the current color (both locally and remotely) to that
   * specified (in IBM format).
   */
  void SystemColor(int nColor);
  void SystemColor(wwiv::sdk::Color color);
  std::string MakeColor(int wwiv_color);
  std::string MakeSystemColor(int nColor);
  std::string MakeSystemColor(wwiv::sdk::Color color);
  
  void litebar(const std::string& msg);
  template <typename... Args> void litebarf(const std::string& format_str, const Args&... args) {
    const auto s2 = fmt::sprintf(format_str, args...);
    litebar(s2);
  }

  /** Backspaces from the current cursor position to the beginning of a line */
  void backline();

  /**
   * Clears from the cursor to the end of the line using ANSI sequences.  If the user
   * does not have ansi, this this function does nothing.
   */
  void clreol();

  /**
   * Moves the cursor to the beginning of the line and clears the whole like.
   * If the user does not have ansi, this this function does nothing.
   */
  void clear_whole_line();

  /**
   * Clears the local and remote screen using ANSI (if enabled), otherwise DEC 12
   */
  void cls();

  /**
   * This will make a reverse-video prompt line i characters long, repositioning
   * the cursor at the beginning of the input prompt area.  Of course, if the
   * user does not want ansi, this routine does nothing.
   */
  void mpl(int numOfChars);

  /**
   * This function outputs a string of characters to the screen (and remotely
   * if applicable).  The com port is also checked first to see if a remote
   * user has hung up
   */
  int bputs(const std::string& text);

  // Prints an abortable string (contained in *text). Returns 1 in *abort if the
  // string was aborted, else *abort should be zero.
  int bpla(const std::string& text, bool* abort);

  /**
   * Displays s which checking for abort and next
   * @see checka
   * <em>Note: bout.bputs means Output String And Next</em>
   *
   * @param text The text to display
   * @param abort The abort flag (Output Parameter)
   * @param next The next flag (Output Parameter)
   */
  int bputs(const std::string& text, bool* abort, bool* next);

  template <typename T> Output& operator<<(T const& value) {
    std::ostringstream ss;
    ss << value;
    bputs(ss.str());
    return *this;
  }

  Output& operator<<(ENDL_TYPE_O* value) {
    std::ostringstream ss;
    ss << value;
    bputs(ss.str());
    return *this;
  }

  int bputch(char c, bool use_buffer = false);
  void flush();
  void rputch(char ch, bool use_buffer = false);
  void rputs(const char* text);
  char getkey(bool allow_extended_input = false);
  bool RestoreCurrentLine(const SavedLine& line);
  SavedLine SaveCurrentLine();
  void dump();
  void clear_lines_listed() { lines_listed_ = 0; }
  [[nodiscard]] int lines_listed() const noexcept { return lines_listed_; }
  int wherex();
  [[nodiscard]] bool IsLastKeyLocal() const noexcept { return last_key_local_; }
  void SetLastKeyLocal(bool b) { last_key_local_ = b; }
  void RedrawCurrentLine();

  // Key Timeouts
  [[nodiscard]] std::chrono::duration<double> key_timeout() const;
  void set_key_timeout(std::chrono::duration<double> k) { non_sysop_key_timeout_ = k; }
  void set_sysop_key_timeout(std::chrono::duration<double> k) { sysop_key_timeout_ = k; }
  void set_default_key_timeout(std::chrono::duration<double> k) { default_key_timeout_ = k; }
  void set_logon_key_timeout(std::chrono::duration<double> k) { logon_key_timeout_ = k; }

  // Key Timeout manipulators.
  void set_logon_key_timeout() { non_sysop_key_timeout_ = logon_key_timeout_; };
  void reset_key_timeout() { non_sysop_key_timeout_ = default_key_timeout_; }
  void move_up_if_newline(int num_lines);

  // ANSI movement happened.
  [[nodiscard]] bool ansi_movement_occurred() const noexcept { return ansi_movement_occurred_; }
  void clear_ansi_movement_occurred() { ansi_movement_occurred_ = false; }

  // curatr_provider
  [[nodiscard]] int curatr() const noexcept override { return curatr_; }
  void curatr(int n) override { curatr_ = n; }

  [[nodiscard]] bool okskey() const noexcept { return okskey_; }
  void okskey(bool n) { okskey_ = n; }

  // reset the state of Output
  void reset();

public:
  int lines_listed_{0};
  bool newline{true};
  int charbufferpointer_{0};
  char charbuffer[255];

private:
  std::string bputch_buffer_;
  std::vector<std::pair<char, uint8_t>> current_line_;
  int x_{0};
  bool last_key_local_{true};
  // Means we need to reset the color before displaying our
  // next newline character.
  bool needs_color_reset_at_newline_{false};
  std::chrono::duration<double> non_sysop_key_timeout_ = std::chrono::minutes(3);
  std::chrono::duration<double> default_key_timeout_ = std::chrono::minutes(3);
  std::chrono::duration<double> sysop_key_timeout_ = std::chrono::minutes(10);
  std::chrono::duration<double> logon_key_timeout_ = std::chrono::minutes(3);

  bool ansi_movement_occurred_{false};
  int curatr_{7};
  bool okskey_{true};
  std::unique_ptr<wwiv::sdk::ansi::LocalIOScreen> screen_;
  std::unique_ptr<wwiv::sdk::ansi::Ansi> ansi_;
};

/**
 * This is wwiv::endl, notice it does not flush the buffer afterwards.
 */
namespace wwiv {
template <class charT, class traits>
std::basic_ostream<charT, traits>& endl(std::basic_ostream<charT, traits>& strm) {
  strm.write("\r\n", 2);
  return strm;
}
} // namespace wwiv

#endif // __INCLUDED_BBS_OUTPUT_H__
