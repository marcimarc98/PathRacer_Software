using System.IO.Ports;
using System.Drawing.Drawing2D;

namespace LenkradSenderApp;

public partial class Form1 : Form
{
    private readonly SenderService _senderService = new();
    private readonly System.Windows.Forms.Timer _statusTimer = new();
    private string _lastStatusLine = string.Empty;
    private GroupBox? _groupLinkQuality;
    private Label? _labelUpLqValue;
    private Label? _labelUpRssiValue;
    private Label? _labelUpSnrValue;
    private Label? _labelDownLqValue;
    private Label? _labelDownRssiValue;
    private Label? _labelDownSnrValue;
    private Label? _labelRfProfileValue;
    private Label? _labelTxPowerValue;
    private ThemedGroupBox _groupConnections = null!;
    private Label _labelWheelConnection = null!;
    private Button _buttonWheelStart = null!;
    private Button _buttonWheelStop = null!;
    private string _probedWheelName = "Nicht verbunden";

    public Form1()
    {
        InitializeComponent();
        InitializeConnectionHeader();
        SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint, true);
        UpdateStyles();
        ApplyWindowIcon();
        InitializeCockpitIcons();
        InitializeLinkQualityPanel();
        ApplyDebugVisibility();
        labelVehicleControlsCaption.Visible = false;
        labelVehicleControlsValue.Visible = false;

        _senderService.StatusMessage += HandleStatusMessage;
        RefreshComPorts();
        ProbeWheel();
        AppendStatus("App bereit.");
        UpdateStatus();

        _statusTimer.Interval = 200;
        _statusTimer.Tick += (_, _) => UpdateStatus();
        _statusTimer.Start();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

        var leftRect = Rectangle.FromLTRB(
            groupStatus.Left - 12,
            groupStatus.Top - 12,
            textStatusLog.Right + 12,
            textStatusLog.Bottom + 12);

        var rightRect = Rectangle.FromLTRB(
            groupVehicle.Left - 8,
            groupVehicle.Top - 12,
            groupVehicle.Right + 12,
            groupVehicle.Bottom + 12);

        DrawSectionCard(
            e.Graphics,
            leftRect,
            Color.FromArgb(252, 253, 255),
            Color.FromArgb(165, 175, 190),
            Color.FromArgb(0, 0, 0, 0));

