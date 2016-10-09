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
    private NodeType nodeType;
    private Blacklist bl;
    private Dictionary<String, List<DateTime>> connections;

    public delegate void StatusMessageEventHandler(object sender, StatusMessageEventArgs e);
    public event StatusMessageEventHandler StatusMessageChanged;

    public delegate void NodeStatusEventHandler(object sender, NodeStatusEventArgs e);
    public event NodeStatusEventHandler NodeStatusChanged;

    public SocketServer(NodeManager nodeManager, Int32 port, string argsTemplate, string name, NodeType nodeType)
    {
      this.nodeManager = nodeManager;
      this.port = port;
      this.argumentsTemplate = argsTemplate;
      this.name = name;
      this.nodeType = nodeType;

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
      launcherThread = new Thread(() => { Run(); Log("Server Thread Ended.");  });
      launcherThread.Name = this.name;
      launcherThread.Start();
      Log(this.name + " Server Started");
    }

    public void Stop()
    {
      if (launcherThread == null)
      {
        DebugLog("ERROR: LauncherThread was never set.");
        return;
      }
      Log(String.Format("Stopping {0} Server.", this.name));
      if (server != null)
      {
        server.Close();
        server = null;
      }
      launcherThread.Abort();
      // Causes a deadlock with updating the main form.
      // launcherThread.Join();
      launcherThread = null;
      Log(String.Format("{0} Server Stopped", this.name));
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

      if (bl.IsWhiteListed(ip))
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

    private int GetCountryCode(string ip)
    {
      var server = Properties.Settings.Default.dnsCC;
      CountryCodeIP cip = new CountryCodeIP(server, ip);
      return cip.Get();
    }

    private bool IsCountryBanned(int country)
    {
      if (country == 0)
      {
        return false;
      }
      var countries = Properties.Settings.Default.badCountries;
      foreach (var b in countries)
      {
        int current;
        if (Int32.TryParse(b.ToString(), out current))
        {
          if (current != 0 && current == country)
          {
            return true;
          }
        }
      }
      return false;
    }

    private void send(Socket socket, string s)
    {
      try
      {
        byte[] bytes = System.Text.Encoding.ASCII.GetBytes(s.ToCharArray());
        socket.Send(bytes);
      }
      catch (SocketException e)
      {
        Debug.WriteLine(e.ToString());
      }
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
      SendMessageAndCloseSocket(socket, "BUSY");
    }

    private void SendMessageAndCloseSocket(Socket socket, string message)
    {
      // Send BUSY signal.
      OnStatusMessageUpdated("Sending Busy Signal.",
          StatusMessageEventArgs.MessageType.Status);
      bool needClose = true;
      try
      {
        send(socket, message);
      }
      catch (ObjectDisposedException)
      {
        // Socket is already disposed, no need to close it.
        needClose = false;
      }
      finally
      {
        if (needClose)
        {
          socket.Close();
        }
      }
    }

    private void BlackListIP(Socket socket, string ip)
    {
      if (bl.BlacklistIP(ip))
      {
        Log("Blacklisting IP: " + ip);
      }
      else
      {
        DebugLog("Error Blacklisting IP: " + ip);
      }
      SendBusyAndCloseSocket(socket);
    }

    private bool DoMailerLoop(Socket socket, string ip)
    {
      send(socket, "Press <ESC> twice for the BBS..\r\n");
      int numEscs = 0;
      string total = "";
      var startTime = DateTime.Now;
      var timeout = TimeSpan.FromSeconds(10);
      while (DateTime.Now - startTime < timeout)
      {
        string s = receive(socket);
        if (!socket.Connected)
        {
          // Lost connection.
          return false;
        }
        if (s.Length == 0)
        {
          Thread.Sleep(1000);
          send(socket, ".");
          continue;
        }
        total += s;
        numEscs += s.Split((char)27).Length - 1;
        if (numEscs >= 2)
        {
          send(socket, "Launching BBS...\r\n");
          return true;
        }
        if (total.Contains("root") || total.Contains("admin"))
        {
          Log("Received ROOT or ADMIN login ,blacklisting...");
          SendBusyAndCloseSocket(socket);
          BlackListIP(socket, ip);
          return false;
        }
      }
      return true;
    }

    private bool CanConnect(Socket socket)
    {
      try
      {
        string ip = ((System.Net.IPEndPoint)socket.RemoteEndPoint).Address.ToString();

        if (bl.IsWhiteListed(ip))
        {
          return true;
        }

        if (bl.IsBlackListed(ip))
        {
          Log("Attempt from blacklisted IP: " + ip);
          SendBusyAndCloseSocket(socket);
          return false;
        }

        if (ShouldBeBanned(ip))
        {
          // Add it to the blacklist file.
          BlackListIP(socket, ip);
          SendBusyAndCloseSocket(socket);
          return false;
        }

        var countryCode = GetCountryCode(ip);
        if (IsCountryBanned(countryCode))
        {
          Log("Blocking connection from banned country code: " + countryCode);
          SendBusyAndCloseSocket(socket);
          return false;
        }
        else
        {
          Log("IP from country code:" + countryCode);
        }

        var savedTimeout = socket.ReceiveTimeout;
        socket.ReceiveTimeout = 5000;
        // Since we don't terminate SSH, we can't do this for SSH connections.
        if (Properties.Settings.Default.pressEsc && this.name.Equals("Telnet"))
        {
          send(socket, "CONNECT 2400\r\nWWIV - Server\r\n");
          if (!DoMailerLoop(socket, ip))
          {
            SendBusyAndCloseSocket(socket);
            return false;
          }
        }
        socket.ReceiveTimeout = savedTimeout;

        if (!socket.Connected)
        {
          // NO node available.
          Log("nobody home.");
          SendBusyAndCloseSocket(socket);
          return false;
        }
        return true;
      }
      catch (SocketException e)
      {
        Debug.WriteLine(e.ToString());
        return true;
      }
      catch (Exception e)
      {
        Debug.WriteLine(e.ToString());
        return true;
      }
    }

    private void Run()
    {
      server = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
      server.Bind(new IPEndPoint(IPAddress.Any, port));
      server.Listen(4);

      while (true)
      {
        Debug.WriteLine("Waiting for connection.");
        try
        {
          Socket socket = server.Accept();
          string ip = ((System.Net.IPEndPoint)socket.RemoteEndPoint).Address.ToString();
          Debug.WriteLine("After accept from IP: " + ip);
          OnStatusMessageUpdated(this.name + " from " + ip, StatusMessageEventArgs.MessageType.Connect);

          if (!CanConnect(socket))
          {
            continue;
          }

          // Grab a node # after we've cleared everything else.
          NodeStatus node = nodeManager.getNextNode(this.nodeType);
          if (node == null)
          {
            // NO node available.
            Log("No node available.");
            SendBusyAndCloseSocket(socket);
            continue;
          }

          node.RemoteAddress = ip;
          Thread instanceThread = new Thread(() => LaunchInstance(node, socket));
          StringBuilder n = new StringBuilder();
          if (node.NodeType == NodeType.BBS)
          {
            n.Append("Instance #").Append(node.Node);
          } else if (node.NodeType == NodeType.BINKP)
          {
            n.Append("BinkP");
          }
          instanceThread.Name = n.ToString();
          instanceThread.Start();
          OnNodeUpdated(node);
        }
        catch (SocketException e)
        {
          DebugLog("Exception" + e.ToString());
        }
        catch (ThreadAbortException)
        {
          Debug.WriteLine("Server Exiting normally...");
          return;
        }
        catch (Exception e)
        {
          DebugLog("Exception" + e.ToString());
        }
      }
    }

    private void LaunchInstance(NodeStatus node, Socket socket)
    {
      try
      {
        var executable = (node.NodeType == NodeType.BBS)
          ? Properties.Settings.Default.executable : Properties.Settings.Default.binkpExecutable;

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
        DebugLog(e.ToString());
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
          Debug.WriteLine(e.ToString());
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

    protected virtual void Log(string message)
    {
      OnStatusMessageUpdated(message, StatusMessageEventArgs.MessageType.LogInfo);
    }

    protected virtual void DebugLog(string message)
    {
      Debug.WriteLine(message);
      Console.WriteLine(message);
      OnStatusMessageUpdated(message, StatusMessageEventArgs.MessageType.LogDebug);
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