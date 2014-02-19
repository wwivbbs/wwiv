/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C) 2014, WWIV Software Services             */
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
    class NodeStatusEventArgs : EventArgs
    {
        private int node;
        private string status;
        private string remoteAddress;

        public NodeStatusEventArgs(int node, string status, string remoteAddress)
        {
            this.node = node;
            this.status = status;
            this.remoteAddress = remoteAddress;
        }

        public NodeStatusEventArgs(NodeStatus status)
        {
            this.node = status.Node;
            this.status = status.Status;
            this.remoteAddress = status.RemoteAddress;
        }

        public int Node
        {
            get
            {
                return node;
            }
        }

        public string Status
        {
            get
            {
                return status;
            }
        }

        public string RemoteAddress
        {
            get
            {
                return remoteAddress;
            }
        }
    }
}
