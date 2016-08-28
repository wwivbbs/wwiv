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
using System.Collections.Generic;
using System.IO;
using System.Collections;
using System.Text;

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
    private Blacklist bl;
    private Dictionary<String, List<DateTime>> connections;

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

      var homeDirectory = Properties.Settings.Default.homeDirectory;
      var badip_file = Path.Combine(homeDirectory, "badip.txt");
      var goodip_file = Path.Combine(homeDirectory, "goodip.txt");
      var rbl = new List<string>();
      if (Properties.Settings.Default.useDnsRbl)
      {
        rbl.Add(Properties.Settings.Default.dnsRbl);
      }
      this.bl = new Blacklist(badip_file, goodip_file, rbl);
      this.connections = new Dictionary<string, List<DateTime>>();
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

    /** Updates the banned state. */
    bool ShouldBeBanned(String ip)
    {
      if (!Properties.Settings.Default.autoban)
      {
        return false;
      }
      if (Properties.Settings.Default.banSeconds == 0 || Properties.Settings.Default.banSessions == 0)
      {
        return false;
      }


      List<DateTime> value;
      if (connections.TryGetValue(ip, out value))
      {
        var s = TimeSpan.FromSeconds(Properties.Settings.Default.banSeconds);
        value.RemoveAll(x => DateTime.Now.Subtract(x) > s);
        value.Add(DateTime.Now);
        connections[ip] = value;

        var c = "Current Sessions: " + value.Count + 
                ": {" + string.Join(",", value.ToArray()) + "}";
        OnStatusMessageUpdated(c, StatusMessageEventArgs.MessageType.Status);

        // We should ban if we still have more connections than allowed.
        var banCount = Properties.Settings.Default.banSessions;
        return value.Count > banCount;
      }
      else
      {
        value = new List<DateTime>();
        value.Add(DateTime.Now);
        connections[ip] = value;
        return false;
      }
    }

    private void send(Socket socket, string s)
    {
      byte[] bytes = System.Text.Encoding.ASCII.GetBytes(s.ToCharArray());
      socket.Send(bytes);
    }

    private string receive(Socket socket)
    {
      var a = socket.Available;
      if (a == 0)
      {
        return "";
      }

      a = Math.Max(a, 255);
      byte[] buf = new byte[a];
      var received = socket.Receive(buf);
      return Encoding.ASCII.GetString(buf, 0, received);
    }

    private void SendBusyAndCloseSocket(Socket socket)
    {
      // Send BUSY signal.
      OnStatusMessageUpdated("Sending Busy Signal.", StatusMessageEventArgs.MessageType.Status);
      try
      {
        send(socket, "BUSY");
      }
      finally
      {
        socket.Close();
      }
    }

    private void BlackListIP(Socket socket, string ip)
    {
      Thread.Sleep(1000);
      SendBusyAndCloseSocket(socket);
      if (bl.BlacklistIP(ip))
      {
        OnStatusMessageUpdated("Blacklisting IP: " + ip, StatusMessageEventArgs.MessageType.LogInfo);
      }
      else
      {
        Console.WriteLine("Error Blacklisting IP: " + ip);
      }
      return;
    }

    private void Run()
    {
      server = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
      server.Bind(new IPEndPoint(IPAddress.Any, port));
      server.Listen(4);

      while (true)
      {
        Console.WriteLine("Waiting for connection.", StatusMessageEventArgs.MessageType.LogInfo);
        try
        {
          Socket socket = server.Accept();
          string ip = ((System.Net.IPEndPoint)socket.RemoteEndPoint).Address.ToString();
          Console.WriteLine("After accept from IP: " + ip);
          OnStatusMessageUpdated(name + " from " + ip, StatusMessageEventArgs.MessageType.Connect);

          if (ShouldBeBanned(ip))
          {
            // Add it to the blacklist file.
            BlackListIP(socket, ip);
            continue;
          }

          if (bl.IsBlackListed(ip))
          {
            OnStatusMessageUpdated("Attempt from blacklisted IP.", StatusMessageEventArgs.MessageType.LogInfo);
            SendBusyAndCloseSocket(socket);
            continue;
          }

          // Since we don't terminate SSH, we can't do this for SSH connections.
          if (Properties.Settings.Default.pressEsc && name.Equals("Telnet"))
          {
            send(socket, "CONNECT 2400\r\nWWIV-Server\r\nPress <ESC> twice for the BBS..\r\n");
            int numEscs = 0;
            string total = "";
            while (true)
            {
              string s = receive(socket);
              if (s.Length == 0)
              {
                continue;
              }
              total += s;
              numEscs += s.Split((char)27).Length - 1;
              if (numEscs >= 2)
              {
                send(socket, "Launching BBS...\r\n");
                break;
              }
              if (total.Contains("root") || total.Contains("admin"))
              {
                BlackListIP(socket, ip);
                continue;
              }
            }
          }

          // Grab a node # after we've cleared everything else.
          NodeStatus node = nodeManager.getNextNode();
          if (node == null)
          {
            // NO node available.
            OnStatusMessageUpdated("No node available.", StatusMessageEventArgs.MessageType.LogInfo);
            SendBusyAndCloseSocket(socket);
            continue;
          }
          node.RemoteAddress = ip;
          OnStatusMessageUpdated("Launching Node #" + node.Node, StatusMessageEventArgs.MessageType.LogInfo);
          Thread instanceThread = new Thread(() => LaunchInstance(node, socket));
          instanceThread.Name = "Instance #" + node.Node;
          instanceThread.Start();
          OnNodeUpdated(node);
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
        using (Process p = launcher.launchSocketNode(node.Node, socketHandle))
        {
          if (p != null)
          {
            p.WaitForExit();
          }
        }
      }
      catch (Exception e)
      {
        Console.WriteLine(e.ToString());
      }
      finally
      {
        try
        {
          // Let's try to shutdown in a try..catch incase it's already closed.
          socket.Shutdown(SocketShutdown.Both);
          socket.Close();
        }
        catch (SocketException e)
        {
          Console.WriteLine(e.ToString());
        }
        finally
        {
          nodeManager.freeNode(node);
          OnNodeUpdated(node);
        }
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