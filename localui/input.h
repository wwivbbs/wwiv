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
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "wwivconfig/utility.h"

enum class EditLineMode { NUM_ONLY, UPPER_ONLY, ALL, SET };

#ifndef EDITLINE_FILENAME_CASE
#ifdef __unix__
#define EDITLINE_FILENAME_CASE EditLineMode::ALL
#else
#define EDITLINE_FILENAME_CASE EditLineMode::ALL
#endif // __unix__
#endif // EDITLINE_FILENAME_CASE

// Function prototypes

enum class EditlineResult { PREV, NEXT, DONE, ABORTED };

bool dialog_yn(CursesWindow* window, const std::vector<std::string>& text);
bool dialog_yn(CursesWindow* window, const std::string& prompt);
std::string dialog_input_string(CursesWindow* window, const std::string& prompt, size_t max_length);
int dialog_input_number(CursesWindow* window, const std::string& prompt, int min_value,
                        int max_value);
char onek(CursesWindow* window, const char* s);
EditlineResult editline(CursesWindow* window, std::string* s, int len, EditLineMode status,
                        const char* ss);
EditlineResult editline(CursesWindow* window, char* s, int len, EditLineMode status,
                        const char* ss);
std::vector<std::string>::size_type toggleitem(CursesWindow* window,
                                               std::vector<std::string>::size_type value,
                                               const std::vector<std::string>& strings,
                                               EditlineResult* returncode);

void input_password(CursesWindow* window, const std::string& prompt,
                    const std::vector<std::string>& text, std::string* output, int max_length);
int messagebox(UIWindow* window, const std::string& text);
int messagebox(UIWindow* window, const std::vector<std::string>& text);

void trimstrpath(char* s);

template <typename T>
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
  BaseEditItem(int x, int y, int maxsize) : x_(x), y_(y), maxsize_(maxsize){};
  virtual ~BaseEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) = 0;
  virtual void Display(CursesWindow* window) const = 0;
  BaseEditItem* set_help_text(const std::string& help_text) {
    help_text_ = help_text;
    return this;
  }
  const std::string& help_text() const { return help_text_; }
  virtual int x() const noexcept { return x_; }
  virtual void set_x(int x) noexcept { x_ = x; }
  virtual int y() const noexcept { return y_; }
  virtual int maxsize() const noexcept { return maxsize_; }

protected:
  int x_;
  int y_;
  int maxsize_;
  std::string help_text_;
};

// Label class that's used to display a label for an EditItem
class Label {
public:
  Label(int x, int y, const std::string& text) : x_(x), y_(y), text_(text), width_(text.size()) {}
  Label(int x, int y, int width, const std::string& text)
      : x_(x), y_(y), text_(text), width_(width) {}
  virtual void Display(CursesWindow* window);
  virtual void Display(CursesWindow* window, int x, int y);
  void set_width(int width) noexcept { width_ = width; }
  int width() const noexcept { return width_; }
  void set_right_justified(bool r) noexcept { right_justify_ = r; }
  virtual int x() const noexcept { return x_; }
  virtual int y() const noexcept { return y_; }
  virtual const std::string& text() const noexcept { return text_; }

protected:
  int x_;
  int y_;
  int width_;
  bool right_justify_{true};
  const std::string text_;
};

// Base item of an editable value that uses templates to hold the
// value under edit.
template <typename T> class EditItem : public BaseEditItem {
public:
  typedef std::function<std::string(void)> displayfn;

  EditItem(int x, int y, int maxsize, T data) : BaseEditItem(x, y, maxsize), data_(data){};
  virtual ~EditItem() {}

  void set_displayfn(displayfn f) { display_ = f; }

  virtual void Display(CursesWindow* window) const {
    if (display_) {
      std::string blanks(maxsize_, ' ');
      auto custom = display_();
      window->PutsXY(x_, y_, blanks.c_str());
      window->PutsXY(x_, y_, custom.c_str());
    } else {
      DefaultDisplay(window);
    }
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const = 0;
  virtual void DefaultDisplayString(CursesWindow* window, const std::string& text) const {
    auto s = text;
    if (wwiv::stl::size_int(s) > maxsize_) {
      s = text.substr(0, maxsize_);
    } else if (wwiv::stl::size_int(s) < maxsize_) {
      s = text + std::string(static_cast<std::string::size_type>(maxsize_) - text.size(), ' ');
    }

    window->PutsXY(this->x_, this->y_, s);
  }

  const T data() const { return data_; }
  void set_data(T data) { data_ = data; }

  T data_;
  displayfn display_;
};

template <typename T> class StringEditItem : public EditItem<T> {
public:
  StringEditItem(int x, int y, int maxsize, T data, EditLineMode edit_line_mode)
      : EditItem<T>(x, y, maxsize, data), edit_line_mode_(edit_line_mode) {}
  StringEditItem(int x, int y, int maxsize, T data) : StringEditItem(x, y, maxsize, data, EditLineMode::ALL) {}
  virtual ~StringEditItem() {}

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    return editline(window, reinterpret_cast<char*>(data_), maxsize_, edit_line_mode_, "");
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    this->DefaultDisplayString(
        window, std::string(reinterpret_cast<char*>(const_cast<const T>(this->data_))));
  }

private:
  EditLineMode edit_line_mode_;
};


