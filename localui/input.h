/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
// ReSharper disable CppClangTidyBugproneMacroParentheses
#ifndef INCLUDED_LOCALUI_INPUT_H
#define INCLUDED_LOCALUI_INPUT_H

#include "core/file.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/wwiv_curses.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/value/valueprovider.h"

#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace wwiv::local::ui {

enum class EditLineMode { NUM_ONLY, UPPER_ONLY, LOWER, ALL, SET };

#ifdef __unix__
static constexpr auto EDITLINE_FILENAME_CASE = EditLineMode::ALL;
#else
static constexpr auto EDITLINE_FILENAME_CASE = EditLineMode::ALL;
#endif // __unix__

enum class EditlineResult { PREV, NEXT, DONE, ABORTED };

class NavigationKeyConfig final {
public:
  NavigationKeyConfig() = delete;
  NavigationKeyConfig(const NavigationKeyConfig&) = delete;
  NavigationKeyConfig(NavigationKeyConfig&&) = delete;
  NavigationKeyConfig& operator=(const NavigationKeyConfig&) = delete;
  NavigationKeyConfig& operator=(NavigationKeyConfig&&) = delete;
  explicit NavigationKeyConfig(std::string keys) : keys_(std::move(keys)) {}
  ~NavigationKeyConfig() = default;

  char up{'['};
  char dn{']'};
  char up10{'{'};
  char down10{'}'};
  char quit{'Q'};
  const std::string keys_;
};

bool dialog_yn(CursesWindow* window, const std::vector<std::string>& text);
bool dialog_yn(CursesWindow* window, const std::string& text);
std::string dialog_input_string(CursesWindow* window, const std::string& prompt, int max_length);
int dialog_input_number(CursesWindow* window, const std::string& prompt, int min_value,
                        int max_value);
int onek(UIWindow* window, const std::string& allowed, bool allow_keycodes = false);

/////////////////////////////////////////////////////////////////////////////
// Line editing support

typedef std::function<void(std::string)> edline_validation_fn;

EditlineResult editline(CursesWindow* window, std::string* s, int len, EditLineMode status,
                        const char* ss, edline_validation_fn);
EditlineResult editline(CursesWindow* window, char* s, int len, EditLineMode status,
                        const char* ss, const edline_validation_fn&);

EditlineResult editline(CursesWindow* window, std::string* s, int len, EditLineMode status,
                        const char* ss);
EditlineResult editline(CursesWindow* window, char* s, int len, EditLineMode status,
                        const char* ss);

/////////////////////////////////////////////////////////////////////////////
// Line editing support

std::vector<std::string>::size_type toggleitem(CursesWindow* window,
                                               std::vector<std::string>::size_type value,
                                               const std::vector<std::string>& strings,
                                               EditlineResult* rc);

void input_password(CursesWindow* window, const std::string& prompt,
                    const std::vector<std::string>& text, std::string* output, int max_length);
int messagebox(UIWindow* window, const std::string& text);
int messagebox(UIWindow* window, const std::vector<std::string>& text);
int input_select_item(UIWindow* window, const std::string& prompt,
                      const std::vector<std::string>& items);
void trimstrpath(char* s);

template <typename T>
static std::string to_restriction_string(T data, int size, const std::string& res) {
  std::string s;
  for (auto i = 0; i < size; i++) {
    if (data & (1 << i)) {
      s.push_back(res[i]);
    } else {
      s.push_back(' ');
    }
  }
  return s;
}

// Base class of all items that can appear in a LocalUI based environment.

class Item {
public:
  Item() = delete;
  Item(const Item&) = delete;
  Item(Item&&) = delete;
  Item& operator=(const Item&) = delete;
  Item& operator=(Item&&) = delete;
  explicit Item(int width) : x_(0), y_(0), width_(width) {}
  virtual ~Item() = default;
  typedef ssize_t size_type;

  [[nodiscard]] virtual int x() const noexcept { return x_; }
  [[nodiscard]] virtual int y() const noexcept { return y_; }
  virtual void set_x(int x) noexcept { x_ = x; }
  virtual void set_y(int y) noexcept { y_ = y; }
  [[nodiscard]] virtual int width() const noexcept { return width_; }
  virtual void set_width(int width) noexcept { width_ = width; }
  [[nodiscard]] std::size_t size() const noexcept { return width_; }

