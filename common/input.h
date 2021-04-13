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
#ifndef INCLUDED_COMMON_INPUT_H
#define INCLUDED_COMMON_INPUT_H

#include "common/iobase.h"
#include "common/context.h"
#include "common/remote_io.h"
#include <chrono>
#include <functional>
#include <set>
#include <string>

namespace wwiv::common {
  class Input;
}

extern wwiv::common::Input bin;

namespace wwiv::common {

// Text editing modes for input routines
enum class InputMode { UPPER, MIXED, PROPER, FILENAME, FULL_PATH_NAME, CMDLINE, DATE, PHONE };

template <typename T> struct input_result_t {
  T num{0};
  char key{0};
};

/**
 * Creates the Output class responsible for displaying information both
 * locally and remotely.
 *
 * To use this class you must set the needed fields from IOBase.
 */
class Input final : public IOBase {
public:
  Input();
  ~Input();

  [[nodiscard]] bool IsLastKeyLocal() const noexcept { return last_key_local_; }
  void SetLastKeyLocal(bool b) { last_key_local_ = b; }

  char bgetch(bool allow_extended_input = false);
  char bgetchraw();
  bool bkbhitraw();
  bool bkbhit();
  char getkey(bool allow_extended_input = false);

  // Key Timeouts
  [[nodiscard]] std::chrono::duration<double> key_timeout() const;
  void set_key_timeout(std::chrono::duration<double> k) { non_sysop_key_timeout_ = k; }
  void set_sysop_key_timeout(std::chrono::duration<double> k) { sysop_key_timeout_ = k; }
  void set_default_key_timeout(std::chrono::duration<double> k) { default_key_timeout_ = k; }
  void set_logon_key_timeout(std::chrono::duration<double> k) { logon_key_timeout_ = k; }

  //
  // Key Timeout manipulators.
  //

  void set_logon_key_timeout() { non_sysop_key_timeout_ = logon_key_timeout_; };
  void reset_key_timeout() { non_sysop_key_timeout_ = default_key_timeout_; }
  
  //
  // bgetch_event - bgetch with cursor keys and such.
  //

  // For bgetch_event, decides if the number pad turns '8' into an arrow etc.. or not
  enum class numlock_status_t { NUMBERS, NOTNUMBERS };

  enum class bgetch_timeout_status_t { WARNING, CLEAR, IDLE };

  typedef std::function<void(bgetch_timeout_status_t, int)> bgetch_callback_fn;
  int bgetch_event(numlock_status_t numlock_mode, std::chrono::duration<double> idle_time,
                   bgetch_callback_fn cb);
  int bgetch_event(numlock_status_t numlock_mode, bgetch_callback_fn cb);
  int bgetch_event(numlock_status_t numlock_mode);

  //
  // Yes/No/Quit prompts
  //

  bool yesno();
  bool noyes();
  char ynq();

  //
  // Need Screen Pause. Used by Yes/No/Continue prompts.
  //

  void clearnsp();
  void resetnsp();
  int nsp() const noexcept;
  void nsp(int n);

  [[nodiscard]] bool okskey() const noexcept { return okskey_; }
  void okskey(bool n) { okskey_ = n; }

  /**
   * Inputs password up to length max_length.
   */
  std::string input_password(const std::string& prompt_text, int max_length);

  /**
   * Inputs number or a key contained in hotkeys.
   */
  input_result_t<int64_t> input_number_or_key_raw(int64_t cur, int64_t minv, int64_t maxv,
                                                  bool setdefault, const std::set<char>& hotkeys);

  /**
   * Inputs filename up to length max_length.
   */
  std::string input_filename(int max_length);

  /**
   * Inputs filename (without directory separators) up to length max_length.
   */
  std::string input_filename(const std::string& orig_text, int max_length);

  /**
   * Inputs path (including directory separators) up to length max_length.
   */
  std::string input_path(const std::string& orig_text, int max_length);

  /**
   * Inputs path (including directory separators) up to length max_length.
   */
  std::string input_path(int max_length);

  /**
   * Inputs command-line up to length max_length.
   */
  std::string input_cmdline(const std::string& orig_text, int max_length);

  /**
   * Inputs phone number up to length max_length.
   */
  std::string input_phonenumber(const std::string& orig_text, int max_length);

  /**
   * Inputs random text (upper and lower case) up to length max_length.
   */
  std::string input_text(const std::string& orig_text, int max_length);