template <> class StringEditItem<std::string&> : public EditItem<std::string&> {
public:
  StringEditItem(int x, int y, int maxsize, std::string& data,  EditLineMode mode)
      : EditItem<std::string&>(x, y, maxsize, data), edit_line_mode_(mode) {}
  virtual ~StringEditItem() {}

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto result = editline(window, &this->data_, this->maxsize_, edit_line_mode_, "");
    return result;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    this->DefaultDisplayString(window, data_);
  }

private:
  EditLineMode edit_line_mode_;
};

template <typename T, int MAXLEN = std::numeric_limits<T>::digits10>
class NumberEditItem : public EditItem<T*> {
public:
  NumberEditItem(int x, int y, T* data) : EditItem<T*>(x, y, MAXLEN + 2, data) {}
  virtual ~NumberEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    auto s = std::to_string(*this->data_);
    auto return_code = editline(window, &s, MAXLEN + 1, EditLineMode::NUM_ONLY, "");
    *this->data_ = wwiv::strings::to_number<T>(s);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    auto d = std::to_string(*this->data_);
    window->PutsXY(this->x_, this->y_, wwiv::strings::pad_to(d, this->maxsize()));
  }
};

template <typename T> constexpr typename std::underlying_type<T>::type enum_to_int(T value) {
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
  for (std::string::size_type i = 0; i < needle.size(); i++) {
    if (haystack == needle.at(i).first) {
      return i;
    }
  }
  return 0;
}

template <typename T> class ToggleEditItem : public EditItem<T*> {
public:
  ToggleEditItem(int x, int y, const std::vector<std::pair<T, std::string>>& items, T* data)
      : EditItem<T*>(x, y, 0, data), items_(items) {
    for (const auto& item : items) {
      this->maxsize_ = std::max<std::size_t>(this->maxsize_, item.second.size());
    }
  }
  virtual ~ToggleEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) {
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});

    window->GotoXY(this->x_, this->y_);
    EditlineResult return_code = EditlineResult::NEXT;
    auto index = index_of(*this->data_, items_);
    const auto items = item_list(items_);
    index = toggleitem(window, index, items, &return_code);
    *this->data_ = items_.at(index).first;
    DefaultDisplay(window);
    out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    try {
      auto index = index_of(*this->data_, items_);
      const auto& s = items_.at(index).second;
      this->DefaultDisplayString(window, s);
    } catch (const std::out_of_range&) {
      this->DefaultDisplayString(window, "");
    }
  }

private:
  const std::vector<std::pair<T, std::string>> items_;
};

static int maxlen_from_list(const std::vector<std::string>& items) {
  std::string::size_type m = 0;
  for (const auto& item : items) {
    m = std::max<std::string::size_type>(m, item.size());
  }
  return m;
}

class StringListItem : public EditItem<std::string&> {
public:
  StringListItem(int x, int y, const std::vector<std::string>& items, std::string& data)
      : EditItem<std::string&>(x, y, maxlen_from_list(items), data), items_(items) {}
  virtual ~StringListItem() {}

  virtual EditlineResult Run(CursesWindow* window) {
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    window->GotoXY(this->x_, this->y_);
    EditlineResult return_code = EditlineResult::NEXT;
    auto it = std::find(items_.begin(), items_.end(), data_);
    std::vector<std::string>::size_type selection = 0;
    if (it != items_.end()) {
      selection = std::distance(items_.begin(), it);
    }
    selection = toggleitem(window, static_cast<std::vector<std::string>::size_type>(selection),
                           items_, &return_code);
    data_ = items_.at(selection);
    out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const { DefaultDisplayString(window, data_); }

private:
  const std::vector<std::string> items_;
};

template <typename T> class FlagEditItem : public EditItem<T*> {
public:
  FlagEditItem(int x, int y, int flag, const std::string& on, const std::string& off, T* data)
      : EditItem<T*>(x, y, 0, data), flag_(flag) {
    this->maxsize_ = std::max<int>(on.size(), off.size());
    this->items_.push_back(off);
    this->items_.push_back(on);
  }
  virtual ~FlagEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) {
    window->GotoXY(this->x_, this->y_);
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    EditlineResult return_code = EditlineResult::NEXT;
    std::vector<std::string>::size_type state = (*this->data_ & this->flag_) ? 1 : 0;
    state = toggleitem(window, state, this->items_, &return_code);
    if (state == 0) {
      *this->data_ &= ~this->flag_;
    } else {
      *this->data_ |= this->flag_;
    }
    out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    int state = (*this->data_ & this->flag_) ? 1 : 0;
    this->DefaultDisplayString(window, items_.at(state));
  }

private:
  std::vector<std::string> items_;
  int flag_;
};

