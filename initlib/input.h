/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#ifndef __INCLUDED_INPUT_H__
#define __INCLUDED_INPUT_H__

#include <cstdlib>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <string.h>

#include "core/strings.h"
#include "core/wwivport.h"
#include "initlib/curses_io.h"
#include "initlib/curses_win.h"
#include "init/utility.h"

#define NUM_ONLY            1
#define UPPER_ONLY          2
#define ALL                 4
#define SET                 8

#ifndef EDITLINE_FILENAME_CASE
#ifdef __unix__
#define EDITLINE_FILENAME_CASE ALL
#else
#define EDITLINE_FILENAME_CASE UPPER_ONLY
#endif  // __unix__
#endif  // EDITLINE_FILENAME_CASE

// Function prototypes

bool dialog_yn(CursesWindow* window, const std::vector<std::string>& text);
bool dialog_yn(CursesWindow* window, const std::string& prompt);
int dialog_input_number(CursesWindow* window, const std::string& prompt, int min_value, int max_value);
char onek(CursesWindow* window, const char *s);
void editline(CursesWindow* window, std::string* s, int len, int status, int *returncode, const char *ss);
void editline(CursesWindow* window, char *s, int len, int status, int *returncode, const char *ss);
int toggleitem(CursesWindow* window, int value, const std::vector<std::string>& strings, int *returncode);

void input_password(CursesWindow* window, const std::string& prompt, const std::vector<std::string>& text, std::string *output, int max_length);
int messagebox(CursesWindow* window, const std::string& text);
int messagebox(CursesWindow* window, const std::vector<std::string>& text);

void trimstrpath(char *s);


// Base item of an editable value, this class does not use templates.
class BaseEditItem {
public:
  BaseEditItem(int x, int y, int maxsize)
    : x_(x), y_(y), maxsize_(maxsize) {};
  virtual ~BaseEditItem() {}

  virtual int Run(CursesWindow* window) = 0;
  virtual void Display(CursesWindow* window) const = 0;

protected:
  int x_;
  int y_;
  int maxsize_;
};


// Base item of an editable value that uses templates to hold the
// value under edit.
template<typename T>
class EditItem : public BaseEditItem {
public:
  typedef std::function<std::string(void)> displayfn;

  EditItem(int x, int y, int maxsize, T data) : BaseEditItem(x, y, maxsize), data_(data) {};
  virtual ~EditItem() {}

  void set_displayfn(displayfn f) { display_ = f; }
  virtual void Display(CursesWindow* window, const std::string& custom) const {
    std::string blanks(maxsize_, ' ');
    window->PutsXY(x_, y_, blanks.c_str());
    window->PutsXY(x_, y_, custom.c_str());
  };

  virtual void Display(CursesWindow* window) const { 
    if (display_) { 
      Display(window, display_()); 
    } else {
      DefaultDisplay(window);
    }
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const = 0;

  const T data() const { return data_; }
  void set_data(T data) { data_ = data; }
  T data_;
  displayfn display_;
};

template<typename T> class StringEditItem : public EditItem<T> {
public:
  StringEditItem(int x, int y, int maxsize, T data, bool uppercase) 
    : EditItem<T>(x, y, maxsize, data), uppercase_(uppercase) {}
  virtual ~StringEditItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    int status = uppercase_ ? UPPER_ONLY : ALL;
    editline(window, reinterpret_cast<char*>(this->data_), this->maxsize_, status, &return_code, "");
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());

    const std::string pattern = wwiv::strings::StringPrintf("%%-%ds", this->maxsize_);
    window->PrintfXY(this->x_, this->y_, pattern.c_str(), this->data_);
  }
private:
  bool uppercase_;
};

template<typename T, int MAXLEN = std::numeric_limits<T>::digits10> 
class NumberEditItem : public EditItem<T*> {
public:
  NumberEditItem(int x, int y, T* data) : EditItem<T*>(x, y, 0, data) {}
  virtual ~NumberEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    std::string s = wwiv::strings::StringPrintf("%-7u", *this->data_);
    editline(window, &s, MAXLEN + 1, NUM_ONLY, &return_code, "");
    *this->data_ = static_cast<T>(atoi(s.c_str()));
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    window->PrintfXY(this->x_, this->y_, "%-7d", *this->data_);
  }
};

