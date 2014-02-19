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
