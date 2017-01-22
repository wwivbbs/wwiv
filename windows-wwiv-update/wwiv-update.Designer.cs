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
            this.lbl52 = new System.Windows.Forms.Label();
            this.lbl51 = new System.Windows.Forms.Label();
            this.versionUnstable = new System.Windows.Forms.Label();
            this.versionStable = new System.Windows.Forms.Label();
            this.update52 = new System.Windows.Forms.Button();
            this.update51 = new System.Windows.Forms.Button();
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
            this.label1.Location = new System.Drawing.Point(4, 90);
            this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(325, 29);
            this.label1.TabIndex = 0;
            this.label1.Text = "Welcome to WWIV Update!";
            // 
            // lbl52
            // 
            this.lbl52.AutoSize = true;
            this.lbl52.Location = new System.Drawing.Point(12, 178);
            this.lbl52.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lbl52.Name = "lbl52";
            this.lbl52.Size = new System.Drawing.Size(152, 17);
            this.lbl52.TabIndex = 1;
            this.lbl52.Text = "Latest WWIV 5.3 Build:";
            // 
            // lbl51
            // 
            this.lbl51.AutoSize = true;
            this.lbl51.Location = new System.Drawing.Point(12, 217);
            this.lbl51.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.lbl51.Name = "lbl51";
            this.lbl51.Size = new System.Drawing.Size(152, 17);
            this.lbl51.TabIndex = 2;
            this.lbl51.Text = "Latest WWIV 5.2 Build:";
            // 
            // versionUnstable
            // 
            this.versionUnstable.AutoSize = true;
            this.versionUnstable.ForeColor = System.Drawing.SystemColors.HotTrack;
            this.versionUnstable.Location = new System.Drawing.Point(177, 178);
            this.versionUnstable.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.versionUnstable.Name = "versionUnstable";
            this.versionUnstable.Size = new System.Drawing.Size(58, 17);
            this.versionUnstable.TabIndex = 3;
            this.versionUnstable.Text = "ERROR";
            // 
            // versionStable
            // 
            this.versionStable.AutoSize = true;
            this.versionStable.ForeColor = System.Drawing.SystemColors.HotTrack;
            this.versionStable.Location = new System.Drawing.Point(177, 217);
            this.versionStable.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.versionStable.Name = "versionStable";
            this.versionStable.Size = new System.Drawing.Size(58, 17);
            this.versionStable.TabIndex = 4;
            this.versionStable.Text = "ERROR";
            // 
            // update52
            // 
            this.update52.Location = new System.Drawing.Point(251, 172);
            this.update52.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.update52.Name = "update52";
            this.update52.Size = new System.Drawing.Size(100, 28);
            this.update52.TabIndex = 5;
            this.update52.Text = "Update 5.3";
            this.update52.UseVisualStyleBackColor = true;
            this.update52.Click += new System.EventHandler(this.updateUnstable_Click);
            // 
            // update51
            // 
            this.update51.Location = new System.Drawing.Point(251, 210);
            this.update51.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.update51.Name = "update51";
            this.update51.Size = new System.Drawing.Size(100, 28);
            this.update51.TabIndex = 6;
            this.update51.Text = "Update 5.2";
            this.update51.UseVisualStyleBackColor = true;
            this.update51.Click += new System.EventHandler(this.updateStable_Click);
            // 
            // versionNumber
            // 
            this.versionNumber.AutoSize = true;
            this.versionNumber.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.versionNumber.ForeColor = System.Drawing.SystemColors.MenuHighlight;
            this.versionNumber.Location = new System.Drawing.Point(221, 300);
            this.versionNumber.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.versionNumber.Name = "versionNumber";
            this.versionNumber.Size = new System.Drawing.Size(133, 17);
            this.versionNumber.TabIndex = 7;
            this.versionNumber.Text = "WWIV Update Version";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(16, 254);
            this.label4.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(15, 17);
            this.label4.TabIndex = 8;
            this.label4.Text = "s";
            // 
            // customBuild
            // 
            this.customBuild.Location = new System.Drawing.Point(160, 250);
            this.customBuild.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.customBuild.MaxLength = 4;
            this.customBuild.Name = "customBuild";
            this.customBuild.Size = new System.Drawing.Size(77, 22);
            this.customBuild.TabIndex = 9;
            // 
            // customUpdate
            // 
            this.customUpdate.Location = new System.Drawing.Point(251, 247);
            this.customUpdate.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.customUpdate.Name = "customUpdate";
            this.customUpdate.Size = new System.Drawing.Size(100, 28);
            this.customUpdate.TabIndex = 10;
            this.customUpdate.Text = "Update";
            this.customUpdate.UseVisualStyleBackColor = true;
            this.customUpdate.Click += new System.EventHandler(this.customUpdate_Click);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(12, 128);
            this.label5.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(344, 34);
            this.label5.TabIndex = 11;
            this.label5.Text = "Below you will find the latest updates for WWIV 5.x  or\r\nyou can enter a desired " +
    "build to install.";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.ForeColor = System.Drawing.Color.Gray;
            this.label6.Location = new System.Drawing.Point(3, 279);
            this.label6.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(371, 17);
            this.label6.TabIndex = 12;
            this.label6.Text = "* This is can be used to go backward in case of problems.";
            // 
            // pictureBox1
            // 
            this.pictureBox1.Image = global::windows_wwiv_update.Properties.Resources.wwiv;
            this.pictureBox1.InitialImage = null;
            this.pictureBox1.Location = new System.Drawing.Point(68, 6);
            this.pictureBox1.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(240, 80);
            this.pictureBox1.TabIndex = 13;
            this.pictureBox1.TabStop = false;
            // 
            // currentVersionInfo
            // 
            this.currentVersionInfo.AutoSize = true;
            this.currentVersionInfo.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.currentVersionInfo.ForeColor = System.Drawing.SystemColors.MenuHighlight;
            this.currentVersionInfo.Location = new System.Drawing.Point(4, 300);
            this.currentVersionInfo.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.currentVersionInfo.Name = "currentVersionInfo";
            this.currentVersionInfo.Size = new System.Drawing.Size(0, 17);
            this.currentVersionInfo.TabIndex = 14;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(379, 321);
            this.Controls.Add(this.currentVersionInfo);
            this.Controls.Add(this.pictureBox1);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.customUpdate);
            this.Controls.Add(this.customBuild);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.versionNumber);
            this.Controls.Add(this.update51);
            this.Controls.Add(this.update52);
            this.Controls.Add(this.versionStable);
            this.Controls.Add(this.versionUnstable);
            this.Controls.Add(this.lbl51);
            this.Controls.Add(this.lbl52);
            this.Controls.Add(this.label1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.Name = "Form1";
            this.Text = "WWIV 5 Update";
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label lbl52;
        private System.Windows.Forms.Label lbl51;
        private System.Windows.Forms.Label versionUnstable;
        private System.Windows.Forms.Label versionStable;
        private System.Windows.Forms.Button update52;
        private System.Windows.Forms.Button update51;
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

