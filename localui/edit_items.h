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
#ifndef INCLUDED_LOCALUI_EDIT_ITEMS_H
#define INCLUDED_LOCALUI_EDIT_ITEMS_H

#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/input.h"
#include "sdk/config.h"
#include <functional>
#include <set>
#include <string>
#include <vector>

enum class cell_type_t { label, edit_item };
/**
 * Represent a cell in the UI.
 *
 * The column number is a number 1-N
 * x is the start position relative to the window
 * Width span both label, padding between label and item, and item widths.
 */
class Cell final {
public:
  Cell() = default;
  [[nodiscard]] int width() const;
  void set_width(int);
  [[nodiscard]] int x() const { return x_; }
  void set_x(int);
  [[nodiscard]] int column() const;
  void set_column(int c) { column_ = c; }

  Item* item{nullptr};
  int x_{1};
  int width_{0};
  int column_{0};
  cell_type_t cell_type_{cell_type_t::edit_item};
  int colspan_ = 1;
};


class Column final {
public:
  Column() = default;
  int width{0};
  int x{0};
};

//TODO(rushfan): Maybe this should be renamed to Form, since that's really
//what this has evolved into from a simple structure of EditItem instances.
/**
 * Creates a window containing a form of items that may be filled out
 * or "edited".
 *
 * Typically the items are instances of "EditItem" classes
 */
class EditItems final {
public:
  typedef std::function<void()> additional_helpfn;
  EditItems();
  EditItems(EditItems const&) = delete;
  EditItems(EditItems&&) = delete;
  EditItems& operator=(EditItems const&) = delete;
  EditItems& operator=(EditItems&&) = delete;
  ~EditItems();

  void Run(const std::string& title = "");
  void Display() const;

  void set_navigation_help_items(const std::vector<HelpItem>& items) {
    navigation_help_items_ = items;
  }

  void set_editmode_help_items(const std::vector<HelpItem>& items) { editor_help_items_ = items; }
  void set_navigation_extra_help_items(const std::vector<HelpItem>& items) {
    navigation_extra_help_items_ = items;
  }

  std::vector<BaseEditItem*>& items() { return items_; }

  /**
   * Adds an item to the list of EditItems.
   *
   * This uses both a column number and optionally a value to override for Y.
   * If y is not set, the items preferred Y value will be used.
   */
  BaseEditItem* add(BaseEditItem* item, int column = 1, int y = -1);

  /**
   * Adds a label to the list of EditItems.
   *
   * This uses both a column number and optionally a value to override for Y.
   * If y is not set, the items preferred Y value will be used.
   */
  Label* add(Label* label, int column = 1, int y = -1);

  /**
   * Adds a label and item
   *
   * This uses both a column number and optionally a value to override for Y.
   * If y is not set, the items preferred Y value will be used.
   */
  BaseEditItem* add(Label* label, BaseEditItem* item, int column = 1, int y = -1);

  /**
   * Adds a label and item and help text
   *
   * This uses both a column number and optionally a value to override for Y.
   * If y is not set, the items preferred Y value will be used.
   */
  BaseEditItem* add(Label* label, BaseEditItem* item, const std::string& help, int column = 1,
                    int y = -1);

  void create_window(const std::string& title);

  /**
   * Gets a key and automatically includes all navigation related keys
   * from config.
   */
  [[nodiscard]] char GetKeyWithNavigation(const NavigationKeyConfig& config) const;

  [[nodiscard]] CursesWindow* window() const { return window_.get(); }
  [[nodiscard]] int size() const noexcept { return static_cast<int>(items_.size()); }

  [[nodiscard]] int max_display_width() const;

  [[nodiscard]] int max_display_height();

  /**
   * Moves the labels to the x position just after the labels.
   * This only works for single column layouts of: {label:} {item}
   */
  void relayout_items_and_labels();

  Cell& cell(int row, int col);

  /**
   * Add a column number to align label sizes for.
   */
  void add_aligned_width_column(int n) { align_label_cols_.insert(n); }

  static std::vector<HelpItem> StandardNavigationHelpItems();

  static std::vector<HelpItem> StandardEditorHelpItems() { return {{"Esc", "Exit"}}; }

  static std::vector<HelpItem> ExitOnlyHelpItems() { return {{"Esc", "Exit"}}; }

private:
  std::vector<BaseEditItem*> items_;
  std::vector<Label*> labels_;
  std::vector<HelpItem> navigation_help_items_;
  std::vector<HelpItem> navigation_extra_help_items_;
  std::vector<HelpItem> editor_help_items_;
  std::unique_ptr<CursesWindow> window_;
  bool edit_mode_;
  // row, then columns
  std::map<int, std::map<int, Cell>> cells_;
  std::set<int> align_label_cols_;
};

#endif