class BaseRestrictionsEditItem : public EditItem<uint16_t*> {
public:
  BaseRestrictionsEditItem(int x, int y, const char* rs, int rs_size, uint16_t* data)
      : EditItem<uint16_t*>(x, y, rs_size, data), rs_size_(rs_size) {
    strcpy(rs_, rs);
  }

  virtual ~BaseRestrictionsEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) {

    window->GotoXY(this->x_, this->y_);
    char s[21];
    char rs[21];
    char ch1 = '0';

    strcpy(rs, rs_);
    for (int i = 0; i < rs_size_; i++) {
      if (rs[i] == ' ') {
        rs[i] = ch1++;
      }
      if (*this->data_ & (1 << i)) {
        s[i] = rs[i];
      } else {
        s[i] = 32;
      }
    }
    s[rs_size_] = 0;

    auto return_code = editline(window, s, 16, EditLineMode::SET, rs);

    *this->data_ = 0;
    for (int i = 0; i < rs_size_; i++) {
      if (s[i] != 32 && s[i] != 0) {
        *this->data_ |= (1 << i);
      }
    }

    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    std::string s = to_restriction_string(*data_, rs_size_, rs_);
    DefaultDisplayString(window, s);
  }
  char rs_[21];
  int rs_size_{0};
};

static const char* ar_string = "ABCDEFGHIJKLMNOP";
static constexpr int ar_string_size = 16;
static const char* xrestrictstring = "LCMA*PEVKNU     ";
static constexpr int xrestrictstring_size = 16;

class RestrictionsEditItem : public BaseRestrictionsEditItem {
public:
  RestrictionsEditItem(int x, int y, uint16_t* data)
      : BaseRestrictionsEditItem(x, y, xrestrictstring, xrestrictstring_size, data) {}
  virtual ~RestrictionsEditItem() {}
};

class ArEditItem : public BaseRestrictionsEditItem {
public:
  ArEditItem(int x, int y, uint16_t* data)
      : BaseRestrictionsEditItem(x, y, ar_string, ar_string_size, data) {}
  virtual ~ArEditItem() {}
};

class BooleanEditItem : public EditItem<bool*> {
public:
  BooleanEditItem(int x, int y, bool* data) : EditItem<bool*>(x, y, 6, data) {}
  virtual ~BooleanEditItem() {}

  virtual EditlineResult Run(CursesWindow* window) {
    out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    static const std::vector<std::string> boolean_strings = {"No ", "Yes"};

    window->GotoXY(this->x_, this->y_);
    std::vector<std::string>::size_type data = *this->data_ ? 1 : 0;
    EditlineResult return_code = EditlineResult::NEXT;
    data = toggleitem(window, data, boolean_strings, &return_code);

    *this->data_ = (data > 0) ? true : false;
    out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const {
    static const std::vector<std::string> boolean_strings = {"No ", "Yes"};
    std::string s = boolean_strings.at(*data_ ? 1 : 0);
    DefaultDisplayString(window, s);
  }
};

class CustomEditItem : public BaseEditItem {
public:
  typedef std::function<void(const std::string&)> displayfn;
  typedef std::function<std::string(void)> prefn;
  typedef std::function<void(const std::string&)> postfn;
  CustomEditItem(int x, int y, int maxsize, prefn to_field, postfn from_field)
      : BaseEditItem(x, y, maxsize), to_field_(to_field), from_field_(from_field) {}