template<typename T> 
class ToggleEditItem : public EditItem<T*> {
public:
  ToggleEditItem(int x, int y, const std::vector<std::string>& items, T* data) 
      : EditItem<T*>(x, y, 0, data), items_(items) {
    for (const auto& item : items) {
      this->maxsize_ = std::max<std::size_t>(this->maxsize_, item.size());
    }
  }
  virtual ~ToggleEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    if (*this->data_ > items_.size()) {
      // Data is out of bounds, reset it to a senible value.
      *this->data_ = 0;
    }
    *this->data_ = toggleitem(window, *this->data_, items_, &return_code);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    try {
      window->PutsXY(this->x_, this->y_, this->items_.at(*this->data_).c_str());
    } catch (const std::out_of_range) {
      // Leave it empty since we are out of range.
    }
  }
private:
  const std::vector<std::string> items_;
};

template<typename T> 
class FlagEditItem : public EditItem<T*> {
public:
  FlagEditItem(int x, int y, int flag, const std::string& on, const std::string& off, T* data) 
      : EditItem<T*>(x, y, 0, data), flag_(flag) {
    this->maxsize_ = std::max<int>(on.size(), off.size());
    this->items_.push_back(off);
    this->items_.push_back(on);
  }
  virtual ~FlagEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    int state = (*this->data_ & this->flag_) ? 1 : 0;
    state = toggleitem(window, state, this->items_, &return_code);
    if (state == 0) {
      *this->data_ &= ~this->flag_;
    } else {
      *this->data_ |= this->flag_;
    }
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    int state = (*this->data_ & this->flag_) ? 1 : 0;
    window->PrintfXY(this->x_, this->y_, "%s", this->items_.at(state).c_str());
  }
private:
  std::vector<std::string> items_;
  int flag_;
};

static const char* restrictstring = "LCMA*PEVKNU     ";
class RestrictionsEditItem : public EditItem<uint16_t*> {
public:
  RestrictionsEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 0, data) {}
  virtual ~RestrictionsEditItem() {}

  virtual int Run(CursesWindow* window) {

    window->GotoXY(this->x_, this->y_);
    char s[21];
    char rs[21];
    char ch1 = '0';
    int return_code = 0;

    strcpy(rs, restrictstring);
    for (int i = 0; i <= 15; i++) {
      if (rs[i] == ' ') {
        rs[i] = ch1++;
      }
      if (*this->data_ & (1 << i)) {
        s[i] = rs[i];
      } else {
        s[i] = 32;
      }
    }
    s[16] = 0;

    editline(window, s, 16, SET, &return_code, rs);

    *this->data_ = 0;
    for (int i = 0; i < 16; i++) {
      if (s[i] != 32 && s[i] != 0) {
        *this->data_ |= (1 << i);
      }
    }

    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    char s[21];
    for (int i=0; i < 16; i++) {
      if (*this->data_ & (1 << i)) {
        s[i] = restrictstring[i];
      } else {
        s[i] = 32;
      }
    }
    s[16] = 0;
    window->PrintfXY(this->x_, this->y_, "%s", s);
  }
};

static const char* ar_string = "ABCDEFGHIJKLMNOP";
class ArEditItem : public EditItem<uint16_t*> {
public:
  ArEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 0, data) {}
  virtual ~ArEditItem() {}

  virtual int Run(CursesWindow* window) {

    window->GotoXY(this->x_, this->y_);
    char s[21];
    char rs[21];
    char ch1 = '0';
    int return_code = 0;

    strcpy(rs, ar_string);
    for (int i = 0; i <= 15; i++) {
      if (rs[i] == ' ') {
        rs[i] = ch1++;
      }
      if (*this->data_ & (1 << i)) {
        s[i] = rs[i];
      } else {
        s[i] = 32;
      }
    }
    s[16] = 0;
    editline(window, s, 16, SET, &return_code, rs);
    *this->data_ = 0;
    for (int i = 0; i < 16; i++) {
      if (s[i] != 32 && s[i] != 0) {
        *this->data_ |= (1 << i);
      }
    }
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    char s[21];
    for (int i=0; i < 16; i++) {
      if (*this->data_ & (1 << i)) {
        s[i] = ar_string[i];
      } else {
        s[i] = 32;
      }
    }
    s[16] = 0;
    window->PrintfXY(this->x_, this->y_, "%s", s);
  }
};

