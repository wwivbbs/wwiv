/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "localui/colors.h"
#include "localui/wwiv_curses.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using std::string;
using namespace wwiv::stl;
using namespace wwiv::strings;

static constexpr int MINIMUM_LISTBOX_HEIGHT = 10;
static constexpr double RATIO_LISTBOX_HEIGHT = 0.8;
static constexpr double RATIO_LISTBOX_WIDTH = 0.9;

namespace wwiv::localui {


static std::vector<HelpItem> StandardHelpItems() { return {{"Esc", "Exit"}}; }

ListBox::ListBox(UIWindow* parent, const string& title, int max_x, int max_y,
                 std::vector<ListBoxItem>& items, ColorScheme* scheme)
    : title_(title), items_(items), color_scheme_(scheme), window_top_min_(1), parent_(parent) {
  height_ = std::min<int>(size_int(items), max_y);
  const auto window_height = 2 + height_ + window_top_min_ - 1;
  auto longest_line = std::max<int>(2, size_int(title) + 4);
  for (const auto& item : items) {
    longest_line = std::max<int>(longest_line, size_int(item.text()));
    if (item.hotkey() > 0) {
      hotkeys_.push_back(static_cast<char>(item.hotkey()));
    }
  }
  width_ = std::min<int>(max_x, longest_line);
  const auto window_width = 4 + width_;
  const auto maxx = curses_out->window()->GetMaxX();
  const auto maxy = curses_out->window()->GetMaxY();
  const auto begin_x = (maxx - window_width) / 2;
  const auto begin_y = (maxy - window_height) / 2;

  CHECK(curses_out->window()->IsGUI()) << "ListBox needs a GUI.";

  window_.reset(new CursesWindow(curses_out->window(), parent->color_scheme(),
                                 window_height, window_width, begin_y, begin_x));
  window_->SetColor(SchemeId::WINDOW_BOX);
  window_->Box(0, 0);
  window_->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::WINDOW_TEXT));
  if (!title.empty()) {
    window_->SetTitle(title);
  }
  help_items_ = StandardHelpItems();
}

ListBox::ListBox(UIWindow* parent, const string& title,
                 std::vector<ListBoxItem>& items)
    : ListBox(parent, title,
              static_cast<int>(floor(curses_out->window()->GetMaxX() * RATIO_LISTBOX_WIDTH)),
              std::min<int>(
                  std::max<int>(size_int(items), MINIMUM_LISTBOX_HEIGHT),
                  static_cast<int>(floor(curses_out->window()->GetMaxY() * RATIO_LISTBOX_HEIGHT))),
              items, parent->color_scheme()) {}

void ListBox::DrawAllItems() {
  for (auto y = 0; y < height_; y++) {
    const auto current_item = window_top_ + y - window_top_min_;
    auto line{items_[current_item].text()};
    if (size_int(line) > width_) {
      line = line.substr(0, width_);
    } else {
      for (auto i = size_int(line); i < width_; i++) {
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
    window_->PutsXY(1, y + window_top_min_, line);
  }
}

ListBoxResult ListBox::RunDialog() {
  if (selected_ < 0) {
    selected_ = 0;
  } else if (selected_ >= size_int(items_)) {
    selected_ = size_int(items_);
  }
  // Move window top to furthest possible spot.
  const auto temp_top = std::max<int>(window_top_min_, selected_);
  // Pull it back to the top of the window.
  window_top_ = std::min<int>(temp_top, size_int(items_) - height_ + window_top_min_);
  selected_ = std::min<int>(selected_, size_int(items_) - 1);

  bool need_redraw{true};
  while (true) {
    if (need_redraw) {
      DrawAllItems();
      window_->Move(selected_ - (window_top_ - window_top_min_) + window_top_min_, 1);
      window_->Refresh();
    }
    need_redraw = true;
    auto ch = window_->GetChar();
    switch (ch) {
    case KEY_HOME:
      selected_ = 0;
      window_top_ = window_top_min_;
      break;
    case KEY_END:
      window_top_ = size_int(items_) - height_ + window_top_min_;
      selected_ = window_top_ - window_top_min_;
      break;
    case KEY_PREVIOUS: // What is this key?
    case KEY_UP:
      if (selected_ + window_top_min_ == window_top_) {
        // At the top of the window, move the window up one.
        if (window_top_ > window_top_min_) {
          window_top_--;
          selected_--;
        } else {
          // nothing happened.
          need_redraw = false;
        }
      } else {
        selected_--;
      }
      break;
    case KEY_PPAGE: {
      const auto old_window_top = window_top_;
      const auto old_selected = selected_;
      window_top_ -= height_;
      selected_ -= height_;
      window_top_ = std::max<int>(window_top_, window_top_min_);
      selected_ = std::max<int>(selected_, 0);

      if (old_window_top == window_top_ && old_selected == selected_) {
        need_redraw = false;
      }
    } break;
    case KEY_NEXT: // What is this key?
    case KEY_DOWN: {
      const auto window_bottom = window_top_ + height_ - window_top_min_ - 1;
      if (selected_ < window_bottom) {
        selected_++;
      } else if (window_top_ < size_int(items_) - height_ + window_top_min_) {
        selected_++;
        window_top_++;
      } else {
        need_redraw = false;
      }
    } break;
    case KEY_NPAGE: {
      const auto old_window_top = window_top_;
      const auto old_selected = selected_;

      window_top_ += height_;
      selected_ += height_;
      window_top_ = std::min<int>(window_top_, size_int(items_) - height_ + window_top_min_);
      selected_ = std::min<int>(selected_, size_int(items_) - 1);

      if (old_window_top == window_top_ && old_selected == selected_) {
        need_redraw = false;
      }
    } break;
    case KEY_ENTER:
    case 13: {
      if (items_.empty()) {
        // Can not select an item when the list is empty.
        break;
      }
      const auto hotkey = items_.at(selected_).hotkey();
      if (selection_returns_hotkey_ && hotkey > 0) {
        return ListBoxResult{ListBoxResultType::HOTKEY, selected_, hotkey};
      }
      return ListBoxResult{ListBoxResultType::SELECTION, selected_, hotkey};
    }
    case 27: // ESCAPE_KEY
      return ListBoxResult{ListBoxResultType::NO_SELECTION, 0, 0};
    default:
      ch = toupper(ch);
      if (hotkeys_.find(static_cast<char>(ch & 0xff)) != string::npos) {
        // Since a hotkey was pressed, update selected_ to match the index
        // of the item containing the hotkey.
        for (auto i = 0; i < wwiv::stl::ssize(items_); i++) {
          if (items_.at(i).hotkey() == ch) {
            selected_ = i;
            break;
          }
        }
        return ListBoxResult{ListBoxResultType::HOTKEY, selected_, ch};
      }
    }
  }
}

void ListBox::DisplayFooter() {
  // Show help bar.
  curses_out->footer()->window()->Move(1, 0);
  curses_out->footer()->window()->ClrtoEol(); 
  curses_out->footer()->ShowHelpItems(0, help_items_);
}

}
