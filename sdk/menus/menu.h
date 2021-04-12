/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#ifndef INCLUDED_SDK_MENUS_MENU_H
#define INCLUDED_SDK_MENUS_MENU_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
class Config;
class User;
}
}

constexpr uint8_t MENU_FLAG_DELETED = 0x01;
constexpr uint8_t MENU_FLAG_MAINMENU = 0x02;

constexpr uint8_t MENU_NUMFLAG_NOTHING = 0;
constexpr uint8_t MENU_NUMFLAG_SUBNUMBER = 1;
constexpr uint8_t MENU_NUMFLAG_DIRNUMBER = 2;

constexpr uint8_t MENU_LOGTYPE_KEY = 0;
constexpr uint8_t MENU_LOGTYPE_NONE = 1;
constexpr uint8_t MENU_LOGTYPE_COMMAND = 2;
constexpr uint8_t MENU_LOGTYPE_DESC = 3;
constexpr uint8_t MENU_LOGTYPE_LAST = 4;

constexpr uint8_t MENU_HELP_DONTFORCE = 0;
constexpr uint8_t MENU_HELP_FORCE = 1;
constexpr uint8_t MENU_HELP_ONENTRANCE = 2;

constexpr int MENU_MAX_KEYS = 10;

#pragma pack(push, 1)

struct menu_header_430_t {
  char szSig[10];      /* Menu Signature WWIV431\x1a */
  char unused[54];

  uint16_t  nVersion;
  uint16_t  nEmpty;
  uint8_t   nFlags;

  uint8_t   nums;     /* What does a number do?  Set sub#, Dir#, nothing? */
  uint8_t   nLogging;     /* Types of logging, Key, None, command, desc       */

  uint8_t   nForceHelp;   /* force, don't force, on entrance only              */
  uint8_t   nAllowedMenu; /* Can pull down, regular or both menus be used?     */

  uint8_t  nTitleColor, nMainBorderColor, nMainBoxColor, nMainTextColor,
           nMainTextHLColor, nMainSelectedColor, nMainSelectedHLColor;

  uint8_t  nItemBorderColor, nItemBoxColor, nItemTextColor, nItemTextHLColor,
           nItemSelectedColor, nItemSelectedHLColor;

  char   szMenuTitle[21];
  char   unused_misc2[60];
  char   szPassWord[21];     /* required for entry of menu */
  uint16_t nMinSL, nMinDSL;    /* required for entry of menu */
  uint16_t uAR, uDAR;          /* required for entry of menu */
  uint16_t uRestrict;          /* not allowed restrictions   */
  bool nSysop, nCoSysop;   /* Must be either sysop or co */
  char   misc3[30];
  char   szScript[101];      /* Gets executed on entry     */
  char   szExitScript[101];  /* Executed on rtn from menu  */
  char unused_padding[109];
};

struct menu_rec_430_t {
  uint8_t nFlags;   /* AFLAG_????? */

  char szKey[MENU_MAX_KEYS + 1]; /* Keystroke to execute menu item   */
  char szExecute[101];           /* Command to execute               */
  char szMenuText[41];           /* Menu description                 */
  char unused_szPDText[41];      /* Pull down menu text               */

  char szHelp[81];               /* Help for this item               */
  char szSysopLog[51];           /* Msg to put in the log            */

  char szInstanceMessage[81];

  /* Security */
  uint16_t nMinSL, iMaxSL;
  uint16_t nMinDSL, iMaxDSL;
  uint16_t uAR, uDAR;        /* Must match all specified to be able to run     */
  uint16_t uRestrict;        /* If any of these restrictions, you cant execute */
  bool nSysop, nCoSysop;  /* true and false, does it take a co/sysop to run */
  char szPassWord[21];

  uint16_t nHide;            /* Hide text from PD/Regular/both or no menus */
  uint16_t unused_nPDFlags;  /* special characteristics for pull downs       */

  char unused_data[92];
};

#pragma pack(pop)

namespace wwiv::sdk::menus {

class MenuSet56;

class Menu430 {
public:
  Menu430(std::filesystem::path menu_dir, std::string menu_set,
          std::string menu_name);
  [[nodiscard]] bool Load();
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }

  menu_header_430_t header{};
  std::vector<menu_rec_430_t> recs;

