/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2014-2016 WWIV Software Services              */
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
using System.Windows.Forms;
using System.Threading;
using System.Diagnostics;
using System.Net;
using System.Text.RegularExpressions;

namespace WWIV5TelnetServer
{
    public partial class MainForm : Form
    {
        private TelnetServer server = new TelnetServer();
        //private BeginDayHandler beginDay;
    
        public MainForm()
        {
            InitializeComponent();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            notifyIcon1.Visible = false;
            server.StatusMessageChanged += server_StatusMessage;
            server.NodeStatusChanged += server_NodeStatusChanged;

            Action<string> logger = delegate (string s)
            {
                server_StatusMessage(this, new StatusMessageEventArgs(s, StatusMessageEventArgs.MessageType.LogInfo));
            };
        }

        private void server_NodeStatusChanged(object sender, NodeStatusEventArgs e)
        {
            Console.WriteLine("server_NodeStatusChanged");
            // TODO(rushfan): Build list of strings for model on this side.
            Action update = delegate()
            {
                // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
                listBoxNodes.Items.Clear();
                listBoxNodes.Items.AddRange(server.Nodes.ToArray());
            };
            if (InvokeRequired)
            {
                this.Invoke(update);
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
            Action a = delegate() { 
              messages.AppendText(message + "\r\n");
              if (notifyIcon1.Visible && Properties.Settings.Default.useBalloons && e.IsConnectionRelated())
              { 
                notifyIcon1.BalloonTipText = e.Message;
                notifyIcon1.ShowBalloonTip(5000);
              }
            };
            if (this.messages.InvokeRequired)
            {
                this.Invoke(a);
            }
            else
            {
                a();
            }
        }

        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
             MessageBox.Show("WWIV5TelnetServer/C# (experimental)");
        }

        private void startToolStripMenuItem_Click(object sender, EventArgs e)
        {
            server.Start();
            // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
            listBoxNodes.Items.Clear();
            listBoxNodes.Items.AddRange(server.Nodes.ToArray());
            startToolStripMenuItem.Enabled = false;
            stopToolStripMenuItem.Enabled = true;
            preferencesToolStripMenuItem.Enabled = false;
            notifyIcon1.Text = "WWIV Telnet Server: Active";
        }

        private void stopToolStripMenuItem_Click(object sender, EventArgs e)
        {
            listBoxNodes.DataSource = null;
            server.Stop();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            preferencesToolStripMenuItem.Enabled = true;
            // Clear the list of nodes in the list.
            listBoxNodes.Items.Clear();
            notifyIcon1.Text = "WWIV Telnet Server: Offline";
        }

        private void preferencesToolStripMenuItem_Click(object sender, EventArgs e)
        {
            PreferencesDialog prefs = new PreferencesDialog();
            prefs.ShowDialog();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void MainForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            Console.WriteLine("MainForm_FormClosing");
            server.Stop();
        }

