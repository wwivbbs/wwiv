/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
#ifndef __INCLUDED_FIX_H__
#define __INCLUDED_FIX_H__

class WFile;

void giveUp();
void maybeGiveUp();

namespace wwiv {
namespace fix {

bool checkDirExists(WFile &dir, const char *desc);
class FixConfiguration;

class Command {
public:
    virtual int Execute() = 0;
};

class BaseCommand : public Command {
public:
    BaseCommand(FixConfiguration* config);
    virtual ~BaseCommand();

protected:
    FixConfiguration* config() const { return config_; }

private:
    FixConfiguration* config_;
};

}  // namespace fix
}  // namespace wwiv

#endif  // __INCLUDED_FIX_H__