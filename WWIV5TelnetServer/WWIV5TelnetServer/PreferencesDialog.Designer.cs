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
      this.buttonBrowseHomeDir = new System.Windows.Forms.Button();
      this.checkUpdates = new System.Windows.Forms.ComboBox();
      this.label1 = new System.Windows.Forms.Label();
      this.launchLocalNodeCheckBox = new System.Windows.Forms.CheckBox();
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
      this.tableLayoutPanel2 = new System.Windows.Forms.TableLayoutPanel();
      this.parametersField2 = new System.Windows.Forms.TextBox();
      this.labelParameters2 = new System.Windows.Forms.Label();
      this.labelSSH = new System.Windows.Forms.Label();
      this.sshSpinner = new System.Windows.Forms.NumericUpDown();
      this.binkpTab = new System.Windows.Forms.TabPage();
      this.tableLayoutPanel4 = new System.Windows.Forms.TableLayoutPanel();
      this.label7 = new System.Windows.Forms.Label();
      this.binkpPort = new System.Windows.Forms.NumericUpDown();
      this.cbUseBinkp = new System.Windows.Forms.CheckBox();
      this.lblNetworkbParams = new System.Windows.Forms.Label();
      this.label6 = new System.Windows.Forms.Label();
      this.textNetworkbParameters = new System.Windows.Forms.TextBox();
      this.binkpExecutable = new System.Windows.Forms.TextBox();
      this.blockTab = new System.Windows.Forms.TabPage();
      this.tableLayoutPanel3 = new System.Windows.Forms.TableLayoutPanel();
      this.cbPressEsc = new System.Windows.Forms.CheckBox();
      this.lblConcurrent = new System.Windows.Forms.Label();
      this.maxConcurrent = new System.Windows.Forms.NumericUpDown();
      this.useBadIp = new System.Windows.Forms.CheckBox();
      this.useGoodIp = new System.Windows.Forms.CheckBox();
      this.autoBan = new System.Windows.Forms.CheckBox();
      this.lblAutoBlacklist = new System.Windows.Forms.Label();
      this.flowLayoutPanel1 = new System.Windows.Forms.FlowLayoutPanel();
      this.banCount = new System.Windows.Forms.NumericUpDown();
      this.label2 = new System.Windows.Forms.Label();
      this.banSeconds = new System.Windows.Forms.NumericUpDown();
      this.label3 = new System.Windows.Forms.Label();
      this.cbUseDbsRbl = new System.Windows.Forms.CheckBox();
      this.label4 = new System.Windows.Forms.Label();
      this.tbDbsRbl = new System.Windows.Forms.TextBox();
      this.linkBadIpFile = new System.Windows.Forms.LinkLabel();
      this.groupBox1 = new System.Windows.Forms.GroupBox();
      this.layoutBlockCountries = new System.Windows.Forms.TableLayoutPanel();
      this.label5 = new System.Windows.Forms.Label();
      this.tbDnsCC = new System.Windows.Forms.TextBox();
      this.flowLayoutPanel2 = new System.Windows.Forms.FlowLayoutPanel();
      this.btnBadCountryAdd = new System.Windows.Forms.Button();
      this.btnBadCountryRemove = new System.Windows.Forms.Button();
      this.lbBadCountries = new System.Windows.Forms.ListBox();
      this.linkGoodIpFile = new System.Windows.Forms.LinkLabel();
      this.tabControlPreferences.SuspendLayout();
      this.generalTab.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.localNodeSpinner)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.highNodeSpinner)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.lowNodeSpinner)).BeginInit();
      this.telnetTab.SuspendLayout();
      this.tableLayoutPanel1.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.portSpinner)).BeginInit();
      this.sshTab.SuspendLayout();
      this.tableLayoutPanel2.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).BeginInit();
      this.binkpTab.SuspendLayout();
      this.tableLayoutPanel4.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.binkpPort)).BeginInit();
      this.blockTab.SuspendLayout();
      this.tableLayoutPanel3.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.maxConcurrent)).BeginInit();
      this.flowLayoutPanel1.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.banCount)).BeginInit();
      ((System.ComponentModel.ISupportInitialize)(this.banSeconds)).BeginInit();
      this.groupBox1.SuspendLayout();
      this.layoutBlockCountries.SuspendLayout();
      this.flowLayoutPanel2.SuspendLayout();
      this.SuspendLayout();
      // 
      // okButton
      // 
      this.okButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.okButton.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.okButton.Location = new System.Drawing.Point(597, 476);
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
      this.cancelButton.Location = new System.Drawing.Point(516, 476);
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
      this.tabControlPreferences.Controls.Add(this.binkpTab);
      this.tabControlPreferences.Controls.Add(this.blockTab);
      this.tabControlPreferences.Location = new System.Drawing.Point(12, 12);
      this.tabControlPreferences.Name = "tabControlPreferences";
      this.tabControlPreferences.SelectedIndex = 0;
      this.tabControlPreferences.Size = new System.Drawing.Size(660, 458);
      this.tabControlPreferences.TabIndex = 2;
      // 
      // generalTab
      // 
      this.generalTab.Controls.Add(this.buttonBrowseHomeDir);
      this.generalTab.Controls.Add(this.checkUpdates);
      this.generalTab.Controls.Add(this.label1);
      this.generalTab.Controls.Add(this.launchLocalNodeCheckBox);
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
      this.generalTab.Size = new System.Drawing.Size(652, 432);
      this.generalTab.TabIndex = 0;
      this.generalTab.Text = "General";
      this.generalTab.UseVisualStyleBackColor = true;
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
      this.telnetTab.Size = new System.Drawing.Size(652, 432);
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
      this.sshTab.Size = new System.Drawing.Size(652, 432);
      this.sshTab.TabIndex = 2;
      this.sshTab.Text = "SSH";
      this.sshTab.UseVisualStyleBackColor = true;
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
      // parametersField2
      // 
      this.parametersField2.Location = new System.Drawing.Point(102, 32);
      this.parametersField2.Name = "parametersField2";
      this.parametersField2.Size = new System.Drawing.Size(466, 20);
      this.parametersField2.TabIndex = 29;
      this.parametersField2.Text = "-XS -H@H -N@N";
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
      // binkpTab
      // 
      this.binkpTab.Controls.Add(this.tableLayoutPanel4);
      this.binkpTab.Location = new System.Drawing.Point(4, 22);
      this.binkpTab.Name = "binkpTab";
      this.binkpTab.Padding = new System.Windows.Forms.Padding(3);
      this.binkpTab.Size = new System.Drawing.Size(652, 432);
      this.binkpTab.TabIndex = 4;
      this.binkpTab.Text = "BinkP";
      this.binkpTab.UseVisualStyleBackColor = true;
      // 
      // tableLayoutPanel4
      // 
      this.tableLayoutPanel4.ColumnCount = 2;
      this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
      this.tableLayoutPanel4.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.tableLayoutPanel4.Controls.Add(this.label7, 0, 1);
      this.tableLayoutPanel4.Controls.Add(this.binkpPort, 1, 1);
      this.tableLayoutPanel4.Controls.Add(this.cbUseBinkp, 1, 0);
      this.tableLayoutPanel4.Controls.Add(this.lblNetworkbParams, 0, 3);
      this.tableLayoutPanel4.Controls.Add(this.label6, 0, 2);
      this.tableLayoutPanel4.Controls.Add(this.textNetworkbParameters, 1, 3);
      this.tableLayoutPanel4.Controls.Add(this.binkpExecutable, 1, 2);
      this.tableLayoutPanel4.Location = new System.Drawing.Point(5, 5);
      this.tableLayoutPanel4.Name = "tableLayoutPanel4";
      this.tableLayoutPanel4.RowCount = 4;
      this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel4.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel4.Size = new System.Drawing.Size(647, 106);
      this.tableLayoutPanel4.TabIndex = 29;
      // 
      // label7
      // 
      this.label7.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.label7.AutoSize = true;
      this.label7.Location = new System.Drawing.Point(55, 23);
      this.label7.Name = "label7";
      this.label7.Size = new System.Drawing.Size(60, 13);
      this.label7.TabIndex = 26;
      this.label7.Text = "BinkP Port:";
      // 
      // binkpPort
      // 
      this.binkpPort.Location = new System.Drawing.Point(121, 26);
      this.binkpPort.Maximum = new decimal(new int[] {
            65536,
            0,
            0,
            0});
      this.binkpPort.Name = "binkpPort";
      this.binkpPort.Size = new System.Drawing.Size(120, 20);
      this.binkpPort.TabIndex = 27;
      this.binkpPort.Value = new decimal(new int[] {
            22,
            0,
            0,
            0});
      // 
      // cbUseBinkp
      // 
      this.cbUseBinkp.AutoSize = true;
      this.cbUseBinkp.Checked = true;
      this.cbUseBinkp.CheckState = System.Windows.Forms.CheckState.Checked;
      this.cbUseBinkp.Location = new System.Drawing.Point(121, 3);
      this.cbUseBinkp.Name = "cbUseBinkp";
      this.cbUseBinkp.Size = new System.Drawing.Size(175, 17);
      this.cbUseBinkp.TabIndex = 32;
      this.cbUseBinkp.Text = "Listen to BinkP using NetworkB";
      this.cbUseBinkp.UseVisualStyleBackColor = true;
      this.cbUseBinkp.CheckedChanged += new System.EventHandler(this.checkBox1_CheckedChanged);
      // 
      // lblNetworkbParams
      // 
      this.lblNetworkbParams.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.lblNetworkbParams.AutoSize = true;
      this.lblNetworkbParams.Location = new System.Drawing.Point(3, 75);
      this.lblNetworkbParams.Name = "lblNetworkbParams";
      this.lblNetworkbParams.Size = new System.Drawing.Size(112, 13);
      this.lblNetworkbParams.TabIndex = 30;
      this.lblNetworkbParams.Text = "Networkb Parameters:";
      this.lblNetworkbParams.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // label6
      // 
      this.label6.AutoSize = true;
      this.label6.Location = new System.Drawing.Point(3, 49);
      this.label6.Name = "label6";
      this.label6.Size = new System.Drawing.Size(112, 13);
      this.label6.TabIndex = 33;
      this.label6.Text = "Networkb Executable:";
      this.label6.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
      // 
      // textNetworkbParameters
      // 
      this.textNetworkbParameters.Location = new System.Drawing.Point(121, 78);
      this.textNetworkbParameters.Name = "textNetworkbParameters";
      this.textNetworkbParameters.Size = new System.Drawing.Size(466, 20);
      this.textNetworkbParameters.TabIndex = 29;
      this.textNetworkbParameters.Text = "-receive --handle=@H";
      // 
      // binkpExecutable
      // 
      this.binkpExecutable.Location = new System.Drawing.Point(121, 52);
      this.binkpExecutable.Name = "binkpExecutable";
      this.binkpExecutable.Size = new System.Drawing.Size(466, 20);
      this.binkpExecutable.TabIndex = 34;
      // 
      // blockTab
      // 
      this.blockTab.Controls.Add(this.tableLayoutPanel3);
      this.blockTab.Location = new System.Drawing.Point(4, 22);
      this.blockTab.Name = "blockTab";
      this.blockTab.Size = new System.Drawing.Size(652, 432);
      this.blockTab.TabIndex = 3;
      this.blockTab.Text = "Blocking";
      this.blockTab.UseVisualStyleBackColor = true;
      // 
      // tableLayoutPanel3
      // 
      this.tableLayoutPanel3.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.tableLayoutPanel3.AutoSize = true;
      this.tableLayoutPanel3.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
      this.tableLayoutPanel3.ColumnCount = 2;
      this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
      this.tableLayoutPanel3.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.tableLayoutPanel3.Controls.Add(this.cbPressEsc, 0, 0);
      this.tableLayoutPanel3.Controls.Add(this.lblConcurrent, 0, 1);
      this.tableLayoutPanel3.Controls.Add(this.maxConcurrent, 1, 1);
      this.tableLayoutPanel3.Controls.Add(this.useBadIp, 0, 2);
      this.tableLayoutPanel3.Controls.Add(this.useGoodIp, 0, 3);
      this.tableLayoutPanel3.Controls.Add(this.autoBan, 0, 4);
      this.tableLayoutPanel3.Controls.Add(this.lblAutoBlacklist, 0, 5);
      this.tableLayoutPanel3.Controls.Add(this.flowLayoutPanel1, 1, 5);
      this.tableLayoutPanel3.Controls.Add(this.cbUseDbsRbl, 0, 6);
      this.tableLayoutPanel3.Controls.Add(this.label4, 0, 7);
      this.tableLayoutPanel3.Controls.Add(this.tbDbsRbl, 1, 7);
      this.tableLayoutPanel3.Controls.Add(this.linkBadIpFile, 1, 2);
      this.tableLayoutPanel3.Controls.Add(this.groupBox1, 0, 8);
      this.tableLayoutPanel3.Controls.Add(this.linkGoodIpFile, 1, 3);
      this.tableLayoutPanel3.Location = new System.Drawing.Point(4, 4);
      this.tableLayoutPanel3.Name = "tableLayoutPanel3";
      this.tableLayoutPanel3.RowCount = 10;
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 180F));
      this.tableLayoutPanel3.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 40F));
      this.tableLayoutPanel3.Size = new System.Drawing.Size(648, 427);
      this.tableLayoutPanel3.TabIndex = 0;
      // 
      // cbPressEsc
      // 
      this.cbPressEsc.AutoSize = true;
      this.tableLayoutPanel3.SetColumnSpan(this.cbPressEsc, 2);
      this.cbPressEsc.Location = new System.Drawing.Point(3, 3);
      this.cbPressEsc.Name = "cbPressEsc";
      this.cbPressEsc.Size = new System.Drawing.Size(200, 17);
      this.cbPressEsc.TabIndex = 6;
      this.cbPressEsc.Text = "Use \"Press <ESC> Twice for BBS...\"";
      this.cbPressEsc.UseVisualStyleBackColor = true;
      // 
      // lblConcurrent
      // 
      this.lblConcurrent.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.lblConcurrent.AutoSize = true;
      this.lblConcurrent.Location = new System.Drawing.Point(3, 23);
      this.lblConcurrent.Name = "lblConcurrent";
      this.lblConcurrent.Size = new System.Drawing.Size(127, 13);
      this.lblConcurrent.TabIndex = 2;
      this.lblConcurrent.Text = "Max concurrent sessions:";
      // 
      // maxConcurrent
      // 
      this.maxConcurrent.Location = new System.Drawing.Point(136, 26);
      this.maxConcurrent.Name = "maxConcurrent";
      this.maxConcurrent.Size = new System.Drawing.Size(120, 20);
      this.maxConcurrent.TabIndex = 5;
      // 
      // useBadIp
      // 
      this.useBadIp.AutoSize = true;
      this.useBadIp.Location = new System.Drawing.Point(3, 52);
      this.useBadIp.Name = "useBadIp";
      this.useBadIp.Size = new System.Drawing.Size(104, 17);
      this.useBadIp.TabIndex = 0;
      this.useBadIp.Text = "Use BADIP.TXT";
      this.useBadIp.UseVisualStyleBackColor = true;
      // 
      // useGoodIp
      // 
      this.useGoodIp.AutoSize = true;
      this.useGoodIp.Location = new System.Drawing.Point(3, 75);
      this.useGoodIp.Name = "useGoodIp";
      this.useGoodIp.Size = new System.Drawing.Size(114, 17);
      this.useGoodIp.TabIndex = 1;
      this.useGoodIp.Text = "Use GOODIP.TXT";
      this.useGoodIp.UseVisualStyleBackColor = true;
      // 
      // autoBan
      // 
      this.autoBan.AutoSize = true;
      this.tableLayoutPanel3.SetColumnSpan(this.autoBan, 2);
      this.autoBan.Location = new System.Drawing.Point(3, 98);
      this.autoBan.Name = "autoBan";
      this.autoBan.Size = new System.Drawing.Size(141, 17);
      this.autoBan.TabIndex = 1;
      this.autoBan.Text = "Auto Add to BADIP.TXT";
      this.autoBan.UseVisualStyleBackColor = true;
      // 
      // lblAutoBlacklist
      // 
      this.lblAutoBlacklist.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.lblAutoBlacklist.AutoSize = true;
      this.lblAutoBlacklist.Location = new System.Drawing.Point(31, 118);
      this.lblAutoBlacklist.Name = "lblAutoBlacklist";
      this.lblAutoBlacklist.Size = new System.Drawing.Size(99, 13);
      this.lblAutoBlacklist.TabIndex = 3;
      this.lblAutoBlacklist.Text = "Auto Blacklist After:";
      // 
      // flowLayoutPanel1
      // 
      this.flowLayoutPanel1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.flowLayoutPanel1.Controls.Add(this.banCount);
      this.flowLayoutPanel1.Controls.Add(this.label2);
      this.flowLayoutPanel1.Controls.Add(this.banSeconds);
      this.flowLayoutPanel1.Controls.Add(this.label3);
      this.flowLayoutPanel1.Location = new System.Drawing.Point(133, 118);
      this.flowLayoutPanel1.Margin = new System.Windows.Forms.Padding(0);
      this.flowLayoutPanel1.Name = "flowLayoutPanel1";
      this.flowLayoutPanel1.Size = new System.Drawing.Size(515, 40);
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
      // cbUseDbsRbl
      // 
      this.cbUseDbsRbl.AutoSize = true;
      this.tableLayoutPanel3.SetColumnSpan(this.cbUseDbsRbl, 2);
      this.cbUseDbsRbl.Location = new System.Drawing.Point(3, 161);
      this.cbUseDbsRbl.Name = "cbUseDbsRbl";
      this.cbUseDbsRbl.Size = new System.Drawing.Size(171, 17);
      this.cbUseDbsRbl.TabIndex = 7;
      this.cbUseDbsRbl.Text = "Use DNS-RBL Blacklist Server";
      this.cbUseDbsRbl.UseVisualStyleBackColor = true;
      // 
      // label4
      // 
      this.label4.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.label4.AutoSize = true;
      this.label4.Location = new System.Drawing.Point(39, 181);
      this.label4.Name = "label4";
      this.label4.Size = new System.Drawing.Size(91, 13);
      this.label4.TabIndex = 8;
      this.label4.Text = "DNS-RBL Server:";
      // 
      // tbDbsRbl
      // 
      this.tbDbsRbl.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.tbDbsRbl.Location = new System.Drawing.Point(136, 184);
      this.tbDbsRbl.Name = "tbDbsRbl";
      this.tbDbsRbl.Size = new System.Drawing.Size(509, 20);
      this.tbDbsRbl.TabIndex = 9;
      // 
      // linkBadIpFile
      // 
      this.linkBadIpFile.AutoSize = true;
      this.linkBadIpFile.Location = new System.Drawing.Point(136, 49);
      this.linkBadIpFile.Name = "linkBadIpFile";
      this.linkBadIpFile.Size = new System.Drawing.Size(52, 13);
      this.linkBadIpFile.TabIndex = 12;
      this.linkBadIpFile.TabStop = true;
      this.linkBadIpFile.Text = "Open File";
      this.linkBadIpFile.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.linkBadIpFile_LinkClicked);
      // 
      // groupBox1
      // 
      this.tableLayoutPanel3.SetColumnSpan(this.groupBox1, 2);
      this.groupBox1.Controls.Add(this.layoutBlockCountries);
      this.groupBox1.Dock = System.Windows.Forms.DockStyle.Fill;
      this.groupBox1.Location = new System.Drawing.Point(3, 210);
      this.groupBox1.Name = "groupBox1";
      this.groupBox1.Size = new System.Drawing.Size(642, 174);
      this.groupBox1.TabIndex = 13;
      this.groupBox1.TabStop = false;
      this.groupBox1.Text = "Block Countries";
      // 
      // layoutBlockCountries
      // 
      this.layoutBlockCountries.ColumnCount = 3;
      this.layoutBlockCountries.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 100F));
      this.layoutBlockCountries.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.layoutBlockCountries.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 100F));
      this.layoutBlockCountries.Controls.Add(this.label5, 0, 0);
      this.layoutBlockCountries.Controls.Add(this.tbDnsCC, 1, 0);
      this.layoutBlockCountries.Controls.Add(this.flowLayoutPanel2, 2, 1);
      this.layoutBlockCountries.Controls.Add(this.lbBadCountries, 0, 1);
      this.layoutBlockCountries.Dock = System.Windows.Forms.DockStyle.Fill;
      this.layoutBlockCountries.Location = new System.Drawing.Point(3, 16);
      this.layoutBlockCountries.Name = "layoutBlockCountries";
      this.layoutBlockCountries.RowCount = 3;
      this.layoutBlockCountries.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
      this.layoutBlockCountries.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.layoutBlockCountries.RowStyles.Add(new System.Windows.Forms.RowStyle());
      this.layoutBlockCountries.Size = new System.Drawing.Size(636, 155);
      this.layoutBlockCountries.TabIndex = 0;
      // 
      // label5
      // 
      this.label5.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.label5.AutoSize = true;
      this.label5.Location = new System.Drawing.Point(13, 0);
      this.label5.Name = "label5";
      this.label5.Size = new System.Drawing.Size(84, 13);
      this.label5.TabIndex = 14;
      this.label5.Text = "DNS-CC Server:";
      // 
      // tbDnsCC
      // 
      this.tbDnsCC.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.layoutBlockCountries.SetColumnSpan(this.tbDnsCC, 2);
      this.tbDnsCC.Location = new System.Drawing.Point(103, 3);
      this.tbDnsCC.Name = "tbDnsCC";
      this.tbDnsCC.Size = new System.Drawing.Size(530, 20);
      this.tbDnsCC.TabIndex = 15;
      // 
      // flowLayoutPanel2
      // 
      this.flowLayoutPanel2.Controls.Add(this.btnBadCountryAdd);
      this.flowLayoutPanel2.Controls.Add(this.btnBadCountryRemove);
      this.flowLayoutPanel2.Dock = System.Windows.Forms.DockStyle.Fill;
      this.flowLayoutPanel2.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
      this.flowLayoutPanel2.Location = new System.Drawing.Point(539, 29);
      this.flowLayoutPanel2.Name = "flowLayoutPanel2";
      this.flowLayoutPanel2.Size = new System.Drawing.Size(94, 123);
      this.flowLayoutPanel2.TabIndex = 0;
      // 
      // btnBadCountryAdd
      // 
      this.btnBadCountryAdd.Location = new System.Drawing.Point(3, 3);
      this.btnBadCountryAdd.Name = "btnBadCountryAdd";
      this.btnBadCountryAdd.Size = new System.Drawing.Size(75, 23);
      this.btnBadCountryAdd.TabIndex = 0;
      this.btnBadCountryAdd.Text = "Add";
      this.btnBadCountryAdd.UseVisualStyleBackColor = true;
      this.btnBadCountryAdd.Click += new System.EventHandler(this.btnBadCountryAdd_Click);
      // 
      // btnBadCountryRemove
      // 
      this.btnBadCountryRemove.Location = new System.Drawing.Point(3, 32);
      this.btnBadCountryRemove.Name = "btnBadCountryRemove";
      this.btnBadCountryRemove.Size = new System.Drawing.Size(75, 23);
      this.btnBadCountryRemove.TabIndex = 1;
      this.btnBadCountryRemove.Text = "Remove";
      this.btnBadCountryRemove.UseVisualStyleBackColor = true;
      this.btnBadCountryRemove.Click += new System.EventHandler(this.btnBadCountryRemove_Click);
      // 
      // lbBadCountries
      // 
      this.layoutBlockCountries.SetColumnSpan(this.lbBadCountries, 2);
      this.lbBadCountries.Dock = System.Windows.Forms.DockStyle.Fill;
      this.lbBadCountries.FormattingEnabled = true;
      this.lbBadCountries.Location = new System.Drawing.Point(3, 29);
      this.lbBadCountries.Name = "lbBadCountries";
      this.lbBadCountries.Size = new System.Drawing.Size(530, 123);
      this.lbBadCountries.TabIndex = 1;
      // 
      // linkGoodIpFile
      // 
      this.linkGoodIpFile.AutoSize = true;
      this.linkGoodIpFile.Location = new System.Drawing.Point(136, 72);
      this.linkGoodIpFile.Name = "linkGoodIpFile";
      this.linkGoodIpFile.Size = new System.Drawing.Size(52, 13);
      this.linkGoodIpFile.TabIndex = 14;
      this.linkGoodIpFile.TabStop = true;
      this.linkGoodIpFile.Text = "Open File";
      this.linkGoodIpFile.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.linkGoodIpFile_LinkClicked);
      // 
      // PreferencesDialog
      // 
      this.AcceptButton = this.okButton;
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.CancelButton = this.cancelButton;
      this.ClientSize = new System.Drawing.Size(684, 511);
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
      this.tableLayoutPanel2.ResumeLayout(false);
      this.tableLayoutPanel2.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.sshSpinner)).EndInit();
      this.binkpTab.ResumeLayout(false);
      this.tableLayoutPanel4.ResumeLayout(false);
      this.tableLayoutPanel4.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.binkpPort)).EndInit();
      this.blockTab.ResumeLayout(false);
      this.blockTab.PerformLayout();
      this.tableLayoutPanel3.ResumeLayout(false);
      this.tableLayoutPanel3.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.maxConcurrent)).EndInit();
      this.flowLayoutPanel1.ResumeLayout(false);
      this.flowLayoutPanel1.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.banCount)).EndInit();
      ((System.ComponentModel.ISupportInitialize)(this.banSeconds)).EndInit();
      this.groupBox1.ResumeLayout(false);
      this.layoutBlockCountries.ResumeLayout(false);
      this.layoutBlockCountries.PerformLayout();
      this.flowLayoutPanel2.ResumeLayout(false);
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
        private System.Windows.Forms.CheckBox launchLocalNodeCheckBox;
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
    private System.Windows.Forms.CheckBox cbPressEsc;
    private System.Windows.Forms.CheckBox cbUseDbsRbl;
    private System.Windows.Forms.Label label4;
    private System.Windows.Forms.TextBox tbDbsRbl;
    private System.Windows.Forms.LinkLabel linkBadIpFile;
    private System.Windows.Forms.GroupBox groupBox1;
    private System.Windows.Forms.TableLayoutPanel layoutBlockCountries;
    private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel2;
    private System.Windows.Forms.Button btnBadCountryAdd;
    private System.Windows.Forms.Button btnBadCountryRemove;
    private System.Windows.Forms.ListBox lbBadCountries;
    private System.Windows.Forms.Label label5;
    private System.Windows.Forms.TextBox tbDnsCC;
    private System.Windows.Forms.LinkLabel linkGoodIpFile;
    private System.Windows.Forms.TabPage binkpTab;
    private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
    private System.Windows.Forms.TextBox textNetworkbParameters;
    private System.Windows.Forms.Label lblNetworkbParams;
    private System.Windows.Forms.Label label7;
    private System.Windows.Forms.NumericUpDown binkpPort;
    private System.Windows.Forms.CheckBox cbUseBinkp;
    private System.Windows.Forms.Label label6;
    private System.Windows.Forms.TextBox binkpExecutable;
  }
}
