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
#ifndef __INCLUDED_PLATFORM_LISTBOX_H__
#define __INCLUDED_PLATFORM_LISTBOX_H__

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <curses.h>
#include "curses_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

class ColorScheme;

struct ListBoxItem {
  ListBoxItem(const std::string& text, int hotkey=0, const void* data = nullptr) : text_(text), hotkey_(hotkey), data_(data) {}
  ~ListBoxItem() {}

  const std::string& text() const { return text_; }
  const int hotkey() const { return hotkey_; }
  const void* data() const { return data_; }

  std::string text_;
  int hotkey_;
  const void* data_;
};

// Curses implementation of a list box.
class ListBox {
 public:
  // Constructor/Destructor
  ListBox(CursesWindow* parent, const std::string& title, int max_x, int max_y, 
          std::vector<ListBoxItem>& items, ColorScheme* scheme);
  ListBox(const ListBox& copy) = delete;
  virtual ~ListBox() {}

  // Execute the listbox returning the index of the selected item.
  int Run() {
    if (!RunDialog()) {
      return -1;
    }
    return selected();
  }

  // Returns the index of the selected item.
  int selected() const { return selected_; }

  // If true the hotkey press will return true from Run.  If false the item is just selected
  // but the dialog remains open.
  bool hotkey_executes_item() const { return hotkey_executes_item_; }

  // If true the hotkey press will return true from Run.  If false the item is just selected
  // but the dialog remains open.
  void set_hotkey_executes_item(bool hotkey_executes_item) { hotkey_executes_item_ = hotkey_executes_item; }

 private:
  bool RunDialog();
  void DrawAllItems();
  int selected_;
  const std::string title_;
  std::vector<ListBoxItem> items_;
  int window_top_;
  int width_;
  int height_;
  std::unique_ptr<CursesWindow> window_;
  ColorScheme* color_scheme_;
  const int window_top_min_;
  bool hotkey_executes_item_;
};

#endif // __INCLUDED_PLATFORM_LISTBOX_H__
