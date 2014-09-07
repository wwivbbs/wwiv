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
#include <string>
#include <vector>

#include <string.h>

#include "core/strings.h"
#include "initlib/curses_io.h"
#include "initlib/curses_win.h"

#define NUM_ONLY            1
#define UPPER_ONLY          2
#define ALL                 4
#define SET                 8

// Function prototypes
void nlx(int numLines = 1);

bool dialog_yn(CursesWindow* window, const std::string prompt);
int input_number(CursesWindow* window, int max_digits);
int dialog_input_number(CursesWindow* window, const std::string prompt, int min_value, int max_value);
char onek(CursesWindow* window, const char *s);
void editline(CursesWindow* window, std::string* s, int len, int status, int *returncode, const char *ss);
void editline(CursesWindow* window, char *s, int len, int status, int *returncode, const char *ss);
int toggleitem(CursesWindow* window, int value, const std::vector<std::string>& strings, int *returncode);
void pausescr(CursesWindow* window);

int GetNextSelectionPosition(int nMin, int nMax, int nCurrentPos, int nReturnCode);
void input_password(CursesWindow* window, const std::string prompt, const std::vector<std::string>& text, char *output, int max_length);
void messagebox(CursesWindow* window, const std::string text);
void messagebox(CursesWindow* window, const std::vector<std::string>& text);


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

    char pattern[81];
    sprintf(pattern, "%%-%ds", this->maxsize_);
    window->PrintfXY(this->x_, this->y_, pattern, this->data_);
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
  ToggleEditItem(int x, int y, const std::vector<std::string> items, T* data) 
      : EditItem<T*>(x, y, 0, data), items_(items) {
    for (const auto& item : items) {
      maxlen_ = std::max(maxlen_, item.size());
    }
  }
  virtual ~ToggleEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    *this->data_ = toggleitem(window, *this->data_, items_, &return_code);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    window->PrintfXY(this->x_, this->y_, "%s", this->items_.at(*this->data_).c_str());
  }
private:
  const std::vector<std::string> items_;
};

class RestrictionsEditItem : public EditItem<uint16_t*> {
public:
  RestrictionsEditItem(int x, int y, uint16_t* data) : EditItem<uint16_t*>(x, y, 0, data) {}
  virtual ~RestrictionsEditItem() {}

  virtual int Run(CursesWindow* window) {
    static const char* restrictstring = "LCMA*PEVKNU     ";

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
    static const char* restrictstring = "LCMA*PEVKNU     ";

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
