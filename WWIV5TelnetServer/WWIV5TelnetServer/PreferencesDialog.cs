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
using System.Windows.Forms;

namespace WWIV5TelnetServer
{
    public partial class PreferencesDialog : Form
    {
        public PreferencesDialog()
        {
            InitializeComponent();
            // Properties.Settings.Default.
            CancelButton = this.cancelButton;
        }

        private void PreferencesDialog_Load(object sender, EventArgs e)
        {
            Properties.Settings.Default.Reload();
            highNodeSpinner.Value = Properties.Settings.Default.endNode;
            autostartCheckBox.Checked = Properties.Settings.Default.autostart;
            executableField.Text = Properties.Settings.Default.executable;
            homeField.Text = Properties.Settings.Default.homeDirectory;
            localNodeSpinner.Value = Properties.Settings.Default.localNode;
            parametersField.Text = Properties.Settings.Default.parameters;
            portSpinner.Value = Properties.Settings.Default.port;
            lowNodeSpinner.Value = Properties.Settings.Default.startNode;
            balloonsCheckBox.Checked = Properties.Settings.Default.useBalloons;
            beginDayCheckBox.Checked = Properties.Settings.Default.useBegindayEvent;
            runEventsCheckbox.Checked = Properties.Settings.Default.useEvents;
            launchMinimizedCheckBox.Checked = Properties.Settings.Default.launchMinimized;
            launchLocalNodeCheckBox.Checked = Properties.Settings.Default.launchLocalNodeAtStartup;
            launchNetworkCheckBox.Checked = Properties.Settings.Default.launchNetworkAtStartup;
            checkUpdates.Text = Properties.Settings.Default.checkUpdates;
        }

        private void ok_Clicked(object sender, EventArgs e)
        {
            Close();
        }

        private void cancel_Click(object sender, EventArgs e)
        {
            Close();
            this.DialogResult = DialogResult.Cancel;
        }

        private void PreferencesDialog_FormClosed(object sender, FormClosedEventArgs e)
        {
            Console.WriteLine("PreferencesDialog.FormClosing");
            if (e.CloseReason == CloseReason.UserClosing && this.DialogResult == DialogResult.OK)
            {
                Console.WriteLine("Should Save: ");
                Properties.Settings.Default.Upgrade();
                Properties.Settings.Default.endNode = highNodeSpinner.Value;
                Properties.Settings.Default.autostart = autostartCheckBox.Checked;
                Properties.Settings.Default.executable = executableField.Text;
                Properties.Settings.Default.homeDirectory = homeField.Text;
                Properties.Settings.Default.localNode = localNodeSpinner.Value;
                Properties.Settings.Default.parameters = parametersField.Text;
                Properties.Settings.Default.port = portSpinner.Value;
                Properties.Settings.Default.startNode = lowNodeSpinner.Value;
                Properties.Settings.Default.useBalloons = balloonsCheckBox.Checked;
                Properties.Settings.Default.useBegindayEvent = beginDayCheckBox.Checked;
                Properties.Settings.Default.useEvents = runEventsCheckbox.Checked;
                Properties.Settings.Default.launchMinimized = launchMinimizedCheckBox.Checked;
                Properties.Settings.Default.launchLocalNodeAtStartup = launchLocalNodeCheckBox.Checked;
                Properties.Settings.Default.launchNetworkAtStartup = launchNetworkCheckBox.Checked;
                Properties.Settings.Default.checkUpdates = checkUpdates.Text;
                Properties.Settings.Default.Save();
            }
        }
    }
}
