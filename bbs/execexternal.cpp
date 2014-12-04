/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"
#include "instmsg.h"

int ExecuteExternalProgram(const std::string& commandLine, int nFlags) {
  // forget it if the user has hung up
  if (!(nFlags & EFLAG_NOHUP)) {
    if (CheckForHangup()) {
      return -1;
    }
  }
  create_chain_file();

  // get ready to run it
  if (session()->IsUserOnline()) {
    session()->WriteCurrentUser();
    write_qscn(session()->usernum, qsc, false);
  }
  session()->localIO()->set_global_handle(false);

  // extra processing for net programs
  if (nFlags & EFLAG_NETPROG) {
    write_inst(INST_LOC_NET, session()->GetNetworkNumber() + 1, INST_FLAGS_NONE);
  }

  // Execute the program and make sure the workingdir is reset
  int nExecRetCode = ExecExternalProgram(commandLine, nFlags);
  GetApplication()->CdHome();

  // Reread the user record.
  if (session()->IsUserOnline()) {
    GetApplication()->GetUserManager()->ReadUser(session()->user(), session()->usernum, true);
    read_qscn(session()->usernum, qsc, false, true);
    GetApplication()->UpdateTopScreen();
  }

  // return to caller
  return nExecRetCode;
}

