using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace WWIV5TelnetServer
{
  internal class Controller
  {
    public static string WWIV_Version;
    public static string WWIV_Build;

    private NodeManager nodeManager;
    private SocketServer serverTelnet;
    private SocketServer serverSSH;

    private Form form_;
    private ListBox listBoxNodes_;
    private TextBox messages_;
    private NotifyIcon balloon_;

    internal Controller(Form f, ListBox l, TextBox t, NotifyIcon n)
    {
      form_ = f;
      listBoxNodes_ = l;
      messages_ = t;
      balloon_ = n;
    }

    internal void Initalize()
    {
      var lowNode = Convert.ToInt32(Properties.Settings.Default.startNode);
      var highNode = Convert.ToInt32(Properties.Settings.Default.endNode);
      nodeManager = new NodeManager(lowNode, highNode);

      var portTelnet = Convert.ToInt32(Properties.Settings.Default.port);
      var argumentsTemplateTelnet = Properties.Settings.Default.parameters;
      var portSSH = Convert.ToInt32(Properties.Settings.Default.portSSH); // SSH
      var argumentsTemplateSSH = Properties.Settings.Default.parameters2; // SSH
      serverTelnet = new SocketServer(nodeManager, portTelnet, argumentsTemplateTelnet, "Telnet");
      serverSSH = new SocketServer(nodeManager, portSSH, argumentsTemplateSSH, "SSH"); // SSH


      serverTelnet.StatusMessageChanged += server_StatusMessage;
      serverTelnet.NodeStatusChanged += server_NodeStatusChanged;
      serverSSH.StatusMessageChanged += server_StatusMessage; // SSH
      serverSSH.NodeStatusChanged += server_NodeStatusChanged; // SSH
    }


    private void server_NodeStatusChanged(object sender, NodeStatusEventArgs e)
    {
      Console.WriteLine("server_NodeStatusChanged");
      // TODO(rushfan): Build list of strings for model on this side.
      Action update = delegate ()
      {
        // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
        listBoxNodes_.Items.Clear();
        listBoxNodes_.Items.AddRange(nodeManager.Nodes.ToArray());
      };
      if (form_.InvokeRequired)
      {
        form_.Invoke(update);
      }
      else
      {
        update();
      }
    }

    void server_StatusMessage(object sender, StatusMessageEventArgs e)
    {
      var message = String.Format("{0}: {1}", DateTime.Now.ToString(), e.Message);
      Console.WriteLine(message);
      Console.WriteLine("Properties.Settings.Default.useBalloons: " + Properties.Settings.Default.useBalloons);
      Action a = delegate ()
      {
        messages_.AppendText(message + "\r\n");
        if (balloon_.Visible && Properties.Settings.Default.useBalloons && e.IsConnectionRelated())
        {
          balloon_.BalloonTipText = e.Message;
          balloon_.ShowBalloonTip(5000);
        }
      };
      if (this.messages_.InvokeRequired)
      {
        form_.Invoke(a);
      }
      else
      {
        a();
      }
    }

    private String FetchBbsVersion(out String version, out String build)
    {
      string currentFullVersion = "WWIV Server";
      version = "5.2.0.unknown";
      build = "0";
      // Fetch Current WWIV Version And Build Number.
      // It's ok if it fails.
      var bbsExe = Properties.Settings.Default.executable;

      if (!File.Exists(bbsExe))
      {
        // Don't try to execute bbs.exe if it does not exist.
        return currentFullVersion;
      }
      try
      {
        using (Process p = new Process())
        {
          p.StartInfo.FileName = bbsExe;
          p.StartInfo.Arguments = "-V";
          p.StartInfo.UseShellExecute = false;
          p.StartInfo.RedirectStandardOutput = true;
          p.Start();
          var output = p.StandardOutput.ReadToEnd();
          p.WaitForExit();
          char[] delims = { '[', '.', ']' };
          var partsVersion = output.Split(delims);
          var majorVersion = partsVersion[1];
          var minorVersion = partsVersion[2];
          var minorVersion2 = partsVersion[3];
          build = partsVersion[4];
          version = (majorVersion + "." + minorVersion + "." + minorVersion2 + "." + build);
          currentFullVersion = "WWIV Server - Running WWIV: " + version;
        }
      }
      catch
      {
        // Ignore error and return the default currentFullVersion.
      }
      return currentFullVersion;
    }

    public void Start()
    {
      serverTelnet.Start();
      serverSSH.Start();

      // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
      listBoxNodes_.Items.Clear();
      listBoxNodes_.Items.AddRange(nodeManager.Nodes.ToArray());
    }

    public void Stop()
    {
      serverTelnet.Stop();
      serverSSH.Stop();

      // Clear the list of nodes in the list.
      listBoxNodes_.Items.Clear();
      balloon_.Text = "WWIV Server: Offline";
    }

    public void RunBinkP()
    {
      Console.WriteLine("AutoStarting WWIVNet.");
      ProcessStartInfo binkP = new ProcessStartInfo("binkp.cmd");
      binkP.WindowStyle = ProcessWindowStyle.Minimized;
      Process.Start(binkP);
    }

    public void RunLocalNode(Action done)
    {
      var executable = Properties.Settings.Default.executable;
      var argumentsTemplate = Properties.Settings.Default.parameters;
      var homeDirectory = Properties.Settings.Default.homeDirectory;

      Action<string> logger = delegate (string s)
      {
        server_StatusMessage(this, new StatusMessageEventArgs(s, StatusMessageEventArgs.MessageType.LogDebug));
      };
      Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, logger);
      Process p = launcher.launchLocalNode(Convert.ToInt32(Properties.Settings.Default.localNode));
      Thread localNodeCleanupThread = new Thread(delegate ()
      {
        if (p != null)
        {
          p.WaitForExit();
        }
        done();
      });
      // Set this to be a background thread so we can still exit and not wait for it.
      localNodeCleanupThread.IsBackground = true;
      localNodeCleanupThread.Name = "Local Node Cleanup Thread";
      localNodeCleanupThread.Start();
    }

    public void CheckForUpdates()
    {
      try
      {
        // Set Main Wiwndow Title and update global strings.
        string text = FetchBbsVersion(out WWIV_Version, out WWIV_Build);

        // HERE FOR UPDATE
        CheckUpdates instance = new CheckUpdates();
        instance.UpdateHeartbeat();
      }
      catch (Exception ex)
      {
        MessageBox.Show("The Following Error Occured: " + ex);
      }
    }

    public void LaunchBugReport()
    {
      // Launch GitHub Issues Page into Default Browser
      string wwivIssues = "https://github.com/wwivbbs/wwiv/issues";
      Process.Start(wwivIssues);
    }

    public void LaunchDocs()
    {
      // Launch WWIV Online Documents into Default Browser
      string wwivDocs = "http://docs.wwivbbs.org";
      Process.Start(wwivDocs);
    }

    public void ShowLogs()
    {
      LogForm wwivLogs = new LogForm();
      wwivLogs.ShowDialog();
    }
  }
}
