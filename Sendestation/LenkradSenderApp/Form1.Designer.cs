namespace LenkradSenderApp;

partial class Form1
{
    private System.ComponentModel.IContainer components = null;
    private Label labelPort;
    private ComboBox comboPorts;
    private Button buttonRefreshPorts;
    private Button buttonStart;
    private Button buttonStop;
    private GroupBox groupStatus;
    private Label labelWheelCaption;
    private Label labelWheelValue;
    private Label labelSenderCaption;
    private Label labelSenderValue;
    private Label labelPacketsCaption;
    private Label labelPacketsValue;
    private Label labelStateCaption;
    private Label labelStateValue;
    private TextBox textStatusLog;
    private GroupBox groupLive;
    private Label labelSteeringLiveCaption;
    private Label labelSteeringLiveValue;
    private Label labelGasLiveCaption;
    private Label labelGasLiveValue;
    private Label labelBrakeLiveCaption;
    private Label labelBrakeLiveValue;
    private GroupBox groupVehicle;
    private Label labelVehiclePlaceholder;

    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }

        base.Dispose(disposing);
    }

    private void InitializeComponent()
    {
        components = new System.ComponentModel.Container();
        labelPort = new Label();
        comboPorts = new ComboBox();
        buttonRefreshPorts = new Button();
        buttonStart = new Button();
        buttonStop = new Button();
        groupStatus = new GroupBox();
        labelWheelCaption = new Label();
        labelWheelValue = new Label();
        labelSenderCaption = new Label();
        labelSenderValue = new Label();
        labelPacketsCaption = new Label();
        labelPacketsValue = new Label();
        labelStateCaption = new Label();
        labelStateValue = new Label();
        textStatusLog = new TextBox();
        groupLive = new GroupBox();
        labelSteeringLiveCaption = new Label();
        labelSteeringLiveValue = new Label();
        labelGasLiveCaption = new Label();
        labelGasLiveValue = new Label();
        labelBrakeLiveCaption = new Label();
        labelBrakeLiveValue = new Label();
        groupVehicle = new GroupBox();
        labelVehiclePlaceholder = new Label();
        groupStatus.SuspendLayout();
        groupLive.SuspendLayout();
        groupVehicle.SuspendLayout();
        SuspendLayout();
        // 
        // labelPort
        // 
        labelPort.AutoSize = true;
        labelPort.Location = new Point(18, 20);
        labelPort.Name = "labelPort";
        labelPort.Size = new Size(61, 15);
        labelPort.TabIndex = 0;
        labelPort.Text = "COM-Port";
        // 
        // comboPorts
        // 
        comboPorts.DropDownStyle = ComboBoxStyle.DropDownList;
        comboPorts.FormattingEnabled = true;
        comboPorts.Location = new Point(18, 38);
        comboPorts.Name = "comboPorts";
        comboPorts.Size = new Size(150, 23);
        comboPorts.TabIndex = 1;
        // 
        // buttonRefreshPorts
        // 
        buttonRefreshPorts.Location = new Point(180, 37);
        buttonRefreshPorts.Name = "buttonRefreshPorts";
        buttonRefreshPorts.Size = new Size(95, 25);
        buttonRefreshPorts.TabIndex = 2;
        buttonRefreshPorts.Text = "Ports neu";
        buttonRefreshPorts.UseVisualStyleBackColor = true;
        buttonRefreshPorts.Click += buttonRefreshPorts_Click;
        // 
        // buttonStart
        // 
        buttonStart.Location = new Point(292, 37);
        buttonStart.Name = "buttonStart";
        buttonStart.Size = new Size(95, 25);
        buttonStart.TabIndex = 3;
        buttonStart.Text = "Start";
        buttonStart.UseVisualStyleBackColor = true;
        buttonStart.Click += buttonStart_Click;
        // 
        // buttonStop
        // 
        buttonStop.Location = new Point(402, 37);
        buttonStop.Name = "buttonStop";
        buttonStop.Size = new Size(95, 25);
        buttonStop.TabIndex = 4;
        buttonStop.Text = "Stop";
        buttonStop.UseVisualStyleBackColor = true;
        buttonStop.Click += buttonStop_Click;
        // 
        // groupStatus
        // 
        groupStatus.Controls.Add(labelWheelCaption);
        groupStatus.Controls.Add(labelWheelValue);
        groupStatus.Controls.Add(labelSenderCaption);
        groupStatus.Controls.Add(labelSenderValue);
        groupStatus.Controls.Add(labelPacketsCaption);
        groupStatus.Controls.Add(labelPacketsValue);
        groupStatus.Controls.Add(labelStateCaption);
        groupStatus.Controls.Add(labelStateValue);
        groupStatus.Location = new Point(18, 79);
        groupStatus.Name = "groupStatus";
        groupStatus.Size = new Size(520, 123);
        groupStatus.TabIndex = 6;
        groupStatus.TabStop = false;
        groupStatus.Text = "Status";
        groupStatus.Anchor = AnchorStyles.Top | AnchorStyles.Left;
        // 
        // labelWheelCaption
        // 
        labelWheelCaption.AutoSize = true;
        labelWheelCaption.Location = new Point(17, 28);
        labelWheelCaption.Name = "labelWheelCaption";
        labelWheelCaption.Size = new Size(52, 15);
        labelWheelCaption.TabIndex = 0;
        labelWheelCaption.Text = "Lenkrad:";
        // 
        // labelWheelValue
        // 
        labelWheelValue.AutoEllipsis = true;
        labelWheelValue.Location = new Point(120, 28);
        labelWheelValue.Name = "labelWheelValue";
        labelWheelValue.Size = new Size(360, 15);
        labelWheelValue.TabIndex = 1;
        labelWheelValue.Text = "-";
        // 
        // labelSenderCaption
        // 
        labelSenderCaption.AutoSize = true;
        labelSenderCaption.Location = new Point(17, 53);
        labelSenderCaption.Name = "labelSenderCaption";
        labelSenderCaption.Size = new Size(47, 15);
        labelSenderCaption.TabIndex = 2;
        labelSenderCaption.Text = "Sender:";
        // 
        // labelSenderValue
        // 
        labelSenderValue.Location = new Point(120, 53);
        labelSenderValue.Name = "labelSenderValue";
        labelSenderValue.Size = new Size(360, 15);
        labelSenderValue.TabIndex = 3;
        labelSenderValue.Text = "-";
        // 
        // labelPacketsCaption
        // 
        labelPacketsCaption.AutoSize = true;
        labelPacketsCaption.Location = new Point(17, 78);
        labelPacketsCaption.Name = "labelPacketsCaption";
        labelPacketsCaption.Size = new Size(73, 15);
        labelPacketsCaption.TabIndex = 4;
        labelPacketsCaption.Text = "Pakete ges.:";
        // 
        // labelPacketsValue
        // 
        labelPacketsValue.Location = new Point(120, 78);
        labelPacketsValue.Name = "labelPacketsValue";
        labelPacketsValue.Size = new Size(360, 15);
        labelPacketsValue.TabIndex = 5;
        labelPacketsValue.Text = "0";
        // 
        // labelStateCaption
        // 
        labelStateCaption.AutoSize = true;
        labelStateCaption.Location = new Point(17, 100);
        labelStateCaption.Name = "labelStateCaption";
        labelStateCaption.Size = new Size(43, 15);
        labelStateCaption.TabIndex = 6;
        labelStateCaption.Text = "Zust.:";
        // 
        // labelStateValue
        // 
        labelStateValue.Location = new Point(120, 100);
        labelStateValue.Name = "labelStateValue";
        labelStateValue.Size = new Size(360, 15);
        labelStateValue.TabIndex = 7;
        labelStateValue.Text = "-";
        //
        // groupLive
        //
        groupLive.Controls.Add(labelSteeringLiveCaption);
        groupLive.Controls.Add(labelSteeringLiveValue);
        groupLive.Controls.Add(labelGasLiveCaption);
        groupLive.Controls.Add(labelGasLiveValue);
        groupLive.Controls.Add(labelBrakeLiveCaption);
        groupLive.Controls.Add(labelBrakeLiveValue);
        groupLive.Location = new Point(18, 218);
        groupLive.Name = "groupLive";
        groupLive.Size = new Size(520, 132);
        groupLive.TabIndex = 7;
        groupLive.TabStop = false;
        groupLive.Text = "Live-Daten";
        groupLive.Anchor = AnchorStyles.Top | AnchorStyles.Left;
        //
        // labelSteeringLiveCaption
        //
        labelSteeringLiveCaption.AutoSize = true;
        labelSteeringLiveCaption.Location = new Point(17, 28);
        labelSteeringLiveCaption.Name = "labelSteeringLiveCaption";
        labelSteeringLiveCaption.Size = new Size(53, 15);
        labelSteeringLiveCaption.TabIndex = 0;
        labelSteeringLiveCaption.Text = "Lenkung:";
        //
        // labelSteeringLiveValue
        //
        labelSteeringLiveValue.Location = new Point(170, 28);
        labelSteeringLiveValue.Name = "labelSteeringLiveValue";
        labelSteeringLiveValue.Size = new Size(220, 20);
        labelSteeringLiveValue.TabIndex = 1;
        labelSteeringLiveValue.Text = "0";
        //
        // labelGasLiveCaption
        //
        labelGasLiveCaption.AutoSize = true;
        labelGasLiveCaption.Location = new Point(17, 63);
        labelGasLiveCaption.Name = "labelGasLiveCaption";
        labelGasLiveCaption.Size = new Size(30, 15);
        labelGasLiveCaption.TabIndex = 2;
        labelGasLiveCaption.Text = "Gas:";
        //
        // labelGasLiveValue
        //
        labelGasLiveValue.Location = new Point(170, 63);
        labelGasLiveValue.Name = "labelGasLiveValue";
        labelGasLiveValue.Size = new Size(220, 20);
        labelGasLiveValue.TabIndex = 3;
        labelGasLiveValue.Text = "0";
        //
        // labelBrakeLiveCaption
        //
        labelBrakeLiveCaption.AutoSize = true;
        labelBrakeLiveCaption.Location = new Point(17, 98);
        labelBrakeLiveCaption.Name = "labelBrakeLiveCaption";
        labelBrakeLiveCaption.Size = new Size(48, 15);
        labelBrakeLiveCaption.TabIndex = 4;
        labelBrakeLiveCaption.Text = "Bremse:";
        //
        // labelBrakeLiveValue
        //
        labelBrakeLiveValue.Location = new Point(170, 98);
        labelBrakeLiveValue.Name = "labelBrakeLiveValue";
        labelBrakeLiveValue.Size = new Size(220, 20);
        labelBrakeLiveValue.TabIndex = 5;
        labelBrakeLiveValue.Text = "0";
        // 
        // groupVehicle
        // 
        groupVehicle.Controls.Add(labelVehiclePlaceholder);
        groupVehicle.Location = new Point(556, 79);
        groupVehicle.Name = "groupVehicle";
        groupVehicle.Size = new Size(950, 614);
        groupVehicle.TabIndex = 8;
        groupVehicle.TabStop = false;
        groupVehicle.Text = "Fahrzeugdaten / Rueckkanal";
        groupVehicle.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        // 
        // labelVehiclePlaceholder
        // 
        labelVehiclePlaceholder.AutoSize = true;
        labelVehiclePlaceholder.Location = new Point(18, 31);
        labelVehiclePlaceholder.Name = "labelVehiclePlaceholder";
        labelVehiclePlaceholder.Size = new Size(283, 15);
        labelVehiclePlaceholder.TabIndex = 0;
        labelVehiclePlaceholder.Text = "Rechter Bereich reserviert fuer spaetere Fahrzeugdaten.";
        // 
        // textStatusLog
        // 
        textStatusLog.Location = new Point(18, 368);
        textStatusLog.Multiline = true;
        textStatusLog.Name = "textStatusLog";
        textStatusLog.ReadOnly = true;
        textStatusLog.ScrollBars = ScrollBars.Vertical;
        textStatusLog.Size = new Size(520, 325);
        textStatusLog.TabIndex = 8;
        textStatusLog.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left;
        // 
        // Form1
        // 
        AutoScaleDimensions = new SizeF(7F, 15F);
        AutoScaleMode = AutoScaleMode.Font;
        ClientSize = new Size(1524, 711);
        Controls.Add(textStatusLog);
        Controls.Add(groupVehicle);
        Controls.Add(groupLive);
        Controls.Add(groupStatus);
        Controls.Add(buttonStop);
        Controls.Add(buttonStart);
        Controls.Add(buttonRefreshPorts);
        Controls.Add(comboPorts);
        Controls.Add(labelPort);
        MinimumSize = new Size(1400, 750);
        Name = "Form1";
        StartPosition = FormStartPosition.CenterScreen;
        Text = "Lenkrad Sender";
        WindowState = FormWindowState.Maximized;
        groupStatus.ResumeLayout(false);
        groupStatus.PerformLayout();
        groupLive.ResumeLayout(false);
        groupLive.PerformLayout();
        groupVehicle.ResumeLayout(false);
        groupVehicle.PerformLayout();
        ResumeLayout(false);
        PerformLayout();
    }
}
