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
    private Controller controller_;

    public MainForm()
    {
      InitializeComponent();
      startToolStripMenuItem.Enabled = true;
      startTb.Enabled = true;
      stopToolStripMenuItem.Enabled = false;
      stopTb.Enabled = false;
      notifyIcon1.Visible = false;

      controller_ = new Controller(this, listBoxNodes, messages, notifyIcon1);
      controller_.Initalize();
    }


    private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
    {
      // Get Curent Version
      var ver = Assembly.GetExecutingAssembly().GetName().Version.ToString();
      MessageBox.Show("WWIV Server " + ver,
          "About WWIV Server", MessageBoxButtons.OK);
    }

    private void startToolStripMenuItem_Click(object sender, EventArgs e)
    {
      controller_.Start();
      
      startToolStripMenuItem.Enabled = false;
      startTb.Enabled = false;
      stopToolStripMenuItem.Enabled = true;
      stopTb.Enabled = true;
      preferencesToolStripMenuItem.Enabled = false;
      prefsTb.Enabled = false;
      notifyIcon1.Text = "WWIV Server: Active";
    }

    private void stopToolStripMenuItem_Click(object sender, EventArgs e)
    {
      listBoxNodes.DataSource = null;
      controller_.Stop();
      startToolStripMenuItem.Enabled = true;
      startTb.Enabled = true;
      stopToolStripMenuItem.Enabled = false;
      stopTb.Enabled = false;
      preferencesToolStripMenuItem.Enabled = true;
      prefsTb.Enabled = true;
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
      controller_.Stop();
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
        controller_.RunBinkP();
      }
      controller_.CheckForUpdates();
    }

    private void runLocalNodeToolStripMenuItem_Click(object sender, EventArgs e)
    {
      this.runLocalNodeToolStripMenuItem.Enabled = false;

      Action done = () => {
        MethodInvoker d = delegate () { this.runLocalNodeToolStripMenuItem.Enabled = true; };
        this.Invoke(d);
      };
      controller_.RunLocalNode(done);
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
      controller_.LaunchBugReport();
    }

    private void wWIVOnlineDocumentsToolStripMenuItem_Click(object sender, EventArgs e)
    {
      controller_.LaunchDocs();
    }

    private void viewWWIVLogsToolStripMenuItem_Click(object sender, EventArgs e)
    {
      controller_.ShowLogs();
    }

    private void toolStripButton1_Click(object sender, EventArgs e)
    {
      startToolStripMenuItem_Click(sender, e);
    }

    private void toolStripButton2_Click(object sender, EventArgs e)
    {
      stopToolStripMenuItem_Click(sender, e);
    }

    private void toolStripButton3_Click(object sender, EventArgs e)
    {
      preferencesToolStripMenuItem_Click(sender, e);
    }

    private void runLocalTb_Click(object sender, EventArgs e)
    {
      runLocalNodeToolStripMenuItem_Click(sender, e);
    }

    private void aboutTb_Click(object sender, EventArgs e)
    {
      aboutToolStripMenuItem_Click(sender, e);
    }
  }
}
