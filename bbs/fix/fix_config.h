/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-20014, WWIV Software Services            */
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
#ifndef __INCLUDED_FIXCONFIG_H__
#define __INCLUDED_FIXCONFIG_H__

#include <string>
#include <vector>

namespace wwiv {
namespace fix {

class FixConfiguration {
public:
	FixConfiguration(const std::vector<std::string> commands) 
        : flag_yes(false), flag_experimental(false), flag_check_users(true), commands_(commands) {}
	~FixConfiguration() {}

    void ParseCommandLine(int argc, char* argv[]);

	bool flag_yes;
	bool flag_experimental;
	bool flag_check_users;
    const std::vector<std::string>& commands() const { return commands_; }

private:
    void ShowHelp();
    std::vector<std::string> commands_;
};

}  // namespace wwiv
}  // namespace fix

#endif  // __INCLUDED_FIXCONFIG_H__
