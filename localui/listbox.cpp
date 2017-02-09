/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "listbox.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "localui/colors.h"
#include "localui/wwiv_curses.h"

using std::string;
using namespace wwiv::stl;
using namespace wwiv::strings;

static std::vector<HelpItem> StandardHelpItems() {
  return { {"Esc", "Exit"} };
}

ListBox::ListBox(CursesIO* io, UIWindow* parent, const string& title, int max_x, int max_y, 
                 std::vector<ListBoxItem>& items, ColorScheme* scheme) 
    : io_(io), title_(title), items_(items), color_scheme_(scheme), 
      window_top_min_(title.empty() ? 1 : 1 /* 3 */) {
  height_ = std::min<int>(items.size(), max_y);
  int window_height = 2 + height_ + window_top_min_ - 1;
  int longest_line = std::max<int>(2, title.size() + 4);
  for (const auto& item : items) {
    longest_line = std::max<int>(longest_line, item.text().size());
    if (item.hotkey() > 0) {
      hotkeys_.push_back(to_upper_case<char>(item.hotkey()));
    }
  }
  width_ = std::min<int>(max_x, longest_line);
  int window_width = 4 + width_;
  int maxx = parent->GetMaxX();
  int maxy = parent->GetMaxY();
  int begin_x = ((maxx - window_width) / 2);
  int begin_y = ((maxy - window_height) / 2);

  CHECK(parent->IsGUI()) << "ListBox constructor needs a GUI.";

  window_.reset(new CursesWindow(static_cast<CursesWindow*>(parent), 
    parent->color_scheme(), window_height, window_width, begin_y, begin_x));
  window_->SetColor(SchemeId::WINDOW_BOX);
  window_->Box(0, 0);
  window_->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::WINDOW_TEXT));
  if (!title.empty()) {
    window_->SetTitle(title);
  }
  help_items_ = StandardHelpItems();
}

void ListBox::DrawAllItems() {
  for (int y = 0; y < height_; y++) {
    int current_item = window_top_ + y - window_top_min_;
    string line(items_[current_item].text());
    if (static_cast<int>(line.size()) > width_) {
      line = line.substr(0, width_);
    } else {
      for (int i=line.size(); i < width_; i++) {
        line.push_back(' ');
      }
    }
    if (selected_ == current_item) {
      window_->SetColor(SchemeId::DIALOG_SELECTION);
    } else {
      window_->SetColor(SchemeId::WINDOW_TEXT);
    }
    line.insert(line.begin(), 1, ' ');
    line.push_back(' ');
    window_->PutsXY(1, y + window_top_min_, line.c_str());
  }
}

ListBoxResult ListBox::RunDialog() {
  if (selected_ < 0) {
    selected_ = 0;
  } else if (selected_ >= size_int(items_)) {
    selected_ = items_.size();
  }
  // Move window top to furthest possible spot.
  auto temp_top = std::max<int>(window_top_min_, selected_);
  // Pull it back to the top of the window.
  window_top_ = std::min<int>(temp_top, items_.size() - height_ + window_top_min_);
  selected_ = std::min<int>(selected_, items_.size() - 1);

  while (true) {
    DrawAllItems();
    window_->Move(selected_ - (window_top_ - window_top_min_) + window_top_min_, 1);
    window_->Refresh();
    int ch = window_->GetChar();
    switch (ch) {
    case KEY_HOME:
      selected_ = 0;
      window_top_ = window_top_min_;
      break;
    case KEY_END:
      window_top_ = items_.size() - height_ + window_top_min_;
      selected_ = window_top_ - window_top_min_;
      break;
    case KEY_PREVIOUS:  // What is this key?
    case KEY_UP:
      if (selected_ + window_top_min_ == window_top_) {
        // At the top of the window, move the window up one.
        if (window_top_ > window_top_min_) {
          window_top_--;
          selected_--;
        }
      } else {
        selected_--;
      }
      break;
    case KEY_PPAGE: {
      window_top_ -= height_;
      selected_ -= height_;
      window_top_ = std::max<int>(window_top_, window_top_min_);
      selected_ = std::max<int>(selected_, 0);
    } break;
    case KEY_NEXT: // What is this key?
    case KEY_DOWN: {
      int window_bottom = window_top_ + height_ - window_top_min_ - 1;
      if (selected_ < window_bottom) {
        selected_++;
      } else if (window_top_ < static_cast<int>(items_.size()) - height_ + window_top_min_) {
        selected_++;
        window_top_++;
      }
    } break;
    case KEY_NPAGE: {
      window_top_ += height_;
      selected_ += height_;
      window_top_ = std::min<int>(window_top_, items_.size() - height_ + window_top_min_);
      selected_ = std::min<int>(selected_, items_.size() - 1);
    } break;
#ifdef PADENTER
    case PADENTER:
#endif
    case KEY_ENTER:
    case 13: {
      if (items_.empty()) {
        // Can not select an item when the list is empty.
        break;
      }
      int hotkey = items_.at(selected_).hotkey();
      if (selection_returns_hotkey_ && hotkey > 0) {
        return ListBoxResult{ ListBoxResultType::HOTKEY, selected_, hotkey};
      }
      return ListBoxResult{ ListBoxResultType::SELECTION, selected_, hotkey};
    } break;
    case 27:  // ESCAPE_KEY
      return ListBoxResult{ ListBoxResultType::NO_SELECTION, 0, 0};
    default:
      ch = toupper(ch);
      if (hotkeys_.find(ch) != string::npos) {
        // Since a hotkey was pressed, update selected_ to match the index
        // of the item containing the hotkey.
        for (size_t i = 0; i < items_.size(); i++) {
          if (items_.at(i).hotkey() == ch) {
            selected_ = i;
            break;
          }
        }
        return ListBoxResult{ ListBoxResultType::HOTKEY, selected_, ch};
      }
    }
  }
}

void ListBox::DisplayFooter() {
  // Show help bar.
  io_->footer()->window()->Move(1, 0);
  io_->footer()->window()->ClrtoEol();
  io_->footer()->ShowHelpItems(0, help_items_);
}