/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright © 2014-2016 WWIV Software Services               */
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
using System.IO.Compression;
using System.Net;
using System.Diagnostics;
using System.Windows.Forms;
using System.ComponentModel;

namespace windows_wwiv_update
{
    public partial class Form2 : Form
    {
        // BackgroundWorker Event
        BackgroundWorker m_oWorker;

        public Form2(string fetchVersion)
        {
            // Fetch Version
            InitializeComponent();
            label2.Text = fetchVersion;

            m_oWorker = new BackgroundWorker();

            // BackgroundWorker Properties
            m_oWorker.DoWork += new DoWorkEventHandler(m_oWorker_DoWork);
            m_oWorker.ProgressChanged += new ProgressChangedEventHandler
                    (m_oWorker_ProgressChanged);
            m_oWorker.RunWorkerCompleted += new RunWorkerCompletedEventHandler
                    (m_oWorker_RunWorkerCompleted);
            m_oWorker.WorkerReportsProgress = true;
            m_oWorker.WorkerSupportsCancellation = true;

            // Update UI Cosmetics
            label1.Visible = false;
            label2.Visible = false;
            button3.Visible = false;
            button5.Visible = false;
            progressBar1.Visible = false;
            activeStatus.Visible = false;

            // Check For Running Instances Of WWIV Programs
            if (Process.GetProcessesByName("bbs").Length >= 1)
            {
                wwivStatus.ForeColor = System.Drawing.Color.Red;
                wwivStatus.Text = "ONLINE";
            }
            else
            {
                wwivStatus.ForeColor = System.Drawing.Color.Green;
                wwivStatus.Text = "OFFLINE";
            }
            if (Process.GetProcessesByName("WWIV5TelnetServer").Length >= 1)
            {
                telnetStatus.ForeColor = System.Drawing.Color.Red;
                telnetStatus.Text = "ONLINE";
            }
            else
            {
                telnetStatus.ForeColor = System.Drawing.Color.Green;
                telnetStatus.Text = "OFFLINE";
            }
            if (Process.GetProcessesByName("networkb").Length >= 1)
            {
                netStatus.ForeColor = System.Drawing.Color.Red;
                netStatus.Text = "ONLINE";
            }
            else
            {
                netStatus.ForeColor = System.Drawing.Color.Green;
                netStatus.Text = "OFFLINE";
            }
            if (wwivStatus.Text != "OFFLINE" || netStatus.Text != "OFFLINE" || telnetStatus.Text != "OFFLINE")
            {
                button2.Enabled = false;
                infoStatus.ForeColor = System.Drawing.Color.Red;
                infoStatus.Text = "All Systems Must Be Offline Before Update!";
            }
            else
            {
                button1.Enabled = false;
                infoStatus.ForeColor = System.Drawing.Color.Green;
                infoStatus.Text = "All Systems Are Offline... Ready To Update!";
            }
        }

        // BackgroundWorker End Of Task Reporting
        void m_oWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            // Fetch Version
            string fetchVersion;
            fetchVersion = label2.Text;
            // Cancel Reporting 
            if (e.Cancelled)
            {
                infoStatus.Text = "Task Cancelled.";
            }
            // Error Reporting
            else if (e.Error != null)
            {
                infoStatus.Text = "Error while performing background operation.";
            }
            else
            {
                // Everything completed normally.
                infoStatus.Text = "WWIV 5 Build " + fetchVersion + " Is Complete!";
                progressBar1.Visible = true;
                activeStatus.Visible = false;

                // Update UI Cosmetics
                button3.Visible = true;
                button5.Visible = true;
            }
        }

