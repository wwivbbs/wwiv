/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "wwivconfig/new_user.h"

#include "core/strings.h"
#include "localui/curses_win.h"
#include "localui/edit_items.h"
#include "localui/input.h"

#include <cstdint>
#include <string>

using namespace wwiv::localui;
using namespace wwiv::strings;

void newuser_settings(wwiv::sdk::Config& config, wwiv::sdk::newuser_config_t& nc, CursesWindow*) {
  const std::vector<std::pair<wwiv::sdk::newuser_item_type_t, std::string>> newuser_item_type_list =
      {{wwiv::sdk::newuser_item_type_t::unused, "Unused"},
       {wwiv::sdk::newuser_item_type_t::optional, "Optional"},
       {wwiv::sdk::newuser_item_type_t::required, "Required"}};

  auto closed_system = config.closed_system();
  auto newuser_gold = static_cast<int>(config.newuser_gold());
  auto newuser_pw = config.newuser_password();
  auto newusersl = config.newuser_sl();
  auto newuserdsl = config.newuser_dsl();
  auto newuser_restrict = config.newuser_restrict();
  const auto validated_sl = config.validated_sl();

  auto y = 1;
  EditItems items{};
  items.add(new Label("Closed system:"),
            new BooleanEditItem(&closed_system),
    "Are users allowed to establish accounts on this BBS", 1, y);
  items.add(
      new Label("Password:"),
      new StringEditItem<std::string&>(20, newuser_pw, EditLineMode::UPPER_ONLY),
    "Optional password users must provide in order create an account", 3, y);
  ++y;
  items.add(new Label("SL:"),
            new NumberEditItem<uint8_t>(&newusersl),
    "The security level that all new users are given. The default is 10", 1, y);
  items.add(new Label("DSL:"),
            new NumberEditItem<uint8_t>(&newuserdsl),
        "The download security level that all new users are given. The default is 10", 3, y);
  ++y;
  items.add(new Label("Gold:"),
            new NumberEditItem<int>(&newuser_gold),
    "The default amount of gold given to new users", 1, y);
  items.add(new Label("Restrict:"),
            new RestrictionsEditItem(&newuser_restrict),
    "Restrictions given to new users from certain features of the system.", 3, y);
  ++y;
  items.add(new Label("Validated SL:"),
            new NumberEditItem<uint8_t>(&newusersl),
    "The lowest security level that means validated. For autoval set this == Newuser SL", 1, y);

  ++y;
  ++y;
  items.add(new Label("Real Name:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_real_name),
    "Ask Callers for a real name. This may be needed for some networks", 1, y);
  items.add(new Label("Email Address:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_email_address),
    "Ask callers for an email address", 3, y);
  ++y;
  items.add(new Label("Voice Phone:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_voice_phone),
    "Ask Callers for a void phone number", 1, y);
  items.add(new Label("Data Phone:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_data_phone),
    "Ask callers for a modem/data phone number", 3, y);
  ++y;
  items.add(new Label("Street Address:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_address_street),
    "Ask Callers for a street address", 1, y);
  items.add(new Label("City/State:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_address_city_state),
    "Ask callers the city they are calling from", 3, y);
  ++y;
  items.add(new Label("ZipCode:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_address_zipcode),
    "Ask callers for the zip code they are calling from", 1, y);
  items.add(new Label("Country:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_address_country),
    "Ask Callers for the country they are calling from", 3, y);
  ++y;
  items.add(new Label("Gender:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_gender),
    "Ask Callers for their gender (may be used to limit access)", 1, y);
  items.add(new Label("Birthday:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_birthday),
    "Ask callers for their date of birth (may be used to limit access)", 3, y);
  ++y;
  items.add(new Label("Computer Type:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_computer_type),
    "Ask Callers for their computer type.", 1, y);
  items.add(new Label("Callsign:"),
            new ToggleEditItem<wwiv::sdk::newuser_item_type_t>(newuser_item_type_list, &nc.use_callsign),
    "Ask callers for their amateur radio callsign (if they have one)", 3, y);

  items.add_aligned_width_column(1);
  items.relayout_items_and_labels();
  items.Run("New User Configuration");
  config.closed_system(closed_system);
  config.newuser_password(newuser_pw);
  config.newuser_sl(newusersl);
  config.newuser_dsl(newuserdsl);
  config.validated_sl(validated_sl);
  config.newuser_restrict(newuser_restrict);
  config.newuser_gold(static_cast<float>(newuser_gold));
}
