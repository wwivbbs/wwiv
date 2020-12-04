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

BaseEditItem* EditItems::add(BaseEditItem* item) {
  items_.push_back(item);
  return item;
}

Label* EditItems::add(Label* l) {
  labels_.push_back(l);
  return l;
}

void EditItems::add_labels(std::initializer_list<Label*> labels) {
  for (auto* l : labels) {
    DCHECK(l);
    labels_.push_back(l);
  }
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item) {
  DCHECK(label);
  DCHECK(item);
  labels_.push_back(label);
  items_.push_back(item);
  return item;
}

BaseEditItem* EditItems::add(Label* label, BaseEditItem* item, const std::string& help) {
  item->set_help_text(help);
  return add(label, item);
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
  int result = 0;
  for (const auto* i : items_) {
    if ((i->x() + i->maxsize()) > result) {
      result = i->x() + i->maxsize();
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

int EditItems::max_label_width() const {
  std::string::size_type result = 0;
  for (const auto* l : labels_) {
    if (l->text().size() > result) {
      result = l->text().size();
    }
  }
  return static_cast<int>(result);
}

void EditItems::relayout_items_and_labels() {
  const auto x = max_label_width();
  for (auto* l : labels_) {
    l->set_width(x);
  }
  for (auto* i : items_) {
    i->set_x(x + 2 + 1); // 2 is a hack since that's always col1 position for label.
  }
}

std::vector<HelpItem> EditItems::StandardNavigationHelpItems() {
  return {
      {"Esc", "Exit"}, {"Enter", "Edit"}, {"[", "Previous"},
      {"]", "Next"}, {"{", "Previous 10"}, {"}", "Next 10"},
  };
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