        // BackgroundWorker Progress And Infomation Update
        void m_oWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar1.Value = e.ProgressPercentage;
            infoStatus.Text = "Updating WWIV 5......" + progressBar1.Value.ToString() + "%";
        }

        // Restart Button
        private void button1_Click(object sender, EventArgs e)
        {
            Application.Restart();
        }

        // Update WWIV Button
        private void button2_Click(object sender, EventArgs e)
        {
            // Update UI Cosmetics
            string fetchVersion;
            fetchVersion = label2.Text;
            button1.Visible = false;
            button2.Visible = false;
            label1.Visible = true;
            label2.Visible = true;
            progressBar1.Visible = true;
            activeStatus.Visible = true;

            // Perform BackgroundWorker
            m_oWorker.RunWorkerAsync();
        }

        void m_oWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            // PERFORM UPDATE

            // Fetch Version
            string fetchVersion;
            fetchVersion = label2.Text;

            // Make Sure Build Number Is NOT Null
            if (fetchVersion != null)
            {
                // Set Global Variables For Update
                activeStatus.Text = "Initializing Update...";
                string backupPath = Directory.GetCurrentDirectory();
                string zipPath = Environment.GetEnvironmentVariable("USERPROFILE") + @"\Documents\" + DateTime.Now.ToString("yyyyMMddHHmmssfff") + "_wwiv-backup.zip";
                string extractPath = Directory.GetCurrentDirectory();
                string extractPath2 = Environment.GetEnvironmentVariable("SystemRoot") + @"\System32";
                string remoteUri = "http://build.wwivbbs.org/jenkins/job/wwiv/" + fetchVersion + "/label=windows/artifact/";
                string fileName = "wwiv-build-win-" + fetchVersion + ".zip", myStringWebResource = null;
                string updatePath = Environment.GetEnvironmentVariable("USERPROFILE") + @"\Downloads\wwiv-build-win-" + fetchVersion + ".zip";

                // Update Progress Bar
                m_oWorker.ReportProgress(20);

                // Begin WWIV Backup
                activeStatus.Text = "Performing WWIV Backup...";
                ZipFile.CreateFromDirectory(backupPath, zipPath);

                // Update Progress Bar
                m_oWorker.ReportProgress(40);

                // Fetch Latest Sucessful Build
                activeStatus.Text = "Fetching WWIV Package From Server...";
                WebClient myWebClient = new WebClient();
                myStringWebResource = remoteUri + fileName;
                myWebClient.DownloadFile(myStringWebResource, Environment.GetEnvironmentVariable("USERPROFILE") + @"\Downloads\" + fileName);

                // Update Progress Bar
                m_oWorker.ReportProgress(60);

                // Patch Existing WWIV Install
                activeStatus.Text = "Patching WWIV Files For Update...";
                using (ZipArchive archive = ZipFile.OpenRead(updatePath))
                {
                    foreach (ZipArchiveEntry entry in archive.Entries)
                    {
                        if (entry.FullName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
                        {
                            entry.ExtractToFile(Path.Combine(extractPath, entry.FullName), true);
                        }
                        if (entry.FullName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                        {
                            entry.ExtractToFile(Path.Combine(extractPath, entry.FullName), true);
                        }
                        if (entry.FullName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
                        {
                            entry.ExtractToFile(Path.Combine(extractPath2, entry.FullName), true);
                        }
                    }
                }
                // Update Progress Bar
                m_oWorker.ReportProgress(80);
            }
            //Report 100% completion on operation completed
            m_oWorker.ReportProgress(100);
        }

        // Launch WWIV
        private void button3_Click(object sender, EventArgs e)
        {
            // Launch WWIV, WWIVnet and Latest Changes in Browser.
            string fetchVersion;
            fetchVersion = label2.Text;
            string wwivChanges = "http://build.wwivbbs.org/jenkins/job/wwiv/" + fetchVersion + "/label=windows/changes";
            Environment.CurrentDirectory = @"C:\wwiv";

            // Launch Telnet Server
            ProcessStartInfo telNet = new ProcessStartInfo("WWIV5TelnetServer.exe");
            telNet.WindowStyle = ProcessWindowStyle.Minimized;
            Process.Start(telNet);

            // Launch Latest Realse Changes into Default Browser
            Process.Start(wwivChanges);

            // Exit Application
            Application.Exit();
        }

        // Exit Program Button
        private void button5_Click(object sender, EventArgs e)
        {
            // Exit Application
            Application.Exit();
        }

        private void Form2_Load(object sender, EventArgs e)
        {
            // Set Update Version Label
            versionNumber.Text = Form1.updateVersionLabel;
        }
    }
}
