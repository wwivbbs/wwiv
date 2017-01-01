/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright © 2014-2017, WWIV Software Services              */
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
    BackgroundWorker worker_ = new BackgroundWorker();
    private string baseUrl_;
    private string buildNumber_;
    private string majorVersion_;
    private string fullVersion_;

    public Form2(string baseUrl, string majorVersion, string buildNumber)
    {
      baseUrl_ = baseUrl;
      majorVersion_ = majorVersion;
      buildNumber_ = buildNumber;
      // Fetch Version
      InitializeComponent();
      fullVersion_ = majorVersion_ + "." + buildNumber_;
      labelVersion.Text = fullVersion_;

      // BackgroundWorker Properties
      worker_.DoWork += new DoWorkEventHandler(m_oWorker_DoWork);
      worker_.ProgressChanged += new ProgressChangedEventHandler
              (m_oWorker_ProgressChanged);
      worker_.RunWorkerCompleted += new RunWorkerCompletedEventHandler
              (m_oWorker_RunWorkerCompleted);
      worker_.WorkerReportsProgress = true;
      worker_.WorkerSupportsCancellation = true;

      // Update UI Cosmetics
      label1.Visible = false;
      labelVersion.Visible = false;
      buttonLaunch.Visible = false;
      buttonExit.Visible = false;
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
      if (Process.GetProcessesByName("WWIVerver").Length >= 1)
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
        buttonUpdate.Enabled = false;
        infoStatus.ForeColor = System.Drawing.Color.Red;
        infoStatus.Text = "All Systems Must Be Offline Before Update!";
      }
      else
      {
        buttonRestart.Enabled = false;
        infoStatus.ForeColor = System.Drawing.Color.Green;
        infoStatus.Text = "All Systems Are Offline... Ready To Update!";
      }
    }

    // BackgroundWorker End Of Task Reporting
    void m_oWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
    {
      // Fetch Version
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
        infoStatus.Text = "WWIV 5 Build " + majorVersion_ + "." + buildNumber_ + " Is Complete!";
        progressBar1.Visible = true;
        activeStatus.Visible = false;

        // Update UI Cosmetics
        buttonLaunch.Visible = true;
        buttonExit.Visible = true;
      }
    }

    // BackgroundWorker Progress And Infomation Update
    void m_oWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
    {
      progressBar1.Value = e.ProgressPercentage;
      infoStatus.Text = "Updating WWIV 5......" + progressBar1.Value.ToString() + "%";
    }

    // Restart Button
    private void buttonRestart_Click(object sender, EventArgs e)
    {
      Application.Restart();
    }

    // Update WWIV Button
    private void buttonUpdate_Click(object sender, EventArgs e)
    {
      // Update UI Cosmetics
      buttonRestart.Visible = false;
      buttonUpdate.Visible = false;
      label1.Visible = true;
      labelVersion.Visible = true;
      progressBar1.Visible = true;
      activeStatus.Visible = true;

      // Perform BackgroundWorker
      worker_.RunWorkerAsync();
    }

    void UpdateStatus(string text)
    {
      MethodInvoker d = delegate () { activeStatus.Text = text; };
      if (this.InvokeRequired)
      {
        this.Invoke(d);
      }
      d.Invoke();
    }

    void m_oWorker_DoWork(object sender, DoWorkEventArgs e)
    {
      // PERFORM UPDATE

      // Make Sure Build Number Is NOT Null
      if (buildNumber_ != null)
      {
        UpdateStatus("Initializing Update...");
        var USERPROFILE = Environment.GetEnvironmentVariable("USERPROFILE");
        var documentsDir = Environment.GetEnvironmentVariable("USERPROFILE") + @"\Documents\";
        var downloadsDir = Environment.GetEnvironmentVariable("USERPROFILE") + @"\Downloads\";

        // Set Global Variables For Update
        var wwivUpdateFile = "wwiv-update.exe";

        var backupPath = Directory.GetCurrentDirectory();
        var zipPath = documentsDir + DateTime.Now.ToString("yyyyMMddHHmmssfff") + "_wwiv-backup.zip";
        var extractPath = Directory.GetCurrentDirectory();
        var updateTempPath = Directory.GetCurrentDirectory() + @"\update-temp";
        var fileName = "wwiv-win-" + fullVersion_ + ".zip";
        var fullUri = new Uri(baseUrl_ + "/lastSuccessfulBuild/artifact/" + fileName);
        var outputPathToSaveFile = Path.Combine(downloadsDir, fileName);

        // Update Progress Bar.
        worker_.ReportProgress(20);

        // Fetch Latest Sucessful Build.
        UpdateStatus("Fetching WWIV Package From Server...");
        WebClient myWebClient = new WebClient();
        myWebClient.DownloadFile(fullUri, outputPathToSaveFile);

        // Update Progress Bar.
        worker_.ReportProgress(60);

        // Begin WWIV Backup.
        UpdateStatus("Performing WWIV Backup...");
        ZipFile.CreateFromDirectory(backupPath, zipPath);

        // Update Progress Bar.
        worker_.ReportProgress(40);

        // Create Temp Update Directory.
        Directory.CreateDirectory(updateTempPath);

        // Create Cleanup Batch File if it does not already exist.
        string cleanupFile = Path.Combine(Application.StartupPath, "cleanup.bat");
        if (!File.Exists(cleanupFile))
        {
          File.Delete(cleanupFile);
          using (StreamWriter w = new StreamWriter(cleanupFile))
          {
            w.WriteLine(@"
@ECHO OFF

REM This File Is Created By wwiv-update.exe. DO NOT MODIFY!
:START_CLEANUP
TIMEOUT /T 1
xcopy update-temp\*.* .\ /Y
rd update-temp /S /Q
:EXIT");
            w.Close();
          }
        }

        // Patch Existing WWIV Install
        UpdateStatus("Patching WWIV Files For Update...");
        using (ZipArchive archive = ZipFile.OpenRead(outputPathToSaveFile))
        {
          foreach (ZipArchiveEntry entry in archive.Entries)
          {
            try
            {
              if (entry.FullName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) && !entry.FullName.Equals(wwivUpdateFile, StringComparison.OrdinalIgnoreCase))
              {
                entry.ExtractToFile(Path.Combine(extractPath, entry.FullName), true);
              }
              if (entry.FullName.Equals(wwivUpdateFile, StringComparison.OrdinalIgnoreCase))
              {
                entry.ExtractToFile(Path.Combine(updateTempPath, entry.FullName), true);
              }
              if (entry.FullName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
              {
                entry.ExtractToFile(Path.Combine(extractPath, entry.FullName), true);
              }
              if (entry.FullName.EndsWith("whatsnew.txt", StringComparison.OrdinalIgnoreCase) && entry.FullName.EndsWith("changelog.txt", StringComparison.OrdinalIgnoreCase))
              {
                entry.ExtractToFile(Path.Combine(extractPath, entry.FullName), true);
              }
            } catch (IOException ex) {
              Debug.WriteLine(ex.ToString());
              var message = "Unable to extract file: " + entry.FullName + "\nCause: " + ex.Message;
              MessageBox.Show(message);
            }
          }
        }
        // Update Progress Bar
        worker_.ReportProgress(80);
      }
      //Report 100% completion on operation completed
      worker_.ReportProgress(100);
    }

    private void cleanupAfterWWIVUpdate()
    {
      // Launch Latest Realse Changes into Default Browser
      string wwivChanges = baseUrl_ + "/" + buildNumber_ + "/changes";
      Process.Start(wwivChanges);

      // Run CleanUp.Bat
      ProcessStartInfo cleanUp = new ProcessStartInfo("cleanup.bat");
      cleanUp.WindowStyle = ProcessWindowStyle.Minimized;
      try
      {
        Process.Start(cleanUp);
      }
      catch (Win32Exception ex)
      {
        Debug.WriteLine(ex.ToString());
      }

    }

    // Launch WWIV
    private void buttonLaunch_Click(object sender, EventArgs e)
    {
      cleanupAfterWWIVUpdate();

      try
      {
        ProcessStartInfo telNet = new ProcessStartInfo("WWIVServer.exe");
        Process.Start(telNet);
      }
      catch (Win32Exception ex)
      {
        Debug.WriteLine(ex.ToString());
      }
      // Exit Application
      Application.Exit();
    }

    // Exit Program Button
    private void buttonExit_Click(object sender, EventArgs e)
    {
      cleanupAfterWWIVUpdate();
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