class BooleanEditItem : public EditItem<bool*> {
public:
  BooleanEditItem(int x, int y, bool* data) : EditItem<bool*>(x, y, 4, data) {}
  virtual ~BooleanEditItem() {}

  virtual int Run(CursesWindow* window) {
    static const std::vector<std::string> boolean_strings = { "No ", "Yes" };

    window->GotoXY(this->x_, this->y_);
    int data = *this->data_ ? 1 : 0;
    int return_code = 0;
    data = toggleitem(window, data, boolean_strings, &return_code);

    *this->data_ = (data > 0) ? true : false;
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    static const std::vector<std::string> boolean_strings = { "No ", "Yes" };
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());

    int data = *this->data_ ? 1 : 0;
    window->PrintfXY(this->x_, this->y_, "%s", boolean_strings.at(data).c_str());
  }
};

class CustomEditItem : public BaseEditItem {
public:
  typedef std::function<void(const std::string&)> displayfn;
  typedef std::function<std::string(void)> prefn;
  typedef std::function<void(const std::string&)> postfn;
  CustomEditItem(int x, int y, int maxsize,
    prefn to_field, postfn from_field) 
      : BaseEditItem(x, y, maxsize), 
        to_field_(to_field), from_field_(from_field) {}

  virtual int Run(CursesWindow* window);
  virtual void Display(CursesWindow* window) const;
  void set_displayfn(CustomEditItem::displayfn f) { display_ = f; }

private:
  prefn to_field_;
  postfn from_field_;
  displayfn display_;
};

class FilePathItem : public EditItem<char*> {
public:
  FilePathItem(int x, int y, int maxsize, char* data) 
    : EditItem<char*>(x, y, maxsize, data) {}
  virtual ~FilePathItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, &return_code, "");
    trimstrpath(this->data_);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());

    const std::string pattern = wwiv::strings::StringPrintf("%%-%ds", this->maxsize_);
    window->PrintfXY(this->x_, this->y_, pattern.c_str(), this->data_);
  }
};

class CommandLineItem : public EditItem<char*> {
public:
  CommandLineItem(int x, int y, int maxsize, char* data) 
    : EditItem<char*>(x, y, maxsize, data) {}
  virtual ~CommandLineItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, &return_code, "");
    wwiv::strings::StringTrimEnd(this->data_);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());

    const std::string pattern = wwiv::strings::StringPrintf("%%-%ds", this->maxsize_);
    window->PrintfXY(this->x_, this->y_, pattern.c_str(), this->data_);
  }
};

class EditItems {
public:
  typedef std::function<void(void)> additional_helpfn;
  EditItems(std::initializer_list<BaseEditItem*> l)
    : items_(l), navigation_help_items_(StandardNavigationHelpItems()),
      editor_help_items_(StandardEditorHelpItems()), 
      edit_mode_(false) {}
  virtual ~EditItems();

  virtual void Run();
  virtual void Display() const;

  void set_navigation_help_items(const std::vector<HelpItem> items) { navigation_help_items_ = items; }
  void set_editmode_help_items(const std::vector<HelpItem> items) { editor_help_items_ = items; }
  void set_navigation_extra_help_items(const std::vector<HelpItem> items) { navigation_extra_help_items_ = items; }
  std::vector<BaseEditItem*>& items() { return items_; }

  void set_curses_io(CursesIO* io, CursesWindow* window) { io_ = io; window_ = window; }
  CursesWindow* window() const { return window_; }

  static std::vector<HelpItem> StandardNavigationHelpItems() {
    return { {"Esc", "Exit"}, 
        { "Enter", "Edit" },
        { "[", "Previous" },
        { "]", "Next" },
        { "{", "Previous 10" },
        { "}", "Next 10" },
    };
  }

  static std::vector<HelpItem> StandardEditorHelpItems() {
    return { {"Esc", "Exit"} };
  }

  static std::vector<HelpItem> ExitOnlyHelpItems() {
    return { {"Esc", "Exit"} };
  }

private:
  std::vector<BaseEditItem*> items_;
  std::vector<HelpItem> navigation_help_items_;
  std::vector<HelpItem> navigation_extra_help_items_;
  std::vector<HelpItem> editor_help_items_;
  CursesWindow* window_;
  CursesIO* io_;
  bool edit_mode_;
};

#endif // __INCLUDED_INPUT_H__
