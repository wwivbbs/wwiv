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
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;

namespace WWIV5TelnetServer
{
    class TelnetServer : IDisposable
    {
        private Socket server;
        private Thread launcherThread;
        private Object nodeLock = new Object();
        private int lowNode;
        private int highNode;
        private NodeStatus[] nodes;

        public delegate void StatusMessageEventHandler(object sender, StatusMessageEventArgs e);
        public event StatusMessageEventHandler StatusMessageChanged;

        public delegate void NodeStatusEventHandler(object sender, NodeStatusEventArgs e);
        public event NodeStatusEventHandler NodeStatusChanged;

        public void Start()
        {
            launcherThread = new Thread(Run);
            launcherThread.Start();
            OnStatusMessageUpdated("Telnet Sever Started");
            lowNode = Convert.ToInt32(Properties.Settings.Default.startNode);
            highNode = Convert.ToInt32(Properties.Settings.Default.endNode);
            nodes = new NodeStatus[highNode - lowNode + 1];
            for (int i = 0; i < nodes.Length; i++)
            {
                nodes[i] = new NodeStatus(i + lowNode);
            }
        }

        public void Stop()
        {
            OnStatusMessageUpdated("Stopping Telnet Server.");
            if (launcherThread == null)
            {
                OnStatusMessageUpdated("ERROR: LauncherThread was never set.");
                return;
            }
            if (server != null)
            {
                server.Close();
                server = null;
            }
            launcherThread.Abort();
            launcherThread.Join();
            launcherThread = null;
            nodes = null;
            OnStatusMessageUpdated("Telnet Sever Stopped");
        }

        private void Run()
        {
            Int32 port = 23;
            server = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            server.Bind(new IPEndPoint(IPAddress.Any, port));
            server.Listen(4);
            while (true)
            {
                OnStatusMessageUpdated("Waiting for connection.");
                try 
                {
                    Socket socket = server.Accept();
                    Console.WriteLine("After accept.");
                    NodeStatus node = getNextNode();
                    string ip = socket.RemoteEndPoint.ToString();
                    OnStatusMessageUpdated("Connection from " + ip);
                    if (node != null)
                    {
                        node.RemoteAddress = ip;
                        OnStatusMessageUpdated("Launching Node #" +node.Node);
                        Thread instanceThread = new Thread(() => LaunchInstance(node, socket));
                        instanceThread.Start();
                    }
                    else
                    {
                        // Send BUSY signal.
                        OnStatusMessageUpdated("Sending Busy Signal.");
                        byte[] busy = System.Text.Encoding.ASCII.GetBytes("BUSY");
                        try {
                            socket.Send(busy);
                        } finally {
                            socket.Close();
                        }
                    }
                } catch (SocketException e) {
                    Console.WriteLine(e.ToString());
                }
            }
        }

        private const int SW_MINIMIZED = 2;
        [DllImport("user32.dll")]
        private static extern int ShowWindowAsync(IntPtr hWnd, int nCmdShow);

        private void LaunchInstance(NodeStatus node, Socket socketParam)
        {
            using (Socket socket = socketParam)
            {
                var executable = Properties.Settings.Default.executable;
                var arguments = Properties.Settings.Default.parameters;
                var homeDirectory = Properties.Settings.Default.homeDirectory;

                CommandLineBuilder cmdlineBuilder = new CommandLineBuilder(arguments, executable);

                Process p = new Process();
                var socketHandle = socket.Handle.ToInt32();
                p.EnableRaisingEvents = false;
                p.StartInfo.FileName = executable;
                p.StartInfo.Arguments = cmdlineBuilder.CreateArguments(node.Node, socketHandle);
                p.StartInfo.WorkingDirectory = homeDirectory;
                p.StartInfo.UseShellExecute = false;
                OnStatusMessageUpdated("Launching binary: " + cmdlineBuilder.CreateFullCommandLine(node.Node, socketHandle));
                p.Start();
                Console.WriteLine("binary launched.");

                if (Properties.Settings.Default.launchMinimized)
                {
                    for (int i = 0; i < 10; i++)
                    {
                        // The process is launched asynchronously, so wait for up to a second
                        // for the main window handle to be created and set on the process class.
                        if (p.MainWindowHandle.ToInt32() != 0)
                        {
                            break;
                        }
                        Thread.Sleep(100);
                    }
                    OnStatusMessageUpdated("Trying to minimize process on handle:" + p.MainWindowHandle);
                    ShowWindowAsync(p.MainWindowHandle, SW_MINIMIZED);
                }

                p.WaitForExit();
                lock (nodeLock)
                {
                    node.InUse = false;
                }
            }
        }


        public void Dispose()
        {
            Stop();
        }

        /**
         * Gets the next free node or null of none exists.
         */
        private NodeStatus getNextNode()
        {
            lock (nodeLock)
            {
                foreach(NodeStatus node in nodes) {
                    if (!node.InUse)
                    {
                        // Mark it in use.
                        node.InUse = true;
                        // return it.
                        return node;
                    }
                }
            }
            // No node is available, return null.
            return null;
        }

        protected virtual void OnStatusMessageUpdated(string message)
        {
            StatusMessageEventArgs e = new StatusMessageEventArgs(message);
            StatusMessageChanged(this, e);
        }


        protected virtual void OnNodeUpdated(NodeStatus nodeStatus)
        {
            NodeStatusEventArgs e = new NodeStatusEventArgs(nodeStatus);
            NodeStatusChanged(this, e);
        }
    }

}
