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
// ReSharper disable CppClangTidyBugproneMacroParentheses
#ifndef INCLUDED_LOCALUI_EDIT_ITEMS_H
#define INCLUDED_LOCALUI_EDIT_ITEMS_H

#include "core/file.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/input.h"
#include "sdk/config.h"
#include <functional>
#include <string>
#include <vector>

class Column final {
public:
  explicit Column(int num) : num_(num) {}

  int num_;
};

class EditItems final {
public:
  typedef std::function<void()> additional_helpfn;
  EditItems()
      : navigation_help_items_(StandardNavigationHelpItems()),
        editor_help_items_(StandardEditorHelpItems()), edit_mode_(false) {}
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
  /** Adds an item to the list of EditItems. */
  BaseEditItem* add(BaseEditItem* item);
  /** Adds a label to the list of EditItems. */
  Label* add(Label* label);
  /** Adds a list of labels */
  void add_labels(std::initializer_list<Label*> labels);
  /** Adds a label and item */
  BaseEditItem* add(Label* label, BaseEditItem* item);
  /** Adds a label and item and help text*/
  BaseEditItem* add(Label* label, BaseEditItem* item, const std::string& help);
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

  /** Returns the size of the longest label */
  [[nodiscard]] int max_label_width() const;

  /**
   * Moves the labels to the x position just after the labels.
   * This only works for single column layouts of: {label:} {item}
   */
  void relayout_items_and_labels();

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
  int num_columns_{0};
};

#endif
