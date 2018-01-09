/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#include <type_traits>
#include <utility>
#include <vector>

#include <string.h>

#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "init/utility.h"
#include "core/stl.h"

enum class EditLineMode {
  NUM_ONLY,
  UPPER_ONLY,
  ALL,
  SET
};

#ifndef EDITLINE_FILENAME_CASE
#ifdef __unix__
#define EDITLINE_FILENAME_CASE EditLineMode::ALL
#else
#define EDITLINE_FILENAME_CASE EditLineMode::ALL
#endif  // __unix__
#endif  // EDITLINE_FILENAME_CASE

// Function prototypes

bool dialog_yn(CursesWindow* window, const std::vector<std::string>& text);
bool dialog_yn(CursesWindow* window, const std::string& prompt);
std::string dialog_input_string(CursesWindow* window, const std::string& prompt, size_t max_length);
int dialog_input_number(CursesWindow* window, const std::string& prompt, int min_value, int max_value);
char onek(CursesWindow* window, const char *s);
int editline(CursesWindow* window, std::string* s, int len, EditLineMode status, const char *ss);
int editline(CursesWindow* window, char *s, int len, EditLineMode status, const char *ss);
std::vector<std::string>::size_type toggleitem(CursesWindow* window, std::vector<std::string>::size_type value, const std::vector<std::string>& strings, int *returncode);

void input_password(CursesWindow* window, const std::string& prompt, const std::vector<std::string>& text, std::string *output, int max_length);
int messagebox(UIWindow* window, const std::string& text);
int messagebox(UIWindow* window, const std::vector<std::string>& text);

void trimstrpath(char *s);

template<typename T>
static std::string to_restriction_string(T data, std::size_t size, const char* res) {
  std::string s;
  for (size_t i = 0; i < size; i++) {
    if (data & (1 << i)) {
      s.push_back(res[i]);
    } else {
      s.push_back(' ');
    }
  }
  return s;
}

// Base item of an editable value, this class does not use templates.
class BaseEditItem {
public:
  BaseEditItem(int x, int y, int maxsize)
    : x_(x), y_(y), maxsize_(maxsize) {};
  virtual ~BaseEditItem() {}

  virtual int Run(CursesWindow* window) = 0;
  virtual void Display(CursesWindow* window) const = 0;
  virtual void set_curses_io(CursesIO* io) = 0;
  void set_help_text(const std::string& help_text) { help_text_ = help_text; }
  const std::string& help_text() const { return help_text_; }

protected:
  int x_;
  int y_;
  int maxsize_;
  std::string help_text_;
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
  virtual void DefaultDisplayString(CursesWindow* window, const std::string& text) const {
    std::string s = text;
    if (wwiv::stl::size_int(s) > maxsize_) {
      s = text.substr(0, maxsize_);
    } else if (wwiv::stl::size_int(s) < maxsize_) {
      s = text + std::string(static_cast<std::string::size_type> (maxsize_)-text.size(), ' ');
    }

    window->PutsXY(this->x_, this->y_, s);
  }

  const T data() const { return data_; }
  void set_data(T data) { data_ = data; }
  virtual void set_curses_io(CursesIO* io) override { io_ = io; }

  CursesIO* io() const { return io_; }

  CursesIO* io_ = nullptr;
  T data_;
  displayfn display_;
};

template<typename T> class StringEditItem: public EditItem<T> {
public:
  StringEditItem(int x, int y, int maxsize, T data, bool uppercase)
    : EditItem<T>(x, y, maxsize, data), uppercase_(uppercase) {}
  StringEditItem(int x, int y, int maxsize, T data)
    : StringEditItem(x, y, maxsize, data, false) {}
  virtual ~StringEditItem() {}

  int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto st = uppercase_ ? EditLineMode::UPPER_ONLY : EditLineMode::ALL;
    int return_code = editline(window, reinterpret_cast<char*>(this->data_), this->maxsize_, st, "");
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    std::string s = reinterpret_cast<char*>(const_cast<const T>(this->data_));
    this->DefaultDisplayString(window, s);
  }
private:
  bool uppercase_;
};

template<> class StringEditItem<std::string&>: public EditItem<std::string&> {
public:
  StringEditItem(int x, int y, int maxsize, std::string& data, bool uppercase)
    : EditItem<std::string&>(x, y, maxsize, data), uppercase_(uppercase) {}
  virtual ~StringEditItem() {}

  int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto st = uppercase_ ? EditLineMode::UPPER_ONLY : EditLineMode::ALL;
    int return_code = editline(window, &this->data_, this->maxsize_, st, "");
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    this->DefaultDisplayString(window, data_);
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
    std::string s = std::to_string(*this->data_);
    int return_code = editline(window, &s, MAXLEN + 1, EditLineMode::NUM_ONLY, "");
    *this->data_ = wwiv::strings::to_number<T>(s);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());
    window->PrintfXY(this->x_, this->y_, "%-7d", *this->data_);
  }
};

