namespace windows_wwiv_update
{
    partial class Form1
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.version51 = new System.Windows.Forms.Label();
            this.version50 = new System.Windows.Forms.Label();
            this.update51 = new System.Windows.Forms.Button();
            this.update50 = new System.Windows.Forms.Button();
            this.versionNumber = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.customBuild = new System.Windows.Forms.TextBox();
            this.customUpdate = new System.Windows.Forms.Button();
            this.label5 = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            this.currentVersionInfo = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 14.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.ForeColor = System.Drawing.Color.CornflowerBlue;
            this.label1.Location = new System.Drawing.Point(3, 73);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(279, 24);
            this.label1.TabIndex = 0;
            this.label1.Text = "Welcome to WWIV 5 Update!";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(9, 145);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(118, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Latest WWIV 5.1 Build:";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(9, 176);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(118, 13);
            this.label3.TabIndex = 2;
            this.label3.Text = "Latest WWIV 5.0 Build:";
            // 
            // version51
            // 
            this.version51.AutoSize = true;
            this.version51.ForeColor = System.Drawing.SystemColors.HotTrack;
            this.version51.Location = new System.Drawing.Point(133, 145);
            this.version51.Name = "version51";
            this.version51.Size = new System.Drawing.Size(46, 13);
            this.version51.TabIndex = 3;
            this.version51.Text = "ERROR";
            // 
            // version50
            // 
            this.version50.AutoSize = true;
            this.version50.ForeColor = System.Drawing.SystemColors.HotTrack;
            this.version50.Location = new System.Drawing.Point(133, 176);
            this.version50.Name = "version50";
            this.version50.Size = new System.Drawing.Size(46, 13);
            this.version50.TabIndex = 4;
            this.version50.Text = "ERROR";
            // 
            // update51
            // 
            this.update51.Location = new System.Drawing.Point(188, 140);
            this.update51.Name = "update51";
            this.update51.Size = new System.Drawing.Size(75, 23);
            this.update51.TabIndex = 5;
            this.update51.Text = "Update 5.1";
            this.update51.UseVisualStyleBackColor = true;
            this.update51.Click += new System.EventHandler(this.update51_Click);
            // 
            // update50
            // 
            this.update50.Location = new System.Drawing.Point(188, 171);
            this.update50.Name = "update50";
            this.update50.Size = new System.Drawing.Size(75, 23);
            this.update50.TabIndex = 6;
            this.update50.Text = "Update 5.0";
            this.update50.UseVisualStyleBackColor = true;
            this.update50.Click += new System.EventHandler(this.update50_Click);
            // 
            // versionNumber
            // 
            this.versionNumber.AutoSize = true;
            this.versionNumber.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.versionNumber.ForeColor = System.Drawing.SystemColors.MenuHighlight;
            this.versionNumber.Location = new System.Drawing.Point(139, 244);
            this.versionNumber.Name = "versionNumber";
            this.versionNumber.Size = new System.Drawing.Size(110, 13);
            this.versionNumber.TabIndex = 7;
            this.versionNumber.Text = "WWIV Update Version";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(12, 206);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(102, 13);
            this.label4.TabIndex = 8;
            this.label4.Text = "Other Build Number:";
            // 
            // customBuild
            // 
            this.customBuild.Location = new System.Drawing.Point(120, 203);
            this.customBuild.MaxLength = 4;
            this.customBuild.Name = "customBuild";
            this.customBuild.Size = new System.Drawing.Size(50, 20);
            this.customBuild.TabIndex = 9;
            // 
            // customUpdate
            // 
            this.customUpdate.Location = new System.Drawing.Point(188, 201);
            this.customUpdate.Name = "customUpdate";
            this.customUpdate.Size = new System.Drawing.Size(75, 23);
            this.customUpdate.TabIndex = 10;
            this.customUpdate.Text = "Update";
            this.customUpdate.UseVisualStyleBackColor = true;
            this.customUpdate.Click += new System.EventHandler(this.customUpdate_Click);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(9, 104);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(262, 26);
            this.label5.TabIndex = 11;
            this.label5.Text = "Below you will find the latest updates for WWIV 5.x  or\r\nyou can enter a desired " +
    "build to install.";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.ForeColor = System.Drawing.Color.Gray;
            this.label6.Location = new System.Drawing.Point(2, 227);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(280, 13);
            this.label6.TabIndex = 12;
            this.label6.Text = "* This is can be used to go backward in case of problems.";
            // 
            // pictureBox1
            // 
            this.pictureBox1.Image = global::windows_wwiv_update.Properties.Resources.wwiv;
            this.pictureBox1.InitialImage = null;
            this.pictureBox1.Location = new System.Drawing.Point(51, 5);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(180, 65);
            this.pictureBox1.TabIndex = 13;
            this.pictureBox1.TabStop = false;
            // 
            // currentVersionInfo
            // 
            this.currentVersionInfo.AutoSize = true;
            this.currentVersionInfo.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.currentVersionInfo.ForeColor = System.Drawing.SystemColors.MenuHighlight;
            this.currentVersionInfo.Location = new System.Drawing.Point(3, 244);
            this.currentVersionInfo.Name = "currentVersionInfo";
            this.currentVersionInfo.Size = new System.Drawing.Size(0, 13);
            this.currentVersionInfo.TabIndex = 14;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(284, 261);
            this.Controls.Add(this.currentVersionInfo);
            this.Controls.Add(this.pictureBox1);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.customUpdate);
            this.Controls.Add(this.customBuild);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.versionNumber);
            this.Controls.Add(this.update50);
            this.Controls.Add(this.update51);
            this.Controls.Add(this.version50);
            this.Controls.Add(this.version51);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Form1";
            this.Text = "WWIV 5 Update";
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label version51;
        private System.Windows.Forms.Label version50;
        private System.Windows.Forms.Button update51;
        private System.Windows.Forms.Button update50;
        private System.Windows.Forms.Label versionNumber;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.TextBox customBuild;
        private System.Windows.Forms.Button customUpdate;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.PictureBox pictureBox1;
        private System.Windows.Forms.Label currentVersionInfo;
    }
}