  virtual EditlineResult Run(CursesWindow* window);
  virtual void Display(CursesWindow* window) const;
  void set_displayfn(CustomEditItem::displayfn f) { display_ = f; }

private:
  prefn to_field_;
  postfn from_field_;
  displayfn display_;
};

class FilePathItem : public EditItem<char*> {
public:
  FilePathItem(int x, int y, int maxsize, const std::string& base, char* data)
      : EditItem<char*>(x, y, maxsize, data), base_(base) {
    help_text_ = wwiv::strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  virtual ~FilePathItem() {}

  virtual EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto return_code = editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
    trimstrpath(this->data_);

    // Update what we display in case it changed.
    DefaultDisplay(window);

    auto dir = wwiv::core::File::absolute(this->base_, this->data_);
    if (!wwiv::core::File::Exists(dir)) {
      const auto s1 = wwiv::strings::StrCat("The path '", this->data_, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!wwiv::core::File::mkdirs(dir)) {
          messagebox(window, {"Unable to create directory: ", dir});
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

class StringFilePathItem : public EditItem<std::string&> {
public:
  StringFilePathItem(int x, int y, int maxsize, const std::string& base, std::string& data)
      : EditItem<std::string&>(x, y, maxsize, data), base_(base) {
    help_text_ = wwiv::strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  virtual ~StringFilePathItem() {}

  virtual EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto return_code = editline(window, &this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
    wwiv::strings::StringTrimEnd(&this->data_);
    if (!data_.empty()) {
      wwiv::core::File::EnsureTrailingSlash(&data_);
    }
    // Update what we display in case it changed.
    DefaultDisplay(window);

    auto dir = wwiv::core::File::absolute(this->base_, this->data_);
    if (!wwiv::core::File::Exists(dir)) {
      const auto s1 = wwiv::strings::StrCat("The path '", dir, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!wwiv::core::File::mkdirs(dir)) {
          messagebox(window, {"Unable to create directory: ", dir});
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
  CommandLineItem(int x, int y, int maxsize, char* data) : EditItem<char*>(x, y, maxsize, data) {}
  virtual ~CommandLineItem() {}

  virtual EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto return_code = editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, "");
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
  EditItems()
      : navigation_help_items_(StandardNavigationHelpItems()),
        editor_help_items_(StandardEditorHelpItems()), edit_mode_(false) {}
  virtual ~EditItems();

  virtual void Run(const std::string& title = "");
  virtual void Display() const;

  void set_navigation_help_items(const std::vector<HelpItem> items) {
    navigation_help_items_ = items;
  }
  void set_editmode_help_items(const std::vector<HelpItem> items) { editor_help_items_ = items; }
  void set_navigation_extra_help_items(const std::vector<HelpItem> items) {
    navigation_extra_help_items_ = items;
  }
  std::vector<BaseEditItem*>& items() { return items_; }
  /** Adds an item to the list of EditItems. */
  BaseEditItem* add(BaseEditItem* item);
  /** Adds a label to the list of EditItems. */
  Label* add(Label* label);
  /** Adds a list of labels */
  void add_labels(std::initializer_list<Label*> items);
  /** Adds a list of EditItems */
  void add_items(std::initializer_list<BaseEditItem*> items);
  /** Adds a label on the same line as item */
  BaseEditItem* add(Label* label, BaseEditItem* item);
  void create_window(const std::string& title);

  CursesWindow* window() const { return window_.get(); }
  size_t size() const noexcept { return items_.size(); }

  int max_display_width() const {
    int result = 0;
    for (const auto i : items_) {
      if ((i->x() + i->maxsize()) > result) {
        result = i->x() + i->maxsize();
      }
    }
    return std::min<int>(out->window()->GetMaxX(), result + 2); // 2 is padding
  }

  int max_display_height() {
    int result = 1;
    for (const auto l : labels_) {
      if (l->y() > result) {
        result = l->y();
      }
    }
    for (const auto i : items_) {
      if (i->y() > result) {
        result = i->y();
      }
    }
    return std::min<int>(out->window()->GetMaxY(), result + 2);
  }

  /** Returns the size of the longest label */
  int max_label_width() const {
    std::string::size_type result = 0;
    for (const auto l : labels_) {
      if (l->text().size() > result) {
        result = l->text().size();
      }
    }
    return static_cast<int>(result);
  }

  /**
   * Moves the labels to the x position just after the labels.
   * This only works for single column layouts of: {label:} {item}
   */
  void relayout_items_and_labels() {
    auto x = max_label_width();
    for (auto l : labels_) {
      l->set_width(x);
    }
    for (auto i : items_) {
      i->set_x(x + 2 + 1); // 2 is a hack since that's always col1 position for label.
    }
  }

  static std::vector<HelpItem> StandardNavigationHelpItems() {
    return {
        {"Esc", "Exit"}, {"Enter", "Edit"},    {"[", "Previous"},
        {"]", "Next"},   {"{", "Previous 10"}, {"}", "Next 10"},
    };
  }

  static std::vector<HelpItem> StandardEditorHelpItems() { return {{"Esc", "Exit"}}; }

  static std::vector<HelpItem> ExitOnlyHelpItems() { return {{"Esc", "Exit"}}; }

private:
  std::vector<BaseEditItem*> items_;
  std::vector<Label*> labels_;
  std::vector<HelpItem> navigation_help_items_;
  std::vector<HelpItem> navigation_extra_help_items_;
  std::vector<HelpItem> editor_help_items_;
  std::unique_ptr<CursesWindow> window_;
  bool edit_mode_;
};

#endif // __INCLUDED_INPUT_H__
