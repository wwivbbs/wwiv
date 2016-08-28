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
      System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PreferencesDialog));
      this.okButton = new System.Windows.Forms.Button();
      this.cancelButton = new System.Windows.Forms.Button();
      this.tabControlPreferences = new System.Windows.Forms.TabControl();
      this.generalTab = new System.Windows.Forms.TabPage();
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
      this.homeField = new System.Windows.Forms.TextBox();
      this.buttonBrowseExecutable = new System.Windows.Forms.Button();
      this.executableField = new System.Windows.Forms.TextBox();
      this.highNodeSpinner = new System.Windows.Forms.NumericUpDown();
      this.labelEndNode = new System.Windows.Forms.Label();
      this.lowNodeSpinner = new System.Windows.Forms.NumericUpDown();
      this.labelStartNode = new System.Windows.Forms.Label();
      this.autostartCheckBox = new System.Windows.Forms.CheckBox();
      this.telnetTab = new System.Windows.Forms.TabPage();
      this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
      this.parametersField = new System.Windows.Forms.TextBox();
      this.portSpinner = new System.Windows.Forms.NumericUpDown();
      this.labelPort = new System.Windows.Forms.Label();
      this.labelParameters = new System.Windows.Forms.Label();
      this.sshTab = new System.Windows.Forms.TabPage();
      this.sshSpinner = new System.Windows.Forms.NumericUpDown();
      this.labelSSH = new System.Windows.Forms.Label();
      this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
      this.labelParameters2 = new System.Windows.Forms.Label();
      this.parametersField2 = new System.Windows.Forms.TextBox();
      this.buttonBrowseHomeDir = new System.Windows.Forms.Button();
      this.blockTab = new System.Windows.Forms.TabPage();
      this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
      this.useBadIp = new System.Windows.Forms.CheckBox();
      this.autoBan = new System.Windows.Forms.CheckBox();
      this.useGoodIp = new System.Windows.Forms.CheckBox();
      this.lblConcurrent = new System.Windows.Forms.Label();
      this.lblAutoBlacklist = new System.Windows.Forms.Label();
      this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
      this.banCount = new System.Windows.Forms.NumericUpDown();
      this.label2 = new System.Windows.Forms.Label();
      this.banSeconds = new System.Windows.Forms.NumericUpDown();
      this.label3 = new System.Windows.Forms.Label();
      this.maxConcurrent = new System.Windows.Forms.NumericUpDown();
      this.tabControlPreferences.SuspendLayout();
      this.generalTab.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.localNodeSpinner)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.highNodeSpinner)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.lowNodeSpinner)).BeginInit();
      this.telnetTab.SuspendLayout();
      this.tableLayoutPanel1.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.portSpinner)).BeginInit();
      this.sshTab.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).BeginInit();
      this.tableLayoutPanel2.SuspendLayout();
      this.blockTab.SuspendLayout();
      this.tableLayoutPanel3.SuspendLayout();
      this.flowLayoutPanel1.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.banCount)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.banSeconds)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.maxConcurrent)).BeginInit();
      this.SuspendLayout();
      // 
      // okButton
      // 
      this.okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.okButton.Location = new System.Drawing.Point(476, 329);
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
      this.cancelButton.Location = new System.Drawing.Point(395, 329);
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
      this.tabControlPreferences.Controls.Add(this.telnetTab);
      this.tabControlPreferences.Controls.Add(this.sshTab);
      this.tabControlPreferences.Controls.Add(this.blockTab);
      this.tabControlPreferences.Location = new System.Drawing.Point(12, 12);
      this.tabControlPreferences.Name = "tabControlPreferences";
      this.tabControlPreferences.SelectedIndex = 0;
      this.tabControlPreferences.Size = new System.Drawing.Size(539, 311);
      this.tabControlPreferences.TabIndex = 2;
      // 
      // generalTab
      // 
      this.generalTab.Controls.Add(this.buttonBrowseHomeDir);
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
      this.generalTab.Controls.Add(this.homeField);
      this.generalTab.Controls.Add(this.buttonBrowseExecutable);
      this.generalTab.Controls.Add(this.executableField);
      this.generalTab.Controls.Add(this.highNodeSpinner);
      this.generalTab.Controls.Add(this.labelEndNode);
      this.generalTab.Controls.Add(this.lowNodeSpinner);
      this.generalTab.Controls.Add(this.labelStartNode);
      this.generalTab.Controls.Add(this.autostartCheckBox);
      this.generalTab.Location = new System.Drawing.Point(4, 22);
      this.generalTab.Name = "generalTab";
      this.generalTab.Padding = new System.Windows.Forms.Padding(3);
      this.generalTab.Size = new System.Drawing.Size(531, 285);
      this.generalTab.TabIndex = 0;
      this.generalTab.Text = "General";
      this.generalTab.UseVisualStyleBackColor = true;
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
      this.checkUpdates.Location = new System.Drawing.Point(393, 226);
      this.checkUpdates.Name = "checkUpdates";
      this.checkUpdates.RightToLeft = System.Windows.Forms.RightToLeft.No;
      this.checkUpdates.Size = new System.Drawing.Size(121, 21);
      this.checkUpdates.TabIndex = 23;
      this.checkUpdates.Text = "UpdatePref";
      // 
      // label1
      // 
      this.label1.AutoSize = true;
      this.label1.Location = new System.Drawing.Point(250, 231);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(137, 13);
      this.label1.TabIndex = 22;
      this.label1.Text = "Check For WWIV Updates:";
      // 
      // launchNetworkCheckBox
      // 
      this.launchNetworkCheckBox.AutoSize = true;
      this.launchNetworkCheckBox.Location = new System.Drawing.Point(253, 206);
      this.launchNetworkCheckBox.Name = "launchNetworkCheckBox";
      this.launchNetworkCheckBox.Size = new System.Drawing.Size(174, 17);
      this.launchNetworkCheckBox.TabIndex = 21;
      this.launchNetworkCheckBox.Text = "Launch WWIVNet On Startup?";
      this.launchNetworkCheckBox.UseVisualStyleBackColor = true;
      // 
      // launchLocalNodeCheckBox
      // 
      this.launchLocalNodeCheckBox.AutoSize = true;
      this.launchLocalNodeCheckBox.Location = new System.Drawing.Point(253, 182);
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
      this.runEventsCheckbox.Location = new System.Drawing.Point(22, 254);
      this.runEventsCheckbox.Name = "runEventsCheckbox";
      this.runEventsCheckbox.Size = new System.Drawing.Size(226, 17);
      this.runEventsCheckbox.TabIndex = 19;
      this.runEventsCheckbox.Text = "Run All WWIV Events? (Not Implemented)";
      this.runEventsCheckbox.UseVisualStyleBackColor = true;
      // 
      // balloonsCheckBox
      // 
      this.balloonsCheckBox.AutoSize = true;
      this.balloonsCheckBox.Location = new System.Drawing.Point(22, 230);
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
      this.beginDayCheckBox.Location = new System.Drawing.Point(22, 206);
      this.beginDayCheckBox.Name = "beginDayCheckBox";
      this.beginDayCheckBox.Size = new System.Drawing.Size(130, 17);
      this.beginDayCheckBox.TabIndex = 17;
      this.beginDayCheckBox.Text = "Run Beginday Event?";
      this.beginDayCheckBox.UseVisualStyleBackColor = true;
      // 
      // launchMinimizedCheckBox
      // 
      this.launchMinimizedCheckBox.AutoSize = true;
      this.launchMinimizedCheckBox.Location = new System.Drawing.Point(22, 182);
      this.launchMinimizedCheckBox.Name = "launchMinimizedCheckBox";
      this.launchMinimizedCheckBox.Size = new System.Drawing.Size(152, 17);
      this.launchMinimizedCheckBox.TabIndex = 16;
      this.launchMinimizedCheckBox.Text = "Launch WWIV Minimized?";
      this.launchMinimizedCheckBox.UseVisualStyleBackColor = true;
      // 
      // labelLocalNode
      // 
      this.labelLocalNode.AutoSize = true;
      this.labelLocalNode.Location = new System.Drawing.Point(52, 67);
      this.labelLocalNode.Name = "labelLocalNode";
      this.labelLocalNode.Size = new System.Drawing.Size(65, 13);
      this.labelLocalNode.TabIndex = 15;
      this.labelLocalNode.Text = "Local Node:";
      this.labelLocalNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // localNodeSpinner
      // 
      this.localNodeSpinner.Location = new System.Drawing.Point(123, 65);
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
      this.labelExecutable.Location = new System.Drawing.Point(19, 115);
      this.labelExecutable.Name = "labelExecutable";
      this.labelExecutable.Size = new System.Drawing.Size(98, 13);
      this.labelExecutable.TabIndex = 13;
      this.labelExecutable.Text = "WWIV Executable:";
      this.labelExecutable.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelHomeDirectory
      // 
      this.labelHomeDirectory.AutoSize = true;
      this.labelHomeDirectory.Location = new System.Drawing.Point(34, 146);
      this.labelHomeDirectory.Name = "labelHomeDirectory";
      this.labelHomeDirectory.Size = new System.Drawing.Size(83, 13);
      this.labelHomeDirectory.TabIndex = 12;
      this.labelHomeDirectory.Text = "Home Directory:";
      this.labelHomeDirectory.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // homeField
      // 
      this.homeField.Location = new System.Drawing.Point(123, 139);
      this.homeField.Name = "homeField";
      this.homeField.Size = new System.Drawing.Size(310, 20);
      this.homeField.TabIndex = 9;
      this.homeField.Text = "C:\\wwiv";
      // 
      // buttonBrowseExecutable
      // 
      this.buttonBrowseExecutable.Location = new System.Drawing.Point(439, 112);
      this.buttonBrowseExecutable.Name = "buttonBrowseExecutable";
      this.buttonBrowseExecutable.Size = new System.Drawing.Size(75, 23);
      this.buttonBrowseExecutable.TabIndex = 8;
      this.buttonBrowseExecutable.Text = "Browse";
      this.buttonBrowseExecutable.UseVisualStyleBackColor = true;
      this.buttonBrowseExecutable.Click += new System.EventHandler(this.buttonBrowseExecutable_Click);
      // 
      // executableField
      // 
      this.executableField.Location = new System.Drawing.Point(123, 112);
      this.executableField.Name = "executableField";
      this.executableField.Size = new System.Drawing.Size(310, 20);
      this.executableField.TabIndex = 7;
      this.executableField.Text = "C:\\wwiv\\bbs.exe";
      // 
      // highNodeSpinner
      // 
      this.highNodeSpinner.Location = new System.Drawing.Point(273, 37);
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
      this.labelEndNode.Location = new System.Drawing.Point(250, 44);
      this.labelEndNode.Name = "labelEndNode";
      this.labelEndNode.Size = new System.Drawing.Size(16, 13);
      this.labelEndNode.TabIndex = 5;
      this.labelEndNode.Text = "to";
      // 
      // lowNodeSpinner
      // 
      this.lowNodeSpinner.Location = new System.Drawing.Point(123, 38);
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
      this.labelStartNode.Location = new System.Drawing.Point(54, 40);
      this.labelStartNode.Name = "labelStartNode";
      this.labelStartNode.Size = new System.Drawing.Size(63, 13);
      this.labelStartNode.TabIndex = 3;
      this.labelStartNode.Text = "Use Nodes:";
      this.labelStartNode.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // autostartCheckBox
      // 
      this.autostartCheckBox.AutoSize = true;
      this.autostartCheckBox.Location = new System.Drawing.Point(7, 7);
      this.autostartCheckBox.Name = "autostartCheckBox";
      this.autostartCheckBox.Size = new System.Drawing.Size(195, 17);
      this.autostartCheckBox.TabIndex = 0;
      this.autostartCheckBox.Text = "Automatically Start Socket Servers?";
      this.autostartCheckBox.UseVisualStyleBackColor = true;
      // 
      // telnetTab
      // 
      this.telnetTab.Controls.Add(this.tableLayoutPanel1);
      this.telnetTab.Location = new System.Drawing.Point(4, 22);
      this.telnetTab.Name = "telnetTab";
      this.telnetTab.Padding = new System.Windows.Forms.Padding(3);
      this.telnetTab.Size = new System.Drawing.Size(531, 285);
      this.telnetTab.TabIndex = 1;
      this.telnetTab.Text = "Telnet";
      this.telnetTab.UseVisualStyleBackColor = true;
      // 
      // tableLayoutPanel1
      // 
      this.tableLayoutPanel1.ColumnCount = 2;
      this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 20.47478F));
      this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 79.52522F));
      this.tableLayoutPanel1.Controls.Add(this.parametersField, 1, 1);
      this.tableLayoutPanel1.Controls.Add(this.portSpinner, 1, 0);
      this.tableLayoutPanel1.Controls.Add(this.labelPort, 0, 0);
      this.tableLayoutPanel1.Controls.Add(this.labelParameters, 0, 1);
      this.tableLayoutPanel1.Location = new System.Drawing.Point(4, 3);
      this.tableLayoutPanel1.Name = "tableLayoutPanel1";
      this.tableLayoutPanel1.RowCount = 2;
      this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
      this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
      this.tableLayoutPanel1.Size = new System.Drawing.Size(674, 61);
      this.tableLayoutPanel1.TabIndex = 6;
      // 
      // parametersField
      // 
      this.parametersField.Location = new System.Drawing.Point(141, 33);
      this.parametersField.Name = "parametersField";
      this.parametersField.Size = new System.Drawing.Size(466, 20);
      this.parametersField.TabIndex = 12;
      this.parametersField.Text = "-XT -H@H -N@N";
      // 
      // portSpinner
      // 
      this.portSpinner.Location = new System.Drawing.Point(141, 3);
      this.portSpinner.Maximum = new decimal(new int[] {
            65536,
            0,
            0,
            0});
      this.portSpinner.Name = "portSpinner";
      this.portSpinner.Size = new System.Drawing.Size(120, 20);
      this.portSpinner.TabIndex = 15;
      this.portSpinner.Value = new decimal(new int[] {
            23,
            0,
            0,
            0});
      // 
      // labelPort
      // 
      this.labelPort.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.labelPort.AutoSize = true;
      this.labelPort.Location = new System.Drawing.Point(34, 0);
      this.labelPort.Name = "labelPort";
      this.labelPort.Size = new System.Drawing.Size(101, 13);
      this.labelPort.TabIndex = 14;
      this.labelPort.Text = "Telnet TCP/IP Port:";
      this.labelPort.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // labelParameters
      // 
      this.labelParameters.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.labelParameters.AutoSize = true;
      this.labelParameters.Location = new System.Drawing.Point(39, 30);
      this.labelParameters.Name = "labelParameters";
      this.labelParameters.Size = new System.Drawing.Size(96, 13);
      this.labelParameters.TabIndex = 13;
      this.labelParameters.Text = "Telnet Parameters:";
      this.labelParameters.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // sshTab
      // 
      this.sshTab.Controls.Add(this.tableLayoutPanel2);
      this.sshTab.Location = new System.Drawing.Point(4, 22);
      this.sshTab.Name = "sshTab";
      this.sshTab.Size = new System.Drawing.Size(531, 285);
      this.sshTab.TabIndex = 2;
      this.sshTab.Text = "SSH";
      this.sshTab.UseVisualStyleBackColor = true;
      // 
      // sshSpinner
      // 
      this.sshSpinner.Location = new System.Drawing.Point(102, 3);
      this.sshSpinner.Maximum = new decimal(new int[] {
            65536,
            0,
            0,
            0});
      this.sshSpinner.Name = "sshSpinner";
      this.sshSpinner.Size = new System.Drawing.Size(120, 20);
      this.sshSpinner.TabIndex = 27;
      this.sshSpinner.Value = new decimal(new int[] {
            22,
            0,
            0,
            0});
      // 
      // labelSSH
      // 
      this.labelSSH.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.labelSSH.AutoSize = true;
      this.labelSSH.Location = new System.Drawing.Point(3, 0);
      this.labelSSH.Name = "labelSSH";
      this.labelSSH.Size = new System.Drawing.Size(93, 13);
      this.labelSSH.TabIndex = 26;
      this.labelSSH.Text = "SSH TCP/IP Port:";
      // 
      // tableLayoutPanel2
      // 
      this.tableLayoutPanel2.ColumnCount = 2;
      this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
      this.tableLayoutPanel2.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.tableLayoutPanel2.Controls.Add(this.parametersField2, 1, 1);
      this.tableLayoutPanel2.Controls.Add(this.labelParameters2, 0, 1);
      this.tableLayoutPanel2.Controls.Add(this.labelSSH, 0, 0);
      this.tableLayoutPanel2.Controls.Add(this.sshSpinner, 1, 0);
      this.tableLayoutPanel2.Location = new System.Drawing.Point(3, 3);
      this.tableLayoutPanel2.Name = "tableLayoutPanel2";
      this.tableLayoutPanel2.RowCount = 2;
      this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
      this.tableLayoutPanel2.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 50F));
      this.tableLayoutPanel2.Size = new System.Drawing.Size(680, 59);
      this.tableLayoutPanel2.TabIndex = 28;
      // 
      // labelParameters2
      // 
      this.labelParameters2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.labelParameters2.AutoSize = true;
      this.labelParameters2.Location = new System.Drawing.Point(8, 29);
      this.labelParameters2.Name = "labelParameters2";
      this.labelParameters2.Size = new System.Drawing.Size(88, 13);
      this.labelParameters2.TabIndex = 30;
      this.labelParameters2.Text = "SSH Parameters:";
      this.labelParameters2.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // parametersField2
      // 
      this.parametersField2.Location = new System.Drawing.Point(102, 32);
      this.parametersField2.Name = "parametersField2";
      this.parametersField2.Size = new System.Drawing.Size(466, 20);
      this.parametersField2.TabIndex = 29;
      this.parametersField2.Text = "-XS -H@H -N@N";
      // 
      // buttonBrowseHomeDir
      // 
      this.buttonBrowseHomeDir.Location = new System.Drawing.Point(439, 136);
      this.buttonBrowseHomeDir.Name = "buttonBrowseHomeDir";
      this.buttonBrowseHomeDir.Size = new System.Drawing.Size(75, 23);
      this.buttonBrowseHomeDir.TabIndex = 24;
      this.buttonBrowseHomeDir.Text = "Browse";
      this.buttonBrowseHomeDir.UseVisualStyleBackColor = true;
      this.buttonBrowseHomeDir.Click += new System.EventHandler(this.buttonBrowseHomeDir_Click);
      // 
      // blockTab
      // 
      this.blockTab.Controls.Add(this.tableLayoutPanel3);
      this.blockTab.Location = new System.Drawing.Point(4, 22);
      this.blockTab.Name = "blockTab";
      this.blockTab.Size = new System.Drawing.Size(531, 285);
      this.blockTab.TabIndex = 3;
      this.blockTab.Text = "Blocking";
      this.blockTab.UseVisualStyleBackColor = true;
      // 
      // tableLayoutPanel3
      // 
      this.tableLayoutPanel3.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.tableLayoutPanel3.ColumnCount = 2;
      this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
      this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.tableLayoutPanel3.Controls.Add(this.lblAutoBlacklist, 0, 4);
      this.tableLayoutPanel3.Controls.Add(this.flowLayoutPanel1, 1, 4);
      this.tableLayoutPanel3.Controls.Add(this.lblConcurrent, 0, 2);
      this.tableLayoutPanel3.Controls.Add(this.maxConcurrent, 1, 2);
      this.tableLayoutPanel3.Controls.Add(this.autoBan, 1, 3);
      this.tableLayoutPanel3.Controls.Add(this.useBadIp, 0, 0);
      this.tableLayoutPanel3.Controls.Add(this.useGoodIp, 0, 1);
      this.tableLayoutPanel3.Location = new System.Drawing.Point(4, 4);
      this.tableLayoutPanel3.Name = "tableLayoutPanel3";
      this.tableLayoutPanel3.RowCount = 7;
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.Size = new System.Drawing.Size(527, 278);
      this.tableLayoutPanel3.TabIndex = 0;
      this.tableLayoutPanel3.Paint += new System.Windows.Forms.PaintEventHandler(this.tableLayoutPanel3_Paint);
      // 
      // useBadIp
      // 
      this.useBadIp.AutoSize = true;
      this.useBadIp.Location = new System.Drawing.Point(3, 3);
      this.useBadIp.Name = "useBadIp";
      this.useBadIp.Size = new System.Drawing.Size(104, 17);
      this.useBadIp.TabIndex = 0;
      this.useBadIp.Text = "Use BADIP.TXT";
      this.useBadIp.UseVisualStyleBackColor = true;
      // 
      // autoBan
      // 
      this.autoBan.AutoSize = true;
      this.autoBan.Location = new System.Drawing.Point(136, 75);
      this.autoBan.Name = "autoBan";
      this.autoBan.Size = new System.Drawing.Size(141, 17);
      this.autoBan.TabIndex = 1;
      this.autoBan.Text = "Auto Add to BADIP.TXT";
      this.autoBan.UseVisualStyleBackColor = true;
      this.autoBan.CheckedChanged += new System.EventHandler(this.checkBox2_CheckedChanged);
      // 
      // useGoodIp
      // 
      this.useGoodIp.AutoSize = true;
      this.useGoodIp.Location = new System.Drawing.Point(3, 26);
      this.useGoodIp.Name = "useGoodIp";
      this.useGoodIp.Size = new System.Drawing.Size(114, 17);
      this.useGoodIp.TabIndex = 1;
      this.useGoodIp.Text = "Use GOODIP.TXT";
      this.useGoodIp.UseVisualStyleBackColor = true;
      // 
      // lblConcurrent
      // 
      this.lblConcurrent.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.lblConcurrent.AutoSize = true;
      this.lblConcurrent.Location = new System.Drawing.Point(3, 46);
      this.lblConcurrent.Name = "lblConcurrent";
      this.lblConcurrent.Size = new System.Drawing.Size(127, 13);
      this.lblConcurrent.TabIndex = 2;
      this.lblConcurrent.Text = "Max concurrent sessions:";
      this.lblConcurrent.Click += new System.EventHandler(this.label2_Click);
      // 
      // lblAutoBlacklist
      // 
      this.lblAutoBlacklist.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.lblAutoBlacklist.AutoSize = true;
      this.lblAutoBlacklist.Location = new System.Drawing.Point(34, 95);
      this.lblAutoBlacklist.Name = "lblAutoBlacklist";
      this.lblAutoBlacklist.Size = new System.Drawing.Size(96, 13);
      this.lblAutoBlacklist.TabIndex = 3;
      this.lblAutoBlacklist.Text = "Auto Blacklist After";
      // 
      // flowLayoutPanel1
      // 
      this.flowLayoutPanel1.Controls.Add(this.banCount);
      this.flowLayoutPanel1.Controls.Add(this.label2);
      this.flowLayoutPanel1.Controls.Add(this.banSeconds);
      this.flowLayoutPanel1.Controls.Add(this.label3);
      this.flowLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
      this.flowLayoutPanel1.Location = new System.Drawing.Point(136, 98);
      this.flowLayoutPanel1.Name = "flowLayoutPanel1";
      this.flowLayoutPanel1.Size = new System.Drawing.Size(388, 100);
      this.flowLayoutPanel1.TabIndex = 4;
      // 
      // banCount
      // 
      this.banCount.Location = new System.Drawing.Point(3, 3);
      this.banCount.Name = "banCount";
      this.banCount.Size = new System.Drawing.Size(120, 20);
      this.banCount.TabIndex = 0;
      // 
      // label2
      // 
      this.label2.AutoSize = true;
      this.label2.Location = new System.Drawing.Point(129, 0);
      this.label2.Name = "label2";
      this.label2.Size = new System.Drawing.Size(61, 13);
      this.label2.TabIndex = 1;
      this.label2.Text = "sessions in ";
      // 
      // banSeconds
      // 
      this.banSeconds.Location = new System.Drawing.Point(196, 3);
      this.banSeconds.Name = "banSeconds";
      this.banSeconds.Size = new System.Drawing.Size(120, 20);
      this.banSeconds.TabIndex = 2;
      // 
      // label3
      // 
      this.label3.AutoSize = true;
      this.label3.Location = new System.Drawing.Point(322, 0);
      this.label3.Name = "label3";
      this.label3.Size = new System.Drawing.Size(47, 13);
      this.label3.TabIndex = 3;
      this.label3.Text = "seconds";
      // 
      // maxConcurrent
      // 
      this.maxConcurrent.Location = new System.Drawing.Point(136, 49);
      this.maxConcurrent.Name = "maxConcurrent";
      this.maxConcurrent.Size = new System.Drawing.Size(120, 20);
      this.maxConcurrent.TabIndex = 5;
      // 
      // PreferencesDialog
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(563, 364);
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
      ((System.ComponentModel.ISupportInitialize)(this.localNodeSpinner)).EndInit();
      ((System.ComponentModel.ISupportInitialize)(this.highNodeSpinner)).EndInit();
      ((System.ComponentModel.ISupportInitialize)(this.lowNodeSpinner)).EndInit();
      this.telnetTab.ResumeLayout(false);
      this.tableLayoutPanel1.ResumeLayout(false);
      this.tableLayoutPanel1.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.portSpinner)).EndInit();
      this.sshTab.ResumeLayout(false);
      ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).EndInit();
      this.tableLayoutPanel2.ResumeLayout(false);
      this.tableLayoutPanel2.PerformLayout();
      this.blockTab.ResumeLayout(false);
      this.tableLayoutPanel3.ResumeLayout(false);
      this.tableLayoutPanel3.PerformLayout();
      this.flowLayoutPanel1.ResumeLayout(false);
      this.flowLayoutPanel1.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.banCount)).EndInit();
      ((System.ComponentModel.ISupportInitialize)(this.banSeconds)).EndInit();
      ((System.ComponentModel.ISupportInitialize)(this.maxConcurrent)).EndInit();
      this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button okButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.TabControl tabControlPreferences;
        private System.Windows.Forms.TabPage generalTab;
        private System.Windows.Forms.CheckBox autostartCheckBox;
        private System.Windows.Forms.TabPage telnetTab;
        private System.Windows.Forms.Label labelExecutable;
        private System.Windows.Forms.Label labelHomeDirectory;
        private System.Windows.Forms.TextBox homeField;
        private System.Windows.Forms.Button buttonBrowseExecutable;
        private System.Windows.Forms.TextBox executableField;
        private System.Windows.Forms.NumericUpDown highNodeSpinner;
        private System.Windows.Forms.Label labelEndNode;
        private System.Windows.Forms.NumericUpDown lowNodeSpinner;
        private System.Windows.Forms.Label labelStartNode;
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
    private System.Windows.Forms.TabPage sshTab;
    private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
    private System.Windows.Forms.TextBox parametersField;
    private System.Windows.Forms.NumericUpDown portSpinner;
    private System.Windows.Forms.Label labelPort;
    private System.Windows.Forms.Label labelParameters;
    private System.Windows.Forms.NumericUpDown sshSpinner;
    private System.Windows.Forms.Label labelSSH;
    private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
    private System.Windows.Forms.TextBox parametersField2;
    private System.Windows.Forms.Label labelParameters2;
    private System.Windows.Forms.Button buttonBrowseHomeDir;
    private System.Windows.Forms.TabPage blockTab;
    private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
    private System.Windows.Forms.CheckBox autoBan;
    private System.Windows.Forms.CheckBox useBadIp;
    private System.Windows.Forms.CheckBox useGoodIp;
    private System.Windows.Forms.Label lblConcurrent;
    private System.Windows.Forms.Label lblAutoBlacklist;
    private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
    private System.Windows.Forms.NumericUpDown banCount;
    private System.Windows.Forms.Label label2;
    private System.Windows.Forms.NumericUpDown banSeconds;
    private System.Windows.Forms.Label label3;
    private System.Windows.Forms.NumericUpDown maxConcurrent;
  }
}
