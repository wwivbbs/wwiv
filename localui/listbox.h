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
#ifndef INCLUDED_LOCALUI_LISTBOX_H
#define INCLUDED_LOCALUI_LISTBOX_H

#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::local::ui {

class ColorScheme;

class ListBoxItem {
public:
  // Need non-explicit constructor to use in initialize_lists
  ListBoxItem(std::string text, int hotkey = 0, int data = 0)
      : text_(std::move(text)), hotkey_(hotkey), data_(data) {}
  ~ListBoxItem() = default;

  [[nodiscard]] const std::string& text() const noexcept { return text_; }
  [[nodiscard]] int hotkey() const noexcept { return hotkey_; }
  [[nodiscard]] int data() const noexcept { return data_; }
  [[nodiscard]] int index() const noexcept { return index_; }
  void index(int i) { index_ = i; }

 private:
  std::string text_;
  int hotkey_;
  int data_;
  int index_{-1};
};

enum class ListBoxResultType { NO_SELECTION, SELECTION, HOTKEY };
struct ListBoxResult {
  ListBoxResultType type;
  int selected;
  int hotkey;
  int data{0};
};

// Curses implementation of a list box.
class ListBox final {
public:
  // Constructor/Destructor
  ListBox(UIWindow* parent, const std::string& title, std::vector<ListBoxItem>& items);
  ListBox(const ListBox& copy) = delete;
  ~ListBox() = default;

  // Execute the listbox returning the index of the selected item.
  ListBoxResult Run() {
    this->DisplayFooter();
    const auto result = RunDialog();
    curses_out->footer()->SetDefaultFooter();
    parent_->RedrawWin();
    return result;
  }

  // Returns the index of the selected item.
  [[nodiscard]] int selected() const noexcept { return selected_; }
  void set_selected(int s) noexcept { selected_ = s; }

  // List of additionally allowed hotkeys.
  void set_additional_hotkeys(const std::string& hotkeys) { hotkeys_.append(hotkeys); }
  // If true, a selection will return as a hotkey if a hotkey is set on the item.
  void selection_returns_hotkey(bool selection_returns_hotkey) noexcept {
    selection_returns_hotkey_ = selection_returns_hotkey;
  }
  // Sets the extra help items.
  void set_help_items(const std::vector<HelpItem>& items) { help_items_ = items; }

private:
  ListBox(UIWindow* parent, const std::string& title, int max_x, int max_y,
          std::vector<ListBoxItem>& items, ColorScheme* scheme);
  ListBoxResult RunDialogInner();
  ListBoxResult RunDialog();
  void DrawAllItems(std::vector<ListBoxItem>& items);
  void DisplayFooter();

  int selected_ = -1;
  const std::string title_;
  std::vector<ListBoxItem> items_;
  std::vector<HelpItem> help_items_;
  int window_top_ = 0;
  int width_ = 4;
  int height_ = 2;
  std::unique_ptr<CursesWindow> window_;
  ColorScheme* color_scheme_;
  const int window_top_min_;
  std::string hotkeys_;
  bool selection_returns_hotkey_{false};
  UIWindow* parent_{nullptr};
};

}

#endif