        DrawSectionCard(
            e.Graphics,
            rightRect,
            Color.FromArgb(250, 252, 255),
            Color.FromArgb(148, 160, 178),
            Color.FromArgb(0, 0, 0, 0));
    }

    private void ApplyWindowIcon()
    {
        try
        {
            var executablePath = Application.ExecutablePath;
            var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "LenkradSenderApp.ico");

            if (File.Exists(iconPath))
            {
                Icon = new Icon(iconPath);
            }
            else if (File.Exists(executablePath))
            {
                Icon = Icon.ExtractAssociatedIcon(executablePath);
            }
        }
        catch
        {
        }
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _statusTimer.Stop();
        _senderService.StatusMessage -= HandleStatusMessage;
        _senderService.Dispose();
        base.OnFormClosing(e);
    }

    private void menuDebug_CheckedChanged(object? sender, EventArgs e)
    {
        ApplyDebugVisibility();
        BeginInvoke(new Action(() =>
        {
            if (!IsDisposed && menuSettings.Visible)
            {
                menuSettings.ShowDropDown();
            }
        }));
    }

    private void menuControlsHelp_Click(object? sender, EventArgs e)
    {
        using var helpForm = new Form
        {
            Text = "Bedienungshinweise",
            StartPosition = FormStartPosition.CenterParent,
            ClientSize = new Size(860, 430),
            MinimumSize = new Size(780, 380),
            BackColor = Color.FromArgb(246, 249, 253),
            Font = new Font("Segoe UI", 11F, FontStyle.Regular, GraphicsUnit.Point, 0),
            Icon = Icon
        };

        var titleLabel = new Label
        {
            Text = "Aktuelle Tastenbelegung",
            Dock = DockStyle.Top,
            Height = 44,
            Font = new Font("Segoe UI", 14F, FontStyle.Bold, GraphicsUnit.Point, 0),
            ForeColor = Color.FromArgb(28, 36, 46),
            Padding = new Padding(18, 12, 18, 0)
        };

        var contentBox = new TextBox
        {
            Dock = DockStyle.Fill,
            Multiline = true,
            ReadOnly = true,
            ScrollBars = ScrollBars.Vertical,
            BorderStyle = BorderStyle.None,
            BackColor = Color.FromArgb(246, 249, 253),
            ForeColor = Color.FromArgb(40, 48, 58),
            Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0),
            Text =
                "L1 + R1 halten und einmal D/R waehlen: Fahrgasse aktiv" + Environment.NewLine +
                "Up Shift: D" + Environment.NewLine +
                "Down Shift: R" + Environment.NewLine +
                "Danach D/R ohne erneute Freigabe umschaltbar" + Environment.NewLine +
                "PS: Sicherheits-N" + Environment.NewLine +
                "L2: Fahrmodus Aggressiv/Normal" + Environment.NewLine +
                "R2: Kamera vorne/hinten" + Environment.NewLine +
                "R1: Licht ein/aus" + Environment.NewLine +
                "L1: Lichthupe",
            Margin = new Padding(0),
            TabStop = false
        };

        var contentPanel = new Panel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(18, 8, 18, 18)
        };
        contentPanel.Controls.Add(contentBox);

        helpForm.Controls.Add(contentPanel);
        helpForm.Controls.Add(titleLabel);
        helpForm.ShowDialog(this);
    }

    private void buttonRefreshPorts_Click(object sender, EventArgs e)
    {
        RefreshComPorts();
    }

    private void InitializeConnectionHeader()
    {
        _groupConnections = new ThemedGroupBox
        {
            Name = "groupConnections",
            Text = "Verbindungen",
            Location = new Point(18, 32),
            Size = new Size(790, 66),
            BackColor = Color.FromArgb(252, 253, 255),
            ForeColor = Color.FromArgb(36, 42, 50),
            Anchor = AnchorStyles.Top | AnchorStyles.Left
        };

        _labelWheelConnection = new Label
        {
            AutoSize = true,
            Location = new Point(18, 28),
            Name = "labelWheelConnection",
            Size = new Size(52, 15),
            Text = "Lenkrad:"
        };

        _buttonWheelStart = new Button
        {
            Location = new Point(84, 24),
            Name = "buttonWheelStart",
            Size = new Size(82, 26),
            Text = "Start",
            UseVisualStyleBackColor = true
        };
        _buttonWheelStart.Click += buttonWheelStart_Click;

        _buttonWheelStop = new Button
        {
            Location = new Point(174, 24),
            Name = "buttonWheelStop",
            Size = new Size(82, 26),
            Text = "Stop",
            UseVisualStyleBackColor = true
        };
        _buttonWheelStop.Click += buttonWheelStop_Click;

        labelPort.Text = "ESP32:";
        labelPort.Location = new Point(304, 28);
        comboPorts.Location = new Point(358, 25);
        comboPorts.Size = new Size(125, 23);
        buttonRefreshPorts.Location = new Point(492, 24);
        buttonRefreshPorts.Size = new Size(74, 26);
        buttonRefreshPorts.Text = "Ports";
        buttonStart.Location = new Point(578, 24);
        buttonStart.Size = new Size(96, 26);
        buttonStart.Text = "Verbinden";
        buttonStop.Location = new Point(682, 24);
        buttonStop.Size = new Size(82, 26);
        buttonStop.Text = "Trennen";

        labelSenderCaption.Text = "ESP:";
        labelPacketsCaption.Text = "ESP-Pakete:";
        labelStateCaption.Text = "Status:";

        Controls.Remove(labelPort);
        Controls.Remove(comboPorts);
        Controls.Remove(buttonRefreshPorts);
        Controls.Remove(buttonStart);
        Controls.Remove(buttonStop);

        _groupConnections.Controls.Add(_labelWheelConnection);
        _groupConnections.Controls.Add(_buttonWheelStart);
        _groupConnections.Controls.Add(_buttonWheelStop);
        _groupConnections.Controls.Add(labelPort);
        _groupConnections.Controls.Add(comboPorts);
        _groupConnections.Controls.Add(buttonRefreshPorts);
        _groupConnections.Controls.Add(buttonStart);
        _groupConnections.Controls.Add(buttonStop);

        Controls.Add(_groupConnections);
        _groupConnections.BringToFront();
    }

    private void buttonWheelStart_Click(object? sender, EventArgs e)
    {
        try
        {
            _senderService.StartWheel();
            AppendStatus("Lenkrad gestartet.");
        }
        catch (Exception ex)
        {
            AppendStatus($"Lenkrad-Start fehlgeschlagen: {ex.Message}");
            MessageBox.Show(this, ex.Message, "Lenkrad-Start fehlgeschlagen");
        }

        UpdateStatus();
    }

    private void buttonWheelStop_Click(object? sender, EventArgs e)
    {
        _senderService.StopWheel();
        AppendStatus("Lenkrad gestoppt.");
        UpdateStatus();
    }

    private void InitializeCockpitIcons()
    {
        labelVehicleLinkIcon.Image = LoadCockpitIcon("link-ui.png", () => CreateLinkIcon(Color.DeepSkyBlue));
        labelVehicleLinkIcon.Text = string.Empty;

        labelVehicleModeIcon.Image = LoadCockpitIcon("drive-mode-ui.png", () => CreateDriveModeIcon(Color.DarkOrange));
        labelVehicleModeIcon.Text = string.Empty;

        labelVehicleCameraIcon.Image = LoadCockpitIcon("camera-ui.png", () => CreateCameraIcon(Color.DeepSkyBlue));
        labelVehicleCameraIcon.Text = string.Empty;

        labelVehicleLightIcon.Image = LoadCockpitIcon("light-ui.png", () => CreateLowBeamIcon(Color.LimeGreen));
        labelVehicleLightIcon.Text = string.Empty;

        labelVehicleBatteryIcon.Image = LoadCockpitIcon("battery-ui.png", () => CreateBatteryIcon(Color.DeepSkyBlue));
        labelVehicleBatteryIcon.Text = string.Empty;

        labelVehicleTempIcon.Image = LoadCockpitIcon("temp-ui.png", () => CreateTempIcon(Color.OrangeRed));
        labelVehicleTempIcon.Text = string.Empty;
    }

    private void InitializeLinkQualityPanel()
    {
        _groupLinkQuality = new GroupBox
        {
            Name = "groupLinkQuality",
            Text = "Funkqualitaet",
            Location = new Point(24, 454),
            Size = new Size(620, 136),
            BackColor = Color.FromArgb(250, 252, 255),
            ForeColor = Color.FromArgb(34, 40, 48),
            Font = new Font("Segoe UI", 11F, FontStyle.Bold, GraphicsUnit.Point, 0),
            Anchor = AnchorStyles.Left | AnchorStyles.Bottom
        };

        AddQualityRow(_groupLinkQuality, 24, "Uplink LQ", out _labelUpLqValue, "Downlink LQ", out _labelDownLqValue);
        AddQualityRow(_groupLinkQuality, 52, "Uplink RSSI", out _labelUpRssiValue, "Downlink RSSI", out _labelDownRssiValue);
        AddQualityRow(_groupLinkQuality, 80, "Uplink SNR", out _labelUpSnrValue, "Downlink SNR", out _labelDownSnrValue);
        AddQualityRow(_groupLinkQuality, 108, "RF Profil", out _labelRfProfileValue, "TX Power", out _labelTxPowerValue);

        groupVehicle.Controls.Add(_groupLinkQuality);
        _groupLinkQuality.BringToFront();
    }

    private static void AddQualityRow(Control parent, int top, string leftCaption, out Label leftValue, string rightCaption, out Label rightValue)
    {
        var leftCaptionLabel = new Label
        {
            AutoSize = false,
            Location = new Point(22, top),
            Size = new Size(104, 24),
            Font = new Font("Segoe UI", 10F, FontStyle.Bold, GraphicsUnit.Point, 0),
            ForeColor = Color.FromArgb(54, 64, 78),
            Text = leftCaption + ":"
        };

        leftValue = new Label
        {
            AutoSize = false,
            Location = new Point(128, top),
            Size = new Size(118, 24),
            Font = new Font("Segoe UI", 10.5F, FontStyle.Regular, GraphicsUnit.Point, 0),
            ForeColor = Color.FromArgb(28, 36, 46),
            Text = "--"
        };

        var rightCaptionLabel = new Label
        {
            AutoSize = false,
            Location = new Point(320, top),
            Size = new Size(120, 24),
            Font = new Font("Segoe UI", 10F, FontStyle.Bold, GraphicsUnit.Point, 0),
            ForeColor = Color.FromArgb(54, 64, 78),
            Text = rightCaption + ":"
        };

        rightValue = new Label
        {
            AutoSize = false,
            Location = new Point(448, top),
            Size = new Size(148, 24),
            Font = new Font("Segoe UI", 10.5F, FontStyle.Regular, GraphicsUnit.Point, 0),
            ForeColor = Color.FromArgb(28, 36, 46),
            Text = "--"
        };

        parent.Controls.Add(leftCaptionLabel);
        parent.Controls.Add(leftValue);
        parent.Controls.Add(rightCaptionLabel);
        parent.Controls.Add(rightValue);
    }

    private static string FormatRssi(byte value) => value > 0 ? $"-{value} dBm" : "--";
    private static string FormatSnr(sbyte value) => value == 0 ? "0 dB" : $"{value:+#;-#;0} dB";

    private static string FormatRfProfile(byte value) => $"Profil {value}";

    private static string FormatTxPower(byte value) => value switch
    {
        0 => "0 mW",
        1 => "10 mW",
        2 => "25 mW",
        3 => "100 mW",
        4 => "500 mW",
        5 => "1000 mW",
        6 => "2000 mW",
        7 => "250 mW",
        8 => "50 mW",
        _ => $"Code {value}"
    };

    private static Image LoadCockpitIcon(string fileName, Func<Bitmap> fallbackFactory)
    {
        try
        {
            var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", fileName);

            if (File.Exists(iconPath))
            {
                using var source = Image.FromFile(iconPath);
                return new Bitmap(source, new Size(40, 40));
            }
        }
        catch
        {
        }

        return fallbackFactory();
    }

    private static Bitmap CreateCanvas(int width = 40, int height = 40)
    {
        return new Bitmap(width, height);
    }

    private static void PrepareGraphics(Graphics graphics)
    {
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
        graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
        graphics.Clear(Color.Transparent);
    }

    private static Bitmap CreateLinkIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 3f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };

        PrepareGraphics(graphics);
        graphics.DrawLine(pen, 20, 28, 20, 13);
        graphics.DrawLine(pen, 15, 33, 20, 28);
        graphics.DrawLine(pen, 25, 33, 20, 28);
        graphics.DrawArc(pen, 11, 7, 18, 18, 210, 120);
        graphics.DrawArc(pen, 6, 3, 28, 28, 215, 110);
        return bitmap;
    }

    private static Bitmap CreateDriveModeIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 2.8f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };
        using var tickPen = new Pen(color, 2.2f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };
        using var centerBrush = new SolidBrush(color);

        PrepareGraphics(graphics);
        graphics.DrawArc(pen, 7, 10, 26, 22, 200, 140);
        graphics.DrawLine(tickPen, 11, 23, 9, 27);
        graphics.DrawLine(tickPen, 17, 15, 15, 11);
        graphics.DrawLine(tickPen, 24, 15, 26, 11);
        graphics.DrawLine(tickPen, 29, 23, 31, 27);
        graphics.DrawLine(pen, 20, 21, 27, 14);
        graphics.FillEllipse(centerBrush, 18, 19, 5, 5);

        return bitmap;
    }

    private static Bitmap CreateCameraIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 2.6f);
        using var lensPen = new Pen(color, 2.2f);

        PrepareGraphics(graphics);
        using (var path = CreateRoundedRectanglePath(new RectangleF(8, 13, 24, 16), 4f))
        {
            graphics.DrawPath(pen, path);
        }
        graphics.DrawEllipse(lensPen, 16, 17, 8, 8);
        graphics.DrawLine(pen, 13, 13, 17, 9);
        graphics.DrawLine(pen, 17, 9, 25, 9);
        graphics.DrawLine(pen, 25, 9, 27, 13);

        return bitmap;
    }

    private static Bitmap CreateLowBeamIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 2.8f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };

        PrepareGraphics(graphics);
        graphics.DrawLine(pen, 10, 10, 10, 30);
        graphics.DrawArc(pen, 6, 10, 18, 20, -90, 180);
        graphics.DrawLine(pen, 24, 13, 33, 10);
        graphics.DrawLine(pen, 24, 20, 34, 20);
        graphics.DrawLine(pen, 24, 27, 33, 30);

        return bitmap;
    }

    private static Bitmap CreateBatteryIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 2.4f);
        using var brush = new SolidBrush(color);

        PrepareGraphics(graphics);
        using (var path = CreateRoundedRectanglePath(new RectangleF(6, 11, 24, 18), 4f))
        {
            graphics.DrawPath(pen, path);
        }
        graphics.FillRectangle(brush, 30, 16, 4, 8);
        graphics.FillRectangle(brush, 10, 15, 4, 10);
        graphics.FillRectangle(brush, 16, 15, 4, 10);
        graphics.FillRectangle(brush, 22, 15, 4, 10);

        return bitmap;
    }

    private static Bitmap CreateTempIcon(Color color)
    {
        var bitmap = CreateCanvas();

        using var graphics = Graphics.FromImage(bitmap);
        using var pen = new Pen(color, 2.4f);
        using var brush = new SolidBrush(color);

        PrepareGraphics(graphics);
        graphics.DrawEllipse(pen, 14, 24, 12, 12);
        graphics.DrawLine(pen, 20, 10, 20, 28);
        graphics.DrawArc(pen, 16, 10, 8, 8, 180, 180);
        graphics.FillEllipse(brush, 17, 26, 6, 6);

        return bitmap;
    }

    private static GraphicsPath CreateRoundedRectanglePath(RectangleF rect, float radius)
    {
        var diameter = radius * 2f;
        var path = new GraphicsPath();

        path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
        path.AddArc(rect.Right - diameter, rect.Y, diameter, diameter, 270, 90);
        path.AddArc(rect.Right - diameter, rect.Bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(rect.X, rect.Bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();

        return path;
    }

    private static void DrawSectionCard(Graphics graphics, Rectangle bounds, Color fillColor, Color borderColor, Color shadowColor)
    {
        using (var fillPath = CreateRoundedRectanglePath(bounds, 16f))
        using (var fillBrush = new SolidBrush(fillColor))
        using (var borderPen = new Pen(borderColor, 1.8f))
        {
            graphics.FillPath(fillBrush, fillPath);
            graphics.DrawPath(borderPen, fillPath);
        }
    }

    private void ApplyDebugVisibility()
    {
        var showDebug = menuDebug.Checked;
        labelVehicleDebugCaption.Visible = showDebug;
        labelVehicleDebugValue.Visible = showDebug;
    }

    private void buttonStart_Click(object sender, EventArgs e)
    {
        if (comboPorts.SelectedItem is not string portName || string.IsNullOrWhiteSpace(portName))
        {
            MessageBox.Show(this, "Bitte zuerst einen COM-Port auswaehlen.", "COM-Port fehlt");
            return;
        }

        try
        {
            _senderService.ConnectEsp(portName);

            AppendStatus($"ESP verbunden auf {portName}.");
        }
        catch (Exception ex)
        {
            AppendStatus($"ESP-Verbindung fehlgeschlagen: {ex.Message}");
            MessageBox.Show(this, ex.Message, "ESP-Verbindung fehlgeschlagen");
        }

        UpdateStatus();
    }

    private void buttonStop_Click(object sender, EventArgs e)
    {
        _senderService.DisconnectEsp();
        AppendStatus("ESP getrennt.");
        UpdateStatus();
    }

    private void RefreshComPorts()
    {
        var previous = comboPorts.SelectedItem as string;
        var ports = SerialPort.GetPortNames().OrderBy(name => name, StringComparer.OrdinalIgnoreCase).ToArray();

        comboPorts.BeginUpdate();
        comboPorts.Items.Clear();
        comboPorts.Items.AddRange(ports);
        comboPorts.EndUpdate();

        if (!string.IsNullOrWhiteSpace(previous) && ports.Contains(previous, StringComparer.OrdinalIgnoreCase))
        {
            comboPorts.SelectedItem = previous;
        }
        else if (ports.Length > 0)
        {
            comboPorts.SelectedIndex = 0;
        }

        AppendStatus($"COM-Ports aktualisiert: {string.Join(", ", ports)}");
    }

    private void UpdateStatus()
    {
        labelWheelValue.Text = _senderService.IsWheelRunning
            ? $"{_senderService.WheelName} (aktiv)"
            : _probedWheelName;
        labelSenderValue.Text = _senderService.IsEspConnected ? "Verbunden" : "Getrennt";
        labelPacketsValue.Text = _senderService.PacketCount.ToString();
        labelStateValue.Text = _senderService.StatusText;
        var telemetry = _senderService.LastTelemetry;
        labelSteeringLiveValue.Text = telemetry.SteeringRaw.ToString();
        labelGasLiveValue.Text = telemetry.GasRaw.ToString();
        labelBrakeLiveValue.Text = telemetry.BrakeRaw.ToString();
        UpdateVehicleStatus(_senderService.LastControlState, _senderService.LastVehicleTelemetry);

        buttonStart.Enabled = !_senderService.IsEspConnected;
        buttonStop.Enabled = _senderService.IsEspConnected;
        buttonRefreshPorts.Enabled = !_senderService.IsEspConnected;
        comboPorts.Enabled = !_senderService.IsEspConnected;
        _buttonWheelStart.Enabled = !_senderService.IsWheelRunning;
        _buttonWheelStop.Enabled = _senderService.IsWheelRunning;
    }

    private void UpdateVehicleStatus(SenderService.ControlStateSnapshot control, SenderService.VehicleTelemetrySnapshot telemetry)
    {
        labelVehicleLinkValue.Text = telemetry.LinkActive ? "Aktiv" : "Inaktiv";
        labelVehicleModeValue.Text = control.SportMode ? "Normal" : "Aggressiv";
        labelVehicleCameraValue.Text = control.CameraRearActive ? "Hinten" : "Vorne";
        labelVehicleLightValue.Text = control.FlashActive
            ? (control.MainLightOn ? "Ein + Lichthupe" : "Lichthupe")
            : (control.MainLightOn ? "Ein" : "Aus");
        labelVehicleBatteryValue.Text = telemetry.BatteryMv > 0 ? $"{telemetry.BatteryPercent} %" : "--";
        labelVehicleTempValue.Text = telemetry.VehicleStatusValid ? $"{telemetry.BatteryTempC} °C" : "--";
        labelGearValue.Text = control.Gear.ToString();
        labelGearValue.ForeColor = control.Gear switch
        {
            'R' => Color.Firebrick,
            'N' => Color.ForestGreen,
            'D' => Color.RoyalBlue,
            _ => Color.DimGray,
        };
        if (_labelUpLqValue is not null)
        {
            _labelUpLqValue.Text = $"{telemetry.UplinkLq} %";
            _labelUpRssiValue!.Text = FormatRssi(telemetry.UplinkRssi);
            _labelUpSnrValue!.Text = FormatSnr(telemetry.UplinkSnr);
            _labelDownLqValue!.Text = $"{telemetry.DownlinkLq} %";
            _labelDownRssiValue!.Text = FormatRssi(telemetry.DownlinkRssi);
            _labelDownSnrValue!.Text = FormatSnr(telemetry.DownlinkSnr);
            _labelRfProfileValue!.Text = FormatRfProfile(telemetry.RfProfile);
            _labelTxPowerValue!.Text = FormatTxPower(telemetry.TxPower);
        }

        var debug = _senderService.LastDebugTelemetry;
        labelVehicleDebugValue.Text =
            $"RX Bytes: {debug.RxBytes}{Environment.NewLine}" +
            $"Frames: {debug.Frames}{Environment.NewLine}" +
            $"CRC Fehler: {debug.CrcErrors}{Environment.NewLine}" +
            $"FlightMode: {debug.FlightModeFrames}{Environment.NewLine}" +
            $"Battery: {debug.BatteryFrames}{Environment.NewLine}" +
            $"LinkStats: {debug.LinkStatsFrames}{Environment.NewLine}" +
            $"Device: {debug.DeviceInfoFrames}{Environment.NewLine}" +
            $"Last Type: 0x{debug.LastTypeHex}{Environment.NewLine}" +
            $"Valid: {(debug.Valid ? 1 : 0)}";
    }

    private void ProbeWheel()
    {
        try
        {
            var wheelName = DirectInputWheel.ProbePreferredDeviceName();

            if (string.IsNullOrWhiteSpace(wheelName))
            {
                AppendStatus("Kein Lenkrad gefunden.");
                _probedWheelName = "Nicht gefunden";
                labelWheelValue.Text = _probedWheelName;
                return;
            }

            AppendStatus($"Lenkrad gefunden: {wheelName}");
            _probedWheelName = wheelName;
            labelWheelValue.Text = _probedWheelName;
        }
        catch (Exception ex)
        {
            AppendStatus($"Lenkrad-Pruefung fehlgeschlagen: {ex.Message}");
        }
    }

    private void HandleStatusMessage(string message)
    {
        if (IsDisposed)
        {
            return;
        }

        if (InvokeRequired)
        {
            BeginInvoke(() => HandleStatusMessage(message));
            return;
        }

        if (string.Equals(_lastStatusLine, message, StringComparison.Ordinal))
        {
            return;
        }

        _lastStatusLine = message;
        AppendStatus(message);
    }

    private void AppendStatus(string message)
    {
        var line = $"[{DateTime.Now:HH:mm:ss}] {message}";

        if (textStatusLog.TextLength > 0)
        {
            textStatusLog.AppendText(Environment.NewLine);
        }

        textStatusLog.AppendText(line);
        textStatusLog.SelectionStart = textStatusLog.TextLength;
        textStatusLog.ScrollToCaret();
    }

    private sealed class ThemedGroupBox : GroupBox
    {
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.Clear(Parent?.BackColor ?? BackColor);

            var textSize = TextRenderer.MeasureText(Text, Font);
            var borderTop = Math.Max(8, textSize.Height / 2);
            var borderRect = Rectangle.FromLTRB(0, borderTop, Width - 1, Height - 1);

            using (var path = CreateRoundedRectanglePath(borderRect, 10f))
            using (var fillBrush = new SolidBrush(BackColor))
            using (var borderPen = new Pen(Color.FromArgb(54, 82, 124), 1.8f))
            {
                e.Graphics.FillPath(fillBrush, path);
                e.Graphics.DrawPath(borderPen, path);
            }

            var textBounds = new Rectangle(10, 0, textSize.Width + 8, textSize.Height);
            using var textBackBrush = new SolidBrush(Parent?.BackColor ?? BackColor);
            e.Graphics.FillRectangle(textBackBrush, textBounds);
            TextRenderer.DrawText(e.Graphics, Text, Font, new Point(14, 0), ForeColor);
        }
    }
}
