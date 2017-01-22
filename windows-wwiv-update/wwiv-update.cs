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
using System.Windows.Forms;
using System.Text.RegularExpressions;
using System.Net;
using System.Diagnostics;
using System.Reflection;

namespace windows_wwiv_update
{
  public partial class Form1 : Form
  {
    // Global Strings
    public static string wwivUpdateVersion;
    public static string updateVersionLabel;
    public static string updateTagLabel;

    public static readonly string baseUrlUnstable = "http://build.wwivbbs.org/jenkins/job/wwiv/label=windows";
    public static readonly string baseUrlStable = "https://build.wwivbbs.org/jenkins/job/wwiv52/label=windows";
    // Default to Stable Release.
    public static string version_number = "5.2";

    public Form1()
    {
      InitializeComponent();
            // Fetch Latest Build Number For WWIV Unstable
            // http://build.wwivbbs.org/jenkins/job/wwiv/label=windows/lastSuccessfulBuild/buildNumber
            // Fetch Latest Build Number For WWIV Stable
            // http://build.wwivbbs.org/jenkins/job/wwiv52/label=windows/lastSuccessfulBuild/buildNumber
            WebClient wc = new WebClient();
      string wwivBuildUnstable = wc.DownloadString(baseUrlUnstable + "/lastSuccessfulBuild/buildNumber");

            // Fetch Latest Build Number For WWIV Stable
      var stableUrl = baseUrlStable + "/lastSuccessfulBuild/buildNumber";
      string wwivBuildStable = wc.DownloadString(stableUrl);
      versionUnstable.Text = wwivBuildUnstable;
      versionStable.Text = wwivBuildStable;
      currentVersion();
    }

    // Get Current Version
    private void currentVersion()
    {
      Process p = new Process();
      p.StartInfo.FileName = @"bbs.exe";
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
      displayVersion = ("WWIV v" + majorVersion + "." + minorVersion + "." + buildVersion + "." + revisVersion);

      currentVersionInfo.Text = displayVersion;

      // Get Current Version Of Windows WWIV Update
      updateTagLabel = "RC3";
      wwivUpdateVersion = Assembly.GetExecutingAssembly().GetName().Version.ToString();
      updateVersionLabel = "WWIV UPDATE v" + wwivUpdateVersion + " | " + updateTagLabel;
      versionNumber.Text = updateVersionLabel;
    }

    // Update To Newest WWIV  Unstable
    private void updateUnstable_Click(object sender, EventArgs e)
    {
      string updateToNewUnstable = versionUnstable.Text;
      version_number = "5.3";
      if (updateToNewUnstable == null || string.IsNullOrWhiteSpace(updateToNewUnstable))
      {
        if (MessageBox.Show("Error! Build Server Unavailable.", "Build Server Down", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error) == DialogResult.Retry)
        {
          // User Clicked Retry
          Application.Restart();
        }
        else
        {
          // User Clicked Cancel
          Application.Exit();
        }
      }
      else
      {
        // Begin Update Unstable
        string fetchVersion = updateToNewUnstable;
        this.Hide();
        Form2 frm = new Form2(baseUrlUnstable, "5.3", fetchVersion);
        frm.ShowDialog();
        this.Close();
      }
    }

    // Update To Newest WWIV
    private void updateStable_Click(object sender, EventArgs e)
    {
      string updateToNewStable = versionStable.Text;
      version_number = "5.2";
      if (updateToNewStable == null || string.IsNullOrWhiteSpace(updateToNewStable))
      {
        if (MessageBox.Show("Error! Build Server Unavailable.", "Build Server Down", MessageBoxButtons.RetryCancel, MessageBoxIcon.Error) == DialogResult.Retry)
        {
          // User Clicked Retry
          Application.Restart();
        }
        else
        {
          // User Clicked Cancel
          Application.Exit();
        }
      }
      else
      {
        // Begin Update Stable
        string fetchVersion = updateToNewStable;
        this.Hide();
        Form2 frm = new Form2(baseUrlStable, "5.2", fetchVersion);
        frm.ShowDialog();
        this.Close();
      }
    }

    // Update To User Defined WWIV 5.x
    private void customUpdate_Click(object sender, EventArgs e)
    {
      const int MinLength = 4;
      string customBuildNumber;
      customBuildNumber = customBuild.Text;
      if (customBuildNumber == null || string.IsNullOrWhiteSpace(customBuildNumber) || customBuildNumber.Length < MinLength)
      {
        MessageBox.Show("Please Enter A Valid Build Number.", "Build Number Invalid", MessageBoxButtons.OK, MessageBoxIcon.Error);
      }
      else
      {
        // Begin Update Custom Build
        string fetchVersion = customBuildNumber;
        this.Hide();
        Form2 frm = new Form2(baseUrlUnstable, "5.2", fetchVersion);
        frm.ShowDialog();
        this.Close();
      }
    }
  }
}