  const std::filesystem::path menu_dir_;
  const std::string menu_set_;
  const std::string menu_name_;
  bool initialized_{false};
};

static_assert(sizeof(menu_rec_430_t) == sizeof(menu_header_430_t), "sizeof(menu_rec_430_t) == sizeof(menu_header_430_t)");

// What does a number key do in this menu.
enum class menu_numflag_t { none, subs, dirs };

// What to log for this command.
enum class menu_logtype_t { key, command, description, none };

// When is the menu displayed.
enum class menu_help_display_t { always, never, on_entrance, user_choice };

struct menu_action_56_t {
  // MenuCommand to execute
  std::string cmd;
  // Data to pass to menu command
  std::string data;
  // ACS needed to execute this menu action
  std::string acs;
};

struct menu_item_56_t {
  // KEY of the item (such as 'C' or "CHAINEDIT")
  std::string item_key;
  // Text for the item (i.e. "Chain Edit")
  std::string item_text;
  // Is this item visible in generated menus
  bool visible{true};
  // Help text to display in help screen
  std::string help_text;
  // What to log to sysops log when this menu item is invoked
  std::string log_text;
  // What to set for the instance message when this menu item is invoked.
  std::string instance_message;
  // ACS needed to invoke this command
  std::string acs;
  // password needed to invoke this menu item
  std::string password;
  // All of the actions to run when invoking this menu item
  std::vector<menu_action_56_t> actions;
};

struct generated_menu_56_t {
  int num_cols{3};
  // Colors
  std::string color_title{"|#4"};
  std::string color_item_text{"|#0"};
  std::string color_item_key{"|#2"};
  std::string color_item_braces{"|#1"};
  bool show_empty_text{false};
  int num_newlines_at_end{1};
};

struct menu_56_t {
  /** Clear screen before displaying menu? */
  bool cls{false};
  /* What does a number do?  Set sub#, Set dir#, nothing? */
  menu_numflag_t num_action;
  /* Types of logging, Key, None, command, desc */
  menu_logtype_t logging_action;
  /* force, display always, display on entrance only */
  menu_help_display_t help_type;

  // Details for generated menus.
  generated_menu_56_t generated_menu;

  // Title of this menu
  std::string title;
  // ACS needed to execute this menu
  std::string acs;
  // password needed to execute this menu
  std::string password;

  // Actions to invoke when entering this menu
  std::vector<menu_action_56_t> enter_actions;
  // Actions to invoke when exiting this menu
  std::vector<menu_action_56_t> exit_actions;
  // Menu items
  std::vector<menu_item_56_t> items;
};

/**
 * Represents a menuset, which is a collection of menus.
 */
struct menu_set_t {
  // Name of this menuset
  std::string name;
  // Description of this menuset.
  std::string description;
  // ACS needed to use this menuset.
  std::string acs;
  // Global menu items. These will be available in every menu in this menuset.
  std::vector<menu_item_56_t> items;
};

class Menu56 {
public:
  Menu56(std::filesystem::path menu_dir, const MenuSet56& menu_set,
          std::string menu_name);
  [[nodiscard]] bool Load();
  [[nodiscard]] bool Save();
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  void set_initialized(bool i) { initialized_ = i; }

  menu_56_t menu{};

  private:
  const std::filesystem::path menu_dir_;
  const std::string menu_set_;
  const std::string menu_name_;
  bool initialized_{false};
};

std::optional<Menu56> Create56MenuFrom43(const Menu430& m4, int max_backups);

// What does a number key do in this menu.
menu_numflag_t to_menu_numflag_t(int n);

// What to log for this command.
menu_logtype_t to_menu_logtype(int n);

// Converts help int to menu_help_display_t
menu_help_display_t to_menu_help_display(int n);

struct menu_command_help_t {
  // MenuCommand category
  std::string cat;
  // MenuCommand name
  std::string cmd;
  // Help text.
  std::string help;

  bool operator<(const menu_command_help_t& other) const {
    if (cat == other.cat) {
      return cmd < other.cmd;
    }
    return cat < other.cat;
  }
};

std::vector<menu_command_help_t> LoadCommandHelpJSON(const std::string& datadir);
bool SaveCommandHelpJSON(const std::string& datadir, const std::vector<menu_command_help_t>& cmds);

enum class menu_type_t { short_menu, long_menu };


} 
#endif