  [[nodiscard]] virtual int column() const noexcept { return column_; }
  void set_column(int column) noexcept { column_ = column; }

  virtual void Display(CursesWindow* window) const = 0;

 protected:
  int x_;
  int y_;
  int width_;
  int column_{1};
};

// Base item of an editable value, this class does not use templates.
class BaseEditItem : public Item {
public:
  BaseEditItem(int maxsize) : Item(maxsize) {}
  ~BaseEditItem() override = default;

  BaseEditItem() = delete;
  BaseEditItem(BaseEditItem const&) = delete;
  BaseEditItem(BaseEditItem&&) = delete;
  BaseEditItem& operator=(BaseEditItem const&) = delete;
  BaseEditItem& operator=(BaseEditItem&&) = delete;

  void Display(CursesWindow* window) const override = 0;
  virtual EditlineResult Run(CursesWindow* window) = 0;
  BaseEditItem* set_help_text(const std::string& help_text) {
    help_text_ = help_text;
    return this;
  }

  [[nodiscard]] std::string help_text() const { return help_text_; }

protected:
  std::string help_text_;
};

// Label class that's used to display a label for an EditItem
class Label : public Item {
public:
  explicit Label(const std::string& text) : Label(wwiv::stl::size_int(text), text) {}

  void Display(CursesWindow* window) const override;
  void set_right_justified(bool r) noexcept { right_justify_ = r; }
  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  [[nodiscard]] virtual int height() const { return 1; }

protected:
  Label(int width, std::string text) : Item(width), text_(std::move(text)) {}
  bool right_justify_{true};
  const std::string text_;
};

// Label class that's used to display a label for an EditItem
class MultilineLabel final : public Label {
public:
  MultilineLabel(const std::string& text);

  void Display(CursesWindow* window) const override;
  [[nodiscard]] int height() const override;

private:
  std::vector<std::string> lines_;
};