template <typename T>
constexpr typename std::underlying_type<T>::type enum_to_int(T value) {
  return static_cast<typename std::underlying_type<T>::type>(value);
}

template <typename T>
std::vector<std::string> item_list(const std::vector<std::pair<T, std::string>>& orig) {
  std::vector<std::string> items;
  for (const auto& e : orig) {
    items.push_back(e.second);
  }
  return items;
}

template <typename T>
std::size_t index_of(T& haystack, const std::vector<std::pair<T, std::string>>& needle) {
  for (std::size_t i = 0; i < needle.size(); i++) {
    if (haystack == needle.at(i).first) {
      return i;
    }
  }
  return 0;
}

template<typename T> 
class ToggleEditItem : public EditItem<T*> {
public:
  ToggleEditItem(CursesIO* io, int x, int y, const std::vector<std::pair<T, std::string>>& items, T* data)
      : EditItem<T*>(x, y, 0, data), io_(io), items_(items) {
    for (const auto& item : items) {
      this->maxsize_ = std::max<std::size_t>(this->maxsize_, item.second.size());
    }
  }
  virtual ~ToggleEditItem() {}

  virtual int Run(CursesWindow* window) {
    io_->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, { "SPACE", "Toggle Item" } });

    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    std::size_t index = index_of(*this->data_, items_);
    const std::vector<std::string> items = item_list(items_);
    index = toggleitem(window, index, items, &return_code);
    *this->data_ = items_.at(index).first;
    DefaultDisplay(window);
    io_->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    try {
      std::size_t index = index_of(*this->data_, items_);
      const auto& s = items_.at(index).second;
      this->DefaultDisplayString(window, s);
    } catch (const std::out_of_range&) {
      this->DefaultDisplayString(window, "");
    }
  }
private:
  CursesIO* io_;
  const std::vector<std::pair<T, std::string>> items_;
};

class StringListItem: public EditItem<std::string&> {
public:
  StringListItem(CursesIO* io, int x, int y, const std::vector<std::string>& items, std::string& data)
    : EditItem<std::string&>(x, y, 0, data), io_(io), items_(items) {
    for (const auto& item : items) {
      this->maxsize_ = std::max<std::size_t>(this->maxsize_, item.size());
    }
  }
  virtual ~StringListItem() {}

  virtual int Run(CursesWindow* window) {
    io_->footer()->ShowHelpItems(0, {{"Esc", "Exit"},{"SPACE", "Toggle Item"}});
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    auto it = std::find(items_.begin(), items_.end(), data_);
    std::vector<std::string>::size_type selection = 0;
    if (it != items_.end()) {
      selection = std::distance(items_.begin(), it);
    }
    selection = toggleitem(window, static_cast<std::vector<std::string>::size_type>(selection), items_, &return_code);
    data_ = items_.at(selection);
    io_->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    DefaultDisplayString(window, data_);
  }
private:
  CursesIO* io_;
  const std::vector<std::string> items_;
};

template<typename T>
class FlagEditItem : public EditItem<T*> {
public:
  FlagEditItem(CursesIO* io, int x, int y, int flag, const std::string& on, const std::string& off, T* data) 
      : EditItem<T*>(x, y, 0, data), io_(io), flag_(flag) {
    this->maxsize_ = std::max<int>(on.size(), off.size());
    this->items_.push_back(off);
    this->items_.push_back(on);
  }
  virtual ~FlagEditItem() {}

  virtual int Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    io_->footer()->ShowHelpItems(0, {{"Esc", "Exit"},{"SPACE", "Toggle Item"}});
    int return_code = 0;
    std::vector<std::string>::size_type state = (*this->data_ & this->flag_) ? 1 : 0;
    state = toggleitem(window, state, this->items_, &return_code);
    if (state == 0) {
      *this->data_ &= ~this->flag_;
    } else {
      *this->data_ |= this->flag_;
    }
    io_->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    int state = (*this->data_ & this->flag_) ? 1 : 0;
    this->DefaultDisplayString(window, items_.at(state));
  }
private:
  CursesIO* io_;
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

    int return_code = editline(window, s, 16, EditLineMode::SET, rs);

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
    std::string s = to_restriction_string(*data_, 16, restrictstring);
    DefaultDisplayString(window, s);
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
    int return_code = editline(window, s, 16, EditLineMode::SET, rs);
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
    std::string s = to_restriction_string(*data_, 16, ar_string);
    DefaultDisplayString(window, s);
  }
};

