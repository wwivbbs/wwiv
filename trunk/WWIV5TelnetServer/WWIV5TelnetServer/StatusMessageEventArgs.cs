/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2014-2015 WWIV Software Services              */
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
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace WWIV5TelnetServer
{
  class StatusMessageEventArgs : EventArgs
  {
    public enum MessageType { Connect, Disconnect , Status, LogInfo, LogDebug, LogWarning, LogError}
    public StatusMessageEventArgs(string message, MessageType type)
    {
      this.Message = message;
      this.Type = type;
    }

    public string Message { get; private set; }
    public MessageType Type { get; private set; }

    public bool IsConnectionRelated() {
      var type = this.Type;
      return type == MessageType.Connect || type == MessageType.Disconnect || type == MessageType.Status;
    }
  }

}
