namespace WWIV5TelnetServer
{
    partial class MainForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Usage", "CA2213:DisposableFieldsShouldBeDisposed", MessageId = "server")]
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
      this.components = new System.ComponentModel.Container();
      System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
      this.menuStrip1 = new System.Windows.Forms.MenuStrip();
      this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.startToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.stopToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.preferencesToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
      this.runLocalNodeToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.toolStripSeparator2 = new System.Windows.Forms.ToolStripSeparator();
      this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.helpToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.aboutToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.wWIVOnlineDocumentsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.submitBugIssueToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.viewWWIVLogsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.splitContainer1 = new System.Windows.Forms.SplitContainer();
      this.listBoxNodes = new System.Windows.Forms.ListBox();
      this.messages = new System.Windows.Forms.TextBox();
      this.notifyIcon1 = new System.Windows.Forms.NotifyIcon(this.components);
      this.toolStrip1 = new System.Windows.Forms.ToolStrip();
      this.startTb = new System.Windows.Forms.ToolStripButton();
      this.stopTb = new System.Windows.Forms.ToolStripButton();
      this.prefsTb = new System.Windows.Forms.ToolStripButton();
      this.toolStripSeparator3 = new System.Windows.Forms.ToolStripSeparator();
      this.runLocalTb = new System.Windows.Forms.ToolStripButton();
      this.toolStripSeparator4 = new System.Windows.Forms.ToolStripSeparator();
      this.aboutTb = new System.Windows.Forms.ToolStripButton();
      this.statusStrip1 = new System.Windows.Forms.StatusStrip();
      this.menuStrip1.SuspendLayout();
      ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
      this.splitContainer1.Panel1.SuspendLayout();
      this.splitContainer1.Panel2.SuspendLayout();
      this.splitContainer1.SuspendLayout();
      this.toolStrip1.SuspendLayout();
      this.SuspendLayout();
      // 
      // menuStrip1
      // 
      this.menuStrip1.Dock = System.Windows.Forms.DockStyle.None;
      this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.helpToolStripMenuItem});
      this.menuStrip1.Location = new System.Drawing.Point(0, 1);
      this.menuStrip1.Name = "menuStrip1";
      this.menuStrip1.Size = new System.Drawing.Size(195, 24);
      this.menuStrip1.TabIndex = 0;
      this.menuStrip1.Text = "menuStrip1";
      // 
      // fileToolStripMenuItem
      // 
      this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.startToolStripMenuItem,
            this.stopToolStripMenuItem,
            this.preferencesToolStripMenuItem,
            this.toolStripSeparator1,
            this.runLocalNodeToolStripMenuItem,
            this.toolStripSeparator2,
            this.exitToolStripMenuItem});
      this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
      this.fileToolStripMenuItem.Size = new System.Drawing.Size(51, 20);
      this.fileToolStripMenuItem.Text = "Server";
      // 
      // startToolStripMenuItem
      // 
      this.startToolStripMenuItem.Name = "startToolStripMenuItem";
      this.startToolStripMenuItem.Size = new System.Drawing.Size(158, 22);
      this.startToolStripMenuItem.Text = "Start";
      this.startToolStripMenuItem.Click += new System.EventHandler(this.startToolStripMenuItem_Click);
      // 
      // stopToolStripMenuItem
      // 
      this.stopToolStripMenuItem.Name = "stopToolStripMenuItem";
      this.stopToolStripMenuItem.Size = new System.Drawing.Size(158, 22);
      this.stopToolStripMenuItem.Text = "Stop";
      this.stopToolStripMenuItem.Click += new System.EventHandler(this.stopToolStripMenuItem_Click);
      // 
      // preferencesToolStripMenuItem
      // 
      this.preferencesToolStripMenuItem.BackColor = System.Drawing.SystemColors.ControlLight;
      this.preferencesToolStripMenuItem.Name = "preferencesToolStripMenuItem";
      this.preferencesToolStripMenuItem.Size = new System.Drawing.Size(158, 22);
      this.preferencesToolStripMenuItem.Text = "Preferences";
      this.preferencesToolStripMenuItem.Click += new System.EventHandler(this.preferencesToolStripMenuItem_Click);
      // 
      // toolStripSeparator1
      // 
      this.toolStripSeparator1.Name = "toolStripSeparator1";
      this.toolStripSeparator1.Size = new System.Drawing.Size(155, 6);
      // 
      // runLocalNodeToolStripMenuItem
      // 
      this.runLocalNodeToolStripMenuItem.Name = "runLocalNodeToolStripMenuItem";
      this.runLocalNodeToolStripMenuItem.Size = new System.Drawing.Size(158, 22);
      this.runLocalNodeToolStripMenuItem.Text = "Run Local Node";
      this.runLocalNodeToolStripMenuItem.Click += new System.EventHandler(this.runLocalNodeToolStripMenuItem_Click);
      // 
      // toolStripSeparator2
      // 
      this.toolStripSeparator2.Name = "toolStripSeparator2";
      this.toolStripSeparator2.Size = new System.Drawing.Size(155, 6);
      // 
      // exitToolStripMenuItem
      // 
      this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
      this.exitToolStripMenuItem.Size = new System.Drawing.Size(158, 22);
      this.exitToolStripMenuItem.Text = "Exit";
      this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
      // 
      // helpToolStripMenuItem
      // 
      this.helpToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.aboutToolStripMenuItem,
            this.wWIVOnlineDocumentsToolStripMenuItem,
            this.submitBugIssueToolStripMenuItem,
            this.viewWWIVLogsToolStripMenuItem});
      this.helpToolStripMenuItem.Name = "helpToolStripMenuItem";
      this.helpToolStripMenuItem.Size = new System.Drawing.Size(44, 20);
      this.helpToolStripMenuItem.Text = "Help";
      // 
      // aboutToolStripMenuItem
      // 
      this.aboutToolStripMenuItem.Name = "aboutToolStripMenuItem";
      this.aboutToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
      this.aboutToolStripMenuItem.Text = "About";
      this.aboutToolStripMenuItem.Click += new System.EventHandler(this.aboutToolStripMenuItem_Click);
      // 
      // wWIVOnlineDocumentsToolStripMenuItem
      // 
      this.wWIVOnlineDocumentsToolStripMenuItem.Name = "wWIVOnlineDocumentsToolStripMenuItem";
      this.wWIVOnlineDocumentsToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
      this.wWIVOnlineDocumentsToolStripMenuItem.Text = "WWIV Online Documents";
      this.wWIVOnlineDocumentsToolStripMenuItem.Click += new System.EventHandler(this.wWIVOnlineDocumentsToolStripMenuItem_Click);
      // 
      // submitBugIssueToolStripMenuItem
      // 
      this.submitBugIssueToolStripMenuItem.Name = "submitBugIssueToolStripMenuItem";
      this.submitBugIssueToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
      this.submitBugIssueToolStripMenuItem.Text = "Submit Bug Issue";
      this.submitBugIssueToolStripMenuItem.Click += new System.EventHandler(this.submitBugIssueToolStripMenuItem_Click);
      // 
      // viewWWIVLogsToolStripMenuItem
      // 
      this.viewWWIVLogsToolStripMenuItem.Name = "viewWWIVLogsToolStripMenuItem";
      this.viewWWIVLogsToolStripMenuItem.Size = new System.Drawing.Size(208, 22);
      this.viewWWIVLogsToolStripMenuItem.Text = "View WWIV Logs";
      this.viewWWIVLogsToolStripMenuItem.Click += new System.EventHandler(this.viewWWIVLogsToolStripMenuItem_Click);
      // 
      // splitContainer1
      // 
      this.splitContainer1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
      this.splitContainer1.Location = new System.Drawing.Point(0, 29);
      this.splitContainer1.Name = "splitContainer1";
      // 
      // splitContainer1.Panel1
      // 
      this.splitContainer1.Panel1.Controls.Add(this.listBoxNodes);
      // 
      // splitContainer1.Panel2
      // 
      this.splitContainer1.Panel2.Controls.Add(this.messages);
      this.splitContainer1.Size = new System.Drawing.Size(633, 208);
      this.splitContainer1.SplitterDistance = 240;
      this.splitContainer1.TabIndex = 1;
      // 
      // listBoxNodes
      // 
      this.listBoxNodes.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.listBoxNodes.FormattingEnabled = true;
      this.listBoxNodes.Location = new System.Drawing.Point(3, 0);
      this.listBoxNodes.Name = "listBoxNodes";
      this.listBoxNodes.Size = new System.Drawing.Size(234, 212);
      this.listBoxNodes.TabIndex = 0;
      // 
      // messages
      // 
      this.messages.AcceptsTab = true;
      this.messages.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.messages.Location = new System.Drawing.Point(-1, 3);
      this.messages.MaxLength = 16777216;
      this.messages.Multiline = true;
      this.messages.Name = "messages";
      this.messages.ReadOnly = true;
      this.messages.ScrollBars = System.Windows.Forms.ScrollBars.Both;
      this.messages.Size = new System.Drawing.Size(396, 205);
      this.messages.TabIndex = 0;
      // 
      // notifyIcon1
      // 
      this.notifyIcon1.Icon = ((System.Drawing.Icon)(resources.GetObject("notifyIcon1.Icon")));
      this.notifyIcon1.Text = "notifyIcon1";
      this.notifyIcon1.Visible = true;
      this.notifyIcon1.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.notifyIcon1_MouseDoubleClick);
      // 
      // toolStrip1
      // 
      this.toolStrip1.Dock = System.Windows.Forms.DockStyle.None;
      this.toolStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.startTb,
            this.stopTb,
            this.prefsTb,
            this.toolStripSeparator3,
            this.runLocalTb,
            this.toolStripSeparator4,
            this.aboutTb});
      this.toolStrip1.Location = new System.Drawing.Point(217, 1);
      this.toolStrip1.Name = "toolStrip1";
      this.toolStrip1.Size = new System.Drawing.Size(139, 25);
      this.toolStrip1.Stretch = true;
      this.toolStrip1.TabIndex = 0;
      // 
      // startTb
      // 
      this.startTb.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
      this.startTb.ImageTransparentColor = System.Drawing.Color.Magenta;
      this.startTb.Name = "startTb";
      this.startTb.Size = new System.Drawing.Size(23, 22);
      this.startTb.Text = "Start";
      this.startTb.Click += new System.EventHandler(this.toolStripButton1_Click);
      // 
      // stopTb
      // 
      this.stopTb.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
      this.stopTb.ImageTransparentColor = System.Drawing.Color.Magenta;
      this.stopTb.Name = "stopTb";
      this.stopTb.Size = new System.Drawing.Size(23, 22);
      this.stopTb.Text = "Stop";
      this.stopTb.Click += new System.EventHandler(this.toolStripButton2_Click);
      // 
      // prefsTb
      // 
      this.prefsTb.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
      this.prefsTb.ImageTransparentColor = System.Drawing.Color.Magenta;
      this.prefsTb.Name = "prefsTb";
      this.prefsTb.Size = new System.Drawing.Size(23, 22);
      this.prefsTb.Text = "Preferences";
      this.prefsTb.Click += new System.EventHandler(this.toolStripButton3_Click);
      // 
      // toolStripSeparator3
      // 
      this.toolStripSeparator3.Name = "toolStripSeparator3";
      this.toolStripSeparator3.Size = new System.Drawing.Size(6, 25);
      // 
      // runLocalTb
      // 
      this.runLocalTb.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
      this.runLocalTb.ImageTransparentColor = System.Drawing.Color.Magenta;
      this.runLocalTb.Name = "runLocalTb";
      this.runLocalTb.Size = new System.Drawing.Size(23, 22);
      this.runLocalTb.Text = "Run Local Node";
      this.runLocalTb.Click += new System.EventHandler(this.runLocalTb_Click);
      // 
      // toolStripSeparator4
      // 
      this.toolStripSeparator4.Name = "toolStripSeparator4";
      this.toolStripSeparator4.Size = new System.Drawing.Size(6, 25);
      // 
      // aboutTb
      // 
      this.aboutTb.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
      this.aboutTb.ImageTransparentColor = System.Drawing.Color.Magenta;
      this.aboutTb.Name = "aboutTb";
      this.aboutTb.Size = new System.Drawing.Size(23, 22);
      this.aboutTb.Text = "About";
      this.aboutTb.Click += new System.EventHandler(this.aboutTb_Click);
      // 
      // statusStrip1
      // 
      this.statusStrip1.Location = new System.Drawing.Point(0, 240);
      this.statusStrip1.Name = "statusStrip1";
      this.statusStrip1.Size = new System.Drawing.Size(635, 22);
      this.statusStrip1.TabIndex = 2;
      this.statusStrip1.Text = "statusStrip1";
      // 
      // MainForm
      // 
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.ClientSize = new System.Drawing.Size(635, 262);
      this.Controls.Add(this.statusStrip1);
      this.Controls.Add(this.toolStrip1);
      this.Controls.Add(this.menuStrip1);
      this.Controls.Add(this.splitContainer1);
      this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
      this.MainMenuStrip = this.menuStrip1;
      this.Name = "MainForm";
      this.Text = "WWIV Server";
      this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MainForm_FormClosing);
      this.Load += new System.EventHandler(this.MainForm_Load);
      this.Resize += new System.EventHandler(this.MainForm_Resize);
      this.menuStrip1.ResumeLayout(false);
      this.menuStrip1.PerformLayout();
      this.splitContainer1.Panel1.ResumeLayout(false);
      this.splitContainer1.Panel2.ResumeLayout(false);
      this.splitContainer1.Panel2.PerformLayout();
      ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
      this.splitContainer1.ResumeLayout(false);
      this.toolStrip1.ResumeLayout(false);
      this.toolStrip1.PerformLayout();
      this.ResumeLayout(false);
      this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem startToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem stopToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem preferencesToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem helpToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem aboutToolStripMenuItem;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.ListBox listBoxNodes;
        private System.Windows.Forms.TextBox messages;
        private System.Windows.Forms.ToolStripMenuItem runLocalNodeToolStripMenuItem;
        private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
        private System.Windows.Forms.NotifyIcon notifyIcon1;
        private System.Windows.Forms.ToolStripMenuItem submitBugIssueToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem wWIVOnlineDocumentsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem viewWWIVLogsToolStripMenuItem;
    private System.Windows.Forms.ToolStrip toolStrip1;
    private System.Windows.Forms.ToolStripButton startTb;
    private System.Windows.Forms.ToolStripButton stopTb;
    private System.Windows.Forms.ToolStripButton prefsTb;
    private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
    private System.Windows.Forms.ToolStripButton runLocalTb;
    private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
    private System.Windows.Forms.ToolStripButton aboutTb;
    private System.Windows.Forms.StatusStrip statusStrip1;
  }
}

