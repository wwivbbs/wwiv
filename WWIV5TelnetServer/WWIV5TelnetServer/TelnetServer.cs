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
using System.ComponentModel;
using System.Timers;

namespace WWIV5TelnetServer
{
    class TelnetServer : IDisposable
    {
        private Socket server;
        private Thread launcherThread;
        private Object nodeLock = new Object();
        private int lowNode;
        private int highNode;
        private List<NodeStatus> nodes;

        // Property for Nodes
        public List<NodeStatus> Nodes { get { return nodes; } }

        public delegate void StatusMessageEventHandler(object sender, StatusMessageEventArgs e);
        public event StatusMessageEventHandler StatusMessageChanged;

        public delegate void NodeStatusEventHandler(object sender, NodeStatusEventArgs e);
        public event NodeStatusEventHandler NodeStatusChanged;

        public void Start()
        {
            launcherThread = new Thread(Run);
            launcherThread.Name = "TelnetServer";
            launcherThread.Start();
            OnStatusMessageUpdated("Telnet Server Started", StatusMessageEventArgs.MessageType.LogInfo);
            lowNode = Convert.ToInt32(Properties.Settings.Default.startNode);
            highNode = Convert.ToInt32(Properties.Settings.Default.endNode);
            var size = highNode - lowNode + 1;
            nodes = new List<NodeStatus>(size);
            for (int i = 0; i < size; i++)
            {
                nodes.Add(new NodeStatus(i + lowNode));
            }
        }

        public void Stop()
        {
            if (launcherThread == null)
            {
              OnStatusMessageUpdated("ERROR: LauncherThread was never set.", StatusMessageEventArgs.MessageType.LogError);
              return;
            }
            OnStatusMessageUpdated("Stopping Telnet Server.", StatusMessageEventArgs.MessageType.LogInfo);
            if (server != null)
            {
                server.Close();
                server = null;
            }
            launcherThread.Abort();
            launcherThread.Join();
            launcherThread = null;
            nodes = null;
            OnStatusMessageUpdated("Telnet Server Stopped", StatusMessageEventArgs.MessageType.LogInfo);
        }

        private void Run()
        {
            Int32 port = Convert.ToInt32(Properties.Settings.Default.port);
            server = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            server.Bind(new IPEndPoint(IPAddress.Any, port));
            server.Listen(4);
            while (true)
            {
              OnStatusMessageUpdated("Waiting for connection.", StatusMessageEventArgs.MessageType.LogInfo);
                try 
                {
                    Socket socket = server.Accept();
                    Console.WriteLine("After accept.");
                    NodeStatus node = getNextNode();
                    string ip = ((System.Net.IPEndPoint)socket.RemoteEndPoint).Address.ToString();
                    // HACK
                    if (ip == "202.39.236.116")
                    {
                        // This IP has been bad. Blacklist it until proper filtering is added.
                      OnStatusMessageUpdated("Attempt from blacklisted IP.", StatusMessageEventArgs.MessageType.LogInfo);
                        Thread.Sleep(1000);
                        node = null;
                    }
                    OnStatusMessageUpdated("Connection from " + ip, StatusMessageEventArgs.MessageType.Connect);
                    if (node != null)
                    {
                        node.RemoteAddress = ip;
                        OnStatusMessageUpdated("Launching Node #" + node.Node, StatusMessageEventArgs.MessageType.LogInfo);
                        Thread instanceThread = new Thread(() => LaunchInstance(node, socket));
                        instanceThread.Name = "Instance #" + node.Node;
                        instanceThread.Start();
                        OnNodeUpdated(node);
                    }
                    else
                    {
                        // Send BUSY signal.
                      OnStatusMessageUpdated("Sending Busy Signal.", StatusMessageEventArgs.MessageType.Status);
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

        private void LaunchInstance(NodeStatus node, Socket socket)
        {
            try
            {
                var executable = Properties.Settings.Default.executable;
                var argumentsTemplate = Properties.Settings.Default.parameters;
                var homeDirectory = Properties.Settings.Default.homeDirectory;

                Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, DebugLog);
                var socketHandle = socket.Handle.ToInt32();
                Process p = launcher.launchTelnetNode(node.Node, socketHandle);
                p.WaitForExit();
                socket.Shutdown(SocketShutdown.Both);
                socket.Close();
            }
            catch (SocketException e)
            {
              Console.WriteLine(e.ToString());
            }
            catch (Exception e)
            {
              Console.WriteLine(e.ToString());
            }
            finally
            {
              lock (nodeLock)
              {
                node.InUse = false;
              }
              OnNodeUpdated(node);
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

        protected virtual void OnStatusMessageUpdated(string message, StatusMessageEventArgs.MessageType type)
        {
            StatusMessageEventArgs e = new StatusMessageEventArgs(message, type);
            var handler = StatusMessageChanged;
            if (handler != null)
            {
                StatusMessageChanged(this, e);
            }
        }

        protected virtual void DebugLog(string message)
        {
          StatusMessageEventArgs e = new StatusMessageEventArgs(message, StatusMessageEventArgs.MessageType.LogDebug);
          var handler = StatusMessageChanged;
          if (handler != null)
          {
            StatusMessageChanged(this, e);
          }
        }

        protected virtual void OnNodeUpdated(NodeStatus nodeStatus)
        {
            NodeStatusEventArgs e = new NodeStatusEventArgs(nodeStatus);
            var handler = NodeStatusChanged;
            if (handler != null)
            {
                NodeStatusChanged(this, e);
            }
        }
    }

}