  /**
   * Inputs random text (upper and lower case) up to length max_length
   * with the ability to turn on or off the MPL.
   */
  std::string input_text(const std::string& orig_text, bool mpl, int max_length);

  /**
   * Inputs random text (upper and lower case) up to length max_length.
   */
  std::string input_text(int max_length);

  /**
   * Inputs random text (upper case) up to length max_length.
   */
  std::string input_upper(const std::string& orig_text, int max_length, bool auto_mpl);

  /**
   * Inputs random text (upper case) up to length max_length.
   */
  std::string input_upper(const std::string& orig_text, int max_length);

  /**
   * Inputs random text (upper case) up to length max_length.
   */
  std::string input_upper(int max_length);

  /**
   * Inputs random text (In Proper Case) up to length max_length.
   */
  std::string input_proper(const std::string& orig_text, int max_length);

  /**
   * Inputs a date (10-digit) of MM/DD/YYYY.
   */
  std::string input_date_mmddyyyy(const std::string& orig_text);

  /**
   * Inputs a string in upper case.  These should probably all be replaced
   * by calling the appropriate input_xxx routine.
   */
  void input(char* out_text, int max_length, bool auto_mpl = false);
  std::string input(int max_length, bool auto_mpl = false);

  // TODO(rushfan): Using an int64_t for the min/max both here and in input_number_or_key_raw
  //               is a bit wonky, but works for probably all case in WWIV since it won't be
  //               using int64_t user input most likely ever.  Otherwise need to wait till compilers
  //               are happy enough with auto everywhere to pass through caller types.
  /**
   * Inputs a number of type T within the range of min_value and max_value.
   * If set_default_value is true (the default) then the input box will have
   * the current_value populated.
   */
  template <typename T>
  T input_number(T current_value, int64_t min_value = std::numeric_limits<T>::min(),
                 int64_t max_value = std::numeric_limits<T>::max(), bool set_default_value = true) {
    if (current_value < min_value) {
      current_value = static_cast<T>(min_value);
    } else if (current_value > max_value) {
      current_value = static_cast<T>(max_value);
    }
    auto r =
        bin.input_number_or_key_raw(current_value, min_value, max_value, set_default_value, {'Q'});
    return (r.key != 0) ? current_value : static_cast<T>(r.num);
  }

  template <typename T>
  wwiv::common::input_result_t<T> input_number_hotkey(T current_value, const std::set<char>& keys,
                                                      int min_value = std::numeric_limits<T>::min(),
                                                      int max_value = std::numeric_limits<T>::max(),
                                                      bool set_default_value = false) {
    auto orig =
        bin.input_number_or_key_raw(current_value, min_value, max_value, set_default_value, keys);
    wwiv::common::input_result_t<T> r{static_cast<T>(orig.num), orig.key};
    return r;
  }

  /**
   * Returns the screen size as a coordnate (x, y) denoting the position
   * of the lower right corner.
   */
  std::optional<ScreenPos> screen_size();


  //
  // Check for Abort
  //

  bool checka();
  bool checka(bool* abort);
  bool checka(bool* abort, bool* next);

  // Used for macros.  TODO: Make this private
  int charbufferpointer_{0};
  char charbuffer[255]{};

private:
  // Used by input_xxx
  void Input1(char* out_text, const std::string& orig_text, int max_length, bool insert,
              InputMode mode, bool auto_mpl);
  // Used by input_xxx
  std::string Input1(const std::string& orig_text, int max_length, bool bInsert, InputMode mode, bool auto_mpl);
  // used by input_xxx
  void input1(char* out_text, int max_length, InputMode lc, bool crend, bool auto_mpl);
  // used by input_password
  std::string input_password_minimal(int max_length);
  // used by bgetch_event
  int bgetch_handle_key_translation(int key, numlock_status_t numlock_mode);
  // used by bgetch_event
  int bgetch_handle_escape(int key);


private:
  local::io::LocalIO* local_io_{nullptr};
  RemoteIO* comm_{nullptr};
  bool last_key_local_{true};
  int nsp_{0};
  bool okskey_{true};

  std::chrono::duration<double> non_sysop_key_timeout_ = std::chrono::minutes(3);
  std::chrono::duration<double> default_key_timeout_ = std::chrono::minutes(3);
  std::chrono::duration<double> sysop_key_timeout_ = std::chrono::minutes(10);
  std::chrono::duration<double> logon_key_timeout_ = std::chrono::minutes(3);
};

} // namespace wwiv::common


#endif
