namespace WWIV5TelnetServer
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
      this.okButton = new System.Windows.Forms.Button();
      this.cancelButton = new System.Windows.Forms.Button();
      this.tabControlPreferences = new System.Windows.Forms.TabControl();
      this.generalTab = new System.Windows.Forms.TabPage();
      this.runEventsCheckbox = new System.Windows.Forms.CheckBox();
      this.labelLocalNode = new System.Windows.Forms.Label();
      this.labelExecutable = new System.Windows.Forms.Label();
      this.labelHomeDirectory = new System.Windows.Forms.Label();
      this.labelParameters = new System.Windows.Forms.Label();
      this.buttonBrowseExecutable = new System.Windows.Forms.Button();
      this.labelEndNode = new System.Windows.Forms.Label();
      this.labelStartNode = new System.Windows.Forms.Label();
      this.labelPort = new System.Windows.Forms.Label();
      this.loggingTab = new System.Windows.Forms.TabPage();
      this.balloonsCheckBox = new System.Windows.Forms.CheckBox();
      this.beginDayCheckBox = new System.Windows.Forms.CheckBox();
      this.launchMinimizedCheckBox = new System.Windows.Forms.CheckBox();
      this.localNodeSpinner = new System.Windows.Forms.NumericUpDown();
      this.parametersField = new System.Windows.Forms.TextBox();
      this.homeField = new System.Windows.Forms.TextBox();
      this.executableField = new System.Windows.Forms.TextBox();
      this.highNodeSpinner = new System.Windows.Forms.NumericUpDown();
      this.lowNodeSpinner = new System.Windows.Forms.NumericUpDown();
      this.portSpinner = new System.Windows.Forms.NumericUpDown();
      this.autostartCheckBox = new System.Windows.Forms.CheckBox();
      this.tabControlPreferences.SuspendLayout();
      this.generalTab.SuspendLayout();
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
      this.okButton.Location = new System.Drawing.Point(631, 387);
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
      this.cancelButton.Location = new System.Drawing.Point(550, 387);
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
      this.tabControlPreferences.Size = new System.Drawing.Size(694, 369);
      this.tabControlPreferences.TabIndex = 2;
      // 
      // generalTab
      // 
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
      this.generalTab.Size = new System.Drawing.Size(686, 343);
      this.generalTab.TabIndex = 0;
      this.generalTab.Text = "General";
      this.generalTab.UseVisualStyleBackColor = true;
      // 
      // runEventsCheckbox
      // 
      this.runEventsCheckbox.AutoSize = true;
      this.runEventsCheckbox.Enabled = false;
      this.runEventsCheckbox.Location = new System.Drawing.Point(22, 319);
      this.runEventsCheckbox.Name = "runEventsCheckbox";
      this.runEventsCheckbox.Size = new System.Drawing.Size(226, 17);
      this.runEventsCheckbox.TabIndex = 19;
      this.runEventsCheckbox.Text = "Run All WWIV Events? (Not Implemented)";
      this.runEventsCheckbox.UseVisualStyleBackColor = true;
      // 
      // labelLocalNode
      // 
      this.labelLocalNode.AutoSize = true;
      this.labelLocalNode.Location = new System.Drawing.Point(52, 89);
      this.labelLocalNode.Name = "labelLocalNode";
      this.labelLocalNode.Size = new System.Drawing.Size(65, 13);
      this.labelLocalNode.TabIndex = 15;
      this.labelLocalNode.Text = "Local Node:";
      this.labelLocalNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelExecutable
      // 
      this.labelExecutable.AutoSize = true;
      this.labelExecutable.Location = new System.Drawing.Point(19, 137);
      this.labelExecutable.Name = "labelExecutable";
      this.labelExecutable.Size = new System.Drawing.Size(98, 13);
      this.labelExecutable.TabIndex = 13;
      this.labelExecutable.Text = "WWIV Executable:";
      this.labelExecutable.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelHomeDirectory
      // 
      this.labelHomeDirectory.AutoSize = true;
      this.labelHomeDirectory.Location = new System.Drawing.Point(34, 168);
      this.labelHomeDirectory.Name = "labelHomeDirectory";
      this.labelHomeDirectory.Size = new System.Drawing.Size(83, 13);
      this.labelHomeDirectory.TabIndex = 12;
      this.labelHomeDirectory.Text = "Home Directory:";
      this.labelHomeDirectory.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelParameters
      // 
      this.labelParameters.AutoSize = true;
      this.labelParameters.Location = new System.Drawing.Point(19, 191);
      this.labelParameters.Name = "labelParameters";
      this.labelParameters.Size = new System.Drawing.Size(98, 13);
      this.labelParameters.TabIndex = 11;
      this.labelParameters.Text = "WWIV Parameters:";
      this.labelParameters.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // buttonBrowseExecutable
      // 
      this.buttonBrowseExecutable.Location = new System.Drawing.Point(595, 134);
      this.buttonBrowseExecutable.Name = "buttonBrowseExecutable";
      this.buttonBrowseExecutable.Size = new System.Drawing.Size(75, 23);
      this.buttonBrowseExecutable.TabIndex = 8;
      this.buttonBrowseExecutable.Text = "Browse";
      this.buttonBrowseExecutable.UseVisualStyleBackColor = true;
      // 
      // labelEndNode
      // 
      this.labelEndNode.AutoSize = true;
      this.labelEndNode.Location = new System.Drawing.Point(250, 66);
      this.labelEndNode.Name = "labelEndNode";
      this.labelEndNode.Size = new System.Drawing.Size(16, 13);
      this.labelEndNode.TabIndex = 5;
      this.labelEndNode.Text = "to";
      // 
      // labelStartNode
      // 
      this.labelStartNode.AutoSize = true;
      this.labelStartNode.Location = new System.Drawing.Point(54, 62);
      this.labelStartNode.Name = "labelStartNode";
      this.labelStartNode.Size = new System.Drawing.Size(63, 13);
      this.labelStartNode.TabIndex = 3;
      this.labelStartNode.Text = "Use Nodes:";
      this.labelStartNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelPort
      // 
      this.labelPort.AutoSize = true;
      this.labelPort.Location = new System.Drawing.Point(49, 33);
      this.labelPort.Name = "labelPort";
      this.labelPort.Size = new System.Drawing.Size(68, 13);
      this.labelPort.TabIndex = 1;
      this.labelPort.Text = "TCP/IP Port:";
      this.labelPort.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // loggingTab
      // 
      this.loggingTab.Location = new System.Drawing.Point(4, 22);
      this.loggingTab.Name = "loggingTab";
      this.loggingTab.Padding = new System.Windows.Forms.Padding(3);
      this.loggingTab.Size = new System.Drawing.Size(686, 343);
      this.loggingTab.TabIndex = 1;
      this.loggingTab.Text = "Logging (Unused)";
      this.loggingTab.UseVisualStyleBackColor = true;
      // 
      // balloonsCheckBox
      // 
      this.balloonsCheckBox.AutoSize = true;
      this.balloonsCheckBox.Location = new System.Drawing.Point(22, 295);
      this.balloonsCheckBox.Name = "balloonsCheckBox";
      this.balloonsCheckBox.Size = new System.Drawing.Size(128, 17);
      this.balloonsCheckBox.TabIndex = 18;
      this.balloonsCheckBox.Text = "Use Balloon Popups?";
      this.balloonsCheckBox.UseVisualStyleBackColor = true;
      // 
      // beginDayCheckBox
      // 
      this.beginDayCheckBox.AutoSize = true;
      this.beginDayCheckBox.Enabled = false;
      this.beginDayCheckBox.Location = new System.Drawing.Point(22, 271);
      this.beginDayCheckBox.Name = "beginDayCheckBox";
      this.beginDayCheckBox.Size = new System.Drawing.Size(130, 17);
      this.beginDayCheckBox.TabIndex = 17;
      this.beginDayCheckBox.Text = "Run Beginday Event?";
      this.beginDayCheckBox.UseVisualStyleBackColor = true;
      // 
      // launchMinimizedCheckBox
      // 
      this.launchMinimizedCheckBox.AutoSize = true;
      this.launchMinimizedCheckBox.Location = new System.Drawing.Point(22, 247);
      this.launchMinimizedCheckBox.Name = "launchMinimizedCheckBox";
      this.launchMinimizedCheckBox.Size = new System.Drawing.Size(152, 17);
      this.launchMinimizedCheckBox.TabIndex = 16;
      this.launchMinimizedCheckBox.Text = "Launch WWIV Minimized?";
      this.launchMinimizedCheckBox.UseVisualStyleBackColor = true;
      // 
      // localNodeSpinner
      // 
      this.localNodeSpinner.Location = new System.Drawing.Point(123, 87);
      this.localNodeSpinner.Name = "localNodeSpinner";
      this.localNodeSpinner.Size = new System.Drawing.Size(120, 20);
      this.localNodeSpinner.TabIndex = 7;
      this.localNodeSpinner.Value = new decimal(new int[] {
            1,
            0,
            0,
            0});
      // 
      // parametersField
      // 
      this.parametersField.Location = new System.Drawing.Point(123, 188);
      this.parametersField.Name = "parametersField";
      this.parametersField.Size = new System.Drawing.Size(466, 20);
      this.parametersField.TabIndex = 10;
      this.parametersField.Text = "-XT -H@H -N@N";
      // 
      // homeField
      // 
      this.homeField.Location = new System.Drawing.Point(123, 161);
      this.homeField.Name = "homeField";
      this.homeField.Size = new System.Drawing.Size(466, 20);
      this.homeField.TabIndex = 9;
      this.homeField.Text = "C:\\bbs";
      // 
      // executableField
      // 
      this.executableField.Location = new System.Drawing.Point(123, 134);
      this.executableField.Name = "executableField";
      this.executableField.Size = new System.Drawing.Size(466, 20);
      this.executableField.TabIndex = 7;
      this.executableField.Text = "C:\\bbs\\bbs.exe";
      // 
      // highNodeSpinner
      // 
      this.highNodeSpinner.Location = new System.Drawing.Point(273, 59);
      this.highNodeSpinner.Name = "highNodeSpinner";
      this.highNodeSpinner.Size = new System.Drawing.Size(120, 20);
      this.highNodeSpinner.TabIndex = 6;
      this.highNodeSpinner.Value = new decimal(new int[] {
            4,
            0,
            0,
            0});
      // 
      // lowNodeSpinner
      // 
      this.lowNodeSpinner.Location = new System.Drawing.Point(123, 60);
      this.lowNodeSpinner.Name = "lowNodeSpinner";
      this.lowNodeSpinner.Size = new System.Drawing.Size(120, 20);
      this.lowNodeSpinner.TabIndex = 4;
      this.lowNodeSpinner.Value = new decimal(new int[] {
            2,
            0,
            0,
            0});
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
      // autostartCheckBox
      // 
      this.autostartCheckBox.AutoSize = true;
      this.autostartCheckBox.Location = new System.Drawing.Point(7, 7);
      this.autostartCheckBox.Name = "autostartCheckBox";
      this.autostartCheckBox.Size = new System.Drawing.Size(184, 17);
      this.autostartCheckBox.TabIndex = 0;
      this.autostartCheckBox.Text = "Automatically start Telnet Server?";
      this.autostartCheckBox.UseVisualStyleBackColor = true;
      // 
      // PreferencesDialog
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(718, 422);
      this.Controls.Add(this.tabControlPreferences);
      this.Controls.Add(this.cancelButton);
      this.Controls.Add(this.okButton);
      this.Name = "PreferencesDialog";
      this.Text = "Preferences";
      this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.PreferencesDialog_FormClosed);
      this.Load += new System.EventHandler(this.PreferencesDialog_Load);
      this.tabControlPreferences.ResumeLayout(false);
      this.generalTab.ResumeLayout(false);
      this.generalTab.PerformLayout();
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
    }
}