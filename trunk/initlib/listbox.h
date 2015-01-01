/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#ifndef __INCLUDED_PLATFORM_LISTBOX_H__
#define __INCLUDED_PLATFORM_LISTBOX_H__

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <curses.h>

#include "initlib/curses_io.h"
#include "initlib/curses_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

class ColorScheme;

struct ListBoxItem {
  ListBoxItem(const std::string& text, int hotkey=0, int data = 0) : text_(text), hotkey_(hotkey), data_(data) {}
  ~ListBoxItem() {}

  const std::string& text() const { return text_; }
  const int hotkey() const { return hotkey_; }
  const int data() const { return data_; }

  std::string text_;
  int hotkey_;
  int data_;
};

enum class ListBoxResultType { NO_SELECTION, SELECTION, HOTKEY };
struct ListBoxResult {
  ListBoxResultType type;
  int selected;
  int hotkey;
};

// Curses implementation of a list box.
class ListBox {
 public:
  // Constructor/Destructor
  ListBox(CursesIO* io, CursesWindow* parent, const std::string& title, int max_x, int max_y, 
          std::vector<ListBoxItem>& items, ColorScheme* scheme);
  ListBox(const ListBox& copy) = delete;
  virtual ~ListBox() {}

  // Execute the listbox returning the index of the selected item.
  ListBoxResult Run() {
    this->DisplayFooter();
    ListBoxResult result = RunDialog();
    io_->footer()->SetDefaultFooter();
    return result;
  }

  // Returns the index of the selected item.
  int selected() const { return selected_; }

  // List of additionally allowed hotkeys.
  void set_additional_hotkeys(const std::string& hotkeys) { hotkeys_.append(hotkeys); }
  // If true, a selection will return as a hotkey if a hotkey is set on the item.
  void selection_returns_hotkey(bool selection_returns_hotkey) { selection_returns_hotkey_ = selection_returns_hotkey; }
  // Sets the extra help items.
  void set_help_items(const std::vector<HelpItem> items) { help_items_ = items; }

 private:
  ListBoxResult RunDialog();
  void DrawAllItems();
  void DisplayFooter();

  CursesIO* io_;
  int selected_;
  const std::string title_;
  std::vector<ListBoxItem> items_;
  std::vector<HelpItem> help_items_;
  int window_top_;
  int width_;
  int height_;
  std::unique_ptr<CursesWindow> window_;
  ColorScheme* color_scheme_;
  const int window_top_min_;
  std::string hotkeys_;
  bool selection_returns_hotkey_;
};

#endif // __INCLUDED_PLATFORM_LISTBOX_H__
