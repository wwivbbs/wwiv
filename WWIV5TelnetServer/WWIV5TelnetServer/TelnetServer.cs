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

        private void LaunchInstance(NodeStatus node, Socket socketParam)
        {
            using (Socket socket = socketParam)
            {
                var executable = Properties.Settings.Default.executable;
                var arguments = Properties.Settings.Default.parameters;
                var homeDirectory = Properties.Settings.Default.homeDirectory;
                var windowStyle = (Properties.Settings.Default.launchMinimized ? ProcessWindowStyle.Minimized : ProcessWindowStyle.Maximized);

                CommandLineBuilder cmdlineBuilder = new CommandLineBuilder(arguments, executable);

                Process p = new Process();
                var socketHandle = socket.Handle.ToInt32();
                p.EnableRaisingEvents = false;
                p.StartInfo.FileName = executable;
                p.StartInfo.Arguments = cmdlineBuilder.CreateArguments(node.Node, socketHandle);
                p.StartInfo.WorkingDirectory = homeDirectory;
                p.StartInfo.UseShellExecute = false;
                p.StartInfo.WindowStyle = windowStyle;
                OnStatusMessageUpdated("Launching binary: " + cmdlineBuilder.CreateFullCommandLine(node.Node, socketHandle));
                p.Start();
                Console.WriteLine("binary launched.");
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