class BooleanEditItem : public EditItem<bool*> {
public:
  BooleanEditItem(CursesIO* io, int x, int y, bool* data) : EditItem<bool*>(x, y, 4, data), io_(io) {}
  virtual ~BooleanEditItem() {}

  virtual int Run(CursesWindow* window) {
    io_->footer()->ShowHelpItems(0, {{"Esc", "Exit"},{"SPACE", "Toggle Item"}});
    static const std::vector<std::string> boolean_strings = { "No ", "Yes" };

    window->GotoXY(this->x_, this->y_);
    std::vector<std::string>::size_type data = *this->data_ ? 1 : 0;
    int return_code = 0;
    data = toggleitem(window, data, boolean_strings, &return_code);

    *this->data_ = (data > 0) ? true : false;
    io_->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    static const std::vector<std::string> boolean_strings = { "No ", "Yes" };
    std::string s = boolean_strings.at(*data_ ? 1 : 0);
    DefaultDisplayString(window, s);
  }

private:
  CursesIO* io_;
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
  virtual void set_curses_io(CursesIO* io) override { io_ = io; }

private:
  prefn to_field_;
  postfn from_field_;
  displayfn display_;
  CursesIO* io_ = nullptr;
};

class FilePathItem : public EditItem<char*> {
public:
  FilePathItem(int x, int y, int maxsize, const std::string& base, char* data) 
    : EditItem<char*>(x, y, maxsize, data), base_(base) {
    help_text_ = wwiv::strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  virtual ~FilePathItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
    trimstrpath(this->data_);

    // Update what we display in case it changed.
    DefaultDisplay(window);

    auto dir = File::absolute(this->base_, this->data_);
    if (!File::Exists(dir)) {
      const std::string s1 = wwiv::strings::StrCat("The path '", this->data_, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!File::mkdirs(dir)) {
          messagebox(window, { "Unable to create directory: ", dir });
        }
      }
    }
    return return_code;
  }
protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    DefaultDisplayString(window, data_);
  }
private:
  const std::string base_;
};

class StringFilePathItem: public EditItem<std::string&> {
public:
  StringFilePathItem(int x, int y, int maxsize, const std::string& base, std::string& data)
    : EditItem<std::string&>(x, y, maxsize, data), base_(base) {
    help_text_ = wwiv::strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  virtual ~StringFilePathItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = editline(window, &this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
    wwiv::strings::StringTrimEnd(&this->data_);
    if (!data_.empty()) {
      File::EnsureTrailingSlash(&data_);
    }
    // Update what we display in case it changed.
    DefaultDisplay(window);

    auto dir = File::absolute(this->base_, this->data_);
    if (!File::Exists(dir)) {
      const std::string s1 = wwiv::strings::StrCat("The path '", dir, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!File::mkdirs(dir)) {
          messagebox(window, { "Unable to create directory: ", dir });
        }
      }
    }
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    DefaultDisplayString(window, data_);
  }
private:
  const std::string base_;
};

class CommandLineItem : public EditItem<char*> {
public:
  CommandLineItem(int x, int y, int maxsize, char* data) 
    : EditItem<char*>(x, y, maxsize, data) {}
  virtual ~CommandLineItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
    wwiv::strings::StringTrimEnd(this->data_);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    DefaultDisplayString(window, data_);
  }
};

class EditItems {
public:
  typedef std::function<void(void)> additional_helpfn;
  EditItems(std::initializer_list<BaseEditItem*> l)
    : items_(l), navigation_help_items_(StandardNavigationHelpItems()),
      editor_help_items_(StandardEditorHelpItems()), 
      edit_mode_(false), io_(CursesIO::Get()) {}
  virtual ~EditItems();

  virtual void Run();
  virtual void Display() const;

  void set_navigation_help_items(const std::vector<HelpItem> items) { navigation_help_items_ = items; }
  void set_editmode_help_items(const std::vector<HelpItem> items) { editor_help_items_ = items; }
  void set_navigation_extra_help_items(const std::vector<HelpItem> items) { navigation_extra_help_items_ = items; }
  std::vector<BaseEditItem*>& items() { return items_; }
  BaseEditItem* add(BaseEditItem* item) {
    items_.push_back(item);
    return item;
  }

  void set_curses_io(CursesIO* io, CursesWindow* window);
  CursesWindow* window() const { return window_; }
  size_t size() const { return items_.size(); }

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
