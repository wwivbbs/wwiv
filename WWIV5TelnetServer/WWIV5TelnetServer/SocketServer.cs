/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5.x                            */
/*                Copyright (C)2014-2016 WWIV Software Services           */
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
using System.Net;
using System.Net.Sockets;
using System.Diagnostics;
using System.Threading;

namespace WWIV5TelnetServer
{
    class SocketServer : IDisposable
    {
        private Socket server;
        private Thread launcherThread;
        private NodeManager nodeManager;
        private Int32 port;
        private string argumentsTemplate;
        private string name;

        public delegate void StatusMessageEventHandler(object sender, StatusMessageEventArgs e);
        public event StatusMessageEventHandler StatusMessageChanged;

        public delegate void NodeStatusEventHandler(object sender, NodeStatusEventArgs e);
        public event NodeStatusEventHandler NodeStatusChanged;

        public SocketServer(NodeManager nodeManager, Int32 port, string argsTemplate, string name)
        {
            this.nodeManager = nodeManager;
            this.port = port;
            this.argumentsTemplate = argsTemplate;
            this.name = name;
        }

        public void Start()
        {
            launcherThread = new Thread(Run);
            launcherThread.Name = this.name;
            launcherThread.Start();
            OnStatusMessageUpdated(this.name + " Server Started", StatusMessageEventArgs.MessageType.LogInfo);
        }

        public void Stop()
        {
            if (launcherThread == null)
            {
                OnStatusMessageUpdated("ERROR: LauncherThread was never set.", StatusMessageEventArgs.MessageType.LogError);
                return;
            }
            OnStatusMessageUpdated(String.Format("Stopping {0} Server.", this.name), StatusMessageEventArgs.MessageType.LogInfo);
            if (server != null)
            {
                server.Close();
                server = null;
            }
            launcherThread.Abort();
            launcherThread.Join();
            launcherThread = null;
            OnStatusMessageUpdated(String.Format("{0} Server Stopped", this.name), StatusMessageEventArgs.MessageType.LogInfo);
        }

        private void Run()
        {
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
                    NodeStatus node = nodeManager.getNextNode();
                    string ip = ((System.Net.IPEndPoint)socket.RemoteEndPoint).Address.ToString();
                    // HACK
                    if (ip == "202.39.236.116")
                    {
                        // This IP has been bad. Blacklist it until proper filtering is added.
                        OnStatusMessageUpdated("Attempt from blacklisted IP.", StatusMessageEventArgs.MessageType.LogInfo);
                        Thread.Sleep(1000);
                        node = null;
                    }
                    OnStatusMessageUpdated(name + " from " + ip, StatusMessageEventArgs.MessageType.Connect);
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
                        try
                        {
                            socket.Send(busy);
                        }
                        finally
                        {
                            socket.Close();
                        }
                    }
                }
                catch (SocketException e)
                {
                    Console.WriteLine(e.ToString());
                }
            }
        }

        private void LaunchInstance(NodeStatus node, Socket socket)
        {
            try
            {
                var executable = Properties.Settings.Default.executable;
                var homeDirectory = Properties.Settings.Default.homeDirectory;

                Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, DebugLog);
                var socketHandle = socket.Handle.ToInt32();
                Process p = launcher.launchSocketNode(node.Node, socketHandle);
                if (p != null)
                {
                    p.WaitForExit();
                }
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
                socket.Shutdown(SocketShutdown.Both);
                socket.Close();
                nodeManager.freeNode(node);
                OnNodeUpdated(node);
            }
        }

        public void Dispose()
        {
            Stop();
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