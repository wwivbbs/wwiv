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
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace wwiv::stl;

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

BaseEditItem* EditItems::add(BaseEditItem* item, int column, int row) {
  DCHECK(item);
  item->set_column(column);
  if (row >= 0) {
    item->set_y(row);
    auto& cell = cells_[row][column];
    cell.item = item;
    cell.set_column(column);
  }
  items_.push_back(item);
  return item;
}

Label* EditItems::add(Label* label, int column, int row) {
  DCHECK(label);
  label->set_column(column);
  if (row >= 0) {
    label->set_y(row);
    auto& cell = cells_[row][column];
    cell.item = label;
    cell.set_column(column);
    cell.cell_type_ = cell_type_t::label;
  }
  labels_.push_back(label);
  return label;
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item, int column, int row) {
  add(label, column, row);
  return add(item, column+1, row);
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item, const std::string& help, int column, int row) {
  item->set_help_text(help);
  return add(label, item, column, row);
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
      result = l->y() + l->height() - 1;
    }
  }
  for (const auto* i : items_) {
    if (i->y() > result) {
      result = i->y();
    }
  }
  return std::min<int>(curses_out->window()->GetMaxY(), result + 2);
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
  if (cells_.empty()) {
    return;
  }

  std::map<int, std::set<int>> widths_and_rows;
  for (const auto& [row_num, row] : cells_) {
    widths_and_rows[size_int(row)].insert(row_num);
  }

  std::map<int, std::map<int, Column>> widths_and_columns;
  for (const auto& [num_cols_for_this_row, row_nums] : widths_and_rows) {
    // Compute Max Width per cell
    for (const auto& row_num : row_nums) {
      for (auto& [col_num, cell] : cells_[row_num]) {
        // c.first is column, c.second is Cell
        auto& columns = widths_and_columns[num_cols_for_this_row];
        if (cell.colspan_ == 1 && cell.width() > columns[col_num].width) {
          // Ignore cells with colspan > 1
          columns[col_num].width = cell.width();
        }
      }
    }
    // Set Max Width per cell
    for (const auto& row_num : row_nums) {
      auto& columns = widths_and_columns[size_int(cells_[row_num])];
      for (auto& [col_num, cell] : cells_[row_num]) {
        if (const auto width = columns[col_num].width; width > 0) {
          cell.set_width(width);
        }
      }
    }
  }

  if (!align_label_cols_.empty()) {
    std::map<int, int> aligned_col_max_width;
    for (const auto aligned_col : align_label_cols_) {
      auto& max_width = aligned_col_max_width[aligned_col];
      for (auto& [rownum, row] : cells_) {
        if (!contains(row, aligned_col)) {
          continue;
        }
        auto& c = at(row, aligned_col);
        const auto width = c.width();
        if (c.cell_type_ == cell_type_t::label && width > max_width) {
          max_width = width;
        }
      }
    }
    for (const auto aligned_col : align_label_cols_) {
      auto& width = aligned_col_max_width[aligned_col];
      // Update width for the individual item.
      for (auto& [rownum, row] : cells_) {
        if (!contains(row, aligned_col)) {
          continue;
        }
        auto& cell = at(row, aligned_col);
        if (cell.colspan_ == 1) {
          cell.set_width(width);
        }
      }
      // Update column width in Columns, used for spacing
      for (auto& [wcwidth, cols] : widths_and_columns) {
        if (!contains(cols, aligned_col)) {
          continue;
        }
        at(cols, aligned_col).width = width;
      }
    }

  }

  // Figure out column x;
  for (const auto& r : widths_and_rows) {
    auto& columns = widths_and_columns[r.first];
    for (auto& [col_num, col] : columns) {
      if (col_num <= 0) {
        // should be in order, so skip column 0, which is width 0;
        continue;
      }
      auto& prev = columns[col_num - 1];
      col.x = prev.x + prev.width + 1;
    }
  }

  // Now need to set X values for everything.
  for (auto& [_, row] : cells_) {
    auto& columns = widths_and_columns[size_int(row)];
    for (auto& [col_num, cell] : row) {
      auto& col = columns[col_num];
      cell.set_x(col.x);
    }
  }
}

Cell& EditItems::cell(int row, int col) {
  return cells_[row][col];
}

std::vector<HelpItem> EditItems::StandardNavigationHelpItems() {
  return {
      {"Esc", "Exit"}, {"Enter", "Edit"}, {"[", "Previous"},
      {"]", "Next"}, {"{", "Previous 10"}, {"}", "Next 10"},
  };
}

int Cell::width() const {
  return item ? item->width() + 1 : 0;
}

void Cell::set_width(int w) {
  width_ = w;
  if (item) {
    item->set_width(w);
  }
}

void Cell::set_x(int new_x) {
  x_ = new_x;
  if (item) {
    item->set_x(new_x);
  }
}

int Cell::column() const { return column_; }

EditItems::EditItems()
  : navigation_help_items_(StandardNavigationHelpItems()),
    editor_help_items_(StandardEditorHelpItems()), edit_mode_(false) { }

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
