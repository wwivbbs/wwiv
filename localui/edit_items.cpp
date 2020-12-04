/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "localui/edit_items.h"

#include "core/stl.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/stdio_win.h"
#include "localui/wwiv_curses.h"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

void EditItems::Run(const std::string& title) {
  if (!window_) {
    curses_out->Cls(ACS_CKBOARD);
    create_window(title);
  }
  edit_mode_ = true;
  auto cp = 0;
  const auto size = static_cast<int>(items_.size());
  Display();
  for (;;) {
    auto* item = items_[cp];
    curses_out->footer()->ShowContextHelp(item->help_text());
    const auto i1 = item->Run(window_.get());
    item->Display(window_.get());
    curses_out->footer()->SetDefaultFooter();
    if (i1 == EditlineResult::PREV) {
      if (--cp < 0) {
        cp = size - 1;
      }
    } else if (i1 == EditlineResult::NEXT) {
      if (++cp >= size) {
        cp = 0;
      }
    } else if (i1 == EditlineResult::DONE) {
      curses_out->SetIndicatorMode(IndicatorMode::NONE);
      edit_mode_ = false;
      Display();
      return;
    }
  }
}

void EditItems::Display() const {
  // Show help bar.
  if (edit_mode_) {
    curses_out->footer()->window()->Move(1, 0);
    curses_out->footer()->window()->ClrtoEol();
    curses_out->footer()->ShowHelpItems(0, editor_help_items_);
  } else {
    curses_out->footer()->ShowHelpItems(0, navigation_help_items_);
    curses_out->footer()->ShowHelpItems(1, navigation_extra_help_items_);
  }

  window_->SetColor(SchemeId::NORMAL);
  for (auto* l : labels_) {
    l->Display(window_.get());
  }
  window_->SetColor(SchemeId::WINDOW_DATA);
  for (auto* item : items_) {
    item->Display(window_.get());
  }
  window_->Refresh();
}

BaseEditItem* EditItems::add(BaseEditItem* item, int column, int y) {
  DCHECK(item);
  item->set_column(column);
  if (y >= 0) {
    item->set_y(y);
  }
  items_.push_back(item);
  return item;
}

Label* EditItems::add(Label* label, int column, int y) {
  DCHECK(label);
  label->set_column(column);
  if (y >= 0) {
    label->set_y(y);
  }
  labels_.push_back(label);
  return label;
}

void EditItems::add_labels(std::initializer_list<Label*> labels) {
  for (auto* l : labels) {
    DCHECK(l);
    labels_.push_back(l);
  }
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item, int column, int y) {
  add(label, column, y);
  return add(item, column, y);
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item, const std::string& help, int column, int y) {
  item->set_help_text(help);
  return add(label, item, column, y);
}

void EditItems::create_window(const std::string& title) {
  window_ = std::unique_ptr<CursesWindow>(
      curses_out->CreateBoxedWindow(title, max_display_height(), max_display_width()));
}

char EditItems::GetKeyWithNavigation(const NavigationKeyConfig& config) const {
  auto keys = config.keys_;
  keys.push_back(config.dn);
  keys.push_back(config.down10);
  keys.push_back(config.up);
  keys.push_back(config.up10);
  if (config.quit) {
    keys.push_back(config.quit);
    keys.push_back('\x1b');
  }
  for (;;) {
    const auto key = window()->GetChar();
    if (has_key(key)) {
      switch (key) {
      case KEY_PPAGE:
        return config.up10;
      case KEY_NPAGE:
        return config.down10;
      case KEY_LEFT:
      case KEY_UP:
        return config.up;
      case KEY_RIGHT:
      case KEY_DOWN:
        return config.dn;
      default:
        continue;
      }
    }
    const auto ch = static_cast<char>(std::toupper(key));
    if (keys.find(ch) != std::string::npos) {
      if (ch == '\x1b') {
        return config.quit;
      }
      return ch;
    }
  }
}

