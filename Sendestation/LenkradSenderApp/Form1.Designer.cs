namespace LenkradSenderApp;

partial class Form1
{
    private System.ComponentModel.IContainer components = null;
    private MenuStrip menuMain;
    private ToolStripMenuItem menuSettings;
    private ToolStripMenuItem menuSpeedLimit;
    private ToolStripMenuItem menuDebug;
    private ToolStripMenuItem menuControlsHelp;
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
    private Label labelGearCaption;
    private Label labelGearValue;
    private Label labelVehicleLinkCaption;
    private Label labelVehicleLinkIcon;
    private Label labelVehicleLinkValue;
    private Label labelVehicleModeCaption;
    private Label labelVehicleModeIcon;
    private Label labelVehicleModeValue;
    private Label labelVehicleCameraCaption;
    private Label labelVehicleCameraIcon;
    private Label labelVehicleCameraValue;
    private Label labelVehicleLightCaption;
    private Label labelVehicleLightIcon;
    private Label labelVehicleLightValue;
    private Label labelVehicleBatteryCaption;
    private Label labelVehicleBatteryIcon;
    private Label labelVehicleBatteryValue;
    private Label labelVehicleTempCaption;
    private Label labelVehicleTempIcon;
    private Label labelVehicleTempValue;
    private Label labelVehicleDiffFrontCaption;
    private Label labelVehicleDiffFrontValue;
    private Label labelVehicleDiffRearCaption;
    private Label labelVehicleDiffRearValue;
    private Label labelVehicleDebugCaption;
    private Label labelVehicleDebugValue;
    private Label labelVehicleControlsCaption;
    private Label labelVehicleControlsValue;

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
        menuMain = new MenuStrip();
        menuSettings = new ToolStripMenuItem();
        menuSpeedLimit = new ToolStripMenuItem();
        menuDebug = new ToolStripMenuItem();
        menuControlsHelp = new ToolStripMenuItem();
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
        labelGearCaption = new Label();
        labelGearValue = new Label();
        labelVehicleLinkCaption = new Label();
        labelVehicleLinkIcon = new Label();
        labelVehicleLinkValue = new Label();
        labelVehicleModeCaption = new Label();
        labelVehicleModeIcon = new Label();
        labelVehicleModeValue = new Label();
        labelVehicleCameraCaption = new Label();
        labelVehicleCameraIcon = new Label();
        labelVehicleCameraValue = new Label();
        labelVehicleLightCaption = new Label();
        labelVehicleLightIcon = new Label();
        labelVehicleLightValue = new Label();
        labelVehicleBatteryCaption = new Label();
        labelVehicleBatteryIcon = new Label();
        labelVehicleBatteryValue = new Label();
        labelVehicleTempCaption = new Label();
        labelVehicleTempIcon = new Label();
        labelVehicleTempValue = new Label();
        labelVehicleDiffFrontCaption = new Label();
        labelVehicleDiffFrontValue = new Label();
        labelVehicleDiffRearCaption = new Label();
        labelVehicleDiffRearValue = new Label();
        labelVehicleDebugCaption = new Label();
        labelVehicleDebugValue = new Label();
        labelVehicleControlsCaption = new Label();
        labelVehicleControlsValue = new Label();
        groupStatus.SuspendLayout();
        groupLive.SuspendLayout();
        groupVehicle.SuspendLayout();
        SuspendLayout();
        // 
        // menuMain
        // 
        menuMain.Items.AddRange(new ToolStripItem[] { menuSettings, menuControlsHelp });
        menuMain.Location = new Point(0, 0);
        menuMain.Name = "menuMain";
        menuMain.Size = new Size(1524, 24);
        menuMain.TabIndex = 0;
        menuMain.Text = "menuMain";
        // 
        // menuSettings
        // 
        menuSettings.DropDownItems.AddRange(new ToolStripItem[] { menuSpeedLimit, menuDebug });
        menuSettings.Name = "menuSettings";
        menuSettings.Size = new Size(61, 20);
        menuSettings.Text = "Settings";
        //
        // menuSpeedLimit
        // 
        menuSpeedLimit.Name = "menuSpeedLimit";
        menuSpeedLimit.Size = new Size(133, 22);
        menuSpeedLimit.Text = "Speedlimit";
        menuSpeedLimit.Click += menuSpeedLimit_Click;
        //
        // menuDebug
        // 
        menuDebug.CheckOnClick = true;
        menuDebug.Name = "menuDebug";
        menuDebug.Size = new Size(133, 22);
        menuDebug.Text = "Debug";
        menuDebug.CheckedChanged += menuDebug_CheckedChanged;
        // 
        // menuControlsHelp
        // 
        menuControlsHelp.Name = "menuControlsHelp";
        menuControlsHelp.Size = new Size(123, 20);
        menuControlsHelp.Text = "Bedienungshinweise";
        menuControlsHelp.Click += menuControlsHelp_Click;
        // 
        // labelPort
        // 
        labelPort.AutoSize = true;
        labelPort.Location = new Point(18, 44);
        labelPort.Name = "labelPort";
        labelPort.Size = new Size(61, 15);
        labelPort.TabIndex = 0;
        labelPort.Text = "COM-Port";
        // 
        // comboPorts
        // 
        comboPorts.DropDownStyle = ComboBoxStyle.DropDownList;
        comboPorts.FormattingEnabled = true;
        comboPorts.Location = new Point(18, 62);
        comboPorts.Name = "comboPorts";
        comboPorts.Size = new Size(150, 23);
        comboPorts.TabIndex = 1;
        // 
        // buttonRefreshPorts
        // 
        buttonRefreshPorts.Location = new Point(180, 61);
        buttonRefreshPorts.Name = "buttonRefreshPorts";
        buttonRefreshPorts.Size = new Size(95, 25);
        buttonRefreshPorts.TabIndex = 2;
        buttonRefreshPorts.Text = "Ports neu";
        buttonRefreshPorts.UseVisualStyleBackColor = true;
        buttonRefreshPorts.Click += buttonRefreshPorts_Click;
        // 
        // buttonStart
        // 
        buttonStart.Location = new Point(292, 61);
        buttonStart.Name = "buttonStart";
        buttonStart.Size = new Size(95, 25);
        buttonStart.TabIndex = 3;
        buttonStart.Text = "Start";
        buttonStart.UseVisualStyleBackColor = true;
        buttonStart.Click += buttonStart_Click;
        // 
        // buttonStop
        // 
        buttonStop.Location = new Point(402, 61);
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
        groupStatus.Location = new Point(18, 103);
        groupStatus.Name = "groupStatus";
        groupStatus.Size = new Size(520, 123);
        groupStatus.TabIndex = 6;
        groupStatus.TabStop = false;
        groupStatus.Text = "Status";
        groupStatus.Anchor = AnchorStyles.Top | AnchorStyles.Left;
        groupStatus.BackColor = Color.FromArgb(252, 253, 255);
        groupStatus.ForeColor = Color.FromArgb(36, 42, 50);
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
        groupLive.Location = new Point(18, 242);
        groupLive.Name = "groupLive";
        groupLive.Size = new Size(520, 132);
        groupLive.TabIndex = 7;
        groupLive.TabStop = false;
        groupLive.Text = "Live-Daten";
        groupLive.Anchor = AnchorStyles.Top | AnchorStyles.Left;
        groupLive.BackColor = Color.FromArgb(252, 253, 255);
        groupLive.ForeColor = Color.FromArgb(36, 42, 50);
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
        groupVehicle.Controls.Add(labelGearCaption);
        groupVehicle.Controls.Add(labelGearValue);
        groupVehicle.Controls.Add(labelVehicleLinkCaption);
        groupVehicle.Controls.Add(labelVehicleLinkIcon);
        groupVehicle.Controls.Add(labelVehicleLinkValue);
        groupVehicle.Controls.Add(labelVehicleModeCaption);
        groupVehicle.Controls.Add(labelVehicleModeIcon);
        groupVehicle.Controls.Add(labelVehicleModeValue);
        groupVehicle.Controls.Add(labelVehicleCameraCaption);
        groupVehicle.Controls.Add(labelVehicleCameraIcon);
        groupVehicle.Controls.Add(labelVehicleCameraValue);
        groupVehicle.Controls.Add(labelVehicleLightCaption);
        groupVehicle.Controls.Add(labelVehicleLightIcon);
        groupVehicle.Controls.Add(labelVehicleLightValue);
        groupVehicle.Controls.Add(labelVehicleBatteryCaption);
        groupVehicle.Controls.Add(labelVehicleBatteryIcon);
        groupVehicle.Controls.Add(labelVehicleBatteryValue);
        groupVehicle.Controls.Add(labelVehicleTempCaption);
        groupVehicle.Controls.Add(labelVehicleTempIcon);
        groupVehicle.Controls.Add(labelVehicleTempValue);
        groupVehicle.Controls.Add(labelVehicleDiffFrontCaption);
        groupVehicle.Controls.Add(labelVehicleDiffFrontValue);
        groupVehicle.Controls.Add(labelVehicleDiffRearCaption);
        groupVehicle.Controls.Add(labelVehicleDiffRearValue);
        groupVehicle.Controls.Add(labelVehicleDebugCaption);
        groupVehicle.Controls.Add(labelVehicleDebugValue);
        groupVehicle.Controls.Add(labelVehicleControlsCaption);
        groupVehicle.Controls.Add(labelVehicleControlsValue);
        groupVehicle.Location = new Point(570, 103);
        groupVehicle.Name = "groupVehicle";
        groupVehicle.Size = new Size(936, 590);
        groupVehicle.TabIndex = 8;
        groupVehicle.TabStop = false;
        groupVehicle.Text = "Fahrzeugdaten / Rueckkanal";
        groupVehicle.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
        groupVehicle.BackColor = Color.FromArgb(250, 252, 255);
        groupVehicle.ForeColor = Color.FromArgb(34, 40, 48);
        // 
        // labelGearCaption
        // 
        labelGearCaption.AutoSize = true;
        labelGearCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelGearCaption.Location = new Point(24, 32);
        labelGearCaption.Name = "labelGearCaption";
        labelGearCaption.Size = new Size(101, 25);
        labelGearCaption.TabIndex = 0;
        labelGearCaption.Text = "Fahrstufe";
        // 
        // labelGearValue
        // 
        labelGearValue.Font = new Font("Segoe UI", 92F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelGearValue.Location = new Point(18, 62);
        labelGearValue.Name = "labelGearValue";
        labelGearValue.Size = new Size(200, 160);
        labelGearValue.TabIndex = 1;
        labelGearValue.Text = "-";
        labelGearValue.TextAlign = ContentAlignment.MiddleLeft;
        // 
        // labelVehicleLinkCaption
        // 
        labelVehicleLinkCaption.AutoSize = true;
        labelVehicleLinkCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleLinkCaption.Location = new Point(315, 42);
        labelVehicleLinkCaption.Name = "labelVehicleLinkCaption";
        labelVehicleLinkCaption.Size = new Size(119, 25);
        labelVehicleLinkCaption.TabIndex = 2;
        labelVehicleLinkCaption.Text = "Rueckkanal:";
        // 
        // labelVehicleLinkIcon
        // 
        labelVehicleLinkIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleLinkIcon.ForeColor = Color.DeepSkyBlue;
        labelVehicleLinkIcon.Location = new Point(260, 34);
        labelVehicleLinkIcon.Name = "labelVehicleLinkIcon";
        labelVehicleLinkIcon.Size = new Size(42, 42);
        labelVehicleLinkIcon.TabIndex = 3;
        labelVehicleLinkIcon.Text = "📡";
        labelVehicleLinkIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleLinkValue
        // 
        labelVehicleLinkValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleLinkValue.Location = new Point(470, 42);
        labelVehicleLinkValue.Name = "labelVehicleLinkValue";
        labelVehicleLinkValue.Size = new Size(180, 34);
        labelVehicleLinkValue.TabIndex = 4;
        labelVehicleLinkValue.Text = "Inaktiv";
        // 
        // labelVehicleModeCaption
        // 
        labelVehicleModeCaption.AutoSize = true;
        labelVehicleModeCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleModeCaption.Location = new Point(315, 97);
        labelVehicleModeCaption.Name = "labelVehicleModeCaption";
        labelVehicleModeCaption.Size = new Size(122, 25);
        labelVehicleModeCaption.TabIndex = 5;
        labelVehicleModeCaption.Text = "Fahrmodus:";
        // 
        // labelVehicleModeIcon
        // 
        labelVehicleModeIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleModeIcon.ForeColor = Color.DarkOrange;
        labelVehicleModeIcon.Location = new Point(260, 89);
        labelVehicleModeIcon.Name = "labelVehicleModeIcon";
        labelVehicleModeIcon.Size = new Size(42, 42);
        labelVehicleModeIcon.TabIndex = 6;
        labelVehicleModeIcon.Text = "🏁";
        labelVehicleModeIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleModeValue
        // 
        labelVehicleModeValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleModeValue.Location = new Point(470, 97);
        labelVehicleModeValue.Name = "labelVehicleModeValue";
        labelVehicleModeValue.Size = new Size(180, 34);
        labelVehicleModeValue.TabIndex = 7;
        labelVehicleModeValue.Text = "-";
        // 
        // labelVehicleCameraCaption
        // 
        labelVehicleCameraCaption.AutoSize = true;
        labelVehicleCameraCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleCameraCaption.Location = new Point(315, 152);
        labelVehicleCameraCaption.Name = "labelVehicleCameraCaption";
        labelVehicleCameraCaption.Size = new Size(88, 25);
        labelVehicleCameraCaption.TabIndex = 8;
        labelVehicleCameraCaption.Text = "Kamera:";
        // 
        // labelVehicleCameraIcon
        // 
        labelVehicleCameraIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleCameraIcon.ForeColor = Color.DeepSkyBlue;
        labelVehicleCameraIcon.Location = new Point(260, 144);
        labelVehicleCameraIcon.Name = "labelVehicleCameraIcon";
        labelVehicleCameraIcon.Size = new Size(42, 42);
        labelVehicleCameraIcon.TabIndex = 9;
        labelVehicleCameraIcon.Text = "📷";
        labelVehicleCameraIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleCameraValue
        // 
        labelVehicleCameraValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleCameraValue.Location = new Point(470, 152);
        labelVehicleCameraValue.Name = "labelVehicleCameraValue";
        labelVehicleCameraValue.Size = new Size(260, 34);
        labelVehicleCameraValue.TabIndex = 10;
        labelVehicleCameraValue.Text = "-";
        // 
        // labelVehicleLightCaption
        // 
        labelVehicleLightCaption.AutoSize = true;
        labelVehicleLightCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleLightCaption.Location = new Point(315, 207);
        labelVehicleLightCaption.Name = "labelVehicleLightCaption";
        labelVehicleLightCaption.Size = new Size(60, 25);
        labelVehicleLightCaption.TabIndex = 11;
        labelVehicleLightCaption.Text = "Licht:";
        // 
        // labelVehicleLightIcon
        // 
        labelVehicleLightIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleLightIcon.ForeColor = Color.LimeGreen;
        labelVehicleLightIcon.Location = new Point(260, 199);
        labelVehicleLightIcon.Name = "labelVehicleLightIcon";
        labelVehicleLightIcon.Size = new Size(42, 42);
        labelVehicleLightIcon.TabIndex = 12;
        labelVehicleLightIcon.Text = "💡";
        labelVehicleLightIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleLightValue
        // 
        labelVehicleLightValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleLightValue.Location = new Point(470, 207);
        labelVehicleLightValue.Name = "labelVehicleLightValue";
        labelVehicleLightValue.Size = new Size(180, 34);
        labelVehicleLightValue.TabIndex = 13;
        labelVehicleLightValue.Text = "-";
        // 
        // labelVehicleBatteryCaption
        // 
        labelVehicleBatteryCaption.AutoSize = true;
        labelVehicleBatteryCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleBatteryCaption.Location = new Point(315, 262);
        labelVehicleBatteryCaption.Name = "labelVehicleBatteryCaption";
        labelVehicleBatteryCaption.Size = new Size(89, 25);
        labelVehicleBatteryCaption.TabIndex = 14;
        labelVehicleBatteryCaption.Text = "Akku [%]:";
        // 
        // labelVehicleBatteryIcon
        // 
        labelVehicleBatteryIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleBatteryIcon.ForeColor = Color.DeepSkyBlue;
        labelVehicleBatteryIcon.Location = new Point(260, 254);
        labelVehicleBatteryIcon.Name = "labelVehicleBatteryIcon";
        labelVehicleBatteryIcon.Size = new Size(42, 42);
        labelVehicleBatteryIcon.TabIndex = 15;
        labelVehicleBatteryIcon.Text = "🔋";
        labelVehicleBatteryIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleBatteryValue
        // 
        labelVehicleBatteryValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleBatteryValue.Location = new Point(470, 262);
        labelVehicleBatteryValue.Name = "labelVehicleBatteryValue";
        labelVehicleBatteryValue.Size = new Size(180, 34);
        labelVehicleBatteryValue.TabIndex = 16;
        labelVehicleBatteryValue.Text = "--";
        // 
        // labelVehicleTempCaption
        // 
        labelVehicleTempCaption.AutoSize = true;
        labelVehicleTempCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleTempCaption.Location = new Point(315, 317);
        labelVehicleTempCaption.Name = "labelVehicleTempCaption";
        labelVehicleTempCaption.Size = new Size(92, 25);
        labelVehicleTempCaption.TabIndex = 17;
        labelVehicleTempCaption.Text = "Akku [°C]:";
        // 
        // labelVehicleTempIcon
        // 
        labelVehicleTempIcon.Font = new Font("Segoe UI Emoji", 20F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleTempIcon.ForeColor = Color.OrangeRed;
        labelVehicleTempIcon.Location = new Point(260, 309);
        labelVehicleTempIcon.Name = "labelVehicleTempIcon";
        labelVehicleTempIcon.Size = new Size(42, 42);
        labelVehicleTempIcon.TabIndex = 18;
        labelVehicleTempIcon.Text = "";
        labelVehicleTempIcon.TextAlign = ContentAlignment.MiddleCenter;
        // 
        // labelVehicleTempValue
        // 
        labelVehicleTempValue.Font = new Font("Segoe UI", 18F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleTempValue.Location = new Point(470, 317);
        labelVehicleTempValue.Name = "labelVehicleTempValue";
        labelVehicleTempValue.Size = new Size(180, 34);
        labelVehicleTempValue.TabIndex = 19;
        labelVehicleTempValue.Text = "--";
        //
        // labelVehicleDiffFrontCaption
        //
        labelVehicleDiffFrontCaption.AutoSize = true;
        labelVehicleDiffFrontCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleDiffFrontCaption.Location = new Point(24, 262);
        labelVehicleDiffFrontCaption.Name = "labelVehicleDiffFrontCaption";
        labelVehicleDiffFrontCaption.Size = new Size(91, 25);
        labelVehicleDiffFrontCaption.TabIndex = 24;
        labelVehicleDiffFrontCaption.Text = "Diff vorn:";
        //
        // labelVehicleDiffFrontValue
        //
        labelVehicleDiffFrontValue.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleDiffFrontValue.ForeColor = Color.ForestGreen;
        labelVehicleDiffFrontValue.Location = new Point(145, 262);
        labelVehicleDiffFrontValue.Name = "labelVehicleDiffFrontValue";
        labelVehicleDiffFrontValue.Size = new Size(110, 25);
        labelVehicleDiffFrontValue.TabIndex = 25;
        labelVehicleDiffFrontValue.Text = "Frei";
        //
        // labelVehicleDiffRearCaption
        //
        labelVehicleDiffRearCaption.AutoSize = true;
        labelVehicleDiffRearCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleDiffRearCaption.Location = new Point(24, 317);
        labelVehicleDiffRearCaption.Name = "labelVehicleDiffRearCaption";
        labelVehicleDiffRearCaption.Size = new Size(103, 25);
        labelVehicleDiffRearCaption.TabIndex = 26;
        labelVehicleDiffRearCaption.Text = "Diff hinten:";
        //
        // labelVehicleDiffRearValue
        //
        labelVehicleDiffRearValue.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleDiffRearValue.ForeColor = Color.ForestGreen;
        labelVehicleDiffRearValue.Location = new Point(145, 317);
        labelVehicleDiffRearValue.Name = "labelVehicleDiffRearValue";
        labelVehicleDiffRearValue.Size = new Size(110, 25);
        labelVehicleDiffRearValue.TabIndex = 27;
        labelVehicleDiffRearValue.Text = "Frei";
        // 
        // labelVehicleDebugCaption
        // 
        labelVehicleDebugCaption.AutoSize = true;
        labelVehicleDebugCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleDebugCaption.Location = new Point(710, 42);
        labelVehicleDebugCaption.Name = "labelVehicleDebugCaption";
        labelVehicleDebugCaption.Size = new Size(74, 25);
        labelVehicleDebugCaption.TabIndex = 20;
        labelVehicleDebugCaption.Text = "Debug:";
        // 
        // labelVehicleDebugValue
        // 
        labelVehicleDebugValue.Font = new Font("Consolas", 11F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleDebugValue.Location = new Point(710, 78);
        labelVehicleDebugValue.Name = "labelVehicleDebugValue";
        labelVehicleDebugValue.Size = new Size(190, 220);
        labelVehicleDebugValue.TabIndex = 21;
        labelVehicleDebugValue.Text = "RX Bytes: 0\r\nFrames: 0\r\nCRC Fehler: 0\r\nFlightMode: 0\r\nBattery: 0\r\nDevice: 0\r\nLast Type: 0x00\r\nValid: 0";
        // 
        // labelVehicleControlsCaption
        // 
        labelVehicleControlsCaption.AutoSize = true;
        labelVehicleControlsCaption.Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0);
        labelVehicleControlsCaption.Location = new Point(24, 504);
        labelVehicleControlsCaption.Name = "labelVehicleControlsCaption";
        labelVehicleControlsCaption.Size = new Size(137, 25);
        labelVehicleControlsCaption.TabIndex = 22;
        labelVehicleControlsCaption.Text = "Tastenbelegung";
        // 
        // labelVehicleControlsValue
        // 
        labelVehicleControlsValue.Font = new Font("Segoe UI", 10.5F, FontStyle.Regular, GraphicsUnit.Point, 0);
        labelVehicleControlsValue.Location = new Point(24, 536);
        labelVehicleControlsValue.Name = "labelVehicleControlsValue";
        labelVehicleControlsValue.Size = new Size(860, 58);
        labelVehicleControlsValue.TabIndex = 23;
        labelVehicleControlsValue.Text = "PS: Sicherheits-N   |   Up Shift: D   |   Down Shift: R\r\nL2: Fahrmodus Sport/Drive   |   R2: Kamera vorne/hinten   |   R1: Kamera 0 Grad\r\nL1 kurz: Licht ein/aus   |   L1 lang: Lichthupe 2x";
        // 
        // textStatusLog
        // 
        textStatusLog.Location = new Point(18, 392);
        textStatusLog.Multiline = true;
        textStatusLog.Name = "textStatusLog";
        textStatusLog.BackColor = Color.FromArgb(255, 255, 255);
        textStatusLog.BorderStyle = BorderStyle.FixedSingle;
        textStatusLog.ReadOnly = true;
        textStatusLog.ScrollBars = ScrollBars.Vertical;
        textStatusLog.Size = new Size(520, 301);
        textStatusLog.TabIndex = 8;
        textStatusLog.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left;
        // 
        // Form1
        // 
        AutoScaleDimensions = new SizeF(7F, 15F);
        AutoScaleMode = AutoScaleMode.Font;
        BackColor = Color.FromArgb(236, 240, 246);
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
        Controls.Add(menuMain);
        MinimumSize = new Size(1400, 750);
        MainMenuStrip = menuMain;
        Name = "Form1";
        StartPosition = FormStartPosition.CenterScreen;
        Text = "Vehicle Ground Station";
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

