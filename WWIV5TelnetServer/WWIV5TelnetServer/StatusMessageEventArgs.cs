using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace WWIV5TelnetServer
{
    class StatusMessageEventArgs : EventArgs
    {
        private string message;
        public StatusMessageEventArgs(string message)
        {
            this.message = message;
        }

        public string Message
        {
            get
            {
                return message;
            }
        }
    
    }
}
