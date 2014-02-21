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
using System.ComponentModel;
using System.Linq;
using System.Text;

namespace WWIV5TelnetServer
{
    public class NodeStatus : INotifyPropertyChanged
    {
        public static string WFC = "Waiting for Call";
        public static string CONNECTED = "Connected";
        private int node;
        private string status;
        private string remoteAddress;
        private bool inUse;

        public event PropertyChangedEventHandler PropertyChanged;

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
            set
            {
                if (status != value)
                {
                    status = value;
                    NotifyPropertyChanged("Status");
                }
            }
        }

        public string RemoteAddress
        {
            get
            {
                return remoteAddress;
            }
            set
            {
                if (value != remoteAddress)
                {
                    remoteAddress = value;
                    NotifyPropertyChanged("RemoteAddress");
                }
            }
        }

        public bool InUse
        {
            get
            {
                return inUse;
            }
            set
            {
                if (inUse != value)
                {
                    inUse = value;
                    NotifyPropertyChanged("IsUse");
                }
                Console.WriteLine("InUse: " + inUse + " for node: " + node);
            }
        }

        public string FullStatus
        {
            get
            {
                StringBuilder s = new StringBuilder("Node#" + node);
                s.Append(" ");
                if (inUse)
                {
                    s.AppendFormat("Connected fom {0}", remoteAddress);
                }
                else
                {
                    s.Append(WFC);
                }
                return s.ToString();
            }
        }

        public NodeStatus(int node)
        {
            this.node = node;
            status = "WFC";
            remoteAddress = "";
            inUse = false;
        }

        public override string ToString() {
            return FullStatus;
        }

        private void NotifyPropertyChanged(String info)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(info));
                PropertyChanged(this, new PropertyChangedEventArgs("FullStatus"));
                Console.WriteLine("Firing property changed event for nodestatus: " + FullStatus);
            }
        }

    }
}