int EditItems::max_display_width() const {
  auto result = 0;
  for (const auto* i : items_) {
    if ((i->x() + i->width()) > result) {
      result = i->x() + i->width();
    }
  }
  return std::min<int>(curses_out->window()->GetMaxX(), result + 2); // 2 is padding
}

int EditItems::max_display_height() {
  int result = 1;
  for (const auto* l : labels_) {
    if (l->y() > result) {
      result = l->y();
    }
  }
  for (const auto* i : items_) {
    if (i->y() > result) {
      result = i->y();
    }
  }
  return std::min<int>(curses_out->window()->GetMaxY(), result + 2);
}

int EditItems::max_label_width(int column) const {
  std::string::size_type result = 0;
  for (const auto* l : labels_) {
    if (l->column() != column) {
      continue;
    }
    if (l->text().size() > result) {
      result = l->text().size();
    }
  }
  return static_cast<int>(result);
}

int EditItems::max_item_width(int column) const {
  auto result = 0;
  for (const auto* l : items_) {
    if (l->column() != column) {
      continue;
    }
    if (l->width() > result) {
      result = l->width();
    }
  }
  return result;
}

static constexpr auto PADDING = 2;

/**
 *  
 * TODO(rushfan): This needs a big change.
 *
 * 1) Needs to be able to layout a column and look at max widths for only
 *    a subset of rows, so you can have things like:
 *
 * LLLLL: FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
 * LLLLL: FFFFFFFF    LLLLL: FFFFFFFFFFFFFFFFFFFFF
 *
 * Notice they have different max widths depending on the row. I think we
 * need a two pass.
 * (1) Compute max width per row
 * (2) come up with the max over a span of rows and use that for laying out.
 *     We can look at column count per row, and use that as the thing we
 *     layout on. i.e layout all 1 col rows, then all 2 col rows.
 */
void EditItems::relayout_items_and_labels() {
  std::vector<int> prev_width{};
  for (auto i = 1; i <= num_columns_; i++) {
    // Figure out this columns width.
    auto& c = columns_.at(i);
    if (c.width_ == 0) {
      c.width_ = max_label_width(i) + 1 + max_item_width(i);
    }
    auto& prev = columns_.at(i - 1);
    c.x_ = prev.x_ + prev.width_ + PADDING;
  }

  // column numbers start at 1
  for (auto column = 1; column <= num_columns_; column++) {
    const auto&c = columns_.at(column);
    const auto label_width = max_label_width(column);
    for (auto* l : labels_) {
      if (l->column() == column) {
        l->set_x(c.x_);
        l->set_width(label_width);
      }
    }
    for (auto* i : items_) {
      if (i->column() == column) {
        i->set_x(c.x_ + 1 + label_width);
      }
    }
  }
}

std::vector<HelpItem> EditItems::StandardNavigationHelpItems() {
  return {
      {"Esc", "Exit"}, {"Enter", "Edit"}, {"[", "Previous"},
      {"]", "Next"}, {"{", "Previous 10"}, {"}", "Next 10"},
  };
}

EditItems::EditItems(int num_columns)
  : navigation_help_items_(StandardNavigationHelpItems()),
    editor_help_items_(StandardEditorHelpItems()), edit_mode_(false), num_columns_(num_columns) {
  Column zero(0);
  zero.x_ = 0;
  zero.width_ = 0;
  columns_.emplace_back(zero);
  for (auto i = 1; i <= num_columns_; i++) {
    columns_.emplace_back(i);
  }
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto* item : items_) {
    delete item;
  }
  items_.clear();
  for (auto* l : labels_) {
    delete l;
  }
  labels_.clear();

  // Clear the help bar on exit.
  curses_out->footer()->window()->Erase();
  curses_out->footer()->window()->Refresh();
  curses_out->SetIndicatorMode(IndicatorMode::NONE);
}