        private void MainForm_Load(object sender, EventArgs e)
        {
            notifyIcon1.Text = "WWIV Telnet Server: Offline";
            // Launch Telnet Server on Startup
            if (Properties.Settings.Default.autostart)
            {
                Console.WriteLine("AutoStarting Server.");
                startToolStripMenuItem_Click(sender, e);
            }
            // Launch Local Node of Startup
            if (Properties.Settings.Default.launchLocalNodeAtStartup)
            {
                Console.WriteLine("AutoStarting Local Node.");
                runLocalNodeToolStripMenuItem_Click(sender, e);
            }
            // Launch binkp.cmd for WWIVNet on Startup
            if (Properties.Settings.Default.launchNetworkAtStartup)
            {
                Console.WriteLine("AutoStarting WWIVNet.");
                ProcessStartInfo binkP = new ProcessStartInfo("binkp.cmd");
                binkP.WindowStyle = ProcessWindowStyle.Minimized;
                Process.Start(binkP);
            }

            // Get Current Version
            Process p = new Process();
            
            // Uncomment For Release Build
            p.StartInfo.FileName = "bbs.exe";
            
            // Uncomment For Debuging
            //p.StartInfo.FileName = @"C:\wwiv\bbs.exe";
            p.StartInfo.Arguments = "-V";
            p.StartInfo.UseShellExecute = false;
            p.StartInfo.RedirectStandardOutput = true;
            p.Start();

            string output = p.StandardOutput.ReadToEnd();
            p.WaitForExit();
            char[] delimiter = { '[', '.', ']' };
            string currentVersion = output;
            string[] partsVersion;
            partsVersion = currentVersion.Split(delimiter);
            string majorVersion = partsVersion[1];
            string minorVersion = partsVersion[2];
            string buildVersion = partsVersion[3];
            string revisVersion = partsVersion[4];
            string displayVersion;
            displayVersion = (majorVersion + "." + minorVersion + "." + buildVersion + "." + revisVersion);

            string currentFullVersion;
            currentFullVersion = "WWIV5 Telnet Server - Running WWIV: " + displayVersion;

            this.Text = currentFullVersion;

            // Check For New Version
            // Declare And Initilize Set Build Number Variables to 0
            string wwivBuild5_1 = "0";

            // Fetch Latest Build Number For WWIV 5.1
            WebClient wc = new WebClient();
            string htmlString1 = wc.DownloadString("http://build.wwivbbs.org/jenkins/job/wwiv/lastSuccessfulBuild/label=windows/");
            Match mTitle1 = Regex.Match(htmlString1, "(?:number.*?>)(?<buildNumber1>.*?)(?:<)");
            {
                wwivBuild5_1 = mTitle1.Groups[1].Value;
            }
            string newestVersion;
            newestVersion = wwivBuild5_1;

            int newBuild = Int32.Parse(newestVersion);
            int oldBuild = Int32.Parse(revisVersion);

            if (newBuild > oldBuild)
            {
                MessageBox.Show("A Newer Version of WWIV is Available!");
                // TODO Launch WWIV Update Once Packaged with Distribution.
            }
        }

        private void runLocalNodeToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.runLocalNodeToolStripMenuItem.Enabled = false;

            var executable = Properties.Settings.Default.executable;
            var argumentsTemplate = Properties.Settings.Default.parameters;
            var homeDirectory = Properties.Settings.Default.homeDirectory;

            Action<string> logger = delegate(string s) { 
              server_StatusMessage(this, new StatusMessageEventArgs(s, StatusMessageEventArgs.MessageType.LogDebug)); 
            };
            Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, logger);
            Process p = launcher.launchLocalNode(Convert.ToInt32(Properties.Settings.Default.localNode));
            Thread localNodeCleanupThread = new Thread(delegate()
            {
                p.WaitForExit();
                this.Name = "localNodeCleanupThread";
                MethodInvoker d = delegate() { this.runLocalNodeToolStripMenuItem.Enabled = true; };
                this.Invoke(d);                
            });
            // Set this to be a background thread so we can still exit and not wait for it.
            localNodeCleanupThread.IsBackground = true;
            localNodeCleanupThread.Start();
        }

        private void MainForm_Resize(object sender, EventArgs e)
        {
          notifyIcon1.BalloonTipTitle = "WWIV Telnet Server";
          notifyIcon1.BalloonTipText = "Double click to reopen application.";
          notifyIcon1.Text = "WWIV Telnet Server";

          if (FormWindowState.Minimized == this.WindowState)
          {
            notifyIcon1.Visible = true;
            notifyIcon1.ShowBalloonTip(500);
            this.Hide();
          }
          else if (FormWindowState.Normal == this.WindowState)
          {
            notifyIcon1.Visible = false;
          }
        }

        private void notifyIcon1_MouseDoubleClick(object sender, MouseEventArgs e)
        {
          this.Show();
          this.WindowState = FormWindowState.Normal;
        }

        private void submitBugIssueToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Launch GitHub Issues Page into Default Browser
            string wwivIssues = "https://github.com/wwivbbs/wwiv/issues";
            Process.Start(wwivIssues);
        }
    }
}
