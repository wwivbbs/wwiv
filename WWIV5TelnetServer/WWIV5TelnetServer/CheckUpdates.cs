﻿/**************************************************************************/
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
using System.Net;
using System.Timers;
using System.Windows.Forms;
using System.Diagnostics;
using System.Xml;
using System.Xml.Linq;
using System.Text.RegularExpressions;

namespace WWIV5TelnetServer
{
    class CheckUpdates
    {
        public System.Timers.Timer updateTimer;
        string CheckUpdateFile = @"WWIV5_Update_Stamp.xml";

        public void UpdateHeartbeat()
        {
            // Create or Update External XML For Last Update Check DateTime
            if (!File.Exists(CheckUpdateFile))
            {
                DateTime DateTimeNow = DateTime.Now;
                XmlWriterSettings wSettings = new XmlWriterSettings();
                wSettings.Indent = true;
                XmlWriter writer = XmlWriter.Create(CheckUpdateFile, wSettings);
                writer.WriteStartDocument();
                writer.WriteComment("This file is generated by WWIV5TelnetServer - DO NOT MODIFY.");
                writer.WriteStartElement("WWIV5UpdateStamp");
                writer.WriteStartElement("LastChecked");
                writer.WriteElementString("DateTime", DateTimeNow.ToString());
                writer.WriteEndElement();
                writer.WriteEndElement();
                writer.WriteEndDocument();
                writer.Flush();
                writer.Close();
            }

            // Do Update Check
            updateTimer = new System.Timers.Timer(1000 * 60 * 60); // Hourly - Release Code
            //updateTimer = new System.Timers.Timer(1000 * 10); // 10 Seconds for Testing Only
            updateTimer.Elapsed += new ElapsedEventHandler(DoUpdateCheck);
            updateTimer.AutoReset = true;
            updateTimer.Enabled = true;
            if (Properties.Settings.Default.checkUpdates == "On Startup")
            {
                DoUpdateCheck(null, null);
            }
        }

        public void DoUpdateCheck(object source, ElapsedEventArgs e)
        {
            // Get Last Time Update Was Checked DateTime
            XDocument xdoc = XDocument.Load(@"WWIV5_Update_Stamp.xml");
            string lastUpdateCheck = xdoc.Element("WWIV5UpdateStamp").Element("LastChecked").Element("DateTime").Value;

            // DateTime Now And Then
            DateTime DateTimeNow = DateTime.Now;
            DateTime DateTimeThen = DateTime.Parse(lastUpdateCheck, null);

            // Get Differnece In DateTimes
            TimeSpan duration = (DateTimeNow - DateTimeThen);
            double hourCount = duration.TotalHours;
            double dayCount = duration.TotalDays;

            // Get Global Strings for Version Numbers
            string buildVersion;
            buildVersion = MainForm.WWIV_Build;
            string longVersion;
            longVersion = MainForm.WWIV_Version;

            // Declare And Initilize Set Build Number Variables to 0
            string wwivBuild5_1 = "0";

            // Grab User Check Update Preference 
            string UserUpdatePref;
            UserUpdatePref = Properties.Settings.Default.checkUpdates;

            // Fetch Latest Build Number For WWIV 5.1
            // TODO: Add Newer Short Version To Fetch Version Number From Jenkins
            WebClient wc = new WebClient();
            string htmlString1 = wc.DownloadString("http://build.wwivbbs.org/jenkins/job/wwiv/lastSuccessfulBuild/label=windows/");
            Match mTitle1 = Regex.Match(htmlString1, "(?:number.*?>)(?<buildNumber1>.*?)(?:<)");
            {
                wwivBuild5_1 = mTitle1.Groups[1].Value;
            }
            string newestVersion;
            newestVersion = wwivBuild5_1;

            int newBuild = Int32.Parse(newestVersion);
            int oldBuild = newBuild;
            if (!buildVersion.StartsWith("dev"))
            {
              // Only check our current build if it's not
              // a development build.
              oldBuild = Int32.Parse(buildVersion);
            }

            // On Startup Check For Update
            if (UserUpdatePref == "On Startup" && newBuild > oldBuild)
            {
                updateTimer.AutoReset = false;
                updateTimer.Enabled = false;
                if (MessageBox.Show("A Newer Version of WWIV is Available!\r\n \r\nLaunch WWIV Update Now?", "UPDATE AVAILABLE", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                {
                    // Launch WWIV Update
                    ProcessStartInfo wwivUpdate = new ProcessStartInfo("wwiv-update.exe");
                    Process.Start(wwivUpdate);
                }
            }

            // Periodical Update Checking
            if (UserUpdatePref == "Hourly" && newBuild > oldBuild && hourCount >= 1)
            {
                updateTimer.Enabled = false;
                if (MessageBox.Show("A Newer Version of WWIV is Available!\r\n \r\nLaunch WWIV Update Now?", "UPDATE AVAILABLE", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                {
                    // Launch WWIV Update
                    ProcessStartInfo wwivUpdate = new ProcessStartInfo("wwiv-update.exe");
                    Process.Start(wwivUpdate);
                }
            }
            if (UserUpdatePref == "Daily" && newBuild > oldBuild && hourCount >= 24)
            {
                updateTimer.Enabled = false;
                if (MessageBox.Show("A Newer Version of WWIV is Available!\r\n \r\nLaunch WWIV Update Now?", "UPDATE AVAILABLE", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                {
                    // Launch WWIV Update
                    ProcessStartInfo wwivUpdate = new ProcessStartInfo("wwiv-update.exe");
                    Process.Start(wwivUpdate);
                }
            }
            if (UserUpdatePref == "Weekly" && newBuild > oldBuild && dayCount >= 7)
            {
                updateTimer.Enabled = false;
                if (MessageBox.Show("A Newer Version of WWIV is Available!\r\n \r\nLaunch WWIV Update Now?", "UPDATE AVAILABLE", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                {
                    // Launch WWIV Update
                    ProcessStartInfo wwivUpdate = new ProcessStartInfo("wwiv-update.exe");
                    Process.Start(wwivUpdate);
                }
            }
            if (UserUpdatePref == "Monthly" && newBuild > oldBuild && dayCount >= 30)
            {
                updateTimer.Enabled = false;
                if (MessageBox.Show("A Newer Version of WWIV is Available!\r\n \r\nLaunch WWIV Update Now?", "UPDATE AVAILABLE", MessageBoxButtons.YesNo, MessageBoxIcon.Information) == DialogResult.Yes)
                {
                    // Launch WWIV Update
                    ProcessStartInfo wwivUpdate = new ProcessStartInfo("wwiv-update.exe");
                    Process.Start(wwivUpdate);
                }
            }

            // Update XML File Last Update DateTime
            XmlWriterSettings wSettings = new XmlWriterSettings();
            wSettings.Indent = true;
            XmlWriter writer = XmlWriter.Create(CheckUpdateFile, wSettings);
            writer.WriteStartDocument();
            writer.WriteComment("This file is generated by WWIV5TelnetServer - DO NOT MODIFY.");
            writer.WriteStartElement("WWIV5UpdateStamp");
            writer.WriteStartElement("LastChecked");
            writer.WriteElementString("DateTime", DateTimeNow.ToString());
            writer.WriteEndElement();
            writer.WriteEndElement();
            writer.WriteEndDocument();
            writer.Flush();
            writer.Close();

            // Re-Activate Update Timer
            updateTimer.Enabled = true;
        }
    }
}
