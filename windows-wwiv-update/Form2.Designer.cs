namespace windows_wwiv_update
{
    partial class Form2
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
      System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form2));
      this.buttonRestart = new System.Windows.Forms.Button();
      this.label3 = new System.Windows.Forms.Label();
      this.label4 = new System.Windows.Forms.Label();
      this.label5 = new System.Windows.Forms.Label();
      this.wwivStatus = new System.Windows.Forms.Label();
      this.telnetStatus = new System.Windows.Forms.Label();
      this.netStatus = new System.Windows.Forms.Label();
      this.infoStatus = new System.Windows.Forms.Label();
      this.buttonUpdate = new System.Windows.Forms.Button();
      this.buttonLaunch = new System.Windows.Forms.Button();
      this.versionNumber = new System.Windows.Forms.Label();
      this.buttonExit = new System.Windows.Forms.Button();
      this.progressBar1 = new System.Windows.Forms.ProgressBar();
      this.activeStatus = new System.Windows.Forms.Label();
      this.labelVersion = new System.Windows.Forms.Label();
      this.label1 = new System.Windows.Forms.Label();
      this.logo = new System.Windows.Forms.PictureBox();
      ((System.ComponentModel.ISupportInitialize)(this.logo)).BeginInit();
      this.SuspendLayout();
      // 
      // buttonRestart
      // 
      this.buttonRestart.Location = new System.Drawing.Point(32, 140);
      this.buttonRestart.Name = "buttonRestart";
      this.buttonRestart.Size = new System.Drawing.Size(94, 23);
      this.buttonRestart.TabIndex = 15;
      this.buttonRestart.Text = "Restart";
      this.buttonRestart.UseVisualStyleBackColor = true;
      this.buttonRestart.Click += new System.EventHandler(this.buttonRestart_Click);
      // 
      // label3
      // 
      this.label3.AutoSize = true;
      this.label3.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.label3.Location = new System.Drawing.Point(178, 80);
      this.label3.Name = "label3";
      this.label3.Size = new System.Drawing.Size(56, 13);
      this.label3.TabIndex = 16;
      this.label3.Text = "NETWORK:";
      // 
      // label4
      // 
      this.label4.AutoSize = true;
      this.label4.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.label4.Location = new System.Drawing.Point(12, 80);
      this.label4.Name = "label4";
      this.label4.Size = new System.Drawing.Size(39, 13);
      this.label4.TabIndex = 17;
      this.label4.Text = "WWIV:";
      // 
      // label5
      // 
      this.label5.AutoSize = true;
      this.label5.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.label5.Location = new System.Drawing.Point(94, 80);
      this.label5.Name = "label5";
      this.label5.Size = new System.Drawing.Size(41, 13);
      this.label5.TabIndex = 18;
      this.label5.Text = "TELNET:";
      // 
      // wwivStatus
      // 
      this.wwivStatus.AutoSize = true;
      this.wwivStatus.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.wwivStatus.ForeColor = System.Drawing.Color.Red;
      this.wwivStatus.Location = new System.Drawing.Point(47, 80);
      this.wwivStatus.Name = "wwivStatus";
      this.wwivStatus.Size = new System.Drawing.Size(38, 13);
      this.wwivStatus.TabIndex = 19;
      this.wwivStatus.Text = "STATUS";
      // 
      // telnetStatus
      // 
      this.telnetStatus.AutoSize = true;
      this.telnetStatus.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.telnetStatus.ForeColor = System.Drawing.Color.Red;
      this.telnetStatus.Location = new System.Drawing.Point(131, 80);
      this.telnetStatus.Name = "telnetStatus";
      this.telnetStatus.Size = new System.Drawing.Size(38, 13);
      this.telnetStatus.TabIndex = 20;
      this.telnetStatus.Text = "STATUS";
      // 
      // netStatus
      // 
      this.netStatus.AutoSize = true;
      this.netStatus.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.netStatus.ForeColor = System.Drawing.Color.Red;
      this.netStatus.Location = new System.Drawing.Point(230, 80);
      this.netStatus.Name = "netStatus";
      this.netStatus.Size = new System.Drawing.Size(38, 13);
      this.netStatus.TabIndex = 21;
      this.netStatus.Text = "STATUS";
      // 
      // infoStatus
      // 
      this.infoStatus.AutoSize = true;
      this.infoStatus.Location = new System.Drawing.Point(29, 119);
      this.infoStatus.MinimumSize = new System.Drawing.Size(220, 13);
      this.infoStatus.Name = "infoStatus";
      this.infoStatus.Size = new System.Drawing.Size(220, 13);
      this.infoStatus.TabIndex = 22;
      this.infoStatus.Text = "System Ready For Update?";
      this.infoStatus.TextAlign = System.Drawing.ContentAlignment.TopCenter;
      // 
      // buttonUpdate
      // 
      this.buttonUpdate.Location = new System.Drawing.Point(158, 140);
      this.buttonUpdate.Name = "buttonUpdate";
      this.buttonUpdate.Size = new System.Drawing.Size(94, 23);
      this.buttonUpdate.TabIndex = 23;
      this.buttonUpdate.Text = "Update WWIV";
      this.buttonUpdate.UseVisualStyleBackColor = true;
      this.buttonUpdate.Click += new System.EventHandler(this.buttonUpdate_Click);
      // 
      // buttonLaunch
      // 
      this.buttonLaunch.Location = new System.Drawing.Point(12, 197);
      this.buttonLaunch.Name = "buttonLaunch";
      this.buttonLaunch.Size = new System.Drawing.Size(91, 23);
      this.buttonLaunch.TabIndex = 25;
      this.buttonLaunch.Text = "Launch WWIV";
      this.buttonLaunch.UseVisualStyleBackColor = true;
      this.buttonLaunch.Click += new System.EventHandler(this.buttonLaunch_Click);
      // 
      // versionNumber
      // 
      this.versionNumber.AutoSize = true;
      this.versionNumber.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.versionNumber.ForeColor = System.Drawing.SystemColors.MenuHighlight;
      this.versionNumber.Location = new System.Drawing.Point(139, 244);
      this.versionNumber.Name = "versionNumber";
      this.versionNumber.Size = new System.Drawing.Size(110, 13);
      this.versionNumber.TabIndex = 27;
      this.versionNumber.Text = "WWIV Update Version";
      // 
      // buttonExit
      // 
      this.buttonExit.Location = new System.Drawing.Point(12, 228);
      this.buttonExit.Name = "buttonExit";
      this.buttonExit.Size = new System.Drawing.Size(91, 23);
      this.buttonExit.TabIndex = 29;
      this.buttonExit.Text = "Exit";
      this.buttonExit.UseVisualStyleBackColor = true;
      this.buttonExit.Click += new System.EventHandler(this.buttonExit_Click);
      // 
      // progressBar1
      // 
      this.progressBar1.Location = new System.Drawing.Point(15, 166);
      this.progressBar1.Name = "progressBar1";
      this.progressBar1.Size = new System.Drawing.Size(253, 23);
      this.progressBar1.TabIndex = 31;
      // 
      // activeStatus
      // 
      this.activeStatus.AutoSize = true;
      this.activeStatus.Location = new System.Drawing.Point(33, 192);
      this.activeStatus.MinimumSize = new System.Drawing.Size(220, 13);
      this.activeStatus.Name = "activeStatus";
      this.activeStatus.Size = new System.Drawing.Size(220, 13);
      this.activeStatus.TabIndex = 32;
      this.activeStatus.TextAlign = System.Drawing.ContentAlignment.TopCenter;
      // 
      // labelVersion
      // 
      this.labelVersion.AutoSize = true;
      this.labelVersion.ForeColor = System.Drawing.Color.CornflowerBlue;
      this.labelVersion.Location = new System.Drawing.Point(176, 98);
      this.labelVersion.Name = "labelVersion";
      this.labelVersion.Size = new System.Drawing.Size(66, 13);
      this.labelVersion.TabIndex = 2;
      this.labelVersion.Text = "fetchVersion";
      // 
      // label1
      // 
      this.label1.AutoSize = true;
      this.label1.Location = new System.Drawing.Point(35, 98);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(135, 13);
      this.label1.TabIndex = 1;
      this.label1.Text = "Updating WWIV 5 to Build:";
      // 
      // logo
      // 
      this.logo.Image = global::windows_wwiv_update.Properties.Resources.wwiv;
      this.logo.InitialImage = null;
      this.logo.Location = new System.Drawing.Point(51, 5);
      this.logo.Name = "logo";
      this.logo.Size = new System.Drawing.Size(180, 65);
      this.logo.TabIndex = 14;
      this.logo.TabStop = false;
      // 
      // Form2
      // 
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.ClientSize = new System.Drawing.Size(284, 261);
      this.Controls.Add(this.buttonExit);
      this.Controls.Add(this.versionNumber);
      this.Controls.Add(this.buttonLaunch);
      this.Controls.Add(this.infoStatus);
      this.Controls.Add(this.netStatus);
      this.Controls.Add(this.telnetStatus);
      this.Controls.Add(this.wwivStatus);
      this.Controls.Add(this.label5);
      this.Controls.Add(this.label4);
      this.Controls.Add(this.label3);
      this.Controls.Add(this.logo);
      this.Controls.Add(this.labelVersion);
      this.Controls.Add(this.label1);
      this.Controls.Add(this.progressBar1);
      this.Controls.Add(this.buttonUpdate);
      this.Controls.Add(this.buttonRestart);
      this.Controls.Add(this.activeStatus);
      this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
      this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
      this.Name = "Form2";
      this.Text = "Updating WWIV 5";
      this.Load += new System.EventHandler(this.Form2_Load);
      ((System.ComponentModel.ISupportInitialize)(this.logo)).EndInit();
      this.ResumeLayout(false);
      this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.PictureBox logo;
        private System.Windows.Forms.Button buttonRestart;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label wwivStatus;
        private System.Windows.Forms.Label telnetStatus;
        private System.Windows.Forms.Label netStatus;
        private System.Windows.Forms.Label infoStatus;
        private System.Windows.Forms.Button buttonUpdate;
        private System.Windows.Forms.Button buttonLaunch;
        private System.Windows.Forms.Label versionNumber;
        private System.Windows.Forms.Button buttonExit;
        private System.Windows.Forms.ProgressBar progressBar1;
        private System.Windows.Forms.Label activeStatus;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label labelVersion;
    }
}