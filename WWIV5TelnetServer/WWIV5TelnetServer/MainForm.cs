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

namespace WWIV5TelnetServer
{
    public partial class MainForm : Form
    {
        private TelnetServer server = new TelnetServer();
        delegate void AppendTextCallback(string text);
            
        public MainForm()
        {
            InitializeComponent();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
            server.StatusMessageChanged += server_StatusMessage;
        }

        void server_StatusMessage(object sender, StatusMessageEventArgs e)
        {
            Console.WriteLine(e.Message);
            if (this.messages.InvokeRequired)
            {
                AppendTextCallback a = new AppendTextCallback(AppendMessage);
                this.Invoke(a, new object[] { e.Message });
            }
            else
            {
                AppendMessage(e.Message);
            }
        }

        void AppendMessage(string message)
        {
            messages.AppendText(message + "\r\n");
        }

        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
             MessageBox.Show("About My App");
        }

        private void startToolStripMenuItem_Click(object sender, EventArgs e)
        {
            server.Start();
            startToolStripMenuItem.Enabled = false;
            stopToolStripMenuItem.Enabled = true;
        }

        private void stopToolStripMenuItem_Click(object sender, EventArgs e)
        {
            server.Stop();
            startToolStripMenuItem.Enabled = true;
            stopToolStripMenuItem.Enabled = false;
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

    }
}
