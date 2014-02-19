using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
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

        private void PreferencesDialog_FormClosing(object sender, FormClosingEventArgs e)
        {
        }

        private void PreferencesDialog_Load(object sender, EventArgs e)
        {
            Console.WriteLine("PreferencesDialog.Load");
            Properties.Settings.Default.Reload();
        }

        private void ok_Clicked(object sender, EventArgs e)
        {
            Console.WriteLine("PreferencesDialog.ok_Clicked");
            Close();
            Console.WriteLine("PreferencesDialog.ok_Clicked (After Close)");
        }

        private void cancel_Click(object sender, EventArgs e)
        {
            Console.WriteLine("PreferencesDialog.cancel_Click");
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
                Properties.Settings.Default.Save();
                Console.WriteLine("endNode=" + Properties.Settings.Default.endNode);
                Console.WriteLine("tf=" + highNodeSpinner.Value);
            }
        }
    }
}
