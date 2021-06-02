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
/**************************************************************************/
#ifndef INCLUDED_BBS_EXECEXTERNAL_H
#define INCLUDED_BBS_EXECEXTERNAL_H

#include "bbs/stuffin.h"
#include <string>

/**
 * Executes an external program or am internal script.
 *
 * To execute a script, use prefix the command with @ and script type.
 * For a basic script, use the following syntax for command_line:
 * '@basic:[SCRIPT_NAME]'
 *
 * flags are the EFLAG_XXXX flags defined in vardec.h
 */
int ExecuteExternalProgram(wwiv::bbs::CommandLine& command_line, uint32_t flags);

#endif
