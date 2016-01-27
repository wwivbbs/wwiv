﻿namespace WWIV5TelnetServer
{
    partial class PreferencesDialog
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PreferencesDialog));
            this.okButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.tabControlPreferences = new System.Windows.Forms.TabControl();
            this.generalTab = new System.Windows.Forms.TabPage();
            this.labelParameters2 = new System.Windows.Forms.Label();
            this.parametersField2 = new System.Windows.Forms.TextBox();
            this.sshSpinner = new System.Windows.Forms.NumericUpDown();
            this.labelSSH = new System.Windows.Forms.Label();
            this.checkUpdates = new System.Windows.Forms.ComboBox();
            this.label1 = new System.Windows.Forms.Label();
            this.launchNetworkCheckBox = new System.Windows.Forms.CheckBox();
            this.launchLocalNodeCheckBox = new System.Windows.Forms.CheckBox();
            this.runEventsCheckbox = new System.Windows.Forms.CheckBox();
            this.balloonsCheckBox = new System.Windows.Forms.CheckBox();
            this.beginDayCheckBox = new System.Windows.Forms.CheckBox();
            this.launchMinimizedCheckBox = new System.Windows.Forms.CheckBox();
            this.labelLocalNode = new System.Windows.Forms.Label();
            this.localNodeSpinner = new System.Windows.Forms.NumericUpDown();
            this.labelExecutable = new System.Windows.Forms.Label();
            this.labelHomeDirectory = new System.Windows.Forms.Label();
            this.labelParameters = new System.Windows.Forms.Label();
            this.parametersField = new System.Windows.Forms.TextBox();
            this.homeField = new System.Windows.Forms.TextBox();
            this.buttonBrowseExecutable = new System.Windows.Forms.Button();
            this.executableField = new System.Windows.Forms.TextBox();
            this.highNodeSpinner = new System.Windows.Forms.NumericUpDown();
            this.labelEndNode = new System.Windows.Forms.Label();
            this.lowNodeSpinner = new System.Windows.Forms.NumericUpDown();
            this.labelStartNode = new System.Windows.Forms.Label();
            this.portSpinner = new System.Windows.Forms.NumericUpDown();
            this.labelPort = new System.Windows.Forms.Label();
            this.autostartCheckBox = new System.Windows.Forms.CheckBox();
            this.loggingTab = new System.Windows.Forms.TabPage();
            this.tabControlPreferences.SuspendLayout();
            this.generalTab.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.localNodeSpinner)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.highNodeSpinner)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.lowNodeSpinner)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.portSpinner)).BeginInit();
            this.SuspendLayout();
            // 
            // okButton
            // 
            this.okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.okButton.Location = new System.Drawing.Point(631, 436);
            this.okButton.Name = "okButton";
            this.okButton.Size = new System.Drawing.Size(75, 23);
            this.okButton.TabIndex = 0;
            this.okButton.Text = "OK";
            this.okButton.UseVisualStyleBackColor = true;
            this.okButton.Click += new System.EventHandler(this.ok_Clicked);
            // 
            // cancelButton
            // 
            this.cancelButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancelButton.Location = new System.Drawing.Point(550, 436);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 1;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancel_Click);
            // 
            // tabControlPreferences
            // 
            this.tabControlPreferences.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.tabControlPreferences.Controls.Add(this.generalTab);
            this.tabControlPreferences.Controls.Add(this.loggingTab);
            this.tabControlPreferences.Location = new System.Drawing.Point(12, 12);
            this.tabControlPreferences.Name = "tabControlPreferences";
            this.tabControlPreferences.SelectedIndex = 0;
            this.tabControlPreferences.Size = new System.Drawing.Size(694, 418);
            this.tabControlPreferences.TabIndex = 2;
            // 
            // generalTab
            // 
            this.generalTab.Controls.Add(this.labelParameters2);
            this.generalTab.Controls.Add(this.parametersField2);
            this.generalTab.Controls.Add(this.sshSpinner);
            this.generalTab.Controls.Add(this.labelSSH);
            this.generalTab.Controls.Add(this.checkUpdates);
            this.generalTab.Controls.Add(this.label1);
            this.generalTab.Controls.Add(this.launchNetworkCheckBox);
            this.generalTab.Controls.Add(this.launchLocalNodeCheckBox);
            this.generalTab.Controls.Add(this.runEventsCheckbox);
            this.generalTab.Controls.Add(this.balloonsCheckBox);
            this.generalTab.Controls.Add(this.beginDayCheckBox);
            this.generalTab.Controls.Add(this.launchMinimizedCheckBox);
            this.generalTab.Controls.Add(this.labelLocalNode);
            this.generalTab.Controls.Add(this.localNodeSpinner);
            this.generalTab.Controls.Add(this.labelExecutable);
            this.generalTab.Controls.Add(this.labelHomeDirectory);
            this.generalTab.Controls.Add(this.labelParameters);
            this.generalTab.Controls.Add(this.parametersField);
            this.generalTab.Controls.Add(this.homeField);
            this.generalTab.Controls.Add(this.buttonBrowseExecutable);
            this.generalTab.Controls.Add(this.executableField);
            this.generalTab.Controls.Add(this.highNodeSpinner);
            this.generalTab.Controls.Add(this.labelEndNode);
            this.generalTab.Controls.Add(this.lowNodeSpinner);
            this.generalTab.Controls.Add(this.labelStartNode);
            this.generalTab.Controls.Add(this.portSpinner);
            this.generalTab.Controls.Add(this.labelPort);
            this.generalTab.Controls.Add(this.autostartCheckBox);
            this.generalTab.Location = new System.Drawing.Point(4, 22);
            this.generalTab.Name = "generalTab";
            this.generalTab.Padding = new System.Windows.Forms.Padding(3);
            this.generalTab.Size = new System.Drawing.Size(686, 392);
            this.generalTab.TabIndex = 0;
            this.generalTab.Text = "General";
            this.generalTab.UseVisualStyleBackColor = true;
            // 
            // labelParameters2
            // 
            this.labelParameters2.AutoSize = true;
            this.labelParameters2.Location = new System.Drawing.Point(19, 240);
            this.labelParameters2.Name = "labelParameters2";
            this.labelParameters2.Size = new System.Drawing.Size(88, 13);
            this.labelParameters2.TabIndex = 27;
            this.labelParameters2.Text = "SSH Parameters:";
            this.labelParameters2.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // parametersField2
            // 
            this.parametersField2.Location = new System.Drawing.Point(123, 237);
            this.parametersField2.Name = "parametersField2";
            this.parametersField2.Size = new System.Drawing.Size(466, 20);
            this.parametersField2.TabIndex = 26;
            this.parametersField2.Text = "-XS -H@H -N@N";
            // 
            // sshSpinner
            // 
            this.sshSpinner.Location = new System.Drawing.Point(123, 57);
            this.sshSpinner.Maximum = new decimal(new int[] {
            65536,
            0,
            0,
            0});
            this.sshSpinner.Name = "sshSpinner";
            this.sshSpinner.Size = new System.Drawing.Size(120, 20);
            this.sshSpinner.TabIndex = 25;
            this.sshSpinner.Value = new decimal(new int[] {
            22,
            0,
            0,
            0});
            // 
            // labelSSH
            // 
            this.labelSSH.AutoSize = true;
            this.labelSSH.Location = new System.Drawing.Point(24, 59);
            this.labelSSH.Name = "labelSSH";
            this.labelSSH.Size = new System.Drawing.Size(93, 13);
            this.labelSSH.TabIndex = 24;
            this.labelSSH.Text = "SSH TCP/IP Port:";
            // 
            // checkUpdates
            // 
            this.checkUpdates.FormattingEnabled = true;
            this.checkUpdates.Items.AddRange(new object[] {
            "On Startup",
            "Hourly",
            "Daily",
            "Weekly",
            "Monthly"});
            this.checkUpdates.Location = new System.Drawing.Point(393, 332);
            this.checkUpdates.Name = "checkUpdates";
            this.checkUpdates.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.checkUpdates.Size = new System.Drawing.Size(121, 21);
            this.checkUpdates.TabIndex = 23;
            this.checkUpdates.Text = "UpdatePref";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(250, 337);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(137, 13);
            this.label1.TabIndex = 22;
            this.label1.Text = "Check For WWIV Updates:";
            // 
            // launchNetworkCheckBox
            // 
            this.launchNetworkCheckBox.AutoSize = true;
            this.launchNetworkCheckBox.Location = new System.Drawing.Point(253, 312);
            this.launchNetworkCheckBox.Name = "launchNetworkCheckBox";
            this.launchNetworkCheckBox.Size = new System.Drawing.Size(174, 17);
            this.launchNetworkCheckBox.TabIndex = 21;
            this.launchNetworkCheckBox.Text = "Launch WWIVNet On Startup?";
            this.launchNetworkCheckBox.UseVisualStyleBackColor = true;
            // 
            // launchLocalNodeCheckBox
            // 
            this.launchLocalNodeCheckBox.AutoSize = true;
            this.launchLocalNodeCheckBox.Location = new System.Drawing.Point(253, 288);
            this.launchLocalNodeCheckBox.Name = "launchLocalNodeCheckBox";
            this.launchLocalNodeCheckBox.Size = new System.Drawing.Size(180, 17);
            this.launchLocalNodeCheckBox.TabIndex = 20;
            this.launchLocalNodeCheckBox.Text = "Launch Local Node On Startup?";
            this.launchLocalNodeCheckBox.UseVisualStyleBackColor = true;
            // 
            // runEventsCheckbox
            // 
            this.runEventsCheckbox.AutoSize = true;
            this.runEventsCheckbox.Enabled = false;
            this.runEventsCheckbox.Location = new System.Drawing.Point(22, 360);
            this.runEventsCheckbox.Name = "runEventsCheckbox";
            this.runEventsCheckbox.Size = new System.Drawing.Size(226, 17);
            this.runEventsCheckbox.TabIndex = 19;
            this.runEventsCheckbox.Text = "Run All WWIV Events? (Not Implemented)";
            this.runEventsCheckbox.UseVisualStyleBackColor = true;
            // 
            // balloonsCheckBox
            // 
            this.balloonsCheckBox.AutoSize = true;
            this.balloonsCheckBox.Location = new System.Drawing.Point(22, 336);
            this.balloonsCheckBox.Name = "balloonsCheckBox";
            this.balloonsCheckBox.Size = new System.Drawing.Size(128, 17);
            this.balloonsCheckBox.TabIndex = 18;
            this.balloonsCheckBox.Text = "Use Balloon Popups?";
            this.balloonsCheckBox.UseVisualStyleBackColor = true;
            // 
            // beginDayCheckBox
            // 
            this.beginDayCheckBox.AutoSize = true;
            this.beginDayCheckBox.Checked = true;
            this.beginDayCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
            this.beginDayCheckBox.Enabled = false;
            this.beginDayCheckBox.Location = new System.Drawing.Point(22, 312);
            this.beginDayCheckBox.Name = "beginDayCheckBox";
            this.beginDayCheckBox.Size = new System.Drawing.Size(130, 17);
            this.beginDayCheckBox.TabIndex = 17;
            this.beginDayCheckBox.Text = "Run Beginday Event?";
            this.beginDayCheckBox.UseVisualStyleBackColor = true;
            // 
            // launchMinimizedCheckBox
            // 
            this.launchMinimizedCheckBox.AutoSize = true;
            this.launchMinimizedCheckBox.Location = new System.Drawing.Point(22, 288);
            this.launchMinimizedCheckBox.Name = "launchMinimizedCheckBox";
            this.launchMinimizedCheckBox.Size = new System.Drawing.Size(152, 17);
            this.launchMinimizedCheckBox.TabIndex = 16;
            this.launchMinimizedCheckBox.Text = "Launch WWIV Minimized?";
            this.launchMinimizedCheckBox.UseVisualStyleBackColor = true;
            // 
            // labelLocalNode
            // 
            this.labelLocalNode.AutoSize = true;
            this.labelLocalNode.Location = new System.Drawing.Point(52, 112);
            this.labelLocalNode.Name = "labelLocalNode";
            this.labelLocalNode.Size = new System.Drawing.Size(65, 13);
            this.labelLocalNode.TabIndex = 15;
            this.labelLocalNode.Text = "Local Node:";
            this.labelLocalNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // localNodeSpinner
            // 
            this.localNodeSpinner.Location = new System.Drawing.Point(123, 110);
            this.localNodeSpinner.Name = "localNodeSpinner";
            this.localNodeSpinner.Size = new System.Drawing.Size(120, 20);
            this.localNodeSpinner.TabIndex = 7;
            this.localNodeSpinner.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
            // 
            // labelExecutable
            // 
            this.labelExecutable.AutoSize = true;
            this.labelExecutable.Location = new System.Drawing.Point(19, 160);
            this.labelExecutable.Name = "labelExecutable";
            this.labelExecutable.Size = new System.Drawing.Size(98, 13);
            this.labelExecutable.TabIndex = 13;
            this.labelExecutable.Text = "WWIV Executable:";
            this.labelExecutable.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelHomeDirectory
            // 
            this.labelHomeDirectory.AutoSize = true;
            this.labelHomeDirectory.Location = new System.Drawing.Point(34, 191);
            this.labelHomeDirectory.Name = "labelHomeDirectory";
            this.labelHomeDirectory.Size = new System.Drawing.Size(83, 13);
            this.labelHomeDirectory.TabIndex = 12;
            this.labelHomeDirectory.Text = "Home Directory:";
            this.labelHomeDirectory.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // labelParameters
            // 
            this.labelParameters.AutoSize = true;
            this.labelParameters.Location = new System.Drawing.Point(19, 214);
            this.labelParameters.Name = "labelParameters";
            this.labelParameters.Size = new System.Drawing.Size(96, 13);
            this.labelParameters.TabIndex = 11;
            this.labelParameters.Text = "Telnet Parameters:";
            this.labelParameters.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // parametersField
            // 
            this.parametersField.Location = new System.Drawing.Point(123, 211);
            this.parametersField.Name = "parametersField";
            this.parametersField.Size = new System.Drawing.Size(466, 20);
            this.parametersField.TabIndex = 10;
            this.parametersField.Text = "-XT -H@H -N@N";
            // 
            // homeField
            // 
            this.homeField.Location = new System.Drawing.Point(123, 184);
            this.homeField.Name = "homeField";
            this.homeField.Size = new System.Drawing.Size(466, 20);
            this.homeField.TabIndex = 9;
            this.homeField.Text = "C:\\wwiv";
            // 
            // buttonBrowseExecutable
            // 
            this.buttonBrowseExecutable.Location = new System.Drawing.Point(595, 157);
            this.buttonBrowseExecutable.Name = "buttonBrowseExecutable";
            this.buttonBrowseExecutable.Size = new System.Drawing.Size(75, 23);
            this.buttonBrowseExecutable.TabIndex = 8;
            this.buttonBrowseExecutable.Text = "Browse";
            this.buttonBrowseExecutable.UseVisualStyleBackColor = true;
            // 
            // executableField
            // 
            this.executableField.Location = new System.Drawing.Point(123, 157);
            this.executableField.Name = "executableField";
            this.executableField.Size = new System.Drawing.Size(466, 20);
            this.executableField.TabIndex = 7;
            this.executableField.Text = "C:\\wwiv\\bbs.exe";
            // 
            // highNodeSpinner
            // 
            this.highNodeSpinner.Location = new System.Drawing.Point(273, 82);
            this.highNodeSpinner.Name = "highNodeSpinner";
            this.highNodeSpinner.Size = new System.Drawing.Size(120, 20);
            this.highNodeSpinner.TabIndex = 6;
            this.highNodeSpinner.Value = new decimal(new int[] {
            4,
            0,
            0,
            0});
            // 
            // labelEndNode
            // 
            this.labelEndNode.AutoSize = true;
            this.labelEndNode.Location = new System.Drawing.Point(250, 89);
            this.labelEndNode.Name = "labelEndNode";
            this.labelEndNode.Size = new System.Drawing.Size(16, 13);
            this.labelEndNode.TabIndex = 5;
            this.labelEndNode.Text = "to";
            // 
            // lowNodeSpinner
            // 
            this.lowNodeSpinner.Location = new System.Drawing.Point(123, 83);
            this.lowNodeSpinner.Name = "lowNodeSpinner";
            this.lowNodeSpinner.Size = new System.Drawing.Size(120, 20);
            this.lowNodeSpinner.TabIndex = 4;
            this.lowNodeSpinner.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
            // 
            // labelStartNode
            // 
            this.labelStartNode.AutoSize = true;
            this.labelStartNode.Location = new System.Drawing.Point(54, 85);
            this.labelStartNode.Name = "labelStartNode";
            this.labelStartNode.Size = new System.Drawing.Size(63, 13);
            this.labelStartNode.TabIndex = 3;
            this.labelStartNode.Text = "Use Nodes:";
            this.labelStartNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // portSpinner
            // 
            this.portSpinner.Location = new System.Drawing.Point(123, 31);
            this.portSpinner.Maximum = new decimal(new int[] {
            65536,
            0,
            0,
            0});
            this.portSpinner.Name = "portSpinner";
            this.portSpinner.Size = new System.Drawing.Size(120, 20);
            this.portSpinner.TabIndex = 2;
            this.portSpinner.Value = new decimal(new int[] {
            23,
            0,
            0,
            0});
            // 
            // labelPort
            // 
            this.labelPort.AutoSize = true;
            this.labelPort.Location = new System.Drawing.Point(16, 33);
            this.labelPort.Name = "labelPort";
            this.labelPort.Size = new System.Drawing.Size(101, 13);
            this.labelPort.TabIndex = 1;
            this.labelPort.Text = "Telnet TCP/IP Port:";
            this.labelPort.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            // 
            // autostartCheckBox
            // 
            this.autostartCheckBox.AutoSize = true;
            this.autostartCheckBox.Location = new System.Drawing.Point(7, 7);
            this.autostartCheckBox.Name = "autostartCheckBox";
            this.autostartCheckBox.Size = new System.Drawing.Size(186, 17);
            this.autostartCheckBox.TabIndex = 0;
            this.autostartCheckBox.Text = "Automatically Start Telnet Server?";
            this.autostartCheckBox.UseVisualStyleBackColor = true;
            // 
            // loggingTab
            // 
            this.loggingTab.Location = new System.Drawing.Point(4, 22);
            this.loggingTab.Name = "loggingTab";
            this.loggingTab.Padding = new System.Windows.Forms.Padding(3);
            this.loggingTab.Size = new System.Drawing.Size(686, 392);
            this.loggingTab.TabIndex = 1;
            this.loggingTab.Text = "Logging (Unused)";
            this.loggingTab.UseVisualStyleBackColor = true;
            // 
            // PreferencesDialog
            // 
            this.AcceptButton = this.okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.cancelButton;
            this.ClientSize = new System.Drawing.Size(718, 471);
            this.Controls.Add(this.tabControlPreferences);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.okButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "PreferencesDialog";
            this.Text = "Preferences";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.PreferencesDialog_FormClosed);
            this.Load += new System.EventHandler(this.PreferencesDialog_Load);
            this.tabControlPreferences.ResumeLayout(false);
            this.generalTab.ResumeLayout(false);
            this.generalTab.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.localNodeSpinner)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.highNodeSpinner)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.lowNodeSpinner)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.portSpinner)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button okButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.TabControl tabControlPreferences;
        private System.Windows.Forms.TabPage generalTab;
        private System.Windows.Forms.Label labelPort;
        private System.Windows.Forms.CheckBox autostartCheckBox;
        private System.Windows.Forms.TabPage loggingTab;
        private System.Windows.Forms.Label labelExecutable;
        private System.Windows.Forms.Label labelHomeDirectory;
        private System.Windows.Forms.Label labelParameters;
        private System.Windows.Forms.TextBox parametersField;
        private System.Windows.Forms.TextBox homeField;
        private System.Windows.Forms.Button buttonBrowseExecutable;
        private System.Windows.Forms.TextBox executableField;
        private System.Windows.Forms.NumericUpDown highNodeSpinner;
        private System.Windows.Forms.Label labelEndNode;
        private System.Windows.Forms.NumericUpDown lowNodeSpinner;
        private System.Windows.Forms.Label labelStartNode;
        private System.Windows.Forms.NumericUpDown portSpinner;
        private System.Windows.Forms.Label labelLocalNode;
        private System.Windows.Forms.NumericUpDown localNodeSpinner;
        private System.Windows.Forms.CheckBox balloonsCheckBox;
        private System.Windows.Forms.CheckBox beginDayCheckBox;
        private System.Windows.Forms.CheckBox launchMinimizedCheckBox;
        private System.Windows.Forms.CheckBox runEventsCheckbox;
        private System.Windows.Forms.CheckBox launchLocalNodeCheckBox;
        private System.Windows.Forms.CheckBox launchNetworkCheckBox;
        private System.Windows.Forms.ComboBox checkUpdates;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label labelSSH;
        private System.Windows.Forms.NumericUpDown sshSpinner;
        private System.Windows.Forms.Label labelParameters2;
        private System.Windows.Forms.TextBox parametersField2;
    }
}