// Base item of an editable value that uses templates to hold the
// value under edit.
template <typename T> class EditItem : public BaseEditItem {
public:
  typedef std::function<std::string()> displayfn;
  EditItem() = delete;
  EditItem(EditItem const&) = delete;
  EditItem(EditItem&&) = delete;
  EditItem& operator=(EditItem const&) = delete;
  EditItem& operator=(EditItem&&) = delete;

  EditItem(int maxsize, T data) : BaseEditItem(maxsize), data_(data) {}
  ~EditItem() override = default;

  void set_displayfn(displayfn f) { display_ = f; }
  void set_width(int width) noexcept override {
    if (width < width_) {
      // NOP.  We don't allow setting the width over the original width.
      width_ = width;
    }
  }

  void Display(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    if (display_) {
      const std::string blanks(width(), ' ');
      const auto custom = display_();
      window->PutsXY(x_, y_, blanks);
      window->PutsXY(x_, y_, custom);
    } else {
      DefaultDisplay(window);
    }
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const = 0;
  virtual void DefaultDisplayString(CursesWindow* window, const std::string& text) const {
    auto s = text;
    if (wwiv::stl::ssize(s) > width()) {
      s = text.substr(0, width());
    } else if (wwiv::stl::ssize(s) < width()) {
      s = text + std::string(static_cast<std::string::size_type>(width()) - text.size(), ' ');
    }

    window->SetColor(SchemeId::WINDOW_DATA);
    window->PutsXY(this->x_, this->y_, s);
  }

  [[nodiscard]] T data() const noexcept { return data_; }
  void set_data(T data) noexcept { data_ = data; }

  T data_;
  displayfn display_;
};

template <typename T> class StringEditItem : public EditItem<T> {
public:
  StringEditItem(int maxsize, T data, EditLineMode edit_line_mode = EditLineMode::ALL)
    : EditItem<T>(maxsize, data), edit_line_mode_(edit_line_mode) {}

  ~StringEditItem() override = default;

  StringEditItem() = delete;
  StringEditItem(StringEditItem const&) = delete;
  StringEditItem(StringEditItem&&) = delete;
  StringEditItem& operator=(StringEditItem const&) = delete;
  StringEditItem& operator=(StringEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    // GCC wants this-> on data and width_.
    return editline(window, reinterpret_cast<char*>(this->data_), this->width_, edit_line_mode_,
                    "");
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    this->DefaultDisplayString(
        window, std::string(reinterpret_cast<char*>(const_cast<const T>(this->data_))));
  }

private:
  EditLineMode edit_line_mode_;
};

template <> class StringEditItem<std::string&> final : public EditItem<std::string&> {
public:
  StringEditItem(int maxsize, std::string& data, EditLineMode mode)
      : EditItem<std::string&>(maxsize, data), edit_line_mode_(mode) {}
  ~StringEditItem() override = default;
  StringEditItem() = delete;
  StringEditItem(StringEditItem const&) = delete;
  StringEditItem(StringEditItem&&) = delete;
  StringEditItem& operator=(StringEditItem const&) = delete;
  StringEditItem& operator=(StringEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    return editline(window, &this->data_, this->width_, edit_line_mode_, "");
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    this->DefaultDisplayString(window, data_);
  }

private:
  EditLineMode edit_line_mode_;
};

// String Editor for Strings with Pipe codes.  The UI will attempt to display
// These with color in the UI when not editing.
class StringEditItemWithPipeCodes final : public EditItem<std::string&> {
public:
  StringEditItemWithPipeCodes(int maxsize, std::string& data, EditLineMode mode)
      : EditItem<std::string&>(maxsize, data), edit_line_mode_(mode) {}
  ~StringEditItemWithPipeCodes() override = default;
  StringEditItemWithPipeCodes() = delete;
  StringEditItemWithPipeCodes(StringEditItemWithPipeCodes const&) = delete;
  StringEditItemWithPipeCodes(StringEditItemWithPipeCodes&&) = delete;
  StringEditItemWithPipeCodes& operator=(StringEditItemWithPipeCodes const&) = delete;
  StringEditItemWithPipeCodes& operator=(StringEditItemWithPipeCodes&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    curses_out->footer()->ShowHelpItems(
        0, {{"Esc", "Exit"}, {"NOTE", "Pipe codes allowed."}});
    window->GotoXY(this->x_, this->y_);
    return editline(window, &this->data_, this->width_, edit_line_mode_, "");
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    auto s = data_;
    if (const auto siz = wwiv::strings::size_without_colors(s); siz > width()) {
      s = strings::trim_to_size_ignore_colors(s, width());
    } else if (siz < width()) {
      const auto num_padding = width() - siz;
      s = s + std::string(num_padding, ' ');
    }

    window->PutsWithPipeColors(x_, y_, s);
  }

private:
  EditLineMode edit_line_mode_;
};

class ACSEditItem final : public EditItem<std::string&> {
public:
  ACSEditItem(const sdk::Config&, std::vector<const sdk::value::ValueProvider*> p,
              int maxsize, std::string& data)
      : EditItem<std::string&>(maxsize, data), providers_(std::move(p)),
        edit_line_mode_(EditLineMode::ALL) {}
  ~ACSEditItem() override = default;
  ACSEditItem() = delete;
  ACSEditItem(ACSEditItem const&) = delete;
  ACSEditItem(ACSEditItem&&) = delete;
  ACSEditItem& operator=(ACSEditItem const&) = delete;
  ACSEditItem& operator=(ACSEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override;

protected:
  void DefaultDisplay(CursesWindow* window) const override;

private:
  std::vector<const sdk::value::ValueProvider*> providers_;
  EditLineMode edit_line_mode_;
};


class FidoAddressStringEditItem : public EditItem<std::string&> {
public:
  FidoAddressStringEditItem(int maxsize, std::string& data)
      : EditItem<std::string&>(maxsize, data), edit_line_mode_(EditLineMode::LOWER) {}
  ~FidoAddressStringEditItem() override = default;
  FidoAddressStringEditItem() = delete;
  FidoAddressStringEditItem(FidoAddressStringEditItem const&) = delete;
  FidoAddressStringEditItem(FidoAddressStringEditItem&&) = delete;
  FidoAddressStringEditItem& operator=(FidoAddressStringEditItem const&) = delete;
  FidoAddressStringEditItem& operator=(FidoAddressStringEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override;

protected:
  void DefaultDisplay(CursesWindow* window) const override;

private:
  EditLineMode edit_line_mode_;
};


template <typename T, int MAXLEN = std::numeric_limits<T>::digits10>
class NumberEditItem final : public EditItem<T*> {
public:
  explicit NumberEditItem(T* data) : EditItem<T*>(MAXLEN + 2, data) {}
  ~NumberEditItem() override = default;
  NumberEditItem() = delete;
  NumberEditItem(NumberEditItem const&) = delete;
  NumberEditItem(NumberEditItem&&) = delete;
  NumberEditItem& operator=(NumberEditItem const&) = delete;
  NumberEditItem& operator=(NumberEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto s = std::to_string(*this->data_);
    auto return_code = editline(window, &s, MAXLEN + 1, EditLineMode::NUM_ONLY, "");
    *this->data_ = wwiv::strings::to_number<T>(s);
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    const auto d = std::to_string(*this->data_);
    window->PutsXY(this->x_, this->y_, fmt::format("{:<{}}", d, this->width()));
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
int index_of(T& haystack, const std::vector<std::pair<T, std::string>>& needle) {
  for (auto i = 0; i < wwiv::stl::ssize(needle); i++) {
    if (haystack == needle.at(i).first) {
      return i;
    }
  }
  return 0;
}

template <typename T> class ToggleEditItem final : public EditItem<T*> {
public:
  ToggleEditItem(const std::vector<std::pair<T, std::string>>& items, T* data)
      : EditItem<T*>(0, data), items_(items) {
    for (const auto& item : items) {
      this->width_ = std::max<int>(this->width_, wwiv::strings::ssize(item.second));
    }
  }

  ~ToggleEditItem() override = default;
  ToggleEditItem() = delete;
  ToggleEditItem(ToggleEditItem const&) = delete;
  ToggleEditItem(ToggleEditItem&&) = delete;
  ToggleEditItem& operator=(ToggleEditItem const&) = delete;
  ToggleEditItem& operator=(ToggleEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    curses_out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});

    window->GotoXY(this->x_, this->y_);
    auto return_code = EditlineResult::NEXT;
    auto index = index_of(*this->data_, items_);
    const auto items = item_list(items_);
    index = static_cast<int>(toggleitem(window, index, items, &return_code));
    *this->data_ = static_cast<T>(items_.at(index).first);
    DefaultDisplay(window);
    curses_out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
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
  return static_cast<int>(m);
}

class StringListItem final : public EditItem<std::string&> {
public:
  StringListItem(const std::vector<std::string>& items, std::string& data)
      : EditItem<std::string&>(maxlen_from_list(items), data), items_(items) {}
  ~StringListItem() override = default;
  StringListItem() = delete;
  StringListItem(StringListItem const&) = delete;
  StringListItem(StringListItem&&) = delete;
  StringListItem& operator=(StringListItem const&) = delete;
  StringListItem& operator=(StringListItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    curses_out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    window->GotoXY(this->x_, this->y_);
    auto return_code = EditlineResult::NEXT;
    const auto it = std::find(items_.begin(), items_.end(), data_);
    std::vector<std::string>::size_type selection = 0;
    if (it != items_.end()) {
      selection = std::distance(items_.begin(), it);
    }
    selection = toggleitem(window, static_cast<std::vector<std::string>::size_type>(selection),
                           items_, &return_code);
    data_ = items_.at(selection);
    curses_out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override { DefaultDisplayString(window, data_); }

private:
  const std::vector<std::string> items_;
};

template <typename T> class FlagEditItem final : public EditItem<T*> {
public:
  FlagEditItem(int flag, const std::string& on, const std::string& off, T* data)
      : EditItem<T*>(0, data), flag_(flag) {
    this->width_ = std::max<int>(wwiv::stl::size_int(on), wwiv::stl::size_int(off));
    this->items_.push_back(off);
    this->items_.push_back(on);
  }

  ~FlagEditItem() override = default;
  FlagEditItem() = delete;
  FlagEditItem(FlagEditItem const&) = delete;
  FlagEditItem(FlagEditItem&&) = delete;
  FlagEditItem& operator=(FlagEditItem const&) = delete;
  FlagEditItem& operator=(FlagEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    curses_out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    auto return_code = EditlineResult::NEXT;
    std::vector<std::string>::size_type state = (*this->data_ & this->flag_) ? 1 : 0;
    state = toggleitem(window, state, this->items_, &return_code);
    if (state == 0) {
      *this->data_ &= ~this->flag_;
    } else {
      *this->data_ |= this->flag_;
    }
    curses_out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    const int state = (*this->data_ & this->flag_) ? 1 : 0;
    this->DefaultDisplayString(window, items_.at(state));
  }

private:
  std::vector<std::string> items_;
  int flag_;
};

class BaseRestrictionsEditItem : public EditItem<uint16_t*> {
public:
  BaseRestrictionsEditItem(const char* rs, int rs_size, uint16_t* data)
      : EditItem<uint16_t*>(rs_size, data), rs_(rs), rs_size_(rs_size) {}

  ~BaseRestrictionsEditItem() override = default;
  BaseRestrictionsEditItem() = delete;
  BaseRestrictionsEditItem(BaseRestrictionsEditItem const&) = delete;
  BaseRestrictionsEditItem(BaseRestrictionsEditItem&&) = delete;
  BaseRestrictionsEditItem& operator=(BaseRestrictionsEditItem const&) = delete;
  BaseRestrictionsEditItem& operator=(BaseRestrictionsEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {

    window->GotoXY(this->x_, this->y_);
    char s[21];
    char rs[21];
    char ch1 = '0';

    wwiv::strings::to_char_array(rs, rs_);
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

    const auto return_code = editline(window, s, 16, EditLineMode::SET, rs);

    *this->data_ = 0;
    for (auto i = 0; i < rs_size_; i++) {
      if (s[i] != 32 && s[i] != 0) {
        *this->data_ |= (1 << i);
      }
    }

    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    const auto s = to_restriction_string(*data_, rs_size_, rs_);
    DefaultDisplayString(window, s);
  }
  std::string rs_;
  int rs_size_{0};
};

static const char* ar_string = "ABCDEFGHIJKLMNOP";
static constexpr int ar_string_size = 16;
static const char* xrestrictstring = "LCMA*PEVKNU     ";
static constexpr int xrestrictstring_size = 16;

class RestrictionsEditItem final : public BaseRestrictionsEditItem {
public:
  explicit RestrictionsEditItem(uint16_t* data)
      : BaseRestrictionsEditItem(xrestrictstring, xrestrictstring_size, data) {}
  ~RestrictionsEditItem() override = default;
  RestrictionsEditItem() = delete;
  RestrictionsEditItem(RestrictionsEditItem const&) = delete;
  RestrictionsEditItem(RestrictionsEditItem&&) = delete;
  RestrictionsEditItem& operator=(RestrictionsEditItem const&) = delete;
  RestrictionsEditItem& operator=(RestrictionsEditItem&&) = delete;
};

class ArEditItem final : public BaseRestrictionsEditItem {
public:
  explicit ArEditItem(uint16_t* data)
      : BaseRestrictionsEditItem(ar_string, ar_string_size, data) {}
  ~ArEditItem() override = default;
  ArEditItem() = delete;
  ArEditItem(ArEditItem const&) = delete;
  ArEditItem(ArEditItem&&) = delete;
  ArEditItem& operator=(ArEditItem const&) = delete;
  ArEditItem& operator=(ArEditItem&&) = delete;
};

class BooleanEditItem final : public EditItem<bool*> {
public:
  explicit BooleanEditItem(bool* data) : EditItem<bool*>(6, data) {}
  ~BooleanEditItem() override = default;
  BooleanEditItem() = delete;
  BooleanEditItem(BooleanEditItem const&) = delete;
  BooleanEditItem(BooleanEditItem&&) = delete;
  BooleanEditItem& operator=(BooleanEditItem const&) = delete;
  BooleanEditItem& operator=(BooleanEditItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    curses_out->footer()->ShowHelpItems(0, {{"Esc", "Exit"}, {"SPACE", "Toggle Item"}});
    static const std::vector<std::string> boolean_strings{"No ", "Yes"};

    window->GotoXY(this->x_, this->y_);
    std::vector<std::string>::size_type data = *this->data_ ? 1 : 0;
    auto return_code = EditlineResult::NEXT;
    data = toggleitem(window, data, boolean_strings, &return_code);

    *this->data_ = (data > 0) ? true : false;
    curses_out->footer()->SetDefaultFooter();
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    static const std::vector<std::string> boolean_strings = {"No ", "Yes"};
    const auto& s = boolean_strings.at(*data_ ? 1 : 0);
    DefaultDisplayString(window, s);
  }
};

class CustomEditItem final : public BaseEditItem {
public:
  typedef std::function<void(const std::string&)> displayfn;
  typedef std::function<std::string()> prefn;
  typedef std::function<void(const std::string&)> postfn;
  CustomEditItem(int maxsize, prefn to_field, postfn from_field)
      : BaseEditItem(maxsize), to_field_(std::move(to_field)),
        from_field_(std::move(from_field)), field_width_(maxsize) {}
  CustomEditItem() = delete;
  CustomEditItem(CustomEditItem const&) = delete;
  CustomEditItem(CustomEditItem&&) = delete;
  CustomEditItem& operator=(CustomEditItem const&) = delete;
  CustomEditItem& operator=(CustomEditItem&&) = delete;
  ~CustomEditItem() override = default;

  EditlineResult Run(CursesWindow* window) override;
  void Display(CursesWindow* window) const override;
  void set_displayfn(displayfn f) { display_ = f; }

private:
  prefn to_field_;
  postfn from_field_;
  displayfn display_;
  int field_width_{1};
};

class FilePathItem final : public EditItem<char*> {
public:
  FilePathItem(int maxsize, const std::string& base, char* data)
      : EditItem<char*>(maxsize, data), base_(base) {
    help_text_ = strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  FilePathItem() = delete;
  FilePathItem(FilePathItem&) = delete;
  FilePathItem(FilePathItem&&) = delete;
  FilePathItem& operator=(const FilePathItem&) = delete;
  FilePathItem& operator=(FilePathItem&&) = delete;
  ~FilePathItem() override = default;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    const auto p = core::File::FixPathSeparators(this->data_);
    strcpy(this->data_, p.c_str());
    const auto return_code =
        editline(window, this->data_, this->width_, EDITLINE_FILENAME_CASE, "");
    trimstrpath(this->data_);

    // Update what we display in case it changed.
    DefaultDisplay(window);

    const auto dir = core::File::absolute(this->base_, this->data_);
    if (!core::File::Exists(dir)) {
      const auto s1 = wwiv::strings::StrCat("The path '", this->data_, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!core::File::mkdirs(dir)) {
          messagebox(window, {"Unable to create directory: ", dir.string()});
        }
      }
    }
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    DefaultDisplayString(window, data_);
  }

private:
  const std::string base_;
};

class FileSystemFilePathItem final : public EditItem<std::filesystem::path&> {
public:
  FileSystemFilePathItem(int maxsize, std::filesystem::path base,
                         std::filesystem::path& data)
      : EditItem<std::filesystem::path&>(maxsize, data), base_(std::move(base)) {
    help_text_ =
        strings::StrCat("Enter an absolute path or path relative to: '", base_.string(), "'");
  }
  ~FileSystemFilePathItem() override = default;
  FileSystemFilePathItem() = delete;
  FileSystemFilePathItem(FileSystemFilePathItem const&) = delete;
  FileSystemFilePathItem(FileSystemFilePathItem&&) = delete;
  FileSystemFilePathItem& operator=(FileSystemFilePathItem const&) = delete;
  FileSystemFilePathItem& operator=(FileSystemFilePathItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    auto data = this->data_.string();
    const auto return_code = editline(window, &data, this->width_, EDITLINE_FILENAME_CASE, "");
    strings::StringTrimEnd(&data);
    if (!data.empty()) {
      data = core::File::EnsureTrailingSlash(data);
    }
    // Update what we display in case it changed.
    DefaultDisplay(window);

    const auto dir = wwiv::core::File::absolute(this->base_, data);
    if (!core::File::Exists(dir)) {
      const auto s1 = wwiv::strings::StrCat("The path '", dir, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!core::File::mkdirs(dir)) {
          messagebox(window, {"Unable to create directory: ", dir.string()});
        }
      }
    }
    // Set the path in the base class
    data_ = data;
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    DefaultDisplayString(window, data_.string());
  }

private:
  const std::filesystem::path base_;
};

class StringFilePathItem final : public EditItem<std::string&> {
public:
  StringFilePathItem(int maxsize, const std::filesystem::path& base,
                     std::string& data)
      : EditItem<std::string&>(maxsize, data), base_(base) {
    help_text_ = strings::StrCat("Enter an absolute path or path relative to: '", base, "'");
  }
  ~StringFilePathItem() override = default;

  StringFilePathItem() = delete;
  StringFilePathItem(StringFilePathItem const&) = delete;
  StringFilePathItem(StringFilePathItem&&) = delete;
  StringFilePathItem& operator=(StringFilePathItem const&) = delete;
  StringFilePathItem& operator=(StringFilePathItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    const auto return_code =
        editline(window, &this->data_, this->width_, EDITLINE_FILENAME_CASE, "");
    strings::StringTrimEnd(&this->data_);
    if (!data_.empty()) {
      data_ = core::File::EnsureTrailingSlash(data_);
    }
    // Update what we display in case it changed.
    DefaultDisplay(window);

    const auto dir = wwiv::core::File::absolute(this->base_, this->data_);
    if (!wwiv::core::File::Exists(dir)) {
      const auto s1 = wwiv::strings::StrCat("The path '", dir, "' does not exist.");
      if (dialog_yn(window, {s1, "Would you like to create it?"})) {
        if (!wwiv::core::File::mkdirs(dir)) {
          messagebox(window, {"Unable to create directory: ", dir.string()});
        }
      }
    }
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    DefaultDisplayString(window, data_);
  }

private:
  const std::filesystem::path base_;
};

class CommandLineItem final : public EditItem<char*> {
public:
  CommandLineItem(int maxsize, char* data) : EditItem<char*>(maxsize, data) {}
  ~CommandLineItem() override = default;
  CommandLineItem() = delete;
  CommandLineItem(CommandLineItem const&) = delete;
  CommandLineItem(CommandLineItem&&) = delete;
  CommandLineItem& operator=(CommandLineItem const&) = delete;
  CommandLineItem& operator=(CommandLineItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    const auto return_code =
        editline(window, this->data_, this->width_, EDITLINE_FILENAME_CASE, "");
    strings::StringTrimEnd(this->data_);
    return return_code;
  }

protected:
  void DefaultDisplay(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    DefaultDisplayString(window, data_);
  }
};


/**
 * EditItem that executes a std::function<T, CursesWindow*> to
 * edit the items. It is intended that this function will invoke
 * a new EditItem dialog or ListBox for editing.
 */
template <class T> class SubDialog : public BaseEditItem {
public:
  SubDialog(const wwiv::sdk::Config& c, T& t)
      : BaseEditItem(25), c_(c), t_(t) {}
  ~SubDialog() override = default;
  SubDialog() = delete;
  SubDialog(const SubDialog&) = delete;
  SubDialog(SubDialog&&) = delete;
  SubDialog& operator=(const SubDialog&) = delete;
  SubDialog& operator=(SubDialog&&) = delete;

  virtual void RunSubDialog(CursesWindow* window) = 0;

  EditlineResult Run(CursesWindow* window) override {
    core::ScopeExit at_exit([] { curses_out->footer()->SetDefaultFooter(); });
    curses_out->footer()->ShowHelpItems(
        0, {{"Esc", "Exit"}, {"ENTER", "Edit Items (opens new dialog)."}});
    window->GotoXY(x_, y_);
    uint32_t old_attr;
    short old_pair;
    window->AttrGet(&old_attr, &old_pair);
    window->SetColor(SchemeId::EDITLINE);
    // Don't use Display since we use that when we don't have focus here.
    window->PutsXY(x_, y_, menu_label());
    window->GotoXY(x_, y_);
    const auto ch = window->GetChar();
    window->AttrSet(COLOR_PAIR(old_pair) | old_attr);
    if (ch == KEY_ENTER || ch == io::TAB || ch == io::ENTER) {
      RunSubDialog(window);
      window->RedrawWin();
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return EditlineResult::PREV;
    } else if (ch == io::ESC) {
      return EditlineResult::DONE;
    }
    return EditlineResult::NEXT;
  }

  void Display(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    window->PutsXY(x_, y_, menu_label());
  }

protected:
  [[nodiscard]] const wwiv::sdk::Config& config() const noexcept { return c_; }
  [[nodiscard]] virtual std::string menu_label() const { return "[Press Enter to Edit]"; }

  const sdk::Config& c_;
  T& t_;
};

/**
 * EditItem that executes a std::function<T, CursesWindow*> to
 * edit the items. It is intended that this function will invoke
 * a new EditItem dialog or ListBox for editing.
 */
template <class T> class SubDialogFunction final : public SubDialog<T> {
public:
  SubDialogFunction() = delete;
  SubDialogFunction(const SubDialogFunction&) = delete;
  SubDialogFunction(SubDialogFunction&&) = delete;
  SubDialogFunction& operator=(const SubDialogFunction&) = delete;
  SubDialogFunction& operator=(SubDialogFunction&&) = delete;
  SubDialogFunction(const sdk::Config& c, T& t,
                    std::function<void(const sdk::Config&, T&, CursesWindow*)> fn)
      : SubDialog<T>(c, t), c_(c), typ_(t), fn_(std::move(fn)) {}
  ~SubDialogFunction() override = default;

  void RunSubDialog(CursesWindow* window) override { fn_(c_, typ_, window); }

private:
  // For some reason GCC couldn't find config() or t_ from SubDialog.
  const sdk::Config& c_;
  T& typ_;
  std::function<void(const wwiv::sdk::Config&, T&, CursesWindow*)> fn_;
};

/**
 * EditItem that executes a std::function<T, CursesWindow*> to
 * edit the items. It is intended that this function will invoke
 * a new EditItem dialog or ListBox for editing.
 */
class EditExternalFileItem final : public BaseEditItem {
public:
  explicit EditExternalFileItem(std::filesystem::path path);
  ~EditExternalFileItem() override = default;
  EditExternalFileItem() = delete;
  EditExternalFileItem(EditExternalFileItem const&) = delete;
  EditExternalFileItem(EditExternalFileItem&&) = delete;
  EditExternalFileItem& operator=(EditExternalFileItem const&) = delete;
  EditExternalFileItem& operator=(EditExternalFileItem&&) = delete;

  EditlineResult Run(CursesWindow* window) override {
    core::ScopeExit at_exit([] { curses_out->footer()->SetDefaultFooter(); });
    curses_out->footer()->ShowHelpItems(
        0, {{"Esc", "Exit"}, {"ENTER", "Edit File (opens editor)"}});
    window->GotoXY(x_, y_);
    uint32_t old_attr;
    short old_pair;
    window->AttrGet(&old_attr, &old_pair);
    window->SetColor(SchemeId::EDITLINE);
    window->PutsXY(x_, y_, menu_label());
    window->GotoXY(x_, y_);
    const auto ch = window->GetChar();
    window->AttrSet(COLOR_PAIR(old_pair) | old_attr);
    if (ch == KEY_ENTER || ch == io::TAB || ch == io::ENTER) {
      curses_out->DisableLocalIO();
      std::string exe;
#ifdef _WIN32
      exe = ".exe";
#endif
      const auto cmd = fmt::format(".{}wwivfsed{} -F {}", wwiv::core::File::pathSeparatorChar, exe, path_.string());
      system(cmd.c_str());
      curses_out->ReenableLocalIO();
      window->RedrawWin();
    } else if (ch == KEY_UP || ch == KEY_BTAB) {
      return EditlineResult::PREV;
    } else if (ch == io::ESC) {
      return EditlineResult::DONE;
    }
    return EditlineResult::NEXT;
  }

  void Display(CursesWindow* window) const override {
    window->SetColor(SchemeId::WINDOW_DATA);
    window->PutsXY(x_, y_, menu_label());
  }

protected:
  [[nodiscard]] std::string menu_label() const noexcept { return "[Edit]"; }

  const std::filesystem::path path_;
};

}

#endif
