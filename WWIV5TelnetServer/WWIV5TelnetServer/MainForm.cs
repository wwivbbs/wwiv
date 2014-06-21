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
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Threading;
using System.Diagnostics;

namespace WWIV5TelnetServer
{
    public partial class MainForm : Form
    {
        private TelnetServer server = new TelnetServer();
    
        public MainForm()
        {
            InitializeComponent();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            server.StatusMessageChanged += server_StatusMessage;
            server.NodeStatusChanged += server_NodeStatusChanged;
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
              if (notifyIcon1.Visible && Properties.Settings.Default.useBalloons)
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
            if (Properties.Settings.Default.autostart)
            {
                Console.WriteLine("AutoStarting server.");
                startToolStripMenuItem_Click(sender, e);
            }
        }

        private void runLocalNodeToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.runLocalNodeToolStripMenuItem.Enabled = false;

            var executable = Properties.Settings.Default.executable;
            var argumentsTemplate = Properties.Settings.Default.parameters;
            var homeDirectory = Properties.Settings.Default.homeDirectory;

            Action<string> logger = delegate(string s) { server_StatusMessage(this, new StatusMessageEventArgs(s)); };
            Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, logger);
            Process p = launcher.launchLocalNode(Convert.ToInt32(Properties.Settings.Default.localNode));
            Thread localNodeCleanupThread = new Thread(delegate()
            {
                p.WaitForExit();
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

    }
}
