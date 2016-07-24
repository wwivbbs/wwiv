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
using System.IO;
using System.Windows.Forms;
using System.Threading;
using System.Diagnostics;
using System.Reflection;

namespace WWIV5TelnetServer
{
    public partial class MainForm : Form
    {
        private NodeManager nodeManager;
        private SocketServer serverTelnet;
        private SocketServer serverSSH; // SSH
        //private BeginDayHandler beginDay;

        // Global Strings
        public static string Telnet_Version;
        public static string WWIV_Version;
        public static string WWIV_Build;

        public MainForm()
        {
            InitializeComponent();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            notifyIcon1.Visible = false;

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

            Action<string> logger = delegate (string s)
            {
                server_StatusMessage(this, new StatusMessageEventArgs(s, StatusMessageEventArgs.MessageType.LogInfo));
            };

        }

        private void server_NodeStatusChanged(object sender, NodeStatusEventArgs e)
        {
            Console.WriteLine("server_NodeStatusChanged");
            // TODO(rushfan): Build list of strings for model on this side.
            Action update = delegate ()
            {
                // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
                listBoxNodes.Items.Clear();
                listBoxNodes.Items.AddRange(nodeManager.Nodes.ToArray());
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
            Action a = delegate ()
            {
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
            // Get Curent WWIV5TelnetServer Version
            Telnet_Version = Assembly.GetExecutingAssembly().GetName().Version.ToString();
            MessageBox.Show("WWIV5TelnetServer v" + Telnet_Version + "\r\n \r\nBuilt In Microsoft Visual Studio C# | 2015", "About WWIV5TelnetServer", MessageBoxButtons.OK);
        }

        private void startToolStripMenuItem_Click(object sender, EventArgs e)
        {
            serverTelnet.Start();
            serverSSH.Start(); // SSH
            // Hack. can't figure out C# databinding. According to stackoverflow, many others can't either.
            listBoxNodes.Items.Clear();
            listBoxNodes.Items.AddRange(nodeManager.Nodes.ToArray());
            startToolStripMenuItem.Enabled = false;
            stopToolStripMenuItem.Enabled = true;
            preferencesToolStripMenuItem.Enabled = false;
            notifyIcon1.Text = "WWIV Server: Active";
        }

        private void stopToolStripMenuItem_Click(object sender, EventArgs e)
        {
            listBoxNodes.DataSource = null;
            serverTelnet.Stop();
            serverSSH.Stop(); // SSH
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            preferencesToolStripMenuItem.Enabled = true;
            // Clear the list of nodes in the list.
            listBoxNodes.Items.Clear();
            notifyIcon1.Text = "WWIV Server: Offline";
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
            serverTelnet.Stop();
            serverSSH.Stop(); // SSH
        }

        private String FetchBbsVersion(out String version, out String build)
        {
            string currentFullVersion = "WWIV Server";
            version = "5.1.0.unknown";
            build = "0";
            // Fetch Current WWIV Version And Build Number.
            // It's ok if it fails.
            var bbsExe = Properties.Settings.Default.executable;

            if (!File.Exists(bbsExe))
            {
                // Don't try to execute bbs.exe if it does not exist.
                return currentFullVersion;
            }
            Process p = new Process();
            try
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
                currentFullVersion = "WWIV5 Telnet Server - Running WWIV: " + version;

            }
            catch
            {
                // Ignore error and return the default currentFullVersion.
            }
            finally
            {
                p.Close();
            }
            return currentFullVersion;
        }

        public void MainForm_Load(object sender, EventArgs e)
        {
            notifyIcon1.Text = "WWIV Server: Offline";
            // Launch Server on Startup
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

            try
            {
                // Set Main Wiwndow Title and update global strings.
                Text = FetchBbsVersion(out WWIV_Version, out WWIV_Build);

                // HERE FOR UPDATE
                CheckUpdates instance = new CheckUpdates();
                instance.UpdateHeartbeat();
            }
            catch (Exception ex)
            {
                MessageBox.Show("The Following Error Occured: " + ex);
            }
        }

        private void runLocalNodeToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.runLocalNodeToolStripMenuItem.Enabled = false;

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
                this.Name = "localNodeCleanupThread";
                MethodInvoker d = delegate () { this.runLocalNodeToolStripMenuItem.Enabled = true; };
                this.Invoke(d);
            });
            // Set this to be a background thread so we can still exit and not wait for it.
            localNodeCleanupThread.IsBackground = true;
            localNodeCleanupThread.Start();
        }

        private void MainForm_Resize(object sender, EventArgs e)
        {
            notifyIcon1.BalloonTipTitle = "WWIV Server";
            notifyIcon1.BalloonTipText = "Double click to reopen application.";
            notifyIcon1.Text = "WWIV Server";

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

        private void wWIVOnlineDocumentsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Launch WWIV Online Documents into Default Browser
            string wwivDocs = "http://docs.wwivbbs.org";
            Process.Start(wwivDocs);
        }

        private void viewWWIVLogsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            LogForm wwivLogs = new LogForm();
            wwivLogs.ShowDialog();
        }
    }
}
