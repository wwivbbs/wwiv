/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "core/strings.h"
#include "initlib/colors.h"

using std::string;
using wwiv::strings::StringPrintf;

ListBox::ListBox(CursesWindow* parent, const string& title, int max_x, int max_y, 
                 std::vector<ListBoxItem>& items, ColorScheme* scheme) 
    : title_(title), selected_(-1), items_(items), window_top_(0), width_(4), 
      height_(2), color_scheme_(scheme), 
      window_top_min_(title.empty() ? 1 : 1 /* 3 */) {
  height_ = std::min<int>(items.size(), max_y);
  int window_height = 2 + height_ + window_top_min_ - 1;
  int longest_line = std::max<int>(2, title.size() + 4);
  for (const auto& item : items) {
    longest_line = std::max<int>(longest_line, item.text().size());
    if (item.hotkey() > 0) {
      hotkeys_.push_back(toupper(item.hotkey()));
    }
  }
  width_ = std::min<int>(max_x, longest_line);
  int window_width = 4 + width_;
  int maxx = parent->GetMaxX();
  int maxy = parent->GetMaxY();
  int begin_x = ((maxx - window_width) / 2);
  int begin_y = ((maxy - window_height) / 2);

  window_.reset(new CursesWindow(parent, parent->color_scheme(), window_height, window_width, begin_y, begin_x));
  window_->SetColor(SchemeId::WINDOW_BOX);
  window_->Box(0, 0);
  window_->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::WINDOW_TEXT));
  if (!title.empty()) {
    window_->SetTitle(title);
  }
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
    window_->MvAddStr(y + window_top_min_, 1, line.c_str());
  }
}

ListBoxResult ListBox::RunDialog() {
  window_top_ = window_top_min_;
  selected_ = 0;

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
        return ListBoxResult{ ListBoxResultType::HOTKEY, 0, ch};
      }
    }
  }
}
