using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace WWIV5TelnetServer
{
    public class NodeStatus
    {
        public static string WFC = "Waiting for Call";
        public static string CONNECTED = "Connected";
        private int node;
        private string status;
        private string remoteAddress;
        private bool inUse;

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
                status = value;
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
                remoteAddress = value;
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
                inUse = value;
            }
        }

        public NodeStatus(int node)
        {
            this.node = node;
            status = "WFC";
            remoteAddress = "";
            inUse = false;
        }
    }
}
